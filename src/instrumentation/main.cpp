#include <iostream>

#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

int main() {
    // Attempt to load LLVM module from file.
    llvm::LLVMContext context;
    llvm::SMDiagnostic err;
    std::unique_ptr<llvm::Module> mod = llvm::parseIRFile("somefile.txt", err, context);

    // If we failed to load the LLVM IR, report an error.
    if (!mod) {
        std::string errMsg;
        llvm::raw_string_ostream outStr(errMsg);
        err.print("unk", outStr);
        std::cout << errMsg;
        return 1;
    }

    return 0;
}
