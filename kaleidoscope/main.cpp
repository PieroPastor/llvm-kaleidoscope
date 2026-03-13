#include <cstdio>
#include <string>
#include "LowLevelParser.hpp"
#include "HighLevelParser.hpp"

bool JITOn = true; // Enable JIT compilation by default. This flag can be set to false to disable JIT and only generate object files.
bool FromFile = false; // Flag to indicate whether the input is from a file or not. This can be used to control the behavior of the main loop and other parts of the code that depend on the source of input.

int main(int argc, char *argv[]) {
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    // Install standard binary operators.
    // 1 is lowest precedence.
    BinopPrecedence[':'] = 1; 
    BinopPrecedence['='] = 2;
    BinopPrecedence['<'] = 10;
    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 20;
    BinopPrecedence['*'] = 40;  
    //BinopPrecedence['/'] = 50;
    //BinopPrecedence['^'] = 60; // highest.
    // Prime the first token.

    if(argc >= 2){
        JITOn = false; // Disable JIT compilation if an input file is provided, and instead generate an object file.
        
        if (argc==3){
            if (std::freopen(argv[1], "r", stdin) == nullptr) {
                fprintf(stderr, "Error: could not open input file '%s'\n", argv[1]);
                return 1;
            } else FromFile = true; // Set the FromFile flag to true if we successfully opened the input file. This can be used to control the behavior of the main loop and other parts of the code that depend on the source of input.
        }

        //fprintf(stderr, "Usage: %s <output file>\n", argv[1]);
        if(!FromFile) fprintf(stderr, "ready> ");
        getNextToken();

        InitializeModuleAndPassManager();

        // Run the main "interpreter loop" now.
        MainLoop();
        
        auto TargetTriple = llvm::sys::getDefaultTargetTriple();
        TheModule->setTargetTriple(llvm::Triple(TargetTriple));
        std::string Error;
        auto Target = llvm::TargetRegistry::lookupTarget(TheModule->getTargetTriple(), Error);
        // Print an error and exit if we couldn't find the requested target.
        // This generally occurs if we've forgotten to initialise the
        // TargetRegistry or we have a bogus target triple.
        if (!Target) {
            llvm::errs() << Error;
            return 1;
        }

        auto CPU = "generic";
        auto Features = "";

        llvm::TargetOptions opt;
        auto TheTargetMachine = Target->createTargetMachine(llvm::Triple(TargetTriple), CPU, Features, opt, llvm::Reloc::PIC_);

        TheModule->setDataLayout(TheTargetMachine->createDataLayout());

        std::string IRFilename = argc == 3? argv[2] : argv[1];
        if(IRFilename.size() == 0) IRFilename = "output";
        if(IRFilename.size() <= 3 || IRFilename.substr(IRFilename.size()-3) != ".ll") IRFilename += ".ll";
        std::error_code IREC;
        llvm::raw_fd_ostream irDest(IRFilename, IREC, llvm::sys::fs::OF_Text);
        if (IREC) {
            llvm::errs() << "Could not open IR file: " << IREC.message();
            return 1;
        }
        TheModule->print(irDest, nullptr);
        irDest.flush();
        llvm::outs() << "Wrote " << IRFilename << "\n";

        std::string Filename = argc == 3? argv[2] : argv[1];
        if(Filename.size() == 0) Filename = "output";
        if(Filename.size() <= 2 || Filename.substr(Filename.size()-2) != ".o") Filename += ".o";
        std::error_code EC;
        llvm::raw_fd_ostream dest(Filename, EC, llvm::sys::fs::OF_None);

        if (EC) {
            llvm::errs() << "Could not open file: " << EC.message();
            return 1;
        }

        llvm::legacy::PassManager pass;
        auto FileType = llvm::CodeGenFileType::ObjectFile;

        if (TheTargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
            llvm::errs() << "TheTargetMachine can't emit a file of this type";
            return 1;
        }

        pass.run(*TheModule);
        dest.flush();

        llvm::outs() << "Wrote " << Filename << "\n";

        return 0;
    }

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