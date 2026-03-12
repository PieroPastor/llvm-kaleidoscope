#ifndef HIGH_LEVEL_PARSER_HPP
#define HIGH_LEVEL_PARSER_HPP

#include "ExprAST.hpp"
#include "LowLevelParser.hpp"

void HandleDefinition();

void HandleExtern();

void HandleTopLevelExpression();

/// top ::= definition | external | expression | ';'
void MainLoop();

void InitializeModuleAndPassManager();

#endif // HIGH_LEVEL_PARSER_HPP