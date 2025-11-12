///
/// @file aarch64_backend.cpp
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
// coverity[autosar_cpp14_a16_2_2_violation]
#include "src/config.hpp"
#ifdef JIT_TARGET_AARCH64
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

#include "aarch64_assembler.hpp"
#include "aarch64_backend.hpp"
#include "aarch64_cc.hpp"
#include "aarch64_encoding.hpp"

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
#include "src/core/compiler/backend/aarch64/aarch64_call_dispatch.hpp"
#include "src/core/compiler/backend/aarch64/aarch64_instruction.hpp"
#include "src/core/compiler/backend/aarch64/aarch64_relpatchobj.hpp"
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
#include "src/core/compiler/common/SafeInt.hpp"
#include "src/core/compiler/common/Stack.hpp"
#include "src/core/compiler/common/StackElement.hpp"
#include "src/core/compiler/common/StackType.hpp"
#include "src/core/compiler/common/VariableStorage.hpp"
#include "src/core/compiler/common/util.hpp"

namespace vb {
namespace aarch64 {
using Backend = AArch64_Backend;     ///< Shortcut for AArch64_Backend
using Assembler = AArch64_Assembler; ///< Shortcut for AArch64_Assembler

namespace BD = Basedata;    ///< shortcut of Basedata
namespace NABI = NativeABI; ///< shortcut of NativeABI

Backend::AArch64_Backend(Stack &stack, ModuleInfo &moduleInfo, MemWriter &memory, MemWriter &output, Common &common, Compiler &compiler) VB_NOEXCEPT
    : stack_{stack},
      moduleInfo_{moduleInfo},
      memory_{memory},
      output_{output},
      common_{common},
      compiler_{compiler},
      as_(*this, output, moduleInfo) {
}

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

REG Backend::allocateRegForGlobal(MachineType const type) VB_NOEXCEPT {
  assert(((moduleInfo_.fnc.numLocalsInGPR == 0) && (moduleInfo_.fnc.numLocalsInFPR == 0)) && "Cannot allocate globals after locals");
  assert(type != MachineType::INVALID);

  REG chosenReg{REG::NONE};
  assert(!compiler_.getDebugMode());

  if (MachineTypeUtil::isInt(type)) {
    chosenReg = WasmABI::gpr[moduleInfo_.numGlobalsInGPR];
    moduleInfo_.numGlobalsInGPR++;
  }

  return chosenReg;
}

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
  as_.checkStackFence(callScrRegs[0]); // SP change
#endif

  // Patch the function index in case this was an indirect call, we aren't sure, especially if tables are mutable at
  // some point so we do it unconditionally
  tryPatchFncIndexOfLastStacktraceEntry(moduleInfo_.fnc.index, callScrRegs[0], callScrRegs[1]);

  if (compiler_.getDebugMode()) {
    // Skip params for initialization, they are passed anyway
    for (uint32_t localIdx{moduleInfo_.fnc.numParams}; localIdx < moduleInfo_.fnc.numLocals; localIdx++) {
      StackElement const localElem{StackElement::local(localIdx)};
      VariableStorage const localStorage{moduleInfo_.getStorage(localElem)};
      emitMoveImpl(localStorage, VariableStorage::zero(moduleInfo_.localDefs[localIdx].type), false);
    }
  }
}

void Backend::emitNativeTrapAdapter() const {
  // NABI::gpParams[0] contains pointer to the start of the linear memory. Needed because this function is not called
  // from the native context NABI::gpParams[1] contains the TrapCode which we move to REGS::trapReg
  as_.INSTR(MOV_xD_xM_t).setD(WasmABI::REGS::linMem).setM(NABI::gpParams[0])();
  as_.INSTR(MOV_xD_xM_t).setD(WasmABI::REGS::trapReg).setM(NABI::gpParams[1])();
}

void Backend::emitStackTraceCollector(uint32_t const stacktraceRecordCount) const {
  // Load last frame ref pointer from job memory. This is definitely valid here
  as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
      .setT(StackTrace::frameRefReg)
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::lastFrameRefPtr>())();
  // Set targetReg to target buffer
  as_.MOVimm32(StackTrace::scratchReg, static_cast<uint32_t>(BD::FromEnd::getStacktraceArrayBase(stacktraceRecordCount)));
  as_.INSTR(SUB_xD_xN_xMolsImm6).setT(StackTrace::targetReg).setN(WasmABI::REGS::linMem).setM(StackTrace::scratchReg)();

  // Load number of stacktrace entries
  as_.MOVimm32(StackTrace::counterReg, stacktraceRecordCount);
  uint32_t const loopStartOffset{output_.size()};
  // Load function index to scratch reg and store in buffer
  as_.INSTR(LDR_wT_deref_xN_imm12zxls2_t).setT(StackTrace::scratchReg).setN(StackTrace::frameRefReg).setImm12zxls2(SafeUInt<14U>::fromConst<8U>())();
  as_.INSTR(STR_wT_deref_xN_unscSImm9_postidx).setT(StackTrace::scratchReg).setN(StackTrace::targetReg).setUnscSImm9(SafeInt<9>::fromConst<4>())();

  // Load next frame ref, compare to zero and break if it is zero (means first entry)
  as_.INSTR(LDR_xT_deref_xN_imm12zxls3_t).setT(StackTrace::frameRefReg).setN(StackTrace::frameRefReg).setImm12zxls3(SafeUInt<15U>::fromConst<0U>())();
  RelPatchObj const collectedAll{as_.prepareJMPIfRegIsZero(StackTrace::frameRefReg, true)};

  // Otherwise we decrement the counter and restart the loop if the counter is not zero yet
  as_.INSTR(SUBS_wD_wN_imm12zxols12).setD(StackTrace::counterReg).setN(StackTrace::counterReg).setImm12zx(SafeUInt<12U>::fromConst<1U>())();
  as_.prepareJMP(CC::NE).linkToBinaryPos(loopStartOffset);

  collectedAll.linkToHere();
}

void Backend::emitTrapHandler() const {
  // Restore stack pointer
  as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
      .setT(callScrRegs[0])
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9U>::fromConst<-Basedata::FromEnd::trapStackReentry>())();
  as_.INSTR(ADD_xD_xN_imm12zxols12).setD(REG::SP).setN(callScrRegs[0]).setImm12zx(SafeUInt<12U>::fromConst<0>())();

  // Load trapCodePtr into a register and store the trapCode there
  as_.INSTR(LDR_xT_deref_xN_imm12zxls3_t)
      .setT(callScrRegs[0])
      .setN(REG::SP)
      .setImm12zxls3(SafeUInt<15U>::fromConst<of_trapCodePtr_trapReentryPoint>())();
  as_.INSTR(STR_wT_deref_xN_imm12zxls2_t).setT(WasmABI::REGS::trapReg).setN(callScrRegs[0]).setImm12zxls2(SafeUInt<14U>::fromConst<0U>())();

  as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
      .setT(callScrRegs[0])
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9U>::fromConst<-Basedata::FromEnd::trapHandlerPtr>())();
  as_.INSTR(BR_xN_t).setN(callScrRegs[0])();
}

void Backend::emitFunctionEntryPoint(uint32_t const fncIndex) {
  assert(fncIndex < moduleInfo_.numTotalFunctions && "Function out of range");
  bool const imported{moduleInfo_.functionIsImported(fncIndex)};

  uint32_t currentFrameOffset{0U};

  // Reserve space on stack and spill non volatile registers
  as_.INSTR(SUB_xD_xN_imm12zxols12)
      .setD(REG::SP)
      .setN(REG::SP)
      .setImm12zx(SafeUInt<12U>::fromConst<static_cast<uint32_t>(NABI::nonvolRegs.size()) * 8U>())();
  static_assert(((NABI::nonvolRegs.size() * 8U) % 16U) == 0U, "Stack not aligned to 16B here");

#if ACTIVE_STACK_OVERFLOW_CHECK
  // Manual implementation because neither base pointer nor trap support is set up at this point
  as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
      .setT(callScrRegs[0])
      .setN(NABI::gpParams[1])
      .setUnscSImm9(SafeInt<9>::fromConst<-Basedata::FromEnd::stackFence>())();
  as_.INSTR(CMP_SP_xM_t).setM(callScrRegs[0])();
  RelPatchObj const inRange = as_.prepareJMP(CC::HS);

  // gpParams[2] contains the pointer to a variable where the TrapCode will be stored
  as_.MOVimm32(callScrRegs[0], static_cast<uint32_t>(TrapCode::STACKFENCEBREACHED));
  as_.INSTR(STR_wT_deref_xN_imm12zxls2_t).setT(callScrRegs[0]).setN(NABI::gpParams[2]).setImm12zxls2(SafeUInt<14U>::fromConst<0U>())();
  as_.INSTR(ADD_xD_xN_imm12zxols12)
      .setD(REG::SP)
      .setN(REG::SP)
      .setImm12zx(SafeUInt<12U>::fromConst<static_cast<uint32_t>(NABI::nonvolRegs.size()) * 8U>())();
  as_.INSTR(RET_xN_t).setN(REG::LR)();

  inRange.linkToHere();
#endif
  currentFrameOffset += (static_cast<uint32_t>(NABI::nonvolRegs.size()) * 8U);
  spillRestoreRegsRaw({NABI::nonvolRegs.data(), NABI::nonvolRegs.size()});

  // Nove pointer to serialized arguments from first argument and linMem register from second function argument to the
  // register where all the code will expect it to be
  as_.INSTR(MOV_xD_xM_t).setD(callScrRegs[2]).setM(NABI::gpParams[0])();
  as_.INSTR(MOV_xD_xM_t).setD(WasmABI::REGS::linMem).setM(NABI::gpParams[1])();

  setupJobMemRegFromLinMemReg();

#if LINEAR_MEMORY_BOUNDS_CHECKS
  setupMemSizeReg();
#endif

  //
  //

  common_.recoverGlobalsToRegs();

  // We are setting up the following stack structure from here on
  // When a trap is executed, we load the trapCode (uint32) into W0, then unwind the stack to the unwind target (which
  // is stored in link data), load the return address to X8 and BR to it which will not pop the return address off the
  // stack
  // RSP <------------ Stack growth direction (downwards) v <- unwind target
  // | &trapCode | Stacktrace Record + Debug Info | cachedJobMemoryPtrPtr | of_returnValuesPtr
  uint32_t const sigIndex{moduleInfo_.getFncSigIndex(fncIndex)};

  constexpr uint32_t of_stacktraceRecordAndDebugInfo{of_trapCodePtr_trapReentryPoint + 8U};
  constexpr uint32_t of_cachedJobMemoryPtrPtr{of_stacktraceRecordAndDebugInfo + Widths::stacktraceRecord + Widths::debugInfo};
  constexpr uint32_t of_returnValuesPtr{of_cachedJobMemoryPtrPtr + Widths::jobMemoryPtrPtr};
  constexpr uint32_t of_post{of_returnValuesPtr + 8U};
  constexpr uint32_t totalReserved{roundUpToPow2(of_post, 4U)};
  static_cast<void>(of_cachedJobMemoryPtrPtr);

  as_.INSTR(SUB_xD_xN_imm12zxols12).setD(REG::SP).setN(REG::SP).setImm12zx(SafeUInt<12U>::fromConst<totalReserved>())(); // SP small change
  currentFrameOffset += totalReserved;

  uint32_t const stackParamWidth{getStackParamWidth(sigIndex, imported)};
  uint32_t const stackReturnValueWidth{common_.getStackReturnValueWidth(sigIndex)};
  uint32_t const padding{deltaToNextPow2(currentFrameOffset + stackParamWidth + stackReturnValueWidth, 4U)};
  uint32_t const reservationFunctionCall{stackParamWidth + stackReturnValueWidth + padding};

  uint32_t const offsetToStartOfFrame{imported ? 0U : (padding + of_stacktraceRecordAndDebugInfo)};
  constexpr uint32_t bytecodePos{0U}; // Zero because we are in a wrapper/helper here, not an actual function body described by Wasm
  tryPushStacktraceAndDebugEntry(fncIndex, SafeUInt<12U>::fromConst<of_stacktraceRecordAndDebugInfo>(), offsetToStartOfFrame, bytecodePos,
                                 callScrRegs[0], callScrRegs[1], NABI::gpParams[0]);
#if LINEAR_MEMORY_BOUNDS_CHECKS
  if (imported) {
    cacheJobMemoryPtrPtr(of_cachedJobMemoryPtrPtr, callScrRegs[0]);
  }
#endif

  // gpParams[2] contains the pointer to a variable where the TrapCode will be stored
  as_.INSTR(STR_xT_deref_xN_imm12zxls3_t)
      .setT(NABI::gpParams[2])
      .setN(REG::SP)
      .setImm12zxls3(SafeUInt<15U>::fromConst<of_trapCodePtr_trapReentryPoint>())();

  // gpParams[3] contains the pointer to an area where the returnValues will be stored
  as_.INSTR(STR_xT_deref_xN_imm12zxls3_t).setT(NABI::gpParams[3]).setN(REG::SP).setImm12zxls3(SafeUInt<15U>::fromConst<of_returnValuesPtr>())();

  // If saved stack pointer is not zero, this runtime already has an active frame and is already executing
  as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
      .setT(callScrRegs[1])
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9U>::fromConst<-BD::FromEnd::trapStackReentry>())();
  as_.INSTR(CMP_xN_xM).setN(callScrRegs[1]).setM(REG::ZR)();
  RelPatchObj const alreadyExecuting{as_.prepareJMP(CC::NE)};

  //
  //
  // NOT ALREADY EXECUTING START
  //
  //

  // Store unwind target to link data if this is the first frame
  as_.INSTR(ADD_xD_xN_imm12zxols12).setD(callScrRegs[0]).setN(REG::SP).setImm12zx(SafeUInt<12U>::fromConst<0U>())(); // mov x2, sp
  as_.INSTR(STUR_xT_deref_xN_unscSImm9_t)
      .setT(callScrRegs[0])
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9U>::fromConst<-BD::FromEnd::trapStackReentry>())();

  // Load instruction pointer of trap reentry instruction pointer and store it in job memory
  RelPatchObj const trapEntryAdr{as_.prepareADR(callScrRegs[0])};
  as_.INSTR(STUR_xT_deref_xN_unscSImm9_t)
      .setT(callScrRegs[0])
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9U>::fromConst<-BD::FromEnd::trapHandlerPtr>())();

// If it is enabled, store the native stack fence
#if MAX_WASM_STACKSIZE_BEFORE_NATIVE_CALL != 0 && !ACTIVE_STACK_OVERFLOW_CHECK
  // Subtract constant from SP and store it in link data
  as_.MOVimm32(callScrRegs[0], MAX_WASM_STACKSIZE_BEFORE_NATIVE_CALL);
  as_.INSTR(SUB_xD_SP_xM_t).setD(callScrRegs[0]).setM(callScrRegs[0])();
  as_.INSTR(STUR_xT_deref_xN_unscSImm9_t)
      .setT(callScrRegs[0])
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9U>::fromConst<-BD::FromEnd::nativeStackFence>())();
#elif STACKSIZE_LEFT_BEFORE_NATIVE_CALL != 0 && ACTIVE_STACK_OVERFLOW_CHECK
  as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
      .setT(callScrRegs[0])
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9U>::fromConst<-Basedata::FromEnd::stackFence>())();
  as_.addImmToReg(callScrRegs[0], STACKSIZE_LEFT_BEFORE_NATIVE_CALL, true, RegMask::none(), callScrRegs[1]);
  // Overflow check is performed in Runtime::setStackFence()
  as_.INSTR(STUR_xT_deref_xN_unscSImm9_t)
      .setT(callScrRegs[0])
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9U>::fromConst<-BD::FromEnd::nativeStackFence>())();
#endif

  //
  //
  // NOT ALREADY EXECUTING START
  //
  //

  alreadyExecuting.linkToHere();

  // Check limits for addImm24ToReg
  static_assert(roundUpToPow2(ImplementationLimits::numParams * 8U, 4U) <= 0xFF'FF'FFU, "Too many arguments");
  as_.addImm24ToReg(REG::SP, -static_cast<int32_t>(reservationFunctionCall), true);
#if ACTIVE_STACK_OVERFLOW_CHECK
  as_.checkStackFence(callScrRegs[0]); // SP change
#endif
  currentFrameOffset += reservationFunctionCall;
  assert(((currentFrameOffset % 16U) == 0U) && "Stack before call not aligned to 16B boundary");

  // Check if the limits are good for LDR_wT_deref_xN_scUimm12
  static_assert((ImplementationLimits::numParams * 8U) <= (0xFFFU * 4U), "Too many arguments");

  // Load arguments from serialization buffer to registers and stack according to Wasm and native ABI, respectively
  RegStackTracker tracker{};
  uint32_t serOffset{0U};
  moduleInfo_.iterateParamsForSignature(
      sigIndex, FunctionRef<void(MachineType)>([this, imported, &tracker, &serOffset, stackParamWidth](MachineType const paramType) {
        bool const is64{MachineTypeUtil::is64(paramType)};
        REG const targetReg{getREGForArg(paramType, imported, tracker)};
        if (targetReg != REG::NONE) {
          if (is64) {
            as_.INSTR(LDR_T_deref_N_scUImm12(RegUtil::isGPR(targetReg), true))
                .setT(targetReg)
                .setN(callScrRegs[2])
                .setImm12zxls3(SafeUInt<15U>::fromUnsafe(serOffset))();
          } else {
            as_.INSTR(LDR_T_deref_N_scUImm12(RegUtil::isGPR(targetReg), false))
                .setT(targetReg)
                .setN(callScrRegs[2])
                .setImm12zxls2(SafeUInt<14U>::fromUnsafe(serOffset))();
          }

        } else {
          uint32_t const offsetInArgs{offsetInStackArgs(imported, stackParamWidth, tracker, paramType)};
          if (is64) {
            as_.INSTR(LDR_xT_deref_xN_imm12zxls3_t).setT(callScrRegs[0]).setN(callScrRegs[2]).setImm12zxls3(SafeUInt<15U>::fromUnsafe(serOffset))();
            as_.INSTR(STR_xT_deref_xN_imm12zxls3_t).setT(callScrRegs[0]).setN(REG::SP).setImm12zxls3(SafeUInt<15U>::fromUnsafe(offsetInArgs))();
          } else {
            as_.INSTR(LDR_wT_deref_xN_imm12zxls2_t).setT(callScrRegs[0]).setN(callScrRegs[2]).setImm12zxls2(SafeUInt<14U>::fromUnsafe(serOffset))();
            as_.INSTR(STR_wT_deref_xN_imm12zxls2_t).setT(callScrRegs[0]).setN(REG::SP).setImm12zxls2(SafeUInt<14U>::fromUnsafe(offsetInArgs))();
          }
        }
        serOffset += 8U;
      }));

  if (imported) {
    REG const targetReg{getREGForArg(MachineType::I64, true, tracker)};
    if (targetReg != REG::NONE) {
      as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
          .setT(targetReg)
          .setN(WasmABI::REGS::linMem)
          .setUnscSImm9(SafeInt<9U>::fromConst<-Basedata::FromEnd::customCtxOffset>())();
    } else {
      uint32_t const offsetInArgs{offsetInStackArgs(imported, stackParamWidth, tracker, MachineType::I64)};
      as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
          .setT(callScrRegs[0])
          .setN(WasmABI::REGS::linMem)
          .setUnscSImm9(SafeInt<9U>::fromConst<-Basedata::FromEnd::customCtxOffset>())();
      as_.INSTR(STR_xT_deref_xN_imm12zxls3_t).setT(callScrRegs[0]).setN(REG::SP).setImm12zxls3(SafeUInt<15U>::fromUnsafe(offsetInArgs))();
    }
  }
  assert(tracker.allocatedStackBytes == stackParamWidth && "Stack allocation size mismatch");

  // Check whether we are dealing with a builtin function
  if (moduleInfo_.functionIsBuiltin(fncIndex)) {
    throw FeatureNotSupportedException(ErrorCode::Cannot_export_builtin_function);
  }

  emitRawFunctionCall(fncIndex);

  uint32_t index{0U};
  RegStackTracker returnValueTracker{};
  as_.INSTR(LDR_xT_deref_xN_imm12zxls3_t)
      .setT(callScrRegs[1])
      .setN(REG::SP)
      .setImm12zxls3(SafeUInt<15U>::fromUnsafe(of_returnValuesPtr + reservationFunctionCall))();
  moduleInfo_.iterateResultsForSignature(
      sigIndex, FunctionRef<void(MachineType)>([this, stackParamWidth, &index, &returnValueTracker](MachineType const machineType) {
        bool const is64{MachineTypeUtil::is64(machineType)};
        bool const isInt{MachineTypeUtil::isInt(machineType)};
        REG const srcReg{getREGForReturnValue(machineType, returnValueTracker)};
        uint32_t const returnValueDisp{index * 8U};
        if (srcReg != REG::NONE) {
          if (is64) {
            as_.INSTR(STR_T_deref_N_scUImm12(isInt, true))
                .setT(srcReg)
                .setN(callScrRegs[1])
                .setImm12zxls3(SafeUInt<15U>::fromUnsafe(returnValueDisp))();
          } else {
            as_.INSTR(STR_T_deref_N_scUImm12(isInt, false))
                .setT(srcReg)
                .setN(callScrRegs[1])
                .setImm12zxls2(SafeUInt<14U>::fromUnsafe(returnValueDisp))();
          }
        } else {
          uint32_t const offsetFromSP{stackParamWidth + offsetInStackReturnValues(returnValueTracker, machineType)};
          if (is64) {
            as_.INSTR(LDR_xT_deref_xN_imm12zxls3_t).setT(callScrRegs[0]).setN(REG::SP).setImm12zxls3(SafeUInt<15U>::fromUnsafe(offsetFromSP))();
            as_.INSTR(STR_xT_deref_xN_imm12zxls3_t)
                .setT(callScrRegs[0])
                .setN(callScrRegs[1])
                .setImm12zxls3(SafeUInt<15U>::fromUnsafe(returnValueDisp))();
          } else {
            as_.INSTR(LDR_wT_deref_xN_imm12zxls2_t).setT(callScrRegs[0]).setN(REG::SP).setImm12zxls2(SafeUInt<14U>::fromUnsafe(offsetFromSP))();
            as_.INSTR(STR_wT_deref_xN_imm12zxls2_t)
                .setT(callScrRegs[0])
                .setN(callScrRegs[1])
                .setImm12zxls2(SafeUInt<14U>::fromUnsafe(returnValueDisp))();
          }
        }
        index++;
      }));

  // Remove function arguments again
  as_.addImm24ToReg(REG::SP, static_cast<int32_t>(reservationFunctionCall), true);
  currentFrameOffset -= reservationFunctionCall;

#if LINEAR_MEMORY_BOUNDS_CHECKS
  if (imported) {
    restoreFromJobMemoryPtrPtr(of_cachedJobMemoryPtrPtr);
  }
#endif
  tryPopStacktraceAndDebugEntry(of_stacktraceRecordAndDebugInfo, callScrRegs[0]);

  trapEntryAdr.linkToHere();

  common_.moveGlobalsToLinkData();

  // Load potential unwind target so we can identify whether this was the first frame in the call sequence
  as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
      .setT(callScrRegs[1])
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::trapStackReentry>())();

  // Compare the trap unwind target to the current stack pointer
  as_.INSTR(CMP_SP_xM_t).setM(callScrRegs[1])();
  // If this is equal, we can conclude this was the first frame in the call sequence and subsequently reset the stored
  // trap target
  RelPatchObj const notFirstWasmFrame{as_.prepareJMP(CC::NE)};
  as_.INSTR(STUR_xT_deref_xN_unscSImm9_t)
      .setT(REG::ZR)
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::trapStackReentry>())(); // Reset trap target
  as_.INSTR(STUR_xT_deref_xN_unscSImm9_t)
      .setT(REG::ZR)
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::trapHandlerPtr>())(); // Reset trap target
  notFirstWasmFrame.linkToHere();

  // Remove trap stack identifier and potentially stacktrace entry (or padding)
  as_.INSTR(ADD_xD_xN_imm12zxols12).setD(REG::SP).setN(REG::SP).setImm12zx(SafeUInt<12>::fromConst<totalReserved>())();
  currentFrameOffset -= totalReserved;

  //
  //

  // Restore spilled registers and unwind stack
  spillRestoreRegsRaw({NABI::nonvolRegs.data(), NABI::nonvolRegs.size()}, true);
  as_.addImm24ToReg(REG::SP, static_cast<int32_t>(NABI::nonvolRegs.size()) * 8, true);
  currentFrameOffset -= static_cast<uint32_t>(NABI::nonvolRegs.size()) * 8U;
  static_cast<void>(currentFrameOffset);
  assert(currentFrameOffset == 0 && "Unaligned stack at end of wrapper call");
  as_.INSTR(RET_xN_t).setN(REG::LR)();
}

#if LINEAR_MEMORY_BOUNDS_CHECKS
void Backend::cacheJobMemoryPtrPtr(uint32_t const spOffset, REG const scrReg) const {
  static_assert(Widths::jobMemoryPtrPtr == 8U, "Cached job memory width not suitable");
  assert((spOffset <= 8U * ((1U << 12U) - 1U)) && "spOffset too large");
  // Store cached jobMemoryPtrPtr
  as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
      .setT(scrReg)
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::jobMemoryDataPtrPtr>())();
  as_.INSTR(STR_xT_deref_xN_imm12zxls3_t).setT(scrReg).setN(REG::SP).setImm12zxls3(SafeUInt<15U>::fromUnsafe(spOffset))();
}

void Backend::restoreFromJobMemoryPtrPtr(uint32_t const spOffset) const {
  assert((spOffset <= 8U * ((1U << 12U) - 1U)) && "spOffset too large");
  // Restore cached jobMemoryPtrPtr and dereference
  as_.INSTR(LDR_xT_deref_xN_imm12zxls3_t).setT(WasmABI::REGS::jobMem).setN(REG::SP).setImm12zxls3(SafeUInt<15U>::fromUnsafe(spOffset))();
  as_.INSTR(LDR_xT_deref_xN_imm12zxls3_t).setT(WasmABI::REGS::jobMem).setN(WasmABI::REGS::jobMem).setImm12zxls3(SafeUInt<15U>::fromConst<0U>())();

  setupLinMemRegFromJobMemReg();
}
#endif

#if ENABLE_EXTENSIONS
void Backend::updateRegPressureHistogram(bool const isGPR) const VB_NOEXCEPT {
  auto eval = [this](uint32_t numStaticallyAllocatedRegs, vb::Span<REG const> const &span) VB_NOEXCEPT -> uint32_t {
    // Start at numStaticallyAllocatedRegs so we do not iterate registers with locals
    uint32_t freeScratchRegCount = 0U;
    for (uint32_t regPos = numStaticallyAllocatedRegs; regPos < static_cast<uint32_t>(span.size()); regPos++) {
      REG const currentReg = span[regPos];
      Stack::iterator const referenceToLastOccurrence = moduleInfo_.getReferenceToLastOccurrenceOnStack(currentReg);
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
  bool const isInt{MachineTypeUtil::isInt(type)};

#if ENABLE_EXTENSIONS
  if (compiler_.getAnalytics() != nullptr) {
    updateRegPressureHistogram(isInt);
  }
#endif

  REG chosenReg{REG::NONE};
  bool isUsed{false};

  // Number of actual allocated locals for that register type, the length
  // (number) of allocatable register array for that type and pointer to the correct array (GPR or FPR)
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
  default: {
    UNREACHABLE(break, "Unknown MachineType");
  }
    // GCOVR_EXCL_STOP
  }
}

void Backend::emitMoveIntImpl(VariableStorage const &dstStorage, VariableStorage const &srcStorage, bool const unconditional,
                              bool const presFlags) const {
  static_cast<void>(presFlags);
  // GCOVR_EXCL_START
  assert(dstStorage.type != StorageType::CONSTANT && dstStorage.type != StorageType::INVALID && srcStorage.type != StorageType::INVALID &&
         "Invalid source or destination for emitMove");
  assert(MachineTypeUtil::isInt(srcStorage.machineType));
  assert((dstStorage.machineType == srcStorage.machineType) && "WasmTypes of source and destination must match for emitMoveIntImpl");
  // GCOVR_EXCL_STOP

  if ((!unconditional) && dstStorage.equals(srcStorage)) {
    return;
  }
  bool const is64{MachineTypeUtil::is64(dstStorage.machineType)};

  TempRegManager tempRegManager{*this};

  if (dstStorage.type == StorageType::REGISTER) { // X -> REGISTER
    REG const dstReg{dstStorage.location.reg};
    if (srcStorage.type == StorageType::CONSTANT) { // CONSTANT -> REGISTER
      as_.MOVimm(is64, dstReg, is64 ? srcStorage.location.constUnion.u64 : srcStorage.location.constUnion.u32);
    } else if (srcStorage.type == StorageType::REGISTER) { // REGISTER -> REGISTER
      as_.INSTR(is64 ? MOV_xD_xM_t : MOV_wD_wM_t).setD(dstReg).setM(srcStorage.location.reg)();
    } else { // MEMORY -> REGISTER
      if (is64) {
        RegDisp<15U> const srcRegDisp{getMemRegDisp<15U>(srcStorage, tempRegManager)};
        as_.INSTR(LDR_xT_deref_xN_imm12zxls3_t).setT(dstReg).setN(srcRegDisp.reg).setImm12zxls3(srcRegDisp.disp)();
      } else {
        RegDisp<14U> const srcRegDisp{getMemRegDisp<14U>(srcStorage, tempRegManager)};
        as_.INSTR(LDR_wT_deref_xN_imm12zxls2_t).setT(dstReg).setN(srcRegDisp.reg).setImm12zxls2(srcRegDisp.disp)();
      }
    }
  } else { // X -> MEMORY

    if (srcStorage.type == StorageType::CONSTANT) { // CONSTANT -> MEMORY
      uint64_t const constValue{is64 ? srcStorage.location.constUnion.u64 : srcStorage.location.constUnion.u32};
      REG intermReg{(constValue == 0U) ? REG::ZR : REG::NONE};
      if (intermReg == REG::NONE) {
        intermReg = tempRegManager.getTempGPR();
        as_.MOVimm(is64, intermReg, constValue);
      }
      if (is64) {
        Backend::RegDisp<15U> const dstRegDisp{getMemRegDisp<15U>(dstStorage, tempRegManager)};
        as_.INSTR(STR_xT_deref_xN_imm12zxls3_t).setT(intermReg).setN(dstRegDisp.reg).setImm12zxls3(dstRegDisp.disp)();
      } else {
        Backend::RegDisp<14U> const dstRegDisp{getMemRegDisp<14U>(dstStorage, tempRegManager)};
        as_.INSTR(STR_wT_deref_xN_imm12zxls2_t).setT(intermReg).setN(dstRegDisp.reg).setImm12zxls2(dstRegDisp.disp)();
      }

    } else if (srcStorage.type == StorageType::REGISTER) { // REGISTER -> MEMORY
      REG const srcReg{srcStorage.location.reg};
      if (is64) {
        Backend::RegDisp<15U> const dstRegDisp{getMemRegDisp<15U>(dstStorage, tempRegManager)};
        as_.INSTR(STR_xT_deref_xN_imm12zxls3_t).setT(srcReg).setN(dstRegDisp.reg).setImm12zxls3(dstRegDisp.disp)();
      } else {
        Backend::RegDisp<14U> const dstRegDisp{getMemRegDisp<14U>(dstStorage, tempRegManager)};
        as_.INSTR(STR_wT_deref_xN_imm12zxls2_t).setT(srcReg).setN(dstRegDisp.reg).setImm12zxls2(dstRegDisp.disp)();
      }

    } else { // MEMORY -> MEMORY
      if (is64) {
        Backend::RegDisp<15U> const srcRegDisp{getMemRegDisp<15U>(srcStorage, tempRegManager)};
        Backend::RegDisp<15U> const dstRegDisp{getMemRegDisp<15U>(dstStorage, tempRegManager)};
        as_.INSTR(LDR_dT_deref_xN_imm12zxls3_t).setT(WasmABI::REGS::moveHelper).setN(srcRegDisp.reg).setImm12zxls3(srcRegDisp.disp)();
        as_.INSTR(STR_dT_deref_xN_imm12zxls3_t).setT(WasmABI::REGS::moveHelper).setN(dstRegDisp.reg).setImm12zxls3(dstRegDisp.disp)();
      } else {
        Backend::RegDisp<14U> const srcRegDisp{getMemRegDisp<14U>(srcStorage, tempRegManager)};
        Backend::RegDisp<14U> const dstRegDisp{getMemRegDisp<14U>(dstStorage, tempRegManager)};
        as_.INSTR(LDR_sT_deref_xN_imm12zxls2_t).setT(WasmABI::REGS::moveHelper).setN(srcRegDisp.reg).setImm12zxls2(srcRegDisp.disp)();
        as_.INSTR(STR_sT_deref_xN_imm12zxls2_t).setT(WasmABI::REGS::moveHelper).setN(dstRegDisp.reg).setImm12zxls2(dstRegDisp.disp)();
      }
    }
  }
  tempRegManager.recoverTempGPRs();
}

void Backend::emitMoveFloatImpl(VariableStorage const &dstStorage, VariableStorage const &srcStorage, bool const unconditional,
                                bool const presFlags) const {
  static_cast<void>(presFlags);
  assert(dstStorage.type != StorageType::CONSTANT && dstStorage.type != StorageType::INVALID && srcStorage.type != StorageType::INVALID &&
         "Invalid source or destination for emitMove");
  assert(dstStorage.machineType == srcStorage.machineType);
  assert(!MachineTypeUtil::isInt(dstStorage.machineType));

  if ((!unconditional) && dstStorage.equals(srcStorage)) {
    return;
  }
  bool const is64{MachineTypeUtil::is64(dstStorage.machineType)};

  TempRegManager tempRegManager{*this};

  if (dstStorage.type == StorageType::REGISTER) { // X -> REGISTER
    REG const dstReg{dstStorage.location.reg};
    if (srcStorage.type == StorageType::CONSTANT) { // CONSTANT -> REGISTER
      uint64_t const rawConstValue{is64 ? bit_cast<uint64_t>(srcStorage.location.constUnion.f64)
                                        : bit_cast<uint32_t>(srcStorage.location.constUnion.f32)};
      // Try mov as immediate
      bool const immMovSuccess{as_.FMOVimm(is64, dstReg, rawConstValue)};
      if (!immMovSuccess) {
        REG intermReg{(rawConstValue == 0U) ? REG::ZR : REG::NONE};
        if (intermReg == REG::NONE) {
          intermReg = tempRegManager.getTempGPR();
          as_.MOVimm(is64, intermReg, rawConstValue);
        }
        as_.INSTR(is64 ? FMOV_dD_xN : FMOV_sD_wN).setD(dstReg).setN(intermReg)();
      }
    } else if (srcStorage.type == StorageType::REGISTER) { // REGISTER -> REGISTER
      REG const srcReg{srcStorage.location.reg};
      as_.INSTR(is64 ? FMOV_dD_dN : FMOV_sD_sN).setD(dstReg).setN(srcReg)();
    } else { // MEMORY -> REGISTER
      if (is64) {
        RegDisp<15U> const srcRegDisp{getMemRegDisp<15U>(srcStorage, tempRegManager)};
        as_.INSTR(LDR_dT_deref_xN_imm12zxls3_t).setT(dstReg).setN(srcRegDisp.reg).setImm12zxls3(srcRegDisp.disp)();
      } else {
        RegDisp<14U> const srcRegDisp{getMemRegDisp<14U>(srcStorage, tempRegManager)};
        as_.INSTR(LDR_sT_deref_xN_imm12zxls2_t).setT(dstReg).setN(srcRegDisp.reg).setImm12zxls2(srcRegDisp.disp)();
      }
    }
  } else {
    if (srcStorage.type == StorageType::CONSTANT) { // CONSTANT -> MEMORY
      uint64_t const rawConstValue{is64 ? bit_cast<uint64_t>(srcStorage.location.constUnion.f64)
                                        : bit_cast<uint32_t>(srcStorage.location.constUnion.f32)};
      REG intermReg{(rawConstValue == 0U) ? REG::ZR : REG::NONE};
      if (intermReg == REG::NONE) {
        intermReg = tempRegManager.getTempGPR();
        as_.MOVimm(is64, intermReg, rawConstValue);
      }
      if (is64) {
        RegDisp<15> const dstRegDisp{getMemRegDisp<15>(dstStorage, tempRegManager)};
        as_.INSTR(STR_xT_deref_xN_imm12zxls3_t).setT(intermReg).setN(dstRegDisp.reg).setImm12zxls3(dstRegDisp.disp)();
      } else {
        RegDisp<14> const dstRegDisp{getMemRegDisp<14>(dstStorage, tempRegManager)};
        as_.INSTR(STR_wT_deref_xN_imm12zxls2_t).setT(intermReg).setN(dstRegDisp.reg).setImm12zxls2(dstRegDisp.disp)();
      }

    } else if (srcStorage.type == StorageType::REGISTER) { // REGISTER -> MEMORY
      REG const srcReg{srcStorage.location.reg};
      if (is64) {
        RegDisp<15> const dstRegDisp{getMemRegDisp<15>(dstStorage, tempRegManager)};
        as_.INSTR(STR_T_deref_N_scUImm12(false, true)).setT(srcReg).setN(dstRegDisp.reg).setImm12zxls3(dstRegDisp.disp)();
      } else {
        RegDisp<14> const dstRegDisp{getMemRegDisp<14>(dstStorage, tempRegManager)};
        as_.INSTR(STR_T_deref_N_scUImm12(false, false)).setT(srcReg).setN(dstRegDisp.reg).setImm12zxls2(dstRegDisp.disp)();
      }

    } else { // MEMORY -> MEMORY
      if (is64) {
        RegDisp<15U> const srcRegDisp{getMemRegDisp<15U>(srcStorage, tempRegManager)};
        RegDisp<15> const dstRegDisp{getMemRegDisp<15>(dstStorage, tempRegManager)};
        as_.INSTR(LDR_dT_deref_xN_imm12zxls3_t).setT(WasmABI::REGS::moveHelper).setN(srcRegDisp.reg).setImm12zxls3(srcRegDisp.disp)();
        as_.INSTR(STR_T_deref_N_scUImm12(false, true)).setT(WasmABI::REGS::moveHelper).setN(dstRegDisp.reg).setImm12zxls3(dstRegDisp.disp)();
      } else {
        RegDisp<14U> const srcRegDisp{getMemRegDisp<14U>(srcStorage, tempRegManager)};
        RegDisp<14> const dstRegDisp{getMemRegDisp<14>(dstStorage, tempRegManager)};
        as_.INSTR(LDR_sT_deref_xN_imm12zxls2_t).setT(WasmABI::REGS::moveHelper).setN(srcRegDisp.reg).setImm12zxls2(srcRegDisp.disp)();
        as_.INSTR(STR_T_deref_N_scUImm12(false, false)).setT(WasmABI::REGS::moveHelper).setN(dstRegDisp.reg).setImm12zxls2(dstRegDisp.disp)();
      }
    }
  }
  tempRegManager.recoverTempGPRs();
}

// Requests a target
StackElement Backend::reqSpillTarget(StackElement const &source, RegMask const protRegs, bool const forceToStack, bool const presFlags) {
  static_cast<void>(presFlags);

  MachineType const type{moduleInfo_.getMachineType(&source)};
  RegAllocTracker tempRegAllocTracker{};
  tempRegAllocTracker.writeProtRegs = protRegs;
  if (!forceToStack) {
    // May fail
    REG const reg{common_.reqFreeScratchRegProt(type, tempRegAllocTracker)};
    if (reg != REG::NONE) {
      return StackElement::scratchReg(reg, MachineTypeUtil::toStackTypeFlag(type));
    }
  }

  uint32_t const newOffset{common_.findFreeTempStackSlot(StackElement::tempStackSlotSize)};
  assert(newOffset <= moduleInfo_.fnc.stackFrameSize + StackElement::tempStackSlotSize);
  if (newOffset > moduleInfo_.fnc.stackFrameSize) {
    uint32_t const newAlignedStackFrameSize{as_.alignStackFrameSize(newOffset + 32U)};
    as_.setStackFrameSize(newAlignedStackFrameSize);

#if ACTIVE_STACK_OVERFLOW_CHECK
    if (moduleInfo_.currentState.checkedStackFrameSize < newAlignedStackFrameSize) {
      moduleInfo_.currentState.checkedStackFrameSize = newAlignedStackFrameSize;
      REG scratchReg = common_.reqFreeScratchRegProt(MachineType::I32, tempRegAllocTracker);
      bool const haveFreeRegister = scratchReg != REG::NONE;

      static_assert(BD::FromEnd::spillSize >= 8, "Spill region not large enough");
      if (!haveFreeRegister) {
        as_.INSTR(STUR_xT_deref_xN_unscSImm9_t)
            .setT(callScrRegs[0])
            .setN(WasmABI::REGS::linMem)
            .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::spillRegion>())();
        scratchReg = callScrRegs[0];
      }

      if (!presFlags) {
        as_.checkStackFence(scratchReg); // SP change
      } else {
        REG flagStorageReg = common_.reqFreeScratchRegProt(MachineType::I64, tempRegAllocTracker);
        bool const haveFreeFlagRegister = flagStorageReg != REG::NONE;
        if (!haveFreeFlagRegister) {
          as_.INSTR(STUR_xT_deref_xN_unscSImm9_t)
              .setT(callScrRegs[1])
              .setN(WasmABI::REGS::linMem)
              .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::spillRegion + 8>())();
          flagStorageReg = callScrRegs[1];
        }

        // Store the CPU flags because they will be clobbered by checkStackFence
        as_.INSTR(MRS_xT_NZCV).setT(flagStorageReg)();

        as_.checkStackFence(scratchReg); // SP change

        // Restore the CPU flags
        as_.INSTR(MSR_NZCV_xT).setT(flagStorageReg)();

        if (!haveFreeFlagRegister) {
          as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
              .setT(callScrRegs[1])
              .setN(WasmABI::REGS::linMem)
              .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::spillRegion + 8>())();
        }
      }

      if (!haveFreeRegister) {
        as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
            .setT(callScrRegs[0])
            .setN(WasmABI::REGS::linMem)
            .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::spillRegion>())();
      }
    }
#endif
  }
  StackElement const tempStackElement{
      StackElement::tempResult(type, VariableStorage::stackMemory(type, newOffset), moduleInfo_.getStackMemoryReferencePosition())};
  return tempStackElement;
}

void Backend::tryPushStacktraceAndDebugEntry(uint32_t const fncIndex, SafeUInt<12U> const storeOffsetFromSP, uint32_t const offsetToStartOfFrame,
                                             uint32_t const bytecodePosOfLastParsedInstruction, REG const scratchReg, REG const scratchReg2,
                                             REG const scratchReg3) const {
  static_assert(Widths::stacktraceRecord == 16U, "Stacktrace record width not suitable");
  static_assert(Widths::debugInfo == 8U, "Debug info width not suitable");
  if (!compiler_.shallRecordStacktrace()) {
    return;
  }

  // Calculate new frame ref pointer (SP + spOffset)
  as_.INSTR(ADD_xD_xN_imm12zxols12).setD(scratchReg3).setN(REG::SP).setImm12zx(storeOffsetFromSP)();

  //
  // STACKTRACE
  //
  // Load old frame ref pointer from job memory, and function index into a register
  as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
      .setT(scratchReg)
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::lastFrameRefPtr>())();
  // Don't write if it's an unknown index. In that case it will be patched later anyway
  if (fncIndex != UnknownIndex) {
    as_.MOVimm32(scratchReg2, fncIndex);
  }
  // Store both to stack, STP stores first register on the lower address
  as_.INSTR(STP_xT1_xT2_deref_xN_scSImm7_t).setT1(scratchReg).setT2(scratchReg2).setN(scratchReg3).setSImm7ls3(SafeInt<10>::fromConst<0>())();

  //
  // DEBUG
  //
  if (compiler_.getDebugMode()) {
    // Load offset to start of frame on stack and position of call instruction and store to stack
    as_.MOVimm32(scratchReg, offsetToStartOfFrame);
    as_.MOVimm32(scratchReg2, bytecodePosOfLastParsedInstruction);
    as_.INSTR(STP_wT1_wT2_deref_xN_scSImm7_t).setT1(scratchReg).setT2(scratchReg2).setN(scratchReg3).setSImm7ls2(SafeInt<9>::fromConst<12>())();
  }

  // Store to job memory last so everything else is on the stack in case we are running into a stack overflow here ->
  // then the ref should point to the last one)
  as_.INSTR(STUR_xT_deref_xN_unscSImm9_t)
      .setT(scratchReg3)
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::lastFrameRefPtr>())();
}

void Backend::tryPopStacktraceAndDebugEntry(uint32_t const storeOffsetFromSP, REG const scratchReg) const {
  if (!compiler_.shallRecordStacktrace()) {
    return;
  }

  // Load previous frame ref ptr and store to job memory
  as_.INSTR(LDR_xT_deref_xN_imm12zxls3_t).setT(scratchReg).setN(REG::SP).setImm12zxls3(SafeUInt<15U>::fromUnsafe(storeOffsetFromSP))();
  as_.INSTR(STUR_xT_deref_xN_unscSImm9_t)
      .setT(scratchReg)
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::lastFrameRefPtr>())();
}

void Backend::tryPatchFncIndexOfLastStacktraceEntry(uint32_t const fncIndex, REG const scratchReg, REG const scratchReg2) const {
  if (!compiler_.shallRecordStacktrace()) {
    return;
  }

  // Load old frame ref pointer from job memory
  as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
      .setT(scratchReg)
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::lastFrameRefPtr>())();

  // Store function index to last entry
  as_.MOVimm32(scratchReg2, fncIndex);
  as_.INSTR(STR_wT_deref_xN_imm12zxls2_t).setT(scratchReg2).setN(scratchReg).setImm12zxls2(SafeUInt<14U>::fromConst<8U>())();
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
void Backend::checkForInterruptionRequest(REG const scrReg) const {
  as_.INSTR(LDURB_wT_deref_xN_unscSImm9_t)
      .setT(scrReg)
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::statusFlags>())();

  RelPatchObj const statusFlagIsZero{as_.prepareJMPIfRegIsZero(scrReg, false)};
  // Retrieve the trapCode from the actual flag
  if (scrReg != WasmABI::REGS::trapReg) {
    as_.INSTR(MOV_wD_wM_t).setD(WasmABI::REGS::trapReg).setM(scrReg)();
  }
  as_.TRAP(TrapCode::NONE);
  statusFlagIsZero.linkToHere();
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

uint32_t Backend::getStackParamWidth(uint32_t const sigIndex, bool const imported) const VB_NOEXCEPT {
  RegStackTracker tracker{};
  uint32_t stackParamWidth{0U};
  moduleInfo_.iterateParamsForSignature(
      sigIndex, FunctionRef<void(MachineType)>([this, imported, &tracker, &stackParamWidth](MachineType const paramType) VB_NOEXCEPT {
        REG const targetReg{getREGForArg(paramType, imported, tracker)};
        if (targetReg == REG::NONE) {
#ifdef __APPLE__
          if (imported) {
            // Properly align
            stackParamWidth = roundUpToPow2(stackParamWidth, log2I32(widthInStackArgs(imported, paramType)));
          }
#endif
          stackParamWidth += widthInStackArgs(imported, paramType);
        }
      }));
  if (imported) {
    REG const targetReg{getREGForArg(MachineType::I64, true, tracker)};
    if (targetReg == REG::NONE) {
#ifdef __APPLE__
      stackParamWidth = roundUpToPow2(stackParamWidth, log2I32(widthInStackArgs(true, MachineType::I64)));
#endif
      stackParamWidth += widthInStackArgs(true, MachineType::I64); // For the context pointer
    }
  }
  return roundUpToPow2(stackParamWidth, 3U);
}

uint32_t Backend::offsetInStackArgs(bool const imported, uint32_t const paramWidth, RegStackTracker &tracker,
                                    MachineType const paramType) const VB_NOEXCEPT {
  uint32_t offsetInArgs{0U};
  uint32_t newlyAllocatedBytes{0U};
  if (imported) {
#ifdef __APPLE__
    offsetInArgs = roundUpToPow2(tracker.allocatedStackBytes, log2I32(widthInStackArgs(imported, paramType)));
    newlyAllocatedBytes = offsetInArgs - tracker.allocatedStackBytes;
#else
    static_cast<void>(paramType);
    offsetInArgs = tracker.allocatedStackBytes;
#endif
  } else {
    offsetInArgs = (paramWidth - 8U) - tracker.allocatedStackBytes;
  }
  newlyAllocatedBytes += widthInStackArgs(imported, paramType);

  tracker.allocatedStackBytes += newlyAllocatedBytes;
  return offsetInArgs;
}

uint32_t Backend::widthInStackArgs(bool const imported, MachineType const paramType) VB_NOEXCEPT {
#ifdef __APPLE__
  // https://developer.apple.com/documentation/xcode/writing-arm64-code-for-apple-platforms#Pass-arguments-to-functions-correctly
  // Apple platforms diverge from the ARM64 standard ABI:
  //  - Function arguments may consume slots on the stack that are not multiples of 8 bytes. If the total number of
  //  bytes for stack-based arguments is not a multiple of 8 bytes, insert padding on the
  // stack to maintain the 8-byte alignment requirements.
  if (imported) {
    return MachineTypeUtil::getSize(paramType);
  }
#else
  static_cast<void>(imported);
  static_cast<void>(paramType);
#endif
  return 8U;
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
        if (tracker.allocatedGPR < NABI::gpParams.size()) {
          reg = NABI::gpParams[tracker.allocatedGPR];
        }
      }
    } else {
      if (!imported) {
        if (tracker.allocatedFPR < WasmABI::regsForParams) {
          reg = WasmABI::fpr[moduleInfo_.getLocalStartIndexInFPRs() + tracker.allocatedFPR];
        }
      } else {
        if (tracker.allocatedFPR < NABI::fpParams.size()) {
          reg = NABI::fpParams[tracker.allocatedFPR];
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

void Backend::spillRestoreRegsRaw(Span<REG const> const &regs, bool const restore) const {
  size_t i{0U};

  while (i < regs.size()) {
    REG const reg{regs[i]};
    UnsignedInRangeCheck<6U> const rangeCheck{UnsignedInRangeCheck<6U>::check(static_cast<uint32_t>(i), static_cast<uint32_t>(regs.size()) - 2U)};
    if (rangeCheck.inRange()) {
      REG const reg2{regs[i + 1U]};
      if (RegUtil::isGPR(reg) == RegUtil::isGPR(reg2)) {
        OPCodeTemplate instr;
        if (restore) {
          instr = RegUtil::isGPR(reg) ? LDP_xT1_xT2_deref_xN_scSImm7_t : LDP_dT1_dT2_deref_xN_scSImm7_t;
        } else {
          instr = RegUtil::isGPR(reg) ? STP_xT1_xT2_deref_xN_scSImm7_t : STP_dT1_dT2_deref_xN_scSImm7_t;
        }

        SafeUInt<9U> const imm{rangeCheck.safeInt().leftShift<3U>()};
        as_.INSTR(instr).setT1(reg).setT2(reg2).setN(REG::SP).setSImm7ls3(static_cast<SafeInt<10U>>(imm))();
        i += 2U;
        continue;
      }
    }
    OPCodeTemplate instr;
    if (restore) {
      instr = LDR_T_deref_N_scUImm12(RegUtil::isGPR(reg), true);
    } else {
      instr = STR_T_deref_N_scUImm12(RegUtil::isGPR(reg), true);
    }
    as_.INSTR(instr).setT(reg).setN(REG::SP).setImm12zxls3(SafeUInt<15U>::fromUnsafe(static_cast<uint32_t>(i) * 8U))();
    i++;
  }
}

void Backend::emitV2ImportAdapterImpl(uint32_t const fncIndex) const {
  static_cast<void>(fncIndex);
  static_cast<void>(this);
  // Need handle muti return values to wasm style
  throw FeatureNotSupportedException(ErrorCode::Not_implemented);
}

void Backend::emitV1ImportAdapterImpl(uint32_t const fncIndex) {
  assert(moduleInfo_.functionIsImported(fncIndex) && "Function is not imported");

  uint32_t const sigIndex{moduleInfo_.getFncSigIndex(fncIndex)};
  if (moduleInfo_.functionIsBuiltin(fncIndex)) {
    throw FeatureNotSupportedException(ErrorCode::Cannot_indirectly_call_builtin_functions);
  }

  uint32_t const newStackParamWidth{getStackParamWidth(sigIndex, true)};
  uint32_t const oldStackParamWidth{getStackParamWidth(sigIndex, false)};

  common_.moveGlobalsToLinkData();

  // RSP <------------ Stack growth direction (downwards)
  //                      v <-- callScrRegs[1]                             reserve -->
  // | New Stack Params  | LR | JobMemoryPtrPtr | Padding | Old Stack Params |
  uint32_t const of_lr{newStackParamWidth};
  uint32_t const of_jobMemoryPtrPtr{of_lr + 8U};
  uint32_t const of_post{of_jobMemoryPtrPtr + Widths::jobMemoryPtrPtr};
  uint32_t const totalReserved{roundUpToPow2(of_post, 4U)};
  uint32_t const of_oldStackParams{totalReserved};

  static_assert(roundUpToPow2((ImplementationLimits::numParams * 8U) + 24U + ((WasmABI::regsForParams * 2U) * 8U), 4U) <= 0xFF'FF'FFU,
                "Too many arguments");
  as_.addImm24ToReg(REG::SP, -static_cast<int32_t>(totalReserved), true);
#if ACTIVE_STACK_OVERFLOW_CHECK
  as_.checkStackFence(callScrRegs[1]); // SP change
#endif

  // Spill LR
  as_.INSTR(STR_xT_deref_xN_imm12zxls3_t).setT(REG::LR).setN(REG::SP).setImm12zxls3(SafeUInt<15U>::fromUnsafe(of_lr))();

  // Spill all params that are currently residing in registers to the stack
  RegStackTracker srcTracker{};

  RegStackTracker targetTracker{};

  RegisterCopyResolver<NativeABI::gpParams.size()> registerCopyResolver{};

  // coverity[autosar_cpp14_a8_5_2_violation]
  auto const copyParamsCB = [this, &registerCopyResolver, of_oldStackParams, newStackParamWidth, oldStackParamWidth, &srcTracker,
                             &targetTracker](MachineType const paramType) {
    bool const is64{MachineTypeUtil::is64(paramType)};

    REG const sourceReg{getREGForArg(paramType, false, srcTracker)};
    REG const targetReg{getREGForArg(paramType, true, targetTracker)};

    uint32_t sourceStackOffset{0U};
    if (sourceReg == REG::NONE) {
      uint32_t const offsetInOldStackParams{offsetInStackArgs(false, oldStackParamWidth, srcTracker, paramType)};
      sourceStackOffset = of_oldStackParams + offsetInOldStackParams;
    }

    if (targetReg != REG::NONE) {
      if (sourceReg != REG::NONE) {
        if (targetReg == sourceReg) {
          // No need to move
          return;
        }
        if (RegUtil::isGPR(targetReg)) {
          registerCopyResolver.push(VariableStorage::reg(paramType, targetReg), VariableStorage::reg(paramType, sourceReg));
        } else {
          as_.INSTR(is64 ? FMOV_dD_dN : FMOV_sD_sN).setD(targetReg).setN(sourceReg)();
        }

      } else {
        // Stack to register, Only happen in debug build
        if (RegUtil::isGPR(targetReg)) {
          registerCopyResolver.push(VariableStorage::reg(paramType, targetReg), VariableStorage::stackMemory(paramType, sourceStackOffset));
        } else {
          if (is64) {
            as_.INSTR(LDR_dT_deref_xN_imm12zxls3_t).setT(targetReg).setN(REG::SP).setImm12zxls3(SafeUInt<15U>::fromUnsafe(sourceStackOffset))();
          } else {
            as_.INSTR(LDR_sT_deref_xN_imm12zxls2_t).setT(targetReg).setN(REG::SP).setImm12zxls2(SafeUInt<14U>::fromUnsafe(sourceStackOffset))();
          }
        }
      }

    } else {
      // GCOVR_EXCL_START
      assert(sourceReg == REG::NONE); // NO reg to stack case in Arm64 ABI
      // GCOVR_EXCL_STOP
      uint32_t const offsetFromSP{offsetInStackArgs(true, newStackParamWidth, targetTracker, paramType)};

      if (is64) {
        as_.INSTR(LDR_xT_deref_xN_imm12zxls3_t).setT(callScrRegs[0]).setN(REG::SP).setImm12zxls3(SafeUInt<15U>::fromUnsafe(sourceStackOffset))();
        as_.INSTR(STR_xT_deref_xN_imm12zxls3_t).setT(callScrRegs[0]).setN(REG::SP).setImm12zxls3(SafeUInt<15U>::fromUnsafe(offsetFromSP))();
      } else {
        as_.INSTR(LDR_wT_deref_xN_imm12zxls2_t).setT(callScrRegs[0]).setN(REG::SP).setImm12zxls2(SafeUInt<14U>::fromUnsafe(sourceStackOffset))();
        as_.INSTR(STR_wT_deref_xN_imm12zxls2_t).setT(callScrRegs[0]).setN(REG::SP).setImm12zxls2(SafeUInt<14U>::fromUnsafe(offsetFromSP))();
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
    uint32_t const offsetFromSP{offsetInStackArgs(true, newStackParamWidth, targetTracker, MachineType::I64)};
    as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
        .setT(callScrRegs[0])
        .setN(WasmABI::REGS::linMem)
        .setUnscSImm9(SafeInt<9U>::fromConst<-static_cast<int32_t>(Basedata::FromEnd::customCtxOffset)>())();
    as_.INSTR(STR_xT_deref_xN_imm12zxls3_t).setT(callScrRegs[0]).setN(REG::SP).setImm12zxls3(SafeUInt<15U>::fromUnsafe(offsetFromSP))();
  }

  registerCopyResolver.resolve(MoveEmitter([this](VariableStorage const &target, VariableStorage const &source) {
                                 bool const is64{MachineTypeUtil::is64(source.machineType)};
                                 // Can't use emitMoveIntImpl because it handles stack frame offset calculation differently
                                 if (source.type == StorageType::REGISTER) {
                                   as_.INSTR(is64 ? MOV_xD_xM_t : MOV_wD_wM_t).setD(target.location.reg).setM(source.location.reg)();
                                 } else if (source.type == StorageType::STACKMEMORY) {
                                   if (is64) {
                                     as_.INSTR(LDR_xT_deref_xN_imm12zxls3_t)
                                         .setT(target.location.reg)
                                         .setN(REG::SP)
                                         .setImm12zxls3(SafeUInt<15U>::fromUnsafe(source.location.stackFramePosition))();
                                   } else {
                                     as_.INSTR(LDR_wT_deref_xN_imm12zxls2_t)
                                         .setT(target.location.reg)
                                         .setN(REG::SP)
                                         .setImm12zxls2(SafeUInt<14U>::fromUnsafe(source.location.stackFramePosition))();
                                   }
                                 } else {
                                   as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
                                       .setD(target.location.reg)
                                       .setN(WasmABI::REGS::linMem)
                                       .setUnscSImm9(SafeInt<9U>::fromUnsafe(-static_cast<int32_t>(source.location.linkDataOffset)))();
                                 }
                               }),
                               SwapEmitter(nullptr));

  assert(roundUpToPow2(targetTracker.allocatedStackBytes, 3U) == newStackParamWidth && "Stack allocation size mismatch");

  // Patch the last function index because this was reached via an indirect call and the function index isn't known
  tryPatchFncIndexOfLastStacktraceEntry(fncIndex, callScrRegs[0], callScrRegs[1]);

#if LINEAR_MEMORY_BOUNDS_CHECKS
  cacheJobMemoryPtrPtr(of_jobMemoryPtrPtr, callScrRegs[0]);
#endif
  emitRawFunctionCall(fncIndex, true);
#if LINEAR_MEMORY_BOUNDS_CHECKS
  restoreFromJobMemoryPtrPtr(of_jobMemoryPtrPtr);
#endif
#if INTERRUPTION_REQUEST
  checkForInterruptionRequest(callScrRegs[0]);
#endif

  common_.recoverGlobalsToRegs();

  // Restore LR and unwind stack
  as_.INSTR(LDR_xT_deref_xN_imm12zxls3_t).setT(REG::LR).setN(REG::SP).setImm12zxls3(SafeUInt<15U>::fromUnsafe(of_lr))();
  as_.addImm24ToReg(REG::SP, static_cast<int32_t>(totalReserved), true);
  as_.INSTR(RET_xN_t).setN(REG::LR)();
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

void Backend::emitRawFunctionCall(uint32_t const fncIndex, bool const linkRegister) {
  if (moduleInfo_.functionIsImported(fncIndex)) {
    // Calling an imported function
    assert(!moduleInfo_.functionIsBuiltin(fncIndex) && "Builtin functions cannot be emitted this way, do it explicitly");

    if (!moduleInfo_.functionIsLinked(fncIndex)) {
      as_.TRAP(TrapCode::CALLED_FUNCTION_NOT_LINKED);
      return;
    }

#if (MAX_WASM_STACKSIZE_BEFORE_NATIVE_CALL != 0 && !ACTIVE_STACK_OVERFLOW_CHECK) ||                                                                  \
    (STACKSIZE_LEFT_BEFORE_NATIVE_CALL != 0 && ACTIVE_STACK_OVERFLOW_CHECK)
    as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
        .setT(callScrRegs[0])
        .setN(WasmABI::REGS::linMem)
        .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::nativeStackFence>())();
    as_.INSTR(CMP_SP_xM_t).setM(callScrRegs[0])();
    as_.cTRAP(TrapCode::STACKFENCEBREACHED, CC::LS);
#endif

    ModuleInfo::ImpFuncDef const impFuncDef{moduleInfo_.getImpFuncDef(fncIndex)};

    // Load the address into a register
    constexpr REG callReg{callScrRegs[0]};
    NativeSymbol const &nativeSymbol{moduleInfo_.importSymbols[impFuncDef.symbolIndex]};
    if (nativeSymbol.linkage == NativeSymbol::Linkage::STATIC) {
      as_.MOVimm64(callReg, bit_cast<uint64_t>(nativeSymbol.ptr));
    } else {
      uint32_t const fncPtrBaseOffset{BD::FromStart::linkData + impFuncDef.linkDataOffset};
      UnsignedInRangeCheck<15U> const safeFncPtrBaseOffset{UnsignedInRangeCheck<15U>::check(fncPtrBaseOffset)};
      if (safeFncPtrBaseOffset.inRange()) {
        as_.INSTR(LDR_xT_deref_xN_imm12zxls3_t).setT(callReg).setN(WasmABI::REGS::jobMem).setImm12zxls3(safeFncPtrBaseOffset.safeInt())();
      } else {
        as_.MOVimm32(callScrRegs[1], fncPtrBaseOffset);
        as_.INSTR(LDR_xT_deref_xN_xM_t).setT(callReg).setN(WasmABI::REGS::jobMem).setM(callScrRegs[1])();
      }
    }
    // Execute the actual call
    as_.INSTR(linkRegister ? BLR_xN_t : BR_xN_t).setN(callReg)();
  } else {
    // Calling a Wasm-internal function
    // Check if the function body we are targeting has already been emitted
    if (fncIndex <= moduleInfo_.fnc.index) {
      // Check at which offset in the binary the function body is present
      uint32_t const binaryFncBodyOffset{moduleInfo_.wasmFncBodyBinaryPositions[fncIndex]};
      // If the index is smaller then the current index, it's already defined
      assert(binaryFncBodyOffset != 0xFF'FF'FF'FF && "Function needs to be defined already");

      // Produce a dummy call instruction, synthesize a corresponding RelPatchObj and link it to the start of the body
      RelPatchObj const branchObj{as_.INSTR(linkRegister ? BL_imm26sxls2_t : B_imm26sxls2_t).setImm19o26ls2BranchPlaceHolder().prepJmp()};
      branchObj.linkToBinaryPos(binaryFncBodyOffset);
    } else {
      // Body of the target function has not been emitted yet so we link it to either an unknown target or the last
      // branch that targets this still-unknown function body. This way we are essentially creating a linked-list of
      // branches inside the output binary that we are going to fully patch later

      // We correspondingly produce a call instruction and register the branch so it will be patched later
      RelPatchObj const branchObj{as_.INSTR(linkRegister ? BL_imm26sxls2_t : B_imm26sxls2_t).setImm19o26ls2BranchPlaceHolder().prepJmp()};
      registerPendingBranch(branchObj, moduleInfo_.wasmFncBodyBinaryPositions[fncIndex]);
    }
  }
}

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
    v2ImportCall.storeLR();
    v2ImportCall.iterateParams(paramsBase);
    uint32_t const jobMemoryPtrPtrOffset{v2ImportCall.getJobMemoryPtrPtrOffset()};
    // coverity[autosar_cpp14_a5_1_9_violation]
    v2ImportCall.emitFncCallWrapper(fncIndex, FunctionRef<void()>([this, fncIndex, jobMemoryPtrPtrOffset]() {
                                      // clang-format off
      static_cast<void>(jobMemoryPtrPtrOffset);
      #if LINEAR_MEMORY_BOUNDS_CHECKS
      cacheJobMemoryPtrPtr(jobMemoryPtrPtrOffset, callScrRegs[0]);
      #endif
      emitRawFunctionCall(fncIndex);
      #if LINEAR_MEMORY_BOUNDS_CHECKS
      restoreFromJobMemoryPtrPtr(jobMemoryPtrPtrOffset);
      #endif
      #if INTERRUPTION_REQUEST
      checkForInterruptionRequest(callScrRegs[0]);
      #endif
    }));
    // clang-format on

    v2ImportCall.restoreLR();
#if LINEAR_MEMORY_BOUNDS_CHECKS
    setupMemSizeReg();
#endif
    common_.recoverGlobalsToRegs();
    v2ImportCall.iterateResults();
  } else if (imported) {
    // Direct call to V1 import native function
    common_.moveGlobalsToLinkData();
    ImportCallV1 importCallV1Impl{*this, sigIndex};

    importCallV1Impl.storeLR();
    RegMask const availableLocalsRegMask{common_.saveLocalsAndParamsForFuncCall(true)};
    static_cast<void>(importCallV1Impl.iterateParams(paramsBase, availableLocalsRegMask));
    importCallV1Impl.prepareCtx();
    importCallV1Impl.resolveRegisterCopies();
    uint32_t const jobMemoryPtrPtrOffset{importCallV1Impl.getJobMemoryPtrPtrOffset()};
    // coverity[autosar_cpp14_a5_1_9_violation]
    importCallV1Impl.emitFncCallWrapper(fncIndex, FunctionRef<void()>([this, fncIndex, jobMemoryPtrPtrOffset]() {
                                          // clang-format off
      static_cast<void>(jobMemoryPtrPtrOffset);
      #if LINEAR_MEMORY_BOUNDS_CHECKS
      cacheJobMemoryPtrPtr(jobMemoryPtrPtrOffset, callScrRegs[0]);
      #endif
      emitRawFunctionCall(fncIndex);
      #if LINEAR_MEMORY_BOUNDS_CHECKS
      restoreFromJobMemoryPtrPtr(jobMemoryPtrPtrOffset);
      #endif
      #if INTERRUPTION_REQUEST
      checkForInterruptionRequest(callScrRegs[0]);
      #endif
    }));
    // clang-format on

    importCallV1Impl.restoreLR();
#if LINEAR_MEMORY_BOUNDS_CHECKS
    setupMemSizeReg();
#endif
    common_.recoverGlobalsToRegs();
    importCallV1Impl.iterateResults();
  } else {
    // Direct call to a Wasm function
    InternalCall directWasmCallImpl{*this, sigIndex};

    directWasmCallImpl.storeLR();

    RegMask const availableLocalsRegMask{common_.saveLocalsAndParamsForFuncCall(imported)};
    static_cast<void>(directWasmCallImpl.iterateParams(paramsBase, availableLocalsRegMask));
    directWasmCallImpl.resolveRegisterCopies();
    // coverity[autosar_cpp14_a5_1_9_violation]
    directWasmCallImpl.emitFncCallWrapper(fncIndex, FunctionRef<void()>([this, fncIndex]() {
                                            emitRawFunctionCall(fncIndex);
                                          }));

    directWasmCallImpl.restoreLR();
    directWasmCallImpl.iterateResults();
  }
}

void Backend::execIndirectWasmCall(uint32_t const sigIndex, uint32_t const tableIndex) {
  static_cast<void>(tableIndex);
  assert(moduleInfo_.hasTable && tableIndex == 0 && "Table not defined");
  Stack::iterator const paramsBase{common_.prepareCallParamsAndSpillContext(sigIndex, true)};

  InternalCall indirectCallImpl{*this, sigIndex};
  indirectCallImpl.storeLR();
  RegMask const availableLocalsRegMask{common_.saveLocalsAndParamsForFuncCall(false)};

  Stack::iterator const indirectCallIndex{indirectCallImpl.iterateParams(paramsBase, availableLocalsRegMask)};
  indirectCallImpl.handleIndirectCallReg(indirectCallIndex, availableLocalsRegMask);

  indirectCallImpl.resolveRegisterCopies();

  indirectCallImpl.emitFncCallWrapper(
      UnknownIndex, FunctionRef<void()>([this, sigIndex]() {
        // R0 contains the table index of the function that should be called
        // Check if dynamic function index is in range of table
        UnsignedInRangeCheck<12U> const rangeCheckSize{UnsignedInRangeCheck<12U>::check(moduleInfo_.tableInitialSize)};
        if (rangeCheckSize.inRange()) {
          as_.INSTR(CMP_wN_imm12zxols12).setN(WasmABI::REGS::indirectCallReg).setImm12zx(rangeCheckSize.safeInt())();
        } else {
          as_.MOVimm32(callScrRegs[1], moduleInfo_.tableInitialSize);
          as_.INSTR(CMP_wN_wM).setN(WasmABI::REGS::indirectCallReg).setM(callScrRegs[1])();
        }
        as_.cTRAP(TrapCode::INDIRECTCALL_OUTOFBOUNDS, CC::HS);

        // Load pointer to table start
        as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
            .setT(callScrRegs[0])
            .setN(WasmABI::REGS::linMem)
            .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::tableAddressOffset>())();
        // Step to the actual table entry we are targeting
        as_.INSTR(ADD_xD_xN_xMolsImm6)
            .setD(callScrRegs[0])
            .setN(callScrRegs[0])
            .setM(WasmABI::REGS::indirectCallReg)
            .setOlsImm6(SafeUInt<6U>::fromConst<3U>())();

        // Load function signature index and check if it matches
        as_.INSTR(LDR_wT_deref_xN_imm12zxls2_t).setT(callScrRegs[1]).setN(callScrRegs[0]).setImm12zxls2(SafeUInt<14U>::fromConst<4U>())();

        UnsignedInRangeCheck<12U> const rangeCheckIndex{UnsignedInRangeCheck<12U>::check(sigIndex)};
        if (rangeCheckIndex.inRange()) {
          as_.INSTR(CMP_wN_imm12zxols12).setN(callScrRegs[1]).setImm12zx(rangeCheckIndex.safeInt())();
        } else {
          as_.MOVimm32(callScrRegs[2], sigIndex);
          as_.INSTR(CMP_wN_wM).setN(callScrRegs[1]).setM(callScrRegs[2])();
        }
        as_.cTRAP(TrapCode::INDIRECTCALL_WRONGSIG, CC::NE);

        // Load the offset
        as_.INSTR(LDR_wT_deref_xN_imm12zxls2_t).setT(callScrRegs[1]).setN(callScrRegs[0]).setImm12zxls2(SafeUInt<14U>::fromConst<0U>())();

        // Check if the offset is zero which means the function is not linked
        as_.INSTR(CMP_wN_wM).setN(callScrRegs[1]).setM(REG::ZR)();
        as_.cTRAP(TrapCode::CALLED_FUNCTION_NOT_LINKED, CC::EQ);

        // Otherwise calculate the absolute address and execute the call
        as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
            .setT(callScrRegs[0])
            .setN(WasmABI::REGS::linMem)
            .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::binaryModuleStartAddressOffset>())();
        as_.INSTR(ADD_xD_xN_xMolsImm6).setD(callScrRegs[0]).setN(callScrRegs[0]).setM(callScrRegs[1])();
        as_.INSTR(BLR_xN_t).setN(callScrRegs[0])();
      }));

  indirectCallImpl.restoreLR();
  indirectCallImpl.iterateResults();
}

void Backend::executeTrap(TrapCode const code) const {
  as_.TRAP(code);
}

#if BUILTIN_FUNCTIONS
void Backend::execBuiltinFncCall(BuiltinFunction const builtinFunction) {
  switch (builtinFunction) {
  case BuiltinFunction::TRAP: {
    executeTrap(TrapCode::BUILTIN_TRAP);
    break;
  }
  case BuiltinFunction::GETLENGTHOFLINKEDMEMORY: {
    RegAllocTracker regAllocTracker{};
    RegElement const bufLenRegElem{common_.reqScratchRegProt(MachineType::I32, regAllocTracker, false)};
    as_.INSTR(LDUR_wT_deref_xN_unscSImm9_t)
        .setT(bufLenRegElem.reg)
        .setN(WasmABI::REGS::linMem)
        .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::linkedMemLen>())();
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

    uint32_t const biFncIndex{static_cast<uint32_t>(builtinFunction) - static_cast<uint32_t>(BuiltinFunction::GETU8FROMLINKEDMEMORY)};
    // coverity[autosar_cpp14_a8_5_2_violation]
    constexpr auto dataSizes =
        make_array(SafeUInt<8U>::fromConst<1_U8>(), SafeUInt<8U>::fromConst<1_U8>(), SafeUInt<8U>::fromConst<2_U8>(), SafeUInt<8U>::fromConst<2_U8>(),
                   SafeUInt<8U>::fromConst<4_U8>(), SafeUInt<8U>::fromConst<4_U8>(), SafeUInt<8U>::fromConst<8_U8>(), SafeUInt<8U>::fromConst<8_U8>(),
                   SafeUInt<8U>::fromConst<4_U8>(), SafeUInt<8U>::fromConst<8_U8>());
    // coverity[autosar_cpp14_a8_5_2_violation]
    constexpr auto wasmTypes = make_array(MachineType::I32, MachineType::I32, MachineType::I32, MachineType::I32, MachineType::I32, MachineType::I32,
                                          MachineType::I64, MachineType::I64, MachineType::F32, MachineType::F64);

    SafeUInt<8U> const dataSize{dataSizes[biFncIndex]};
    MachineType const machineType{wasmTypes[biFncIndex]};

    RegAllocTracker regAllocTracker{};
    RegElement const linkedMemLenPtrRegElem{common_.reqScratchRegProt(MachineType::I64, regAllocTracker, false)};
    as_.INSTR(LDUR_wT_deref_xN_unscSImm9_t)
        .setT(linkedMemLenPtrRegElem.reg)
        .setN(WasmABI::REGS::linMem)
        .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::linkedMemLen>())();

    RegElement targetRegElem{};
    if (MachineTypeUtil::isInt(machineType)) {
      targetRegElem.elem = StackElement::scratchReg(linkedMemLenPtrRegElem.reg, MachineTypeUtil::toStackTypeFlag(machineType));
      targetRegElem.reg = linkedMemLenPtrRegElem.reg;
    } else {
      targetRegElem = common_.reqScratchRegProt(machineType, regAllocTracker, false);
    }

    bool constEncoded{false};

    if (offsetElementPtr->type == StackType::CONSTANT_I32) {
      uint32_t const offset{(offsetElementPtr->type == StackType::CONSTANT_I32) ? offsetElementPtr->data.constUnion.u32 : 0U};

      SignedInRangeCheck<9U> const rangeCheck9{SignedInRangeCheck<9U>::check(static_cast<int32_t>(offset), 0, 255)};
      if (rangeCheck9.inRange()) {
        constEncoded = true;
        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto loadInstrs =
            make_array(LDURB_wT_deref_xN_unscSImm9_t, LDURSB_wT_deref_xN_unscSImm9_t, LDURH_wT_deref_xN_unscSImm9_t, LDURSH_wT_deref_xN_unscSImm9_t,
                       LDUR_wT_deref_xN_unscSImm9_t, LDUR_wT_deref_xN_unscSImm9_t, LDUR_xT_deref_xN_unscSImm9_t, LDUR_xT_deref_xN_unscSImm9_t,
                       LDUR_sT_deref_xN_unscSImm9_t, LDUR_dT_deref_xN_unscSImm9_t);

        // cmp bufferLength, u32
        SafeUInt<10> const imm{static_cast<SafeUInt<9>>(rangeCheck9.safeInt()) + dataSize};
        as_.INSTR(CMP_wN_imm12zxols12).setN(linkedMemLenPtrRegElem.reg).setImm12zx(static_cast<SafeUInt<12U>>(imm))();
        as_.cTRAP(TrapCode::LINKEDMEMORY_MUX, CC::LO);

        as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
            .setT(linkedMemLenPtrRegElem.reg)
            .setN(WasmABI::REGS::linMem)
            .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::linkedMemPtr>())();
        as_.INSTR(loadInstrs[biFncIndex]).setT(targetRegElem.reg).setN(linkedMemLenPtrRegElem.reg).setUnscSImm9(rangeCheck9.safeInt())();
      } else if (((offset % dataSize.value()) == 0U) && (offset < (1_U32 << 12_U32))) {
        UnsignedInRangeCheck<12U> const rangeCheck12{UnsignedInRangeCheck<12U>::check(offset + dataSize.value())};

        if (rangeCheck12.inRange()) {
          constEncoded = true;
          // Last condition is for compare as lowest bound (also when LDR is used
          // with operand size=1, but dataSize is more limiting anyway)
          // coverity[autosar_cpp14_a8_5_2_violation]
          constexpr auto loadInstrs =
              make_array(LDRB_wT_deref_xN_imm12zx_t, LDRSB_wT_deref_xN_imm12zx_t, LDRH_wT_deref_xN_imm12zxls1_t, LDRSH_wT_deref_xN_imm12zxls1_t,
                         LDR_wT_deref_xN_imm12zxls2_t, LDR_wT_deref_xN_imm12zxls2_t, LDR_xT_deref_xN_imm12zxls3_t, LDR_xT_deref_xN_imm12zxls3_t,
                         LDR_sT_deref_xN_imm12zxls2_t, LDR_dT_deref_xN_imm12zxls3_t);

          // cmp bufferLength, u32
          as_.INSTR(CMP_wN_imm12zxols12).setN(linkedMemLenPtrRegElem.reg).setImm12zx(rangeCheck12.safeInt())();
          as_.cTRAP(TrapCode::LINKEDMEMORY_MUX, CC::LO);

          as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
              .setT(linkedMemLenPtrRegElem.reg)
              .setN(WasmABI::REGS::linMem)
              .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::linkedMemPtr>())();
          vb::aarch64::OPCodeTemplate const instruction{loadInstrs[biFncIndex]};
          SafeUInt<12> const safeOffset{rangeCheck12.safeInt() - dataSize};
          if (builtinFunction >= BuiltinFunction::GETU32FROMLINKEDMEMORY) {
            bool const is64{MachineTypeUtil::is64(machineType)};
            if (is64) {
              as_.INSTR(instruction).setT(targetRegElem.reg).setN(linkedMemLenPtrRegElem.reg).setImm12zxls3(static_cast<SafeUInt<15U>>(safeOffset))();
            } else {
              as_.INSTR(instruction).setT(targetRegElem.reg).setN(linkedMemLenPtrRegElem.reg).setImm12zxls2(static_cast<SafeUInt<14U>>(safeOffset))();
            }
          } else if (builtinFunction >= BuiltinFunction::GETU16FROMLINKEDMEMORY) {
            as_.INSTR(instruction).setT(targetRegElem.reg).setN(linkedMemLenPtrRegElem.reg).setImm12zxls1(static_cast<SafeUInt<13U>>(safeOffset))();
          } else {
            as_.INSTR(instruction).setT(targetRegElem.reg).setN(linkedMemLenPtrRegElem.reg).setImm12zx(safeOffset)();
          }
        }
      } else {
        // Pass
      }
    }

    if (!constEncoded) {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto loadInstrs =
          make_array(LDRB_wT_deref_xN_xM_t, LDRSB_wT_deref_xN_xM_t, LDRH_wT_deref_xN_xM_t, LDRSH_wT_deref_xN_xM_t, LDR_wT_deref_xN_xM_t,
                     LDR_wT_deref_xN_xM_t, LDR_xT_deref_xN_xM_t, LDR_xT_deref_xN_xM_t, LDR_sT_deref_xN_xM_t, LDR_dT_deref_xN_xM_t);

      REG const offsetReg{common_.liftToRegInPlaceProt(*offsetElementPtr, false, regAllocTracker).reg};
      as_.INSTR(SUBS_wD_wN_imm12zxols12)
          .setD(linkedMemLenPtrRegElem.reg)
          .setN(linkedMemLenPtrRegElem.reg)
          .setImm12zx(static_cast<SafeUInt<12U>>(dataSize))();
      RelPatchObj const underflow{as_.prepareJMP(CC::MI)};
      as_.INSTR(CMP_wN_wM).setN(offsetReg).setM(linkedMemLenPtrRegElem.reg)();
      RelPatchObj const inRange{as_.prepareJMP(CC::LS)};
      underflow.linkToHere();
      as_.TRAP(TrapCode::LINKEDMEMORY_MUX);
      inRange.linkToHere();

      as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
          .setT(linkedMemLenPtrRegElem.reg)
          .setN(WasmABI::REGS::linMem)
          .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::linkedMemPtr>())();
      as_.INSTR(loadInstrs[biFncIndex]).setT(targetRegElem.reg).setN(linkedMemLenPtrRegElem.reg).setM(offsetReg)();
    }
    common_.replaceAndUpdateReference(offsetElementPtr, targetRegElem.elem);
    break;
  }
  case BuiltinFunction::ISFUNCTIONLINKED: {
    Stack::iterator const fncIdxElementPtr{common_.condenseValentBlockBelow(stack_.end())};

    VariableStorage const fncIdxElementStorage{moduleInfo_.getStorage(*fncIdxElementPtr)};
    if (fncIdxElementStorage.type == StorageType::CONSTANT) {
      common_.emitIsFunctionLinkedCompileTimeOpt(fncIdxElementPtr);
    } else {
      // Runtime value, we need to look it up
      RegAllocTracker regAllocTracker{};
      REG const fncIdxReg{common_.liftToRegInPlaceProt(*fncIdxElementPtr, false, regAllocTracker).reg};
      REG const importScratchReg{common_.reqScratchRegProt(MachineType::I64, regAllocTracker, false).reg};

      UnsignedInRangeCheck<12U> const rangeCheckSize{UnsignedInRangeCheck<12U>::check(moduleInfo_.tableInitialSize)};
      if (rangeCheckSize.inRange()) {
        as_.INSTR(CMP_wN_imm12zxols12).setN(fncIdxReg).setImm12zx(rangeCheckSize.safeInt())();
      } else {
        as_.MOVimm32(importScratchReg, moduleInfo_.tableInitialSize);
        as_.INSTR(CMP_wN_wM).setN(fncIdxReg).setM(importScratchReg)();
      }

      RelPatchObj const inRange{as_.prepareJMP(CC::LO)};
      as_.INSTR(MOV_wD_wM_t).setD(importScratchReg).setM(REG::ZR)();
      RelPatchObj const toEnd{as_.prepareJMP()};
      inRange.linkToHere();
      // Load pointer to table start
      as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
          .setT(importScratchReg)
          .setN(WasmABI::REGS::linMem)
          .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::tableAddressOffset>())();
      // Step to the actual table entry we are targeting
      as_.INSTR(ADD_xD_xN_xMolsImm6).setD(importScratchReg).setN(importScratchReg).setM(fncIdxReg).setOlsImm6(SafeUInt<6U>::fromConst<3U>())();

      // Load function offset
      as_.INSTR(LDR_wT_deref_xN_imm12zxls2_t).setT(importScratchReg).setN(importScratchReg)();

      // Check if the offset is 0 or 0xFFFFFFFF. The following instructions are referred from the -O2 build of clang.
      // (x != 0) && (x!=0xFFFFFFFF) can be convert to (0-1)=0xFFFFFFFF 0xFFFFFFFF-1=0xFFFFFFFE Then check if x+2 has carry over
      as_.INSTR(SUB_wD_wN_imm12zxols12).setD(importScratchReg).setN(importScratchReg).setImm12zx(SafeUInt<12U>::fromConst<1U>())();
      as_.INSTR(CMN_wN_imm12zxols12_t).setN(importScratchReg).setImm12zx(SafeUInt<12U>::fromConst<2U>())();
      as_.INSTR(CSET_wD).setD(importScratchReg).setCond(false, CC::CS)();

      toEnd.linkToHere();
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

    // Make scratch registers available
    REG const scratchReg{common_.reqScratchRegProt(MachineType::I32, regAllocTracker, false).reg};
    REG const floatScratchReg{common_.reqScratchRegProt(MachineType::F64, regAllocTracker, false).reg};
    REG const floatScratchReg2{common_.reqScratchRegProt(MachineType::F64, regAllocTracker, false).reg};

    // Add size to destination and check for an overflow
    as_.INSTR(ADDS_wD_wN_wM).setD(dstReg).setN(dstReg).setM(sizeReg)();
    as_.cTRAP(TrapCode::LINMEM_OUTOFBOUNDSACCESS, CC::CS);

#if LINEAR_MEMORY_BOUNDS_CHECKS
    // Check bounds, can use 0 as memObjSize since we already added it to the offset
    emitLinMemBoundsCheck(dstReg, 0U);
#endif
    as_.INSTR(SUB_wD_wN_wMolsImm6).setD(dstReg).setN(dstReg).setM(sizeReg)(); // Subtract size again from dst so we get the start address
    as_.INSTR(ADD_xD_xN_xMolsImm6).setD(dstReg).setN(dstReg).setM(WasmABI::REGS::linMem)();

    // Absolute target pointer is now in dstReg, size is in sizeReg, src offset is in srcReg (all writable)

    // Load length of linked memory into scratch register
    as_.INSTR(LDUR_wT_deref_xN_unscSImm9_t)
        .setT(scratchReg)
        .setN(WasmABI::REGS::linMem)
        .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::linkedMemLen>())();

    // Check bounds of src
    as_.INSTR(SUBS_wD_wN_wM).setD(scratchReg).setN(scratchReg).setM(sizeReg)();
    RelPatchObj const underflow{as_.prepareJMP(CC::MI)};
    as_.INSTR(CMP_wN_wM).setN(srcReg).setM(scratchReg)();
    RelPatchObj const inRange{as_.prepareJMP(CC::LS)};
    underflow.linkToHere();
    as_.TRAP(TrapCode::LINKEDMEMORY_MUX);
    inRange.linkToHere();

    // Both are in bounds, let's copy the data

#if !LINEAR_MEMORY_BOUNDS_CHECKS && !EAGER_ALLOCATION
    // Probe first because memory accesses crossing page boundaries with different permissions are UNPREDICTABLE on ARM
    // If EAGER_ALLOCATION is turned on, the whole formal size is guaranteed to be read-write accessible already

    constexpr uint32_t pageSize{4096U};                                   // Must be power of 2
    as_.MOVimm64(scratchReg, ~(static_cast<uint64_t>(pageSize) - 1_U64)); // mask
    as_.INSTR(AND_xD_xN_xM).setD(scratchReg).setN(dstReg).setM(scratchReg)();

    // Now scratchReg is "start" of first page, might even be before the actual data (lowest address)

    // "Dummy read" first byte so zero width copies trap if address is out of bounds
    as_.INSTR(LDURB_wT_deref_xN_unscSImm9_t).setT(REG::ZR).setN(scratchReg).setUnscSImm9(SafeInt<9>::fromConst<0>())();

    // Temporarily add size to dst, so we know where the end is (highest address)
    as_.INSTR(ADD_xD_xN_xMolsImm6).setD(dstReg).setN(dstReg).setM(sizeReg)();

    uint32_t const tryNextPage{output_.size()};
    as_.INSTR(CMP_xN_xM).setN(scratchReg).setM(dstReg)();
    RelPatchObj const done{as_.prepareJMP(CC::HS)};

    // Access byte and discard
    as_.INSTR(LDURB_wT_deref_xN_unscSImm9_t).setT(REG::ZR).setN(scratchReg).setUnscSImm9(SafeInt<9>::fromConst<0>())();

    as_.INSTR(ADD_xD_xN_imm12zxols12).setD(scratchReg).setN(scratchReg).setImm12zxls12(SafeUInt<24>::fromConst<(pageSize)>())();
    RelPatchObj const toNextPage{as_.prepareJMP(CC::NONE)};
    toNextPage.linkToBinaryPos(tryNextPage);

    done.linkToHere();
    // Subtract size from dst again
    as_.INSTR(SUB_xD_xN_xMolsImm6).setD(dstReg).setN(dstReg).setM(sizeReg)();
#endif

    // Load linked memory start pointer and add it to srcReg
    as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
        .setT(scratchReg)
        .setN(WasmABI::REGS::linMem)
        .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::linkedMemPtr>())();
    as_.INSTR(ADD_xD_xN_xMolsImm6).setD(srcReg).setN(scratchReg).setM(srcReg)();
    constexpr bool canOverlap{false};
    emitMemcpyNoBoundsCheck(dstReg, srcReg, sizeReg, scratchReg, floatScratchReg, floatScratchReg2, canOverlap);

    common_.removeReference(sizeElem);
    common_.removeReference(srcElem);
    common_.removeReference(dstElem);
    static_cast<void>(stack_.erase(sizeElem));
    static_cast<void>(stack_.erase(srcElem));
    static_cast<void>(stack_.erase(dstElem));
    break;
  }
  case BuiltinFunction::TRACE_POINT: {
    static constexpr REG tmpReg1{WasmABI::gpr[WasmABI::gpr.size() - 1U]}; ///< temporary register for trace point
    static constexpr REG tmpReg2{WasmABI::gpr[WasmABI::gpr.size() - 2U]}; ///< temporary register for trace point
    static constexpr REG tmpReg3{WasmABI::gpr[WasmABI::gpr.size() - 3U]}; ///< temporary register for trace point

    TempRegManager tempRegManagerForIdentifier{*this};
    REG const tempGPR{tempRegManagerForIdentifier.getTempGPR()};

    Stack::iterator const identifierElement{common_.condenseValentBlockBelow(stack_.end())};
    VariableStorage const identifierStorage{moduleInfo_.getStorage(*identifierElement)};
    switch (identifierStorage.type) {
    case StorageType::STACKMEMORY:
    case StorageType::LINKDATA: {
      RegDisp<14U> const srcRegDisp{getMemRegDisp<14U>(identifierStorage, tempGPR)};
      as_.INSTR(LDR_wT_deref_xN_imm12zxls2_t).setT(tempGPR).setN(srcRegDisp.reg).setImm12zxls2(srcRegDisp.disp)();
      break;
    }
    case StorageType::REGISTER:
      as_.INSTR(MOV_wD_wM_t).setD(tempGPR).setM(identifierStorage.location.reg)();
      break;
    case StorageType::CONSTANT: {
      as_.MOVimm32(tempGPR, identifierStorage.location.constUnion.u32);
      break;
    }
    default:
      UNREACHABLE(break, "Unknown storage");
    }
    common_.removeReference(identifierElement);
    static_cast<void>(stack_.erase(identifierElement));

    // coverity[autosar_cpp14_a8_5_2_violation]
    auto const ensureTracePointHandlerExistAndInRange = [this, tempGPR]() -> void {
      if (moduleInfo_.helperFunctionBinaryPositions.builtinTracePointHandler != 0xFFFFFFFFU) {
        SignedInRangeCheck<28U> const inRangeCheck{SignedInRangeCheck<28U>::check(
            static_cast<int32_t>(output_.size()) - static_cast<int32_t>(moduleInfo_.helperFunctionBinaryPositions.builtinTracePointHandler))};
        if (inRangeCheck.inRange()) {
          return;
        }
      }

      RelPatchObj const mainCode{as_.prepareJMP(CC::NONE)};

      // start of the trace point handler
      moduleInfo_.helperFunctionBinaryPositions.builtinTracePointHandler = output_.size();

      constexpr REG traceBufferPtrReg{tmpReg1};
      as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
          .setT(traceBufferPtrReg)
          .setN(WasmABI::REGS::linMem)
          .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::traceBufferPtr>())();
      // size<u32> | cursor<u32> | (rdtsc<u32> | identifier<u32> )+
      //                         ^
      //                   traceBufferPtrReg
      RelPatchObj const nullptrTraceBuffer{as_.prepareJMPIfRegIsZero(traceBufferPtrReg, true)};

      constexpr REG cursorReg{tmpReg3};
      {
        constexpr REG sizeReg{tmpReg2};
        as_.INSTR(LDP_wT1_wT2_deref_xN_scSImm7_t).setT1(sizeReg).setT2(cursorReg).setN(traceBufferPtrReg).setSImm7ls2(SafeInt<9>::fromConst<-8>())();
        as_.INSTR(CMP_wN_wM).setN(cursorReg).setM(sizeReg)();
      } /// last use of @b sizeReg
      RelPatchObj const isFull{as_.prepareJMP(CC::HS)};

      // cursor++;
      as_.INSTR(ADD_wD_wN_imm12zxols12).setD(cursorReg).setN(cursorReg).setImm12zx(SafeUInt<12U>::fromConst<1U>())();
      as_.INSTR(STUR_xT_deref_xN_unscSImm9_t).setT(cursorReg).setN(traceBufferPtrReg).setUnscSImm9(SafeInt<9>::fromConst<-4>())();

      // traceBufferPtrReg[cursor] <- timePointReg, identifier
      {
        constexpr REG timePointReg{tmpReg2};
        as_.INSTR(MRS_xT_CNTVCT_EL0).setT(timePointReg)(); // Read time-stamp counter into RDX:RAX
        as_.INSTR(ADD_xD_xN_xMolsImm6).setD(cursorReg).setN(traceBufferPtrReg).setM(cursorReg).setImm6(SafeUInt<6>::fromConst<3>())();
        as_.INSTR(STP_wT1_wT2_deref_xN_scSImm7_t).setT1(timePointReg).setT2(tempGPR).setN(cursorReg).setSImm7ls2(SafeInt<9>::fromConst<-8>())();
      } /// last use of @b timePointReg

      isFull.linkToHere();
      nullptrTraceBuffer.linkToHere();

      as_.INSTR(RET_xN_t).setN(REG::LR)();
      mainCode.linkToHere();
    };

    bool const needPushTempReg1And2ToStack{!moduleInfo_.getReferenceToLastOccurrenceOnStack(tmpReg1).isEmpty() ||
                                           !moduleInfo_.getReferenceToLastOccurrenceOnStack(tmpReg2).isEmpty()};

    constexpr uint32_t stackSize{8U * 4U};
    as_.INSTR(SUB_xD_xN_imm12zxols12).setD(REG::SP).setN(REG::SP).setImm12zx(SafeUInt<12U>::fromConst<stackSize>())();

    constexpr uint32_t tmpReg3And4Offset{16U};
    constexpr uint32_t tmpReg1And2Offset{tmpReg3And4Offset + 16U};

    if (needPushTempReg1And2ToStack) {
      as_.INSTR(STP_xT1_xT2_deref_xN_scSImm7_t)
          .setT1(tmpReg1)
          .setT2(tmpReg2)
          .setN(REG::SP)
          .setSImm7ls3(SafeInt<10>::fromConst<tmpReg1And2Offset>())();
    }
    as_.INSTR(STP_xT1_xT2_deref_xN_scSImm7_t).setT1(tmpReg3).setT2(REG::LR).setN(REG::SP).setSImm7ls3(SafeInt<10>::fromConst<tmpReg3And4Offset>())();

    ensureTracePointHandlerExistAndInRange();
    as_.INSTR(BL_imm26sxls2_t)
        .setImm19o26ls2BranchPlaceHolder()
        .prepJmp()
        .linkToBinaryPos(moduleInfo_.helperFunctionBinaryPositions.builtinTracePointHandler);

    if (needPushTempReg1And2ToStack) {
      as_.INSTR(LDP_xT1_xT2_deref_xN_scSImm7_t)
          .setT1(tmpReg1)
          .setT2(tmpReg2)
          .setN(REG::SP)
          .setSImm7ls3(SafeInt<10>::fromConst<tmpReg1And2Offset>())();
    }
    as_.INSTR(LDP_xT1_xT2_deref_xN_scSImm7_t).setT1(tmpReg3).setT2(REG::LR).setN(REG::SP).setSImm7ls3(SafeInt<10>::fromConst<tmpReg3And4Offset>())();
    as_.INSTR(ADD_xD_xN_imm12zxols12).setD(REG::SP).setN(REG::SP).setImm12zx(SafeUInt<12U>::fromConst<stackSize>())();
    tempRegManagerForIdentifier.recoverTempGPRs();
    break;
  }
  case BuiltinFunction::UNDEFINED:
  // GCOVR_EXCL_START
  default: {
    UNREACHABLE(break, "Unknown BuiltinFunction");
  }
    // GCOVR_EXCL_STOP
  }
}
#endif

void Backend::emitMemcpyWithConstSizeNoBoundsCheck(REG const dstReg, REG const srcReg, REG const sizeReg, uint32_t const sizeToCopy,
                                                   REG const gpScratchReg, REG const floatScratchReg, REG const floatScratchReg2,
                                                   bool const canOverlap) const {
  RelPatchObj reverse{};
  if (canOverlap) {
    as_.INSTR(CMP_xN_xM).setN(srcReg).setM(dstReg)();
    reverse = as_.prepareJMP(CC::LO);
  }
  // when src >= dst, we copy from begin to end

  uint32_t const copy16Count{sizeToCopy / 16U};
  uint32_t const copy8Count{(sizeToCopy % 16U) / 8U};
  uint32_t const copy1ByteCount{sizeToCopy % 8U};
  constexpr uint32_t unrollingThreshold{2U};
  // If not unrolling 1 byte copy, should prepare sizeReg
  bool const unrollingCopy1Byte{copy1ByteCount <= unrollingThreshold};

  if (copy16Count <= unrollingThreshold) {
    for (uint32_t i{0U}; i < copy16Count; ++i) {
      as_.INSTR(LDP_dT1_dT2_deref_xN_scSImm7_postidx_t)
          .setT1(floatScratchReg)
          .setT2(floatScratchReg2)
          .setN(srcReg)
          .setSImm7ls3(SafeInt<10>::fromConst<16>())();
      as_.INSTR(STP_dT1_dT2_deref_xN_scSImm7_postidx_t)
          .setT1(floatScratchReg)
          .setT2(floatScratchReg2)
          .setN(dstReg)
          .setSImm7ls3(SafeInt<10>::fromConst<16>())();
    }
    if (!unrollingCopy1Byte) {
      // prepare size reg
      uint32_t const maxRange{sizeToCopy - copy1ByteCount};
      if (maxRange != 0U) {
        assert(SignedInRangeCheck<12U>::check(bit_cast<int32_t>(maxRange)).inRange());
        as_.INSTR(SUBS_wD_wN_imm12zxols12)
            .setD(sizeReg)
            .setN(sizeReg)
            .setImm12zx(SafeUInt<12U>::fromUnsafe(maxRange))(); // Subtract 16 * copy16Count from size
      }
    }
  } else {
    // Temporarily subtract so we can efficiently compare to 0 (optimization)
    as_.INSTR(SUBS_wD_wN_imm12zxols12).setD(sizeReg).setN(sizeReg).setImm12zx(SafeUInt<12U>::fromConst<16U>())();
    // Check if (remaining) size is at least 16
    RelPatchObj const lessThan16Forward{as_.prepareJMP(CC::MI)}; // Jump back if positive or zero, 16 are remaining anyway (optimization)
    // Copy 16 bytes
    uint32_t const copy16Forward{output_.size()};
    // TODO(SIMD): Do once SIMD is implemented and use LDP/STP for 16-byte SIMD registers (Copy 32 bytes in one
    // iteration).
    as_.INSTR(LDP_dT1_dT2_deref_xN_scSImm7_postidx_t)
        .setT1(floatScratchReg)
        .setT2(floatScratchReg2)
        .setN(srcReg)
        .setSImm7ls3(SafeInt<10>::fromConst<16>())();
    as_.INSTR(SUBS_wD_wN_imm12zxols12).setD(sizeReg).setN(sizeReg).setImm12zx(SafeUInt<12U>::fromConst<16U>())();
    as_.INSTR(STP_dT1_dT2_deref_xN_scSImm7_postidx_t)
        .setT1(floatScratchReg)
        .setT2(floatScratchReg2)
        .setN(dstReg)
        .setSImm7ls3(SafeInt<10>::fromConst<16>())();
    as_.prepareJMP(CC::PL).linkToBinaryPos(copy16Forward); // Jump back if positive or zero, 16 are remaining anyway (optimization)
    lessThan16Forward.linkToHere();
    if (!unrollingCopy1Byte) {
      // Add again (optimization)
      // prepare size reg
      uint32_t const added{16U - (copy8Count * 8U)};
      as_.INSTR(ADD_wD_wN_imm12zxols12).setD(sizeReg).setN(sizeReg).setImm12zx(SafeUInt<12U>::fromUnsafe(added))();
    }
  }

  if (copy8Count == 1U) {
    as_.INSTR(LDR_xT_deref_xN_unscSImm9_postidx).setT(gpScratchReg).setN(srcReg).setUnscSImm9(SafeInt<9U>::fromConst<8>())();
    as_.INSTR(STR_xT_deref_xN_unscSImm9_postidx).setT(gpScratchReg).setN(dstReg).setUnscSImm9(SafeInt<9U>::fromConst<8>())();
  }

  RelPatchObj finishedForward{};
  if (unrollingCopy1Byte) {
    for (uint32_t i{0U}; i < copy1ByteCount; ++i) {
      as_.INSTR(LDRB_wT_deref_xN_unscSImm9_postidx).setT(gpScratchReg).setN(srcReg).setUnscSImm9(SafeInt<9U>::fromConst<1>())();
      as_.INSTR(STRB_wT_deref_xN_unscSImm9_postidx).setT(gpScratchReg).setN(dstReg).setUnscSImm9(SafeInt<9U>::fromConst<1>())();
    }
    finishedForward = as_.prepareJMP();
  } else {
    // Check if (remaining) size is at least 1
    finishedForward = as_.prepareJMPIfRegIsZero(sizeReg, false);
    // Copy 1 byte
    uint32_t const copy1Forward{output_.size()};
    as_.INSTR(LDRB_wT_deref_xN_unscSImm9_postidx).setT(gpScratchReg).setN(srcReg).setUnscSImm9(SafeInt<9U>::fromConst<1>())();
    as_.INSTR(SUB_wD_wN_imm12zxols12).setD(sizeReg).setN(sizeReg).setImm12zx(SafeUInt<12U>::fromConst<1U>())(); // optimize instruction scheduling
    as_.INSTR(STRB_wT_deref_xN_unscSImm9_postidx).setT(gpScratchReg).setN(dstReg).setUnscSImm9(SafeInt<9U>::fromConst<1>())();
    as_.prepareJMPIfRegIsNotZero(sizeReg, false).linkToBinaryPos(copy1Forward);
  }

  if (canOverlap) {
    RelPatchObj const finished2Forward{as_.prepareJMP()};
    reverse.linkToHere();
    // when src < dst, we copy from end to begin
    as_.INSTR(ADD_xD_xN_xMolsImm6).setD(srcReg).setN(srcReg).setM(sizeReg)();
    as_.INSTR(ADD_xD_xN_xMolsImm6).setD(dstReg).setN(dstReg).setM(sizeReg)();

    if (copy16Count <= unrollingThreshold) {
      for (uint32_t i{0U}; i < copy16Count; ++i) {
        as_.INSTR(LDP_dT1_dT2_deref_xN_scSImm7_preidx_t)
            .setT1(floatScratchReg)
            .setT2(floatScratchReg2)
            .setN(srcReg)
            .setSImm7ls3(SafeInt<10U>::fromConst<-16>())();
        as_.INSTR(STP_dT1_dT2_deref_xN_scSImm7_preidx_t)
            .setT1(floatScratchReg)
            .setT2(floatScratchReg2)
            .setN(dstReg)
            .setSImm7ls3(SafeInt<10U>::fromConst<-16>())();
      }
      if (!unrollingCopy1Byte) {
        // prepare size reg
        uint32_t const maxRange{sizeToCopy - copy1ByteCount};
        if (maxRange != 0U) {
          assert(SignedInRangeCheck<12U>::check(bit_cast<int32_t>(maxRange)).inRange());
          as_.INSTR(SUBS_wD_wN_imm12zxols12).setD(sizeReg).setN(sizeReg).setImm12zx(SafeUInt<12U>::fromUnsafe(maxRange))();
        }
      }
    } else {
      // Check if (remaining) size is at least 16
      as_.INSTR(SUBS_wD_wN_imm12zxols12)
          .setD(sizeReg)
          .setN(sizeReg)
          .setImm12zx(SafeUInt<12U>::fromConst<16U>())(); // Temporarily subtract so we can efficiently compare to 0
                                                          // (optimization)
      RelPatchObj const lessThan16InReverse{as_.prepareJMP(CC::MI)};
      // Copy 16 bytes
      uint32_t const copy16InReverse{output_.size()};
      as_.INSTR(LDP_dT1_dT2_deref_xN_scSImm7_preidx_t)
          .setT1(floatScratchReg)
          .setT2(floatScratchReg2)
          .setN(srcReg)
          .setSImm7ls3(SafeInt<10U>::fromConst<-16>())();
      as_.INSTR(SUBS_wD_wN_imm12zxols12).setD(sizeReg).setN(sizeReg).setImm12zx(SafeUInt<12U>::fromConst<16U>())();
      as_.INSTR(STP_dT1_dT2_deref_xN_scSImm7_preidx_t)
          .setT1(floatScratchReg)
          .setT2(floatScratchReg2)
          .setN(dstReg)
          .setSImm7ls3(SafeInt<10U>::fromConst<-16>())();
      as_.prepareJMP(CC::PL).linkToBinaryPos(copy16InReverse); // Jump back if positive or zero, 16 are remaining anyway (optimization)
      lessThan16InReverse.linkToHere();
      if (!unrollingCopy1Byte) {
        // Add again (optimization)
        // prepare size reg
        uint32_t const added{16U - (copy8Count * 8U)};
        as_.INSTR(ADD_wD_wN_imm12zxols12).setD(sizeReg).setN(sizeReg).setImm12zx(SafeUInt<12U>::fromUnsafe(added))();
      }
    }

    if (copy8Count == 1U) {
      as_.INSTR(LDR_xT_deref_xN_unscSImm9_preidx).setT(gpScratchReg).setN(srcReg).setUnscSImm9(SafeInt<9U>::fromConst<-8>())();
      as_.INSTR(STR_xT_deref_xN_unscSImm9_preidx).setT(gpScratchReg).setN(dstReg).setUnscSImm9(SafeInt<9U>::fromConst<-8>())();
    }

    if (unrollingCopy1Byte) {
      for (uint32_t i{0U}; i < copy1ByteCount; ++i) {
        as_.INSTR(LDRB_wT_deref_xN_unscSImm9_preidx).setT(gpScratchReg).setN(srcReg).setUnscSImm9(SafeInt<9U>::fromConst<-1>())();
        as_.INSTR(STRB_wT_deref_xN_unscSImm9_preidx).setT(gpScratchReg).setN(dstReg).setUnscSImm9(SafeInt<9U>::fromConst<-1>())();
      }
    } else {
      // Check if (remaining) size is at least 1
      RelPatchObj const finishedInReverse{as_.prepareJMPIfRegIsZero(sizeReg, false)};
      // Copy 1 byte
      uint32_t const copy1InReverse{output_.size()};
      as_.INSTR(LDRB_wT_deref_xN_unscSImm9_preidx).setT(gpScratchReg).setN(srcReg).setUnscSImm9(SafeInt<9U>::fromConst<-1>())();
      as_.INSTR(SUB_wD_wN_imm12zxols12).setD(sizeReg).setN(sizeReg).setImm12zx(SafeUInt<12U>::fromConst<1U>())(); // optimize instruction scheduling
      as_.INSTR(STRB_wT_deref_xN_unscSImm9_preidx).setT(gpScratchReg).setN(dstReg).setUnscSImm9(SafeInt<9U>::fromConst<-1>())();
      as_.prepareJMPIfRegIsNotZero(sizeReg, false).linkToBinaryPos(copy1InReverse);
      finishedInReverse.linkToHere();
    }
    finished2Forward.linkToHere();
  }

  finishedForward.linkToHere();
}
void Backend::emitMemcpyNoBoundsCheck(REG const dstReg, REG const srcReg, REG const sizeReg, REG const gpScratchReg, REG const floatScratchReg,
                                      REG const floatScratchReg2, bool const canOverlap) const {
  RelPatchObj reverse{};
  if (canOverlap) {
    as_.INSTR(CMP_xN_xM).setN(srcReg).setM(dstReg)();
    reverse = as_.prepareJMP(CC::LO);
  }
  // when src >= dst, we copy from begin to end
  as_.INSTR(SUBS_wD_wN_imm12zxols12)
      .setD(sizeReg)
      .setN(sizeReg)
      .setImm12zx(SafeUInt<12U>::fromConst<16U>())(); // Temporarily subtract so we can efficiently compare to 0 (optimization)
  // Check if (remaining) size is at least 16
  RelPatchObj const lessThan16Forward{as_.prepareJMP(CC::MI)}; // Jump back if positive or zero, 16 are remaining anyway (optimization)
  // Copy 16 bytes
  uint32_t const copy16Forward{output_.size()};
  // TODO(SIMD): Do once SIMD is implemented and use LDP/STP for 16-byte SIMD registers (Copy 32 bytes in one
  // iteration).
  as_.INSTR(LDP_dT1_dT2_deref_xN_scSImm7_postidx_t)
      .setT1(floatScratchReg)
      .setT2(floatScratchReg2)
      .setN(srcReg)
      .setSImm7ls3(SafeInt<10>::fromConst<16>())();
  as_.INSTR(SUBS_wD_wN_imm12zxols12).setD(sizeReg).setN(sizeReg).setImm12zx(SafeUInt<12U>::fromConst<16U>())();
  as_.INSTR(STP_dT1_dT2_deref_xN_scSImm7_postidx_t)
      .setT1(floatScratchReg)
      .setT2(floatScratchReg2)
      .setN(dstReg)
      .setSImm7ls3(SafeInt<10>::fromConst<16>())();
  as_.prepareJMP(CC::PL).linkToBinaryPos(copy16Forward); // Jump back if positive or zero, 16 are remaining anyway (optimization)
  lessThan16Forward.linkToHere();
  as_.INSTR(ADD_wD_wN_imm12zxols12).setD(sizeReg).setN(sizeReg).setImm12zx(SafeUInt<12U>::fromConst<16U>())(); // Add again (optimization)
  // Check if (remaining) size is at least 1
  RelPatchObj const finishedForward{as_.prepareJMPIfRegIsZero(sizeReg, false)};
  // Copy 1 byte
  uint32_t const copy1Forward{output_.size()};
  as_.INSTR(LDRB_wT_deref_xN_unscSImm9_postidx).setT(gpScratchReg).setN(srcReg).setUnscSImm9(SafeInt<9U>::fromConst<1>())();
  as_.INSTR(SUB_wD_wN_imm12zxols12).setD(sizeReg).setN(sizeReg).setImm12zx(SafeUInt<12U>::fromConst<1U>())(); // optimize instruction scheduling
  as_.INSTR(STRB_wT_deref_xN_unscSImm9_postidx).setT(gpScratchReg).setN(dstReg).setUnscSImm9(SafeInt<9U>::fromConst<1>())();
  as_.prepareJMPIfRegIsNotZero(sizeReg, false).linkToBinaryPos(copy1Forward);
  if (canOverlap) {
    RelPatchObj const finished2Forward{as_.prepareJMP()};
    reverse.linkToHere();
    // when src < dst, we copy from end to begin
    as_.INSTR(ADD_xD_xN_xMolsImm6).setD(srcReg).setN(srcReg).setM(sizeReg)();
    as_.INSTR(ADD_xD_xN_xMolsImm6).setD(dstReg).setN(dstReg).setM(sizeReg)();
    // Check if (remaining) size is at least 16
    as_.INSTR(SUBS_wD_wN_imm12zxols12)
        .setD(sizeReg)
        .setN(sizeReg)
        .setImm12zx(SafeUInt<12U>::fromConst<16U>())(); // Temporarily subtract so we can efficiently compare to 0
                                                        // (optimization)
    RelPatchObj const lessThan16InReverse{as_.prepareJMP(CC::MI)};
    // Copy 16 bytes
    uint32_t const copy16InReverse{output_.size()};
    as_.INSTR(LDP_dT1_dT2_deref_xN_scSImm7_preidx_t)
        .setT1(floatScratchReg)
        .setT2(floatScratchReg2)
        .setN(srcReg)
        .setSImm7ls3(SafeInt<10>::fromConst<-16>())();
    as_.INSTR(SUBS_wD_wN_imm12zxols12).setD(sizeReg).setN(sizeReg).setImm12zx(SafeUInt<12U>::fromConst<16U>())();
    as_.INSTR(STP_dT1_dT2_deref_xN_scSImm7_preidx_t)
        .setT1(floatScratchReg)
        .setT2(floatScratchReg2)
        .setN(dstReg)
        .setSImm7ls3(SafeInt<10>::fromConst<-16>())();
    as_.prepareJMP(CC::PL).linkToBinaryPos(copy16InReverse); // Jump back if positive or zero, 16 are remaining anyway (optimization)
    lessThan16InReverse.linkToHere();
    as_.INSTR(ADD_wD_wN_imm12zxols12)
        .setD(sizeReg)
        .setN(sizeReg)
        .setImm12zx(SafeUInt<12U>::fromConst<16U>())(); // Add again (optimization)
                                                        // Check if (remaining) size is at least 1
    RelPatchObj const finishedInReverse{as_.prepareJMPIfRegIsZero(sizeReg, false)};
    // Copy 1 byte
    uint32_t const copy1InReverse{output_.size()};
    as_.INSTR(LDRB_wT_deref_xN_unscSImm9_preidx).setT(gpScratchReg).setN(srcReg).setUnscSImm9(SafeInt<9U>::fromConst<-1>())();
    as_.INSTR(SUB_wD_wN_imm12zxols12).setD(sizeReg).setN(sizeReg).setImm12zx(SafeUInt<12U>::fromConst<1U>())(); // optimize instruction scheduling
    as_.INSTR(STRB_wT_deref_xN_unscSImm9_preidx).setT(gpScratchReg).setN(dstReg).setUnscSImm9(SafeInt<9U>::fromConst<-1>())();
    as_.prepareJMPIfRegIsNotZero(sizeReg, false).linkToBinaryPos(copy1InReverse);

    finishedInReverse.linkToHere();
    finished2Forward.linkToHere();
  }
  finishedForward.linkToHere();
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
      RelPatchObj const relPatchObj{RelPatchObj(position, output_)};
      position = relPatchObj.getLinkedBinaryPos();
      relPatchObj.linkToHere();
      if (position == relPatchObj.getPosOffsetBeforeInstr()) {
        break;
      }
    }
  }
}

void Backend::registerPendingBranch(RelPatchObj const &branchObj, uint32_t &linkVariable) {
  branchObj.linkToBinaryPos((linkVariable == 0xFF'FF'FF'FFU) ? branchObj.getPosOffsetBeforeInstr() : linkVariable);

  // We store the current position (the last branch) in the link variable; position before branch instruction is stored
  linkVariable = branchObj.getPosOffsetBeforeInstr();
}

///
/// @brief Returns a copy of an AbstrInstr with the commutative flag set to true
///
/// @param abstrInstr AbstrInstr to modify so it is marked as mutative
/// @return AbstrInstr Modified AbstrInstr
static constexpr AbstrInstr makeCommutative(AbstrInstr abstrInstr) VB_NOEXCEPT {
  abstrInstr.src_0_1_commutative = true;
  return abstrInstr;
}
bool Backend::emitComparison(OPCode const opcode, StackElement const *const arg0Ptr, StackElement const *const arg1Ptr) {
  moduleInfo_.lastBC = BCforOPCode(opcode);
  switch (opcode) {
  case OPCode::I32_EQZ: {
    StackElement const dummyElement{StackElement::i32Const(0_U32)};
    return emitInstruction(make_array(makeCommutative(CMP_wN_imm12zxols12)), arg0Ptr, &dummyElement, nullptr, RegMask::none(), false).reversed;
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
    return emitInstruction(make_array(makeCommutative(CMP_wN_imm12zxols12), makeCommutative(CMP_wN_wM)), arg0Ptr, arg1Ptr, nullptr, RegMask::none(),
                           false)
        .reversed;
  }
  case OPCode::I64_EQZ: {
    StackElement const dummyElement{StackElement::i64Const(0_U64)};

    return emitInstruction(make_array(makeCommutative(CMP_xN_imm12zxols12)), arg0Ptr, &dummyElement, nullptr, RegMask::none(), false).reversed;
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
    return emitInstruction(make_array(makeCommutative(CMP_xN_imm12zxols12), makeCommutative(CMP_xN_xM)), arg0Ptr, arg1Ptr, nullptr, RegMask::none(),
                           false)
        .reversed;
  }
  case OPCode::F32_EQ:
  case OPCode::F32_NE:
  case OPCode::F32_LT:
  case OPCode::F32_GT:
  case OPCode::F32_LE:
  case OPCode::F32_GE: {
    return emitInstruction(make_array(makeCommutative(FCMP_sN_sM)), arg0Ptr, arg1Ptr, nullptr, RegMask::none(), false).reversed;
  }
  case OPCode::F64_EQ:
  case OPCode::F64_NE:
  case OPCode::F64_LT:
  case OPCode::F64_GT:
  case OPCode::F64_LE:
  case OPCode::F64_GE: {
    return emitInstruction(make_array(makeCommutative(FCMP_dN_dM)), arg0Ptr, arg1Ptr, nullptr, RegMask::none(), false).reversed;
  }
  // GCOVR_EXCL_START
  default: {
    UNREACHABLE(break, "Unknown OPCode");
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
    } else {
      // Block or IFBlock
      registerPendingBranch(relPatchObj, blockElement->data.blockInfo.binaryPosition.lastBlockBranch);
    }
  };

  // Helper to read last instruction, check if it was CMP wN, #0 and CC is EQ or NE, then replace it with CBZ and CBNZ
  // instead of CMP + B.cond
  // coverity[autosar_cpp14_a8_5_2_violation]
  auto const prepareCondJMPAndMergeWithCMPIfPossible = [this](CC const jmpCC) -> RelPatchObj {
    if ((jmpCC == CC::EQ) || (jmpCC == CC::NE)) {
      bool patched{false};
      Assembler::patchInstructionAtOffset(
          // coverity[autosar_cpp14_a5_1_8_violation]
          output_, output_.size() - 4U, FunctionRef<void(Instruction &)>([&patched, jmpCC](Instruction &instr) VB_NOEXCEPT {
            Instruction copy{instr};
            if (copy.clearN().getOPCode() == CMP_wN_imm12zxols12.opcode) {
              REG const originalReg{instr.getN()};
              assert(jmpCC == CC::EQ || jmpCC == CC::NE);
              static_cast<void>(instr.resetOPCode((jmpCC == CC::EQ) ? CBZ_wT_imm19sxls2_t : CBNZ_wT_imm19sxls2_t).setT(originalReg));
              patched = true;
            }
          }));
      if (patched) {
        return RelPatchObj(output_.size() - 4U, output_);
      } // CBZ or CBNZ is already emitted in this case
    }
    return as_.prepareJMP(jmpCC);
  };

  CC const positiveCC{isNegative ? negateCC(CCforBC(branchCond)) : CCforBC(branchCond)};
  if (targetBlockElem != nullptr) {
    // Targeting a block, loop or ifblock
    if ((branchCond == BC::UNCONDITIONAL) || (moduleInfo_.fnc.stackFrameSize == targetBlockElem->data.blockInfo.entryStackFrameSize)) {
      // Either unconditional or no-op anyway
      as_.setStackFrameSize(targetBlockElem->data.blockInfo.entryStackFrameSize, true);
      // coverity[autosar_cpp14_a4_5_1_violation]
      RelPatchObj const branchObj{prepareCondJMPAndMergeWithCMPIfPossible(positiveCC)};
      linkBranchToBlock(branchObj, targetBlockElem);
    } else {
      // coverity[autosar_cpp14_a4_5_1_violation]
      RelPatchObj const conditionRelPatchObj{prepareCondJMPAndMergeWithCMPIfPossible(negateCC(positiveCC))};
      as_.setStackFrameSize(targetBlockElem->data.blockInfo.entryStackFrameSize, true);
      RelPatchObj const branchObj{as_.prepareJMP()};
      conditionRelPatchObj.linkToHere();

      linkBranchToBlock(branchObj, targetBlockElem);
    }
  } else {
    // Targeting the function
    if (branchCond == BC::UNCONDITIONAL) {
      emitReturnAndUnwindStack(true);
    } else {
      // Negated condition -> jump over
      // coverity[autosar_cpp14_a4_5_1_violation]
      RelPatchObj const relPatchObj{prepareCondJMPAndMergeWithCMPIfPossible(negateCC(positiveCC))};
      emitReturnAndUnwindStack(true);
      relPatchObj.linkToHere();
    }
  }
}

Backend::ActionResult Backend::emitInstruction(Span<AbstrInstr const> const &instructions, StackElement const *const arg0,
                                               StackElement const *const arg1, StackElement const *const targetHint, RegMask const protRegs,
                                               bool const presFlags) VB_THROW {
  MachineType const dstType{Assembler::getMachineTypeFromArgType(instructions[0].dstType)};
  std::array<VariableStorage, 2U> inputStorages{{
      (arg0 != nullptr) ? moduleInfo_.getStorage(*arg0) : VariableStorage{},
      (arg1 != nullptr) ? moduleInfo_.getStorage(*arg1) : VariableStorage{},
  }};
  std::array<bool const, 2> const startedAsWritableScratchReg{{isWritableScratchReg(arg0), isWritableScratchReg(arg1)}};

  Assembler::ActionResult const assemblerResult{
      as_.selectInstr(instructions, inputStorages, startedAsWritableScratchReg, targetHint, protRegs, presFlags)};

  ActionResult backendResult{};
  backendResult.reversed = assemblerResult.reversed;
  if ((targetHint != nullptr) && assemblerResult.storage.inSameLocation(moduleInfo_.getStorage(*targetHint))) {
    // target hint is used as result, to avoid breaking StackElement linked list, we should return StackElement of target hint here.
    backendResult.element = common_.getResultStackElement(targetHint, dstType);
  } else {
    if (assemblerResult.storage.type == StorageType::INVALID) {
      backendResult.element = StackElement::invalid();
    } else {
      assert(assemblerResult.storage.type == StorageType::REGISTER && "Invalid storage type");
      backendResult.element = StackElement::scratchReg(assemblerResult.storage.location.reg, MachineTypeUtil::toStackTypeFlag(dstType));
    }
  }
  return backendResult;
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
  REG const scratchReg{common_.reqScratchRegProt(MachineType::I64, regAllocTracker, false).reg};

  // Saturate indexReg to numBranchTargets
  as_.MOVimm32(scratchReg, numBranchTargets);
  as_.INSTR(CMP_wN_wM).setN(indexReg).setM(scratchReg)();
  as_.INSTR(CSELcondh_wD_wN_wM_t).setCond(false, CC::CC).setD(indexReg).setN(indexReg).setM(scratchReg)();

  RelPatchObj const loadTableStart{as_.prepareADR(scratchReg)};
  // scratchReg now points to table start, now load delta from table start to indexReg by accessing table
  as_.INSTR(LDR_wT_deref_xN_xMls2_t).setD(indexReg).setN(scratchReg).setM(indexReg)();
  // scratchReg now points to instruction to execute
  as_.INSTR(ADD_xD_xN_xMolsImm6).setD(scratchReg).setN(scratchReg).setM(indexReg)();
  as_.INSTR(BR_xN_t).setN(scratchReg)();

  loadTableStart.linkToHere();
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
  // No stack fence check needed because it will always make the stack frame smaller
  as_.setStackFrameSize(moduleInfo_.fnc.paramWidth, temporary, true);
  as_.INSTR(RET_xN_t).setN(REG::LR)();
}

#if !LINEAR_MEMORY_BOUNDS_CHECKS
void Backend::emitLandingPad() {
  moduleInfo_.helperFunctionBinaryPositions.landingPad = output_.size();

  constexpr uint32_t lrWidth{8U};
  constexpr uint32_t spillSize{roundUpToPow2(static_cast<uint32_t>((NABI::volRegs.size()) * 8U) + lrWidth, 4U)};

  // Reserve space on stack and spill all volatile registers since we will call a native function
  as_.INSTR(SUB_xD_xN_imm12zxols12).setD(REG::SP).setN(REG::SP).setImm12zx(SafeUInt<12U>::fromConst<spillSize>())();
#if ACTIVE_STACK_OVERFLOW_CHECK
  as_.checkStackFence(WasmABI::REGS::landingPadHelper); // SP change
#endif
  as_.INSTR(STR_xT_deref_xN_imm12zxls3_t)
      .setT(REG::LR)
      .setN(REG::SP)
      .setImm12zxls3(SafeUInt<15U>::fromConst<static_cast<uint32_t>(NABI::volRegs.size()) * 8U>())();
  spillRestoreRegsRaw({NABI::volRegs.data(), NABI::volRegs.size()});

#if (MAX_WASM_STACKSIZE_BEFORE_NATIVE_CALL != 0 && !ACTIVE_STACK_OVERFLOW_CHECK) ||                                                                  \
    (defined(STACKSIZE_LEFT_BEFORE_NATIVE_CALL) && ACTIVE_STACK_OVERFLOW_CHECK)
  constexpr REG scratchReg{NABI::volRegs[0]};
  as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
      .setT(scratchReg)
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::nativeStackFence>())();
  as_.INSTR(CMP_SP_xM_t).setM(scratchReg)();
  as_.cTRAP(TrapCode::STACKFENCEBREACHED, CC::LS);
#endif

  // Call the target of the landing pad, stack pointer on AArch64 is always 16-byte aligned
  as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
      .setT(NABI::gpParams[0])
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::landingPadTarget>())();
  as_.INSTR(BLR_xN_t).setN(NABI::gpParams[0])();

  // Restore the link register and all other previously spilled registers, then unwind the stack
  as_.INSTR(LDR_xT_deref_xN_imm12zxls3_t)
      .setT(REG::LR)
      .setN(REG::SP)
      .setImm12zxls3(SafeUInt<15U>::fromConst<static_cast<uint32_t>(NABI::volRegs.size()) * 8U>())();
  spillRestoreRegsRaw({NABI::volRegs.data(), NABI::volRegs.size()}, true);
  as_.INSTR(ADD_xD_xN_imm12zxols12).setD(REG::SP).setN(REG::SP).setImm12zx(SafeUInt<12U>::fromConst<spillSize>())();

  // Return to proper address via reserved register
  as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
      .setT(WasmABI::REGS::landingPadHelper)
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::landingPadRet>())();
  as_.INSTR(RET_xN_t).setN(WasmABI::REGS::landingPadHelper)();
}
#endif

#if LINEAR_MEMORY_BOUNDS_CHECKS
void Backend::emitExtensionRequestFunction() {
  moduleInfo_.helperFunctionBinaryPositions.extensionRequest = output_.size();

  // Properly check whether the address is actually in bounds. The quick check that has been performed before this only
  // checked whether it is in bounds, but accessing the last 8 bytes would fail Add the 8 bytes to the cache register so
  // we get the actual memory size
  as_.INSTR(ADD_xD_xN_imm12zxols12).setD(WasmABI::REGS::memSize).setN(WasmABI::REGS::memSize).setImm12zx(SafeUInt<12U>::fromConst<8_U32>())();
  as_.INSTR(CMP_xN_xM).setN(WasmABI::REGS::memSize).setM(NABI::gpParams[0])();
  RelPatchObj const withinBounds = as_.prepareJMP(CC::GE);

  //
  //

  // Reserve space on stack and spill all volatile registers since we will call a native function
  uint32_t const lrWidth = 8U;
  uint32_t const spillSize = roundUpToPow2((static_cast<uint32_t>(NABI::volRegs.size()) * 8U) + lrWidth, 4U);
  as_.INSTR(SUB_xD_xN_imm12zxols12).setD(REG::SP).setN(REG::SP).setImm12zx(SafeUInt<12U>::fromConst<spillSize>())();
#if ACTIVE_STACK_OVERFLOW_CHECK
  // We can use REGS::memSize as scratch register since it will be clobbered and re-setup anyway
  as_.checkStackFence(WasmABI::REGS::memSize); // SP change
#endif
  as_.INSTR(STR_xT_deref_xN_imm12zxls3_t)
      .setT(REG::LR)
      .setN(REG::SP)
      .setImm12zxls3(SafeUInt<15>::fromConst<static_cast<uint32_t>(NABI::volRegs.size()) * 8U>())();
  spillRestoreRegsRaw({NABI::volRegs.data(), NABI::volRegs.size()});

  // Load the other arguments for the extension helper, the accessed address is already in the first register
  uint32_t const basedataLength = moduleInfo_.getBasedataLength();
  as_.MOVimm32(NABI::gpParams[1], basedataLength);
  as_.INSTR(MOV_xD_xM_t).setD(NABI::gpParams[2]).setM(WasmABI::REGS::linMem)();

  // Call extension request
  static_assert(sizeof(uintptr_t) <= 8, "uintptr_t datatype too large"); //
  as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
      .setT(NABI::gpParams[3])
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::memoryHelperPtr>())();
  as_.INSTR(BLR_xN_t).setN(NABI::gpParams[3])();

  // Check the return value. If it's zero extension of memory failed
  as_.INSTR(CMP_xN_xM).setN(NABI::gpRetReg).setM(REG::ZR)();
  as_.cTRAP(TrapCode::LINMEM_COULDNOTEXTEND, CC::EQ);

  // Check if the return value is all ones: In this case the module tried to access memory beyond the allowed number of
  // (Wasm) pages
  as_.INSTR(CMN_xN_imm12zxols12_t).setN(NABI::gpRetReg).setImm12zx(SafeUInt<12>::fromConst<1U>())(); // cmn r0, 1 <=> cmp r0, -1
  as_.cTRAP(TrapCode::LINMEM_OUTOFBOUNDSACCESS, CC::EQ);

  // If all succeeded, the return value now points to the start of the job memory
  as_.INSTR(MOV_xD_xM_t).setD(WasmABI::REGS::jobMem).setM(NABI::gpRetReg)();

  // Calculate the new base of the linear memory by adding basedataLength to the new memory base and store it in
  // REGS::linMem
  UnsignedInRangeCheck<12U> const rangeCheckSize = UnsignedInRangeCheck<12U>::check(basedataLength);
  if (rangeCheckSize.inRange()) {
    as_.INSTR(ADD_xD_xN_imm12zxols12).setD(WasmABI::REGS::linMem).setN(NABI::gpRetReg).setImm12zx(rangeCheckSize.safeInt())();
  } else {
    as_.MOVimm32(WasmABI::REGS::linMem, basedataLength);
    as_.INSTR(ADD_xD_xN_xMolsImm6).setD(WasmABI::REGS::linMem).setN(WasmABI::REGS::linMem).setM(NABI::gpRetReg)();
  }

  // Restore the link register and all other previously spilled registers, then unwind the stack
  as_.INSTR(LDR_xT_deref_xN_imm12zxls3_t)
      .setT(REG::LR)
      .setN(REG::SP)
      .setImm12zxls3(SafeUInt<15>::fromConst<static_cast<uint32_t>(NABI::volRegs.size()) * 8U>())();
  spillRestoreRegsRaw({NABI::volRegs.data(), NABI::volRegs.size()}, true);
  as_.INSTR(ADD_xD_xN_imm12zxols12).setD(REG::SP).setN(REG::SP).setImm12zx(SafeUInt<12U>::fromConst<spillSize>())();

  // Load the actual memory size, maybe it changed
  as_.INSTR(LDUR_wT_deref_xN_unscSImm9_t)
      .setT(WasmABI::REGS::memSize)
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::actualLinMemByteSize>())();

  //
  //

  withinBounds.linkToHere();

  // Set up the register for the cached memory size again and then return
  setupMemSizeReg();
  as_.INSTR(RET_xN_t).setN(REG::LR)();
}
#endif

Common::LiftedReg Backend::prepareLinMemAddrProt(StackElement *const addrElem, uint32_t const offset, RegAllocTracker &regAllocTracker,
                                                 StackElement const *const targetHint) {
  Common::LiftedReg const liftedReg{common_.liftToRegInPlaceProt(*addrElem, false, targetHint, regAllocTracker)};
  if (offset == 0U) {
    return liftedReg;
  }
  // Add offset
  if (liftedReg.writable) {
    as_.addImmToReg(liftedReg.reg, static_cast<int64_t>(offset), true, regAllocTracker.readWriteFutureLiftMask());
    return liftedReg;
  } else {
    REG const reg{common_.reqScratchRegProt(MachineType::I64, targetHint, regAllocTracker, false).reg};
    as_.addImmToReg(reg, liftedReg.reg, static_cast<int64_t>(offset), true);
    return {reg, true};
  }
}

#if LINEAR_MEMORY_BOUNDS_CHECKS
void Backend::emitLinMemBoundsCheck(REG const addrReg, uint8_t const memObjSize) {
  assert(moduleInfo_.helperFunctionBinaryPositions.extensionRequest != 0xFF'FF'FF'FF && "Extension request wrapper has not been produced yet");
  assert(moduleInfo_.fnc.stackFrameSize == as_.alignStackFrameSize(moduleInfo_.fnc.stackFrameSize) && "Stack not aligned");

  as_.INSTR(CMP_xN_xM).setN(WasmABI::REGS::memSize).setM(addrReg)();
  RelPatchObj const withinBounds = as_.prepareJMP(CC::GE);

  as_.INSTR(STP_xT1_xT2_deref_xN_scSImm7_t)
      .setT1(REG::LR)
      .setT2(NABI::gpParams[0])
      .setN(WasmABI::REGS::linMem)
      .setSImm7ls3(SafeInt<10>::fromConst<-BD::FromEnd::spillRegion>())();
  as_.INSTR(ADD_xD_xN_imm12zxols12)
      .setD(NABI::gpParams[0])
      .setN(addrReg)
      .setImm12zx(SafeUInt<12U>::max() & static_cast<uint32_t>(memObjSize))(); // Move to gpParams[0] and add memObjSize
  RelPatchObj const extensionRequestRelPatchObj = as_.INSTR(BL_imm26sxls2_t).setImm19o26ls2BranchPlaceHolder().prepJmp();
  extensionRequestRelPatchObj.linkToBinaryPos(moduleInfo_.helperFunctionBinaryPositions.extensionRequest); // CALL extension request or trap
  as_.INSTR(LDP_xT1_xT2_deref_xN_scSImm7_t)
      .setT1(REG::LR)
      .setT2(NABI::gpParams[0])
      .setN(WasmABI::REGS::linMem)
      .setSImm7ls3(SafeInt<10>::fromConst<-BD::FromEnd::spillRegion>())();
  withinBounds.linkToHere();
}
#endif

StackElement Backend::executeLinearMemoryLoad(OPCode const opcode, uint32_t const offset, Stack::iterator const addrElem,
                                              StackElement const *const targetHint) {
  assert(moduleInfo_.hasMemory && "Memory not defined");
  // coverity[autosar_cpp14_a8_5_2_violation]
  constexpr auto memObjSizes = make_array(4_U8, 8_U8, 4_U8, 8_U8, 1_U8, 1_U8, 2_U8, 2_U8, 1_U8, 1_U8, 2_U8, 2_U8, 4_U8, 4_U8);

  MachineType const resultType{getLoadResultType(opcode)};
  bool const resultIsInt{MachineTypeUtil::isInt(resultType)};
  // coverity[autosar_cpp14_a8_5_2_violation]
  constexpr auto opcodeTemplates =
      make_array(LDR_wT_deref_xN_xM_t, LDR_xT_deref_xN_xM_t, LDR_sT_deref_xN_xM_t, LDR_dT_deref_xN_xM_t, LDRSB_wT_deref_xN_xM_t,
                 LDRB_wT_deref_xN_xM_t, LDRSH_wT_deref_xN_xM_t, LDRH_wT_deref_xN_xM_t, LDRSB_xT_deref_xN_xM_t, LDRB_wT_deref_xN_xM_t,
                 LDRSH_xT_deref_xN_xM_t, LDRH_wT_deref_xN_xM_t, LDRSW_xT_deref_xN_xM_t, LDR_wT_deref_xN_xM_t);

  OPCodeTemplate const opcodeTemplate{opcodeTemplates[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_LOAD)]};
  static_cast<void>(memObjSizes);

  RegAllocTracker regAllocTracker{};
  // coverity[autosar_cpp14_a5_3_2_violation]
  Common::LiftedReg const liftedAddrReg{prepareLinMemAddrProt(addrElem.raw(), offset, regAllocTracker, targetHint)};
  REG const addrReg{liftedAddrReg.reg};

  StackElement const *const verifiedTargetHint{(getUnderlyingRegIfSuitable(targetHint, resultType, RegMask::none()) != REG::NONE) ? targetHint
                                                                                                                                  : nullptr};

#if LINEAR_MEMORY_BOUNDS_CHECKS
  emitLinMemBoundsCheck(addrReg, memObjSizes[opcode - OPCode::I32_LOAD]);
#endif

  RegElement targetRegElem{};

  REG targetReg{REG::NONE};
  if (verifiedTargetHint != nullptr) {
    VariableStorage const targetStorage{moduleInfo_.getStorage(*verifiedTargetHint)};
    if (targetStorage.type == StorageType::REGISTER) {
      targetReg = targetStorage.location.reg;
    }
  }

  if (resultIsInt && (targetReg == addrReg)) {
    targetRegElem = {common_.getResultStackElement(targetHint, resultType), addrReg};
  } else if (resultIsInt && liftedAddrReg.writable) {
    targetRegElem = {StackElement::scratchReg(liftedAddrReg.reg, MachineTypeUtil::toStackTypeFlag(resultType)), addrReg};
  } else {
    targetRegElem = common_.reqScratchRegProt(resultType, verifiedTargetHint, regAllocTracker, false);
  }
  as_.INSTR(opcodeTemplate).setT(targetRegElem.reg).setN(WasmABI::REGS::linMem).setM(addrReg)();
  return targetRegElem.elem;
}

void Backend::executeLinearMemoryStore(OPCode const opcode, uint32_t const offset) {
  assert(moduleInfo_.hasMemory && "Memory not defined");
  // coverity[autosar_cpp14_a8_5_2_violation]
  constexpr auto memObjSizes = make_array(4_U8, 8_U8, 4_U8, 8_U8, 1_U8, 2_U8, 1_U8, 2_U8, 4_U8);
  uint8_t const memObjSize{memObjSizes[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_STORE)]};
  static_cast<void>(memObjSize);
  Stack::iterator const valueElem{common_.condenseValentBlockBelow(stack_.end())};
  Stack::iterator const addrElem{common_.condenseValentBlockBelow(valueElem)};

  RegAllocTracker regAllocTracker{};
  regAllocTracker.futureLifts = mask(valueElem.unwrap());
  REG const addrReg{prepareLinMemAddrProt(addrElem.unwrap(), offset, regAllocTracker, nullptr).reg};

#if LINEAR_MEMORY_BOUNDS_CHECKS
  emitLinMemBoundsCheck(addrReg, memObjSize);
#elif !EAGER_ALLOCATION
  // Probe first because memory accesses crossing page boundaries with different permissions are UNPREDICTABLE on ARM
  // If EAGER_ALLOCATION is turned on, the whole formal size is guaranteed to be read-write accessible already.
  if (memObjSize > 1U) {
    // coverity[autosar_cpp14_a8_5_2_violation]
    constexpr auto probeTemplates =
        make_array(LDR_wT_deref_xN_xM_t, LDR_xT_deref_xN_xM_t, LDR_wT_deref_xN_xM_t, LDR_xT_deref_xN_xM_t, LDRB_wT_deref_xN_xM_t,
                   LDRH_wT_deref_xN_xM_t, LDRB_wT_deref_xN_xM_t, LDRH_wT_deref_xN_xM_t, LDR_wT_deref_xN_xM_t);
    as_.INSTR(probeTemplates[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_STORE)])
        .setT(REG::ZR)
        .setN(WasmABI::REGS::linMem)
        .setM(addrReg)();
  }
#endif
  if (valueElem->isConstantZero()) {
    constexpr REG valueReg{REG::ZR};
    // coverity[autosar_cpp14_a8_5_2_violation]
    constexpr auto opcodeTemplates =
        make_array(STR_wT_deref_xN_xM_t, STR_xT_deref_xN_xM_t, STR_wT_deref_xN_xM_t, STR_xT_deref_xN_xM_t, STRB_wT_deref_xN_xM_t,
                   STRH_wT_deref_xN_xM_t, STRB_wT_deref_xN_xM_t, STRH_wT_deref_xN_xM_t, STR_wT_deref_xN_xM_t);
    as_.INSTR(opcodeTemplates[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_STORE)])
        .setT(valueReg)
        .setN(WasmABI::REGS::linMem)
        .setM(addrReg)();
  } else {
    REG const valueReg{common_.liftToRegInPlaceProt(*valueElem, false, regAllocTracker).reg};
    // coverity[autosar_cpp14_a8_5_2_violation]
    constexpr auto opcodeTemplates =
        make_array(STR_wT_deref_xN_xM_t, STR_xT_deref_xN_xM_t, STR_sT_deref_xN_xM_t, STR_dT_deref_xN_xM_t, STRB_wT_deref_xN_xM_t,
                   STRH_wT_deref_xN_xM_t, STRB_wT_deref_xN_xM_t, STRH_wT_deref_xN_xM_t, STR_wT_deref_xN_xM_t);
    as_.INSTR(opcodeTemplates[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_STORE)])
        .setT(valueReg)
        .setN(WasmABI::REGS::linMem)
        .setM(addrReg)();
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
  REG const floatScratchReg{common_.reqScratchRegProt(MachineType::F64, regAllocTracker, false).reg};
  REG const floatScratchReg2{common_.reqScratchRegProt(MachineType::F64, regAllocTracker, false).reg};

  // if src + size is larger then the length of mem.data then trap
  // if dst + size is larger then the length of mem.data then trap
  // can be combined
  // max(src, dst) + size is larger then the length of mem.data then trap
#if LINEAR_MEMORY_BOUNDS_CHECKS
  as_.INSTR(CMP_wN_wM).setN(srcReg).setM(dstReg)();
  as_.INSTR(CSELcondh_wD_wN_wM_t).setCond(false, CC::HI).setD(gpScratchReg).setN(srcReg).setM(dstReg)();
  as_.INSTR(ADD_xD_xN_xMolsImm6).setD(gpScratchReg).setN(gpScratchReg).setM(sizeReg)();

  emitLinMemBoundsCheck(gpScratchReg, 0U);

  as_.INSTR(ADD_xD_xN_xMolsImm6).setD(srcReg).setN(srcReg).setM(WasmABI::REGS::linMem)();
  as_.INSTR(ADD_xD_xN_xMolsImm6).setD(dstReg).setN(dstReg).setM(WasmABI::REGS::linMem)();
#else
  as_.INSTR(ADD_xD_xN_xMolsImm6).setD(srcReg).setN(srcReg).setM(WasmABI::REGS::linMem)();
  as_.INSTR(ADD_xD_xN_xMolsImm6).setD(dstReg).setN(dstReg).setM(WasmABI::REGS::linMem)();

  as_.INSTR(CMP_xN_xM).setN(srcReg).setM(dstReg)();
  as_.INSTR(CSELcondh_xD_xN_xM_t).setCond(false, CC::HI).setD(gpScratchReg).setN(srcReg).setM(dstReg)();
  as_.INSTR(ADD_xD_xN_xMolsImm6).setD(gpScratchReg).setN(gpScratchReg).setM(sizeReg)();

  as_.INSTR(LDRB_wT_deref_xN_unscSImm9_preidx).setT(REG::ZR).setN(gpScratchReg).setUnscSImm9(SafeInt<9U>::fromConst<-1>())();
#endif
  constexpr bool canOverlap{true};
  if (sizeIsConstant) {
    emitMemcpyWithConstSizeNoBoundsCheck(dstReg, srcReg, sizeReg, sizeValue, gpScratchReg, floatScratchReg, floatScratchReg2, canOverlap);
  } else {
    emitMemcpyNoBoundsCheck(dstReg, srcReg, sizeReg, gpScratchReg, floatScratchReg, floatScratchReg2, canOverlap);
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
  // when value is 0, we don't need to rewrite the value.
  REG const valueReg{value->isConstantZero() ? REG::ZR : common_.liftToRegInPlaceProt(*value, true, regAllocTracker).reg};
  REG const dstReg{common_.liftToRegInPlaceProt(*dst, true, regAllocTracker).reg};
  REG const scratchReg{common_.reqScratchRegProt(MachineType::I64, regAllocTracker, false).reg};

  common_.removeReference(size);
  common_.removeReference(value);
  common_.removeReference(dst);
  static_cast<void>(stack_.erase(size));
  static_cast<void>(stack_.erase(value));
  static_cast<void>(stack_.erase(dst));

#if LINEAR_MEMORY_BOUNDS_CHECKS
  as_.INSTR(ADD_xD_xN_xMolsImm6).setD(scratchReg).setN(dstReg).setM(sizeReg)();
  emitLinMemBoundsCheck(scratchReg, 0U);
  as_.INSTR(ADD_xD_xN_xMolsImm6).setD(dstReg).setN(dstReg).setM(WasmABI::REGS::linMem)();
#else
  as_.INSTR(ADD_xD_xN_xMolsImm6).setD(dstReg).setN(dstReg).setM(WasmABI::REGS::linMem)();
  as_.INSTR(ADD_xD_xN_xMolsImm6).setD(scratchReg).setN(dstReg).setM(sizeReg)();
  as_.INSTR(LDRB_wT_deref_xN_unscSImm9_preidx).setT(REG::ZR).setN(scratchReg).setUnscSImm9(SafeInt<9>::fromConst<-1>())();
#endif

  as_.INSTR(SUBS_wD_wN_imm12zxols12)
      .setD(sizeReg)
      .setN(sizeReg)
      .setImm12zx(SafeUInt<12>::fromConst<16>())(); // Temporarily subtract so we can efficiently compare to 0 (optimization)
  // Check if (remaining) size is at least 16
  RelPatchObj const lessThan16Forward{as_.prepareJMP(CC::MI)}; // Jump back if positive or zero, 16 are remaining anyway (optimization)
  // prepare data
  if (valueReg != REG::ZR) {
    as_.INSTR(AND_wD_wN_imm12bitmask).setD(valueReg).setN(valueReg).setImmBitmask(0xFF_U64)();
    as_.MOVimm64(scratchReg, 0x01010101'01010101_U64);
    as_.INSTR(MUL_xD_xN_xM).setD(valueReg).setN(valueReg).setM(scratchReg)();
  }
  // set 16 bytes
  uint32_t const fill16Forward{output_.size()};
  as_.INSTR(SUBS_wD_wN_imm12zxols12).setD(sizeReg).setN(sizeReg).setImm12zx(SafeUInt<12>::fromConst<16>())();
  as_.INSTR(STP_xT1_xT2_deref_xN_scSImm7_postidx_t).setT1(valueReg).setT2(valueReg).setN(dstReg).setSImm7ls3(SafeInt<10>::fromConst<16>())();
  as_.prepareJMP(CC::PL).linkToBinaryPos(fill16Forward); // Jump back if positive or zero, 16 are remaining anyway (optimization)
  lessThan16Forward.linkToHere();
  as_.INSTR(ADD_wD_wN_imm12zxols12).setD(sizeReg).setN(sizeReg).setImm12zx(SafeUInt<12>::fromConst<16>())(); // Add again (optimization)
  // Check if (remaining) size is at least 1
  RelPatchObj const finished{as_.prepareJMPIfRegIsZero(sizeReg, false)};
  // set 1 byte
  uint32_t const copy1Forward{output_.size()};
  as_.INSTR(SUB_wD_wN_imm12zxols12).setD(sizeReg).setN(sizeReg).setImm12zx(SafeUInt<12>::fromConst<1>())(); // optimize instruction scheduling
  as_.INSTR(STRB_wT_deref_xN_unscSImm9_postidx).setT(valueReg).setN(dstReg).setUnscSImm9(SafeInt<9>::fromConst<1>())();
  as_.prepareJMPIfRegIsNotZero(sizeReg, false).linkToBinaryPos(copy1Forward);

  finished.linkToHere();
}

// Loads the current "Wasm" memory size into a scratch register (i32) and
// pushes it onto the stack
void Backend::executeGetMemSize() const {
  assert(moduleInfo_.hasMemory && "No memory defined");

  RegAllocTracker regAllocTracker{};
  RegElement const targetRegElem{common_.reqScratchRegProt(MachineType::I32, regAllocTracker, false)};
  as_.INSTR(LDUR_wT_deref_xN_unscSImm9_t)
      .setT(targetRegElem.reg)
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::linMemWasmSize>())();
  common_.pushAndUpdateReference(targetRegElem.elem);
}

// Condenses the upmost valent block on the stack, validates its type, pops it,
// adds its value to the memory size and pushes the resulting memory size as an
// i32 scratch register onto the stack
void Backend::executeMemGrow() {
  assert(moduleInfo_.hasMemory && "No memory defined");

  Stack::iterator const deltaElement{common_.condenseValentBlockBelow(stack_.end())};

  RegAllocTracker regAllocTracker{};
  RegElement gpOutputRegElem{common_.reqScratchRegProt(MachineType::I32, regAllocTracker, false)};
  as_.INSTR(LDUR_wT_deref_xN_unscSImm9_t)
      .setT(gpOutputRegElem.reg)
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::linMemWasmSize>())();

  // coverity[autosar_cpp14_a8_5_2_violation]
  constexpr auto ops = make_array(ADDS_wD_wN_imm12zxols12, ADDS_wD_wN_wM);

  regAllocTracker = RegAllocTracker();
  gpOutputRegElem.elem = emitInstruction(ops, &gpOutputRegElem.elem, deltaElement.unwrap(), nullptr, RegMask::none(), false).element;
  gpOutputRegElem.reg =
      common_.liftToRegInPlaceProt(gpOutputRegElem.elem, true, regAllocTracker).reg; // Let's make absolutely sure it's in a register

  RelPatchObj const error{as_.prepareJMP(CC::CS)};
  RelPatchObj noError{};
  if (moduleInfo_.memoryHasSizeLimit) {
    StackElement const constElem{StackElement::i32Const(moduleInfo_.memoryMaximumSize)};
    bool const reversed{emitComparison(OPCode::I32_LE_U, &gpOutputRegElem.elem, &constElem)};
    noError = as_.prepareJMP(reversed ? CC::HS : CC::LS);
  } else {
    as_.INSTR(CMP_wN_imm12zxols12).setN(gpOutputRegElem.reg).setImm12zxls12(SafeUInt<24U>::fromConst<(1_U32 << 16_U32)>())();
    noError = as_.prepareJMP(CC::LS);
  }

  error.linkToHere();
  as_.MOVimm32(gpOutputRegElem.reg, 0xFF'FF'FF'FFU);
  RelPatchObj const toEnd{as_.prepareJMP()};

  noError.linkToHere();
  REG const intermReg{common_.reqScratchRegProt(MachineType::I32, regAllocTracker, false).reg};

// Let's use intermReg as a scratch register here
#if !LINEAR_MEMORY_BOUNDS_CHECKS
  // Notify the allocator of the memory growth
  constexpr uint32_t lrWidth{8U};
  constexpr uint32_t spillSize{roundUpToPow2(static_cast<uint32_t>((NABI::volRegs.size()) * 8U) + lrWidth, 4U)};

  // Reserve space on stack and spill all volatile registers since we will call a native function
  as_.INSTR(SUB_xD_xN_imm12zxols12).setD(REG::SP).setN(REG::SP).setImm12zx(SafeUInt<12>::fromConst<spillSize>())();
#if ACTIVE_STACK_OVERFLOW_CHECK
  as_.checkStackFence(intermReg); // SP change
#endif
  as_.INSTR(STR_xT_deref_xN_imm12zxls3_t)
      .setT(REG::LR)
      .setN(REG::SP)
      .setImm12zxls3(SafeUInt<15>::fromConst<static_cast<uint32_t>(NABI::volRegs.size()) * 8U>())();
  spillRestoreRegsRaw({NABI::volRegs.data(), NABI::volRegs.size()});
  // now NABI::volRegs is usable
  // Load the arguments for the call (in this order because gpOutputRegElem.reg could be one of the gpParams)
  as_.INSTR(MOV_xD_xM_t).setD(NABI::gpParams[1]).setM(gpOutputRegElem.reg)();
  as_.INSTR(MOV_xD_xM_t).setD(NABI::gpParams[0]).setM(WasmABI::REGS::linMem)();

  // Call memory helper request
  static_assert(sizeof(uintptr_t) <= 8, "uintptr_t datatype too large");
  as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
      .setT(NABI::gpParams[2])
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::memoryHelperPtr>())();
  as_.INSTR(BLR_xN_t).setN(NABI::gpParams[2])();

  // Check return value
  as_.INSTR(CMP_wN_wM).setN(NABI::gpRetReg).setM(REG::ZR)();
  as_.cTRAP(TrapCode::LINMEM_COULDNOTEXTEND, CC::EQ);

  // Restore the link register and all other previously spilled registers, then unwind the stack
  as_.INSTR(LDR_xT_deref_xN_imm12zxls3_t)
      .setT(REG::LR)
      .setN(REG::SP)
      .setImm12zxls3(SafeUInt<15>::fromConst<static_cast<uint32_t>(NABI::volRegs.size()) * 8U>())();
  spillRestoreRegsRaw({NABI::volRegs.data(), NABI::volRegs.size()}, true);
  as_.INSTR(ADD_xD_xN_imm12zxols12).setD(REG::SP).setN(REG::SP).setImm12zx(SafeUInt<12>::fromConst<spillSize>())();
#endif

  as_.INSTR(LDUR_wT_deref_xN_unscSImm9_t)
      .setT(intermReg)
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::linMemWasmSize>())();
  as_.INSTR(STUR_wT_deref_xN_unscSImm9_t)
      .setT(gpOutputRegElem.reg)
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::linMemWasmSize>())();
  as_.INSTR(MOV_wD_wM_t).setD(gpOutputRegElem.reg).setM(intermReg)();

  toEnd.linkToHere();
  common_.replaceAndUpdateReference(deltaElement, gpOutputRegElem.elem);
}

StackElement Backend::emitSelect(StackElement const &truthyResult, StackElement const &falsyResult, StackElement &condElem,
                                 StackElement const *const targetHint) {
  MachineType const resultWasmType{moduleInfo_.getMachineType(&truthyResult)};
  bool const is64{MachineTypeUtil::is64(resultWasmType)};
  bool const isInt{MachineTypeUtil::isInt(resultWasmType)};

  RegAllocTracker regAllocTracker{};
  regAllocTracker.futureLifts = mask(&falsyResult) | mask(&truthyResult);
  REG const condReg{common_.liftToRegInPlaceProt(condElem, false, targetHint, regAllocTracker).reg};
  as_.INSTR(CMP_wN_imm12zxols12).setN(condReg).setImm12zx(SafeUInt<12U>::fromConst<0U>())();
  AbstrInstr instruction{};
  if (isInt) {
    instruction = is64 ? CSELeq_xD_xN_xM_t : CSELeq_wD_wN_wM_t;
  } else {
    instruction = is64 ? FCSELeq_dD_dN_dM_t : FCSELeq_sD_sN_sM_t;
  }

  // coverity[autosar_cpp14_a8_5_2_violation]
  auto const instructions = make_array(instruction);
  return emitInstruction(instructions, &falsyResult, &truthyResult, targetHint, RegMask::none(), true).element;
}

StackElement Backend::emitCmpResult(BC const branchCond, StackElement const *const targetHint) const {
  assert(((moduleInfo_.lastBC == branchCond) || (moduleInfo_.lastBC == negateBC(branchCond)) || (moduleInfo_.lastBC == reverseBC(branchCond)) ||
          (branchCond == BC::UNCONDITIONAL)) &&
         "BranchCondition not matching");

  CC const cc{negateCC(CCforBC(branchCond))}; // CSET_wD use invert cond
  REG const targetHintReg{getUnderlyingRegIfSuitable(targetHint, MachineType::I32, RegMask::none())};
  if (targetHintReg != REG::NONE) {
    as_.INSTR(CSET_wD).setD(targetHintReg).setCond(false, cc)();
    return common_.getResultStackElement(targetHint, MachineType::I32);
  } else {
    RegAllocTracker regAllocTracker{};
    RegElement const targetRegElem{common_.reqScratchRegProt(MachineType::I32, regAllocTracker, true)};
    as_.INSTR(CSET_wD).setD(targetRegElem.reg).setCond(false, cc)();
    return targetRegElem.elem;
  }
}

StackElement Backend::emitDeferredAction(OPCode const opcode, StackElement *const arg0Ptr, StackElement *const arg1Ptr,
                                         StackElement const *const targetHint) {
  if ((opcode >= OPCode::I32_EQZ) && (opcode <= OPCode::F64_GE)) {
    bool const reversed{emitComparison(opcode, arg0Ptr, arg1Ptr)};
    BC const condition{reversed ? reverseBC(BCforOPCode(opcode)) : BCforOPCode(opcode)};
    StackElement const result{emitCmpResult(condition, targetHint)};
    return result;
  } else {
    switch (opcode) {
    case OPCode::I32_CLZ: {
      StackElement const result{emitInstruction(make_array(CLZ_wD_wN), arg0Ptr, nullptr, targetHint, RegMask::none(), false).element};
      return result;
    }
    case OPCode::I32_CTZ: {
      StackElement const intermElem{emitInstruction(make_array(RBIT_wD_wN), arg0Ptr, nullptr, targetHint, RegMask::none(), false).element};
      StackElement const result{emitInstruction(make_array(CLZ_wD_wN), &intermElem, nullptr, targetHint, RegMask::none(), false).element};
      return result;
    }
    case OPCode::I32_POPCNT: {
      StackElement const result{emitInstrsPopcnt(arg0Ptr, targetHint, false)};
      return result;
    }

    case OPCode::I32_ADD:
    case OPCode::I32_SUB: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops =
          make_array(make_array(ADD_wD_wN_imm12zxols12, ADD_wD_wN_wMolsImm6), make_array(SUB_wD_wN_imm12zxols12, SUB_wD_wN_wMolsImm6));
      StackElement const result{emitInstruction(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_ADD)], arg0Ptr, arg1Ptr,
                                                targetHint, RegMask::none(), false)
                                    .element};
      return result;
    }
    case OPCode::I32_MUL: {
      StackElement const result{emitInstruction(make_array(MUL_wD_wN_wM), arg0Ptr, arg1Ptr, targetHint, RegMask::none(), false).element};
      return result;
    }

    case OPCode::I32_DIV_S:
    case OPCode::I32_DIV_U:
    case OPCode::I32_REM_S:
    case OPCode::I32_REM_U: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto isDiv = make_array(true, true, false, false);
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto isSigned = make_array(true, false, true, false);
      uint32_t const opIdx{static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_DIV_S)};
      StackElement const result{emitInstrsDivRem(arg0Ptr, arg1Ptr, targetHint, isSigned[opIdx], false, isDiv[opIdx])};
      return result;
    }
    case OPCode::I32_AND:
    case OPCode::I32_OR:
    case OPCode::I32_XOR: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(make_array(AND_wD_wN_imm12bitmask, AND_wD_wN_wM), make_array(ORR_wD_wN_imm12bitmask, ORR_wD_wN_wM),
                                      make_array(EOR_wD_wN_imm12bitmask, EOR_wD_wN_wM));
      StackElement const result{emitInstruction(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_AND)], arg0Ptr, arg1Ptr,
                                                targetHint, RegMask::none(), false)
                                    .element};
      return result;
    }
    case OPCode::I32_SHL:
    case OPCode::I32_SHR_S:
    case OPCode::I32_SHR_U: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops =
          make_array(make_array(LSL_wD_wN_imm6x, LSL_wD_wN_wM), make_array(ASR_wD_wN_imm6x, ASR_wD_wN_wM), make_array(LSR_wD_wN_imm6x, LSR_wD_wN_wM));
      StackElement const result{emitInstruction(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_SHL)], arg0Ptr, arg1Ptr,
                                                targetHint, RegMask::none(), false)
                                    .element};
      return result;
    }
    case OPCode::I32_ROTL:
    case OPCode::I32_ROTR: {
      StackElement const result{emitInstrsRot(arg0Ptr, arg1Ptr, targetHint, false, opcode == OPCode::I32_ROTL)};
      return result;
    }

    case OPCode::I64_CLZ: {
      StackElement const result{emitInstruction(make_array(CLZ_xD_xN), arg0Ptr, nullptr, targetHint, RegMask::none(), false).element};
      return result;
    }
    case OPCode::I64_CTZ: {
      StackElement const intermElem{emitInstruction(make_array(RBIT_xD_xN), arg0Ptr, nullptr, targetHint, RegMask::none(), false).element};
      StackElement const result{emitInstruction(make_array(CLZ_xD_xN), &intermElem, nullptr, targetHint, RegMask::none(), false).element};
      return result;
    }
    case OPCode::I64_POPCNT: {
      StackElement const result{emitInstrsPopcnt(arg0Ptr, targetHint, true)};
      return result;
    }
    case OPCode::I64_ADD:
    case OPCode::I64_SUB: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops =
          make_array(make_array(ADD_xD_xN_imm12zxols12, ADD_xD_xN_xMolsImm6), make_array(SUB_xD_xN_imm12zxols12, SUB_xD_xN_xMolsImm6));
      StackElement const result{emitInstruction(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_ADD)], arg0Ptr, arg1Ptr,
                                                targetHint, RegMask::none(), false)
                                    .element};
      return result;
    }
    case OPCode::I64_MUL: {
      StackElement const result{emitInstruction(make_array(MUL_xD_xN_xM), arg0Ptr, arg1Ptr, targetHint, RegMask::none(), false).element};
      return result;
    }

    case OPCode::I64_DIV_S:
    case OPCode::I64_DIV_U:
    case OPCode::I64_REM_S:
    case OPCode::I64_REM_U: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto isDiv = make_array(true, true, false, false);
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto isSigned = make_array(true, false, true, false);
      uint32_t const opIdx{static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_DIV_S)};
      StackElement const result{emitInstrsDivRem(arg0Ptr, arg1Ptr, targetHint, isSigned[opIdx], true, isDiv[opIdx])};
      return result;
    }
    case OPCode::I64_AND:
    case OPCode::I64_OR:
    case OPCode::I64_XOR: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(make_array(AND_xD_xN_imm13bitmask, AND_xD_xN_xM), make_array(ORR_xD_xN_imm13bitmask, ORR_xD_xN_xM),
                                      make_array(EOR_xD_xN_imm13bitmask, EOR_xD_xN_xM));
      StackElement const result{emitInstruction(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_AND)], arg0Ptr, arg1Ptr,
                                                targetHint, RegMask::none(), false)
                                    .element};
      return result;
    }
    case OPCode::I64_SHL:
    case OPCode::I64_SHR_S:
    case OPCode::I64_SHR_U: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops =
          make_array(make_array(LSL_xD_xN_imm6x, LSL_xD_xN_xM), make_array(ASR_xD_xN_imm6x, ASR_xD_xN_xM), make_array(LSR_xD_xN_imm6x, LSR_xD_xN_xM));
      StackElement const result{emitInstruction(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_SHL)], arg0Ptr, arg1Ptr,
                                                targetHint, RegMask::none(), false)
                                    .element};
      return result;
    }
    case OPCode::I64_ROTL:
    case OPCode::I64_ROTR: {
      StackElement const result{emitInstrsRot(arg0Ptr, arg1Ptr, targetHint, true, opcode == OPCode::I64_ROTL)};
      return result;
    }

    case OPCode::F32_ABS:
    case OPCode::F32_NEG:
    case OPCode::F32_CEIL:
    case OPCode::F32_FLOOR:
    case OPCode::F32_TRUNC:
    case OPCode::F32_NEAREST:
    case OPCode::F32_SQRT: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(make_array(FABS_sD_sN), make_array(FNEG_sD_sN), make_array(FRINTP_sD_sN), make_array(FRINTM_sD_sN),
                                      make_array(FRINTZ_sD_sN), make_array(FRINTN_sD_sN), make_array(FSQRT_sD_sN));
      StackElement const result{emitInstruction(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::F32_ABS)], arg0Ptr, nullptr,
                                                targetHint, RegMask::none(), false)
                                    .element};
      return result;
    }
    case OPCode::F32_ADD:
    case OPCode::F32_SUB:
    case OPCode::F32_MUL:
    case OPCode::F32_DIV:
    case OPCode::F32_MIN:
    case OPCode::F32_MAX: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(make_array(FADD_sD_sN_sM), make_array(FSUB_sD_sN_sM), make_array(FMUL_sD_sN_sM), make_array(FDIV_sD_sN_sM),
                                      make_array(FMIN_sD_sN_sM), make_array(FMAX_sD_sN_sM));
      StackElement const result{emitInstruction(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::F32_ADD)], arg0Ptr, arg1Ptr,
                                                targetHint, RegMask::none(), false)
                                    .element};
      return result;
    }
    case OPCode::F32_COPYSIGN: {
      StackElement const result{emitInstrsCopySign(arg0Ptr, arg1Ptr, targetHint, false)};
      return result;
    }

    case OPCode::F64_ABS:
    case OPCode::F64_NEG:
    case OPCode::F64_CEIL:
    case OPCode::F64_FLOOR:
    case OPCode::F64_TRUNC:
    case OPCode::F64_NEAREST:
    case OPCode::F64_SQRT: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(make_array(FABS_dD_dN), make_array(FNEG_dD_dN), make_array(FRINTP_dD_dN), make_array(FRINTM_dD_dN),
                                      make_array(FRINTZ_dD_dN), make_array(FRINTN_dD_dN), make_array(FSQRT_dD_dN));
      StackElement const result{emitInstruction(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::F64_ABS)], arg0Ptr, nullptr,
                                                targetHint, RegMask::none(), false)
                                    .element};
      return result;
    }
    case OPCode::F64_ADD:
    case OPCode::F64_SUB:
    case OPCode::F64_MUL:
    case OPCode::F64_DIV:
    case OPCode::F64_MIN:
    case OPCode::F64_MAX: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(make_array(FADD_dD_dN_dM), make_array(FSUB_dD_dN_dM), make_array(FMUL_dD_dN_dM), make_array(FDIV_dD_dN_dM),
                                      make_array(FMIN_dD_dN_dM), make_array(FMAX_dD_dN_dM));
      StackElement const result{emitInstruction(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::F64_ADD)], arg0Ptr, arg1Ptr,
                                                targetHint, RegMask::none(), false)
                                    .element};
      return result;
    }
    case OPCode::F64_COPYSIGN: {
      StackElement const result{emitInstrsCopySign(arg0Ptr, arg1Ptr, targetHint, true)};
      return result;
    }

    case OPCode::I32_WRAP_I64: {
      // Needed so emitMove doesn't break the strict aliasing rule by accessing arg->u32
      if (arg0Ptr->type == (StackType::CONSTANT_I64)) {
        return StackElement::i32Const(static_cast<uint32_t>(arg0Ptr->data.constUnion.u64));
      } else {
        // coverity[autosar_cpp14_a8_5_2_violation]
        auto const getTargetElement = [this, targetHint, arg0Ptr]() -> StackElement {
          if (targetHint != nullptr) {
            VariableStorage const targetHintStorage{moduleInfo_.getStorage(*targetHint)};
            if (MachineTypeUtil::isInt(targetHintStorage.machineType) && (targetHintStorage.type == StorageType::REGISTER)) {
              return common_.getResultStackElement(targetHint, MachineType::I32);
            }
          }
          if (isWritableScratchReg(arg0Ptr)) {
            return StackElement::scratchReg(arg0Ptr->data.variableData.location.reg, StackType::I32);
          }
          if (targetHint != nullptr) {
            VariableStorage const targetHintStorage{moduleInfo_.getStorage(*targetHint)};
            if (MachineTypeUtil::isInt(targetHintStorage.machineType)) {
              return common_.getResultStackElement(targetHint, MachineType::I32);
            }
          }
          RegAllocTracker regAllocTracker{};
          regAllocTracker.readProtRegs = mask(arg0Ptr);
          return common_.reqScratchRegProt(MachineType::I32, regAllocTracker, false).elem;
        };

        StackElement const targetElem{getTargetElement()};

        VariableStorage targetStorage{moduleInfo_.getStorage(targetElem)};
        VariableStorage sourceStorage{moduleInfo_.getStorage(*arg0Ptr)};
        sourceStorage.machineType = MachineType::I32;      // "Reinterpret", since source is larger than dest (and if reg, both are
                                                           // GPR), we can safely read from source
        if (targetStorage.type == StorageType::REGISTER) { // X -> Reg
          targetStorage.machineType = MachineType::I32;    // "Reinterpret" to mov i32_reg
          emitMoveIntImpl(targetStorage, sourceStorage, true);
        } else {
          if (targetStorage.machineType == MachineType::I64) {
            targetStorage.machineType = MachineType::I64;
          } else {
            targetStorage.machineType = MachineType::I32;
          }
          TempRegManager tempRegManager{*this};
          if (targetStorage.inMemory() && sourceStorage.inMemory()) { // Mem ->Mem

            Backend::RegDisp<14U> const srcRegDisp{getMemRegDisp<14U>(sourceStorage, tempRegManager)};

            as_.INSTR(LDR_sT_deref_xN_imm12zxls2_t).setT(WasmABI::REGS::moveHelper).setN(srcRegDisp.reg).setImm12zxls2(srcRegDisp.disp)();
            if (MachineTypeUtil::is64(targetStorage.machineType)) {
              Backend::RegDisp<15U> const dstRegDisp{getMemRegDisp<15U>(targetStorage, tempRegManager)};
              as_.INSTR(STR_dT_deref_xN_imm12zxls3_t).setT(WasmABI::REGS::moveHelper).setN(dstRegDisp.reg).setImm12zxls3(dstRegDisp.disp)();
            } else {
              Backend::RegDisp<14U> const dstRegDisp{getMemRegDisp<14U>(targetStorage, tempRegManager)};
              as_.INSTR(STR_sT_deref_xN_imm12zxls2_t).setT(WasmABI::REGS::moveHelper).setN(dstRegDisp.reg).setImm12zxls2(dstRegDisp.disp)();
            }

          } else { // Reg/Const -> Mem
            // GCOVR_EXCL_START
            assert(targetStorage.inMemory());
            // GCOVR_EXCL_STOP
            if (targetStorage.machineType == MachineType::I32) {
              emitMoveIntImpl(targetStorage, sourceStorage, true);
            } else {
              if (sourceStorage.type == StorageType::REGISTER) {
                as_.INSTR(FMOV_sD_wN).setD(WasmABI::REGS::moveHelper).setN(sourceStorage.location.reg)();
              } else {
                emitMoveFloatImpl(VariableStorage::reg(MachineType::F32, WasmABI::REGS::moveHelper),
                                  VariableStorage::f32Const(bit_cast<float>(sourceStorage.location.constUnion.u32)), false);
              }

              Backend::RegDisp<15U> const dstRegDisp{getMemRegDisp<15U>(targetStorage, tempRegManager)};
              as_.INSTR(STR_dT_deref_xN_imm12zxls3_t).setT(WasmABI::REGS::moveHelper).setN(dstRegDisp.reg).setImm12zxls3(dstRegDisp.disp)();
            }
          }
          tempRegManager.recoverTempGPRs();
        }
        return targetElem;
      }
    }

    case OPCode::I32_TRUNC_F32_S:
    case OPCode::I32_TRUNC_F32_U:
    case OPCode::I32_TRUNC_F64_S:
    case OPCode::I32_TRUNC_F64_U: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto srcIs64 = make_array(false, false, true, true);
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto isSigned = make_array(true, false, true, false);
      StackElement const result{
          emitInstrsTruncFloatToInt(arg0Ptr, targetHint, isSigned[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_TRUNC_F32_S)],
                                    srcIs64[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_TRUNC_F32_S)], false)};
      return result;
    }

    case OPCode::I64_EXTEND_I32_S: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(SXTW_xD_wN);
      StackElement const result{emitInstruction(ops, arg0Ptr, nullptr, targetHint, RegMask::none(), false).element};
      return result;
    }
    case OPCode::I64_EXTEND_I32_U: {
      VariableStorage const targetHintStorage{(targetHint != nullptr) ? moduleInfo_.getStorage(*targetHint) : VariableStorage{}};
      VariableStorage const sourceStorage{moduleInfo_.getStorage(*arg0Ptr)};
      if (!targetHintStorage.inSameLocation(sourceStorage)) {
        if (isWritableScratchReg(arg0Ptr)) {
          return StackElement::scratchReg(arg0Ptr->data.variableData.location.reg, StackType::I64);
        } else {
          // coverity[autosar_cpp14_a8_5_2_violation]
          constexpr auto ops = make_array(UXTW_xD_wN);
          StackElement const result{emitInstruction(ops, arg0Ptr, nullptr, targetHint, RegMask::none(), false).element};
          return result;
        }
      } else {
        return common_.getResultStackElement(arg0Ptr, MachineType::I64);
      }
    }
    case OPCode::I32_EXTEND8_S:
    case OPCode::I32_EXTEND16_S:
    case OPCode::I64_EXTEND8_S:
    case OPCode::I64_EXTEND16_S:
    case OPCode::I64_EXTEND32_S: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops =
          make_array(make_array(SXTB_wD_wN), make_array(SXTH_wD_wN), make_array(SXTB_xD_xN), make_array(SXTH_xD_xN), make_array(SXTW_xD_xN));
      StackElement const result{emitInstruction(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_EXTEND8_S)], arg0Ptr, nullptr,
                                                targetHint, RegMask::none(), false)
                                    .element};
      return result;
    }

    case OPCode::I64_TRUNC_F32_S:
    case OPCode::I64_TRUNC_F32_U:
    case OPCode::I64_TRUNC_F64_S:
    case OPCode::I64_TRUNC_F64_U: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto srcIs64 = make_array(false, false, true, true);
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto isSigned = make_array(true, false, true, false);
      StackElement const result{
          emitInstrsTruncFloatToInt(arg0Ptr, targetHint, isSigned[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_TRUNC_F32_S)],
                                    srcIs64[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_TRUNC_F32_S)], true)};
      return result;
    }

    case OPCode::F32_CONVERT_I32_S:
    case OPCode::F32_CONVERT_I32_U:
    case OPCode::F32_CONVERT_I64_S:
    case OPCode::F32_CONVERT_I64_U:
    case OPCode::F32_DEMOTE_F64:
    case OPCode::F64_CONVERT_I32_S:
    case OPCode::F64_CONVERT_I32_U:
    case OPCode::F64_CONVERT_I64_S:
    case OPCode::F64_CONVERT_I64_U:
    case OPCode::F64_PROMOTE_F32: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops =
          make_array(make_array(SCVTF_sD_wN), make_array(UCVTF_sD_wN), make_array(SCVTF_sD_xN), make_array(UCVTF_sD_xN), make_array(FCVT_sD_dN),
                     make_array(SCVTF_dD_wN), make_array(UCVTF_dD_wN), make_array(SCVTF_dD_xN), make_array(UCVTF_dD_xN), make_array(FCVT_dD_sN));
      StackElement const result{emitInstruction(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::F32_CONVERT_I32_S)], arg0Ptr,
                                                nullptr, targetHint, RegMask::none(), false)
                                    .element};
      return result;
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
          UNREACHABLE(break, "Unknown OPCode");
        }
          // GCOVR_EXCL_STOP
        }
      }
      case StorageType::REGISTER: {
        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto ops = make_array(make_array(FMOV_wD_sN), make_array(FMOV_xD_dN), make_array(FMOV_sD_wN), make_array(FMOV_dD_xN));
        StackElement const result{emitInstruction(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_REINTERPRET_F32)], arg0Ptr,
                                                  nullptr, targetHint, RegMask::none(), false)
                                      .element};
        return result;
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
        targetStorage.machineType = dstType; // Reinterpret
        emitMoveImpl(targetStorage, srcStorage, false);
        return targetElem;
      }
      case StorageType::INVALID:
      // GCOVR_EXCL_START
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
  MachineType const srcType{srcIs64 ? MachineType::F64 : MachineType::F32};
  MachineType const dstType{dstIs64 ? MachineType::I64 : MachineType::I32};

  RegAllocTracker regAllocTracker{};
  REG const argReg{common_.liftToRegInPlaceProt(*argPtr, false, regAllocTracker).reg};
  RegElement const gpOutRegElem{common_.reqScratchRegProt(dstType, targetHint, regAllocTracker, false)};
  REG const fHelperReg{common_.reqScratchRegProt(srcType, targetHint, regAllocTracker, false).reg};

  FloatTruncLimitsExcl::RawLimits const rawLimits{FloatTruncLimitsExcl::getRawLimits(isSigned, srcIs64, dstIs64)};

  as_.MOVimm(srcIs64, gpOutRegElem.reg, rawLimits.max);
  as_.INSTR(srcIs64 ? FMOV_dD_xN : FMOV_sD_wN).setD(fHelperReg).setN(gpOutRegElem.reg)();
  as_.INSTR(srcIs64 ? FCMP_dN_dM : FCMP_sN_sM).setN(argReg).setM(fHelperReg)();
  RelPatchObj const maxRelPatchObj{as_.prepareJMP(CC::HS)}; // Greater than, equal or unordered
  as_.MOVimm(srcIs64, gpOutRegElem.reg, rawLimits.min);
  as_.INSTR(srcIs64 ? FMOV_dD_xN : FMOV_sD_wN).setD(fHelperReg).setN(gpOutRegElem.reg)();
  as_.INSTR(srcIs64 ? FCMP_dN_dM : FCMP_sN_sM).setN(argReg).setM(fHelperReg)();
  RelPatchObj const minRelPatchObj{as_.prepareJMP(CC::GT)}; // Less than
  maxRelPatchObj.linkToHere();
  as_.TRAP(TrapCode::TRUNC_OVERFLOW); //   TRAP
  minRelPatchObj.linkToHere();

  // coverity[autosar_cpp14_a8_5_2_violation]
  constexpr auto ops = make_array(make_array(make_array(FCVTZS_xD_dN /* dst64 */, FCVTZS_wD_dN /* dst32 */), // signed, src64
                                             make_array(FCVTZS_xD_sN /* dst64 */, FCVTZS_wD_sN /* dst32 */)  // signed, src32
                                             ),
                                  make_array(make_array(FCVTZU_xD_dN /* dst64 */, FCVTZU_wD_dN /* dst32 */), // unsigned, src64
                                             make_array(FCVTZU_xD_sN /* dst64 */, FCVTZU_wD_sN /* dst32 */)  // unsigned, src32
                                             ));

  const AbstrInstr &op{ops[isSigned ? 0 : 1][srcIs64 ? 0 : 1][dstIs64 ? 0 : 1]};
  as_.INSTR(op).setD(gpOutRegElem.reg).setN(argReg)();
  return gpOutRegElem.elem;
}

StackElement Backend::emitInstrsCopySign(StackElement *const arg0Ptr, StackElement *const arg1Ptr, StackElement const *const targetHint,
                                         bool const is64) const {
  RegAllocTracker regAllocTracker{};
  regAllocTracker.futureLifts = mask(arg1Ptr);
  REG const arg0Reg{common_.liftToRegInPlaceProt(*arg0Ptr, true, targetHint, regAllocTracker).reg};
  REG const arg1Reg{common_.liftToRegInPlaceProt(*arg1Ptr, false, regAllocTracker).reg};
  RegElement const helperRegElem{common_.reqScratchRegProt(MachineType::F64, regAllocTracker, false)};

  if (is64) {
    as_.INSTR(MOVI_vD2d_0_t).setD(helperRegElem.reg)();
    as_.INSTR(FNEG_vD2d_vN2d_t).setD(helperRegElem.reg).setN(helperRegElem.reg)();
  } else {
    as_.INSTR(MOVI_vD4s_128lsl24_t).setD(helperRegElem.reg)();
  }
  as_.INSTR(BIT_vD16b_vN16b_vM16b_t).setD(arg0Reg).setN(arg1Reg).setM(helperRegElem.reg)();
  return *arg0Ptr;
}

StackElement Backend::emitInstrsRot(StackElement *const arg0Ptr, StackElement *const arg1Ptr, StackElement const *const targetHint, bool const is64,
                                    bool const isLeft) {
  if (moduleInfo_.getStorage(*arg1Ptr).type == StorageType::CONSTANT) {
    uint32_t newShift{is64 ? static_cast<uint32_t>(arg1Ptr->data.constUnion.u64) : arg1Ptr->data.constUnion.u32};

    if (isLeft) {
      if (is64) {
        newShift = 64_U32 - (newShift & 0b0011'1111_U32);

      } else {
        newShift = 32_U32 - (newShift & 0b0001'1111_U32);
      }
    }

    RegAllocTracker regAllocTracker{};
    REG const arg0Reg{common_.liftToRegInPlaceProt(*arg0Ptr, false, regAllocTracker).reg};
    RegElement targetRegElem{RegElement{*arg0Ptr, arg0Reg}};
    if (!isWritableScratchReg(arg0Ptr)) {
      targetRegElem = common_.reqScratchRegProt(is64 ? MachineType::I64 : MachineType::I32, targetHint, regAllocTracker, false);
    }

    if (is64) {
      SafeUInt<6U> const safeShift{SafeUInt<6U>::fromConst<0b0011'1111U>() & newShift};
      as_.INSTR(EXTR_xD_xN_xM_imm6_t).setD(targetRegElem.reg).setN(arg0Reg).setM(arg0Reg).setImm6(safeShift)();
    } else {
      SafeUInt<6U> const safeShift{SafeUInt<6U>::fromConst<0b0001'1111U>() & newShift};
      as_.INSTR(EXTR_wD_wN_wM_imm6_t).setD(targetRegElem.reg).setN(arg0Reg).setM(arg0Reg).setImm6(safeShift)();
    }

    return targetRegElem.elem;
  } else {
    if (isLeft) {
      RegAllocTracker regAllocTracker{};
      regAllocTracker.readProtRegs = mask(arg0Ptr);
      REG const negatedCountReg{common_.liftToRegInPlaceProt(*arg1Ptr, true, targetHint, regAllocTracker).reg};
      as_.INSTR(is64 ? SUB_xD_xN_xMolsImm6 : SUB_wD_wN_wMolsImm6).setD(negatedCountReg).setN(REG::ZR).setM(negatedCountReg)();
    }

    // coverity[autosar_cpp14_a8_5_2_violation]
    constexpr auto ops = make_array(make_array(ROR_xD_xN_xM), make_array(ROR_wD_wN_wM));
    return emitInstruction(ops[is64 ? 0 : 1], arg0Ptr, arg1Ptr, targetHint, RegMask::none(), false).element;
  }
}

StackElement Backend::emitInstrsPopcnt(StackElement *const argPtr, StackElement const *const targetHint, bool const is64) const {
  RegAllocTracker regAllocTracker{};
  REG const argReg{common_.liftToRegInPlaceProt(*argPtr, true, targetHint, regAllocTracker).reg};
  REG const intermReg{common_.reqScratchRegProt(is64 ? MachineType::F64 : MachineType::F32, nullptr, regAllocTracker, false).reg};
  as_.INSTR(is64 ? FMOV_dD_xN : FMOV_sD_wN).setD(intermReg).setN(argReg)();
  as_.INSTR(CNT_vD8b_vN8b_t).setD(intermReg).setN(intermReg)();
  as_.INSTR(UADDLV_hD_vN8b_t).setD(intermReg).setN(intermReg)();
  as_.INSTR(FMOV_wD_sN).setD(argReg).setN(intermReg)();
  return *argPtr;
}

StackElement Backend::emitInstrsDivRem(StackElement *const arg0Ptr, StackElement *const arg1Ptr, StackElement const *const targetHint,
                                       bool const isSigned, bool const is64, bool const isDiv) const {
  DivRemAnalysisResult const validationResult{analyzeDivRem(arg0Ptr, arg1Ptr)};
  RegAllocTracker regAllocTracker{};
  regAllocTracker.futureLifts = mask(arg1Ptr);
  REG const arg0Reg{common_.liftToRegInPlaceProt(*arg0Ptr, false, regAllocTracker).reg};
  REG const arg1Reg{common_.liftToRegInPlaceProt(*arg1Ptr, false, regAllocTracker).reg};
  RegElement const helperRegElem{common_.reqScratchRegProt(is64 ? MachineType::I64 : MachineType::I32, targetHint, regAllocTracker, false)};

  uint64_t const maxBitSet{1_U64 << (is64 ? 63_U64 : 31_U64)};

  if (!validationResult.mustNotBeDivZero) {
    // Note:
    // ACTIVE_DIV_CHECK must be enabled in arm64 as sdiv/udiv will not trap with div 0.
    // On arm64, div by 0 will always return 0 unless actively trapped.
    // Reference: https://developer.arm.com/documentation/ddi0602/2025-06/Base-Instructions/SDIV--Signed-divide-?lang=en
    as_.INSTR(is64 ? CMP_xN_imm12zxols12 : CMP_wN_imm12zxols12).setN(arg1Reg).setImm12zx(SafeUInt<12U>::fromConst<0>())();
    as_.cTRAP(TrapCode::DIV_ZERO, CC::EQ);
  }

  // coverity[autosar_cpp14_a8_5_2_violation]
  auto const emitInstrsDivRemCore = [this, isSigned, isDiv, is64, helperReg = helperRegElem.reg, arg0Reg, arg1Reg]() -> void {
    if (isSigned) {
      as_.INSTR(is64 ? SDIV_xD_xN_xM : SDIV_wD_wN_wM).setD(helperReg).setN(arg0Reg).setM(arg1Reg)();
    } else {
      as_.INSTR(is64 ? UDIV_xD_xN_xM : UDIV_wD_wN_wM).setD(helperReg).setN(arg0Reg).setM(arg1Reg)();
    }

    if (!isDiv) {
      as_.INSTR(is64 ? MSUB_xD_xN_xM_xA_t : MSUB_wD_wN_wM_wA_t).setD(helperReg).setN(arg1Reg).setM(helperReg).setA(arg0Reg)();
    }
  };

  if (validationResult.mustNotBeOverflow) {
    emitInstrsDivRemCore();
  } else {
    as_.MOVimm(is64, helperRegElem.reg, maxBitSet);
    as_.INSTR(is64 ? CMP_xN_xM : CMP_wN_wM).setN(arg0Reg).setM(helperRegElem.reg)();
    RelPatchObj const noOverflow{as_.prepareJMP(CC::NE)};
    as_.MOVimm(is64, helperRegElem.reg, ~0_U64);
    as_.INSTR(is64 ? CMP_xN_xM : CMP_wN_wM).setN(arg1Reg).setM(helperRegElem.reg)();
    RelPatchObj const noOverflow2{as_.prepareJMP(CC::NE)};

    if (isDiv && isSigned) {
      as_.TRAP(TrapCode::DIV_OVERFLOW);
    } else {
      as_.MOVimm(is64, helperRegElem.reg, !(isSigned || isDiv) ? maxBitSet : 0_U64);
    }

    RelPatchObj const toEnd{as_.prepareJMP()};
    noOverflow.linkToHere();
    noOverflow2.linkToHere();

    emitInstrsDivRemCore();

    toEnd.linkToHere();
  }

  return helperRegElem.elem;
}

void Backend::setupJobMemRegFromLinMemReg() const {
  as_.MOVimm32(WasmABI::REGS::jobMem, moduleInfo_.getBasedataLength());
  as_.INSTR(SUB_xD_xN_xMolsImm6).setD(WasmABI::REGS::jobMem).setN(WasmABI::REGS::linMem).setM(WasmABI::REGS::jobMem)();
}

void Backend::setupLinMemRegFromJobMemReg() const {
  uint32_t const basedataLength{moduleInfo_.getBasedataLength()};
  if (basedataLength <= 0xFF'FF'FFU) {
    as_.addImm24ToReg(WasmABI::REGS::linMem, static_cast<int32_t>(basedataLength), true, WasmABI::REGS::jobMem);
  } else {
    as_.MOVimm32(WasmABI::REGS::linMem, basedataLength);
    as_.INSTR(ADD_xD_xN_xMolsImm6).setD(WasmABI::REGS::linMem).setN(WasmABI::REGS::linMem).setM(WasmABI::REGS::jobMem)();
  }
}

#if LINEAR_MEMORY_BOUNDS_CHECKS
void Backend::setupMemSizeReg() const {
  // Cache actual linear memory size minus 8 in the first reserved scratch register
  as_.INSTR(LDUR_wT_deref_xN_unscSImm9_t)
      .setT(WasmABI::REGS::memSize)
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::actualLinMemByteSize>())();
  as_.INSTR(SUB_xD_xN_imm12zxols12).setD(WasmABI::REGS::memSize).setN(WasmABI::REGS::memSize).setImm12zx(SafeUInt<12U>::fromConst<8U>())();
}
#endif

void TempRegManager::recoverTempGPRs() {
  // Restore registers
  if (clobberedLinMemReg_) {
    backend_.setupLinMemRegFromJobMemReg();
    clobberedLinMemReg_ = false;
  }
  // Not needed for landingPadHelper because that one can be clobbered
#if LINEAR_MEMORY_BOUNDS_CHECKS
  if (clobberedExtraReg_) {
    backend_.setupMemSizeReg();
    clobberedExtraReg_ = false;
  }
#endif
}

uint32_t Backend::reserveStackFrame(uint32_t const width) {
  uint32_t const newOffset{common_.getCurrentMaximumUsedStackFramePosition() + width};
  assert(newOffset <= moduleInfo_.fnc.stackFrameSize + width);
  if (newOffset > moduleInfo_.fnc.stackFrameSize) {
    uint32_t const newAlignedStackFrameSize{as_.alignStackFrameSize(newOffset + 32U)};
    as_.setStackFrameSize(newAlignedStackFrameSize);

#if ACTIVE_STACK_OVERFLOW_CHECK
    if (moduleInfo_.currentState.checkedStackFrameSize < newAlignedStackFrameSize) {
      moduleInfo_.currentState.checkedStackFrameSize = newAlignedStackFrameSize;
      RegAllocTracker tempRegAllocTracker{};
      REG scratchReg = common_.reqFreeScratchRegProt(MachineType::I32, tempRegAllocTracker);
      bool const haveFreeRegister = scratchReg != REG::NONE;

      static_assert(BD::FromEnd::spillSize >= 8, "Spill region not large enough");
      if (!haveFreeRegister) {
        as_.INSTR(STUR_xT_deref_xN_unscSImm9_t)
            .setT(callScrRegs[0])
            .setN(WasmABI::REGS::linMem)
            .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::spillRegion>())();
        scratchReg = callScrRegs[0];
      }

      as_.checkStackFence(scratchReg); // SP change

      if (!haveFreeRegister) {
        as_.INSTR(LDUR_xT_deref_xN_unscSImm9_t)
            .setT(callScrRegs[0])
            .setN(WasmABI::REGS::linMem)
            .setUnscSImm9(SafeInt<9>::fromConst<-BD::FromEnd::spillRegion>())();
      }
    }
#endif
  }
  return newOffset;
}

// coverity[autosar_cpp14_m0_1_8_violation]
// coverity[autosar_cpp14_m9_3_3_violation]
void Backend::execPadding(uint32_t const paddingSize) const VB_NOEXCEPT {
  assert(paddingSize == 0U);
  static_cast<void>(paddingSize);
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
} // namespace aarch64
} // namespace vb
#endif
