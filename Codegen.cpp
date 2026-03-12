#include "ExprAST.hpp"
#include "LowLevelParser.hpp"

std::map<std::string, llvm::Value *> NamedValues;
std::unique_ptr<llvm::LLVMContext> TheContext;
std::unique_ptr<llvm::Module> TheModule;
std::unique_ptr<llvm::IRBuilder<>> Builder;
std::unique_ptr<llvm::orc::KaleidoscopeJIT> TheJIT;
std::unique_ptr<llvm::FunctionPassManager> TheFPM;
std::unique_ptr<llvm::LoopAnalysisManager> TheLAM;
std::unique_ptr<llvm::FunctionAnalysisManager> TheFAM;
std::unique_ptr<llvm::CGSCCAnalysisManager> TheCGAM;
std::unique_ptr<llvm::ModuleAnalysisManager> TheMAM;
std::unique_ptr<llvm::PassInstrumentationCallbacks> ThePIC;
std::unique_ptr<llvm::StandardInstrumentations> TheSI;
std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
llvm::ExitOnError ExitOnErr;

llvm::Value *LogErrorV(const char *Str) {
    LogError(Str);
    return nullptr;
}

llvm::Value *NumberExprAST::codegen() {
    return llvm::ConstantFP::get(*TheContext, llvm::APFloat(Val)); //Create a constant floating point value
}

llvm::Value *VariableExprAST::codegen() {
    // Look this variable up in the function.
    llvm::Value *V = NamedValues[Name]; // If not found, look it up in the global scope.
    if (!V) LogErrorV("Unknown variable name"); //If the variable name is not found, return an error.
    return V;
}

llvm::Value *BinaryExprAST::codegen() {
    llvm::Value *L = LHS->codegen(); //Execute the left-hand side of the binary expression and get its value. Its recursive nature allows for nested expressions.
    llvm::Value *R = RHS->codegen(); //Execute the right-hand side of the binary expression and get its value. Its recursive nature allows for nested expressions.
    if (!L || !R) return nullptr;

    switch (Op) {
        case '+': return Builder->CreateFAdd(L, R, "addtmp"); //Create a floating point addition instruction with the left and right values as operands, and "addtmp" as the name of the resulting value.
        case '-': return Builder->CreateFSub(L, R, "subtmp"); //Create a floating point subtraction instruction with the left and right values as operands, and "subtmp" as the name of the resulting value.
        case '*': return Builder->CreateFMul(L, R, "multmp"); //Create a floating point multiplication instruction with the left and right values as operands, and "multmp" as the name of the resulting value.
        case '<':
            L = Builder->CreateFCmpULT(L, R, "cmptmp"); //Compare the left and right values using an unordered less-than comparison, and "cmptmp" as the name of the resulting value.
            // Convert bool 0/1 to double 0.0 or 1.0
            return Builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*TheContext), "booltmp"); //Convert the resulting boolean value from the comparison to a double type (0.0 or 1.0) using an unsigned integer to floating point conversion, and "booltmp" as the name of the resulting value.
        default: return LogErrorV("invalid binary operator"); //Any other binary key is invalid, so we return an error.
    }
}

llvm::Value *CallExprAST::codegen() {
    // Look up the name in the global module table.
    llvm::Function *CalleeF = getFunction(Callee);
    // Look up the name in the global module table.
    //llvm::Function *CalleeF = TheModule->getFunction(Callee); //Look up the function by name in the current module. If the function is not found, it returns nullptr.
    if (!CalleeF) return LogErrorV("Unknown function referenced");

    // If argument mismatch error.
    if (CalleeF->arg_size() != Args.size()) return LogErrorV("Incorrect # arguments passed");

    std::vector<llvm::Value *> ArgsV; //Argument values
    for (unsigned i = 0, e = Args.size(); i != e; ++i) {
        ArgsV.push_back(Args[i]->codegen()/*Generate the IR code for the argument and saves it*/);
        if (!ArgsV.back()) return nullptr; //If code generation for any argument fails, return nullptr to indicate an error.
    }

    return Builder->CreateCall(CalleeF, ArgsV, "calltmp"); //Create a call instruction to the function CalleeF with the generated argument values, and "calltmp" as the name of the resulting value.
}

llvm::Value *IfExprAST::codegen() {
    llvm::Value *CondV = Cond->codegen();
    if (!CondV) return nullptr;

    // Convert condition to a bool by comparing non-equal to 0.0.
    CondV = Builder->CreateFCmpONE(CondV, llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0)), "ifcond"); //This creates a floating-point comparison instruction to check if the condition is not equal to zero. The result is a boolean value (true if the condition is non-zero, false otherwise) and is named "ifcond".
    llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent(); //Get the current function that we are generating code for. This is done by getting the current basic block from the IR builder and then getting its parent function.
    // Create blocks for the then and else cases.  Insert the 'then' block at the
    // end of the function.
    llvm::BasicBlock *ThenBB = llvm::BasicBlock::Create(*TheContext, "then", TheFunction); //Create a new basic block named "then" and insert it into the current function. This block will contain the code for the "then" part of the if expression.
    llvm::BasicBlock *ElseBB = llvm::BasicBlock::Create(*TheContext, "else"); //Create a new basic block named "else". This block will contain the code for the "else" part of the if expression. Note that it is not yet inserted into the function, as we will insert it later after generating the code for the "then" block.
    llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(*TheContext, "ifcont"); //Create a new basic block named "ifcont". This block will be the merge point for the "then" and "else" blocks, where control flow will converge after executing either branch of the if expression.

    Builder->CreateCondBr(CondV, ThenBB, ElseBB); //Create a conditional branch instruction that branches to the "then" block if the condition is true (non-zero) and to the "else" block if the condition is false (zero). The condition value is CondV, and the target blocks are ThenBB and ElseBB.

    // Emit then value.
    Builder->SetInsertPoint(ThenBB); //Set the insertion point of the IR builder to the "then" block, so that subsequent instructions will be added to this block.

    llvm::Value *ThenV = Then->codegen(); //Generate the IR code for the "then" part of the if expression and get its value. If code generation for the "then" part fails, it will return nullptr.
    if (!ThenV) return nullptr;

    Builder->CreateBr(MergeBB); //Create an unconditional branch instruction that jumps to the merge block after executing the "then" part. This ensures that control flow will continue to the merge block after the "then" block is executed.
    // Codegen of 'Then' can change the current block, update ThenBB for the PHI.
    ThenBB = Builder->GetInsertBlock(); //After generating the code for the "then" part, we update ThenBB to the current basic block. This is necessary because the code generation for the "then" part may have added new basic blocks (e.g., for loops or additional control flow), and we want to ensure that ThenBB points to the correct block for the PHI node that we will create later.

    // Emit else block.
    TheFunction->insert(TheFunction->end(), ElseBB); //Insert the "else" block into the current function. We do this after generating the code for the "then" part to ensure that the "else" block is placed correctly in the function's control flow.
    Builder->SetInsertPoint(ElseBB); //Set the insertion point of the IR builder to the "else" block, so that subsequent instructions will be added to this block.

    llvm::Value *ElseV = Else->codegen(); //Generate the IR code for the "else" part of the if expression and get its value. If code generation for the "else" part fails, it will return nullptr.
    if (!ElseV) return nullptr;

    Builder->CreateBr(MergeBB); //Create an unconditional branch instruction that jumps to the merge block after executing the "else" part. This ensures that control flow will continue to the merge block after the "else" block is executed.
    // codegen of 'Else' can change the current block, update ElseBB for the PHI.
    ElseBB = Builder->GetInsertBlock(); //After generating the code for the "else" part, we update ElseBB to the current basic block. This is necessary because the code generation for the "else" part may have added new basic blocks (e.g., for loops or additional control flow), and we want to ensure that ElseBB points to the correct block for the PHI node that we will create next.

    // Emit merge block.
    TheFunction->insert(TheFunction->end(), MergeBB); //Insert the merge block into the current function. This block will be the point where control flow converges after executing either the "then" or "else" block.
    Builder->SetInsertPoint(MergeBB); //Set the insertion point of the IR builder to the merge block, so that subsequent instructions will be added to this block.
    llvm::PHINode *PN = Builder->CreatePHI(llvm::Type::getDoubleTy(*TheContext), 2, "iftmp"); //Create a PHI node in the merge block that will select the correct value based on which branch was taken. The PHI node has a type of double (the expected type of the if expression), and it has two incoming values (one from the "then" block and one from the "else" block). The name of the PHI node is "iftmp".
    //Phi is a special instruction that selects a value based on the control flow path taken to reach it. It takes multiple incoming values, each associated with a specific predecessor block, and produces a single output value that corresponds to the value from the predecessor block that was executed. In this case, the PHI node will select either ThenV or ElseV based on whether control flow came from ThenBB or ElseBB.
    PN->addIncoming(ThenV, ThenBB); //Add an incoming value to the PHI node for the "then" block. This tells the PHI node that if control flow comes from ThenBB, it should take the value ThenV.
    PN->addIncoming(ElseV, ElseBB); //Add an incoming value to the PHI node for the "else" block. This tells the PHI node that if control flow comes from ElseBB, it should take the value ElseV.
    return PN; //Return the PHI node as the result of the if expression. This value will be used in the code generation for any expressions that use the result of the if expression.
}

llvm::Value *ForExprAST::codegen(){
    // Emit the start code first, without 'variable' in scope.
    llvm::Value *StartVal = Start->codegen();
    if (!StartVal) return nullptr;
    // Make the new basic block for the loop header, inserting after current
    // block.
    llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();
    llvm::BasicBlock *PreheaderBB = Builder->GetInsertBlock(); //Get the current basic block before creating the loop header. This block will be used as the predecessor for the loop header block, and it is where we will insert the initial branch to the loop header.
    llvm::BasicBlock *LoopBB = llvm::BasicBlock::Create(*TheContext, "loop", TheFunction); //Create a new basic block named "loop" and insert it into the current function. This block will be the header of the loop, where we will check the loop condition and execute the loop body.

    // Insert an explicit fall through from the current block to the LoopBB.
    Builder->CreateBr(LoopBB);

    // Start insertion in LoopBB.
    Builder->SetInsertPoint(LoopBB);

    // Start the PHI node with an entry for Start.
    llvm::PHINode *Variable = Builder->CreatePHI(llvm::Type::getDoubleTy(*TheContext), 2, VarName);
    //The PHI node is created in the loop header block (LoopBB) and is used to represent the loop variable. It has a type of double (the expected type of the loop variable), and it has two incoming values (one for the initial value before the loop starts, and one for the updated value after each iteration). The name of the PHI node is set to VarName, which is the name of the loop variable. The PHI node will be updated in the loop body to represent the new value of the loop variable after each iteration.
    Variable->addIncoming(StartVal, PreheaderBB); //Add an incoming value to the PHI node for the initial value of the loop variable. This tells the PHI node that before the loop starts (coming from PreheaderBB), it should take the value StartVal.

    // Within the loop, the variable is defined equal to the PHI node.  If it
    // shadows an existing variable, we have to restore it, so save it now.
    llvm::Value *OldVal = NamedValues[VarName];
    NamedValues[VarName] = Variable;

    // Emit the body of the loop.  This, like any other expr, can change the
    // current BB.  Note that we ignore the value computed by the body, but don't
    // allow an error.
    if (!Body->codegen()) return nullptr;

    //After building the loop body, we need to emit the step value. This is done by creating a new value that adds the step value to the loop variable. If the step value is not specified, we use a default step of 1.0. The resulting value (NextVar) will be used as the new value for the loop variable in the next iteration of the loop.
    // Emit the step value.
    llvm::Value *StepVal = nullptr;
    if (Step) {
        StepVal = Step->codegen();
        if (!StepVal) return nullptr; 
    } else StepVal = llvm::ConstantFP::get(*TheContext, llvm::APFloat(1.0)); // If not specified, use 1.0.
    llvm::Value *NextVar = Builder->CreateFAdd(Variable, StepVal, "nextvar");

    //After emitting the step value, we need to emit the loop end condition. This is done by generating the code for the end expression and then creating a comparison instruction to check if the end condition is met. The resulting boolean value (EndCond) will be used in a conditional branch to determine whether to continue looping or exit the loop.
    // Compute the end condition.
    llvm::Value *EndCond = End->codegen();
    if (!EndCond) return nullptr;

    // Convert condition to a bool by comparing non-equal to 0.0.
    EndCond = Builder->CreateFCmpONE(EndCond, llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0)), "loopcond");

    // Create the "after loop" block and insert it.
    llvm::BasicBlock *LoopEndBB = Builder->GetInsertBlock();
    llvm::BasicBlock *AfterBB = llvm::BasicBlock::Create(*TheContext, "afterloop", TheFunction);

    // Insert the conditional branch into the end of LoopEndBB.
    Builder->CreateCondBr(EndCond, LoopBB, AfterBB);

    // Any new code will be inserted in AfterBB.
    Builder->SetInsertPoint(AfterBB);

    // Add a new entry to the PHI node for the backedge.
    Variable->addIncoming(NextVar, LoopEndBB);

    // Restore the unshadowed variable.
    if (OldVal) NamedValues[VarName] = OldVal;
    else NamedValues.erase(VarName);

    // for expr always returns 0.0.
    return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*TheContext));
}

llvm::Function *PrototypeAST::codegen() {
    // Make the function type:  double(double,double) etc.
    //TheConext is a pointer to the LLVMContext, which is used to create types and other LLVM objects. The getDoubleTy function is called on the context to get the double type, which is used as the return type of the function. The vector of double types is created for the function arguments, and then a FunctionType is created with the return type and argument types.
    std::vector<llvm::Type*> Doubles(Args.size(), llvm::Type::getDoubleTy(*TheContext)); //Create a vector of double types for the function arguments. The size of the vector is determined by the number of arguments in the prototype, and each element is initialized to the double type from the LLVM context.
    llvm::FunctionType *FT = llvm::FunctionType::get(llvm::Type::getDoubleTy(*TheContext), Doubles, false); //Create a function type that returns a double and takes the previously created vector of double types as arguments. The 'false' parameter indicates that the function does not accept variable arguments.
    llvm::Function *F = llvm::Function::Create(FT, llvm::Function::ExternalLinkage/*indicating that the global value 
        (such as a function or variable) is externally visible and can be referenced from 
        other translation units or modules.*/, Name, TheModule.get()); //Create a new function with the specified function type, external linkage, name, and module. The function is added to the current module.
    // Set names for all arguments.
    unsigned Idx = 0;
    for (auto &Arg : F->args()) Arg.setName(Args[Idx++]); //Set the name of each argument in the function to the corresponding name from the Args vector in the prototype. The Idx variable is used to keep track of the current index in the Args vector.
    return F;
}

llvm::Function *FunctionAST::codegen(){
    // Transfer ownership of the prototype to the FunctionProtos map, but keep a
    // reference to it for use below.
    auto &P = *Proto; //Get the prototype function from the unique pointer Proto. This is done to keep a reference to the prototype for use in the code generation process, while transferring ownership of the prototype to the FunctionProtos map.
    FunctionProtos[Proto->getName()] = std::move(Proto); //Transfer ownership of the prototype to the FunctionProtos map using std::move. This allows the prototype to be stored in the map while still allowing us to use it in the code generation process.
    llvm::Function *TheFunction = getFunction(P.getName()); //Look for a function with the given name in the current module. If it exists, return it. If it does not exist, look for a prototype for the function in the FunctionProtos map. If a prototype exists, generate the function from the prototype and return it. If no prototype exists, return nullptr to indicate that the function cannot be found or generated.
    if(!TheFunction) return nullptr; //If the function cannot be found or generated, return nullptr to indicate an error.
    /*==================================LEGACY BEFORE CHECK OTHER MODULES
    // First, check for an existing function from a previous 'extern' declaration.
    llvm::Function *TheFunction = TheModule->getFunction(Proto->getName());
    if (!TheFunction) TheFunction = Proto->codegen(); //If the function does not already exist, we generate it from the prototype. If code generation for the prototype fails, it will return nullptr.
    if (!TheFunction) return nullptr; //If code generation for the function fails, return nullptr to indicate an error.
    */
    if (!TheFunction->empty()) return (llvm::Function*)LogErrorV("Function cannot be redefined."); // If the function already has a body (i.e., it is not empty), then it cannot be redefined, and we return an error.

    // Create a new basic block to start insertion into.
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*TheContext, "entry", TheFunction); //Create a new basic block named "entry" in the function TheFunction. This block will be the entry point for the function's instructions.
    Builder->SetInsertPoint(BB); //Set the insertion point of the IR builder to the newly created basic block. This means that any new instructions generated by the builder will be inserted into this block.
    
    // Record the function arguments in the NamedValues map.
    NamedValues.clear(); //Clear the NamedValues to ensure that it only contains the arguments for the current function. This is important because NamedValues is used to look up variable names during code generation, and we want to avoid conflicts between different functions.
    for (auto &Arg : TheFunction->args()) NamedValues[std::string(Arg.getName())] = &Arg; //Set the NamedValues map to associate each argument name with its corresponding llvm::Value (the argument itself) in the current function. This allows for variable lookup during code generation.

    if (llvm::Value *RetVal = Body->codegen()) { //Generate the IR code for the function body and get the return value. If code generation for the body fails, it will return nullptr, and we will not proceed to finish the function.
        // Finish off the function.
        Builder->CreateRet(RetVal); //Create a return instruction with the generated return value from the function body.
        // Validate the generated code, checking for consistency.
        llvm::verifyFunction(*TheFunction); //Verify the generated function to ensure that it is well-formed and does not contain any errors. If verification fails, it will print an error message and terminate the program.
        
        // Optimize the function.
        TheFPM->run(*TheFunction, *TheFAM); //Run the function pass manager on the generated function to perform optimizations. TheFAM is the function analysis manager that provides analysis results to the optimization passes.

        return TheFunction;
    }

     // Error reading body, remove function.
    TheFunction->eraseFromParent();
    return nullptr;
}

//Look for a function with the given name in the current module. If it exists, return it. If it does not exist, look for a prototype for the function in the FunctionProtos map. If a prototype exists, generate the function from the prototype and return it. If no prototype exists, return nullptr to indicate that the function cannot be found or generated.
//It is used to resolve function references during code generation, allowing for both previously defined functions and functions that are declared but not yet defined (via prototypes) to be handled correctly.
//This function is also usefull while compiling with multiple threads, as it ensures that function references are resolved correctly even if the function definitions are generated in a different thread or module. By checking both the current module and the FunctionProtos map, it provides a robust mechanism for handling function references in a multi-threaded compilation environment.
llvm::Function *getFunction(std::string Name) {
    // First, see if the function has already been added to the current module.
    if (auto *F = TheModule->getFunction(Name)) return F; //Look up the function by name in the current module. If the function is found, it is returned.

    // If not, check whether we can codegen the declaration from some existing prototype.
    auto FI = FunctionProtos.find(Name); //Look for the function prototype in the FunctionProtos map using the function name as the key.
    if (FI != FunctionProtos.end()) return FI->second->codegen(); //If a prototype is found, generate the function from the prototype and return it.
    //This just generate the prototype for the function if it exists in the FunctionProtos map, which allows for functions that are declared but not yet defined to be handled correctly. If the prototype does not exist, it returns nullptr to indicate that the function cannot be found or generated.
    //Doesnt generate the function body, just the declaration from the prototype. This is useful for handling external function declarations and forward references to functions that may be defined later in the code.
    // If no existing prototype exists, return null.
    
    //=====IMPORTANT=======
    //As the function protype is generated with external linkage, it can be referenced from other modules, 
    //allowing for cross-module function calls. By checking the FunctionProtos map, we can ensure that even 
    //if a function is not defined in the current module, we can still generate a declaration for it if a prototype
    // exists, enabling correct handling of external function references.
    
    return nullptr; //If no prototype is found, return nullptr to indicate that the function cannot be found or generated.
}