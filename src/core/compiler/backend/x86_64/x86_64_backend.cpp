///
/// @file x86_64_backend.cpp
/// @copyright Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
/// SPDX-License-Identifier: Apache-2.0
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
///
/// @file x86_64_backend.cpp
/// @copyright Copyright (C) 2021 BMW Group
///
// coverity[autosar_cpp14_a16_2_2_violation]
#include "src/config.hpp"

#ifdef JIT_TARGET_X86_64
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

#include "x86_64_assembler.hpp"
#include "x86_64_backend.hpp"
#include "x86_64_cc.hpp"
#include "x86_64_encoding.hpp"

#include "src/core/common/FunctionRef.hpp"
#include "src/core/common/NativeSymbol.hpp"
#include "src/core/common/Span.hpp"
#include "src/core/common/TrapCode.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/basedataoffsets.hpp"
#include "src/core/common/implementationlimits.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/Compiler.hpp"
#include "src/core/compiler/backend/BackendBase.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_call_dispatch.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_instruction.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_relpatchobj.hpp"
#include "src/core/compiler/common/BranchCondition.hpp"
#include "src/core/compiler/common/BuiltinFunction.hpp"
#include "src/core/compiler/common/Common.hpp"
#include "src/core/compiler/common/FloatTruncLimitsExcl.hpp"
#include "src/core/compiler/common/ListIterator.hpp"
#include "src/core/compiler/common/MachineType.hpp"
#include "src/core/compiler/common/MemWriter.hpp"
#include "src/core/compiler/common/ModuleInfo.hpp"
#include "src/core/compiler/common/OPCode.hpp"
#include "src/core/compiler/common/RegMask.hpp"
#include "src/core/compiler/common/RegisterCopyResolver.hpp"
#include "src/core/compiler/common/Stack.hpp"
#include "src/core/compiler/common/StackElement.hpp"
#include "src/core/compiler/common/StackType.hpp"
#include "src/core/compiler/common/VariableStorage.hpp"
#include "src/core/compiler/common/util.hpp"

namespace vb {
namespace x86_64 {

using Backend = x86_64_Backend; ///< Shortcut for the backend

namespace BD = Basedata;    ///< Shortcut for Basedata
namespace NABI = NativeABI; ///< Shortcut for NativeABI

Backend::x86_64_Backend(Stack &stack, ModuleInfo &moduleInfo, MemWriter &memory, MemWriter &output, Common &common, Compiler &compiler) VB_NOEXCEPT
    : stack_(stack),
      moduleInfo_(moduleInfo),
      memory_(memory),
      output_(output),
      common_(common),
      compiler_(compiler),
      as_(*this, output, moduleInfo) {
}

#if LINEAR_MEMORY_BOUNDS_CHECKS
void Backend::cacheJobMemoryPtrPtr(uint32_t const spOffset, REG const scrReg) const {
  static_assert(Widths::jobMemoryPtrPtr == 8U, "Cached job memory width not suitable");
  assert((spOffset < INT32_MAX) && "spOffset too large");

  // Store cached jobMemoryPtrPtr
  as_.INSTR(MOV_r64_rm64).setR(scrReg).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::jobMemoryDataPtrPtr)();
  as_.INSTR(MOV_rm64_r64).setM4RM(REG::SP, static_cast<int32_t>(spOffset)).setR(scrReg)();
}

void Backend::restoreFromJobMemoryPtrPtr(uint32_t const spOffset) const {
  assert((spOffset < INT32_MAX) && "spOffset too large");

  // Restore cached jobMemoryPtr and dereference
  as_.INSTR(MOV_r64_rm64).setR(WasmABI::REGS::linMem).setM4RM(REG::SP, static_cast<int32_t>(spOffset))();
  as_.INSTR(MOV_r64_rm64).setR(WasmABI::REGS::linMem).setM4RM(WasmABI::REGS::linMem, 0)();

  // Calculate the new base of the linear memory by adding basedataLength to the new memory base and store it in
  // REGS::linMem
  as_.INSTR(ADD_rm64_imm32sx).setR4RM(WasmABI::REGS::linMem).setImm32(moduleInfo_.getBasedataLength())();
}
#endif

#if ENABLE_EXTENSIONS
void Backend::updateRegPressureHistogram(bool const isGPR) const VB_NOEXCEPT {
  auto eval = [this](uint32_t numStaticallyAllocatedRegs, vb::Span<REG const> const &span) VB_NOEXCEPT -> uint32_t {
    // Start at numStaticallyAllocatedRegs so we do not iterate registers with locals
    uint32_t freeScratchRegCount = 0U;
    for (uint32_t regPos = numStaticallyAllocatedRegs; regPos < static_cast<uint32_t>(span.size()); regPos++) {
      REG const currentReg = span[regPos];
      Stack::iterator const referenceToLastOccurrence{moduleInfo_.getReferenceToLastOccurrenceOnStack(currentReg)};
      if (referenceToLastOccurrence.isEmpty()) {
        freeScratchRegCount++;
      }
    }
    assert(freeScratchRegCount <= static_cast<uint32_t>(span.size()));
    return freeScratchRegCount;
  };

  uint32_t numFreeRegs = 0U;
  if (isGPR) {
    numFreeRegs = eval(moduleInfo_.getNumStaticallyAllocatedGPRs(), vb::Span<REG const>(WasmABI::gpr.data(), WasmABI::gpr.size()));
  } else {
    numFreeRegs = eval(moduleInfo_.getNumStaticallyAllocatedFPRs(), vb::Span<REG const>(WasmABI::fpr.data(), WasmABI::fpr.size()));
  }
  compiler_.getAnalytics()->updateRegPressureHistogram(isGPR, numFreeRegs);
}
#endif

RegAllocCandidate Backend::getRegAllocCandidate(MachineType const type, RegMask const protRegs) const VB_NOEXCEPT {
  assert((!protRegs.allMarked()) && "BLOCKALL not allowed for scratch register request");
  assert(type != MachineType::INVALID && "Unsupported MachineType for requesting a scratch register");
  bool const isInt{MachineTypeUtil::isInt(type)};

#if ENABLE_EXTENSIONS
  if (compiler_.getAnalytics() != nullptr) {
    updateRegPressureHistogram(isInt);
  }
#endif

  // Number of actual allocated locals for that register type and the length
  // (number) of allocatable register array for that type and pointer to the correct array (GPR or FPR)
  Span<REG const> allocableRegs{};
  if (isInt) {
    allocableRegs =
        vb::Span<REG const>(WasmABI::gpr.data(), WasmABI::gpr.size()).subspan(static_cast<size_t>(moduleInfo_.getNumStaticallyAllocatedGPRs()));
  } else {
    allocableRegs =
        vb::Span<REG const>(WasmABI::fpr.data(), WasmABI::fpr.size()).subspan(static_cast<size_t>(moduleInfo_.getNumStaticallyAllocatedFPRs()));
  }

  REG chosenReg{REG::NONE};
  bool isUsed{false};

  // Start at numStaticallyAllocatedRegs so we do not iterate registers with locals
  for (REG const currentReg : allocableRegs) {
    // Skip if register is protected
    if (protRegs.contains(currentReg)) {
      continue;
    }

    Stack::iterator const referenceToLastOccurrence{moduleInfo_.getReferenceToLastOccurrenceOnStack(currentReg)};

    // If the register is not on the stack at all, we choose the current register and mark it as unused
    if (referenceToLastOccurrence.isEmpty()) {
      chosenReg = currentReg;
      break;
    } else {
      static_cast<void>(0);
    }
  }

  // There is no free scratchReg here, find the first occurrence of register on the stack
  if (chosenReg == REG::NONE) {
    isUsed = true;
    for (StackElement const &stepIt : stack_) {
      if ((isInt && ((stepIt.type == StackType::SCRATCHREGISTER_I32) || (stepIt.type == StackType::SCRATCHREGISTER_I64))) ||
          (!isInt && ((stepIt.type == StackType::SCRATCHREGISTER_F32) || (stepIt.type == StackType::SCRATCHREGISTER_F64)))) {
        chosenReg = stepIt.data.variableData.location.reg;
        if (!protRegs.contains(chosenReg)) {
          break;
        }
      }
    }
  }

  assert((chosenReg != REG::NONE) && "No register found");
  return {chosenReg, isUsed};
}

void Backend::emitMoveImpl(VariableStorage const &dstStorage, VariableStorage const &srcStorage, bool const unconditional,
                           bool const presFlags) const {
  switch (dstStorage.machineType) {
  case MachineType::I32:
  case MachineType::I64: {
    emitMoveIntImpl(dstStorage, srcStorage, unconditional, presFlags);
    break;
  }
  case MachineType::F32:
  case MachineType::F64: {
    emitMoveFloatImpl(dstStorage, srcStorage, unconditional, presFlags);
    break;
  }
  // GCOVR_EXCL_START
  case MachineType::INVALID:
  default: {
    UNREACHABLE(break, "Unknown MachineType");
  }
    // GCOVR_EXCL_STOP
  }
}

void x86_64_Backend::emitMoveInt(StackElement const &dstElem, StackElement const &srcElem, MachineType const machineType) const {
  VariableStorage dstStorage{moduleInfo_.getStorage(dstElem)};
  VariableStorage srcStorage{moduleInfo_.getStorage(srcElem)};
  dstStorage.machineType = machineType;
  srcStorage.machineType = machineType;
  emitMoveIntImpl(dstStorage, srcStorage, false, false);
}

void x86_64_Backend::emitMoveIntImpl(VariableStorage const &dstStorage, VariableStorage const &srcStorage, bool const unconditional,
                                     bool const presFlags) const {
  // GCOVR_EXCL_START
  assert(dstStorage.type != StorageType::CONSTANT && dstStorage.type != StorageType::INVALID && srcStorage.type != StorageType::INVALID &&
         "Invalid source or destination for emitMove");
  assert(MachineTypeUtil::isInt(srcStorage.machineType));
  assert(dstStorage.machineType == srcStorage.machineType && "WasmTypes of source and destination must match");
  // GCOVR_EXCL_STOP
  if ((!unconditional) && dstStorage.equals(srcStorage)) {
    return;
  }
  bool const is64{MachineTypeUtil::is64(dstStorage.machineType)};

  if (dstStorage.type == StorageType::REGISTER) { // X -> REGISTER
    REG const dstReg{dstStorage.location.reg};
    if (srcStorage.type == StorageType::CONSTANT) { // CONSTANT -> REGISTER
      if (is64) {
        if (in_range<int32_t>(bit_cast<int64_t>(srcStorage.location.constUnion.u64))) {
          uint32_t const immValue{static_cast<uint32_t>(srcStorage.location.constUnion.u64)};
          if (immValue <= static_cast<uint32_t>(INT32_MAX)) {
            // for value<=INT32_MAX, 32bit register can be used as target register to save 2 bytes/instruction.
            // x86_64 will clear the high 4 bytes of the register.
            as_.INSTR(MOV_r32_imm32).setR(dstReg).setImm32(immValue)();
          } else {
            as_.INSTR(MOV_rm64_imm32sx).setR4RM(dstReg).setImm32(immValue)();
          }
        } else {
          as_.MOVimm64(dstReg, srcStorage.location.constUnion.u64);
        }
      } else {
        as_.INSTR(MOV_r32_imm32).setR(dstReg).setImm32(srcStorage.location.constUnion.u32)();
      }
    } else if (srcStorage.type == StorageType::REGISTER) { // REGISTER -> REGISTER
      as_.INSTR(is64 ? MOV_r64_rm64 : MOV_r32_rm32).setR(dstReg).setR4RM(srcStorage.location.reg)();
    } else { // MEMORY -> REGISTER
      RegDisp const srcRegDisp{getMemRegDisp(srcStorage)};
      as_.INSTR(is64 ? MOV_r64_rm64 : MOV_r32_rm32).setR(dstReg).setM4RM(srcRegDisp.reg, srcRegDisp.disp)();
    }
  } else { // X -> MEMORY
    RegDisp const dstRegDisp{getMemRegDisp(dstStorage)};
    if (srcStorage.type == StorageType::CONSTANT) { // CONSTANT -> MEMORY
      if (is64) {
        if (!presFlags && (srcStorage.location.constUnion.u64 == 0U)) {
          as_.INSTR(AND_rm64_imm8sx).setM4RM(dstRegDisp.reg, dstRegDisp.disp).setImm8(0U)();
        } else if (in_range<int32_t>(bit_cast<int64_t>(srcStorage.location.constUnion.u64))) {
          as_.INSTR(MOV_rm64_imm32sx).setM4RM(dstRegDisp.reg, dstRegDisp.disp).setImm32(static_cast<uint32_t>(srcStorage.location.constUnion.u64))();
        } else {
          as_.INSTR(MOV_rm32_imm32).setM4RM(dstRegDisp.reg, dstRegDisp.disp).setImm32(static_cast<uint32_t>(srcStorage.location.constUnion.u64))();
          as_.INSTR(MOV_rm32_imm32)
              .setM4RM(dstRegDisp.reg, dstRegDisp.disp + 4)
              .setImm32(static_cast<uint32_t>(srcStorage.location.constUnion.u64 >> 32U))();
        }
      } else {
        if (!presFlags && (srcStorage.location.constUnion.u32 == 0U)) {
          as_.INSTR(AND_rm32_imm8sx).setM4RM(dstRegDisp.reg, dstRegDisp.disp).setImm8(0U)();
        } else {
          as_.INSTR(MOV_rm32_imm32).setM4RM(dstRegDisp.reg, dstRegDisp.disp).setImm32(srcStorage.location.constUnion.u32)();
        }
      }
    } else if (srcStorage.type == StorageType::REGISTER) { // REGISTER -> MEMORY
      as_.INSTR(is64 ? MOV_rm64_r64 : MOV_rm32_r32).setR(srcStorage.location.reg).setM4RM(dstRegDisp.reg, dstRegDisp.disp)();
    } else { // MEMORY -> MEMORY
      RegDisp const srcRegDisp{getMemRegDisp(srcStorage)};
      as_.INSTR(is64 ? MOVSD_rf_rmf : MOVSS_rf_rmf).setR(WasmABI::REGS::moveHelper).setM4RM(srcRegDisp.reg, srcRegDisp.disp)();
      as_.INSTR(is64 ? MOVSD_rmf_rf : MOVSS_rmf_rf).setR(WasmABI::REGS::moveHelper).setM4RM(dstRegDisp.reg, dstRegDisp.disp)();
    }
  }
}

void x86_64_Backend::emitMoveFloatImpl(VariableStorage const &dstStorage, VariableStorage const &srcStorage, bool const unconditional,
                                       bool const presFlags) const {
  assert(dstStorage.type != StorageType::CONSTANT && dstStorage.type != StorageType::INVALID && srcStorage.type != StorageType::INVALID &&
         "Invalid source or destination for emitMove");
  assert(dstStorage.machineType == srcStorage.machineType);
  assert(!MachineTypeUtil::isInt(dstStorage.machineType));

  if ((!unconditional) && dstStorage.equals(srcStorage)) {
    return;
  }
  bool const is64{MachineTypeUtil::is64(dstStorage.machineType)};

  if (dstStorage.type == StorageType::REGISTER) { // X -> REGISTER
    REG const dstReg{dstStorage.location.reg};
    if (srcStorage.type == StorageType::CONSTANT) { // CONSTANT -> REGISTER
      if (is64) {
        if (srcStorage.location.constUnion.rawF64() == 0U) {
          // xorpd does not affect CPU flags
          as_.INSTR(XORPD_rf_rmf).setR(dstReg).setR4RM(dstReg)();
        } else {
          RelPatchObj const constRelPatchObj{as_.prepareJMP(true)};
          output_.writeBytesLE(srcStorage.location.constUnion.rawF64(), 8U);
          constRelPatchObj.linkToHere();
          as_.INSTR(MOVSD_rf_rmf).setR(dstReg).setMIP4RMabs(constRelPatchObj.getPosOffsetAfterInstr())();
        }
      } else {
        if (srcStorage.location.constUnion.rawF32() == 0U) {
          as_.INSTR(XORPS_rf_rmf).setR(dstReg).setR4RM(dstReg)();
        } else {
          RelPatchObj const constRelPatchObj{as_.prepareJMP(true)};
          output_.writeBytesLE(static_cast<uint64_t>(srcStorage.location.constUnion.rawF32()), 4U);
          constRelPatchObj.linkToHere();
          as_.INSTR(MOVSS_rf_rmf).setR(dstReg).setMIP4RMabs(constRelPatchObj.getPosOffsetAfterInstr())();
        }
      }
    } else if (srcStorage.type == StorageType::REGISTER) { // REGISTER -> REGISTER
      as_.INSTR(is64 ? MOVSD_rf_rmf : MOVSS_rf_rmf).setR(dstReg).setR4RM(srcStorage.location.reg)();
    } else { // MEMORY -> REGISTER
      RegDisp const srcRegDisp{getMemRegDisp(srcStorage)};
      as_.INSTR(is64 ? MOVSD_rf_rmf : MOVSS_rf_rmf).setR(dstReg).setM4RM(srcRegDisp.reg, srcRegDisp.disp)();
    }
  } else { // X -> MEMORY
    RegDisp const dstRegDisp{getMemRegDisp(dstStorage)};
    if (srcStorage.type == StorageType::CONSTANT) { // CONSTANT -> MEMORY
      if (is64) {
        if (!presFlags && (srcStorage.location.constUnion.rawF64() == 0U)) {
          as_.INSTR(AND_rm64_imm8sx).setM4RM(dstRegDisp.reg, dstRegDisp.disp).setImm8(0U)();
        } else {
          as_.INSTR(MOV_rm32_imm32)
              .setM4RM(dstRegDisp.reg, dstRegDisp.disp)
              .setImm32(static_cast<uint32_t>(srcStorage.location.constUnion.rawF64()))();
          as_.INSTR(MOV_rm32_imm32)
              .setM4RM(dstRegDisp.reg, dstRegDisp.disp + 4)
              .setImm32(static_cast<uint32_t>(srcStorage.location.constUnion.rawF64() >> 32U))();
        }
      } else {
        if (!presFlags && (srcStorage.location.constUnion.rawF32() == 0U)) {
          as_.INSTR(AND_rm32_imm8sx).setM4RM(dstRegDisp.reg, dstRegDisp.disp).setImm8(0U)();
        } else {
          as_.INSTR(MOV_rm32_imm32).setM4RM(dstRegDisp.reg, dstRegDisp.disp).setImm32(srcStorage.location.constUnion.rawF32())();
        }
      }
    } else if (srcStorage.type == StorageType::REGISTER) { // REGISTER -> MEMORY
      REG const srcReg{srcStorage.location.reg};
      as_.INSTR(is64 ? MOVSD_rmf_rf : MOVSS_rmf_rf).setR(srcReg).setM4RM(dstRegDisp.reg, dstRegDisp.disp)();
    } else { // MEMORY -> MEMORY
      RegDisp const srcRegDisp{getMemRegDisp(srcStorage)};
      AbstrInstr instruction1{};
      AbstrInstr instruction2{};
      if (is64) {
        instruction1 = MOVSD_rf_rmf;
        instruction2 = MOVSD_rmf_rf;
      } else {
        instruction1 = MOVSS_rf_rmf;
        instruction2 = MOVSS_rmf_rf;
      }
      as_.INSTR(instruction1).setR(WasmABI::REGS::moveHelper).setM4RM(srcRegDisp.reg, srcRegDisp.disp)();
      as_.INSTR(instruction2).setR(WasmABI::REGS::moveHelper).setM4RM(dstRegDisp.reg, dstRegDisp.disp)();
    }
  }
}

// Requests a target
StackElement Backend::reqSpillTarget(StackElement const &source, RegMask const protRegs, bool const forceToStack, bool const presFlags) {
  static_cast<void>(presFlags);

  RegAllocTracker tempRegAllocTracker{};
  tempRegAllocTracker.writeProtRegs = protRegs;
  MachineType const type{moduleInfo_.getMachineType(&source)};
  if (!forceToStack) {
    // May fail
    REG const reg{common_.reqFreeScratchRegProt(type, tempRegAllocTracker)};
    if (reg != REG::NONE) {
      return StackElement::scratchReg(reg, MachineTypeUtil::toStackTypeFlag(type));
    }
  }

  uint32_t const newOffset{common_.findFreeTempStackSlot(StackElement::tempStackSlotSize)};
  assert(newOffset <= moduleInfo_.fnc.stackFrameSize + 8U);
  if (newOffset > moduleInfo_.fnc.stackFrameSize) {
    uint32_t const newAlignedStackFrameSize{as_.alignStackFrameSize(newOffset + 32U)};
    as_.setStackFrameSize(newAlignedStackFrameSize);

#if ACTIVE_STACK_OVERFLOW_CHECK
    if (moduleInfo_.currentState.checkedStackFrameSize < newAlignedStackFrameSize) {
      moduleInfo_.currentState.checkedStackFrameSize = newAlignedStackFrameSize;
      if (!presFlags) {
        as_.checkStackFence();
      } else {
        REG flagStorageReg = common_.reqFreeScratchRegProt(MachineType::I64, tempRegAllocTracker);
        bool const haveFreeRegister = flagStorageReg != REG::NONE;

        static_assert(BD::FromEnd::spillSize >= 8, "Spill region not large enough");
        if (!haveFreeRegister) {
          as_.INSTR(MOV_rm64_r64).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::spillRegion).setR(REG::A)();
          flagStorageReg = REG::A;
        }

        // Store the CPU flags because they will be clobbered by checkStackFence
        if (flagStorageReg != REG::A) {
          as_.INSTR(MOV_r64_rm64).setR(flagStorageReg).setR4RM(REG::A)();
        }
        as_.INSTR(LAHF_T)();

        as_.checkStackFence();

        // Restore the CPU flags
        as_.INSTR(SAHF_T)();
        if (flagStorageReg != REG::A) {
          as_.INSTR(MOV_r64_rm64).setR(REG::A).setR4RM(flagStorageReg)();
        }
        if (!haveFreeRegister) {
          as_.INSTR(MOV_r64_rm64).setR(REG::A).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::spillRegion)();
        }
      }
    }
#endif
  }

  StackElement const tempStackElement{
      StackElement::tempResult(type, VariableStorage::stackMemory(type, newOffset), moduleInfo_.getStackMemoryReferencePosition())};
  return tempStackElement;
}

REG Backend::allocateRegForGlobal(MachineType const type) VB_NOEXCEPT {
  assert(((moduleInfo_.fnc.numLocalsInGPR == 0) && (moduleInfo_.fnc.numLocalsInFPR == 0)) && "Cannot allocate globals after locals");
  assert(type != MachineType::INVALID);
  assert(!compiler_.getDebugMode());
  REG chosenReg{REG::NONE};

  if (MachineTypeUtil::isInt(type)) {
    chosenReg = WasmABI::gpr[moduleInfo_.numGlobalsInGPR];
    moduleInfo_.numGlobalsInGPR++;
  }

  return chosenReg;
}

// Creates a LocalDef object that represents the storage location of a local
// and/or param Interleaving of params and locals is prohibited during allocation
void Backend::allocateLocal(MachineType const type, bool const isParam, uint32_t const multiplicity) {
  assert(type != MachineType::INVALID);
  assert((!isParam || moduleInfo_.fnc.numParams == moduleInfo_.fnc.numLocals) &&
         "Must not interleave params and locals. Allocation of params must be finished before allocating locals");
  // Guaranteed by caller
  assert(static_cast<uint64_t>(moduleInfo_.fnc.numLocals - moduleInfo_.fnc.numParams) + multiplicity <= ImplementationLimits::numDirectLocals &&
         "Too many locals");

  memory_.step(multiplicity * static_cast<uint32_t>(sizeof(ModuleInfo::LocalDef)));

  for (uint32_t i{0U}; i < multiplicity; i++) {
    // Choose a register for the allocation if there is still one left
    REG chosenReg{REG::NONE};
    bool const mustOnStack{compiler_.getDebugMode()};
    if (!mustOnStack) {
      if (MachineTypeUtil::isInt(type)) {
        if (moduleInfo_.fnc.numLocalsInGPR < (isParam ? WasmABI::regsForParams : moduleInfo_.getMaxNumsLocalsInGPRs())) {
          chosenReg = WasmABI::gpr[moduleInfo_.getLocalStartIndexInGPRs() + moduleInfo_.fnc.numLocalsInGPR];
          moduleInfo_.fnc.numLocalsInGPR++;
        }
      } else {
        if (moduleInfo_.fnc.numLocalsInFPR < (isParam ? WasmABI::regsForParams : moduleInfo_.getMaxNumsLocalsInFPRs())) {
          chosenReg = WasmABI::fpr[moduleInfo_.getLocalStartIndexInFPRs() + moduleInfo_.fnc.numLocalsInFPR];
          moduleInfo_.fnc.numLocalsInFPR++;
        }
      }
    }
    ModuleInfo::LocalDef &localDef{moduleInfo_.localDefs[moduleInfo_.fnc.numLocals + i]};
    localDef.reg = chosenReg;
    localDef.type = type;
    if (chosenReg == REG::NONE) {
      moduleInfo_.fnc.stackFrameSize += 8U;
      localDef.stackFramePosition = moduleInfo_.fnc.stackFrameSize;
      if (isParam) {
        moduleInfo_.fnc.paramWidth += 8U;
      } else {
        moduleInfo_.fnc.directLocalsWidth += 8U;
      }
    }
    localDef.currentStorageType = mustOnStack ? StorageType::STACKMEMORY : ModuleInfo::LocalDef::getInitializedStorageType(chosenReg, isParam);
  }

  moduleInfo_.fnc.numLocals += multiplicity;
  // Possibly increment number of params
  if (isParam) {
    moduleInfo_.fnc.numParams += multiplicity;
  }
}

void Backend::tryPushStacktraceAndDebugEntry(uint32_t const fncIndex, uint32_t const storeOffsetFromSP, uint32_t const offsetToStartOfFrame,
                                             uint32_t const bytecodePosOfLastParsedInstruction, REG const scratchReg) const {
  static_assert(Widths::stacktraceRecord == 16U, "Stacktrace record width not suitable");
  static_assert(Widths::debugInfo == 8U, "Debug info width not suitable");
  if (!compiler_.shallRecordStacktrace()) {
    return;
  }

  //
  // DEBUG
  //
  if (compiler_.getDebugMode()) {
    // Store offset to start of frame and position of last call in the bytecode to the stack
    as_.INSTR(MOV_rm32_imm32).setM4RM(REG::SP, static_cast<int32_t>(storeOffsetFromSP) + 12).setImm32(offsetToStartOfFrame)();
    as_.INSTR(MOV_rm32_imm32).setM4RM(REG::SP, static_cast<int32_t>(storeOffsetFromSP) + 16).setImm32(bytecodePosOfLastParsedInstruction)();
  }

  //
  // STACKTRACE
  //

  // Load old frame ref pointer from job memory
  as_.INSTR(MOV_r64_rm64).setR(scratchReg).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::lastFrameRefPtr)();

  // Store old frame ref pointer and function index to stack
  as_.INSTR(MOV_rm64_r64).setM4RM(REG::SP, static_cast<int32_t>(storeOffsetFromSP)).setR(scratchReg)();
  // Don't write if it's an unknown index. In that case it will be patched later anyway
  if (fncIndex != UnknownIndex) {
    as_.INSTR(MOV_rm32_imm32).setM4RM(REG::SP, static_cast<int32_t>(storeOffsetFromSP) + 8).setImm32(fncIndex)();
  }

  // Calculate new frame ref pointer (SP + spOffset)
  as_.INSTR(LEA_r64_m_t).setR(scratchReg).setM4RM(REG::SP, static_cast<int32_t>(storeOffsetFromSP))();
  // Store to job memory last so everything else is on the stack in case we are running into a stack overflow here ->
  // then the ref should point to the last one)
  as_.INSTR(MOV_rm64_r64).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::lastFrameRefPtr).setR(scratchReg)();
}

void Backend::tryPopStacktraceAndDebugEntry(uint32_t const storeOffsetFromSP, REG const scratchReg) const {
  if (!compiler_.shallRecordStacktrace()) {
    return;
  }

  // Load previous frame ref ptr and store to job memory
  as_.INSTR(MOV_r64_rm64).setR(scratchReg).setM4RM(REG::SP, static_cast<int32_t>(storeOffsetFromSP))();
  as_.INSTR(MOV_rm64_r64).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::lastFrameRefPtr).setR(scratchReg)();
}

void Backend::tryPatchFncIndexOfLastStacktraceEntry(uint32_t const fncIndex, REG const scratchReg) const {
  if (!compiler_.shallRecordStacktrace()) {
    return;
  }

  // Load old frame ref pointer from job memory
  as_.INSTR(MOV_r64_rm64).setR(scratchReg).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::lastFrameRefPtr)();

  // Store function index to last entry
  as_.INSTR(MOV_rm32_imm32).setM4RM(scratchReg, 8).setImm32(fncIndex)();
}

/// @details  Called when a function is entered, i.e. when/before a function body starts
/// Will go through previously emitted branches to that function and patch them
void Backend::enteredFunction() {
  moduleInfo_.setupReferenceMap(memory_);

  // Get last binary offset where function entry should be patched into
  // Then save current offset as wrapper start, because the (following) function with
  // the current function index, adhering to the Wasm calling convention, will begin at the current offset
  uint32_t &lastBranchToFnc{moduleInfo_.wasmFncBodyBinaryPositions[moduleInfo_.fnc.index]};
  finalizeBranch(lastBranchToFnc);
  lastBranchToFnc = output_.size();

  // Allocate and initialize stack for locals
  uint32_t const newStackFrameSize{as_.alignStackFrameSize(moduleInfo_.fnc.stackFrameSize + moduleInfo_.fnc.directLocalsWidth + 128U)};

#if !ACTIVE_STACK_OVERFLOW_CHECK
  uint32_t const stackFrameDelta{newStackFrameSize - moduleInfo_.fnc.stackFrameSize};
  as_.probeStack(stackFrameDelta, callScrRegs[0], callScrRegs[1]);
#endif
  as_.setStackFrameSize(newStackFrameSize);
#if ACTIVE_STACK_OVERFLOW_CHECK
  moduleInfo_.currentState.checkedStackFrameSize = moduleInfo_.fnc.stackFrameSize;
  as_.checkStackFence();
#endif

  // Patch the function index in case this was an indirect call, we aren't sure, especially if tables are mutable at
  // some point so we do it unconditionally
  tryPatchFncIndexOfLastStacktraceEntry(moduleInfo_.fnc.index, callScrRegs[0]);

  if (compiler_.getDebugMode()) {
    // Skip params for initialization, they are passed anyway
    for (uint32_t localIdx{moduleInfo_.fnc.numParams}; localIdx < moduleInfo_.fnc.numLocals; localIdx++) {
      StackElement const localElem{StackElement::local(localIdx)};
      VariableStorage const localStorage{moduleInfo_.getStorage(localElem)};
      emitMoveImpl(localStorage, VariableStorage::zero(moduleInfo_.localDefs[localIdx].type), false);
    }
  }
}

// Shall be called when a block (i.e. if/block/loop) is opened
// On entering a block, we spill all scratch registers so they can be used
// within the block
void Backend::spillAllVariables(Stack::iterator const below) const {
  for (uint32_t i{0U}; i < moduleInfo_.fnc.numLocals; i++) {
    spillFromStack(StackElement::local(i), RegMask::none(), true, false, below);
  }

  iterateScratchRegsAndGlobals(FunctionRef<void(StackElement const &)>([this, below](StackElement const &element) {
    spillFromStack(element, RegMask::none(), true, false, below);
  }));
}

#if INTERRUPTION_REQUEST
void Backend::checkForInterruptionRequest() const {
  as_.INSTR(CMP_rm8_imm8).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::statusFlags).setImm8(0x0U)();

  RelPatchObj const relPatchObj{as_.prepareJMP(true, CC::E)};
  // Retrieve the trapCode from the actual flag
  as_.INSTR(MOVZX_r32_rm8_t).setR(WasmABI::REGS::trapReg).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::statusFlags)();
  as_.TRAP(TrapCode::NONE, false);
  relPatchObj.linkToHere();
}
#endif

void Backend::iterateScratchRegsAndGlobals(FunctionRef<void(StackElement const &)> const &lambda) const {
  for (uint32_t regPos{moduleInfo_.getNumStaticallyAllocatedGPRs()}; regPos < WasmABI::gpr.size(); regPos++) {
    lambda(StackElement::scratchReg(WasmABI::gpr[regPos], StackType::SANULL));
  }
  for (uint32_t regPos{moduleInfo_.getNumStaticallyAllocatedFPRs()}; regPos < WasmABI::fpr.size(); regPos++) {
    lambda(StackElement::scratchReg(WasmABI::fpr[regPos], StackType::SANULL));
  }

  for (uint32_t globalIdx{0U}; globalIdx < moduleInfo_.numNonImportedGlobals; globalIdx++) {
    lambda(StackElement::global(globalIdx));
  }
}

// Produce code for calling a function including setting up all arguments and emitting the actual call instruction
void Backend::execDirectFncCall(uint32_t const fncIndex) {
  bool const imported{moduleInfo_.functionIsImported(fncIndex)};
  assert((!imported || (!moduleInfo_.functionIsBuiltin(fncIndex))) && "Builtin functions can only be executed by execBuiltinFncCall");
  assert((!imported || fncIndex != UnknownIndex) && "Need to provide fncIndex for imports");

  uint32_t const sigIndex{moduleInfo_.getFncSigIndex(fncIndex)};
  Stack::iterator const paramsBase{common_.prepareCallParamsAndSpillContext(sigIndex, false)};

  // Load the parameters etc., set up everything then emit the actual call
  if (moduleInfo_.functionIsV2Import(fncIndex)) {
    common_.moveGlobalsToLinkData();
    DirectV2Import v2ImportCall{*this, sigIndex};
    v2ImportCall.iterateParams(paramsBase);
    uint32_t const jobMemPtrPtrOffset{v2ImportCall.getJobMemoryPtrPtrOffset()};
    // coverity[autosar_cpp14_a5_1_9_violation]
    v2ImportCall.emitFncCallWrapper(fncIndex, FunctionRef<void()>([this, fncIndex, jobMemPtrPtrOffset]() {
#if LINEAR_MEMORY_BOUNDS_CHECKS
                                      cacheJobMemoryPtrPtr(jobMemPtrPtrOffset, callScrRegs[0]);
#endif
                                      emitRawFunctionCall(fncIndex);
#if LINEAR_MEMORY_BOUNDS_CHECKS
                                      restoreFromJobMemoryPtrPtr(jobMemPtrPtrOffset);
#endif
#if INTERRUPTION_REQUEST
                                      checkForInterruptionRequest();
#endif
                                    }));

#if LINEAR_MEMORY_BOUNDS_CHECKS
    as_.INSTR(MOV_r32_rm32).setR(WasmABI::REGS::memSize).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::actualLinMemByteSize)();
    as_.INSTR(SUB_rm64_imm8sx).setR4RM(WasmABI::REGS::memSize).setImm8(8_U8)();
#endif
    common_.recoverGlobalsToRegs();
    v2ImportCall.iterateResults();
  } else if (imported) {
    // Direct call to V1 import native function
    common_.moveGlobalsToLinkData();
    ImportCallV1 importCallV1Impl{*this, sigIndex};

    RegMask const availableLocalsRegMask{common_.saveLocalsAndParamsForFuncCall(true)};
    static_cast<void>(importCallV1Impl.iterateParams(paramsBase, availableLocalsRegMask));
    importCallV1Impl.prepareCtx();
    importCallV1Impl.resolveRegisterCopies();
    uint32_t const jobMemPtrPtrOffset{importCallV1Impl.getJobMemoryPtrPtrOffset()};
    // coverity[autosar_cpp14_a5_1_9_violation]
    importCallV1Impl.emitFncCallWrapper(fncIndex, FunctionRef<void()>([this, fncIndex, jobMemPtrPtrOffset]() {
                                          static_cast<void>(jobMemPtrPtrOffset);
#if LINEAR_MEMORY_BOUNDS_CHECKS
                                          cacheJobMemoryPtrPtr(jobMemPtrPtrOffset, callScrRegs[0]);
#endif
                                          emitRawFunctionCall(fncIndex);
#if LINEAR_MEMORY_BOUNDS_CHECKS
                                          restoreFromJobMemoryPtrPtr(jobMemPtrPtrOffset);
#endif
#if INTERRUPTION_REQUEST
                                          checkForInterruptionRequest();
#endif
                                        }));

#if LINEAR_MEMORY_BOUNDS_CHECKS
    as_.INSTR(MOV_r32_rm32).setR(WasmABI::REGS::memSize).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::actualLinMemByteSize)();
    as_.INSTR(SUB_rm64_imm8sx).setR4RM(WasmABI::REGS::memSize).setImm8(8_U8)();
#endif

    common_.recoverGlobalsToRegs();
    importCallV1Impl.iterateResults();
  } else {
    // Direct call to a Wasm function
    InternalCall directWasmCallImpl{*this, sigIndex};

    RegMask const availableLocalsRegMask{common_.saveLocalsAndParamsForFuncCall(false)};
    static_cast<void>(directWasmCallImpl.iterateParams(paramsBase, availableLocalsRegMask));
    directWasmCallImpl.resolveRegisterCopies();
    // coverity[autosar_cpp14_a5_1_9_violation]
    directWasmCallImpl.emitFncCallWrapper(fncIndex, FunctionRef<void()>([this, fncIndex]() {
                                            emitRawFunctionCall(fncIndex);
                                          }));
#if LINEAR_MEMORY_BOUNDS_CHECKS
    as_.INSTR(MOV_r32_rm32).setR(WasmABI::REGS::memSize).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::actualLinMemByteSize)();
    as_.INSTR(SUB_rm64_imm8sx).setR4RM(WasmABI::REGS::memSize).setImm8(8_U8)();
#endif
    directWasmCallImpl.iterateResults();
  }
}

// Emit code for an inlined indirect call to a Wasm function(including adapted importFnc)
void Backend::execIndirectWasmCall(uint32_t const sigIndex, uint32_t const tableIndex) {
  static_cast<void>(tableIndex);
  assert(moduleInfo_.hasTable && tableIndex == 0 && "Table not defined");
  Stack::iterator const paramsBase{common_.prepareCallParamsAndSpillContext(sigIndex, true)};

  InternalCall indirectCallImpl{*this, sigIndex};
  RegMask const availableLocalsRegMask{common_.saveLocalsAndParamsForFuncCall(false)};
  Stack::iterator const indirectCallIndex{indirectCallImpl.iterateParams(paramsBase, availableLocalsRegMask)};
  indirectCallImpl.handleIndirectCallReg(indirectCallIndex, availableLocalsRegMask);
  indirectCallImpl.resolveRegisterCopies();

  indirectCallImpl.emitFncCallWrapper(
      UnknownIndex, FunctionRef<void()>([this, sigIndex]() {
        // Trap if EDX (Register where target table index is stored) is greater than the tableSize
        as_.INSTR(CMP_rm32_imm32).setR4RM(WasmABI::REGS::indirectCallReg).setImm32(moduleInfo_.tableInitialSize)();
        as_.cTRAP(TrapCode::INDIRECTCALL_OUTOFBOUNDS, CC::AE);

        // Load pointer to end of binary to RAX and
        // then load the type/signature index from table in binary; note that binaryTableOffset is negative
        as_.INSTR(MOV_r64_rm64).setR(callScrRegs[0]).setM4RM(WasmABI::REGS::linMem, -Basedata::FromEnd::tableAddressOffset)();
        as_.INSTR(LEA_r64_m_t).setR(callScrRegs[0]).setM4RM(callScrRegs[0], 0, WasmABI::REGS::indirectCallReg, 3U)();

        // Compare signature in table with given signature and trap if it doesn't match
        as_.INSTR(CMP_rm32_imm32).setM4RM(callScrRegs[0], 4).setImm32(sigIndex)();
        as_.cTRAP(TrapCode::INDIRECTCALL_WRONGSIG, CC::NE);

        // Signature matches

        // Load the offset where the function at this table index starts
        as_.INSTR(MOV_r32_rm32).setR(callScrRegs[1]).setM4RM(callScrRegs[0], 0)();

        // Check if the offset is zero which means the function is not linked
        as_.INSTR(CMP_rm32_imm32).setR4RM(callScrRegs[1]).setImm32(0U)();
        as_.cTRAP(TrapCode::CALLED_FUNCTION_NOT_LINKED, CC::E);

        // Otherwise calculate the absolute address and execute the call
        // Subtract the offset from the current position of the table element; MSBs of R15 are zero anyway due to the mov

        // callScrRegs[0] = startAddressOfModuleBinary
        as_.INSTR(MOV_r64_rm64).setR(callScrRegs[0]).setM4RM(WasmABI::REGS::linMem, -Basedata::FromEnd::binaryModuleStartAddressOffset)();

        as_.INSTR(ADD_r64_rm64).setR(callScrRegs[0]).setR4RM(callScrRegs[1])();
        as_.INSTR(CALL_rm64_t).setR4RM(callScrRegs[0])();
      }));

#if LINEAR_MEMORY_BOUNDS_CHECKS
  as_.INSTR(MOV_r32_rm32).setR(WasmABI::REGS::memSize).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::actualLinMemByteSize)();
  as_.INSTR(SUB_rm64_imm8sx).setR4RM(WasmABI::REGS::memSize).setImm8(8_U8)();
#endif
  indirectCallImpl.iterateResults();
}

uint32_t Backend::getStackParamWidth(uint32_t const sigIndex, bool const imported) const VB_NOEXCEPT {
  RegStackTracker tracker{};
  uint32_t stackParamWidth{0U};
  moduleInfo_.iterateParamsForSignature(
      sigIndex, FunctionRef<void(MachineType)>([this, imported, &tracker, &stackParamWidth](MachineType const paramType) VB_NOEXCEPT {
        REG const targetReg{getREGForArg(paramType, imported, tracker)};
        if (targetReg == REG::NONE) {
          stackParamWidth += 8U;
        }
      }));
  if (imported) {
    REG const targetReg{getREGForArg(MachineType::I64, true, tracker)};
    if (targetReg == REG::NONE) {
      stackParamWidth += 8U;
    }
  }
  return stackParamWidth;
}

/// @brief The offset between the address where the trap code is stored with and REG::SP.
static constexpr uint32_t of_trapCodePtr_trapReentryPoint{0U};

// For calling exported functions from C++
void Backend::emitFunctionEntryPoint(uint32_t const fncIndex) {
  assert(fncIndex < moduleInfo_.numTotalFunctions && "Function out of range");
  bool const imported{fncIndex < moduleInfo_.numImportedFunctions};

  // 8B for return address
  uint32_t currentFrameOffset{8U};

  // Reserve space on stack and spill non volatile registers
  as_.INSTR(SUB_rm64_imm32sx).setR4RM(REG::SP).setImm32(static_cast<uint32_t>(NABI::nonvolRegs.size()) * 8U)();
#if ACTIVE_STACK_OVERFLOW_CHECK
  // Manual implementation because neither base pointer nor trap support is set up at this point
  as_.INSTR(CMP_r64_rm64).setR(REG::SP).setM4RM(NABI::gpParams[1], -Basedata::FromEnd::stackFence)();
  RelPatchObj const inRange = as_.prepareJMP(true, CC::AE);
  // gpParams[2] contains the pointer to a variable where the TrapCode will be stored
  as_.INSTR(MOV_rm32_imm32).setM4RM(NABI::gpParams[2], 0).setImm32(static_cast<uint32_t>(TrapCode::STACKFENCEBREACHED))();
  as_.INSTR(ADD_rm64_imm32sx).setR4RM(REG::SP).setImm32(static_cast<uint32_t>(NABI::nonvolRegs.size()) * 8U)();
  as_.INSTR(RET_t)();
  inRange.linkToHere();
#endif
  currentFrameOffset += (static_cast<uint32_t>(NABI::nonvolRegs.size()) * 8U);
  spillRestoreRegsRaw({NABI::nonvolRegs.data(), NABI::nonvolRegs.size()});

  // Nove pointer to serialized arguments from first argument and linMem register from second function argument to the
  // register where all the code will expect it to be
  as_.INSTR(MOV_r64_rm64).setR(callScrRegs[2]).setR4RM(NABI::gpParams[0])();
  as_.INSTR(MOV_r64_rm64).setR(WasmABI::REGS::linMem).setR4RM(NABI::gpParams[1])();

#if LINEAR_MEMORY_BOUNDS_CHECKS
  // Set up actual linear memory size cache (minus 8)
  as_.INSTR(MOV_r32_rm32).setR(WasmABI::REGS::memSize).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::actualLinMemByteSize)();
  as_.INSTR(SUB_rm64_imm8sx).setR4RM(WasmABI::REGS::memSize).setImm8(8_U8)();
#endif

  //
  //

  common_.recoverGlobalsToRegs();

  // We are setting up the following stack structure from here on
  // When a trap is executed, we load the trapCode (uint32) into EAX, then unwind the stack to the unwind target (which
  // is stored in link data), then execute RET
  // RSP <------------ Stack growth direction (downwards) v <- unwind target
  // | &trapCode | Stacktrace Record + Debug Info | cachedJobMemoryPtrPtr | returnValuesPtr
  uint32_t const sigIndex{moduleInfo_.getFncSigIndex(fncIndex)};

  constexpr uint32_t of_stacktraceRecordAndDebugInfo{of_trapCodePtr_trapReentryPoint + 8U};
  constexpr uint32_t of_cachedJobMemoryPtrPtr{of_stacktraceRecordAndDebugInfo + Widths::stacktraceRecord + Widths::debugInfo};
  constexpr uint32_t of_returnValuesPtr{of_cachedJobMemoryPtrPtr + Widths::jobMemoryPtrPtr};
  constexpr uint32_t of_post{(of_returnValuesPtr + 8U)};
  constexpr uint32_t totalReserved{roundUpToPow2(of_post, 3U)};

  // Reserve space on stack for Unwind Target and, if imported, Stacktrace Record
  as_.INSTR(SUB_rm64_imm8sx).setR4RM(REG::SP).setImm8(static_cast<uint8_t>(totalReserved))(); // SP small change
  currentFrameOffset += totalReserved;

  uint32_t const stackParamWidth{getStackParamWidth(sigIndex, imported)};
  uint32_t const shadowSpaceSize{imported ? NABI::shadowSpaceSize : 0U};
  uint32_t const stackReturnValueWidth{common_.getStackReturnValueWidth(sigIndex)};
  uint32_t const padding{deltaToNextPow2(shadowSpaceSize + stackParamWidth + currentFrameOffset + stackReturnValueWidth, 4U)};
  uint32_t const reservationFunctionCall{shadowSpaceSize + stackParamWidth + stackReturnValueWidth + padding};

  uint32_t const offsetToStartOfFrame{padding + of_stacktraceRecordAndDebugInfo};
  constexpr uint32_t bytecodePos{0U}; // Zero because we are in a wrapper/helper here, not an actual function body described by Wasm
  tryPushStacktraceAndDebugEntry(fncIndex, of_stacktraceRecordAndDebugInfo, offsetToStartOfFrame, bytecodePos, callScrRegs[0]);
  if (imported) {
#if LINEAR_MEMORY_BOUNDS_CHECKS
    cacheJobMemoryPtrPtr(of_cachedJobMemoryPtrPtr, callScrRegs[0]);
#endif
  }

  // gpParams[2] contains the pointer to a variable where the TrapCode will be stored
  as_.INSTR(MOV_rm64_r64).setM4RM(REG::SP, static_cast<int32_t>(of_trapCodePtr_trapReentryPoint)).setR(NABI::gpParams[2])();

  // gpParams[3] contains the pointer to an area where the returnValues will be stored
  as_.INSTR(MOV_rm64_r64).setM4RM(REG::SP, static_cast<int32_t>(of_returnValuesPtr)).setR(NABI::gpParams[3])();

  // If saved stack pointer is not zero, this runtime already has an active frame and is already executing
  as_.INSTR(CMP_rm32_imm8sx).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::trapStackReentry).setImm8(0_U8)();
  RelPatchObj const alreadyExecuting{as_.prepareJMP(true, CC::NE)};

  //
  //
  // NOT ALREADY EXECUTING START
  //
  //

  // Store unwind target to link data if this is the first frame
  as_.INSTR(MOV_rm64_r64).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::trapStackReentry).setR(REG::SP)();

  // Load instruction pointer of trap reentry instruction pointer
  RelPatchObj const trapEntryAdr{as_.preparePCRelAddrLEA(callScrRegs[0])};
  as_.INSTR(MOV_rm64_r64).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::trapHandlerPtr).setR(callScrRegs[0])();

// If it is enabled, store the native stack fence
#if MAX_WASM_STACKSIZE_BEFORE_NATIVE_CALL != 0 && !ACTIVE_STACK_OVERFLOW_CHECK
  // Subtract constant from SP and store it in link data
  as_.INSTR(MOV_rm64_r64).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::nativeStackFence).setR(REG::SP)();
  as_.INSTR(SUB_rm64_imm32sx)
      .setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::nativeStackFence)
      .setImm32(static_cast<uint32_t>(MAX_WASM_STACKSIZE_BEFORE_NATIVE_CALL))();
#elif STACKSIZE_LEFT_BEFORE_NATIVE_CALL != 0 && ACTIVE_STACK_OVERFLOW_CHECK
  as_.INSTR(MOV_r64_rm64).setR(callScrRegs[0]).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::stackFence)();
  as_.INSTR(ADD_rm64_imm32sx).setR4RM(callScrRegs[0]).setImm32(static_cast<uint32_t>(STACKSIZE_LEFT_BEFORE_NATIVE_CALL))();
  // Overflow check is performed in Runtime::setStackFence()
  as_.INSTR(MOV_rm64_r64).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::nativeStackFence).setR(callScrRegs[0])();
#endif

  //
  //
  // NOT ALREADY EXECUTING STOP
  //
  //

  alreadyExecuting.linkToHere();

  //
  //
  if (reservationFunctionCall > 0U) {
    as_.INSTR(SUB_rm64_imm32sx).setR4RM(REG::SP).setImm32(reservationFunctionCall)();
  }
#if ACTIVE_STACK_OVERFLOW_CHECK
  as_.checkStackFence();
#endif
  currentFrameOffset += reservationFunctionCall;
  assert(((currentFrameOffset % 16U) == 0U) && "Stack before call not aligned to 16B boundary");

  // Load arguments from serialization buffer to registers and stack according to Wasm and native ABI, respectively
  RegStackTracker tracker{};
  uint32_t serOffset{0U};
  moduleInfo_.iterateParamsForSignature(
      sigIndex, FunctionRef<void(MachineType)>([this, imported, stackParamWidth, shadowSpaceSize, &serOffset, &tracker](MachineType const paramType) {
        bool const is64{MachineTypeUtil::is64(paramType)};
        REG const targetReg{getREGForArg(paramType, imported, tracker)};
        if (targetReg != REG::NONE) {
          as_.INSTR(MOV_r_rm(RegUtil::isGPR(targetReg), is64)).setR(targetReg).setM4RM(callScrRegs[2], static_cast<int32_t>(serOffset))();
        } else {
          uint32_t const offsetFromSP{shadowSpaceSize + offsetInStackArgs(imported, stackParamWidth, tracker)};

          as_.INSTR(is64 ? MOV_r64_rm64 : MOV_r32_rm32).setR(callScrRegs[0]).setM4RM(callScrRegs[2], static_cast<int32_t>(serOffset))();

          as_.INSTR(is64 ? MOV_rm64_r64 : MOV_rm32_r32).setM4RM(REG::SP, static_cast<int32_t>(offsetFromSP)).setR(callScrRegs[0])();
        }
        serOffset += 8U;
      })); // LCOV_EXCL_LINE

  if (imported) {
    REG const targetReg{getREGForArg(MachineType::I64, true, tracker)};
    if (targetReg != REG::NONE) {
      as_.INSTR(MOV_r64_rm64).setR(targetReg).setM4RM(WasmABI::REGS::linMem, -Basedata::FromEnd::customCtxOffset)();
    } else {
      uint32_t const offsetFromSP{shadowSpaceSize + offsetInStackArgs(imported, stackParamWidth, tracker)};
      as_.INSTR(MOV_r64_rm64).setR(callScrRegs[0]).setM4RM(WasmABI::REGS::linMem, -Basedata::FromEnd::customCtxOffset)();

      as_.INSTR(MOV_rm64_r64).setM4RM(REG::SP, static_cast<int32_t>(offsetFromSP)).setR(callScrRegs[0])();
    }
  }

  assert(tracker.allocatedStackBytes == stackParamWidth && "Stack allocation size mismatch");

  // Check whether we are dealing with a builtin function
  if (moduleInfo_.functionIsBuiltin(fncIndex)) {
    throw FeatureNotSupportedException(ErrorCode::Cannot_export_builtin_function);
  }

  // Emit the actual function call
  emitRawFunctionCall(fncIndex);

  uint32_t index{0U};
  RegStackTracker returnValueTracker{};
  uint32_t const returnValuePtrDisp{of_returnValuesPtr + reservationFunctionCall};
  as_.INSTR(MOV_r64_rm64).setR(callScrRegs[1]).setM4RM(REG::SP, static_cast<int32_t>(returnValuePtrDisp))();
  moduleInfo_.iterateResultsForSignature(
      sigIndex,
      FunctionRef<void(MachineType)>([this, stackParamWidth, shadowSpaceSize, &index, &returnValueTracker](MachineType const returnValueType) {
        bool const is64{MachineTypeUtil::is64(returnValueType)};
        REG const srcReg{getREGForReturnValue(returnValueType, returnValueTracker)};
        uint32_t const returnValueDisp{index * 8U};
        if (srcReg != REG::NONE) {
          if (MachineTypeUtil::isInt(returnValueType)) {
            as_.INSTR(is64 ? MOV_rm64_r64 : MOV_rm32_r32).setM4RM(callScrRegs[1], static_cast<int32_t>(returnValueDisp)).setR(srcReg)();
          } else {
            as_.INSTR(is64 ? MOVSD_rmf_rf : MOVSS_rmf_rf).setM4RM(callScrRegs[1], static_cast<int32_t>(returnValueDisp)).setR(srcReg)();
          }
        } else {
          uint32_t const offsetFromSP{shadowSpaceSize + stackParamWidth + offsetInStackReturnValues(returnValueTracker, returnValueType)};
          as_.INSTR(is64 ? MOV_r64_rm64 : MOV_r32_rm32).setR(callScrRegs[0]).setM4RM(REG::SP, static_cast<int32_t>(offsetFromSP))();
          as_.INSTR(is64 ? MOV_rm64_r64 : MOV_rm32_r32).setM4RM(callScrRegs[1], static_cast<int32_t>(returnValueDisp)).setR(callScrRegs[0])();
        }
        index++;
      }));

  // Remove shadow space, arguments, Reentry IP and &trapCode from stack again (This point is not reached via a trap, so
  // they are still on the stack)
  if (reservationFunctionCall > 0U) {
    as_.INSTR(ADD_rm64_imm32sx).setR4RM(REG::SP).setImm32(reservationFunctionCall)();
  }
  currentFrameOffset -= reservationFunctionCall;

  // Now unwind target and potentially the stacktrace record are still on stack

  if (imported) {
#if LINEAR_MEMORY_BOUNDS_CHECKS
    restoreFromJobMemoryPtrPtr(of_cachedJobMemoryPtrPtr);
#endif
  }
  tryPopStacktraceAndDebugEntry(of_stacktraceRecordAndDebugInfo, callScrRegs[0]);

  trapEntryAdr.linkToHere();

  common_.moveGlobalsToLinkData();

  // Compare the trap unwind target to the current stack pointer
  as_.INSTR(CMP_r64_rm64).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::trapStackReentry).setR(REG::SP)();
  // If this is equal, we can conclude this was the first frame in the call sequence and subsequently reset the stored
  // trap target
  RelPatchObj const notFirstFrame{as_.prepareJMP(true, CC::NE)};
  as_.INSTR(MOV_rm64_imm32sx).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::trapStackReentry).setImm32(0_U32)();
  as_.INSTR(MOV_rm64_imm32sx).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::trapHandlerPtr).setImm32(0_U32)();
  notFirstFrame.linkToHere();

  // Remove trap stack identifier and potentially stacktrace entry and cached jobMemoryPtrPtr (or padding)
  as_.INSTR(ADD_rm64_imm8sx).setR4RM(REG::SP).setImm8(static_cast<uint8_t>(totalReserved))();
  currentFrameOffset -= totalReserved;

  //
  //

  // Restore spilled registers
  spillRestoreRegsRaw({NABI::nonvolRegs.data(), NABI::nonvolRegs.size()}, true);

  as_.INSTR(ADD_rm64_imm32sx).setR4RM(REG::SP).setImm32(static_cast<uint32_t>(NABI::nonvolRegs.size()) * 8U)();
  currentFrameOffset -= static_cast<uint32_t>(NABI::nonvolRegs.size()) * 8U;
  static_cast<void>(currentFrameOffset);
  assert(currentFrameOffset == 8 && "Unaligned stack at end of wrapper call");
  as_.INSTR(RET_t)();
}

void Backend::spillRestoreRegsRaw(Span<REG const> const &regs, bool const restore, uint32_t const stackOffset) const {
  assert(regs.size() < INT32_MAX && "Count too high");
  for (size_t i{0U}; i < regs.size(); i++) {
    REG const reg{regs[i]};
    AbstrInstr instr{};
    if (restore) {
      instr = RegUtil::isGPR(reg) ? MOV_r64_rm64 : MOVSD_rf_rmf;
    } else {
      instr = RegUtil::isGPR(reg) ? MOV_rm64_r64 : MOVSD_rmf_rf;
    }

    assert(static_cast<uint64_t>(stackOffset) + static_cast<uint32_t>(i) * 8U < INT32_MAX && "Offset too large");
    as_.INSTR(instr).setM4RM(REG::SP, static_cast<int32_t>(stackOffset) + (static_cast<int32_t>(i) * 8)).setR(reg)();
  }
}

uint32_t Backend::offsetInStackArgs(bool const imported, uint32_t const paramWidth, RegStackTracker &tracker) VB_NOEXCEPT {
  uint32_t offset{0U};
  // coverity[autosar_cpp14_m0_1_2_violation]
  // coverity[autosar_cpp14_m0_1_9_violation]
  if (imported && (NABI::stackOrder == NABI::StackOrder::RTL)) {
    offset = tracker.allocatedStackBytes;
  } else {
    offset = (paramWidth - 8U) - tracker.allocatedStackBytes;
  }
  tracker.allocatedStackBytes += 8U;
  return offset;
}

REG Backend::getREGForArg(MachineType const paramType, bool const imported, RegStackTracker &tracker) const VB_NOEXCEPT {
  REG reg{REG::NONE};
  bool const useRegisters{imported || (!compiler_.getDebugMode())};
  if (useRegisters) {
    if (MachineTypeUtil::isInt(paramType)) {
      if (!imported) {
        if (tracker.allocatedGPR < WasmABI::regsForParams) {
          reg = WasmABI::gpr[moduleInfo_.getLocalStartIndexInGPRs() + tracker.allocatedGPR];
        }
      } else {
        uint32_t const allocatedRegCounter{(NativeABI::regArgAllocation == NativeABI::RegArgAllocation::MUTUAL)
                                               ? (tracker.allocatedGPR + tracker.allocatedFPR)
                                               : tracker.allocatedGPR};
        if (allocatedRegCounter < NABI::gpParams.size()) {
          reg = NABI::gpParams[allocatedRegCounter];
        }
      }
    } else {
      if (!imported) {
        if (tracker.allocatedFPR < WasmABI::regsForParams) {
          reg = WasmABI::fpr[moduleInfo_.getLocalStartIndexInFPRs() + tracker.allocatedFPR];
        }
      } else {
        uint32_t const allocatedRegCounter{(NativeABI::regArgAllocation == NativeABI::RegArgAllocation::MUTUAL)
                                               ? (tracker.allocatedGPR + tracker.allocatedFPR)
                                               : tracker.allocatedFPR};
        if (allocatedRegCounter < NABI::flParams.size()) {
          reg = NABI::flParams[allocatedRegCounter];
        }
      }
    }
  }

  // If nothing has matched, we allocate it on the stack
  if (reg != REG::NONE) {
    if (RegUtil::isGPR(reg)) {
      tracker.allocatedGPR++;
    } else {
      tracker.allocatedFPR++;
    }
  }
  return reg;
}

uint32_t Backend::offsetInStackReturnValues(RegStackTracker &tracker, MachineType const returnValueType) VB_NOEXCEPT {
  static_cast<void>(returnValueType);
  uint32_t const offset{tracker.allocatedStackBytes};
  tracker.allocatedStackBytes += 8U;
  return offset;
}

REG Backend::getREGForReturnValue(MachineType const returnValueType, RegStackTracker &tracker) const VB_NOEXCEPT {
  REG reg{REG::NONE};
  if (MachineTypeUtil::isInt(returnValueType)) {
    if (tracker.allocatedGPR < WasmABI::gpRegsForReturnValues) {
      reg = WasmABI::REGS::gpRetRegs[tracker.allocatedGPR];
      tracker.allocatedGPR++;
    }
  } else {
    if (tracker.allocatedFPR < WasmABI::fpRegsForReturnValues) {
      reg = WasmABI::REGS::fpRetRegs[tracker.allocatedFPR];
      tracker.allocatedFPR++;
    }
  }

  return reg;
}

void Backend::emitV1ImportAdapterImpl(uint32_t const fncIndex) {
  uint32_t const sigIndex{moduleInfo_.getFncSigIndex(fncIndex)};
  uint32_t const newStackParamWidth{getStackParamWidth(sigIndex, true)};
  uint32_t const oldStackParamWidth{getStackParamWidth(sigIndex, false)};

  // RSP <------------ Stack growth direction (downwards)
  // v <------------------------------------- totalReserved ----------------------------------> v
  // | Shadow Space | New Stack Params | (jobMemoryPtrPtr) | Padding | RET address (8B) | Old
  // Stack Args
  static constexpr uint32_t of_newStackParams{NABI::shadowSpaceSize};
  uint32_t const of_jobMemoryPtrPtr{of_newStackParams + newStackParamWidth};
  uint32_t const of_post{of_jobMemoryPtrPtr + Widths::jobMemoryPtrPtr};
  uint32_t const totalReserved{roundUpToPow2(of_post + 8U, 4U) - 8U}; // excluding 8 bytes ret address on stack, must be aligned before call
  uint32_t const of_oldStackParams{totalReserved + 8U};

  as_.INSTR(SUB_rm64_imm32sx).setR4RM(REG::SP).setImm32(totalReserved)();
#if ACTIVE_STACK_OVERFLOW_CHECK
  as_.checkStackFence();
#endif

  RegStackTracker srcTracker{}; // Reset tracker

  uint32_t offsetInOldStackParams{oldStackParamWidth - 8U};
  RegStackTracker targetTracker{};

  RegisterCopyResolver<NativeABI::gpParams.size()> registerCopyResolver{};
  // coverity[autosar_cpp14_a8_5_2_violation]
  auto const copyParamsCB = [this, &registerCopyResolver, of_oldStackParams, newStackParamWidth, &srcTracker, &offsetInOldStackParams,
                             &targetTracker](MachineType const paramType) {
    bool const is64{MachineTypeUtil::is64(paramType)};
    REG const sourceReg{getREGForArg(paramType, false, srcTracker)};   // Wasm ABI REG
    REG const targetReg{getREGForArg(paramType, true, targetTracker)}; // Native ABI REG
    if ((targetReg == sourceReg) && (targetReg != REG::NONE)) {
      return; // If the source and target are the same, we can skip the move
    }

    uint32_t sourceStackOffset{0U};
    uint32_t offsetFromSP{0U};
    if (sourceReg == REG::NONE) {
      sourceStackOffset = of_oldStackParams + offsetInOldStackParams;
      offsetInOldStackParams -= 8U;
    }

    if (targetReg == REG::NONE) {
      offsetFromSP = of_newStackParams + offsetInStackArgs(true, newStackParamWidth, targetTracker);
    }

    if (targetReg != REG::NONE) {
      if (sourceReg == targetReg) {
        // If the source and target are the same, we can skip the move
        return;
      }
      if (RegUtil::isGPR(targetReg)) {
        if (sourceReg != REG::NONE) {
          registerCopyResolver.push(VariableStorage::reg(paramType, targetReg), VariableStorage::reg(paramType, sourceReg));
        } else {
          registerCopyResolver.push(VariableStorage::reg(paramType, targetReg), VariableStorage::stackMemory(paramType, sourceStackOffset));
        }
      } else {
        if (sourceReg != REG::NONE) {
          as_.INSTR(MOV_r_rm(false, is64)).setR(targetReg).setR4RM(sourceReg)();
        } else {
          as_.INSTR(MOV_r_rm(false, is64)).setR(targetReg).setM4RM(REG::SP, static_cast<int32_t>(sourceStackOffset))();
        }
      }
    } else {
      if (sourceReg != REG::NONE) {
        as_.INSTR(MOV_rm_r(RegUtil::isGPR(sourceReg), is64)).setM4RM(REG::SP, static_cast<int32_t>(offsetFromSP)).setR(sourceReg)();
      } else {
        as_.INSTR(is64 ? MOV_r64_rm64 : MOV_r32_rm32).setR(callScrRegs[0]).setM4RM(REG::SP, static_cast<int32_t>(sourceStackOffset))();
        as_.INSTR(is64 ? MOV_rm64_r64 : MOV_rm32_r32).setM4RM(REG::SP, static_cast<int32_t>(offsetFromSP)).setR(callScrRegs[0])();
      }
    }
  };
  // coverity[autosar_cpp14_a5_1_4_violation]
  moduleInfo_.iterateParamsForSignature(sigIndex, FunctionRef<void(MachineType)>(copyParamsCB));

  REG const targetReg{getREGForArg(MachineType::I64, true, targetTracker)};
  if (targetReg != REG::NONE) {
    registerCopyResolver.push(VariableStorage::reg(MachineType::I64, targetReg),
                              VariableStorage::linkData(MachineType::I64, static_cast<uint32_t>(BD::FromEnd::customCtxOffset)));
  } else {
    uint32_t const offsetFromSP{of_newStackParams + offsetInStackArgs(true, newStackParamWidth, targetTracker)};
    as_.INSTR(MOV_r64_rm64).setR(callScrRegs[0]).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::customCtxOffset)();
    as_.INSTR(MOV_rm64_r64).setM4RM(REG::SP, static_cast<int32_t>(offsetFromSP)).setR(callScrRegs[0])();
  }

  registerCopyResolver.resolve(
      MoveEmitter([this](VariableStorage const &target, VariableStorage const &source) {
        // Can't use emitMoveIntImpl because it handles stack frame offset calculation differently
        bool const is64{MachineTypeUtil::is64(source.machineType)};
        if (source.type == StorageType::REGISTER) {
          as_.INSTR(MOV_r_rm(true, is64)).setR(target.location.reg).setR4RM(source.location.reg)();
        } else if (source.type == StorageType::STACKMEMORY) {
          as_.INSTR(MOV_r_rm(true, is64)).setR(target.location.reg).setM4RM(REG::SP, static_cast<int32_t>(source.location.stackFramePosition))();
        } else {
          as_.INSTR(MOV_r64_rm64).setR(target.location.reg).setM4RM(WasmABI::REGS::linMem, -static_cast<int32_t>(source.location.linkDataOffset))();
        }
      }),
      SwapEmitter(nullptr));

  assert(targetTracker.allocatedStackBytes == newStackParamWidth && "Stack allocation size mismatch");

  // Patch the last function index because this was reached via an indirect call and the function index isn't known
  tryPatchFncIndexOfLastStacktraceEntry(fncIndex, callScrRegs[0]);

#if LINEAR_MEMORY_BOUNDS_CHECKS
  cacheJobMemoryPtrPtr(of_jobMemoryPtrPtr, callScrRegs[0]);
#endif
  emitRawFunctionCall(fncIndex);
#if LINEAR_MEMORY_BOUNDS_CHECKS
  restoreFromJobMemoryPtrPtr(of_jobMemoryPtrPtr);
#endif

#if INTERRUPTION_REQUEST
  checkForInterruptionRequest();
#endif
  common_.recoverGlobalsToRegs();

  // Remove args from stack again (and trap ptr and reentry reference)
  as_.INSTR(ADD_rm64_imm32sx).setR4RM(REG::SP).setImm32(totalReserved)();
  as_.INSTR(RET_t)();
}

void Backend::emitV2ImportAdapterImpl(uint32_t const fncIndex) const {
  static_cast<void>(fncIndex);
  static_cast<void>(this);
  // Need handle muti return values to wasm style
  throw FeatureNotSupportedException(ErrorCode::Not_implemented);
}

// For calling imported functions via an indirect call
void Backend::emitWasmToNativeAdapter(uint32_t const fncIndex) {
  assert(fncIndex < moduleInfo_.numImportedFunctions && "Function is not imported");

  if (moduleInfo_.functionIsBuiltin(fncIndex)) {
    throw FeatureNotSupportedException(ErrorCode::Cannot_indirectly_call_builtin_functions);
  }

  common_.moveGlobalsToLinkData();

  bool const isV2Import{moduleInfo_.functionIsV2Import(fncIndex)};
  if (isV2Import) {
    emitV2ImportAdapterImpl(fncIndex);
  } else {
    emitV1ImportAdapterImpl(fncIndex);
  }
}

void Backend::emitRawFunctionCall(uint32_t const fncIndex) {
  if (fncIndex < moduleInfo_.numImportedFunctions) {
    // Calling an imported function
    ModuleInfo::ImpFuncDef const impFuncDef{moduleInfo_.getImpFuncDef(fncIndex)};
    assert(impFuncDef.builtinFunction == BuiltinFunction::UNDEFINED && "Builtin functions cannot be emitted this way, do it explicitly");

    if (!impFuncDef.linked) {
      as_.TRAP(TrapCode::CALLED_FUNCTION_NOT_LINKED);
      return;
    }

#if (MAX_WASM_STACKSIZE_BEFORE_NATIVE_CALL != 0 && !ACTIVE_STACK_OVERFLOW_CHECK) ||                                                                  \
    (STACKSIZE_LEFT_BEFORE_NATIVE_CALL != 0 && ACTIVE_STACK_OVERFLOW_CHECK)
    as_.INSTR(CMP_r64_rm64).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::nativeStackFence).setR(REG::SP)(); // Compare stack pointer to stack fence
    as_.cTRAP(TrapCode::STACKFENCEBREACHED, CC::LE);
#endif

    // We have to call an actual C++ host function
    NativeSymbol const &nativeSymbol{moduleInfo_.importSymbols[impFuncDef.symbolIndex]};
    if (nativeSymbol.linkage == NativeSymbol::Linkage::STATIC) {
      // A statically linked symbol where the address is known at compile time
      // Load the address as a constant into RAX and call it
      as_.MOVimm64(callScrRegs[0], bit_cast<uint64_t>(nativeSymbol.ptr)); // mov rax, 0x1231238019831857
      as_.INSTR(CALL_rm64_t).setR4RM(callScrRegs[0])();                   // call rax
    } else {
      // Load the offset (from start of linear memory/end of basedata) where the address of this function is stored in
      // the link data
      uint32_t const linkDataOffset{moduleInfo_.getBasedataLength() - BD::FromStart::linkData};
      uint32_t const offsetFromEnd{linkDataOffset - impFuncDef.linkDataOffset};
      as_.INSTR(CALL_rm64_t).setM4RM(WasmABI::REGS::linMem, -static_cast<int32_t>(offsetFromEnd))(); // call [rbx - offsetFromBase]
    }
  } else {
    // Calling a Wasm-internal function
    // If the index is smaller than the current index, it's already defined
    if (fncIndex <= moduleInfo_.fnc.index) {
      uint32_t const binaryFncBodyOffset{moduleInfo_.wasmFncBodyBinaryPositions[fncIndex]};
      // Check if the function body we are targeting shall already be emitted (0xFFFF'FFFF = not emitted yet)
      assert(binaryFncBodyOffset != 0xFF'FF'FF'FFU && "Function needs to be defined already");

      // Produce a dummy call rel32 instruction, synthesize a corresponding RelPatchObj and link it to the start of the
      // body
      as_.INSTR(CALL_rel32_t).setRel32(0x00)();
      RelPatchObj const branchObj{RelPatchObj(false, output_.size(), output_)};
      branchObj.linkToBinaryPos(binaryFncBodyOffset);
    } else {
      // Body of the target function has not been emitted yet so we link it to either an unknown target or the last
      // branch that targets this still-unknown function body. This way we are essentially creating a linked-list of
      // branches inside the output binary that we are going to fully patch later

      // We correspondingly produce a call rel32 instruction
      as_.INSTR(CALL_rel32_t).setRel32(0x00)();
      RelPatchObj const branchObj{RelPatchObj(false, output_.size(), output_)};
      registerPendingBranch(branchObj, moduleInfo_.wasmFncBodyBinaryPositions[fncIndex]);
    }
  }
}

// Emit code that produces a trap
void Backend::executeTrap(TrapCode const code) const {
  as_.TRAP(code);
}

#if BUILTIN_FUNCTIONS
// Emit code for an inlined builtin compiler-specific function
// These functions are not part of the WebAssembly specification
void Backend::execBuiltinFncCall(BuiltinFunction const builtinFunction) {
  switch (builtinFunction) { // GCOVR_EXCL_LINE
  case BuiltinFunction::TRAP: {
    executeTrap(TrapCode::BUILTIN_TRAP);
    break;
  }
  case BuiltinFunction::GETLENGTHOFLINKEDMEMORY: {
    RegAllocTracker regAllocTracker{};
    RegElement const bufLenRegElem{common_.reqScratchRegProt(MachineType::I32, regAllocTracker, false)};
    // mov reg, [rbx - linkedMemLenPosition] -- load the length of the linked memory from the link data
    as_.INSTR(MOV_r32_rm32).setR(bufLenRegElem.reg).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::linkedMemLen)();
    common_.pushAndUpdateReference(bufLenRegElem.elem);
    break;
  }
  case BuiltinFunction::GETU8FROMLINKEDMEMORY:
  case BuiltinFunction::GETI8FROMLINKEDMEMORY:
  case BuiltinFunction::GETU16FROMLINKEDMEMORY:
  case BuiltinFunction::GETI16FROMLINKEDMEMORY:
  case BuiltinFunction::GETU32FROMLINKEDMEMORY:
  case BuiltinFunction::GETI32FROMLINKEDMEMORY:
  case BuiltinFunction::GETU64FROMLINKEDMEMORY:
  case BuiltinFunction::GETI64FROMLINKEDMEMORY:
  case BuiltinFunction::GETF32FROMLINKEDMEMORY:
  case BuiltinFunction::GETF64FROMLINKEDMEMORY: {
    Stack::iterator const offsetElementPtr{common_.condenseValentBlockBelow(stack_.end())};

    uint32_t const biFncIndex{
        static_cast<uint32_t>(static_cast<uint32_t>(builtinFunction) - static_cast<uint32_t>(BuiltinFunction::GETU8FROMLINKEDMEMORY))};
    // coverity[autosar_cpp14_a8_5_2_violation]
    constexpr auto dataSizes = make_array(1_U8, 1_U8, 2_U8, 2_U8, 4_U8, 4_U8, 8_U8, 8_U8, 4_U8, 8_U8);
    // coverity[autosar_cpp14_a8_5_2_violation]
    constexpr auto wasmTypes = make_array(MachineType::I32, MachineType::I32, MachineType::I32, MachineType::I32, MachineType::I32, MachineType::I32,
                                          MachineType::I64, MachineType::I64, MachineType::F32, MachineType::F64);
    // coverity[autosar_cpp14_a8_5_2_violation]
    constexpr auto loadTemplates =
        make_array(MOVZX_r32_rm8_t, MOVSX_r32_rm8_t, MOVZX_r32_rm16_t, MOVSX_r32_rm16_t, MOV_r32_rm32.opTemplate, MOV_r32_rm32.opTemplate,
                   MOV_r64_rm64.opTemplate, MOV_r64_rm64.opTemplate, MOVSS_rf_rmf.opTemplate, MOVSD_rf_rmf.opTemplate);

    uint8_t const dataSize{dataSizes[biFncIndex]};
    MachineType const machineType{wasmTypes[biFncIndex]};

    RegAllocTracker regAllocTracker{};
    RegElement const linkedMemPtrRegElem{common_.reqScratchRegProt(MachineType::I64, regAllocTracker, false)};
    as_.INSTR(MOV_r64_rm64).setR(linkedMemPtrRegElem.reg).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::linkedMemPtr)(); // mov rax, [rbx - ??]

    RegElement targetRegElem{};
    if (MachineTypeUtil::isInt(machineType)) {
      targetRegElem.elem = StackElement::scratchReg(linkedMemPtrRegElem.reg, MachineTypeUtil::toStackTypeFlag(machineType));
      targetRegElem.reg = linkedMemPtrRegElem.reg;
    } else {
      targetRegElem = common_.reqScratchRegProt(machineType, regAllocTracker, false);
    }

    if ((offsetElementPtr->type == StackType::CONSTANT_I32) &&
        ((static_cast<uint64_t>(offsetElementPtr->data.constUnion.u32) + dataSize) <= static_cast<uint32_t>(INT32_MAX))) {
      // Check whether linked memory is at least the needed size
      as_.INSTR(CMP_rm32_imm32)
          .setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::linkedMemLen)
          .setImm32(offsetElementPtr->data.constUnion.u32 + dataSize)();
      as_.cTRAP(TrapCode::LINKEDMEMORY_MUX, CC::B);

      // Load the actual data
      as_.INSTR(loadTemplates[biFncIndex])
          .setR(targetRegElem.reg)
          .setM4RM(linkedMemPtrRegElem.reg, static_cast<int32_t>(offsetElementPtr->data.constUnion.u32))();
    } else {
      // Load offset to a register
      REG const offsetReg{common_.liftToRegInPlaceProt(*offsetElementPtr, true, regAllocTracker).reg};

      // Add the datatype size and trap if overflow
      as_.INSTR(ADD_rm32_imm8sx).setR4RM(offsetReg).setImm8(dataSize)();
      RelPatchObj const trap{as_.prepareJMP(true, CC::C)};

      // Check if the end of the data that shall be loaded is in range
      as_.INSTR(CMP_rm32_r32).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::linkedMemLen).setR(offsetReg)();
      RelPatchObj const success{as_.prepareJMP(true, CC::AE)};

      trap.linkToHere();
      as_.TRAP(TrapCode::LINKEDMEMORY_MUX);
      success.linkToHere();

      // Load the actual data
      as_.INSTR(loadTemplates[biFncIndex]).setR(targetRegElem.reg).setM4RM(linkedMemPtrRegElem.reg, -static_cast<int32_t>(dataSize), offsetReg)();
    }

    common_.replaceAndUpdateReference(offsetElementPtr, targetRegElem.elem);
    break;
  }
  case BuiltinFunction::ISFUNCTIONLINKED: {
    Stack::iterator const fncIdxElementPtr{common_.condenseValentBlockBelow(stack_.end())};

    VariableStorage const fncIdxElementStorage{moduleInfo_.getStorage(*fncIdxElementPtr)};
    if (fncIdxElementStorage.type == StorageType::CONSTANT) {
      // Constant, can be evaluated at compile time
      common_.emitIsFunctionLinkedCompileTimeOpt(fncIdxElementPtr);
    } else {
      // Runtime value, we need to look it up
      RegAllocTracker regAllocTracker{};
      REG const fncIdxReg{common_.liftToRegInPlaceProt(*fncIdxElementPtr, false, regAllocTracker).reg};
      REG const importScratchReg{common_.reqScratchRegProt(MachineType::I64, regAllocTracker, false).reg};

      as_.INSTR(XOR_r32_rm32).setR(importScratchReg).setR4RM(importScratchReg)();
      as_.INSTR(CMP_rm32_imm32).setR4RM(fncIdxReg).setImm32(moduleInfo_.tableInitialSize)();
      RelPatchObj const outOfRange{as_.prepareJMP(true, CC::AE)};

      // load table address into importScratchReg
      as_.INSTR(MOV_r64_rm64).setR(importScratchReg).setM4RM(WasmABI::REGS::linMem, -Basedata::FromEnd::tableAddressOffset)();

      // Load the offset where the function at this table index starts
      as_.INSTR(MOV_r32_rm32).setR(importScratchReg).setM4RM(importScratchReg, 0, fncIdxReg, 3U)();

      // Check if the offset is 0 or 0xFFFFFFFF. The following instructions are referred from the -O2 build of GCC.

      as_.INSTR(ADD_rm32_imm32).setR4RM(importScratchReg).setImm32(1U)();
      as_.INSTR(CMP_rm32_imm32).setR4RM(importScratchReg).setImm32(1U)();
      as_.INSTR(MOV_r32_imm32).setR(importScratchReg).setImm32(0U)();
      as_.INSTR(SETCC_rm8).setCC(CC::A).setR8_4RM(importScratchReg)();
      outOfRange.linkToHere();
      StackElement const returnElement{StackElement::scratchReg(importScratchReg, StackType::I32)};
      common_.replaceAndUpdateReference(fncIdxElementPtr, returnElement);
    }

    break;
  }
  case BuiltinFunction::COPYFROMLINKEDMEMORY: {
    Stack::iterator const sizeElem{common_.condenseValentBlockBelow(stack_.end())};
    Stack::iterator const srcElem{common_.condenseValentBlockBelow(sizeElem)};
    Stack::iterator const dstElem{common_.condenseValentBlockBelow(srcElem)};

    RegAllocTracker regAllocTracker{};
    regAllocTracker.futureLifts = mask(srcElem.unwrap()) | mask(dstElem.unwrap());
    REG const sizeReg{common_.liftToRegInPlaceProt(*sizeElem, true, regAllocTracker).reg};
    REG const srcReg{common_.liftToRegInPlaceProt(*srcElem, true, regAllocTracker).reg};
    REG const dstReg{common_.liftToRegInPlaceProt(*dstElem, true, regAllocTracker).reg};
    REG const scratchReg{common_.reqScratchRegProt(MachineType::I64, regAllocTracker, false).reg};

    // Add size to destination and check for an overflow
    as_.INSTR(ADD_rm32_r32).setR4RM(dstReg).setR(sizeReg)();
    as_.cTRAP(TrapCode::LINMEM_OUTOFBOUNDSACCESS, CC::C);

// Check bounds, can use 0 as memObjSize since we already added it to the offset
#if LINEAR_MEMORY_BOUNDS_CHECKS
    emitLinMemBoundsCheck(dstReg, 0, 0U);
#endif
    as_.INSTR(SUB_rm32_r32).setR4RM(dstReg).setR(sizeReg)();
    as_.INSTR(ADD_r64_rm64).setR(dstReg).setR4RM(WasmABI::REGS::linMem)();

    // Absolute target pointer is now in dstReg, size is in sizeReg, src offset is in srcReg (all writable)
#if !LINEAR_MEMORY_BOUNDS_CHECKS
    // "Dummy read" first byte so zero width copies trap if address is out of bounds
    as_.INSTR(CMP_rm8_imm8).setM4RM(dstReg, 0).setImm8(0U)();
#endif
    // Load length of linked memory into scratch register
    as_.INSTR(MOV_r32_rm32).setR(scratchReg).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::linkedMemLen)();

    // Check bounds of src
    as_.INSTR(SUB_rm32_r32).setR4RM(scratchReg).setR(sizeReg)();
    RelPatchObj const underflow{as_.prepareJMP(true, CC::C)};
    as_.INSTR(CMP_rm32_r32).setR4RM(srcReg).setR(scratchReg)();
    RelPatchObj const inRange{as_.prepareJMP(true, CC::BE)};
    underflow.linkToHere();
    as_.TRAP(TrapCode::LINKEDMEMORY_MUX);
    inRange.linkToHere();

    // Both are in bounds, let's copy the data

    // Load linked memory start pointer and add it to srcReg
    as_.INSTR(ADD_r64_rm64).setR(srcReg).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::linkedMemPtr)();

    emitMemcpyNoBoundsCheck(dstReg, srcReg, sizeReg, scratchReg, false);

    common_.removeReference(sizeElem);
    common_.removeReference(srcElem);
    common_.removeReference(dstElem);
    static_cast<void>(stack_.erase(sizeElem));
    static_cast<void>(stack_.erase(srcElem));
    static_cast<void>(stack_.erase(dstElem));
    break;
  }
  case BuiltinFunction::TRACE_POINT: {
    Stack::iterator const identifierElement{common_.condenseValentBlockBelow(stack_.end())};
    VariableStorage identifierStorage{moduleInfo_.getStorage(*identifierElement)};
    switch (identifierStorage.type) {
    case StorageType::STACKMEMORY:
    case StorageType::LINKDATA:
      identifierStorage.machineType = MachineType::F32;
      emitMoveFloatImpl(VariableStorage::reg(MachineType::F32, WasmABI::REGS::moveHelper), identifierStorage, false);
      break;
    case StorageType::REGISTER:
      as_.INSTR(MOVD_rf_rm32).setR(WasmABI::REGS::moveHelper).setR4RM(identifierStorage.location.reg)();
      break;
    case StorageType::CONSTANT:
      emitMoveFloatImpl(VariableStorage::reg(MachineType::F32, WasmABI::REGS::moveHelper),
                        VariableStorage::f32Const(bit_cast<float>(identifierStorage.location.constUnion.u32)), false);
      break;
    default:
      UNREACHABLE(break, "Unknown storage");
    }
    common_.removeReference(identifierElement);
    static_cast<void>(stack_.erase(identifierElement));

    if (moduleInfo_.helperFunctionBinaryPositions.builtinTracePointHandler == 0xFFFFFFFFU) {
      RelPatchObj const mainCode{as_.prepareJMP(true, CC::NONE)};

      moduleInfo_.helperFunctionBinaryPositions.builtinTracePointHandler = output_.size();
      constexpr REG tmpReg1{REG::A};
      constexpr REG tmpReg2{REG::D};
      constexpr REG tmpReg3{REG::C};
      as_.INSTR(PUSH_r64_t).setR(tmpReg1)();
      as_.INSTR(PUSH_r64_t).setR(tmpReg2)();
      as_.INSTR(PUSH_r64_t).setR(tmpReg3)();

      constexpr REG rdtscLowReg{tmpReg1};
      as_.INSTR(RDTSC)(); // Read time-stamp counter into RDX:RAX
      constexpr REG traceBufferPtrReg{tmpReg2};
      as_.INSTR(MOV_r64_rm64).setR(traceBufferPtrReg).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::traceBufferPtr)();
      // size<u32> | cursor<u32> | (rdtsc<u32> | identifier<u32> )+
      //                         ^
      //                   traceBufferPtrReg
      as_.INSTR(TEST_rm64_r64_t).setR4RM(traceBufferPtrReg).setR(traceBufferPtrReg)();
      RelPatchObj const nullptrTraceBuffer{as_.prepareJMP(true, CC::E)};
      constexpr REG cursorReg{tmpReg3};
      as_.INSTR(MOV_r32_rm32).setR(cursorReg).setM4RM(traceBufferPtrReg, -4)();

      as_.INSTR(CMP_r32_rm32).setR(cursorReg).setM4RM(traceBufferPtrReg, -8)();
      RelPatchObj const isFull{as_.prepareJMP(true, CC::AE)};

      // traceBufferPtrReg[cursor] <- rdtscLow
      as_.INSTR(MOV_rm32_r32).setM4RM(traceBufferPtrReg, 0, cursorReg, 3U).setR(rdtscLowReg)();
      /// last use of @b rdtscLowReg

      // traceBufferPtrReg[cursor + 4] <- identifier
      as_.INSTR(MOVD_rm32_rf).setM4RM(traceBufferPtrReg, 4, cursorReg, 3U).setR(WasmABI::REGS::moveHelper)();
      /// last use of @b identifierReg

      // cursor++;
      as_.INSTR(ADD_rm32_imm32).setM4RM(traceBufferPtrReg, -4).setImm32(1U)();

      isFull.linkToHere();
      nullptrTraceBuffer.linkToHere();

      as_.INSTR(POP_r64_t).setR(tmpReg3)();
      as_.INSTR(POP_r64_t).setR(tmpReg2)();
      as_.INSTR(POP_r64_t).setR(tmpReg1)();

      as_.INSTR(RET_t)();

      mainCode.linkToHere();
    }

    as_.INSTR(CALL_rel32_t).setRel32(0x00)();
    RelPatchObj const branchObj{RelPatchObj(false, output_.size(), output_)};
    branchObj.linkToBinaryPos(moduleInfo_.helperFunctionBinaryPositions.builtinTracePointHandler);
    break;
  }
  // GCOVR_EXCL_START
  case BuiltinFunction::UNDEFINED:
  default: {
    UNREACHABLE(break, "Unknown BuiltinFunction");
  }
    // GCOVR_EXCL_STOP
  }
}
#endif

void Backend::emitMemcpyWithConstSizeNoBoundsCheck(REG const dstReg, REG const srcReg, REG const sizeReg, uint32_t const sizeToCopy,
                                                   REG const gpScratchReg, bool const canOverlap) const {
  RelPatchObj reverse{};
  if (canOverlap) {
    as_.INSTR(CMP_r64_rm64).setR(srcReg).setR4RM(dstReg)();
    reverse = as_.prepareJMP(true, CC::A);
  }

  // src <= dst

  // Choose 3 as the threshold for unrolling temporarily that can be adjusted later.
  // Conservatively estimated, at least within 3, the code size of the loop unrolling is reduced
  constexpr uint32_t unrollingThreshold{3U};
  uint32_t const copy8ByteCount{sizeToCopy / 8U};
  bool const unrollingCopy8ByteLoop{copy8ByteCount <= unrollingThreshold};
  uint32_t const copy1ByteCount{sizeToCopy % 8U};
  bool const unrollingCopy1ByteLoop{copy1ByteCount <= unrollingThreshold};

  if (unrollingCopy8ByteLoop) {
    int32_t offset{0};
    // 8 bytes copy loop unrolling
    for (uint32_t i{0U}; i < copy8ByteCount; ++i) {
      offset -= 8;
      as_.INSTR(MOV_r64_rm64).setR(gpScratchReg).setM4RM(srcReg, offset, sizeReg)();
      as_.INSTR(MOV_rm64_r64).setM4RM(dstReg, offset, sizeReg).setR(gpScratchReg)();
    }
    if ((copy8ByteCount > 0U) && (copy1ByteCount != 0U)) {
      // prepare the correct size reg
      as_.INSTR(MOV_r32_imm32).setR(sizeReg).setImm32(sizeToCopy - (copy8ByteCount * 8U))();
    }
  } else {
    // Normal loop in runtime
    // Check if (remaining) size is at least 8
    uint32_t const copy8ByteLoop{output_.size()};
    as_.INSTR(CMP_rm32_imm8sx).setR4RM(sizeReg).setImm8(8U)();
    RelPatchObj const lessThan8InReverse{as_.prepareJMP(true, CC::B)};

    as_.INSTR(MOV_r64_rm64).setR(gpScratchReg).setM4RM(srcReg, -8, sizeReg)();
    as_.INSTR(MOV_rm64_r64).setM4RM(dstReg, -8, sizeReg).setR(gpScratchReg)();
    as_.INSTR(SUB_rm32_imm8sx).setR4RM(sizeReg).setImm8(8U)();
    as_.prepareJMP(true, CC::NONE).linkToBinaryPos(copy8ByteLoop);
    lessThan8InReverse.linkToHere();
  }

  RelPatchObj quickFinishedInReverse{};
  if (unrollingCopy1ByteLoop) {
    int32_t offset{0};
    // 1 byte copy loop unrolling
    for (uint32_t i{0U}; i < copy1ByteCount; ++i) {
      offset -= 1;
      as_.INSTR(MOV_r8_rm8_t).setR(gpScratchReg).setM4RM(srcReg, offset, sizeReg)();
      as_.INSTR(MOV_rm8_r8_t).setM4RM(dstReg, offset, sizeReg).setR(gpScratchReg)();
    }
    quickFinishedInReverse = as_.prepareJMP(true);
  } else {
    // Normal loop in runtime
    // Check if (remaining) size is at least 1
    as_.INSTR(CMP_rm32_imm8sx).setR4RM(sizeReg).setImm8(1U)();
    quickFinishedInReverse = as_.prepareJMP(true, CC::B);
    // Copy 1 byte
    uint32_t const copy1InReverse{output_.size()};
    as_.INSTR(MOV_r8_rm8_t).setR(gpScratchReg).setM4RM(srcReg, -1, sizeReg)();
    as_.INSTR(MOV_rm8_r8_t).setM4RM(dstReg, -1, sizeReg).setR(gpScratchReg)();
    as_.INSTR(SUB_rm32_imm8sx).setR4RM(sizeReg).setImm8(1U)();
    as_.prepareJMP(true, CC::NE).linkToBinaryPos(copy1InReverse); // Jump back if not zero
  }

  if (canOverlap) {
    RelPatchObj const finishedForward{as_.prepareJMP(true)};
    reverse.linkToHere();
    // src > dst

    if (unrollingCopy8ByteLoop) {
      int32_t const sizeReversed{-static_cast<int32_t>(sizeToCopy)};
      int32_t offset{sizeReversed};
      for (uint32_t i{0U}; i < copy8ByteCount; ++i) {
        as_.INSTR(MOV_r64_rm64).setR(gpScratchReg).setM4RM(srcReg, offset, sizeReg)();
        as_.INSTR(MOV_rm64_r64).setM4RM(dstReg, offset, sizeReg).setR(gpScratchReg)();
        offset += 8;
      }

      if (copy1ByteCount > 0U) {
        as_.INSTR(ADD_r64_rm64).setR(srcReg).setR4RM(sizeReg)();
        as_.INSTR(ADD_r64_rm64).setR(dstReg).setR4RM(sizeReg)();
        int64_t const copy8ByteOffset{static_cast<int64_t>(copy8ByteCount) * 8LL};
        int64_t const totalOffset{static_cast<int64_t>(sizeReversed) + copy8ByteOffset};
        as_.INSTR(MOV_r64_imm64_t).setR(sizeReg).setImm64(bit_cast<uint64_t>(totalOffset))();
        // Then, sizeReg is negative size if has remain bytes to copy
      }
    } else {
      as_.INSTR(ADD_r64_rm64).setR(srcReg).setR4RM(sizeReg)();
      as_.INSTR(ADD_r64_rm64).setR(dstReg).setR4RM(sizeReg)();
      as_.INSTR(NEG_rm64).setR4RM(sizeReg)();
      // Then, sizeReg is negative size

      // Check if (remaining) size is at least 8
      uint32_t const check8Forward{output_.size()};
      as_.INSTR(CMP_rm32_imm8sx).setR4RM(sizeReg).setImm8(static_cast<uint8_t>(-8))();
      RelPatchObj const lessThan8Forward{as_.prepareJMP(true, CC::G)};
      // Copy 8 bytes
      // TODO(SIMD): Update to SIMD later? Same as for other architectures
      as_.INSTR(MOV_r64_rm64).setR(gpScratchReg).setM4RM(srcReg, 0, sizeReg)();
      as_.INSTR(MOV_rm64_r64).setM4RM(dstReg, 0, sizeReg).setR(gpScratchReg)();
      as_.INSTR(ADD_rm64_imm8sx).setR4RM(sizeReg).setImm8(8U)();
      as_.prepareJMP(true, CC::NONE).linkToBinaryPos(check8Forward);
      lessThan8Forward.linkToHere();
    }

    // sizeReg is negative number
    if (unrollingCopy1ByteLoop) {
      int32_t offset{0};
      for (uint32_t i{0U}; i < copy1ByteCount; ++i) {
        as_.INSTR(MOV_r8_rm8_t).setR(gpScratchReg).setM4RM(srcReg, offset, sizeReg)();
        as_.INSTR(MOV_rm8_r8_t).setM4RM(dstReg, offset, sizeReg).setR(gpScratchReg)();
        offset += 1;
      }
    } else {
      // Check if (remaining) size is at least 1
      as_.INSTR(CMP_rm64_imm8sx).setR4RM(sizeReg).setImm8(static_cast<uint8_t>(-1))();
      RelPatchObj const quickFinishedForward{as_.prepareJMP(true, CC::G)};
      // Copy 1 byte
      uint32_t const copy1Forward{output_.size()};
      as_.INSTR(MOV_r8_rm8_t).setR(gpScratchReg).setM4RM(srcReg, 0, sizeReg)();
      as_.INSTR(MOV_rm8_r8_t).setM4RM(dstReg, 0, sizeReg).setR(gpScratchReg)();
      as_.INSTR(ADD_rm64_imm8sx).setR4RM(sizeReg).setImm8(1U)();
      as_.prepareJMP(true, CC::NE).linkToBinaryPos(copy1Forward); // Jump back if not zero
      quickFinishedForward.linkToHere();
    }
    finishedForward.linkToHere();
  }

  quickFinishedInReverse.linkToHere();
}

void Backend::emitMemcpyNoBoundsCheck(REG const dstReg, REG const srcReg, REG const sizeReg, REG const gpScratchReg, bool const canOverlap) const {
  RelPatchObj reverse{};
  if (canOverlap) {
    as_.INSTR(CMP_r64_rm64).setR(srcReg).setR4RM(dstReg)();
    reverse = as_.prepareJMP(true, CC::A);
  }
  // src <= dst, copy from end to begin
  // Check if (remaining) size is at least 8
  uint32_t const check8InReverse{output_.size()};
  as_.INSTR(CMP_rm32_imm8sx).setR4RM(sizeReg).setImm8(8U)();
  RelPatchObj const lessThan8InReverse{as_.prepareJMP(true, CC::B)};
  // Copy 8 bytes
  // TODO(SIMD): Update to SIMD later? Same as for other architectures
  as_.INSTR(MOV_r64_rm64).setR(gpScratchReg).setM4RM(srcReg, -8, sizeReg)();
  as_.INSTR(MOV_rm64_r64).setM4RM(dstReg, -8, sizeReg).setR(gpScratchReg)();
  as_.INSTR(SUB_rm32_imm8sx).setR4RM(sizeReg).setImm8(8U)();
  as_.prepareJMP(true, CC::NONE).linkToBinaryPos(check8InReverse);
  lessThan8InReverse.linkToHere();
  // Check if (remaining) size is at least 1
  as_.INSTR(CMP_rm32_imm8sx).setR4RM(sizeReg).setImm8(1U)();
  RelPatchObj const quickFinishedInReverse{as_.prepareJMP(true, CC::B)};
  // Copy 1 byte
  uint32_t const copy1InReverse{output_.size()};
  as_.INSTR(MOV_r8_rm8_t).setR(gpScratchReg).setM4RM(srcReg, -1, sizeReg)();
  as_.INSTR(MOV_rm8_r8_t).setM4RM(dstReg, -1, sizeReg).setR(gpScratchReg)();
  as_.INSTR(SUB_rm32_imm8sx).setR4RM(sizeReg).setImm8(1U)();
  as_.prepareJMP(true, CC::NE).linkToBinaryPos(copy1InReverse); // Jump back if not zero

  if (canOverlap) {
    RelPatchObj const finishedForward{as_.prepareJMP(true)};
    reverse.linkToHere();
    // src > dst, copy from begin to end

    // src += size;
    // dst += size;
    // size = -size;
    // dst[size] = src[size]
    as_.INSTR(ADD_r64_rm64).setR(srcReg).setR4RM(sizeReg)();
    as_.INSTR(ADD_r64_rm64).setR(dstReg).setR4RM(sizeReg)();
    as_.INSTR(NEG_rm64).setR4RM(sizeReg)();
    // Check if (remaining) size is at least 8
    uint32_t const check8Forward{output_.size()};
    as_.INSTR(CMP_rm32_imm8sx).setR4RM(sizeReg).setImm8(static_cast<uint8_t>(-8))();
    RelPatchObj const lessThan8Forward{as_.prepareJMP(true, CC::G)};
    // Copy 8 bytes
    // TODO(SIMD): Update to SIMD later? Same as for other architectures
    as_.INSTR(MOV_r64_rm64).setR(gpScratchReg).setM4RM(srcReg, 0, sizeReg)();
    as_.INSTR(MOV_rm64_r64).setM4RM(dstReg, 0, sizeReg).setR(gpScratchReg)();
    as_.INSTR(ADD_rm64_imm8sx).setR4RM(sizeReg).setImm8(8U)();
    as_.prepareJMP(true, CC::NONE).linkToBinaryPos(check8Forward);
    lessThan8Forward.linkToHere();
    // Check if (remaining) size is at least 1
    as_.INSTR(CMP_rm64_imm8sx).setR4RM(sizeReg).setImm8(static_cast<uint8_t>(-1))();
    RelPatchObj const quickFinishedForward{as_.prepareJMP(true, CC::G)};
    // Copy 1 byte
    uint32_t const copy1Forward{output_.size()};
    as_.INSTR(MOV_r8_rm8_t).setR(gpScratchReg).setM4RM(srcReg, 0, sizeReg)();
    as_.INSTR(MOV_rm8_r8_t).setM4RM(dstReg, 0, sizeReg).setR(gpScratchReg)();
    as_.INSTR(ADD_rm64_imm8sx).setR4RM(sizeReg).setImm8(1U)();
    as_.prepareJMP(true, CC::NE).linkToBinaryPos(copy1Forward); // Jump back if not zero

    finishedForward.linkToHere();
    quickFinishedForward.linkToHere();
  }
  quickFinishedInReverse.linkToHere();
}

void Backend::finalizeBlock(StackElement const *const blockElement) {
  assert(blockElement && "Given block is NULL");

  if ((blockElement->type == StackType::BLOCK) || (blockElement->type == StackType::IFBLOCK)) {
    as_.setStackFrameSize(blockElement->data.blockInfo.entryStackFrameSize);
    uint32_t const lastBlockBranch{blockElement->data.blockInfo.binaryPosition.lastBlockBranch};
    finalizeBranch(lastBlockBranch);
  }
}

void Backend::finalizeBranch(uint32_t const linkVariable) const {
  if (linkVariable != 0xFF'FF'FF'FFU) {
    assert(linkVariable <= output_.size() && "Out of range");

    uint32_t position{linkVariable};
    while (true) {
      RelPatchObj const relPatchObj{RelPatchObj(false, position, output_)};
      position = relPatchObj.getLinkedBinaryPos();
      relPatchObj.linkToHere();
      if (position == relPatchObj.getPosOffsetAfterInstr()) {
        break;
      }
    }
  }
}

void Backend::registerPendingBranch(RelPatchObj const &branchObj, uint32_t &linkVariable) {
  branchObj.linkToBinaryPos((linkVariable == 0xFF'FF'FF'FFU) ? branchObj.getPosOffsetAfterInstr() : linkVariable);

  // We store the current position (the last branch) in the link variable; position before branch instruction is stored
  linkVariable = branchObj.getPosOffsetAfterInstr();
}

///
/// @brief Returns a copy of an AbstrInstr with the commutative flag set to true
///
/// @param abstrInstr AbstrInstr to modify so it is marked as mutative
/// @return AbstrInstr Modified AbstrInstr
// coverity[autosar_cpp14_a8_4_7_violation]
static constexpr AbstrInstr makeCommutative(AbstrInstr abstrInstr) VB_NOEXCEPT {
  abstrInstr.commutative = true;
  return abstrInstr;
}

// Produces machine code for a comparison between two StackElements
// Uses instructions which are inherently non-commutative (CMP), but makes them
// commutative and returns whether the commutation ("reversion") was used
bool Backend::emitComparison(OPCode const opcode, StackElement const *const arg0Ptr, StackElement const *const arg1Ptr) {
  assert(opcode >= OPCode::I32_EQZ && opcode <= OPCode::F64_GE && "Comparison opcode out of range");
  moduleInfo_.lastBC = BCforOPCode(opcode);
  switch (opcode) { // GCOVR_EXCL_LINE
  case OPCode::I32_EQZ: {
    StackElement const dummyElement{StackElement::i32Const(0_U32)};
    return as_.selectInstr(make_array(makeCommutative(CMP_rm32_imm8sx)), arg0Ptr, &dummyElement, nullptr, RegMask::none(), true).reversed;
  }
  case OPCode::I32_EQ:
  case OPCode::I32_NE:
  case OPCode::I32_LT_S:
  case OPCode::I32_LT_U:
  case OPCode::I32_GT_S:
  case OPCode::I32_GT_U:
  case OPCode::I32_LE_S:
  case OPCode::I32_LE_U:
  case OPCode::I32_GE_S:
  case OPCode::I32_GE_U: {
    // coverity[autosar_cpp14_a8_5_2_violation]
    constexpr auto ops =
        make_array(makeCommutative(CMP_rm32_imm8sx), makeCommutative(CMP_rm32_imm32), makeCommutative(CMP_rm32_r32), makeCommutative(CMP_r32_rm32));
    return as_.selectInstr(ops, arg0Ptr, arg1Ptr, nullptr, RegMask::none(), true).reversed;
  }
  case OPCode::I64_EQZ: {
    StackElement const dummyElement{StackElement::i64Const(0_U64)};
    return as_.selectInstr(make_array(makeCommutative(CMP_rm64_imm8sx)), arg0Ptr, &dummyElement, nullptr, RegMask::none(), true).reversed;
  }
  case OPCode::I64_EQ:
  case OPCode::I64_NE:
  case OPCode::I64_LT_S:
  case OPCode::I64_LT_U:
  case OPCode::I64_GT_S:
  case OPCode::I64_GT_U:
  case OPCode::I64_LE_S:
  case OPCode::I64_LE_U:
  case OPCode::I64_GE_S:
  case OPCode::I64_GE_U: {
    // coverity[autosar_cpp14_a8_5_2_violation]
    constexpr auto ops =
        make_array(makeCommutative(CMP_rm64_imm8sx), makeCommutative(CMP_rm64_imm32sx), makeCommutative(CMP_rm64_r64), makeCommutative(CMP_r64_rm64));
    return as_.selectInstr(ops, arg0Ptr, arg1Ptr, nullptr, RegMask::none(), true).reversed;
  }
  case OPCode::F32_EQ:
  case OPCode::F32_NE:
  case OPCode::F32_LT:
  case OPCode::F32_GT:
  case OPCode::F32_LE:
  case OPCode::F32_GE: {
    return as_.selectInstr(make_array(makeCommutative(UCOMISS_rf_rmf)), arg0Ptr, arg1Ptr, nullptr, RegMask::none(), true).reversed;
  }
  case OPCode::F64_EQ:
  case OPCode::F64_NE:
  case OPCode::F64_LT:
  case OPCode::F64_GT:
  case OPCode::F64_LE:
  case OPCode::F64_GE: {
    return as_.selectInstr(make_array(makeCommutative(UCOMISD_rf_rmf)), arg0Ptr, arg1Ptr, nullptr, RegMask::none(), true).reversed;
  }
  // GCOVR_EXCL_START
  default: {
    UNREACHABLE(break, "Instruction is not a comparison");
  }
    // GCOVR_EXCL_STOP
  }
}

void Backend::emitBranch(StackElement *const targetBlockElem, BC const branchCond, bool const isNegative) {
  assert(((moduleInfo_.lastBC == branchCond) || (moduleInfo_.lastBC == negateBC(branchCond)) || (moduleInfo_.lastBC == reverseBC(branchCond)) ||
          (branchCond == BC::UNCONDITIONAL)) &&
         "BranchCondition not matching");

  // coverity[autosar_cpp14_a8_5_2_violation]
  // coverity[autosar_cpp14_a7_1_2_violation]
  auto const linkBranchToBlock = [](RelPatchObj const &relPatchObj, StackElement *const blockElement) {
    if (blockElement->type == StackType::LOOP) {
      relPatchObj.linkToBinaryPos(blockElement->data.blockInfo.binaryPosition.loopStartOffset);
    } else { // Block or IFBlock
      registerPendingBranch(relPatchObj, blockElement->data.blockInfo.binaryPosition.lastBlockBranch);
    }
  };

  bool const bcIsFloat{(static_cast<uint8_t>(branchCond) >= static_cast<uint8_t>(BC::EQ_F)) &&
                       (static_cast<uint8_t>(branchCond) <= static_cast<uint8_t>(BC::GE_F))};
  bool const branchOnNan{isNegative ? (branchCond != BC::NE_F) : (branchCond == BC::NE_F)};

  CC const majorPositiveCC{isNegative ? negateCC(CCforBC(branchCond)) : CCforBC(branchCond)};

  if (targetBlockElem != nullptr) { // Targeting a block, loop or ifblock
    if ((branchCond == BC::UNCONDITIONAL) || (moduleInfo_.fnc.stackFrameSize == targetBlockElem->data.blockInfo.entryStackFrameSize)) {
      as_.setStackFrameSize(targetBlockElem->data.blockInfo.entryStackFrameSize,
                            true); // Either unconditional or no-op anyway
      if (bcIsFloat && branchOnNan) {
        RelPatchObj const nanRelPatchObj{as_.prepareJMP(false, CC::P)};
        linkBranchToBlock(nanRelPatchObj, targetBlockElem);
      }
      RelPatchObj nanRelPatchObj2{};
      if (bcIsFloat && (!branchOnNan)) {
        nanRelPatchObj2 = as_.prepareJMP(true, CC::P);
      }
      RelPatchObj const branchObj{as_.prepareJMP(false, majorPositiveCC)};
      linkBranchToBlock(branchObj, targetBlockElem);
      if (bcIsFloat && (!branchOnNan)) {
        nanRelPatchObj2.linkToHere();
      }
    } else {
      RelPatchObj nanRelPatchObj{};
      if (bcIsFloat) {
        nanRelPatchObj = as_.prepareJMP(true, CC::P); // If nan
      }
      RelPatchObj const conditionRelPatchObj{as_.prepareJMP(true, negateCC(majorPositiveCC))};
      if (bcIsFloat && branchOnNan) {
        nanRelPatchObj.linkToHere();
      }
      as_.setStackFrameSize(targetBlockElem->data.blockInfo.entryStackFrameSize, true);
      RelPatchObj const branchObj{as_.prepareJMP(false)};
      if (bcIsFloat && (!branchOnNan)) {
        nanRelPatchObj.linkToHere();
      }
      conditionRelPatchObj.linkToHere();

      linkBranchToBlock(branchObj, targetBlockElem);
    }
  } else { // Targeting the function
    if (branchCond == BC::UNCONDITIONAL) {
      emitReturnAndUnwindStack(true);
    } else {
      RelPatchObj nanRelPatchObj{};
      if (bcIsFloat) {
        nanRelPatchObj = as_.prepareJMP(true, CC::P); // If nan
      }
      RelPatchObj const relPatchObj{as_.prepareJMP(true, negateCC(majorPositiveCC))}; // Negated condition -> jump over
      if (bcIsFloat && branchOnNan) {
        nanRelPatchObj.linkToHere();
      }
      emitReturnAndUnwindStack(true);
      if (bcIsFloat && (!branchOnNan)) {
        nanRelPatchObj.linkToHere();
      }
      relPatchObj.linkToHere();
    }
  }
}

void Backend::executeTableBranch(uint32_t const numBranchTargets, FunctionRef<Stack::iterator()> const &getNextTableBranchDepthLambda) {
  Stack::iterator const indexElem{common_.condenseValentBlockBelow(stack_.end())};

  Stack::iterator const firstBlockRef{getNextTableBranchDepthLambda()};
  uint32_t const firstBlockSigIndex{(firstBlockRef.isEmpty()) ? moduleInfo_.getFuncDef(moduleInfo_.fnc.index).sigIndex
                                                              : firstBlockRef->data.blockInfo.sigIndex};
  bool const isFirstBlockLoop{(firstBlockRef.isEmpty()) ? false : (firstBlockRef->type == StackType::LOOP)};
  uint32_t const numReturnValues{isFirstBlockLoop ? moduleInfo_.getNumParamsForSignature(firstBlockSigIndex)
                                                  : moduleInfo_.getNumReturnValuesForSignature(firstBlockSigIndex)};

  common_.condenseSideEffectInstructionBlewValentBlock(numReturnValues);

  Stack::iterator returnValuesBase{};
  if (numReturnValues > 0U) {
    returnValuesBase = common_.condenseMultipleValentBlocksWithTargetHintBelow(indexElem, firstBlockSigIndex, isFirstBlockLoop);
  }

  RegAllocTracker regAllocTracker{};
  REG const indexReg{common_.liftToRegInPlaceProt(*indexElem, true, regAllocTracker).reg};
  RegElement const scratchRegElem{common_.reqScratchRegProt(MachineType::I64, regAllocTracker, false)};

  // Saturate indexReg to numBranchTargets
  as_.INSTR(CMP_rm32_imm32).setR4RM(indexReg).setImm32(numBranchTargets)();
  RelPatchObj const inRange{as_.prepareJMP(true, CC::BE)};
  as_.INSTR(MOV_rm32_imm32).setR4RM(indexReg).setImm32(numBranchTargets)();
  inRange.linkToHere();

  RelPatchObj const toTableStart{as_.preparePCRelAddrLEA(scratchRegElem.reg)};
  // scratchRegElem now points to table start, now load delta from table start to indexReg by accessing table
  as_.INSTR(MOV_r32_rm32).setR(indexReg).setM4RM(scratchRegElem.reg, 0, indexReg, 2U)();
  as_.INSTR(ADD_r64_rm64).setR(scratchRegElem.reg).setR4RM(indexReg)();
  as_.INSTR(JMP_rm64_t).setR4RM(scratchRegElem.reg)();

  toTableStart.linkToHere();
  uint32_t const tableStart{output_.size()};
  uint32_t const tableByteSize{(numBranchTargets + 1_U32) * static_cast<uint32_t>(sizeof(uint32_t))};
  output_.step(tableByteSize);

  for (uint32_t i{0U}; i < (numBranchTargets + 1_U32); i++) {
    uint32_t const offsetFromTableStart{output_.size() - tableStart};
    uint32_t const patchPos{tableStart + (i * static_cast<uint32_t>(sizeof(uint32_t)))};
    writeToPtr<uint32_t>(output_.posToPtr(patchPos), offsetFromTableStart);
    Stack::iterator const blockRef{(i == 0U) ? firstBlockRef : getNextTableBranchDepthLambda()};

    if (numReturnValues > 0U) {
      common_.loadReturnValues(returnValuesBase, numReturnValues, blockRef.raw(), true);
    }
    emitBranch(blockRef.raw(), BC::UNCONDITIONAL);
  }

  common_.popAndUpdateReference();
  if (numReturnValues > 0U) {
    common_.popReturnValueElems(returnValuesBase, numReturnValues);
  }
}

void Backend::emitReturnAndUnwindStack(bool const temporary) {
  as_.setStackFrameSize(moduleInfo_.fnc.paramWidth + NBackend::returnAddrWidth, temporary, true);
  as_.INSTR(RET_t)();
}

void Backend::emitNativeTrapAdapter() const {
  // NABI::gpParams[0] contains pointer to the start of the linear memory. Needed because this function is not called
  // from the native context
  as_.INSTR(MOV_r64_rm64).setR(WasmABI::REGS::linMem).setR4RM(NABI::gpParams[0])();
  // NABI::gpParams[1] contains the TrapCode which we move to REGS::trapReg
  as_.INSTR(MOV_r64_rm64).setR(WasmABI::REGS::trapReg).setR4RM(NABI::gpParams[1])();
}

void Backend::emitStackTraceCollector(uint32_t const stacktraceRecordCount) const {
  assert(stacktraceRecordCount > 0 && "No stacktrace records");

  // Load last frame ref pointer from job memory. This is definitely valid here
  as_.INSTR(MOV_r64_rm64).setR(StackTrace::frameRefReg).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::lastFrameRefPtr)();

  // Set counterReg to zero (count down because we will use it as index register)
  as_.INSTR(XOR_r32_rm32).setR(StackTrace::counterReg).setR4RM(StackTrace::counterReg)();
  uint32_t const loopStartOffset{output_.size()};
  // Load function index to scratch reg and store in buffer
  as_.INSTR(MOV_r32_rm32).setR(StackTrace::scratchReg).setM4RM(StackTrace::frameRefReg, 8)();
  as_.INSTR(MOV_rm32_r32)
      .setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::getStacktraceArrayBase(stacktraceRecordCount), StackTrace::counterReg, 2U)
      .setR(StackTrace::scratchReg)();

  // Load next frame ref, compare to zero and break if it is zero (means first entry)
  as_.INSTR(MOV_r64_rm64).setR(StackTrace::frameRefReg).setM4RM(StackTrace::frameRefReg, 0)();
  as_.INSTR(CMP_rm64_imm8sx).setR4RM(StackTrace::frameRefReg).setImm8(0U)();
  RelPatchObj const collectedAll{as_.prepareJMP(true, CC::E)};

  // Otherwise we increment the counter and restart the loop if the counter is less than stacktraceRecordCount
  as_.INSTR(ADD_rm32_imm8sx).setR4RM(StackTrace::counterReg).setImm8(1U)();
  as_.INSTR(CMP_rm32_imm32).setR4RM(StackTrace::counterReg).setImm32(stacktraceRecordCount)();
  as_.prepareJMP(true, CC::L).linkToBinaryPos(loopStartOffset);

  collectedAll.linkToHere();
}

void Backend::emitTrapHandler() const {
  // Restore stack pointer
  as_.INSTR(MOV_r64_rm64).setR(REG::SP).setM4RM(WasmABI::REGS::linMem, -Basedata::FromEnd::trapStackReentry)();

  // Load trapCodePtr into a register and store the trapCode there
  as_.INSTR(MOV_r64_rm64).setR(callScrRegs[0]).setM4RM(REG::SP, static_cast<int32_t>(of_trapCodePtr_trapReentryPoint))();
  as_.INSTR(MOV_rm32_r32).setM4RM(callScrRegs[0], 0).setR(WasmABI::REGS::trapReg)();

  as_.INSTR(JMP_rm64_t).setM4RM(WasmABI::REGS::linMem, -Basedata::FromEnd::trapHandlerPtr)();
}

#if !LINEAR_MEMORY_BOUNDS_CHECKS
void Backend::emitLandingPad() {
  moduleInfo_.helperFunctionBinaryPositions.landingPad = output_.size();

  constexpr uint32_t retWidth{8U};
  constexpr uint32_t volRegsSpillSize{static_cast<uint32_t>(NABI::volRegs.size()) * 8U};

  // RSP <-
  //  | Shadow Space | Vol Regs Spill | Return Address Width
  //

  // Reserve space on stack and spill all volatile registers since we will call a native function
  as_.INSTR(SUB_rm64_imm32sx).setR4RM(REG::SP).setImm32(NABI::shadowSpaceSize + volRegsSpillSize + retWidth)();
#if ACTIVE_STACK_OVERFLOW_CHECK
  as_.checkStackFence();
#endif
  spillRestoreRegsRaw({NABI::volRegs.data(), NABI::volRegs.size()}, false, NABI::shadowSpaceSize);

#if (MAX_WASM_STACKSIZE_BEFORE_NATIVE_CALL != 0 && !ACTIVE_STACK_OVERFLOW_CHECK) ||                                                                  \
    (STACKSIZE_LEFT_BEFORE_NATIVE_CALL != 0 && ACTIVE_STACK_OVERFLOW_CHECK)
  as_.INSTR(CMP_r64_rm64).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::nativeStackFence).setR(REG::SP)(); // Compare stack pointer to stack fence
  as_.cTRAP(TrapCode::STACKFENCEBREACHED, CC::LE);
#endif

  // Check if stack pointer is properly aligned, we can use NABI::gpParams[0] as a volatile scratch register
  as_.INSTR(MOV_rm64_r64).setR4RM(NABI::gpParams[0]).setR(REG::SP)();
  as_.INSTR(AND_rm64_imm8sx).setR4RM(NABI::gpParams[0]).setImm8(0xFU)();
  as_.INSTR(SUB_rm64_r64).setR4RM(REG::SP).setR(NABI::gpParams[0])();

  // We can retrieve the landing pad target and store the volatile register we used there so that the information
  // survives the call
  as_.INSTR(XCHG_rm64_r64_t).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::landingPadTarget).setR(NABI::gpParams[0])();

  // Call the target of the landing pad now that the stack pointer is aligned
  as_.INSTR(CALL_rm64_t).setR4RM(NABI::gpParams[0])();

  // Remove the extra alignment space from the stack
  as_.INSTR(ADD_r64_rm64).setR(REG::SP).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::landingPadTarget)();

  // Set up the return address on stack
  as_.INSTR(MOV_r64_rm64).setR(NABI::gpParams[0]).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::landingPadRet)();
  as_.INSTR(MOV_rm64_r64)
      .setM4RM(REG::SP, static_cast<int32_t>(NABI::shadowSpaceSize) + static_cast<int32_t>(volRegsSpillSize))
      .setR(NABI::gpParams[0])();

  // Restore all previously spilled registers, then unwind the stack
  spillRestoreRegsRaw({NABI::volRegs.data(), NABI::volRegs.size()}, true, NABI::shadowSpaceSize);
  // Leave new return address on stack
  as_.INSTR(ADD_rm64_imm32sx).setR4RM(REG::SP).setImm32(NABI::shadowSpaceSize + volRegsSpillSize)();

  // Consume the new return address on stack
  as_.INSTR(RET_t)();
}
#endif

#if LINEAR_MEMORY_BOUNDS_CHECKS
void Backend::emitExtensionRequestFunction() {
  moduleInfo_.helperFunctionBinaryPositions.extensionRequest = output_.size();

  // Properly check whether the address is actually in bounds. The quick check that has been performed before this only
  // checked whether it is in bounds, but accessing the last 8 bytes would fail Add the 8 bytes to the cache register so
  // we get the actual memory size
  as_.INSTR(ADD_rm64_imm8sx).setR4RM(WasmABI::REGS::memSize).setImm8(8_U8)();
  as_.INSTR(CMP_rm64_r64).setR4RM(WasmABI::REGS::memSize).setR(NABI::gpParams[0])();
  RelPatchObj const withinBounds = as_.prepareJMP(false, CC::GE);

  //
  //

  // Reserve space on stack and spill all volatile registers since we will call a native function
  uint32_t const spillSize = roundUpToPow2((static_cast<uint32_t>(NABI::volRegs.size()) * 8U) + returnAddrWidth, 4U) - returnAddrWidth;
  as_.INSTR(SUB_rm64_imm32sx).setR4RM(REG::SP).setImm32(spillSize + NABI::shadowSpaceSize)();
#if ACTIVE_STACK_OVERFLOW_CHECK
  as_.checkStackFence();
#endif
  spillRestoreRegsRaw({NABI::volRegs.data(), NABI::volRegs.size()}, false, NABI::shadowSpaceSize);

  // Load the other arguments for the extension helper, the accessed address is already in the first register
  uint32_t const basedataLength = moduleInfo_.getBasedataLength();
  as_.INSTR(MOV_r32_imm32).setR(NABI::gpParams[1]).setImm32(basedataLength)();
  as_.INSTR(MOV_r64_rm64).setR(NABI::gpParams[2]).setR4RM(WasmABI::REGS::linMem)();

  // Call extension request
  static_assert(sizeof(uintptr_t) <= 8, "uintptr_t datatype too large");
  as_.INSTR(CALL_rm64_t).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::memoryHelperPtr)();

  // Check the return value. If it's zero extension of memory failed
  as_.INSTR(CMP_rm64_imm8sx).setR4RM(NABI::gpRetReg).setImm8(0x00U)();
  as_.cTRAP(TrapCode::LINMEM_COULDNOTEXTEND, CC::E);

  // Check if the return value is all ones: In this case the module tried to access memory beyond the allowed number of
  // (Wasm) pages
  as_.INSTR(CMP_rm64_imm8sx).setR4RM(NABI::gpRetReg).setImm8(0xFFU)();
  as_.cTRAP(TrapCode::LINMEM_OUTOFBOUNDSACCESS, CC::E);

  // Calculate the new base of the linear memory by adding basedataLength to the new memory base and store it in
  // REGS::linMem
  as_.INSTR(LEA_r64_m_t).setR(WasmABI::REGS::linMem).setM4RM(NABI::gpRetReg, static_cast<int32_t>(basedataLength))();

  // Restore all previously spilled registers, then unwind the stack
  spillRestoreRegsRaw({NABI::volRegs.data(), NABI::volRegs.size()}, true, NABI::shadowSpaceSize);
  as_.INSTR(ADD_rm64_imm32sx).setR4RM(REG::SP).setImm32(spillSize + NABI::shadowSpaceSize)();

  // Load the actual memory size, maybe it changed
  as_.INSTR(MOV_r32_rm32).setR(WasmABI::REGS::memSize).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::actualLinMemByteSize)();

  //
  //

  withinBounds.linkToHere();

  // Set up the register for the cached memory size again and then return
  as_.INSTR(SUB_rm64_imm8sx).setR4RM(WasmABI::REGS::memSize).setImm8(8_U8)();
  as_.INSTR(RET_t)();
}
#endif

#if LINEAR_MEMORY_BOUNDS_CHECKS
void Backend::emitLinMemBoundsCheck(REG const addrReg, int32_t displ, uint8_t const memObjSize) const {
  assert(moduleInfo_.helperFunctionBinaryPositions.extensionRequest != 0xFF'FF'FF'FF && "Extension request wrapper has not been produced yet");
  assert(moduleInfo_.fnc.stackFrameSize == as_.alignStackFrameSize(moduleInfo_.fnc.stackFrameSize) && "Stack not aligned");
  assert(displ >= 0);
  assert(in_range<int8_t>(static_cast<int32_t>(memObjSize)));

  if (addrReg == REG::NONE) {
    uint32_t const bytesNeeded = static_cast<uint32_t>(displ) + memObjSize;

    if (in_range<int8_t>(displ)) {
      as_.INSTR(CMP_rm64_imm8sx).setR4RM(WasmABI::REGS::memSize).setImm8(static_cast<uint8_t>(displ))();
    } else {
      as_.INSTR(CMP_rm64_imm32sx).setR4RM(WasmABI::REGS::memSize).setImm32(static_cast<uint32_t>(displ))();
    }

    RelPatchObj const withinBounds = as_.prepareJMP(true, CC::GE);
    as_.INSTR(PUSH_r64_t).setR(NABI::gpParams[0])(); // push
    as_.INSTR(PUSH_r64_t).setR(NABI::gpParams[0])(); // push twice so RSP is aligned
    if (in_range<int32_t>(static_cast<int64_t>(bytesNeeded))) {
      as_.INSTR(MOV_rm64_imm32sx).setR4RM(NativeABI::gpParams[0]).setImm32(bit_cast<uint32_t>(bytesNeeded))();
    } else {
      as_.INSTR(MOV_rm64_imm32sx).setR4RM(NativeABI::gpParams[0]).setImm32(bit_cast<uint32_t>(displ))();
      as_.INSTR(ADD_rm64_imm8sx).setR4RM(NativeABI::gpParams[0]).setImm8(memObjSize)();
    }
    as_.INSTR(CALL_rel32_t).setRel32(0)(); // CALL extension request
    RelPatchObj(false, output_.size(), output_).linkToBinaryPos(moduleInfo_.helperFunctionBinaryPositions.extensionRequest);

    as_.INSTR(POP_r64_t).setR(NABI::gpParams[0])(); // pop
    as_.INSTR(POP_r64_t).setR(NABI::gpParams[0])(); // pop twice so RSP is aligned
    withinBounds.linkToHere();
  } else {
    as_.INSTR(CMP_rm64_r64).setR4RM(WasmABI::REGS::memSize).setR(addrReg)();

    RelPatchObj const withinBounds = as_.prepareJMP(true, CC::GE);
    as_.INSTR(PUSH_r64_t).setR(NABI::gpParams[0])(); // push
    as_.INSTR(PUSH_r64_t).setR(NABI::gpParams[0])(); // push twice so RSP is aligned
    as_.INSTR(LEA_r64_m_t).setR(NABI::gpParams[0]).setM4RM(addrReg, memObjSize)();
    as_.INSTR(CALL_rel32_t).setRel32(0)(); // CALL extension request
    RelPatchObj(false, output_.size(), output_).linkToBinaryPos(moduleInfo_.helperFunctionBinaryPositions.extensionRequest);
    as_.INSTR(POP_r64_t).setR(NABI::gpParams[0])(); // pop
    as_.INSTR(POP_r64_t).setR(NABI::gpParams[0])(); // pop twice so RSP is aligned
    withinBounds.linkToHere();
  }
}
#endif

Backend::LiftedRegDisp Backend::prepareLinMemAddrProt(StackElement *const addrElem, uint32_t const offset, RegAllocTracker &regAllocTracker,
                                                      StackElement const *const targetHint) const {
  int64_t constAddr{0};
  bool const addrIsConst{addrElem->type == StackType::CONSTANT_I32};
  if (addrIsConst) {
    constAddr = static_cast<int64_t>(addrElem->data.constUnion.u32) + static_cast<int64_t>(offset);
  }

  if (addrIsConst && in_range<int32_t>(constAddr)) {
    return {{REG::NONE, false}, static_cast<int32_t>(constAddr)};
  } else {
#if LINEAR_MEMORY_BOUNDS_CHECKS
    assert(moduleInfo_.helperFunctionBinaryPositions.extensionRequest != 0xFF'FF'FF'FF && "Extension request wrapper has not been produced yet");
    Common::LiftedReg const liftedAddrReg{
        common_.liftToRegInPlaceProt(*addrElem, offset > 0, targetHint, regAllocTracker)}; // only has to be writable if offset > 0
    REG const addrReg = liftedAddrReg.reg;
    if (offset > 0) {
      if (in_range<int8_t>(static_cast<int64_t>(offset))) {
        as_.INSTR(ADD_rm64_imm8sx).setR4RM(addrReg).setImm8(static_cast<uint8_t>(offset))();
      } else if (in_range<int32_t>(static_cast<int64_t>(offset))) {
        as_.INSTR(ADD_rm64_imm32sx).setR4RM(addrReg).setImm32(offset)();
      } else {
        // Corner case, doesn't need to be optimized
        as_.INSTR(MOV_rm32_imm32).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::spillRegion).setImm32(offset)();
        as_.INSTR(MOV_rm32_imm32).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::spillRegion + 4).setImm32(0_U32)();
        as_.INSTR(ADD_r64_rm64).setR(addrReg).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::spillRegion)();
      }
    }
    return {liftedAddrReg, 0};
#else
    Common::LiftedReg const liftedAddrReg{
        common_.liftToRegInPlaceProt(*addrElem, offset > static_cast<uint32_t>(INT32_MAX), targetHint, regAllocTracker)};
    int32_t displ;
    if (!in_range<int32_t>(static_cast<int64_t>(offset))) {
      // Corner case, doesn't need to be optimized
      REG const addrReg{liftedAddrReg.reg};
      as_.INSTR(MOV_rm32_imm32).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::spillRegion).setImm32(offset)();
      as_.INSTR(MOV_rm32_imm32).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::spillRegion + 4).setImm32(0_U32)();
      as_.INSTR(ADD_r64_rm64).setR(addrReg).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::spillRegion)();
      displ = 0;
    } else {
      displ = static_cast<int32_t>(offset);
    }
    static_cast<void>(displ);
    return {liftedAddrReg, displ};
#endif
  }
}

StackElement Backend::executeLinearMemoryLoad(OPCode const opcode, uint32_t const offset, Stack::iterator const addrElem,
                                              StackElement const *const targetHint) {
  assert(moduleInfo_.hasMemory && "Memory not defined");

  MachineType const resultType{getLoadResultType(opcode)};

  // coverity[autosar_cpp14_a8_5_2_violation]
  constexpr auto opcodeTemplates = make_array(MOV_r32_rm32.opTemplate, MOV_r64_rm64.opTemplate, MOVSS_rf_rmf.opTemplate, MOVSD_rf_rmf.opTemplate,
                                              MOVSX_r32_rm8_t, MOVZX_r32_rm8_t, MOVSX_r32_rm16_t, MOVZX_r32_rm16_t, MOVSX_r64_rm8_t, MOVZX_r64_rm8_t,
                                              MOVSX_r64_rm16_t, MOVZX_r64_rm16_t, MOVSXD_r64_rm32.opTemplate, MOV_r32_rm32.opTemplate);

  RegAllocTracker regAllocTracker{};
  LiftedRegDisp const addrRegDisp{prepareLinMemAddrProt(addrElem.unwrap(), offset, regAllocTracker, targetHint)};

#if LINEAR_MEMORY_BOUNDS_CHECKS
  constexpr auto memObjSizes = make_array(4_U8, 8_U8, 4_U8, 8_U8, 1_U8, 1_U8, 2_U8, 2_U8, 1_U8, 1_U8, 2_U8, 2_U8, 4_U8, 4_U8);
  emitLinMemBoundsCheck(addrRegDisp.liftedReg.reg, addrRegDisp.disp, memObjSizes[opcode - OPCode::I32_LOAD]);
#endif
  StackElement const *const verifiedTargetHint{(getUnderlyingRegIfSuitable(targetHint, resultType, RegMask::none()) != REG::NONE) ? targetHint
                                                                                                                                  : nullptr};
  RegElement targetRegElem{};
  if (((resultType == MachineType::I32) || (resultType == MachineType::I64)) && addrRegDisp.liftedReg.writable) {
    targetRegElem = {common_.getResultStackElement(addrElem.unwrap(), resultType), addrRegDisp.liftedReg.reg};
  } else {
    targetRegElem = common_.reqScratchRegProt(resultType, verifiedTargetHint, regAllocTracker, false);
  }
  as_.INSTR(opcodeTemplates[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_LOAD)])
      .setR(targetRegElem.reg)
      .setM4RM(WasmABI::REGS::linMem, addrRegDisp.disp, addrRegDisp.liftedReg.reg)();

  return targetRegElem.elem;
}

void Backend::executeLinearMemoryStore(OPCode const opcode, uint32_t const offset) {
  assert(moduleInfo_.hasMemory && "Memory not defined");
  Stack::iterator const valueElem{common_.condenseValentBlockBelow(stack_.end())};
  Stack::iterator const addrElem{common_.condenseValentBlockBelow(valueElem)};

  RegAllocTracker regAllocTracker{};
  regAllocTracker.futureLifts = mask(valueElem.unwrap());
  LiftedRegDisp const addrRegDisp{prepareLinMemAddrProt(addrElem.unwrap(), offset, regAllocTracker, nullptr)};

#if LINEAR_MEMORY_BOUNDS_CHECKS
  constexpr auto memObjSizes = make_array(4_U8, 8_U8, 4_U8, 8_U8, 1_U8, 2_U8, 1_U8, 2_U8, 4_U8);
  emitLinMemBoundsCheck(addrRegDisp.liftedReg.reg, addrRegDisp.disp, memObjSizes[opcode - OPCode::I32_STORE]);
#endif

  // If value is constant
  if (valueElem->getBaseType() == StackType::CONSTANT) {
    switch (opcode) { // GCOVR_EXCL_LINE
    case OPCode::I32_STORE:
      as_.INSTR(MOV_rm32_imm32)
          .setM4RM(WasmABI::REGS::linMem, addrRegDisp.disp, addrRegDisp.liftedReg.reg)
          .setImm32(valueElem->data.constUnion.u32)();
      break;
    case OPCode::I64_STORE: {
      if (in_range<int32_t>(bit_cast<int64_t>(valueElem->data.constUnion.u64))) {
        as_.INSTR(MOV_rm64_imm32sx)
            .setM4RM(WasmABI::REGS::linMem, addrRegDisp.disp, addrRegDisp.liftedReg.reg)
            .setImm32(static_cast<uint32_t>(valueElem->data.constUnion.u64))();
      } else {
        REG const srcReg{common_.liftToRegInPlaceProt(*valueElem, false, regAllocTracker).reg};
        as_.INSTR(MOV_rm64_r64).setM4RM(WasmABI::REGS::linMem, addrRegDisp.disp, addrRegDisp.liftedReg.reg).setR(srcReg)();
      }
      break;
    }
    case OPCode::F32_STORE:
      as_.INSTR(MOV_rm32_imm32)
          .setM4RM(WasmABI::REGS::linMem, addrRegDisp.disp, addrRegDisp.liftedReg.reg)
          .setImm32(bit_cast<uint32_t>(valueElem->data.constUnion.f32))();
      break;
    case OPCode::F64_STORE: {
      REG const srcReg{common_.liftToRegInPlaceProt(*valueElem, false, regAllocTracker).reg};
      as_.INSTR(MOVSD_rmf_rf).setM4RM(WasmABI::REGS::linMem, addrRegDisp.disp, addrRegDisp.liftedReg.reg).setR(srcReg)();
      break;
    }
    case OPCode::I32_STORE8: {
      uint32_t const imm{valueElem->data.constUnion.u32 & 0xFFU};
      as_.INSTR(MOV_rm8_imm8_t).setM4RM(WasmABI::REGS::linMem, addrRegDisp.disp, addrRegDisp.liftedReg.reg).setImm8(static_cast<uint8_t>(imm))();
      break;
    }
    case OPCode::I32_STORE16: {
      uint32_t const imm{valueElem->data.constUnion.u32 & 0xFFFFU};
      as_.INSTR(MOV_rm16_imm16_t).setM4RM(WasmABI::REGS::linMem, addrRegDisp.disp, addrRegDisp.liftedReg.reg).setImm16(static_cast<uint16_t>(imm))();
      break;
    }
    case OPCode::I64_STORE8: {
      uint64_t const imm{valueElem->data.constUnion.u64 & 0xFFU};
      as_.INSTR(MOV_rm8_imm8_t).setM4RM(WasmABI::REGS::linMem, addrRegDisp.disp, addrRegDisp.liftedReg.reg).setImm8(static_cast<uint8_t>(imm))();
      break;
    }
    case OPCode::I64_STORE16: {
      uint64_t const imm{valueElem->data.constUnion.u64 & 0xFFFFU};
      as_.INSTR(MOV_rm16_imm16_t).setM4RM(WasmABI::REGS::linMem, addrRegDisp.disp, addrRegDisp.liftedReg.reg).setImm16(static_cast<uint16_t>(imm))();
      break;
    }
    case OPCode::I64_STORE32:
      as_.INSTR(MOV_rm32_imm32)
          .setM4RM(WasmABI::REGS::linMem, addrRegDisp.disp, addrRegDisp.liftedReg.reg)
          .setImm32(static_cast<uint32_t>(valueElem->data.constUnion.u64))();
      break;
    // GCOVR_EXCL_START
    default: {
      UNREACHABLE(break, "Instruction is not a memory store instruction");
    }
      // GCOVR_EXCL_STOP
    }
  } else {
    // coverity[autosar_cpp14_a8_5_2_violation]
    constexpr auto templates = make_array(MOV_rm32_r32.opTemplate, MOV_rm64_r64.opTemplate, MOVSS_rmf_rf.opTemplate, MOVSD_rmf_rf.opTemplate,
                                          MOV_rm8_r8_t, MOV_rm16_r16_t, MOV_rm8_r8_t, MOV_rm16_r16_t, MOV_rm32_r32.opTemplate);
    REG const srcReg{common_.liftToRegInPlaceProt(*valueElem, false, regAllocTracker).reg};
    as_.INSTR(templates[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_STORE)])
        .setM4RM(WasmABI::REGS::linMem, addrRegDisp.disp, addrRegDisp.liftedReg.reg)
        .setR(srcReg)();
  }

  common_.removeReference(valueElem);
  common_.removeReference(addrElem);
  static_cast<void>(stack_.erase(valueElem));
  static_cast<void>(stack_.erase(addrElem));
}

void Backend::executeLinearMemoryCopy(Stack::iterator const dst, Stack::iterator const src, Stack::iterator const size) {
  RegAllocTracker regAllocTracker{};
  regAllocTracker.futureLifts = mask(src.unwrap()) | mask(dst.unwrap());
  // get size value before lift to reg if size is compile-time constant
  uint32_t sizeValue{0U};
  bool const sizeIsConstant{moduleInfo_.getStorage(*size).type == StorageType::CONSTANT};
  if (sizeIsConstant) {
    sizeValue = size->data.constUnion.u32;
  }
  REG const sizeReg{common_.liftToRegInPlaceProt(*size, true, regAllocTracker).reg};
  REG const srcReg{common_.liftToRegInPlaceProt(*src, true, regAllocTracker).reg};
  REG const dstReg{common_.liftToRegInPlaceProt(*dst, true, regAllocTracker).reg};
  REG const gpScratchReg{common_.reqScratchRegProt(MachineType::I64, regAllocTracker, false).reg};

  // if src + size is larger then the length of mem.data then trap
  // if dst + size is larger then the length of mem.data then trap
  // can be combined
  // max(src, dst) + size is larger then the length of mem.data then trap
#if LINEAR_MEMORY_BOUNDS_CHECKS
  as_.INSTR(MOV_r32_rm32).setR(gpScratchReg).setR4RM(srcReg)();
  as_.INSTR(CMP_r32_rm32).setR(dstReg).setR4RM(srcReg)();
  as_.INSTR(CMOVCC_r32_rm32_t).setR(gpScratchReg).setR4RM(dstReg).setCC(CC::A)();
  as_.INSTR(ADD_r64_rm64).setR(gpScratchReg).setR4RM(sizeReg)();
  emitLinMemBoundsCheck(gpScratchReg, 0, 0U);
  as_.INSTR(ADD_r64_rm64).setR(srcReg).setR4RM(WasmABI::REGS::linMem)();
  as_.INSTR(ADD_r64_rm64).setR(dstReg).setR4RM(WasmABI::REGS::linMem)();
#else
  as_.INSTR(ADD_r64_rm64).setR(srcReg).setR4RM(WasmABI::REGS::linMem)();
  as_.INSTR(ADD_r64_rm64).setR(dstReg).setR4RM(WasmABI::REGS::linMem)();
  as_.INSTR(MOV_r64_rm64).setR(gpScratchReg).setR4RM(srcReg)();
  as_.INSTR(CMP_r64_rm64).setR(dstReg).setR4RM(srcReg)();
  as_.INSTR(CMOVCC_r64_rm64_t).setR(gpScratchReg).setR4RM(dstReg).setCC(CC::A)();
  as_.INSTR(CMP_rm8_imm8).setM4RM(gpScratchReg, -1, sizeReg, 0U).setImm8(0U)();
#endif

  constexpr bool canOverlap{true};
  if (sizeIsConstant) {
    emitMemcpyWithConstSizeNoBoundsCheck(dstReg, srcReg, sizeReg, sizeValue, gpScratchReg, canOverlap);
  } else {
    emitMemcpyNoBoundsCheck(dstReg, srcReg, sizeReg, gpScratchReg, canOverlap);
  }

  common_.removeReference(size);
  common_.removeReference(src);
  common_.removeReference(dst);
  static_cast<void>(stack_.erase(size));
  static_cast<void>(stack_.erase(src));
  static_cast<void>(stack_.erase(dst));
}

void Backend::executeLinearMemoryFill(Stack::iterator const dst, Stack::iterator const value, Stack::iterator const size) {
  RegAllocTracker regAllocTracker{};
  regAllocTracker.futureLifts = mask(value.unwrap()) | mask(dst.unwrap());
  REG const sizeReg{common_.liftToRegInPlaceProt(*size, true, regAllocTracker).reg};
  REG const valueReg{common_.liftToRegInPlaceProt(*value, true, regAllocTracker).reg};
  REG const dstReg{common_.liftToRegInPlaceProt(*dst, true, regAllocTracker).reg};
  REG const gpScratchReg{common_.reqScratchRegProt(MachineType::I64, regAllocTracker, false).reg};

  common_.removeReference(size);
  common_.removeReference(value);
  common_.removeReference(dst);
  static_cast<void>(stack_.erase(size));
  static_cast<void>(stack_.erase(value));
  static_cast<void>(stack_.erase(dst));

  // if dst + size is larger then the length of mem.data then trap
#if LINEAR_MEMORY_BOUNDS_CHECKS
  as_.INSTR(LEA_r64_m_t).setR(gpScratchReg).setM4RM(dstReg, 0, sizeReg, 0U)();
  emitLinMemBoundsCheck(gpScratchReg, 0, 0U);
  as_.INSTR(ADD_r64_rm64).setR(dstReg).setR4RM(WasmABI::REGS::linMem)();
#else
  as_.INSTR(ADD_r64_rm64).setR(dstReg).setR4RM(WasmABI::REGS::linMem)();
  as_.INSTR(CMP_rm8_imm8).setM4RM(dstReg, -1, sizeReg, 0U).setImm8(0U)();
#endif

  // prepare value
  as_.INSTR(AND_rm32_imm32).setR4RM(valueReg).setImm32(0xFFU)();
  as_.MOVimm64(gpScratchReg, 0x01010101'01010101_U64);
  as_.INSTR(IMUL_r64_rm64).setR(valueReg).setR4RM(gpScratchReg)();
  // set 8 bytes
  uint32_t const check8{output_.size()};
  as_.INSTR(CMP_rm32_imm8sx).setR4RM(sizeReg).setImm8(8U)();
  RelPatchObj const lessThan8{as_.prepareJMP(true, CC::B)};
  as_.INSTR(MOV_rm64_r64).setM4RM(dstReg, -8, sizeReg).setR(valueReg)();
  as_.INSTR(SUB_rm32_imm8sx).setR4RM(sizeReg).setImm8(8U)();
  as_.prepareJMP(true, CC::NONE).linkToBinaryPos(check8);
  lessThan8.linkToHere();
  // check if (remaining) size is at least 1
  as_.INSTR(CMP_rm32_imm8sx).setR4RM(sizeReg).setImm8(1U)();
  RelPatchObj const quickFinished{as_.prepareJMP(true, CC::B)};
  // set 1 byte
  uint32_t const copy1{output_.size()};
  as_.INSTR(MOV_rm8_r8_t).setM4RM(dstReg, -1, sizeReg).setR(valueReg)();
  as_.INSTR(SUB_rm32_imm8sx).setR4RM(sizeReg).setImm8(1U)();
  as_.prepareJMP(true, CC::NE).linkToBinaryPos(copy1); // Jump back if not zero
  quickFinished.linkToHere();
}

/// @details Loads the current "Wasm" memory size into a scratch register (i32) and
/// pushes it onto the stack
void Backend::executeGetMemSize() const {
  assert(moduleInfo_.hasMemory && "No memory defined");

  RegAllocTracker regAllocTracker{};
  RegElement const targetRegElem{common_.reqScratchRegProt(MachineType::I32, regAllocTracker, false)};
  as_.INSTR(MOV_r32_rm32).setR(targetRegElem.reg).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::linMemWasmSize)(); // mov r32, [rbx - ??]
  common_.pushAndUpdateReference(targetRegElem.elem);
}

/// @details Condenses the upmost valent block on the stack, validates its type, pops it,
/// adds its value to the memory size and pushes the resulting memory size as an
/// i32 scratch register onto the stack
void Backend::executeMemGrow() {
  assert(moduleInfo_.hasMemory && "No memory defined");

  Stack::iterator const deltaElement{common_.condenseValentBlockBelow(stack_.end())};

  RegAllocTracker regAllocTracker{};
  RegElement gpOutputRegElem{common_.reqScratchRegProt(MachineType::I32, regAllocTracker, false)};
  as_.INSTR(MOV_r32_rm32).setR(gpOutputRegElem.reg).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::linMemWasmSize)();

  // coverity[autosar_cpp14_a8_5_2_violation]
  constexpr auto ops = make_array(ADD_rm32_imm8sx, ADD_rm32_imm32, ADD_rm32_r32, ADD_r32_rm32);
  regAllocTracker = RegAllocTracker();
  gpOutputRegElem.elem = as_.selectInstr(ops, &gpOutputRegElem.elem, deltaElement.unwrap(), nullptr, RegMask::none(), false).element;
  gpOutputRegElem.reg =
      common_.liftToRegInPlaceProt(gpOutputRegElem.elem, true, regAllocTracker).reg; // Let's make absolutely sure it's in a register

  RelPatchObj const error{as_.prepareJMP(true, CC::C)};
  RelPatchObj noError{};
  if (moduleInfo_.memoryHasSizeLimit) {
    StackElement const constElem{StackElement::i32Const(moduleInfo_.memoryMaximumSize)};
    bool const reversed{emitComparison(OPCode::I32_LE_U, &gpOutputRegElem.elem, &constElem)};
    noError = as_.prepareJMP(true, reversed ? CC::AE : CC::BE);
  } else {
    as_.INSTR(CMP_rm32_imm32).setR4RM(gpOutputRegElem.reg).setImm32(1_U32 << 16_U32)();
    noError = as_.prepareJMP(true, CC::BE);
  }

  error.linkToHere();
  as_.INSTR(MOV_r32_imm32).setR(gpOutputRegElem.reg).setImm32(0xFF'FF'FF'FFU)();
  RelPatchObj const toEnd{as_.prepareJMP(false)};

  noError.linkToHere();

#if !LINEAR_MEMORY_BOUNDS_CHECKS
  // Notify the allocator of the memory growth
  constexpr uint32_t spillSize{static_cast<uint32_t>(NABI::volRegs.size()) * 8U};
  uint32_t const newStackFrameSize{as_.alignStackFrameSize(moduleInfo_.fnc.stackFrameSize + spillSize)};
  uint32_t const stackFrameSizeDelta{(NABI::shadowSpaceSize + newStackFrameSize) - moduleInfo_.fnc.stackFrameSize};

  // Reserve space on stack and spill all volatile registers since we will call a native function
  as_.INSTR(SUB_rm64_imm32sx).setR4RM(REG::SP).setImm32(stackFrameSizeDelta)();
#if ACTIVE_STACK_OVERFLOW_CHECK
  as_.checkStackFence();
#endif
  spillRestoreRegsRaw({NABI::volRegs.data(), NABI::volRegs.size()}, false, NABI::shadowSpaceSize);

  // Load the arguments for the call (in this order because gpOutputReg could be one of the gpParams)
  as_.INSTR(MOV_r64_rm64).setR(NABI::gpParams[1]).setR4RM(gpOutputRegElem.reg)();
  as_.INSTR(MOV_r64_rm64).setR(NABI::gpParams[0]).setR4RM(WasmABI::REGS::linMem)();

  // Call memory helper (extend)
  static_assert(sizeof(uintptr_t) <= 8, "uintptr_t datatype too large");
  as_.INSTR(CALL_rm64_t).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::memoryHelperPtr)();

  // Check return value
  as_.INSTR(CMP_rm64_imm8sx).setR4RM(NABI::gpRetReg).setImm8(0U)();
  as_.cTRAP(TrapCode::LINMEM_COULDNOTEXTEND, CC::E);

  // Restore all previously spilled registers, then unwind the stack
  spillRestoreRegsRaw({NABI::volRegs.data(), NABI::volRegs.size()}, true, NABI::shadowSpaceSize);
  as_.INSTR(ADD_rm64_imm32sx).setR4RM(REG::SP).setImm32(stackFrameSizeDelta)();
#endif

  as_.INSTR(XCHG_rm32_r32_t).setM4RM(WasmABI::REGS::linMem, -BD::FromEnd::linMemWasmSize).setR(gpOutputRegElem.reg)();

  toEnd.linkToHere();
  common_.replaceAndUpdateReference(deltaElement, gpOutputRegElem.elem);
}

StackElement Backend::emitSelect(StackElement &truthyResult, StackElement &falsyResult, StackElement &condElem,
                                 StackElement const *const targetHint) const {
  MachineType const resultType{moduleInfo_.getMachineType(&truthyResult)};

  bool const isInt{MachineTypeUtil::isInt(resultType)};

  RegAllocTracker regAllocTracker{};
  regAllocTracker.futureLifts = mask(&condElem) | mask(&truthyResult) | mask(&falsyResult);
  RegElement const resultRegElem{common_.reqScratchRegProt(resultType, targetHint, regAllocTracker, false)};
  VariableStorage const resultStorage{VariableStorage::reg(resultRegElem.reg, resultType)};
  REG const condReg{common_.liftToRegInPlaceProt(condElem, false, regAllocTracker).reg};
  if (isInt) {
    CC movCond;
    StackElement const *firstMoveElement;
    StackElement *secondMoveElement;

    // resultRegElem may be equal to falsyReg, normalize the  `resultRegElem = cond?trueResult:falseResult` to `resultRegElem =
    // !cond?falseResult:trueResult` Because load the first value to resultRegElem can overwrite falsyResult in this case
    VariableStorage const falsyStorage{moduleInfo_.getStorage(falsyResult)};
    if ((falsyStorage.type == StorageType::REGISTER) && (falsyStorage.location.reg == resultRegElem.reg)) {
      firstMoveElement = &falsyResult;
      secondMoveElement = &truthyResult;
      movCond = CC::NE;
    } else {
      firstMoveElement = &truthyResult;
      secondMoveElement = &falsyResult;
      movCond = CC::E;
    }

    VariableStorage srcStorage{moduleInfo_.getStorage(*firstMoveElement)};
    srcStorage.machineType = resultType;
    emitMoveIntImpl(resultStorage, srcStorage, false);

    vb::x86_64::OPCodeTemplate const opCode{MachineTypeUtil::is64(resultType) ? CMOVCC_r64_rm64_t : CMOVCC_r32_rm32_t};
    VariableStorage const secondStorage{moduleInfo_.getStorage(*secondMoveElement)};

    if (secondStorage.type == StorageType::STACKMEMORY) {
      RegDisp const srcRegDisp{getMemRegDisp(secondStorage)};
      as_.INSTR(TEST_rm64_r64_t).setR(condReg).setR4RM(condReg)();
      as_.INSTR(opCode).setR(resultRegElem.reg).setM4RM(srcRegDisp.reg, srcRegDisp.disp).setCC(movCond)();
    } else {
      // cmovcc doesn't receive imm as operand
      REG const secondMoveReg{common_.liftToRegInPlaceProt(*secondMoveElement, false, regAllocTracker).reg};
      // since the active stack overflow check will change the CPUFlag, so move the test instr here
      as_.INSTR(TEST_rm64_r64_t).setR(condReg).setR4RM(condReg)();
      as_.INSTR(opCode).setR(resultRegElem.reg).setR4RM(secondMoveReg).setCC(movCond)();
    }
  } else {
    as_.INSTR(TEST_rm64_r64_t).setR(condReg).setR4RM(condReg)();
    RelPatchObj const selectChoice{as_.prepareJMP(true, CC::NE)};
    emitMoveFloatImpl(resultStorage, moduleInfo_.getStorage(falsyResult), false);
    RelPatchObj const endJmp{as_.prepareJMP(true)};
    selectChoice.linkToHere();
    emitMoveFloatImpl(resultStorage, moduleInfo_.getStorage(truthyResult), false);
    endJmp.linkToHere();
  }
  return resultRegElem.elem;
}

StackElement Backend::emitCmpResult(BC const branchCond, StackElement const *const targetHint) const {
  assert(((moduleInfo_.lastBC == branchCond) || (moduleInfo_.lastBC == negateBC(branchCond)) || (moduleInfo_.lastBC == reverseBC(branchCond)) ||
          (branchCond == BC::UNCONDITIONAL)) &&
         "BranchCondition not matching");
  bool const bcIsFloat{(static_cast<uint8_t>(branchCond) >= static_cast<uint8_t>(BC::EQ_F)) &&
                       (static_cast<uint8_t>(branchCond) <= static_cast<uint8_t>(BC::GE_F))};
  CC const majorPositiveCC{CCforBC(branchCond)};

  StackElement condLoadTarget{};
  REG const targetHintReg{getUnderlyingRegIfSuitable(targetHint, MachineType::I32, RegMask::none())};
  MachineType const targetHintType{moduleInfo_.getMachineType(targetHint)};
  if ((targetHint != nullptr) && ((targetHintReg != REG::NONE) || (targetHintType == MachineType::I32))) {
    condLoadTarget = common_.getResultStackElement(targetHint, MachineType::I32);
  } else {
    RegAllocTracker regAllocTracker{};
    RegElement const targetRegElem{common_.reqScratchRegProt(MachineType::I32, regAllocTracker, true)};
    condLoadTarget = targetRegElem.elem;
  }
  VariableStorage targetStorage{moduleInfo_.getStorage(condLoadTarget)};
  // coverity[autosar_cpp14_a8_5_2_violation]
  auto const emitSetCC = [this, &targetStorage](CC const cc) {
    emitMoveIntImpl(targetStorage, VariableStorage::zero(targetStorage.machineType), false, true);
    if (targetStorage.type == StorageType::REGISTER) {
      as_.INSTR(SETCC_rm8).setCC(cc).setR8_4RM(targetStorage.location.reg)();
    } else {
      RegDisp const dstRegDisp{getMemRegDisp(targetStorage)};
      as_.INSTR(SETCC_rm8).setCC(cc).setM8_4RM(dstRegDisp.reg, dstRegDisp.disp)();
    }
  };

  if (bcIsFloat) {
    // coverity[autosar_cpp14_a8_5_2_violation]
    auto const emitFloatResultWithNanCheck = [this, &targetStorage, &condLoadTarget](CC const cc) {
      RegAllocTracker regAllocTracker{};
      regAllocTracker.writeProtRegs = mask(&condLoadTarget);
      RegElement const regElement{common_.reqScratchRegProt(MachineType::I32, regAllocTracker, true)};
      emitMoveIntWithCastTo32(targetStorage, VariableStorage::i32Const(0U), false, true);
      as_.INSTR(MOV_r32_imm32).setR(regElement.reg).setImm32(0U)();
      if (targetStorage.type == StorageType::REGISTER) {
        as_.INSTR(SETCC_rm8).setCC(cc).setR8_4RM(targetStorage.location.reg)();
        as_.INSTR(SETCC_rm8).setCC(CC::NP).setR8_4RM(regElement.reg)();
        as_.INSTR(AND_r32_rm32).setR(targetStorage.location.reg).setR4RM(regElement.reg)();
      } else {
        as_.INSTR(SETCC_rm8).setCC(cc).setR8_4RM(regElement.reg)();
        RegDisp const dstRegDisp{getMemRegDisp(targetStorage)};
        as_.INSTR(SETCC_rm8).setCC(CC::NP).setM8_4RM(dstRegDisp.reg, dstRegDisp.disp)();
        as_.INSTR(AND_r32_rm32).setR(regElement.reg).setM8_4RM(dstRegDisp.reg, dstRegDisp.disp)();
        condLoadTarget = regElement.elem;
      }
    };

    switch (branchCond) {
    case BC::EQ_F: {
      // coverity[autosar_cpp14_a4_5_1_violation]
      emitFloatResultWithNanCheck(CC::E);
      break;
    }
    case BC::NE_F: {
      RegAllocTracker regAllocTracker{};
      regAllocTracker.writeProtRegs = mask(&condLoadTarget);
      RegElement const regElement{common_.reqScratchRegProt(MachineType::I32, regAllocTracker, true)};
      emitMoveIntWithCastTo32(targetStorage, VariableStorage::i32Const(0U), false, true);
      as_.INSTR(MOV_r32_imm32).setR(regElement.reg).setImm32(0U)();
      if (targetStorage.type == StorageType::REGISTER) {
        as_.INSTR(SETCC_rm8).setCC(CC::NE).setR8_4RM(targetStorage.location.reg)();
        as_.INSTR(SETCC_rm8).setCC(CC::P).setR8_4RM(regElement.reg)();
        as_.INSTR(OR_r32_rm32).setR(targetStorage.location.reg).setR4RM(regElement.reg)();
      } else {
        as_.INSTR(SETCC_rm8).setCC(CC::NE).setR8_4RM(regElement.reg)();
        RegDisp const dstRegDisp{getMemRegDisp(targetStorage)};
        as_.INSTR(SETCC_rm8).setCC(CC::P).setM8_4RM(dstRegDisp.reg, dstRegDisp.disp)();
        as_.INSTR(OR_r32_rm32).setR(regElement.reg).setM8_4RM(dstRegDisp.reg, dstRegDisp.disp)();
        condLoadTarget = regElement.elem;
      }
      break;
    }
    case BC::LT_F: {
      // coverity[autosar_cpp14_a4_5_1_violation]
      emitFloatResultWithNanCheck(CC::B);
      break;
    }
    case BC::GT_F: {
      // coverity[autosar_cpp14_a4_5_1_violation]
      emitSetCC(CC::A);
      break;
    }
    case BC::LE_F: {
      // coverity[autosar_cpp14_a4_5_1_violation]
      emitFloatResultWithNanCheck(CC::BE);
      break;
    }
    default: {
      // GCOVR_EXCL_START
      assert((branchCond == BC::GE_F && "Unexpected branch condition"));
      // GCOVR_EXCL_STOP
      // coverity[autosar_cpp14_a4_5_1_violation]
      emitSetCC(CC::AE);
      break;
    }
    }
  } else {
    // coverity[autosar_cpp14_a4_5_1_violation]
    emitSetCC(majorPositiveCC);
  }
  return condLoadTarget;
}

StackElement Backend::emitDeferredAction(OPCode const opcode, StackElement *const arg0Ptr, StackElement *const arg1Ptr,
                                         StackElement const *const targetHint) {
  if ((opcode >= OPCode::I32_EQZ) && (opcode <= OPCode::F64_GE)) { // GCOVR_EXCL_LINE
    bool const reversed{emitComparison(opcode, arg0Ptr, arg1Ptr)};
    BC const condition{reversed ? reverseBC(BCforOPCode(opcode)) : BCforOPCode(opcode)};
    return emitCmpResult(condition, targetHint);
  } else {
    switch (opcode) { // GCOVR_EXCL_LINE
    case OPCode::I32_CLZ:
    case OPCode::I32_CTZ:
    case OPCode::I32_POPCNT: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(make_array(LZCNT_r32_rm32), make_array(TZCNT_r32_rm32), make_array(POPCNT_r32_rm32));
      return as_
          .selectInstr(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_CLZ)], nullptr, arg0Ptr, targetHint, RegMask::none(),
                       false)
          .element;
    }
    case OPCode::I32_ADD:
    case OPCode::I32_SUB: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(make_array(ADD_rm32_imm8sx, ADD_rm32_imm32, ADD_rm32_r32, ADD_r32_rm32),
                                      make_array(SUB_rm32_imm8sx, SUB_rm32_imm32, SUB_rm32_r32, SUB_r32_rm32));
      return as_
          .selectInstr(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_ADD)], arg0Ptr, arg1Ptr, targetHint, RegMask::none(),
                       false)
          .element;
    }
    case OPCode::I32_MUL: {
      return emitInstrsMul(arg0Ptr, arg1Ptr, targetHint, false);
    }
    case OPCode::I32_DIV_S:
    case OPCode::I32_DIV_U:
    case OPCode::I32_REM_S:
    case OPCode::I32_REM_U: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto isSigned = make_array(true, false, true, false);
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto isDiv = make_array(true, true, false, false);
      return emitInstrsDivRem(arg0Ptr, arg1Ptr, targetHint, isSigned[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_DIV_S)],
                              isDiv[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_DIV_S)], false);
    }
    case OPCode::I32_AND:
    case OPCode::I32_OR:
    case OPCode::I32_XOR: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(make_array(AND_rm32_imm8sx, AND_rm32_imm32, AND_rm32_r32, AND_r32_rm32),
                                      make_array(OR_rm32_imm8sx, OR_rm32_imm32, OR_rm32_r32, OR_r32_rm32),
                                      make_array(XOR_rm32_imm8sx, XOR_rm32_imm32, XOR_rm32_r32, XOR_r32_rm32));
      return as_
          .selectInstr(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_AND)], arg0Ptr, arg1Ptr, targetHint, RegMask::none(),
                       false)
          .element;
    }
    case OPCode::I32_SHL:
    case OPCode::I32_SHR_S:
    case OPCode::I32_SHR_U:
    case OPCode::I32_ROTL:
    case OPCode::I32_ROTR: {
      bool const arg1IsConst{arg1Ptr->type == (StackType::CONSTANT_I32)};
      if (arg1IsConst) {
        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto shiftOps =
            make_array(make_array(SHL_rm32_1, SHL_rm32_imm8), make_array(SAR_rm32_1, SAR_rm32_imm8), make_array(SHR_rm32_1, SHR_rm32_imm8),
                       make_array(ROL_rm32_1, ROL_rm32_imm8), make_array(ROR_rm32_1, ROR_rm32_imm8));
        return as_
            .selectInstr(shiftOps[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_SHL)], arg0Ptr, arg1Ptr, targetHint,
                         RegMask::none(), false)
            .element;
      } else {
        // coverity[autosar_cpp14_a7_1_2_violation] C++ 14 does not eval constexpr of union, need p1330r0 of C++20
        StackElement const regC{StackElement::scratchReg(REG::C, StackType::I32)};
        // spill regC
        spillFromStack(regC, mask(targetHint), false, false);
        // enforce arg1Ptr in ecx (cl), integer mov to reg never needs a scratch register
        emitMoveInt(regC, *arg1Ptr, MachineType::I32);

        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto shiftOps = make_array(make_array(SHL_rm32_omit_CL), make_array(SAR_rm32_omit_CL), make_array(SHR_rm32_omit_CL),
                                             make_array(ROL_rm32_omit_CL), make_array(ROR_rm32_omit_CL));
        return as_
            .selectInstr(shiftOps[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_SHL)], arg0Ptr, nullptr, targetHint, mask(REG::C),
                         false)
            .element;
      }
    }
    case OPCode::I64_CLZ:
    case OPCode::I64_CTZ:
    case OPCode::I64_POPCNT: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(make_array(LZCNT_r64_rm64), make_array(TZCNT_r64_rm64), make_array(POPCNT_r64_rm64));
      return as_
          .selectInstr(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_CLZ)], nullptr, arg0Ptr, targetHint, RegMask::none(),
                       false)
          .element;
    }
    case OPCode::I64_ADD:
    case OPCode::I64_SUB: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(make_array(ADD_rm64_imm8sx, ADD_rm64_imm32sx, ADD_rm64_r64, ADD_r64_rm64),
                                      make_array(SUB_rm64_imm8sx, SUB_rm64_imm32sx, SUB_rm64_r64, SUB_r64_rm64));
      return as_
          .selectInstr(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_ADD)], arg0Ptr, arg1Ptr, targetHint, RegMask::none(),
                       false)
          .element;
    }
    case OPCode::I64_MUL: {
      return emitInstrsMul(arg0Ptr, arg1Ptr, targetHint, true);
    }
    case OPCode::I64_DIV_S:
    case OPCode::I64_DIV_U:
    case OPCode::I64_REM_S:
    case OPCode::I64_REM_U: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto isSigned = make_array(true, false, true, false);
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto isDiv = make_array(true, true, false, false);
      return emitInstrsDivRem(arg0Ptr, arg1Ptr, targetHint, isSigned[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_DIV_S)],
                              isDiv[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_DIV_S)], true);
    }
    case OPCode::I64_AND:
    case OPCode::I64_OR:
    case OPCode::I64_XOR: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(make_array(AND_rm64_imm8sx, AND_rm64_imm32sx, AND_rm64_r64, AND_r64_rm64),
                                      make_array(OR_rm64_imm8sx, OR_rm64_imm32sx, OR_rm64_r64, OR_r64_rm64),
                                      make_array(XOR_rm64_imm8sx, XOR_rm64_imm32sx, XOR_rm64_r64, XOR_r64_rm64));
      return as_
          .selectInstr(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_AND)], arg0Ptr, arg1Ptr, targetHint, RegMask::none(),
                       false)
          .element;
    }
    case OPCode::I64_SHL:
    case OPCode::I64_SHR_S:
    case OPCode::I64_SHR_U:
    case OPCode::I64_ROTL:
    case OPCode::I64_ROTR: {
      bool const arg1IsConst{arg1Ptr->type == (StackType::CONSTANT_I64)};
      if (arg1IsConst) {
        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto shiftOps =
            make_array(make_array(SHL_rm64_1, SHL_rm64_imm8), make_array(SAR_rm64_1, SAR_rm64_imm8), make_array(SHR_rm64_1, SHR_rm64_imm8),
                       make_array(ROL_rm64_1, ROL_rm64_imm8), make_array(ROR_rm64_1, ROR_rm64_imm8));

        return as_
            .selectInstr(shiftOps[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_SHL)], arg0Ptr, arg1Ptr, targetHint,
                         RegMask::none(), false)
            .element;
      } else {
        // coverity[autosar_cpp14_a7_1_2_violation] C++ 14 does not eval constexpr of union, need p1330r0 of C++20
        StackElement const regC{StackElement::scratchReg(REG::C, StackType::I64)};
        // spill regC
        spillFromStack(regC, mask(targetHint), false, false);
        // enforce arg1Ptr in ecx (cl), integer mov to reg never needs a scratch register
        emitMoveInt(regC, *arg1Ptr, MachineType::I64);
        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto shiftOps = make_array(make_array(SHL_rm64_omit_CL), make_array(SAR_rm64_omit_CL), make_array(SHR_rm64_omit_CL),
                                             make_array(ROL_rm64_omit_CL), make_array(ROR_rm64_omit_CL));
        return as_
            .selectInstr(shiftOps[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_SHL)], arg0Ptr, nullptr, targetHint, mask(REG::C),
                         false)
            .element;
      }
    }
    case OPCode::F32_ABS: {
      constexpr StackElement maskElem{StackElement::i32Const(1_U32)};
      // coverity[autosar_cpp14_a7_1_2_violation] C++ 14 does not eval constexpr of union, need p1330r0 of C++20
      StackElement const result1{as_.selectInstr(make_array(PSLLD_rf_imm8), arg0Ptr, &maskElem, targetHint, RegMask::none(), false).element};
      return as_.selectInstr(make_array(PSRLD_rf_imm8), &result1, &maskElem, targetHint, RegMask::none(), false).element;
    }
    case OPCode::F32_NEG: {
      constexpr uint32_t signMask{1_U32 << 31_U32};
      // coverity[autosar_cpp14_a7_1_2_violation] Non-constexpr function 'memcpy' in bit_cast
      StackElement const maskElem{StackElement::f32Const(bit_cast<float>(signMask))};
      return as_.selectInstr(make_array(XORPS_rf_rmf), arg0Ptr, &maskElem, targetHint, RegMask::none(), false).element;
    }
    case OPCode::F32_CEIL:
    case OPCode::F32_FLOOR:
    case OPCode::F32_TRUNC:
    case OPCode::F32_NEAREST: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto roundModByte = make_array(2_U8, 1_U8, 3_U8, 0_U8);
      StackElement const resultElement{
          as_.selectInstr(make_array(ROUNDSS_rf_rmf_omit_imm8), nullptr, arg0Ptr, targetHint, RegMask::none(), false).element};
      output_.writeBytesLE(static_cast<uint64_t>(roundModByte[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::F32_CEIL)]), 1U);
      return resultElement;
    }
    case OPCode::F32_SQRT: {
      return as_.selectInstr(make_array(SQRTSS_rf_rmf), nullptr, arg0Ptr, targetHint, RegMask::none(), false).element;
    }
    case OPCode::F32_ADD:
    case OPCode::F32_SUB:
    case OPCode::F32_MUL:
    case OPCode::F32_DIV: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(make_array(ADDSS_rf_rmf), make_array(SUBSS_rf_rmf), make_array(MULSS_rf_rmf), make_array(DIVSS_rf_rmf));
      return as_
          .selectInstr(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::F32_ADD)], arg0Ptr, arg1Ptr, targetHint, RegMask::none(),
                       false)
          .element;
    }

    case OPCode::F32_MIN:
    case OPCode::F32_MAX: {
      return emitInstrsFloatMinMax(arg0Ptr, arg1Ptr, targetHint, opcode == OPCode::F32_MIN, false);
    }

    case OPCode::F32_COPYSIGN: {
      constexpr uint32_t signMask{1_U32 << 31_U32};
      // coverity[autosar_cpp14_a7_1_2_violation] Non-constexpr function 'memcpy' in bit_cast
      StackElement const fSignMask{StackElement::f32Const(bit_cast<float>(signMask))};
      // coverity[autosar_cpp14_a7_1_2_violation] Non-constexpr function 'memcpy' in bit_cast
      StackElement const fRestMask{StackElement::f32Const(bit_cast<float>(~signMask))};

      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto andps = make_array(ANDPS_rf_rmf);
      StackElement const returnValue1{as_.selectInstr(andps, arg1Ptr, &fSignMask, nullptr, RegMask::none(), false).element};
      StackElement const returnValue2{as_.selectInstr(andps, arg0Ptr, &fRestMask, targetHint, mask(&returnValue1), false).element};
      return as_.selectInstr(make_array(ORPS_rf_rmf), &returnValue1, &returnValue2, targetHint, mask(&returnValue1) | mask(&returnValue2), false)
          .element;
    }

    case OPCode::F64_ABS: {
      // coverity[autosar_cpp14_a7_1_2_violation] Non-constexpr function 'memcpy' in bit_cast
      StackElement const dummyElem{StackElement::i32Const(1_U32)};
      StackElement const result1{as_.selectInstr(make_array(PSLLQ_rf_imm8), arg0Ptr, &dummyElem, targetHint, RegMask::none(), false).element};
      return as_.selectInstr(make_array(PSRLQ_rf_imm8), &result1, &dummyElem, targetHint, RegMask::none(), false).element;
    }
    case OPCode::F64_NEG: {
      constexpr uint64_t signMask{1_U64 << 63_U64};
      // coverity[autosar_cpp14_a7_1_2_violation] Non-constexpr function 'memcpy' in bit_cast
      StackElement const fSignMask{StackElement::f64Const(bit_cast<double>(signMask))};
      return as_.selectInstr(make_array(XORPD_rf_rmf), arg0Ptr, &fSignMask, targetHint, RegMask::none(), false).element;
    }
    case OPCode::F64_CEIL:
    case OPCode::F64_FLOOR:
    case OPCode::F64_TRUNC:
    case OPCode::F64_NEAREST: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto roundModByte = make_array(2_U8, 1_U8, 3_U8, 0_U8);
      StackElement const resultElement{
          as_.selectInstr(make_array(ROUNDSD_rf_rmf_omit_imm8), nullptr, arg0Ptr, targetHint, RegMask::none(), false).element};
      output_.writeBytesLE(static_cast<uint64_t>(roundModByte[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::F64_CEIL)]), 1U);
      return resultElement;
    }
    case OPCode::F64_SQRT: {
      return as_.selectInstr(make_array(SQRTSD_rf_rmf), nullptr, arg0Ptr, targetHint, RegMask::none(), false).element;
    }
    case OPCode::F64_ADD:
    case OPCode::F64_SUB:
    case OPCode::F64_MUL:
    case OPCode::F64_DIV: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(make_array(ADDSD_rf_rmf), make_array(SUBSD_rf_rmf), make_array(MULSD_rf_rmf), make_array(DIVSD_rf_rmf));
      return as_
          .selectInstr(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::F64_ADD)], arg0Ptr, arg1Ptr, targetHint, RegMask::none(),
                       false)
          .element;
    }

    case OPCode::F64_MIN:
    case OPCode::F64_MAX: {
      return emitInstrsFloatMinMax(arg0Ptr, arg1Ptr, targetHint, opcode == OPCode::F64_MIN, true);
    }

    case OPCode::F64_COPYSIGN: {
      constexpr uint64_t signMask{1_U64 << 63_U64};
      // coverity[autosar_cpp14_a7_1_2_violation] Non-constexpr function 'memcpy' in bit_cast
      StackElement const fSignMask{StackElement::f64Const(bit_cast<double>(signMask))};
      // coverity[autosar_cpp14_a7_1_2_violation] Non-constexpr function 'memcpy' in bit_cast
      StackElement const fRestMask{StackElement::f64Const(bit_cast<double>(~signMask))};

      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto andpd = make_array(ANDPD_rf_rmf);
      StackElement const returnValue1{as_.selectInstr(andpd, arg1Ptr, &fSignMask, nullptr, RegMask::none(), false).element};
      StackElement const returnValue2{as_.selectInstr(andpd, arg0Ptr, &fRestMask, targetHint, mask(&returnValue1), false).element};
      return as_.selectInstr(make_array(ORPD_rf_rmf), &returnValue1, &returnValue2, targetHint, mask(&returnValue1) | mask(&returnValue2), false)
          .element;
    }

    case OPCode::I32_WRAP_I64: {
      // Needed so emitMove doesn't break the strict aliasing rule by accessing arg->u32
      if (arg0Ptr->type == (StackType::CONSTANT_I64)) {
        return StackElement::i32Const(static_cast<uint32_t>(arg0Ptr->data.constUnion.u64));
      } else {
        StackElement targetElem{StackElement::invalid()};
        MachineType targetHintType{MachineType::INVALID};
        // Try to use targetHint if it is a register
        // Otherwise, try to use arg0Ptr's register if it is writable
        // Otherwise, use targetHint stack memory
        // Request a scratch register is lowest priority
        if (targetHint != nullptr) {
          VariableStorage const targetHintStorage{moduleInfo_.getStorage(*targetHint)};
          targetHintType = targetHintStorage.machineType;
          if (MachineTypeUtil::isInt(targetHintType) && (targetHintStorage.type == StorageType::REGISTER)) {
            targetElem = common_.getResultStackElement(targetHint, MachineType::I32);
          }
        }
        if (targetElem.type == StackType::INVALID) {
          if (isWritableScratchReg(arg0Ptr)) {
            targetElem = StackElement::scratchReg(arg0Ptr->data.variableData.location.reg, StackType::I32);
          } else if ((targetHintType != MachineType::INVALID) && MachineTypeUtil::isInt(targetHintType)) {
            targetElem = common_.getResultStackElement(targetHint, MachineType::I32);
          } else {
            RegAllocTracker regAllocTracker{};
            regAllocTracker.readProtRegs = mask(arg0Ptr);
            targetElem = common_.reqScratchRegProt(MachineType::I32, regAllocTracker, false).elem;
          }
        }

        VariableStorage targetStorage{moduleInfo_.getStorage(targetElem)};
        VariableStorage sourceStorage{moduleInfo_.getStorage(*arg0Ptr)};
        sourceStorage.machineType =
            MachineType::I32; // "Reinterpret", since source is larger than dest (and if reg, both are GPR), we can safely read from source

        emitMoveIntWithCastTo32(targetStorage, sourceStorage, true, false);

        return targetElem;
      }
    }
    case OPCode::I32_TRUNC_F32_S:
    case OPCode::I32_TRUNC_F32_U:
    case OPCode::I32_TRUNC_F64_S:
    case OPCode::I32_TRUNC_F64_U: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto isSigned = make_array(true, false, true, false);
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto srcIs64 = make_array(false, false, true, true);
      return emitInstrsTruncFloatToInt(arg0Ptr, targetHint, isSigned[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_TRUNC_F32_S)],
                                       srcIs64[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_TRUNC_F32_S)], false);
    }

    case OPCode::I64_EXTEND_I32_S: {
      return as_.selectInstr(make_array(MOVSXD_r64_rm32), nullptr, arg0Ptr, targetHint, RegMask::none(), false).element;
    }
    case OPCode::I64_EXTEND_I32_U: {
      // Needed so emitMove doesn't break the strict aliasing rule by accessing arg->u64 on a 32-bit value
      if (arg0Ptr->type == StackType::CONSTANT_I32) {
        StackElement const newElement{StackElement::i64Const(static_cast<uint64_t>(arg0Ptr->data.constUnion.u32))};
        return newElement;
      } else {
        VariableStorage const targetHintStorage{(targetHint != nullptr) ? moduleInfo_.getStorage(*targetHint) : VariableStorage{}};
        VariableStorage const sourceStorage{moduleInfo_.getStorage(*arg0Ptr)};
        if (!targetHintStorage.inSameLocation(sourceStorage)) {
          RegAllocTracker regAllocTracker{};
          if (isWritableScratchReg(arg0Ptr)) {
            return StackElement::scratchReg(arg0Ptr->data.variableData.location.reg, StackType::I64);
          } else {
            RegElement const targetElem{common_.reqScratchRegProt(MachineType::I64, targetHint, regAllocTracker, false)};
            VariableStorage const dummyTarget{VariableStorage::reg(MachineType::I32, targetElem.reg)}; // "Reinterpret"
            emitMoveIntImpl(dummyTarget, sourceStorage, false);
            return targetElem.elem;
          }
        } else {
          return common_.getResultStackElement(arg0Ptr, MachineType::I64);
        }
      }
    }
    case OPCode::I32_EXTEND8_S:
    case OPCode::I32_EXTEND16_S:
    case OPCode::I64_EXTEND8_S:
    case OPCode::I64_EXTEND16_S:
    case OPCode::I64_EXTEND32_S: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto opcodeTemplates = make_array(MOVSX_r32_rm8_t, MOVSX_r32_rm16_t, MOVSX_r64_rm8_t, MOVSX_r64_rm16_t, MOVSXD_r64_rm32.opTemplate);

      RegAllocTracker regAllocTracker{};
      REG const inputReg{common_.liftToRegInPlaceProt(*arg0Ptr, false, regAllocTracker).reg};

      RegElement gpOutputRegElem{};
      if ((targetHint == nullptr) && isWritableScratchReg(arg0Ptr)) {
        gpOutputRegElem = {*arg0Ptr, inputReg};
      } else {
        bool const is64{(opcode != OPCode::I32_EXTEND8_S) && (opcode != OPCode::I32_EXTEND16_S)};
        gpOutputRegElem = common_.reqScratchRegProt(is64 ? MachineType::I64 : MachineType::I32, targetHint, regAllocTracker, false);
      }

      as_.INSTR(opcodeTemplates[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_EXTEND8_S)])
          .setR(gpOutputRegElem.reg)
          .setR4RM(inputReg)();
      return gpOutputRegElem.elem;
    }
    case OPCode::I64_TRUNC_F32_S:
    case OPCode::I64_TRUNC_F32_U:
    case OPCode::I64_TRUNC_F64_S:
    case OPCode::I64_TRUNC_F64_U: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto isSigned = make_array(true, false, true, false);
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto srcIs64 = make_array(false, false, true, true);
      return emitInstrsTruncFloatToInt(arg0Ptr, targetHint, isSigned[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_TRUNC_F32_S)],
                                       srcIs64[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_TRUNC_F32_S)], true);
    }

    case OPCode::F32_CONVERT_I32_S: {
      return as_.selectInstr(make_array(CVTSI2SS_rf_rm32), nullptr, arg0Ptr, targetHint, RegMask::none(), false).element;
    }
    case OPCode::F32_CONVERT_I32_U: {
      RegAllocTracker regAllocTracker{};
      static_cast<void>(common_.liftToRegInPlaceProt(*arg0Ptr, false, regAllocTracker));
      constexpr AbstrInstr CVTSI2SS_rf_rm32as64{{CVTSI2SS_rf_rm64.opTemplate}, ArgType::r32f, ArgType::rm32, true, false};
      return as_.selectInstr(make_array(CVTSI2SS_rf_rm32as64), nullptr, arg0Ptr, targetHint, RegMask::none(), false).element;
    }
    case OPCode::F32_CONVERT_I64_S: {
      return as_.selectInstr(make_array(CVTSI2SS_rf_rm64), nullptr, arg0Ptr, targetHint, RegMask::none(), false).element;
    }
    case OPCode::F32_CONVERT_I64_U: {
      return emitInstrsConvU64ToFloat(arg0Ptr, targetHint, false);
    }
    case OPCode::F32_DEMOTE_F64: {
      return as_.selectInstr(make_array(CVTSD2SS_rf_rmf), nullptr, arg0Ptr, targetHint, RegMask::none(), false).element;
    }

    case OPCode::F64_CONVERT_I32_S: {
      return as_.selectInstr(make_array(CVTSI2SD_rf_rm32), nullptr, arg0Ptr, targetHint, RegMask::none(), false).element;
    }
    case OPCode::F64_CONVERT_I32_U: {
      RegAllocTracker regAllocTracker{};
      static_cast<void>(common_.liftToRegInPlaceProt(*arg0Ptr, false, regAllocTracker));

      constexpr AbstrInstr CVTSI2SD_rf_rm32as64{CVTSI2SD_rf_rm64.opTemplate, ArgType::r64f, ArgType::rm32, true, false};
      return as_.selectInstr(make_array(CVTSI2SD_rf_rm32as64), nullptr, arg0Ptr, targetHint, RegMask::none(), false).element;
    }
    case OPCode::F64_CONVERT_I64_S: {
      return as_.selectInstr(make_array(CVTSI2SD_rf_rm64), nullptr, arg0Ptr, targetHint, RegMask::none(), false).element;
    }
    case OPCode::F64_CONVERT_I64_U: {
      return emitInstrsConvU64ToFloat(arg0Ptr, targetHint, true);
    }
    case OPCode::F64_PROMOTE_F32: {
      return as_.selectInstr(make_array(CVTSS2SD_rf_rmf), nullptr, arg0Ptr, targetHint, RegMask::none(), false).element;
    }

    case OPCode::I32_REINTERPRET_F32:
    case OPCode::I64_REINTERPRET_F64:
    case OPCode::F32_REINTERPRET_I32:
    case OPCode::F64_REINTERPRET_I64: {
      VariableStorage srcStorage{moduleInfo_.getStorage(*arg0Ptr)};
      switch (srcStorage.type) {
      case StorageType::CONSTANT: {
        switch (opcode) {
        case OPCode::I32_REINTERPRET_F32:
          return StackElement::i32Const(bit_cast<uint32_t>(arg0Ptr->data.constUnion.f32));
        case OPCode::I64_REINTERPRET_F64:
          return StackElement::i64Const(bit_cast<uint64_t>(arg0Ptr->data.constUnion.f64));
        case OPCode::F32_REINTERPRET_I32:
          return StackElement::f32Const(bit_cast<float>(arg0Ptr->data.constUnion.u32));
        case OPCode::F64_REINTERPRET_I64:
          return StackElement::f64Const(bit_cast<double>(arg0Ptr->data.constUnion.u64));
        // GCOVR_EXCL_START
        default: {
          UNREACHABLE(break, "Instruction is not a reinterpretation");
        }
          // GCOVR_EXCL_STOP
        }
      }
      case StorageType::REGISTER: {
        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto ops = make_array(make_array(MOVD_rm32_rf), make_array(MOVQ_rm64_rf), make_array(MOVD_rf_rm32), make_array(MOVQ_rf_rm64));
        return as_
            .selectInstr(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_REINTERPRET_F32)], nullptr, arg0Ptr, targetHint,
                         RegMask::none(), false)
            .element;
      }
      case StorageType::STACKMEMORY:
      case StorageType::LINKDATA: {
        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto dstTypes = make_array(MachineType::I32, MachineType::I64, MachineType::F32, MachineType::F64);
        MachineType const dstType{dstTypes[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_REINTERPRET_F32)]};
        StackElement targetElem{};
        if ((moduleInfo_.getMachineType(targetHint) == dstType) || (getUnderlyingRegIfSuitable(targetHint, dstType, RegMask::none()) != REG::NONE)) {
          targetElem = common_.getResultStackElement(targetHint, dstType);
        } else {
          RegAllocTracker regAllocTracker{};
          targetElem = common_.reqScratchRegProt(dstType, targetHint, regAllocTracker, false).elem;
        }
        srcStorage.machineType = dstType; // Reinterpret
        VariableStorage targetStorage{moduleInfo_.getStorage(targetElem)};
        targetStorage.machineType = dstType;
        emitMoveImpl(targetStorage, srcStorage, false);
        return targetElem;
      }
      // GCOVR_EXCL_START
      case StorageType::INVALID:
      default: {
        UNREACHABLE(break, "Unknown StorageType");
      }
        // GCOVR_EXCL_STOP
      }
    }
      // GCOVR_EXCL_START
    default: {
      UNREACHABLE(break, "Unknown instruction");
    }
      // GCOVR_EXCL_STOP
    }
  }
}

StackElement Backend::emitInstrsTruncFloatToInt(StackElement *const argPtr, StackElement const *const targetHint, bool const isSigned,
                                                bool const srcIs64, bool const dstIs64) {
  uint8_t const srcSize{srcIs64 ? static_cast<uint8_t>(8U) : static_cast<uint8_t>(4U)};

  AbstrInstr ucomis_rf_rmf{};
  AbstrInstr rounds_rf_rmf_omit_imm8{};
  if (srcIs64) {
    ucomis_rf_rmf = UCOMISD_rf_rmf;
    rounds_rf_rmf_omit_imm8 = ROUNDSD_rf_rmf_omit_imm8;
  } else {
    ucomis_rf_rmf = UCOMISS_rf_rmf;
    rounds_rf_rmf_omit_imm8 = ROUNDSS_rf_rmf_omit_imm8;
  }

  // Constants for compare
  FloatTruncLimitsExcl::RawLimits const rawLimits{FloatTruncLimitsExcl::getRawLimits(isSigned, srcIs64, dstIs64)};
  RelPatchObj const skipConstants{as_.prepareJMP(true)};
  uint32_t const extraConst{output_.size()};
  static_cast<void>(extraConst);
  if ((!isSigned) && dstIs64) {
    if (!srcIs64) {
      output_.writeBytesLE(static_cast<uint64_t>(bit_cast<uint32_t>(static_cast<float>(INT64_MAX))), 4U);
    } else {
      output_.writeBytesLE(bit_cast<uint64_t>(static_cast<double>(INT64_MAX)), 8U);
    }
  }
  uint32_t const maxLimit{output_.size()};
  output_.writeBytesLE(rawLimits.max, srcSize);
  uint32_t const minLimit{output_.size()};
  output_.writeBytesLE(rawLimits.min, srcSize);
  skipConstants.linkToHere();

  // Check bounds
  RegAllocTracker regAllocTracker{};
  REG const argReg{common_.liftToRegInPlaceProt(*argPtr, true, targetHint, regAllocTracker).reg};
  as_.INSTR(ucomis_rf_rmf).setR(argReg).setMIP4RMabs(maxLimit)();
  RelPatchObj const aboveEqualMax{as_.prepareJMP(true, CC::AE)};
  RelPatchObj const isNan{as_.prepareJMP(true, CC::P)};
  as_.INSTR(ucomis_rf_rmf).setR(argReg).setMIP4RMabs(minLimit)();
  RelPatchObj const aboveMin{as_.prepareJMP(true, CC::A)};
  aboveEqualMax.linkToHere();
  isNan.linkToHere();
  as_.TRAP(TrapCode::TRUNC_OVERFLOW);
  aboveMin.linkToHere();

  // Convert
  as_.INSTR(rounds_rf_rmf_omit_imm8).setR(argReg).setR4RM(argReg).setImm8(0x3U)();

  if (isSigned) {
    AbstrInstr cvts2si_r_rmf{};
    if (dstIs64) {
      cvts2si_r_rmf = srcIs64 ? CVTSD2SI_r64_rmf : CVTSS2SI_r64_rmf;
    } else {
      cvts2si_r_rmf = srcIs64 ? CVTSD2SI_r32_rmf : CVTSS2SI_r32_rmf;
    }
    return as_.selectInstr(make_array(cvts2si_r_rmf), nullptr, argPtr, targetHint, RegMask::none(), false).element;

  } else {
    if (dstIs64) {
      AbstrInstr sub_rf_rmf{};
      AbstrInstr cvts2si_r64_rmf{};
      if (srcIs64) {
        sub_rf_rmf = SUBSD_rf_rmf;
        cvts2si_r64_rmf = CVTSD2SI_r64_rmf;
      } else {
        sub_rf_rmf = SUBSS_rf_rmf;
        cvts2si_r64_rmf = CVTSS2SI_r64_rmf;
      }

      RegElement const gpOutputRegElem{common_.reqScratchRegProt(MachineType::I64, targetHint, regAllocTracker, false)};
      as_.INSTR(rounds_rf_rmf_omit_imm8).setR(argReg).setR4RM(argReg).setImm8(0x3U)();
      as_.INSTR(ucomis_rf_rmf).setR(argReg).setMIP4RMabs(extraConst)();
      RelPatchObj const inSignedRange{as_.prepareJMP(true, CC::B)};

      as_.INSTR(sub_rf_rmf).setR(argReg).setMIP4RMabs(extraConst)();
      as_.INSTR(cvts2si_r64_rmf).setR(gpOutputRegElem.reg).setR4RM(argReg)();
      as_.INSTR(BTC_rm64_imm8_t).setR4RM(gpOutputRegElem.reg).setImm8(63U)();
      RelPatchObj const toEnd{as_.prepareJMP(true)};

      inSignedRange.linkToHere();
      as_.INSTR(cvts2si_r64_rmf).setR(gpOutputRegElem.reg).setR4RM(argReg)();
      toEnd.linkToHere();

      return gpOutputRegElem.elem;
    } else {
      constexpr AbstrInstr CVTSD2SI_r64to32_rmf{{CVTSD2SI_r64_rmf.opTemplate}, ArgType::r32, ArgType::rm64f, true, false};
      constexpr AbstrInstr CVTSS2SI_r64to32_rmf{{CVTSS2SI_r64_rmf.opTemplate}, ArgType::r32, ArgType::rm32f, true, false};
      AbstrInstr const cvts2si_r64to32_rmf{srcIs64 ? CVTSD2SI_r64to32_rmf : CVTSS2SI_r64to32_rmf};
      StackElement const targetRegElem{as_.selectInstr(make_array(cvts2si_r64to32_rmf), nullptr, argPtr, targetHint, RegMask::none(), false).element};
      emitMoveInt(targetRegElem, targetRegElem, MachineType::I32); // clean higher bits
      return targetRegElem;
    }
  }
}

StackElement Backend::emitInstrsConvU64ToFloat(StackElement *const argPtr, StackElement const *const targetHint, bool const dstIs64) const {
  AbstrInstr adds_rf_rmf{};
  AbstrInstr cvtsi2s_rf_rm64{};
  MachineType dstType;
  if (dstIs64) {
    adds_rf_rmf = ADDSD_rf_rmf;
    cvtsi2s_rf_rm64 = CVTSI2SD_rf_rm64;
    dstType = MachineType::F64;
  } else {
    adds_rf_rmf = ADDSS_rf_rmf;
    cvtsi2s_rf_rm64 = CVTSI2SS_rf_rm64;
    dstType = MachineType::F32;
  }

  RegAllocTracker regAllocTracker{};
  REG const argReg{common_.liftToRegInPlaceProt(*argPtr, true, regAllocTracker).reg};
  REG const gpScratchReg{common_.reqScratchRegProt(MachineType::I64, regAllocTracker, false).reg};
  RegElement const fOutputRegElem{common_.reqScratchRegProt(dstType, targetHint, regAllocTracker, false)};

  as_.INSTR(TEST_rm64_r64_t).setR(argReg).setR4RM(argReg)();
  RelPatchObj const inSignedRange{as_.prepareJMP(true, CC::NS)};

  as_.INSTR(MOV_rm64_r64).setR4RM(gpScratchReg).setR(argReg)();
  as_.INSTR(SHR_rm64_1).setR4RM(argReg)();
  as_.INSTR(AND_rm64_imm8sx).setR4RM(gpScratchReg).setImm8(1U)();
  as_.INSTR(OR_r64_rm64).setR(gpScratchReg).setR4RM(argReg)();
  as_.INSTR(cvtsi2s_rf_rm64).setR(fOutputRegElem.reg).setR4RM(gpScratchReg)();
  as_.INSTR(adds_rf_rmf).setR(fOutputRegElem.reg).setR4RM(fOutputRegElem.reg)();
  RelPatchObj const toEnd{as_.prepareJMP(true)};

  inSignedRange.linkToHere();
  as_.INSTR(cvtsi2s_rf_rm64).setR(fOutputRegElem.reg).setR4RM(argReg)();
  toEnd.linkToHere();
  return fOutputRegElem.elem;
}

StackElement Backend::emitInstrsFloatMinMax(StackElement *const arg0Ptr, StackElement *const arg1Ptr, StackElement const *const targetHint,
                                            bool const isMin, bool const is64) {
  AbstrInstr ucomis_rf_rmf{};
  AbstrInstr movs_rf_rmf{};
  AbstrInstr orp_rf_rmf{};
  AbstrInstr andp_rf_rmf{};
  if (is64) {
    ucomis_rf_rmf = UCOMISD_rf_rmf;
    movs_rf_rmf = MOVSD_rf_rmf;
    orp_rf_rmf = ORPD_rf_rmf;
    andp_rf_rmf = ANDPD_rf_rmf;
  } else {
    ucomis_rf_rmf = UCOMISS_rf_rmf;
    movs_rf_rmf = MOVSS_rf_rmf;
    orp_rf_rmf = ORPS_rf_rmf;
    andp_rf_rmf = ANDPS_rf_rmf;
  }

  RegAllocTracker regAllocTracker{};
  REG const arg0Reg{common_.liftToRegInPlaceProt(*arg0Ptr, true, targetHint, regAllocTracker).reg};
  REG const arg1Reg{common_.liftToRegInPlaceProt(*arg1Ptr, false, regAllocTracker).reg};

  as_.INSTR(ucomis_rf_rmf).setR(arg0Reg).setR4RM(arg1Reg)();
  RelPatchObj const notNan{as_.prepareJMP(true, CC::NP)};

  as_.INSTR(movs_rf_rmf).setR(arg0Reg).setMIP4RM(0)();
  RelPatchObj const fRelPatchObj{RelPatchObj(false, output_.size(), output_)};
  RelPatchObj const toEnd2{as_.prepareJMP(true)};

  fRelPatchObj.linkToHere();
  if (is64) {
    output_.writeBytesLE(0x7FF8'0000'0000'0000_U64, 8U);
  } else {
    output_.writeBytesLE(0x7F'C0'00'00U, 4U);
  }

  notNan.linkToHere();
  RelPatchObj const notEqual{as_.prepareJMP(true, CC::NE)};
  as_.INSTR(isMin ? orp_rf_rmf : andp_rf_rmf).setR(arg0Reg).setR4RM(arg1Reg)();
  RelPatchObj const toEnd3{as_.prepareJMP(true)};
  notEqual.linkToHere();

  RelPatchObj const toEnd4{as_.prepareJMP(true, isMin ? CC::BE : CC::AE)};
  as_.INSTR(movs_rf_rmf).setR(arg0Reg).setR4RM(arg1Reg)();

  toEnd2.linkToHere();
  toEnd3.linkToHere();
  toEnd4.linkToHere();
  return *arg0Ptr;
}

StackElement Backend::emitInstrsMul(StackElement const *const arg0Ptr, StackElement const *const arg1Ptr, StackElement const *const targetHint,
                                    bool const is64) {
  AbstrInstr imul_r_rm_omit_imm8sx{};
  AbstrInstr imul_r_rm_omit_imm32_or_imm32sx{};
  AbstrInstr imul_r_rm{};
  if (is64) {
    imul_r_rm_omit_imm8sx = IMUL_r64_rm64_omit_imm8sx;
    imul_r_rm_omit_imm32_or_imm32sx = IMUL_r64_rm64_omit_imm32sx;
    imul_r_rm = IMUL_r64_rm64;
  } else {
    imul_r_rm_omit_imm8sx = IMUL_r32_rm32_omit_imm8sx;
    imul_r_rm_omit_imm32_or_imm32sx = IMUL_r32_rm32_omit_imm32;
    imul_r_rm = IMUL_r32_rm32;
  }

  bool const arg0IsConst{arg0Ptr->getBaseType() == StackType::CONSTANT};
  bool const arg1IsConst{arg1Ptr->getBaseType() == StackType::CONSTANT};
  if (arg0IsConst || arg1IsConst) {
    StackElement const *const constArg{arg0IsConst ? arg0Ptr : arg1Ptr};
    StackElement const *const otherArg{arg0IsConst ? arg1Ptr : arg0Ptr};
    uint64_t const constant{is64 ? constArg->data.constUnion.u64 : static_cast<uint64_t>(constArg->data.constUnion.u32)};
    if (in_range<int8_t>(bit_cast<int64_t>(constant))) {
      StackElement const resultElement{
          as_.selectInstr(make_array(imul_r_rm_omit_imm8sx), nullptr, otherArg, targetHint, RegMask::none(), false).element};
      output_.writeBytesLE(constant, 1U);
      return resultElement;
    } else if ((!is64) || in_range<int32_t>(bit_cast<int64_t>(constant))) {
      StackElement const resultElement{
          as_.selectInstr(make_array(imul_r_rm_omit_imm32_or_imm32sx), nullptr, otherArg, targetHint, RegMask::none(), false).element};
      output_.writeBytesLE(constant, 4U);
      return resultElement;
    } else {
      static_cast<void>(0);
    }
  }
  return as_.selectInstr(make_array(imul_r_rm), arg0Ptr, arg1Ptr, targetHint, RegMask::none(), false).element;
}

StackElement Backend::emitInstrsDivRem(StackElement const *const arg0Ptr, StackElement *const arg1Ptr, StackElement const *const targetHint,
                                       bool const isSigned, bool const isDiv, bool const is64) {
  assert(WasmABI::isResScratchReg(REG::A) && "REG::A must be a reserved scratch register, otherwise loading values could overwrite locals");
  assert(WasmABI::isResScratchReg(REG::D) && "REG::D must be a reserved scratch register, otherwise loading values could overwrite locals");

  DivRemAnalysisResult const analysisResult{analyzeDivRem(arg0Ptr, arg1Ptr)};

  StackElement regA{};
  StackElement regD{};
  StackElement zeroConst{};
  StackElement highestBitSetConst{};
  StackElement allOnesConst{};
  AbstrInstr cmp_rm_imm8sx{};
  AbstrInstr xor_r_rm{};
  AbstrInstr div_rm{};
  AbstrInstr idiv_rm{};
  OPCodeTemplate cd_t{};
  if (is64) {
    regA = StackElement::scratchReg(REG::A, StackType::I64);
    regD = StackElement::scratchReg(REG::D, StackType::I64);
    zeroConst = StackElement::i64Const(0_U64);
    highestBitSetConst = StackElement::i64Const(0x8000'0000'0000'0000_U64);
    allOnesConst = StackElement::i64Const(0xFFFF'FFFF'FFFF'FFFF_U64);
    cmp_rm_imm8sx = CMP_rm64_imm8sx;
    xor_r_rm = XOR_r64_rm64;
    div_rm = DIV_rm64;
    idiv_rm = IDIV_rm64;
    cd_t = CDO_t;
  } else {
    regA = StackElement::scratchReg(REG::A, StackType::I32);
    regD = StackElement::scratchReg(REG::D, StackType::I32);
    zeroConst = StackElement::i32Const(0_U32);
    highestBitSetConst = StackElement::i32Const(0x80000000_U32);
    allOnesConst = StackElement::i32Const(0xFFFFFFFF_U32);
    cmp_rm_imm8sx = CMP_rm32_imm8sx;
    xor_r_rm = XOR_r32_rm32;
    div_rm = DIV_rm32;
    idiv_rm = IDIV_rm32;
    cd_t = CDQ_t;
  }
  spillFromStack(regD, mask(REG::A) | mask(REG::D) | mask(targetHint), false, false);
  // exclude arg0 because if arg0 is already in RAX, it doesn't need to be spilled
  if ((arg0Ptr->getBaseType() == StackType::SCRATCHREGISTER) && (arg0Ptr->data.variableData.location.reg == REG::A)) {
    Stack::iterator const excludeIt{moduleInfo_.getReferenceToLastOccurrenceOnStack(*arg0Ptr)};
    if (!excludeIt.isEmpty() && (!excludeIt->data.variableData.indexData.prevOccurrence.isEmpty())) {
      // GCOVR_EXCL_START
      assert(excludeIt->data.variableData.indexData.nextOccurrence.isEmpty() && "excludeIt should be last Occurrence on stack");
      // GCOVR_EXCL_STOP
      spillFromStack(regA, mask(REG::A) | mask(REG::D) | mask(targetHint), false, false);
    }
  } else {
    spillFromStack(regA, mask(REG::A) | mask(REG::D) | mask(targetHint), false, false);
    emitMoveInt(regA, *arg0Ptr, is64 ? MachineType::I64 : MachineType::I32);
  }

  // For cmp and div we need it to be in reg or mem anyway
  if (arg1Ptr->getBaseType() == StackType::CONSTANT) {
    RegAllocTracker regAllocTracker{};
    regAllocTracker.writeProtRegs = mask(REG::A) | mask(REG::D);
    static_cast<void>(common_.liftToRegInPlaceProt(*arg1Ptr, false, regAllocTracker));
  }

#if ACTIVE_DIV_CHECK
  if (!analysisResult.mustNotBeDivZero) {
    static_cast<void>(as_.selectInstr(make_array(cmp_rm_imm8sx), arg1Ptr, &zeroConst, nullptr, RegMask::all(), true));
    as_.cTRAP(TrapCode::DIV_ZERO, CC::E);
  }
#endif

  AbstrInstr const instructions{isSigned ? idiv_rm : div_rm};

  // coverity[autosar_cpp14_a8_5_2_violation]
  auto const emitDivRemCore = [this, isSigned, instructions, cd_t, xor_r_rm, arg1Ptr, targetHint]() {
    if (isSigned) {
      as_.INSTR(cd_t)();
    } else {
      as_.INSTR(xor_r_rm).setR(REG::D).setR4RM(REG::D)();
    }
    static_cast<void>(as_.selectInstr(make_array(instructions), nullptr, arg1Ptr, targetHint, mask(REG::A) | mask(REG::D), false));
  };

  if (analysisResult.mustNotBeOverflow) {
    emitDivRemCore();
  } else {
    if (is64) {
      emitMoveInt(regD, highestBitSetConst, MachineType::I64);
      static_cast<void>(as_.selectInstr(make_array(CMP_r64_rm64), &regA, &regD, nullptr, RegMask::all(), true));
    } else {
      static_cast<void>(as_.selectInstr(make_array(CMP_rm32_imm32), &regA, &highestBitSetConst, nullptr, RegMask::all(), true));
    }
    RelPatchObj const noOverflow1{as_.prepareJMP(true, CC::NE)};
    static_cast<void>(as_.selectInstr(make_array(cmp_rm_imm8sx), arg1Ptr, &allOnesConst, nullptr, RegMask::all(), true));
    RelPatchObj const noOverflow2{as_.prepareJMP(true, CC::NE)};

    if (isSigned && isDiv) {
      as_.TRAP(TrapCode::DIV_OVERFLOW);
    } else {
      StackElement const dstElem{(isDiv) ? regA : regD};
      StackElement const srcElem{((!isDiv) && (!isSigned)) ? highestBitSetConst : zeroConst};
      emitMoveInt(dstElem, srcElem, is64 ? MachineType::I64 : MachineType::I32);
    }
    RelPatchObj const toEnd{as_.prepareJMP(true)};

    noOverflow1.linkToHere();
    noOverflow2.linkToHere();

    emitDivRemCore();

    toEnd.linkToHere();
  }
  return isDiv ? regA : regD;
}

Backend::RegDisp Backend::getMemRegDisp(VariableStorage const &storage) const {
  REG returnReg{REG::NONE};
  int64_t returnDisp{0};
  if (storage.type == StorageType::LINKDATA) {
    uint32_t const basedataLength{moduleInfo_.getBasedataLength()};
    returnReg = WasmABI::REGS::linMem;
    returnDisp = (-static_cast<int64_t>(basedataLength) + static_cast<int64_t>(Basedata::FromStart::linkData)) +
                 static_cast<int64_t>(storage.location.linkDataOffset);
  } else if (storage.type == StorageType::STACKMEMORY) {
    returnReg = REG::SP;
    returnDisp = static_cast<int64_t>(moduleInfo_.fnc.stackFrameSize) - static_cast<int64_t>(storage.location.stackFramePosition);
  } else {
    // GCOVR_EXCL_START
    UNREACHABLE(_, "Unknown StorageType");
    // GCOVR_EXCL_STOP
  }
  if ((static_cast<int32_t>(returnDisp) < INT32_MIN) || (static_cast<int32_t>(returnDisp) > INT32_MAX)) {
    throw ImplementationLimitationException(ErrorCode::Maximum_offset_reached);
  } // GCOVR_EXCL_LINE
  return {returnReg, static_cast<int32_t>(returnDisp)};
}

uint32_t Backend::reserveStackFrame(uint32_t const width) {
  uint32_t const newOffset{common_.getCurrentMaximumUsedStackFramePosition() + width};
  assert(newOffset <= moduleInfo_.fnc.stackFrameSize + width);
  if (newOffset > moduleInfo_.fnc.stackFrameSize) {
    uint32_t const newAlignedStackFrameSize{as_.alignStackFrameSize(newOffset + 32U)};
    updateStackFrameSizeHelper(newAlignedStackFrameSize);
  }
  return newOffset;
}

void Backend::execPadding(uint32_t const paddingSize) {
  for (uint32_t i{0U}; i < paddingSize; i++) {
    as_.INSTR(NOP)();
  }
}

uint32_t Backend::getParamPos(REG const reg, bool const import) const VB_NOEXCEPT {
  if (import) {
    return NativeABI::getNativeParamPos(reg);
  } else {
    uint32_t const regPos{WasmABI::getRegPos(reg)};
    uint32_t pos;
    if (RegUtil::isGPR(reg)) {
      pos = regPos - moduleInfo_.getLocalStartIndexInGPRs();
    } else {
      pos = regPos - moduleInfo_.getLocalStartIndexInFPRs();
    }
    if (pos < WasmABI::regsForParams) {
      return pos;
    } else {
      return static_cast<uint32_t>(UINT8_MAX);
    }
  }
}

void Backend::emitMoveIntWithCastTo32(VariableStorage &targetStorage, VariableStorage const &sourceStorage, bool const unconditional,
                                      bool const preserveFlags) const {
  // GCOVR_EXCL_START
  assert(MachineTypeUtil::isInt(sourceStorage.machineType) && MachineTypeUtil::isInt(targetStorage.machineType));
  // GCOVR_EXCL_STOP

  if (targetStorage.type == StorageType::REGISTER) { // X -> Reg
    targetStorage.machineType = MachineType::I32;    // "Reinterpret" to mov i32_reg
    emitMoveIntImpl(targetStorage, sourceStorage, unconditional, preserveFlags);
  } else {
    // No cast needed if types are match
    if ((sourceStorage.machineType == MachineType::I32) && (targetStorage.machineType == MachineType::I32)) {
      emitMoveIntImpl(targetStorage, sourceStorage, unconditional, preserveFlags);
      return;
    }
    if (targetStorage.inMemory() && sourceStorage.inMemory()) { // Mem ->Mem
      RegDisp const srcRegDisp{getMemRegDisp(sourceStorage)};
      RegDisp const dstRegDisp{getMemRegDisp(targetStorage)};
      as_.INSTR(MOVSS_rf_rmf).setR(WasmABI::REGS::moveHelper).setM4RM(srcRegDisp.reg, srcRegDisp.disp)();
      as_.INSTR(MachineTypeUtil::is64(targetStorage.machineType) ? MOVSD_rmf_rf : MOVSS_rmf_rf)
          .setR(WasmABI::REGS::moveHelper)
          .setM4RM(dstRegDisp.reg, dstRegDisp.disp)();
    } else { // Reg -> Mem
      // GCOVR_EXCL_START
      assert(targetStorage.inMemory());
      // GCOVR_EXCL_STOP
      if (targetStorage.machineType == MachineType::I32) {
        emitMoveIntImpl(targetStorage, sourceStorage, unconditional, preserveFlags);
      } else {
        if (sourceStorage.type == StorageType::REGISTER) {
          RegDisp const dstRegDisp{getMemRegDisp(targetStorage)};
          as_.INSTR(MOVD_rf_rm32).setR(WasmABI::REGS::moveHelper).setR4RM(sourceStorage.location.reg)();
          as_.INSTR(MOVSD_rmf_rf).setR(WasmABI::REGS::moveHelper).setM4RM(dstRegDisp.reg, dstRegDisp.disp)();
        } else {
          // GCOVR_EXCL_START
          assert(sourceStorage.type == StorageType::CONSTANT);
          // GCOVR_EXCL_STOP
          emitMoveIntImpl(targetStorage, VariableStorage::i64Const(static_cast<uint64_t>(sourceStorage.location.constUnion.u32)), unconditional,
                          preserveFlags);
        }
      }
    }
  }
}

REG Backend::getUnderlyingRegIfSuitable(StackElement const *const element, MachineType const dstMachineType,
                                        RegMask const regMask) const VB_NOEXCEPT {
  if (element != nullptr) {
    VariableStorage const targetHintStorage{moduleInfo_.getStorage(*element)};
    bool typeMatch;
    if (targetHintStorage.machineType == dstMachineType) {
      typeMatch = true;
    } else if (MachineTypeUtil::isInt(targetHintStorage.machineType) && MachineTypeUtil::isInt(dstMachineType)) {
      typeMatch = true;
    } else {
      typeMatch = false;
    }
    if ((typeMatch && (targetHintStorage.type == StorageType::REGISTER)) && (!regMask.contains(targetHintStorage.location.reg))) {
      return targetHintStorage.location.reg;
    }
  }
  return REG::NONE;
}

bool Backend::hasEnoughScratchRegForScheduleInstruction(OPCode const opcode) const VB_NOEXCEPT {
  bool const isDivInt{opcodeIsDivInt(opcode)};
  bool const isLoadFloat{opcodeIsLoadFloat(opcode)};

  Span<REG const> allocableRegs{};

  if (isDivInt || !isLoadFloat) {
    allocableRegs =
        vb::Span<REG const>(WasmABI::gpr.data(), WasmABI::gpr.size()).subspan(static_cast<size_t>(moduleInfo_.getNumStaticallyAllocatedGPRs()));
  } else {
    allocableRegs =
        vb::Span<REG const>(WasmABI::fpr.data(), WasmABI::fpr.size()).subspan(static_cast<size_t>(moduleInfo_.getNumStaticallyAllocatedFPRs()));
  }

  uint32_t availableRegsCount{0U};
  for (REG const currentReg : allocableRegs) {
    Stack::iterator const referenceToLastOccurrence{moduleInfo_.getReferenceToLastOccurrenceOnStack(currentReg)};

    if (referenceToLastOccurrence.isEmpty()) {
      availableRegsCount++;
    }
  }
  return availableRegsCount > minimalNumRegsReservedForCondense;
}

void Backend::updateStackFrameSizeHelper(uint32_t const newAlignedStackFrameSize) {
  as_.setStackFrameSize(newAlignedStackFrameSize);

#if ACTIVE_STACK_OVERFLOW_CHECK
  if (moduleInfo_.currentState.checkedStackFrameSize < newAlignedStackFrameSize) {
    moduleInfo_.currentState.checkedStackFrameSize = newAlignedStackFrameSize;
    as_.checkStackFence();
  }
#endif
}

} // namespace x86_64
} // namespace vb
#endif
