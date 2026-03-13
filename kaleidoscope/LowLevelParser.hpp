#ifndef LOW_LEVEL_PARSER_HPP
#define LOW_LEVEL_PARSER_HPP

#include <map>
#include <iostream>
#include "ExprAST.hpp"
#include "Lexer.hpp"

/// CurTok/getNextToken - Provide a simple token buffer.  CurTok is the current
/// token the parser is looking at.  getNextToken reads another token from the
/// lexer and updates CurTok with its results.
extern int CurTok;
int getNextToken();

/// LogError* - These are little helper functions for error handling.
std::unique_ptr<ExprAST> LogError(const char *Str);
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str);

/// numberexpr ::= number
//It expects to be called when the current token is a tok_number token, and it creates a NumberExprAST using the numeric value provided by the lexer. After creating the NumberExprAST, it consumes the number token and returns the created expression.
std::unique_ptr<ExprAST> ParseNumberExpr();

/// parenexpr ::= '(' expression ')'
std::unique_ptr<ExprAST> ParseParenExpr();

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
//It expects to be called if the current token is a tok_identifier token and it handles both simple variable references and function calls. If the token is just an identifier, it creates a VariableExprAST. If the identifier is followed by an open parenthesis, it treats it as a function call, parses the arguments, and creates a CallExprAST.
std::unique_ptr<ExprAST> ParseIdentifierExpr();

/// primary
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
//It is the entry point for parsing an expression, and it decides which specific parsing function to call based on the current token. If the token is an identifier, it calls ParseIdentifierExpr; if it's a number, it calls ParseNumberExpr; if it's an open parenthesis, it calls ParseParenExpr. If the token doesn't match any of these cases, it logs an error and returns null.
std::unique_ptr<ExprAST> ParsePrimary();

/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.
extern std::map<char, int> BinopPrecedence;

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
int GetTokPrecedence();

/// expression
///   ::= primary binoprhs
///
std::unique_ptr<ExprAST> ParseExpression();

/// binoprhs
///   ::= ('+' primary)*
std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec, std::unique_ptr<ExprAST> LHS);
        
/// prototype
///   ::= id '(' id* ')'
std::unique_ptr<PrototypeAST> ParsePrototype();

/// definition ::= 'def' prototype expression
std::unique_ptr<FunctionAST> ParseDefinition();

/// external ::= 'extern' prototype
std::unique_ptr<PrototypeAST> ParseExtern();

/// toplevelexpr ::= expression
std::unique_ptr<FunctionAST> ParseTopLevelExpr();

std::unique_ptr<ExprAST> ParseIfExpr();

std::unique_ptr<ExprAST> ParseForExpr();

std::unique_ptr<ExprAST> ParseUnary();

std::unique_ptr<ExprAST> ParseVarExpr();

#endif // LOW_LEVEL_PARSER_HPP