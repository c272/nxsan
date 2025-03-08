#include "instrumentation/AccessInstrumenter.hpp"

#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/raw_ostream.h>

namespace nxsan {

AccessInstrumenter::AccessInstrumenter(const std::string &llvmIrPath)
    : m_filePath(llvmIrPath), m_numLoads{0}, m_numStores{0} {}

NxsResult<InstrumentedIr, std::string> AccessInstrumenter::GenerateIR() {
  // Reset loads, stores.
  m_numLoads = 0;
  m_numStores = 0;

  // Attempt to load LLVM module from file.
  llvm::LLVMContext context;
  llvm::SMDiagnostic err;
  m_mod = llvm::parseIRFile(m_filePath, err, context);

  // If we failed to load the LLVM IR, report an error.
  if (!m_mod) {
    std::string errMsg;
    {
      llvm::raw_string_ostream outStr(errMsg);
      err.print("nxsan-instrumentation-cxx", outStr);
    }
    return errMsg;
  }

  // Insert function declarations for the external instrumentation functions.
  DeclareInstruments(context);

  // Iterate over all BB instructions, instrument them.
  for (auto mit = m_mod->begin(); mit != m_mod->end(); ++mit) {
    llvm::Function &func = *mit;
    for (auto fit = func.begin(); fit != func.end(); ++fit) {
      llvm::BasicBlock &bb = *fit;
      for (auto bbit = bb.begin(); bbit != bb.end(); ++bbit) {
        InstrumentInstr(*bbit);
      }
    }
  }

  // Output module.
  std::string moduleLlvm;
  {
    llvm::raw_string_ostream modStream(moduleLlvm);
    modStream << *m_mod;
  }

  // Unload module.
  m_mod = nullptr;

  return InstrumentedIr{moduleLlvm, m_numLoads, m_numStores};
}

void AccessInstrumenter::InstrumentInstr(llvm::Instruction &inst) {
  // Attempt to get the instrument mode.
  auto modeOpt = GetInstrumentMode(inst);
  if (!modeOpt.has_value()) {
    return;
  }

  // Increment count appropriately.
  InstrumentMode mode = modeOpt.value();
  switch (mode) {
  case InstrumentMode::Load:
    m_numLoads++;
    break;
  case InstrumentMode::Store:
    m_numStores++;
    break;
  }

  // Fetch instrument size.
  InstrumentSize size = GetInstrumentSize(inst);

  // Insert the instrumenting call.
  auto callee = GetInstrument(mode, size);
  llvm::IRBuilder<> builder(&inst);
  llvm::Value *args[] = {GetPointerOperand(inst)};
  builder.CreateCall(callee, args);
}

llvm::Value *AccessInstrumenter::GetPointerOperand(llvm::Instruction &instr) {
  assert(
      (llvm::isa<llvm::LoadInst>(instr) || llvm::isa<llvm::StoreInst>(instr)) &&
      "Expected Load or Store instruction");
  if (auto *LI = llvm::dyn_cast<llvm::LoadInst>(&instr))
    return LI->getPointerOperand();
  return llvm::cast<llvm::StoreInst>(&instr)->getPointerOperand();
}

std::optional<InstrumentMode>
AccessInstrumenter::GetInstrumentMode(llvm::Instruction &instr) {
  if (llvm::isa<llvm::LoadInst>(&instr)) {
    return InstrumentMode::Load;
  }
  if (llvm::isa<llvm::StoreInst>(&instr)) {
    return InstrumentMode::Store;
  }
  return std::nullopt;
}

InstrumentSize AccessInstrumenter::GetInstrumentSize(llvm::Instruction &instr) {
  llvm::Type *origType = GetLoadStoreType(&instr);
  uint64_t storeSize = m_mod->getDataLayout().getTypeStoreSizeInBits(origType);
  switch (storeSize) {
  case 8:
    return InstrumentSize::A8;
  case 16:
    return InstrumentSize::A16;
  case 32:
    return InstrumentSize::A32;
  case 64:
    return InstrumentSize::A64;
  default:
    // unk
    assert(false && "Unsupported data store/load size.");
  }
}

llvm::Type *AccessInstrumenter::GetLoadStoreType(const llvm::Value *I) {
  assert((llvm::isa<llvm::LoadInst>(I) || llvm::isa<llvm::StoreInst>(I)) &&
         "Expected Load or Store instruction");
  if (auto *LI = llvm::dyn_cast<llvm::LoadInst>(I))
    return LI->getType();
  return llvm::cast<llvm::StoreInst>(I)->getValueOperand()->getType();
}

llvm::FunctionCallee AccessInstrumenter::GetInstrument(InstrumentMode mode,
                                                       InstrumentSize size) {
  auto &dict = mode == InstrumentMode::Load ? m_loadCallees : m_storeCallees;
  return dict[size];
}

void AccessInstrumenter::DeclareInstruments(llvm::LLVMContext &ctx) {
  llvm::Type *instrFuncArgs[] = {llvm::Type::getInt64Ty(ctx)};
  llvm::FunctionType *instrFuncTy = llvm::FunctionType::get(
      llvm::Type::getVoidTy(ctx),
      llvm::ArrayRef<llvm::Type *>(instrFuncArgs, 1), false);
  m_loadCallees[InstrumentSize::A8] =
      m_mod->getOrInsertFunction("__nxsan_report_load8", instrFuncTy);
  m_loadCallees[InstrumentSize::A16] =
      m_mod->getOrInsertFunction("__nxsan_report_load16", instrFuncTy);
  m_loadCallees[InstrumentSize::A32] =
      m_mod->getOrInsertFunction("__nxsan_report_load32", instrFuncTy);
  m_loadCallees[InstrumentSize::A64] =
      m_mod->getOrInsertFunction("__nxsan_report_load64", instrFuncTy);

  m_storeCallees[InstrumentSize::A8] =
      m_mod->getOrInsertFunction("__nxsan_report_store8", instrFuncTy);
  m_storeCallees[InstrumentSize::A16] =
      m_mod->getOrInsertFunction("__nxsan_report_store16", instrFuncTy);
  m_storeCallees[InstrumentSize::A32] =
      m_mod->getOrInsertFunction("__nxsan_report_store32", instrFuncTy);
  m_storeCallees[InstrumentSize::A64] =
      m_mod->getOrInsertFunction("__nxsan_report_store64", instrFuncTy);
}

} // namespace nxsan
