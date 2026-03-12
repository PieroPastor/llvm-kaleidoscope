#include "LowLevelParser.hpp"
#include "HighLevelParser.hpp"
#include "llvm/Support/DynamicLibrary.h"

int main() {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    // Install standard binary operators.
    // 1 is lowest precedence.
    BinopPrecedence['<'] = 10;
    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 20;
    BinopPrecedence['*'] = 40;  // highest.
    // Prime the first token.

    fprintf(stderr, "ready> ");
    getNextToken();

    TheJIT = ExitOnErr(llvm::orc::KaleidoscopeJIT::Create());
    
    // Make the module, which holds all the code.
    InitializeModuleAndPassManager();

    // Run the main "interpreter loop" now.
    MainLoop();

    // Print out all of the generated code.
    TheModule->print(llvm::errs(), nullptr);

    return 0;
}