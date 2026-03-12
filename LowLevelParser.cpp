#include "LowLevelParser.hpp"

int getNextToken() { //Read the next token from the lexer and update CurTok with its results. It returns the updated CurTok value after reading the next token.
    CurTok = gettok();
    return CurTok;
}

/// LogError* - These are little helper functions for error handling.
std::unique_ptr<ExprAST> LogError(const char *Str) {
    fprintf(stderr, "Error: %s\n", Str);
    return nullptr;
}
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
    LogError(Str);
    return nullptr;
}

/// numberexpr ::= number
//It expects to be called when the current token is a tok_number token, and it creates a NumberExprAST using the numeric value provided by the lexer. After creating the NumberExprAST, it consumes the number token and returns the created expression.
std::unique_ptr<ExprAST> ParseNumberExpr() { 
    auto Result = std::make_unique<NumberExprAST>(NumVal); //Use the numeric value from the lexer to create a NumberExprAST
    getNextToken(); // consume the number and set CurTok to the next token after the number
    return std::move(Result); //Return the created NumberExprAST
}

/// parenexpr ::= '(' expression ')'
std::unique_ptr<ExprAST> ParseParenExpr() {
    getNextToken(); // eat (.
    auto V = ParseExpression();
    if (!V) return nullptr;
    if (CurTok != ')') return LogError("expected ')'");
    getNextToken(); // eat ). and set CurTok to the next token after the closing parenthesis
    return V;
}

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
//It expects to be called if the current token is a tok_identifier token and it handles both simple variable references and function calls. If the token is just an identifier, it creates a VariableExprAST. If the identifier is followed by an open parenthesis, it treats it as a function call, parses the arguments, and creates a CallExprAST.
std::unique_ptr<ExprAST> ParseIdentifierExpr() { 
    std::string IdName = IdentifierStr;

    getNextToken();  // eat identifier. and set CurTok to the next token after the identifier

    if (CurTok != '(') return std::make_unique<VariableExprAST>(IdName); // Simple variable ref.

    // Call.
    getNextToken();  // eat ( and set CurTok to the next token after the open parenthesis
    std::vector<std::unique_ptr<ExprAST>> Args;
    if (CurTok != ')') {
        while (true) {
            if (auto Arg = ParseExpression()) Args.push_back(std::move(Arg)); //Store the parsed argument
            else return nullptr; //If parsing the argument failed, return null

            if (CurTok == ')') break;

            if (CurTok != ',') return LogError("Expected ')' or ',' in argument list");
            getNextToken(); //Eat the comma and continue parsing the next argument and set CurTok to the next token after the comma
        }
    }
    
    getNextToken(); // Eat the ')'. and set CurTok to the next token after the closing parenthesis

    return std::make_unique<CallExprAST>(IdName, std::move(Args)); //Create a CallExprAST with the function name and the parsed arguments, and return it
}

/// primary
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
//It is the entry point for parsing an expression, and it decides which specific parsing function to call based on the current token. If the token is an identifier, it calls ParseIdentifierExpr; if it's a number, it calls ParseNumberExpr; if it's an open parenthesis, it calls ParseParenExpr. If the token doesn't match any of these cases, it logs an error and returns null.
std::unique_ptr<ExprAST> ParsePrimary() {
    switch (CurTok) {
        default: return LogError("unknown token when expecting an expression");
        case tok_identifier: return ParseIdentifierExpr();
        case tok_number: return ParseNumberExpr();
        case '(': return ParseParenExpr();
        case tok_if: return ParseIfExpr();
        case tok_for: return ParseForExpr();
    }
}

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
int GetTokPrecedence() {
    if (!isascii(CurTok)) return -1;
    // Make sure it's a declared binop.
    int TokPrec = BinopPrecedence[CurTok];
    if (TokPrec <= 0) return -1;
    return TokPrec;
}

/// expression
///   ::= primary binoprhs
///
std::unique_ptr<ExprAST> ParseExpression() {
    auto LHS = ParsePrimary(); //Parse the left-hand side of the expression using ParsePrimary, which will handle identifiers, numbers, and parenthesized expressions. The result is stored in LHS (Left-Hand Side).
    if (!LHS) return nullptr;
    return ParseBinOpRHS(0, std::move(LHS));
}

/// binoprhs
///   ::= ('+' primary)*
std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                              std::unique_ptr<ExprAST> LHS) {
    // If this is a binop, find its precedence.
    while (true) { //Its a loop because we want to keep parsing binary operators until we can't anymore
        int TokPrec = GetTokPrecedence(); //Get the precedence of the current binary operator. And the recursion inside the loop will handle the right-hand side of the binary expression, so we will keep parsing binary operators until we reach an operator with lower precedence than the current one, at which point we will return the left-hand side of the expression that we have parsed so far.
        // If this is a binop that binds at least as tightly as the current binop,
        // consume it, otherwise we are done.
        //If the precedence of the current operator is less than the precedence of the expression we are parsing, 
        //then we are done parsing the binary expression and we return the left-hand side. (bc for example in 3*4+2 I just need to evaluate 3*4 first, then add 2, so when I am parsing the + operator, the precedence of + is less than the precedence of *, so I just return the left-hand side which is the result of 3*4)
        if (TokPrec < ExprPrec) return LHS; 
        //If the current operator has higher or equal precedence, we consume it and parse the right-hand side of the binary expression. We then combine the left-hand side and right-hand side into a BinaryExprAST and continue parsing any additional binary operators that may follow.
        int BinOp = CurTok; //Store the current operator
        getNextToken(); //Eat the operator
        // Parse the primary expression after the binary operator.
        auto RHS = ParsePrimary(); //Parse the right-hand side of the binary expression using Parse
        if (!RHS) return nullptr; //If parsing the right-hand side failed, return null
        // If BinOp binds less tightly with RHS than the operator after RHS, let
        // the pending operator take RHS as its LHS.
        int NextPrec = GetTokPrecedence();
        if (TokPrec < NextPrec) {
            RHS = ParseBinOpRHS(TokPrec + 1/*We add 1 to the precedence because we want the next operator to have higher precedence even if it's the same operator for the left to right associativity*/, std::move(RHS)); //If the next operator has higher precedence than the current operator, we recursively call ParseBinOpRHS to parse the right-hand side with the higher precedence. This allows us to correctly handle operator precedence in expressions.
            if (!RHS) return nullptr; //If parsing the right-hand side with the higher precedence failed, return null
        }
        // Merge LHS/RHS.
        LHS = std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS)); //Combine the left-hand side and right-hand side into a BinaryExprAST using the current operator, and store the result back in LHS for further parsing.
    }
}
        
/// prototype
///   ::= id '(' id* ')'
std::unique_ptr<PrototypeAST> ParsePrototype() {
    if (CurTok != tok_identifier) return LogErrorP("Expected function name in prototype"); //The prototype of a function must start with an identifier token, which represents the function name. If the current token is not an identifier, we log an error and return null.

    std::string FnName = IdentifierStr; //Store the function name from the lexer
    getNextToken(); //Eat the function name and set CurTok to the next token after the function name

    if (CurTok != '(') return LogErrorP("Expected '(' in prototype"); //After the function name, we expect an open parenthesis to start the argument list. If we don't see it, we log an error and return null.

    // Read the list of argument names.
    std::vector<std::string> ArgNames; 
    while (getNextToken() == tok_identifier) ArgNames.push_back(IdentifierStr); //We read the argument names in a loop. As long as we see identifier tokens, we add them to the list of argument names. The loop ends when we encounter a token that is not an identifier, which should be the closing parenthesis.
    if (CurTok != ')') return LogErrorP("Expected ')' in prototype"); //After reading the argument names, we expect to see a closing parenthesis to end the argument list. If we don't see it, we log an error and return null.

    // success.
    getNextToken();  // eat ')'.

    return std::make_unique<PrototypeAST>(FnName, std::move(ArgNames));
}

/// definition ::= 'def' prototype expression
std::unique_ptr<FunctionAST> ParseDefinition() {
    getNextToken();  // eat def. and set CurTok to the next token after 'def'   
    auto Proto = ParsePrototype(); //Parse the function prototype using ParsePrototype, which will handle the function name and argument list. The result is stored in Proto.
    if (!Proto) return nullptr; //If parsing the prototype failed, return null

    //If parsing the prototype succeeded, we then parse the function body using ParseExpression, which will handle the function's implementation. If parsing the expression for the function body fails, we return null. If it succeeds, we create a FunctionAST with the parsed prototype and body and return it.
    if (auto E = ParseExpression()) return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    return nullptr;
}

/// external ::= 'extern' prototype
std::unique_ptr<PrototypeAST> ParseExtern() {
    getNextToken();  // eat extern.
    return ParsePrototype(); //Saves the parsed prototype and returns it
}

/// toplevelexpr ::= expression
std::unique_ptr<FunctionAST> ParseTopLevelExpr() {
    if (auto E = ParseExpression()) { //If parsing the expression succeeded, we create an anonymous function prototype and return a FunctionAST that represents the top-level expression as a function with no name and no arguments. This allows us to treat top-level expressions as if they were functions, which can be useful for code generation and execution.
        // Make an anonymous proto.
        auto Proto = std::make_unique<PrototypeAST>("__anon_expr", std::vector<std::string>());
        return std::make_unique<FunctionAST>(std::move(Proto), std::move(E));
    }
    return nullptr;
}

/// ifexpr ::= 'if' expression 'then' expression 'else' expression
std::unique_ptr<ExprAST> ParseIfExpr() {
    getNextToken();  // eat the if.

    // condition.
    auto Cond = ParseExpression(); //Parse the condition expression after the 'if' keyword. If parsing the condition fails, we return null to indicate an error.
    if (!Cond) return nullptr;

    if (CurTok != tok_then) return LogError("expected then");
    getNextToken();  // eat the then

    auto Then = ParseExpression(); //Parse the 'then' expression after the 'then' keyword. If parsing the 'then' expression fails, we return null to indicate an error.
    if (!Then) return nullptr;

    if (CurTok != tok_else) return LogError("expected else");

    getNextToken(); // eat the else

    auto Else = ParseExpression(); //Parse the 'else' expression after the 'else' keyword. If parsing the 'else' expression fails, we return null to indicate an error.
    if (!Else) return nullptr;

    return std::make_unique<IfExprAST>(std::move(Cond), std::move(Then), std::move(Else)); //If parsing all parts of the if expression succeeded, we create an IfExprAST with the parsed condition, 'then' expression, and 'else' expression, and return it.
}
       
/// forexpr ::= 'for' identifier '=' expr ',' expr (',' expr)? 'in' expression
std::unique_ptr<ExprAST> ParseForExpr() {
    getNextToken();  // eat the for.

    if (CurTok != tok_identifier) return LogError("expected identifier after for");

    std::string IdName = IdentifierStr;
    getNextToken();  // eat identifier.

    if (CurTok != '=') return LogError("expected '=' after for");
    getNextToken();  // eat '='.

    auto Start = ParseExpression(); //This gets the start value for the for loop by parsing the expression after the '=' token. If parsing the start expression fails, we return null to indicate an error.
    if (!Start) return nullptr;
    if (CurTok != ',') return LogError("expected ',' after for start value");
    getNextToken(); // eat ','.

    //Normally we would expect an expression with >
    auto End = ParseExpression(); //This gets the end value for the for loop by parsing the expression after the first comma. If parsing the end expression fails, we return null to indicate an error.
    if (!End) return nullptr;

    // The step value is optional.
    std::unique_ptr<ExprAST> Step;
    if (CurTok == ',') { //If there is a second comma, it means we have a step value for the for loop, so we parse it. If parsing the step expression fails, we return null to indicate an error.
        getNextToken();
        Step = ParseExpression(); //This gets the step value for the for loop by parsing the expression after the second comma. If parsing the step expression fails, we return null to indicate an error.
        if (!Step) return nullptr;
    }

    if (CurTok != tok_in) return LogError("expected 'in' after for");
    getNextToken();  // eat 'in'.

    auto Body = ParseExpression();
    if (!Body) return nullptr;

    // At this point, we have successfully parsed all components of the for loop: the loop variable name, the start expression, the end expression, the optional step expression, and the loop body. We create a ForExprAST with these components and return it.
    return std::make_unique<ForExprAST>(IdName, std::move(Start), std::move(End), std::move(Step), std::move(Body));
}