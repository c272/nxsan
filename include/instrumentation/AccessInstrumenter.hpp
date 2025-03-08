#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <llvm/IR/Module.h>

#include "utils/NxsResult.hpp"

namespace nxsan {

// Return result from instrumentation.
struct InstrumentedIr {
  std::string ir;
  uint64_t numLoads;
  uint64_t numStores;
};

// Size of each instrument for load/store.
enum class InstrumentSize {
    A8,
    A16,
    A32,
    A64
};

// Type of instrument.
enum class InstrumentMode {
    Load,
    Store
};

// Class for reading & instrumenting pointer accesses within
// LLVM IR for sanitization.
class AccessInstrumenter {
public:
    AccessInstrumenter(const std::string& llvmIrPath);

    // Generates instrumented IR from the source LLVM IR file.
    NxsResult<InstrumentedIr, std::string> GenerateIR();

private:
    void InstrumentInstr(llvm::Instruction& inst);

    llvm::Value* GetPointerOperand(llvm::Instruction& instr);
    std::optional<InstrumentMode> GetInstrumentMode(llvm::Instruction& instr);
    InstrumentSize GetInstrumentSize(llvm::Instruction& instr);
    llvm::Type* GetLoadStoreType(const llvm::Value *I);

    llvm::FunctionCallee GetInstrument(InstrumentMode mode, InstrumentSize size);
    void DeclareInstruments(llvm::LLVMContext& ctx);

    std::unique_ptr<llvm::Module> m_mod;
    std::unordered_map<InstrumentSize, llvm::FunctionCallee> m_loadCallees;
    std::unordered_map<InstrumentSize, llvm::FunctionCallee> m_storeCallees;
    std::string m_filePath;
    uint64_t m_numLoads, m_numStores;
};

}  // namespace nxsan
