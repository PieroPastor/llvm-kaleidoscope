#include <map>
#include <string>
#include "ExprAST.hpp"

int CurTok;
std::string IdentifierStr;
double NumVal;
std::map<char,int> BinopPrecedence;
/*
std::unique_ptr<llvm::LLVMContext> TheContext;
std::unique_ptr<llvm::Module> TheModule;
std::unique_ptr<llvm::IRBuilder<>> Builder;
*/