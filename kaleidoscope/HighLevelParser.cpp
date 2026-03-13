#include "HighLevelParser.hpp"
using namespace llvm;

void InitializeModuleAndPassManager() {
    // Open a new context and module.
    // Open a new context and module.
    TheContext = std::make_unique<LLVMContext>();
    TheModule = std::make_unique<Module>("KaleidoscopeJIT", *TheContext);
    if(JITOn) TheModule->setDataLayout(TheJIT->getDataLayout());
    // Create a new builder for the module.
    Builder = std::make_unique<IRBuilder<>>(*TheContext);

    // Create new pass and analysis managers.
    TheFPM = std::make_unique<FunctionPassManager>(); //This is a manager for function-level passes.
    TheLAM = std::make_unique<LoopAnalysisManager>();
    TheFAM = std::make_unique<FunctionAnalysisManager>();
    TheCGAM = std::make_unique<CGSCCAnalysisManager>();
    TheMAM = std::make_unique<ModuleAnalysisManager>();
    ThePIC = std::make_unique<PassInstrumentationCallbacks>();
    TheSI = std::make_unique<StandardInstrumentations>(*TheContext,/*DebugLogging*/ true);
    TheSI->registerCallbacks(*ThePIC, TheMAM.get());
    // Add transform passes.
    // Promote allocas to registers.
    TheFPM->addPass(PromotePass()); //Adds Mem2Reg pass to the function pass manager, which promotes memory allocations (alloca instructions) to register values, enabling more efficient code generation and optimization by allowing the use of SSA form and eliminating unnecessary memory accesses.
    // Do simple "peephole" optimizations and bit-twiddling optzns.
    TheFPM->addPass(InstCombinePass());
    // Reassociate expressions.
    TheFPM->addPass(ReassociatePass());
    // Eliminate Common SubExpressions.
    TheFPM->addPass(GVNPass());
    // Simplify the control flow graph (deleting unreachable blocks, etc).
    TheFPM->addPass(SimplifyCFGPass());

    // Register analysis passes used in these transform passes.
    PassBuilder PB;
    PB.registerModuleAnalyses(*TheMAM);
    PB.registerFunctionAnalyses(*TheFAM);
    PB.crossRegisterProxies(*TheLAM, *TheFAM, *TheCGAM, *TheMAM);
}

void HandleDefinition() {
    if (auto FnAST = ParseDefinition()) {
        if (auto *FnIR = FnAST->codegen()) {
            /*fprintf(stderr, "Read function definition:\n");
            FnIR->print(errs());
            fprintf(stderr, "\n");*/
            //We send the module to the JIT for compilation and execution. This allows us to execute the code for the function definition and see its result. By adding the module containing the function definition to the JIT, we can then look up the generated function and call it as a native function, allowing us to execute the code for the function definition and see its result. This is a crucial step in the process of evaluating function definitions in the Kaleidoscope JIT, as it enables the execution of dynamically generated code.
            if(JITOn){
                ExitOnErr(TheJIT->addModule(llvm::orc::ThreadSafeModule(std::move(TheModule), std::move(TheContext))));
                InitializeModuleAndPassManager();
            }
        }
    } else getNextToken(); // Skip token for error recovery.
}

void HandleExtern() {
    if (auto ProtoAST = ParseExtern()) {
        if (auto *FnIR = ProtoAST->codegen()) {
            /*fprintf(stderr, "Read extern: \n");
            FnIR->print(errs());
            fprintf(stderr, "\n");*/
            FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST); //Save the prototype in the FunctionProtos map to ensure that it can be referenced later during code generation, allowing for correct handling of external function declarations and references in the generated code.
        }
    } else getNextToken(); // Skip token for error recovery.
}

void HandleTopLevelExpression() {
    // Evaluate a top-level expression into an anonymous function.
    if (auto FnAST = ParseTopLevelExpr()) {
        if (auto *FnIR = FnAST->codegen()) {
            if(!JITOn) return;
            // Create a ResourceTracker to track JIT'd memory allocated to our
            // anonymous expression -- that way we can free it after executing.
            auto RT = TheJIT->getMainJITDylib().createResourceTracker(); 

            //=================================IMPORTANT===============================
            //The reason to send this to JIT is that we want to execute the code generated for the top-level expression, 
            //and to do that we need to add it to the JIT so that it can be compiled and executed. 
            //By adding the module containing the top-level expression to the JIT, we can then look up the generated 
            //function and call it as a native function, allowing us to execute the code for the top-level expression 
            //and see its result. This is a crucial step in the process of evaluating top-level expressions in the Kaleidoscope JIT, as it enables the execution of dynamically generated code.


            // Add the module to the JIT with the ResourceTracker. This transfers ownership of the module to the JIT, which will free it
            // when the ResourceTracker is destroyed. If anything in this process fails, the module will be deleted when the ResourceTracker is destroyed.
            auto TSM = llvm::orc::ThreadSafeModule(std::move(TheModule), std::move(TheContext));
            ExitOnErr(TheJIT->addModule(std::move(TSM), RT));
            //This is usefull because after sendint the module to the JIT, we can free the memory used by the module when we are done with it by simply destroying the ResourceTracker. This allows us to manage memory effectively and avoid leaks, especially in a JIT compilation context where we may be generating and executing code on the fly.
            InitializeModuleAndPassManager();

            // Search the JIT for the __anon_expr symbol.
            auto ExprSymbol = ExitOnErr(TheJIT->lookup("__anon_expr"));

            // Get the symbol's address and cast it to the right type (takes no
            // arguments, returns a double) so we can call it as a native function.
            double (*FP)() = ExprSymbol.toPtr<double (*)()>();
            fprintf(stderr, "Evaluated to %f\n", FP());

            // Delete the anonymous expression module from the JIT.
            ExitOnErr(RT->remove());
        }
    } else getNextToken();// Skip token for error recovery.
}

/// top ::= definition | external | expression | ';'
void MainLoop() {
    while (true) {
        if(!FromFile || JITOn) fprintf(stderr, "ready> ");
        switch (CurTok) {
            case tok_eof: return;
            case ';': // ignore top-level semicolons.
            getNextToken();
            break;
            case tok_def:
            HandleDefinition();
            break;
            case tok_extern:
            HandleExtern();
            break;
            default:
            HandleTopLevelExpression();
            break;
        }
    }
}