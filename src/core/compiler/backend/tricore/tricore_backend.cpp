///
/// @file tricore_backend.cpp
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
#ifdef JIT_TARGET_TRICORE
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

#include "tricore_assembler.hpp"
#include "tricore_aux.hpp"
#include "tricore_backend.hpp"
#include "tricore_cc.hpp"
#include "tricore_encoding.hpp"
#include "tricore_instruction.hpp"
#include "tricore_relpatchobj.hpp"

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
#include "src/core/compiler/backend/tricore/tricore_call_dispatch.hpp"
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
namespace tc {
using Backend = Tricore_Backend;     ///< Shortcut for Tricore_Backend
using Assembler = Tricore_Assembler; ///< Shortcut for Tricore_Assembler

namespace BD = Basedata;    ///< shortcut of Basedata
namespace NABI = NativeABI; ///< shortcut of NativeABI

Backend::Tricore_Backend(Stack &stack, ModuleInfo &moduleInfo, MemWriter &memory, MemWriter &output, Common &common, Compiler &compiler) VB_NOEXCEPT
    : stack_(stack),
      moduleInfo_(moduleInfo),
      memory_(memory),
      output_(output),
      common_(common),
      compiler_(compiler),
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
    // Only allocate 32-bit integer values to registers, float and 64-bit int calculations are very inefficient anyway
    // so it doesn't make a lot of difference if they need to be loaded from memory first
    if (MachineTypeUtil::getSize(type) == 4U) {
      uint32_t const maxNumLocalsReg{isParam ? WasmABI::regsForParams : moduleInfo_.getMaxNumsLocalsInGPRs()};
      uint32_t const numLocalsInDr{getNumLocalsInDr()};
      if (numLocalsInDr < maxNumLocalsReg) {
        chosenReg = WasmABI::dr[moduleInfo_.getLocalStartIndexInGPRs() + numLocalsInDr];
        increaseNumLocalsInDr();
      }
    }

    ModuleInfo::LocalDef &localDef{moduleInfo_.localDefs[moduleInfo_.fnc.numLocals + i]};
    localDef.reg = chosenReg;
    localDef.type = type;
    if (chosenReg == REG::NONE) {
      uint32_t const dataSize{MachineTypeUtil::getSize(type)};
      moduleInfo_.fnc.stackFrameSize += dataSize;
      localDef.stackFramePosition = moduleInfo_.fnc.stackFrameSize;
      if (isParam) {
        moduleInfo_.fnc.paramWidth += dataSize;
      } else {
        moduleInfo_.fnc.directLocalsWidth += dataSize;
      }
    }
    localDef.currentStorageType = ModuleInfo::LocalDef::getInitializedStorageType(chosenReg, isParam);
  }

  moduleInfo_.fnc.numLocals += multiplicity;
  // Possibly increment number of params
  if (isParam) {
    moduleInfo_.fnc.numParams += multiplicity;
  }
}

REG Backend::allocateRegForGlobal(MachineType const type) VB_NOEXCEPT {
  assert(((getNumLocalsInDr() == 0)) && "Cannot allocate globals after locals");
  assert(type != MachineType::INVALID);
  assert(!compiler_.getDebugMode());
  REG chosenReg{REG::NONE};

  if (MachineTypeUtil::getSize(type) == 4U) {
    chosenReg = WasmABI::dr[moduleInfo_.numGlobalsInGPR];
    moduleInfo_.numGlobalsInGPR++;
  }

  return chosenReg;
}

void Backend::cacheJobMemoryPtrPtr(uint32_t const spOffset, REG const scrReg) const {
  static_assert(Widths::jobMemoryPtrPtr == 4U, "Cached job memory width not suitable");
  assert(in_range<16>(bit_cast<int32_t>(spOffset)) && "spOffset too large");

  // Store cached jobMemoryPtrPtr
  as_.loadWordDRegDerefARegDisp16sx(scrReg, WasmABI::REGS::linMem, SafeInt<16U>::fromConst<-BD::FromEnd::jobMemoryDataPtrPtr>());
  as_.storeWordDerefARegDisp16sxDReg(scrReg, REG::SP, SafeInt<16U>::fromUnsafe(static_cast<int32_t>(spOffset)));
}

void Backend::restoreFromJobMemoryPtrPtr(uint32_t const spOffset) const {
  assert(in_range<16>(bit_cast<int32_t>(spOffset)) && "spOffset too large");

  // Restore cached jobMemoryPtrPtr and dereference
  as_.emitLoadDerefOff16sx(WasmABI::REGS::linMem, REG::SP, SafeInt<16>::fromUnsafe(static_cast<int32_t>(spOffset)));
  as_.INSTR(LDA_Ac_deref_Ab).setAc(WasmABI::REGS::linMem).setAb(WasmABI::REGS::linMem)();

  // Calculate the new base of the linear memory by adding basedataLength to the new memory base and store it in
  // REGS::linMem
  as_.addImmToReg(WasmABI::REGS::linMem, moduleInfo_.getBasedataLength());
}

void Backend::enteredFunction() {
  moduleInfo_.setupReferenceMap(memory_);

  // Get last binary offset where function entry should be patched into
  // Then save current offset as wrapper start, because the (following) function with
  // the current function index, adhering to the Wasm calling convention, will begin at the current offset
  uint32_t const lastBranchToFnc{moduleInfo_.wasmFncBodyBinaryPositions[moduleInfo_.fnc.index]};
  finalizeBranch(lastBranchToFnc);
  moduleInfo_.wasmFncBodyBinaryPositions[moduleInfo_.fnc.index] = output_.size();

  // Allocate and initialize stack for locals, stack is already aligned here
  uint32_t const newStackFrameSize{moduleInfo_.fnc.stackFrameSize + roundUpToPow2(moduleInfo_.fnc.directLocalsWidth + 128U, 4U)};

  // Function is under entered by fcall instruction, need to adjust stack frame size
  as_.setStackFrameSize(newStackFrameSize, false, false, stackAdjustAfterCall);
  moduleInfo_.currentState.checkedStackFrameSize = newStackFrameSize;
  as_.checkStackFence(callScrRegs[0], WasmABI::REGS::addrScrReg[0]); // SP change

  // Patch the function index in case this was an indirect call, we aren't sure, especially if tables are mutable at
  // some point so we do it unconditionally
  tryPatchFncIndexOfLastStacktraceEntry(moduleInfo_.fnc.index, WasmABI::REGS::addrScrReg[0], callScrRegs[0]);
}

void Backend::tryPushStacktraceEntry(uint32_t const fncIndex, uint32_t const storeOffsetFromSP, REG const addrScrReg, REG const scratchReg,
                                     REG const scratchReg2) const {
  static_assert(Widths::stacktraceRecord == 8U, "Stacktrace record width not suitable");
  if (!compiler_.isStacktraceEnabled()) {
    return;
  }

  assert((RegUtil::getOtherExtReg(scratchReg) == scratchReg2) && "First two callScrRegs do not form an extended register");

  // Calculate new frame ref pointer (SP + spOffset)
  as_.INSTR(LEA_Aa_deref_Ab_off16sx).setAa(addrScrReg).setAb(REG::SP).setOff16sx(SafeInt<16>::fromUnsafe(static_cast<int32_t>(storeOffsetFromSP)))();

  // Load old frame ref pointer from job memory, and function index into a register
  as_.loadWordDRegDerefARegDisp16sx(scratchReg, WasmABI::REGS::linMem, SafeInt<16U>::fromConst<-BD::FromEnd::lastFrameRefPtr>());

  // Don't write if it's an unknown index. In that case it will be patched later anyway
  if (fncIndex != UnknownIndex) {
    as_.MOVimm(scratchReg2, fncIndex);
  }

  // Store both to stack, STD stores even register on the lower address (will store scratchReg and scratchReg2, using
  // first as extended register)
  as_.INSTR(STD_deref_Ab_off10sx_Ea).setAb(addrScrReg).setOff10sx(SafeInt<10>::fromConst<0>()).setEa(scratchReg)();

  // Store to job memory last so everything else is on the stack in case we are running into a stack overflow here ->
  // then the ref should point to the last one)
  as_.emitStoreDerefOff16sx(WasmABI::REGS::linMem, addrScrReg, SafeInt<16U>::fromConst<-BD::FromEnd::lastFrameRefPtr>());
}

void Backend::tryPopStacktraceEntry(uint32_t const storeOffsetFromSP, REG const scratchReg) const {
  if (!compiler_.isStacktraceEnabled()) {
    return;
  }

  // Load previous frame ref ptr and store to job memory
  as_.loadWordDRegDerefARegDisp16sx(scratchReg, REG::SP, SafeInt<16U>::fromUnsafe(static_cast<int32_t>(storeOffsetFromSP)));
  as_.storeWordDerefARegDisp16sxDReg(scratchReg, WasmABI::REGS::linMem, SafeInt<16U>::fromConst<-BD::FromEnd::lastFrameRefPtr>());
}

void Backend::tryPatchFncIndexOfLastStacktraceEntry(uint32_t const fncIndex, REG const addrScrReg, REG const scratchReg) const {
  if (!compiler_.isStacktraceEnabled()) {
    return;
  }

  // Load old frame ref pointer from job memory
  as_.emitLoadDerefOff16sx(addrScrReg, WasmABI::REGS::linMem, SafeInt<16U>::fromConst<-BD::FromEnd::lastFrameRefPtr>());

  // Store function index to last entry
  as_.MOVimm(scratchReg, fncIndex);
  as_.storeWordDerefARegDisp16sxDReg(scratchReg, addrScrReg, SafeInt<16U>::fromConst<4>());
}

void Backend::emitNativeTrapAdapter() const {
  // NABI::addrParamRegs[0] contains pointer to the start of the linear memory. Needed because this function is not
  // called from the Wasm context
  as_.INSTR(MOVAA_Aa_Ab).setAa(WasmABI::REGS::linMem).setAb(NABI::addrParamRegs[0])();

  // NABI::paramRegs[0] contains the TrapCode which we move to REGS::trapReg
  as_.INSTR(MOV_Da_Db).setDa(WasmABI::REGS::trapReg).setDb(NABI::paramRegs[0])();

  as_.INSTR(JL_disp24sx2).setDisp24sx2(SafeInt<25>::fromConst<4>())();
  // LR/A[11] now points here, we do not need the old value because this function will not return anyway
  // move A[11] to a lower context register because the upper context will be restored during unwinding the CSA (via
  // RET) and increment by 4 so we skip this when iteratively unwinding the CSA
  uint32_t const preLEA{output_.size()};
  static_cast<void>(preLEA);
  as_.INSTR(LEA_Aa_deref_Ab_off16sx).setAa(REG::A3).setAb(REG::A11).setOff16sx(SafeInt<16U>::fromConst<4>())();
  uint32_t const postLEA{output_.size()};
  assert(((postLEA - preLEA) == 4U) && "Instructions length not 4");

  // A3 now points to here

  constexpr uint16_t pcxiCROffset{0xFE00U};
  as_.INSTR(MFCR_Dc_const16).setDc(callScrRegs[0]).setConst16(SafeUInt<16U>::fromConst<static_cast<uint32_t>(pcxiCROffset)>())();
  as_.loadWordDRegDerefARegDisp16sx(callScrRegs[1], WasmABI::REGS::linMem, SafeInt<16U>::fromConst<-BD::FromEnd::unwindPCXI>());
  RelPatchObj const properlyUnwound{as_.INSTR(JEQ_Da_Db_disp15sx2).setDa(callScrRegs[0]).setDb(callScrRegs[1]).prepJmp()};
  // Not properly unwound

  // Check if the next CSA entry to unwind has an upper or lower context tag (UL bit 20: 0 = LCX, 1 = UCX)
  // CAUTION: THIS IS ONLY VALID FOR >= TC1.6.2 (UL was bit 22 before)
  RelPatchObj const upperCX{as_.INSTR(JNZT_Da_n_disp15sx2).setDa(callScrRegs[0]).setN(SafeUInt<5U>::fromConst<20U>()).prepJmp()};
  // We can use RSLCV to unwind a CSA entry with a lower context tag by one 64-word entry
  // This will overwrite REG::A3, WasmABI::REGS::trapReg and WasmABI::REGS::linMem, so we need to temporarily store them
  // in upper context registers
  as_.INSTR(MOVA_Aa_Db).setAa(REG::A13).setDb(WasmABI::REGS::trapReg)();
  as_.INSTR(MOVAA_Aa_Ab).setAa(REG::A14).setAb(WasmABI::REGS::linMem)();
  as_.INSTR(MOVAA_Aa_Ab).setAa(REG::A15).setAb(REG::A3)();

  // Pop the last entry from the CSA. NOTE: This will clobber A[2]-A[7], D[0]-D[7] and A[11. Will also make PCXI point
  // to the previous CSA entry
  as_.INSTR(RSLCX)();

  // Now restore the temporarily saved registers
  as_.INSTR(MOVD_Da_Ab).setDa(WasmABI::REGS::trapReg).setAb(REG::A13)();
  as_.INSTR(MOVAA_Aa_Ab).setAa(WasmABI::REGS::linMem).setAb(REG::A14)();
  as_.INSTR(MOVAA_Aa_Ab).setAa(REG::A3).setAb(REG::A15)();

  // Try again
  RelPatchObj const tryAgain{as_.INSTR(J_disp24sx2).prepJmp()};
  tryAgain.linkToBinaryPos(postLEA);

  upperCX.linkToHere();
  // We need to use RET to unwind a CSA entry with an upper context tag by one 64-word entry
  // Move A3 to A11 and return (WasmABI::REGS::linMem is a lower context register anyway)
  as_.INSTR(MOVAA_Aa_Ab).setAa(REG::A11).setAb(REG::A3)();
  as_.INSTR(RET)();

  properlyUnwound.linkToHere();
  // CSA properly unwound now
}

void Backend::emitStackTraceCollector(uint32_t const stacktraceRecordCount) const {
  assert(stacktraceRecordCount > 0 && "No stacktrace records");

  // Load last frame ref pointer from job memory. This is definitely valid here
  as_.INSTR(LDA_Aa_deref_Ab_off16sx)
      .setAa(StackTrace::frameRefReg)
      .setAb(WasmABI::REGS::linMem)
      .setOff16sx(SafeInt<16U>::fromConst<-BD::FromEnd::lastFrameRefPtr>())();
  // Set targetReg to target buffer
  as_.INSTR(LEA_Aa_deref_Ab_off16sx)
      .setAa(StackTrace::targetReg)
      .setAb(WasmABI::REGS::linMem)
      .setOff16sx(SafeInt<16U>::fromUnsafe(-BD::FromEnd::getStacktraceArrayBase(stacktraceRecordCount)))();

  // Load number of stacktrace entries
  as_.MOVimm(StackTrace::counterReg, stacktraceRecordCount);
  uint32_t const loopStartOffset{output_.size()};
  // Load function index to scratch reg and store in buffer
  as_.loadWordDRegDerefARegDisp16sx(StackTrace::scratchReg, StackTrace::frameRefReg, SafeInt<16U>::fromConst<4>());
  as_.INSTR(STW_deref_Ab_Da).setAb(StackTrace::targetReg).setDa(StackTrace::scratchReg)();

  // Increment target buffer pointer
  as_.addImmToReg(StackTrace::targetReg, 4U);

  // Load next frame ref, compare to zero and break if it is zero (means first entry)
  as_.INSTR(LDA_Ac_deref_Ab).setAc(StackTrace::frameRefReg).setAb(StackTrace::frameRefReg)();
  RelPatchObj const collectedAll{as_.INSTR(JZA_Aa_disp15sx2).setAa(StackTrace::frameRefReg).prepJmp()};

  // Otherwise we decrement the counter and restart the loop if the counter is not zero yet
  as_.INSTR(ADD_Da_const4sx).setDa(StackTrace::counterReg).setConst4sx(SafeInt<4U>::fromConst<-1>())();
  as_.INSTR(JNE_Da_const4sx_disp15sx2)
      .setDa(StackTrace::counterReg)
      .setConst4sx(SafeInt<4U>::fromConst<0>())
      .prepJmp()
      .linkToBinaryPos(loopStartOffset);

  collectedAll.linkToHere();
}

void Backend::emitTrapHandler() const {
  // Restore stack pointer
  as_.INSTR(LDA_Aa_deref_Ab_off16sx)
      .setAa(REG::SP)
      .setAb(WasmABI::REGS::linMem)
      .setOff16sx(SafeInt<16U>::fromConst<-BD::FromEnd::trapStackReentry>())();

  // Load trapCodePtr into a register and store the trapCode there
  as_.emitLoadDerefOff16sx(WasmABI::REGS::addrScrReg[0], REG::SP, SafeInt<16U>::fromConst<static_cast<int32_t>(of_trapCodePtr_trapReentryPoint)>());
  as_.INSTR(STW_deref_Ab_Da).setAb(WasmABI::REGS::addrScrReg[0]).setDa(WasmABI::REGS::trapReg)();

  as_.INSTR(LDA_Aa_deref_Ab_off16sx)
      .setAa(REG::A11)
      .setAb(WasmABI::REGS::linMem)
      .setOff16sx(SafeInt<16U>::fromConst<-BD::FromEnd::trapHandlerPtr>())();
  as_.INSTR(JI_Aa).setAa(REG::A11)();
}

void Backend::emitFunctionEntryPoint(uint32_t const fncIndex) {
  assert(fncIndex < moduleInfo_.numTotalFunctions && "Function out of range");
  bool const imported{moduleInfo_.functionIsImported(fncIndex)};

  uint32_t currentFrameOffset{0U};

  // Move base register from second function argument to the register where all the code will expect it to be
  as_.INSTR(MOVAA_Aa_Ab).setAa(WasmABI::REGS::linMem).setAb(NABI::addrParamRegs[1])();

  // We are setting up the following stack structure from here on
  // When a trap is executed, we load the trapCode (uint32) into a register, then unwind the stack to the unwind target
  // (which is stored in link data), and FRET which will pop the return address off the stack again
  // RSP <------------ Stack growth direction (downwards) v <- unwind target
  // |  &trapCode  | (Stacktrace Record) | (cachedJobMemoryPtrPtr) | old A[11] | returnValuesPtr
  uint32_t const sigIndex{moduleInfo_.getFncSigIndex(fncIndex)};

  constexpr uint32_t of_stacktraceRecord{of_trapCodePtr_trapReentryPoint + 4U};
  constexpr uint32_t of_cachedJobMemoryPtrPtr{of_stacktraceRecord + Widths::stacktraceRecord};
  constexpr uint32_t of_oldA11{of_cachedJobMemoryPtrPtr + Widths::jobMemoryPtrPtr};

  constexpr uint32_t of_returnValuesPtr{of_oldA11 + 4U};
  constexpr uint32_t of_post{of_returnValuesPtr + 8U};
  constexpr uint32_t totalReserved{roundUpToPow2(of_post, 3U)};

  as_.subSp(totalReserved); // SP small change

  currentFrameOffset += totalReserved;

  // Here old A[11] must be saved even if the wasm function is called by fcall, because in trap case A11 won't be restored by fret
  as_.INSTR(STA_deref_Ab_off16sx_Aa).setAb(REG::SP).setOff16sx(SafeInt<16U>::fromConst<static_cast<int32_t>(of_oldA11)>()).setAa(REG::A11)();

  tryPushStacktraceEntry(fncIndex, of_stacktraceRecord, WasmABI::REGS::addrScrReg[0], callScrRegs[0], callScrRegs[1]);
  if (imported) {
    cacheJobMemoryPtrPtr(of_cachedJobMemoryPtrPtr, PreferredCallScrReg);
  }

  // addrParamRegs[2] contains the pointer to a variable where the TrapCode will be stored
  as_.INSTR(STA_deref_Ab_off16sx_Aa)
      .setAb(REG::SP)
      .setOff16sx(SafeInt<16U>::fromConst<static_cast<int32_t>(of_trapCodePtr_trapReentryPoint)>())
      .setAa(NativeABI::addrParamRegs[2])();

  // addrParamRegs[3] contains the pointer to an area where the returnValues will be stored
  as_.INSTR(STA_deref_Ab_off16sx_Aa)
      .setAb(REG::SP)
      .setOff16sx(SafeInt<16U>::fromConst<static_cast<int32_t>(of_returnValuesPtr)>())
      .setAa(NativeABI::addrParamRegs[3])();

  // Cache actual linear memory size as a register for efficiency
  as_.INSTR(LDA_Aa_deref_Ab_off16sx)
      .setAa(WasmABI::REGS::memSize)
      .setAb(WasmABI::REGS::linMem)
      .setOff16sx(SafeInt<16U>::fromConst<-BD::FromEnd::actualLinMemByteSize>())();

  // recover global to register
  common_.recoverGlobalsToRegs();

  // If saved stack pointer is not zero, this runtime already has an active frame and is already executing
  as_.INSTR(LDA_Aa_deref_Ab_off16sx)
      .setAa(WasmABI::REGS::addrScrReg[0])
      .setAb(WasmABI::REGS::linMem)
      .setOff16sx(SafeInt<16U>::fromConst<-BD::FromEnd::trapStackReentry>())();
  RelPatchObj const alreadyExecuting{as_.INSTR(JNZA_Aa_disp15sx2).setAa(WasmABI::REGS::addrScrReg[0]).prepJmp()};

  //
  //
  // NOT ALREADY EXECUTING START
  //
  //

  // Store unwind target to link data if this is the first frame
  as_.INSTR(STA_deref_Ab_off16sx_Aa)
      .setAb(WasmABI::REGS::linMem)
      .setOff16sx(SafeInt<16U>::fromConst<-BD::FromEnd::trapStackReentry>())
      .setAa(REG::SP)();

  // Load instruction pointer of trap reentry instruction pointer and store it on the stack
  // Move current PC (after instruction) to A11, can be clobbered because we saved it before
  as_.INSTR(JL_disp24sx2).setDisp24sx2(SafeInt<25>::fromConst<4>())();
  RelPatchObj const trapEntryAdr{
      as_.INSTR(LEA_Aa_deref_Ab_off16sx).setAa(REG::A11).setAb(REG::A11).setOff16sx(SafeInt<16U>::fromConst<0>()).prepLEA()};
  as_.INSTR(STA_deref_Ab_off16sx_Aa)
      .setAb(WasmABI::REGS::linMem)
      .setOff16sx(SafeInt<16U>::fromConst<-BD::FromEnd::trapHandlerPtr>())
      .setAa(REG::A11)();

  // Retrieve the current PCXI register from the core registers so we can unwind the CSA (Context Save Area) until there
  // when we trap (important if a native function was called via CALL which pushes the upper context to the CSA)
  constexpr uint16_t pcxiCROffset{0xFE00U};
  as_.INSTR(MFCR_Dc_const16).setDc(callScrRegs[0]).setConst16(SafeUInt<16U>::fromConst<static_cast<uint32_t>(pcxiCROffset)>())();
  as_.storeWordDerefARegDisp16sxDReg(callScrRegs[0], WasmABI::REGS::linMem, SafeInt<16U>::fromConst<-BD::FromEnd::unwindPCXI>());

  // Check stack limit for active protection
#if STACKSIZE_LEFT_BEFORE_NATIVE_CALL != 0 && ACTIVE_STACK_OVERFLOW_CHECK
  as_.INSTR(LDA_Aa_deref_Ab_off16sx)
      .setAa(WasmABI::REGS::addrScrReg[0])
      .setAb(WasmABI::REGS::linMem)
      .setOff16sx(SafeInt<16U>::fromConst<-BD::FromEnd::stackFence>())();
  as_.addImmToReg(WasmABI::REGS::addrScrReg[0], STACKSIZE_LEFT_BEFORE_NATIVE_CALL);
  // Overflow check is performed in Runtime::setStackFence()
  as_.INSTR(STA_deref_Ab_off16sx_Aa)
      .setAb(WasmABI::REGS::linMem)
      .setOff16sx(SafeInt<16U>::fromConst<-BD::FromEnd::nativeStackFence>())
      .setAa(WasmABI::REGS::addrScrReg[0])();
#endif

  //
  //
  // NOT ALREADY EXECUTING STOP
  //
  //

  alreadyExecuting.linkToHere();

  uint32_t const stackParamWidth{getStackParamWidth(sigIndex, imported)};
  uint32_t const stackReturnValueWidth{common_.getStackReturnValueWidth(sigIndex)};
  uint32_t const extraAlignment{deltaToNextPow2(currentFrameOffset + stackParamWidth + stackReturnValueWidth, 3U)};

  uint32_t const reservationFunctionCall{stackParamWidth + stackReturnValueWidth + extraAlignment};

  // Check limits for addImm24ToReg
  static_assert(roundUpToPow2(ImplementationLimits::numParams * 8U, 4U) <= 0xFF'FF'FFU, "Too many arguments");
  as_.subSp(reservationFunctionCall);
  as_.checkStackFence(callScrRegs[0], WasmABI::REGS::addrScrReg[0]); // SP change
  currentFrameOffset += reservationFunctionCall;

  int32_t addedSerOffset{0};
  int32_t addedSPOffset{0};

  constexpr REG paramaterHelper{REG::SP};

  // Load arguments from serialization buffer to registers and stack according to Wasm and native ABI, respectively
  uint32_t serOffset{0U};
  RegStackTracker tracker{};

  moduleInfo_.iterateParamsForSignature(
      sigIndex, FunctionRef<void(MachineType)>([this, imported, &addedSerOffset, &addedSPOffset, &tracker, &serOffset,
                                                stackParamWidth](MachineType const paramType) {
        bool const is64{MachineTypeUtil::is64(paramType)};
        REG const targetReg{getREGForArg(paramType, imported, tracker)};

        int32_t const currentSerOffsetUnsafe{static_cast<int32_t>(serOffset) - addedSerOffset};
        // 10bits offset for wasmType64, 16bits offset for wasmType32
        if (is64) {
          constexpr size_t offBitsRange{10U};
          SafeInt<offBitsRange> const currentSerOffset{selectOffsetRegisterHelper<offBitsRange>(addedSerOffset, currentSerOffsetUnsafe)};

          if (targetReg != REG::NONE) {
            as_.INSTR(LDD_Ea_deref_Ab_off10sx).setEa(targetReg).setAb(NABI::addrParamRegs[0]).setOff10sx(currentSerOffset)();
          } else {
            uint32_t const offsetFromSP{offsetInStackArgs(imported, stackParamWidth, tracker, paramType)};
            int32_t const currentSPOffsetUnsafe{static_cast<int32_t>(offsetFromSP) - addedSPOffset};
            SafeInt<offBitsRange> const currentSPOffset{selectOffsetRegisterHelper<offBitsRange>(addedSPOffset, currentSPOffsetUnsafe)};

            as_.INSTR(LDD_Ea_deref_Ab_off10sx).setEa(callScrRegs[0]).setAb(NABI::addrParamRegs[0]).setOff10sx(currentSerOffset)();
            as_.INSTR(STD_deref_Ab_off10sx_Ea).setAb(paramaterHelper).setOff10sx(currentSPOffset).setEa(callScrRegs[0])();
          }
        } else {
          constexpr size_t offBitsRange{16U};
          SafeInt<offBitsRange> const currentSerOffset{selectOffsetRegisterHelper<offBitsRange>(addedSerOffset, currentSerOffsetUnsafe)};

          if (targetReg != REG::NONE) {
            as_.loadWordDRegDerefARegDisp16sx(targetReg, NABI::addrParamRegs[0], currentSerOffset);
          } else {
            uint32_t const offsetFromSP{offsetInStackArgs(imported, stackParamWidth, tracker, paramType)};
            int32_t const currentSPOffsetUnsafe{static_cast<int32_t>(offsetFromSP) - addedSPOffset};
            SafeInt<offBitsRange> const currentSPOffset{selectOffsetRegisterHelper<offBitsRange>(addedSPOffset, currentSPOffsetUnsafe)};

            as_.loadWordDRegDerefARegDisp16sx(PreferredCallScrReg, NABI::addrParamRegs[0], currentSerOffset);
            as_.storeWordDerefARegDisp16sxDReg(PreferredCallScrReg, paramaterHelper, currentSPOffset);
          }
        }
        serOffset += 8U;
      }));

  if (imported) {
    as_.emitLoadDerefOff16sx(NativeABI::addrParamRegs[0], WasmABI::REGS::linMem, SafeInt<16U>::fromConst<-BD::FromEnd::customCtxOffset>());
  }

  assert(tracker.allocatedStackBytes == stackParamWidth && "Stack allocation size mismatch");

  // Check whether we are dealing with a builtin function
  if (moduleInfo_.functionIsBuiltin(fncIndex)) {
    throw FeatureNotSupportedException(ErrorCode::Cannot_export_builtin_function);
  }

  emitRawFunctionCall(fncIndex);

  uint32_t const numReturnValues{moduleInfo_.getNumReturnValuesForSignature(sigIndex)};

  if (numReturnValues > 0U) {
    uint32_t const returnValuePtrOffset{of_returnValuesPtr + reservationFunctionCall};
    as_.emitLoadDerefOff16sx(WasmABI::REGS::addrScrReg[2], REG::SP, SafeInt<16U>::fromUnsafe(static_cast<int32_t>(returnValuePtrOffset)));

    uint32_t index{0U};
    RegStackTracker returnValueTracker{};
    moduleInfo_.iterateResultsForSignature(
        sigIndex, FunctionRef<void(MachineType)>([this, stackParamWidth, &index, &returnValueTracker](MachineType const machineType) {
          bool const is64{MachineTypeUtil::is64(machineType)};
          REG const srcReg{getREGForReturnValue(machineType, returnValueTracker)};
          uint32_t const destOffset{index * 8U};
          if (srcReg != REG::NONE) {
            // here no need to worry about whether destOffset is out of range (10bit/16bit), because only a limited number of the first few return
            // values will be in registers, which means destOffset must be in range.
            if (is64) {
              as_.INSTR(STD_deref_Ab_off10sx_Ea)
                  .setAb(WasmABI::REGS::addrScrReg[2])
                  .setOff10sx(SafeInt<10U>::fromUnsafe(static_cast<int32_t>(destOffset)))
                  .setEa(srcReg)();
            } else {
              as_.storeWordDerefARegDisp16sxDReg(srcReg, WasmABI::REGS::addrScrReg[2], SafeInt<16U>::fromUnsafe(static_cast<int32_t>(destOffset)));
            }
          } else {
            uint32_t const srcOffset{stackParamWidth + offsetInStackReturnValues(returnValueTracker, machineType)};
            if (is64) {
              SignedInRangeCheck<10> const srcRangeChecker{SignedInRangeCheck<10>::check(static_cast<int32_t>(srcOffset))};
              SafeInt<10> currentSrcOffset{};
              if (!srcRangeChecker.inRange()) {
                as_.addImmToReg(NABI::addrParamRegs[0], srcOffset);
                currentSrcOffset = SafeInt<10>::fromConst<0>();
              } else {
                currentSrcOffset = srcRangeChecker.safeInt();
              }

              SignedInRangeCheck<10> const destRangeChecker{SignedInRangeCheck<10>::check(static_cast<int32_t>(destOffset))};
              SafeInt<10> currentDestOffset{};
              if (!destRangeChecker.inRange()) {
                as_.addImmToReg(NABI::addrParamRegs[0], destOffset);
                currentDestOffset = SafeInt<10>::fromConst<0>();
              } else {
                currentDestOffset = destRangeChecker.safeInt();
              }

              as_.INSTR(LDD_Ea_deref_Ab_off10sx).setEa(callScrRegs[0]).setAb(REG::SP).setOff10sx(currentSrcOffset)();
              as_.INSTR(STD_deref_Ab_off10sx_Ea).setAb(WasmABI::REGS::addrScrReg[2]).setOff10sx(currentDestOffset).setEa(callScrRegs[0])();
            } else {
              SignedInRangeCheck<16> const srcRangeChecker{SignedInRangeCheck<16>::check(static_cast<int32_t>(srcOffset))};
              SafeInt<16> currentSrcOffset{};
              if (!srcRangeChecker.inRange()) {
                as_.addImmToReg(NABI::addrParamRegs[0], srcOffset);
                currentSrcOffset = SafeInt<16>::fromConst<0>();
              } else {
                currentSrcOffset = srcRangeChecker.safeInt();
              }

              SignedInRangeCheck<16> const destRangeChecker{SignedInRangeCheck<16>::check(static_cast<int32_t>(destOffset))};
              SafeInt<16> currentDestOffset{};
              if (!destRangeChecker.inRange()) {
                as_.addImmToReg(NABI::addrParamRegs[0], destOffset);
                currentDestOffset = SafeInt<16>::fromConst<0>();
              } else {
                currentDestOffset = destRangeChecker.safeInt();
              }
              as_.loadWordDRegDerefARegDisp16sx(PreferredCallScrReg, REG::SP, currentSrcOffset);
              as_.storeWordDerefARegDisp16sxDReg(PreferredCallScrReg, WasmABI::REGS::addrScrReg[2], currentDestOffset);
            }
          }
          index++;
        }));
  }

  // Remove function arguments again
  as_.addImmToReg(REG::SP, reservationFunctionCall);
  currentFrameOffset -= reservationFunctionCall;

  // Now unwind target and potentially the stacktrace record are still on stack; 8 bytes in any case

  if (imported) {
    restoreFromJobMemoryPtrPtr(of_cachedJobMemoryPtrPtr);
  }
  tryPopStacktraceEntry(of_stacktraceRecord, PreferredCallScrReg);

  trapEntryAdr.linkToHere();

  common_.moveGlobalsToLinkData();

  // Load potential unwind target so we can identify whether this was the first frame in the call sequence
  as_.INSTR(LDA_Aa_deref_Ab_off16sx)
      .setAa(WasmABI::REGS::addrScrReg[0])
      .setAb(WasmABI::REGS::linMem)
      .setOff16sx(SafeInt<16U>::fromConst<-BD::FromEnd::trapStackReentry>())();

  // Compare the trap unwind target to the current stack pointer
  RelPatchObj const notFirstFrame{as_.INSTR(JNEA_Aa_Ab_disp15sx2).setAa(REG::SP).setAb(WasmABI::REGS::addrScrReg[0]).prepJmp()};
  // If this is equal, we can conclude this was the first frame in the call sequence and subsequently reset the stored
  // trap target
  as_.INSTR(MOV_Da_const4sx).setDa(PreferredCallScrReg).setConst4sx(SafeInt<4U>::fromConst<0>())();
  as_.storeWordDerefARegDisp16sxDReg(PreferredCallScrReg, WasmABI::REGS::linMem,
                                     SafeInt<16U>::fromConst<-BD::FromEnd::trapStackReentry>()); // Reset trap target
  as_.storeWordDerefARegDisp16sxDReg(PreferredCallScrReg, WasmABI::REGS::linMem,
                                     SafeInt<16U>::fromConst<-BD::FromEnd::trapHandlerPtr>()); // Reset trap target
  notFirstFrame.linkToHere();

  //
  //

  // Restore old A[11] and unwind stack
  as_.INSTR(LDA_Aa_deref_Ab_off16sx).setAa(REG::A11).setAb(REG::SP).setOff16sx(SafeInt<16U>::fromConst<static_cast<int32_t>(of_oldA11)>())();
  as_.INSTR(LEA_Aa_deref_Ab_off16sx)
      .setAa(REG::SP)
      .setAb(REG::SP)
      .setOff16sx(SafeInt<16U>::fromUnsafe(static_cast<int32_t>(totalReserved)))(); // SP small change
  currentFrameOffset -= totalReserved;
  static_cast<void>(currentFrameOffset);
  assert(currentFrameOffset == 0 && "Unaligned stack at end of wrapper call");
  as_.INSTR(RET)();
}

void Backend::emitRawFunctionCall(uint32_t const fncIndex) {
  if (moduleInfo_.functionIsImported(fncIndex)) {
    // Calling an imported function
    ModuleInfo::ImpFuncDef const impFuncDef{moduleInfo_.getImpFuncDef(fncIndex)};
    assert(impFuncDef.builtinFunction == BuiltinFunction::UNDEFINED && "Builtin functions cannot be emitted this way, do it explicitly");

    if (!impFuncDef.linked) {
      as_.TRAP(TrapCode::CALLED_FUNCTION_NOT_LINKED);
      return;
    }

#if STACKSIZE_LEFT_BEFORE_NATIVE_CALL != 0 && ACTIVE_STACK_OVERFLOW_CHECK
    as_.INSTR(LDA_Aa_deref_Ab_off16sx)
        .setAa(WasmABI::REGS::addrScrReg[0])
        .setAb(WasmABI::REGS::linMem)
        .setOff16sx(SafeInt<16U>::fromConst<-BD::FromEnd::nativeStackFence>())();
    // if (nativeStackFence >= $SP) trap
    as_.INSTR(GEA_Dc_Aa_Ab).setDc(callScrRegs[0]).setAa(WasmABI::REGS::addrScrReg[0]).setAb(REG::SP)();
    as_.cTRAP(TrapCode::STACKFENCEBREACHED, JumpCondition::bitTrue(callScrRegs[0], SafeInt<4U>::fromConst<0>()));
#endif

    NativeSymbol const &nativeSymbol{moduleInfo_.importSymbols[impFuncDef.symbolIndex]};
    if (nativeSymbol.linkage == NativeSymbol::Linkage::STATIC) {
      uint32_t const rawAddr{static_cast<uint32_t>(bit_cast<uintptr_t>(nativeSymbol.ptr))};
      if (Instruction::fitsAbsDisp24sx2(rawAddr)) {
        as_.INSTR(CALLA_absdisp24sx2).setAbsDisp24sx2(rawAddr)();
      } else {
        as_.MOVimm(WasmABI::REGS::addrScrReg[0], rawAddr);
        as_.INSTR(CALLI_Aa).setAa(WasmABI::REGS::addrScrReg[0])();
      }
    } else {
      uint32_t const basedataLength{moduleInfo_.getBasedataLength()};
      int32_t const fncPtrBaseOffset{bit_cast<int32_t>((BD::FromStart::linkData - basedataLength) + impFuncDef.linkDataOffset)};
      SignedInRangeCheck<16U> const rangeCheck{SignedInRangeCheck<16U>::check(fncPtrBaseOffset)};
      if (rangeCheck.inRange()) {
        as_.emitLoadDerefOff16sx(WasmABI::REGS::addrScrReg[0], WasmABI::REGS::linMem, rangeCheck.safeInt());
      } else {
        SafeUInt<16U> const reducedHighPortion{SafeUInt<32U>::fromAny(bit_cast<uint32_t>(fncPtrBaseOffset) + 0x8000U).rightShift<16U>()};
        as_.INSTR(ADDIHA_Ac_Aa_const16).setAc(WasmABI::REGS::addrScrReg[0]).setAa(WasmABI::REGS::linMem).setConst16(reducedHighPortion)();
        as_.emitLoadDerefOff16sx(WasmABI::REGS::addrScrReg[0], WasmABI::REGS::addrScrReg[1],
                                 Instruction::lower16sx(bit_cast<uint32_t>(fncPtrBaseOffset)));
      }
      // Execute the actual call
      as_.INSTR(CALLI_Aa).setAa(WasmABI::REGS::addrScrReg[0])();
    }
  } else {
    // Calling a Wasm-internal function
    // Check if the function body we are targeting has already been emitted
    if (fncIndex <= moduleInfo_.fnc.index) {
      // Check at which offset in the binary the function body is present
      uint32_t const binaryFncBodyOffset{moduleInfo_.wasmFncBodyBinaryPositions[fncIndex]};
      // If the index is smaller then the current index, it's already defined
      assert(binaryFncBodyOffset != 0xFF'FF'FF'FF && "Function needs to be defined already");

      // Produce a dummy call instruction, synthesize a corresponding RelPatchObj and link it to the start of the body
      RelPatchObj const branchObj{as_.INSTR(FCALL_disp24sx2).prepJmp()};
      branchObj.linkToBinaryPos(binaryFncBodyOffset);
    } else {
      // Body of the target function has not been emitted yet so we link it to either an unknown target or the last
      // branch that targets this still-unknown function body. This way we are essentially creating a linked-list of
      // branches inside the output binary that we are going to fully patch later

      // We correspondingly produce a call instruction
      RelPatchObj const branchObj{as_.INSTR(FCALL_disp24sx2).prepJmp()};

      // Register the branch
      registerPendingBranch(branchObj, moduleInfo_.wasmFncBodyBinaryPositions[fncIndex]);
    }
  }
}

uint32_t Backend::getStackParamWidth(uint32_t const sigIndex, bool const imported) const VB_NOEXCEPT {
  uint32_t stackParamWidth{0U};
  RegStackTracker tracker{};
  moduleInfo_.iterateParamsForSignature(
      sigIndex, FunctionRef<void(MachineType)>([this, imported, &tracker, &stackParamWidth](MachineType const paramType) VB_NOEXCEPT {
        REG const targetReg{getREGForArg(paramType, imported, tracker)};
        if (targetReg == REG::NONE) {
          stackParamWidth += widthInStack(paramType);
        }
      }));
  return stackParamWidth;
}

uint32_t Backend::offsetInStackArgs(bool const imported, uint32_t const paramWidth, RegStackTracker &tracker,
                                    MachineType const paramType) const VB_NOEXCEPT {
  uint32_t offsetInArgs{0U};

  if (imported) {
    offsetInArgs = tracker.allocatedStackBytes;
  } else {
    offsetInArgs = paramWidth - tracker.allocatedStackBytes - widthInStack(paramType);
  }

  tracker.allocatedStackBytes += widthInStack(paramType);
  return offsetInArgs;
}

uint32_t Backend::widthInStack(MachineType const machineType) VB_NOEXCEPT {
  return MachineTypeUtil::getSize(machineType);
}

REG Backend::getREGForArg(MachineType const paramType, bool const imported, RegStackTracker &tracker) const VB_NOEXCEPT {
  if (imported) {
    if (!MachineTypeUtil::is64(paramType)) {
      if (tracker.missedReg != REG::NONE) {
        // Consume missedReg
        REG const targetReg{tracker.missedReg};
        tracker.missedReg = REG::NONE;
        tracker.allocatedDRs++;
        return targetReg;
      } else if (tracker.allocatedDRs < NABI::paramRegs.size()) {
        // missedReg is already REG::NONE
        REG const targetReg{NABI::paramRegs[tracker.allocatedDRs]};
        tracker.allocatedDRs++;
        return targetReg;
      } else {
        ; /* No action required - ; is optional */
      }
    } else {
      if (tracker.allocatedDRs < NABI::paramRegs.size()) {
        uint32_t const firstCandidateIdx{RegUtil::canBeExtReg(static_cast<REG>(tracker.allocatedDRs)) ? tracker.allocatedDRs
                                                                                                      : (tracker.allocatedDRs + 1U)};
        if ((firstCandidateIdx + 1U) < static_cast<uint32_t>(NABI::paramRegs.size())) {
          REG const firstCandidateReg{NABI::paramRegs[firstCandidateIdx]};
          REG const otherCandidateReg{NABI::paramRegs[firstCandidateIdx + 1U]};
          static_cast<void>(otherCandidateReg);
          assert((RegUtil::canBeExtReg(firstCandidateReg) && RegUtil::getOtherExtReg(firstCandidateReg) == otherCandidateReg) &&
                 "Extended register pair malformed");

          REG const newMissedReg{(firstCandidateIdx > tracker.allocatedDRs) ? NABI::paramRegs[tracker.allocatedDRs] : REG::NONE};
          assert((tracker.missedReg == REG::NONE || newMissedReg == REG::NONE) && "Either new or old missedReg needs to be none");

          tracker.missedReg = newMissedReg;
          tracker.allocatedDRs += 2U;
          return firstCandidateReg;
        }
      }
    }
  } else {
    assert((tracker.missedReg == REG::NONE) && "missedDR cannot be set for non-imported functions");
    if (!MachineTypeUtil::is64(paramType)) {
      if (tracker.allocatedDRs < WasmABI::regsForParams) {
        // missedReg is already REG::NONE

        REG const targetReg{WasmABI::dr[moduleInfo_.getLocalStartIndexInGPRs() + tracker.allocatedDRs]};
        tracker.allocatedDRs++;
        return targetReg;
      }
    }
  }

  // If nothing has matched, we allocate it on the stack and keep whatever is set for missedReg
  return REG::NONE;
}

uint32_t Backend::offsetInStackReturnValues(RegStackTracker &tracker, MachineType const returnValueType) VB_NOEXCEPT {
  uint32_t const offset{tracker.allocatedStackBytes};
  tracker.allocatedStackBytes += widthInStack(returnValueType);
  return offset;
}

REG Backend::getREGForReturnValue(MachineType const returnValueType, RegStackTracker &tracker) const VB_NOEXCEPT {
  REG reg{REG::NONE};
  if (MachineTypeUtil::is64(returnValueType)) {
    if (tracker.allocatedDRs < WasmABI::gpRegsForReturnValues) {
      reg = WasmABI::REGS::returnValueRegs[tracker.allocatedDRs];
      assert(RegUtil::canBeExtReg(reg) && "Extended register pair malformed");
      tracker.allocatedDRs += 2U;
    }
  } else {
    if (tracker.missedReg != REG::NONE) {
      reg = tracker.missedReg;
      tracker.missedReg = REG::NONE;
    } else if (tracker.allocatedDRs < (WasmABI::gpRegsForReturnValues - 1U)) {
      reg = WasmABI::REGS::returnValueRegs[tracker.allocatedDRs];
      tracker.missedReg = WasmABI::REGS::returnValueRegs[tracker.allocatedDRs + 1U];
      tracker.allocatedDRs += 2U;
    } else {
      ; /* No action required - ; is optional */
    }
  }

  return reg;
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

  common_.moveGlobalsToLinkData();

  uint32_t const newStackParamWidth{getStackParamWidth(sigIndex, true)};
  uint32_t const oldStackParamWidth{getStackParamWidth(sigIndex, false)};

  // We are dealing with the following memory layout
  // RSP <--- Stack growth direction (downwards)            addrScrReg[0] --> v
  // v <------------------------------ totalReserved -----------------------> |
  // | Stack Params | (jobMemoryPtrPtr) | Old Reg Params (32B) | Padding | Old Stack Params |
  uint32_t const of_jobMemoryPtrPtr{newStackParamWidth};
  uint32_t const of_post{of_jobMemoryPtrPtr + Widths::jobMemoryPtrPtr};
  uint32_t const totalReserved{roundUpToPow2(of_post, 3U) + stackAdjustAfterCall}; // Need to adjust the stack pushed by fcall

  // Set up a scratch register so it points to the of the original stack params (lower SP + 4)
  as_.INSTR(LEA_Aa_deref_Ab_off16sx).setAa(WasmABI::REGS::addrScrReg[0]).setAb(REG::SP).setOff16sx(SafeInt<16U>::fromConst<stackAdjustAfterCall>())();

  as_.subSp(totalReserved);

  as_.checkStackFence(callScrRegs[0], WasmABI::REGS::addrScrReg[1]); // SP change

  RegStackTracker targetTracker{};
  RegStackTracker oldTracker{};

  RegisterCopyResolver<NativeABI::paramRegs.size()> registerCopyResolver{};

  moduleInfo_.iterateParamsForSignature(
      sigIndex, FunctionRef<void(MachineType)>([this, &registerCopyResolver, &targetTracker, &oldTracker, oldStackParamWidth,
                                                newStackParamWidth](MachineType const paramType) {
        REG const sourceReg{getREGForArg(paramType, false, oldTracker)};
        REG const targetReg{getREGForArg(paramType, true, targetTracker)};
        bool const is64{MachineTypeUtil::is64(paramType)};

        uint32_t sourceStackOffset{0U};
        if (sourceReg == REG::NONE) {
          sourceStackOffset = offsetInStackArgs(false, oldStackParamWidth, oldTracker, paramType);
        }

        if (targetReg != REG::NONE) {
          if (targetReg == sourceReg) {
            return; // Skip since source and dist registers are same
          }
          if (sourceReg != REG::NONE) {
            // Reg -> Reg
            assert(!is64 && "64-bit register is not used for Wasm parameters");
            registerCopyResolver.push(VariableStorage::reg(paramType, targetReg), VariableStorage::reg(paramType, sourceReg));
          } else {
            // Stack -> REG
            if (is64) {
              registerCopyResolver.push(VariableStorage::reg(paramType, targetReg), ResolverRecord::TargetType::Extend,
                                        VariableStorage::stackMemory(paramType, sourceStackOffset));
              registerCopyResolver.push(VariableStorage::reg(paramType, RegUtil::getOtherExtReg(targetReg)),
                                        ResolverRecord::TargetType::Extend_Placeholder,
                                        VariableStorage::stackMemory(paramType, sourceStackOffset + 4U));
            } else {
              registerCopyResolver.push(VariableStorage::reg(paramType, targetReg), VariableStorage::stackMemory(paramType, sourceStackOffset));
            }
          }
        } else {
          if (sourceReg != REG::NONE) {
            // Reg -> Stack
            uint32_t const newOffsetFromSP{offsetInStackArgs(true, newStackParamWidth, targetTracker, paramType)};
            as_.storeWordDerefARegDisp16sxDReg(sourceReg, REG::SP, SafeInt<16U>::fromUnsafe(static_cast<int32_t>(newOffsetFromSP)));
          } else {
            // Stack -> Stack
            uint32_t const newOffsetFromSP{offsetInStackArgs(true, newStackParamWidth, targetTracker, paramType)};
            as_.emitLoadDerefOff16sx(WasmABI::REGS::addrScrReg[1], WasmABI::REGS::addrScrReg[0],
                                     SafeInt<16U>::fromUnsafe(static_cast<int32_t>(sourceStackOffset)));
            as_.emitStoreDerefOff16sx(REG::SP, WasmABI::REGS::addrScrReg[1], SafeInt<16U>::fromUnsafe(static_cast<int32_t>(newOffsetFromSP)));

            if (is64) {
              as_.emitLoadDerefOff16sx(WasmABI::REGS::addrScrReg[1], WasmABI::REGS::addrScrReg[0],
                                       SafeInt<16U>::fromUnsafe(static_cast<int32_t>(sourceStackOffset) + 4));
              as_.emitStoreDerefOff16sx(REG::SP, WasmABI::REGS::addrScrReg[1], SafeInt<16U>::fromUnsafe(static_cast<int32_t>(newOffsetFromSP) + 4));
            }
          }
        }
      }));

  registerCopyResolver.resolve(MoveEmitter([this](VariableStorage const &target, VariableStorage const &source) {
                                 bool const is64{MachineTypeUtil::is64(source.machineType)};
                                 // Can't use emitMoveIntImpl because it handles stack frame offset calculation differently
                                 if (source.type == StorageType::REGISTER) {
                                   as_.INSTR(MOV_Da_Db).setDa(target.location.reg).setDb(source.location.reg)();
                                 } else {
                                   if (is64) {
                                     as_.INSTR(LDD_Ea_deref_Ab_off10sx)
                                         .setEa(target.location.reg)
                                         .setAb(WasmABI::REGS::addrScrReg[0])
                                         .setOff10sx(SafeInt<10U>::fromUnsafe(static_cast<int32_t>(source.location.stackFramePosition)))();
                                   } else {
                                     as_.loadWordDRegDerefARegDisp16sx(
                                         target.location.reg, WasmABI::REGS::addrScrReg[0],
                                         SafeInt<16U>::fromUnsafe(static_cast<int32_t>(source.location.stackFramePosition)));
                                   }
                                 }
                               }),
                               SwapEmitter(nullptr));

  as_.emitLoadDerefOff16sx(NativeABI::addrParamRegs[0], WasmABI::REGS::linMem, SafeInt<16U>::fromConst<-BD::FromEnd::customCtxOffset>());

  // Patch the last function index because this was reached via an indirect call and the function index isn't known
  tryPatchFncIndexOfLastStacktraceEntry(fncIndex, WasmABI::REGS::addrScrReg[0], callScrRegs[0]);

  cacheJobMemoryPtrPtr(of_jobMemoryPtrPtr, PreferredCallScrReg);
  emitRawFunctionCall(fncIndex);
  restoreFromJobMemoryPtrPtr(of_jobMemoryPtrPtr);
#if INTERRUPTION_REQUEST
  checkForInterruptionRequest(callScrRegs[0]);
#endif

  // Since function is imported we restore the memSize
  as_.INSTR(LDA_Aa_deref_Ab_off16sx)
      .setAa(WasmABI::REGS::memSize)
      .setAb(WasmABI::REGS::linMem)
      .setOff16sx(SafeInt<16U>::fromConst<-BD::FromEnd::actualLinMemByteSize>())();

  common_.recoverGlobalsToRegs();

  as_.addImmToReg(REG::SP, totalReserved);
  as_.INSTR(FRET)();
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

void Backend::execDirectFncCall(uint32_t const fncIndex) {
  bool const imported{moduleInfo_.functionIsImported(fncIndex)};
  assert((!imported || (!moduleInfo_.functionIsBuiltin(fncIndex))) && "Builtin functions can only be executed by execBuiltinFncCall");
  assert((!imported || fncIndex != UnknownIndex) && "Need to provide fncIndex for imports");

  uint32_t const sigIndex{moduleInfo_.getFncSigIndex(fncIndex)};
  Stack::iterator const paramsBase{common_.prepareCallParamsAndSpillContext(sigIndex, false)};

  // Load the parameters etc., set up everything then emit the actual call
  if (moduleInfo_.functionIsV2Import(fncIndex)) {
    DirectV2Import v2ImportCall{*this, sigIndex};
    common_.moveGlobalsToLinkData();
    v2ImportCall.iterateParams(paramsBase);
    uint32_t const jobMemoryPtrPtrOffset{v2ImportCall.getJobMemoryPtrPtrOffset()};
    // coverity[autosar_cpp14_a5_1_9_violation]
    v2ImportCall.emitFncCallWrapper(fncIndex, FunctionRef<void()>([this, fncIndex, jobMemoryPtrPtrOffset]() {
                                      cacheJobMemoryPtrPtr(jobMemoryPtrPtrOffset, PreferredCallScrReg);
                                      emitRawFunctionCall(fncIndex);
                                      restoreFromJobMemoryPtrPtr(jobMemoryPtrPtrOffset);
#if INTERRUPTION_REQUEST
                                      checkForInterruptionRequest(callScrRegs[0]);
#endif
                                    }));
    setupMemSizeReg();
    common_.recoverGlobalsToRegs();
    v2ImportCall.iterateResults();
  } else if (imported) {
    // Direct call to V1 import native function
    ImportCallV1 importCallV1Impl{*this, sigIndex};

    RegMask const availableLocalsRegMask{common_.saveLocalsAndParamsForFuncCall(true)};
    common_.moveGlobalsToLinkData();
    static_cast<void>(importCallV1Impl.iterateParams(paramsBase, availableLocalsRegMask));
    importCallV1Impl.prepareCtx();
    importCallV1Impl.resolveRegisterCopies();
    uint32_t const jobMemoryPtrPtrOffset{importCallV1Impl.getJobMemoryPtrPtrOffset()};
    // coverity[autosar_cpp14_a5_1_9_violation]
    importCallV1Impl.emitFncCallWrapper(fncIndex, FunctionRef<void()>([this, fncIndex, jobMemoryPtrPtrOffset]() {
                                          cacheJobMemoryPtrPtr(jobMemoryPtrPtrOffset, PreferredCallScrReg);
                                          emitRawFunctionCall(fncIndex);
                                          restoreFromJobMemoryPtrPtr(jobMemoryPtrPtrOffset);
#if INTERRUPTION_REQUEST
                                          checkForInterruptionRequest(callScrRegs[0]);
#endif
                                        }));

    setupMemSizeReg();
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
    directWasmCallImpl.iterateResults();
  }
}

void Backend::execIndirectWasmCall(uint32_t const sigIndex, uint32_t const tableIndex) {
  static_cast<void>(tableIndex);
  assert(moduleInfo_.hasTable && tableIndex == 0 && "Table not defined");
  Stack::iterator const paramsBase{common_.prepareCallParamsAndSpillContext(sigIndex, true)};

  InternalCall indirectCallImpl{*this, sigIndex};

  RegMask const availableLocalsRegMask{common_.saveLocalsAndParamsForFuncCall(false)};
  Stack::iterator const indirectCallIndex{indirectCallImpl.iterateParams(paramsBase, availableLocalsRegMask)};
  indirectCallImpl.handleIndirectCallReg(indirectCallIndex, availableLocalsRegMask);
  indirectCallImpl.resolveRegisterCopies();

  // coverity[autosar_cpp14_a5_1_9_violation]
  indirectCallImpl.emitFncCallWrapper(
      UnknownIndex, FunctionRef<void()>([this, sigIndex]() {
        // Check if dynamic function index is in range of table
        // if (tableInitialSize - 1U < indirectCallReg) trap;
        as_.MOVimm(callScrRegs[0], moduleInfo_.tableInitialSize - 1U);
        as_.cTRAP(TrapCode::INDIRECTCALL_OUTOFBOUNDS, JumpCondition::u32LtReg(callScrRegs[0], WasmABI::REGS::indirectCallReg));

        // Load pointer to table start to addrScrReg[0]
        as_.INSTR(LDA_Aa_deref_Ab_off16sx)
            .setAa(WasmABI::REGS::addrScrReg[0])
            .setAb(WasmABI::REGS::linMem)
            .setOff16sx(SafeInt<16U>::fromConst<-Basedata::FromEnd::tableAddressOffset>())();

        // Step to the actual table entry we are targeting
        as_.INSTR(ADDSCA_Ac_Ab_Da_nSc)
            .setAc(WasmABI::REGS::addrScrReg[0])
            .setAb(WasmABI::REGS::addrScrReg[0])
            .setDa(WasmABI::REGS::indirectCallReg)
            .setNSc(SafeUInt<2>::fromConst<3U>())();

        // Load function signature index and check if it matches
        as_.loadWordDRegDerefARegDisp16sx(callScrRegs[0], WasmABI::REGS::addrScrReg[0], SafeInt<16U>::fromConst<4>());
        as_.MOVimm(callScrRegs[1], sigIndex);
        as_.cTRAP(TrapCode::INDIRECTCALL_WRONGSIG, JumpCondition::i32NeReg(callScrRegs[0], callScrRegs[1]));

        // Load the offset
        as_.INSTR(LDA_Ac_deref_Ab).setAc(WasmABI::REGS::addrScrReg[1]).setAb(WasmABI::REGS::addrScrReg[0])();

        // Check if the offset is zero which means the function is not linked
        as_.MOVimm(WasmABI::REGS::addrScrReg[2], 0U);

        as_.cTRAP(TrapCode::CALLED_FUNCTION_NOT_LINKED, JumpCondition::addrEqReg(WasmABI::REGS::addrScrReg[1], WasmABI::REGS::addrScrReg[2]));

        // Otherwise calculate the absolute address and execute the call
        // addrScrReg[0] = startAddressOfModuleBinary
        as_.INSTR(LDA_Aa_deref_Ab_off16sx)
            .setAa(WasmABI::REGS::addrScrReg[0])
            .setAb(WasmABI::REGS::linMem)
            .setOff16sx(SafeInt<16U>::fromConst<-Basedata::FromEnd::binaryModuleStartAddressOffset>())();
        as_.INSTR(ADDA_Ac_Aa_Ab).setAc(WasmABI::REGS::addrScrReg[0]).setAa(WasmABI::REGS::addrScrReg[0]).setAb(WasmABI::REGS::addrScrReg[1])();
        as_.INSTR(FCALLI_Aa).setAa(WasmABI::REGS::addrScrReg[0])();
      }));

  indirectCallImpl.iterateResults();
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
    as_.loadWordDRegDerefARegDisp16sx(bufLenRegElem.reg, WasmABI::REGS::linMem, SafeInt<16U>::fromConst<-BD::FromEnd::linkedMemLen>());
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
    constexpr auto dataOffset =
        make_array(SafeInt<16U>::fromConst<-1>(), SafeInt<16U>::fromConst<-1>(), SafeInt<16U>::fromConst<-2>(), SafeInt<16U>::fromConst<-2>(),
                   SafeInt<16U>::fromConst<-4>(), SafeInt<16U>::fromConst<-4>(), SafeInt<16U>::fromConst<-8>(), SafeInt<16U>::fromConst<-8>(),
                   SafeInt<16U>::fromConst<-4>(), SafeInt<16U>::fromConst<-8>());
    // coverity[autosar_cpp14_a8_5_2_violation]
    constexpr auto machineType = make_array(MachineType::I32, MachineType::I32, MachineType::I32, MachineType::I32, MachineType::I32,
                                            MachineType::I32, MachineType::I64, MachineType::I64, MachineType::F32, MachineType::F64);
    // coverity[autosar_cpp14_a8_5_2_violation]
    constexpr auto signExtends = make_array(false, true, false, true, false, false, false, false, false, false);

    SafeInt<16U> const memObjOffset{dataOffset[biFncIndex]};
    // coverity[autosar_cpp14_m5_0_9_violation]
    uint32_t const memObjSize{static_cast<uint32_t>(-memObjOffset.value())};
    MachineType const resultType{machineType[biFncIndex]};
    bool const signExtend{signExtends[biFncIndex]};

    RegAllocTracker regAllocTracker{};
    REG const offsetReg{common_.liftToRegInPlaceProt(*offsetElementPtr, false, regAllocTracker).reg};

    RegElement const bufLenRegElem{common_.reqScratchRegProt(MachineType::I32, regAllocTracker, false)};
    as_.loadWordDRegDerefARegDisp16sx(bufLenRegElem.reg, WasmABI::REGS::linMem, SafeInt<16U>::fromConst<-BD::FromEnd::linkedMemLen>());

    // if (offset < 0) trap;
    as_.cTRAP(TrapCode::LINKEDMEMORY_OUTOFBOUNDS, JumpCondition::i32LtConst4sx(offsetReg, SafeInt<4U>::fromConst<0>()));

    // if (bufLen - dataLen < offset) trap;
    as_.INSTR(ADDI_Dc_Da_const16sx).setDc(bufLenRegElem.reg).setDa(bufLenRegElem.reg).setConst16sx(memObjOffset)();
    as_.cTRAP(TrapCode::LINKEDMEMORY_OUTOFBOUNDS, JumpCondition::i32LtReg(bufLenRegElem.reg, offsetReg));

    constexpr REG linkedMemPtrReg{WasmABI::REGS::addrScrReg[1]};
    as_.INSTR(LDA_Aa_deref_Ab_off16sx)
        .setAa(linkedMemPtrReg)
        .setAb(WasmABI::REGS::linMem)
        .setOff16sx(SafeInt<16U>::fromConst<-BD::FromEnd::linkedMemPtr>())();

    as_.INSTR(MOVA_Aa_Db).setAa(WasmABI::REGS::addrScrReg[2]).setDb(offsetReg)();
    as_.INSTR(ADDA_Ac_Aa_Ab).setAc(WasmABI::REGS::memLdStReg).setAa(linkedMemPtrReg).setAb(WasmABI::REGS::addrScrReg[2])();

    // WasmABI::REGS::memLdStReg now contains the full raw ptr

    regAllocTracker = RegAllocTracker();
    RegElement const targetRegElem{common_.reqScratchRegProt(resultType, regAllocTracker, false)};

    if (memObjSize == 1U) {
      if (signExtend) {
        as_.INSTR(LDB_Da_deref_Ab_off16sx).setDa(targetRegElem.reg).setAb(WasmABI::REGS::memLdStReg).setOff16sx(SafeInt<16U>::fromConst<0>())();
      } else {
        as_.INSTR(LDBU_Dc_deref_Ab).setDc(targetRegElem.reg).setAb(WasmABI::REGS::memLdStReg)();
      }
    } else if (memObjSize == 2U) {
      as_.INSTR(MOVD_Da_Ab).setDa(targetRegElem.reg).setAb(WasmABI::REGS::memLdStReg)();
      RelPatchObj const unaligned{as_.INSTR(JNZT_Da_n_disp15sx2).setDa(targetRegElem.reg).setN(SafeUInt<5U>::fromConst<0>()).prepJmp()};
      { // Aligned
        if (signExtend) {
          as_.loadHalfwordDRegDerefARegDisp16sx(targetRegElem.reg, WasmABI::REGS::memLdStReg, SafeInt<16U>::fromConst<0>());
        } else {
          as_.INSTR(LDHU_Da_deref_Ab_off16sx).setDa(targetRegElem.reg).setAb(WasmABI::REGS::memLdStReg).setOff16sx(SafeInt<16U>::fromConst<0>())();
        }
      }
      RelPatchObj const end{as_.INSTR(J_disp24sx2).prepJmp()};
      unaligned.linkToHere();
      { // Unaligned
        // Overflow by 1
        as_.loadWordDRegDerefARegDisp16sx(targetRegElem.reg, WasmABI::REGS::memLdStReg, SafeInt<16U>::fromConst<-1>());
        as_.INSTR(signExtend ? EXTR_Dc_Da_pos_width : EXTRU_Dc_Da_pos_width)
            .setDc(targetRegElem.reg)
            .setDa(targetRegElem.reg)
            .setPos(SafeUInt<5>::fromConst<8U>())
            .setWidth(SafeUInt<5U>::fromConst<16U>())();
      }
      end.linkToHere();
    } else if (memObjSize == 4U) {
      as_.INSTR(MOVD_Da_Ab).setDa(targetRegElem.reg).setAb(WasmABI::REGS::memLdStReg)();
      RelPatchObj const unaligned{as_.INSTR(JNZT_Da_n_disp15sx2).setDa(targetRegElem.reg).setN(SafeUInt<5U>::fromConst<0>()).prepJmp()};
      { // Aligned
        as_.INSTR(LDW_Dc_deref_Ab).setDc(targetRegElem.reg).setAb(WasmABI::REGS::memLdStReg)();
      }
      RelPatchObj const end{as_.INSTR(J_disp24sx2).prepJmp()};
      unaligned.linkToHere();
      { // Unaligned
        REG const extraReg{common_.reqScratchRegProt(MachineType::I32, regAllocTracker, false).reg};
        as_.loadWordDRegDerefARegDisp16sx(targetRegElem.reg, WasmABI::REGS::memLdStReg, SafeInt<16U>::fromConst<-1>());

        // No overflow
        as_.loadByteUnsignedDRegDerefARegDisp16sx(extraReg, WasmABI::REGS::memLdStReg, SafeInt<16U>::fromConst<3>());
        as_.INSTR(DEXTR_Dc_Da_Db_pos).setDc(targetRegElem.reg).setDa(extraReg).setDb(targetRegElem.reg).setPos(SafeUInt<5>::fromConst<24U>())();
      }
      end.linkToHere();
    } else { // memObjSize == 8U
      as_.INSTR(MOVD_Da_Ab).setDa(targetRegElem.reg).setAb(WasmABI::REGS::memLdStReg)();
      RelPatchObj const unaligned{as_.INSTR(JNZT_Da_n_disp15sx2).setDa(targetRegElem.reg).setN(SafeUInt<5U>::fromConst<0>()).prepJmp()};
      { // Aligned
        as_.INSTR(LDD_Ea_deref_Ab_off10sx).setEa(targetRegElem.reg).setAb(WasmABI::REGS::memLdStReg).setOff10sx(SafeInt<10>::fromConst<0>())();
      }
      RelPatchObj const end{as_.INSTR(J_disp24sx2).prepJmp()};
      unaligned.linkToHere();
      { // Unaligned
        REG const extraReg{common_.reqScratchRegProt(MachineType::I64, regAllocTracker, false).reg};
        as_.INSTR(LDD_Ea_deref_Ab_off10sx).setEa(extraReg).setAb(WasmABI::REGS::memLdStReg).setOff10sx(SafeInt<10>::fromConst<-1>())();
        as_.INSTR(DEXTR_Dc_Da_Db_pos)
            .setDc(targetRegElem.reg)
            .setDa(RegUtil::getOtherExtReg(extraReg))
            .setDb(extraReg)
            .setPos(SafeUInt<5>::fromConst<24U>())();
        // Overflow by 1
        as_.INSTR(LDD_Ea_deref_Ab_off10sx).setEa(extraReg).setAb(WasmABI::REGS::memLdStReg).setOff10sx(SafeInt<10>::fromConst<1>())();
        as_.INSTR(DEXTR_Dc_Da_Db_pos)
            .setDc(RegUtil::getOtherExtReg(targetRegElem.reg))
            .setDa(RegUtil::getOtherExtReg(extraReg))
            .setDb(extraReg)
            .setPos(SafeUInt<5>::fromConst<8U>())();
      }
      end.linkToHere();
    }

    if ((resultType == MachineType::I64) && (memObjSize <= 4U)) {
      if (signExtend) { // Sign extend 32B to 64B
        as_.INSTR(MUL_Ec_Da_const9sx).setEc(targetRegElem.reg).setDa(targetRegElem.reg).setConst9sx(SafeInt<9U>::fromConst<1>())();
      } else { // Zero extend 32B to 64B
        as_.INSTR(MOV_Da_const4sx).setDa(RegUtil::getOtherExtReg(targetRegElem.reg)).setConst4sx(SafeInt<4U>::fromConst<0>())();
      }
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
      // Get scratch register
      REG const importScratchReg{common_.reqScratchRegProt(MachineType::I32, regAllocTracker, false).reg};
      REG const genScratchReg{common_.reqScratchRegProt(MachineType::I32, regAllocTracker, false).reg};
      as_.MOVimm(genScratchReg, moduleInfo_.tableInitialSize);

      RelPatchObj const inRange{as_.INSTR(JLTU_Da_Db_disp15sx2).setDa(fncIdxReg).setDb(genScratchReg).prepJmp()};

      as_.MOVimm(importScratchReg, 0U);
      RelPatchObj const toEnd{as_.INSTR(J_disp24sx2).prepJmp()};
      inRange.linkToHere();

      // Load pointer to table start to addrScrReg[0]
      as_.INSTR(LDA_Aa_deref_Ab_off16sx)
          .setAa(WasmABI::REGS::addrScrReg[0])
          .setAb(WasmABI::REGS::linMem)
          .setOff16sx(SafeInt<16U>::fromConst<-Basedata::FromEnd::tableAddressOffset>())();

      // Step to the actual table entry we are targeting
      as_.INSTR(ADDSCA_Ac_Ab_Da_nSc)
          .setAc(WasmABI::REGS::addrScrReg[0])
          .setAb(WasmABI::REGS::addrScrReg[0])
          .setDa(fncIdxReg)
          .setNSc(SafeUInt<2>::fromConst<3U>())();

      // Load function offset and check if it's 0 or 0xFFFFFFFF
      as_.loadWordDRegDerefARegDisp16sx(importScratchReg, WasmABI::REGS::addrScrReg[0], SafeInt<16U>::fromConst<0>());
      // Check if the offset is 0 or 0xFFFFFFFF. The following instructions are referred from the -O2 build of gcc.
      as_.INSTR(ADD_Da_const4sx).setDa(importScratchReg).setConst4sx(SafeInt<4U>::fromConst<-1>())();
      as_.INSTR(MOV_Da_const4sx).setDa(genScratchReg).setConst4sx(SafeInt<4U>::fromConst<-3>())();
      as_.INSTR(GEU_Dc_Da_Db).setDc(importScratchReg).setDa(genScratchReg).setDb(importScratchReg)();
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
    regAllocTracker.futureLifts = mask(dstElem.unwrap()) | mask(srcElem.unwrap());
    REG const sizeReg{common_.liftToRegInPlaceProt(*sizeElem, true, regAllocTracker).reg};

    copyValueOfElemToAddrReg(WasmABI::REGS::memLdStReg, *dstElem);
    constexpr REG dstReg{WasmABI::REGS::memLdStReg};
    copyValueOfElemToAddrReg(WasmABI::REGS::addrScrReg[0], *srcElem);
    constexpr REG srcReg{WasmABI::REGS::addrScrReg[0]};

    common_.removeReference(sizeElem);
    common_.removeReference(dstElem);
    common_.removeReference(srcElem);
    static_cast<void>(stack_.erase(sizeElem));
    static_cast<void>(stack_.erase(srcElem));
    static_cast<void>(stack_.erase(dstElem));

    // Extended scratch reg (consisting of two data regs)
    regAllocTracker = RegAllocTracker();
    regAllocTracker.writeProtRegs = mask(sizeReg, false);
    REG const scratchReg{common_.reqScratchRegProt(MachineType::I64, regAllocTracker, false).reg};

    // addrScrReg[2] now contains a copy of sizeReg
    as_.INSTR(MOVA_Aa_Db).setAa(WasmABI::REGS::addrScrReg[2]).setDb(sizeReg)();

    // Add size to destination and check for an overflow
    as_.INSTR(ADDA_Aa_Ab).setAa(dstReg).setAb(WasmABI::REGS::addrScrReg[2])();
    // Move to dataReg because we cannot do a lot of comparisons in address regs
    as_.INSTR(MOVD_Da_Ab).setDa(scratchReg).setAb(dstReg)();

    as_.cTRAP(TrapCode::LINMEM_OUTOFBOUNDSACCESS, JumpCondition::u32LtReg(scratchReg, sizeReg));

    // Check bounds and get absolute destination address in a register, can use 0 as memObjSize since we already added
    // it to the offset
    emitLinMemBoundsCheck(scratchReg);
    as_.INSTR(ADDA_Aa_Ab).setAa(WasmABI::REGS::memLdStReg).setAb(WasmABI::REGS::linMem)(); // Calculate the actual pointer
    // Subtract size again from dstReg
    as_.INSTR(SUBA_Ac_Aa_Ab).setAc(dstReg).setAa(dstReg).setAb(WasmABI::REGS::addrScrReg[2])();

    // Absolute target pointer is now in dstReg (addrReg), size is in sizeReg (dataReg), src offset is in srcReg
    // (addrReg), extScratchReg data scratch register and addrScratchReg can be used as address scratch register (all
    // writable)

    // Load length of linked memory into scratch register
    as_.loadWordDRegDerefARegDisp16sx(scratchReg, WasmABI::REGS::linMem, SafeInt<16U>::fromConst<-BD::FromEnd::linkedMemLen>());

    REG const secExtRegScratchReg{RegUtil::getOtherExtReg(scratchReg)};

    // Check bounds of src
    as_.cTRAP(TrapCode::LINKEDMEMORY_MUX, JumpCondition::u32LtReg(scratchReg, sizeReg));

    as_.INSTR(SUB_Da_Db).setDa(scratchReg).setDb(sizeReg)();
    as_.INSTR(MOVD_Da_Ab).setDa(secExtRegScratchReg).setAb(srcReg)();

    as_.cTRAP(TrapCode::LINKEDMEMORY_MUX, JumpCondition::u32LtReg(scratchReg, secExtRegScratchReg));

    // Both are in bounds, let's copy the data

    // Load linked memory start pointer and add it to srcReg
    as_.INSTR(LDA_Aa_deref_Ab_off16sx)
        .setAa(WasmABI::REGS::addrScrReg[1])
        .setAb(WasmABI::REGS::linMem)
        .setOff16sx(SafeInt<16U>::fromConst<-BD::FromEnd::linkedMemPtr>())();
    as_.INSTR(ADDA_Aa_Ab).setAa(srcReg).setAb(WasmABI::REGS::addrScrReg[1])();

    constexpr bool canOverlap{false};
    emitMemcpyNoBoundsCheck(dstReg, srcReg, sizeReg, scratchReg, canOverlap);
    break;
  }
  case BuiltinFunction::TRACE_POINT: {
    throw FeatureNotSupportedException{ErrorCode::Not_implemented};
  }
  case BuiltinFunction::UNDEFINED:
  // GCOVR_EXCL_START
  default: {
    UNREACHABLE(return, "Unknown BuiltinFunction");
  }
    // GCOVR_EXCL_STOP
  }
}
#endif

void Backend::emitMemcpyWithConstSizeNoBoundsCheck(REG const dstReg, REG const srcReg, REG const sizeReg, uint32_t const sizeToCopy,
                                                   REG const scratchReg, bool const canOverlap) const {
  REG const extScratchReg{RegUtil::getOtherExtReg(scratchReg)};

  as_.INSTR(MOVD_Da_Ab).setDa(scratchReg).setAb(srcReg)();
  as_.INSTR(MOVD_Da_Ab).setDa(extScratchReg).setAb(dstReg)();
  RelPatchObj const reverse{canOverlap ? as_.prepareJump(JumpCondition::u32LtReg(scratchReg, extScratchReg)) : RelPatchObj{}};
  // src >= dst

  if (sizeToCopy >= 8U) {
    // If one is aligned and the other is not (LSB is not the same for src and dst), we can only do a bytewise copy
    as_.INSTR(XOR_Da_Db).setDa(scratchReg).setDb(extScratchReg)();
    RelPatchObj const toBytewiseCopyForward{as_.INSTR(JNZT_Da_n_disp15sx2).setDa(scratchReg).setN(SafeUInt<5U>::fromConst<0U>()).prepJmp()};
    // Alignment is the same for dst and src, copy a single byte so it's aligned if they are unaligned
    RelPatchObj const bothAlignedForward{as_.INSTR(JZT_Da_n_disp15sx2).setDa(extScratchReg).setN(SafeUInt<5U>::fromConst<0U>()).prepJmp()};
    as_.INSTR(LDBU_Dc_deref_Ab_postinc).setDc(scratchReg).setAb(srcReg)();
    as_.INSTR(ADDI_Dc_Da_const16sx).setDc(sizeReg).setDa(sizeReg).setConst16sx(SafeInt<16U>::fromConst<-1>())();
    as_.INSTR(STB_deref_Ab_Da_postinc).setAb(dstReg).setDa(scratchReg)();

    RelPatchObj lessThan8Forward{};
    if (sizeToCopy == 8U) {
      // Check if we are now below 8 bytes after alignment(Only original size=8 can reach here)
      lessThan8Forward = as_.INSTR(JLTU_Da_const4zx_disp15sx2).setDa(sizeReg).setConst4zx(SafeUInt<4U>::fromConst<8U>()).prepJmp();
    }
    bothAlignedForward.linkToHere();
    // Copy 8 bytes
    uint32_t const copy8Forward{output_.size()};
    as_.INSTR(LDD_Ea_deref_Ab_off10sx_postinc).setEa(scratchReg).setAb(srcReg).setOff10sx(SafeInt<10U>::fromConst<8>())();
    as_.INSTR(ADDI_Dc_Da_const16sx).setDc(sizeReg).setDa(sizeReg).setConst16sx(SafeInt<16U>::fromConst<-8>())();
    as_.INSTR(STD_deref_Ab_off10sx_Ea_postinc).setAb(dstReg).setOff10sx(SafeInt<10U>::fromConst<8>()).setEa(scratchReg)();
    RelPatchObj const toCopy8Forward{as_.INSTR(JGEU_Da_const4zx_disp15sx2).setDa(sizeReg).setConst4zx(SafeUInt<4U>::fromConst<8U>()).prepJmp()};
    toCopy8Forward.linkToBinaryPos(copy8Forward);

    if (lessThan8Forward.isInitialized()) {
      assert(sizeToCopy == 8U);
      lessThan8Forward.linkToHere();
    }
    toBytewiseCopyForward.linkToHere();
  }

  // Check if (remaining) size is at least 1
  RelPatchObj const quickFinishedForward{as_.INSTR(JLTU_Da_const4zx_disp15sx2).setDa(sizeReg).setConst4zx(SafeUInt<4U>::fromConst<1U>()).prepJmp()};
  // Copy 1 byte
  uint32_t const copy1Forward{output_.size()};
  as_.INSTR(LDBU_Dc_deref_Ab_postinc).setDc(scratchReg).setAb(srcReg)();
  as_.INSTR(STB_deref_Ab_Da_postinc).setAb(dstReg).setDa(scratchReg)();
  as_.INSTR(JNED_Da_const4sx_disp15sx2).setDa(sizeReg).setConst4sx(SafeInt<4U>::fromConst<1U>()).prepJmp().linkToBinaryPos(copy1Forward);

  if (canOverlap) {
    RelPatchObj const finishedForward{as_.INSTR(J_disp24sx2).prepJmp()};
    // src < dst
    reverse.linkToHere();
    // src in scratchReg, dst in extScratchReg
    as_.INSTR(ADD_Da_Db).setDa(scratchReg).setDb(sizeReg)();
    as_.INSTR(MOVA_Aa_Db).setAa(srcReg).setDb(scratchReg)();
    as_.INSTR(ADD_Da_Db).setDa(extScratchReg).setDb(sizeReg)();
    as_.INSTR(MOVA_Aa_Db).setAa(dstReg).setDb(extScratchReg)();

    if (sizeToCopy >= 8U) {
      // If one is aligned and the other is not (LSB is not the same for src and dst), we can only do a bytewise copy
      as_.INSTR(XOR_Da_Db).setDa(scratchReg).setDb(extScratchReg)();
      RelPatchObj const toBytewiseCopyInReverse{as_.INSTR(JNZT_Da_n_disp15sx2).setDa(scratchReg).setN(SafeUInt<5U>::fromConst<0U>()).prepJmp()};
      // Alignment is the same for dst and src, copy a single byte so it's aligned if they are unaligned
      RelPatchObj const bothAlignedInReverse{as_.INSTR(JZT_Da_n_disp15sx2).setDa(extScratchReg).setN(SafeUInt<5U>::fromConst<0U>()).prepJmp()};
      as_.INSTR(LDBU_Da_deref_Ab_off10sx_preinc).setDa(scratchReg).setAb(srcReg).setOff10sx(SafeInt<10U>::fromConst<-1>())();
      as_.INSTR(ADDI_Dc_Da_const16sx).setDc(sizeReg).setDa(sizeReg).setConst16sx(SafeInt<16U>::fromConst<-1>())();
      as_.INSTR(STB_deref_Ab_off10sx_Da_preinc).setAb(dstReg).setOff10sx(SafeInt<10U>::fromConst<-1>()).setDa(scratchReg)();

      RelPatchObj lessThan8Forward{};
      if (sizeToCopy == 8U) {
        // Check if we are now below 8 bytes after alignment(Only original size=8 can reach here)
        lessThan8Forward = as_.INSTR(JLTU_Da_const4zx_disp15sx2).setDa(sizeReg).setConst4zx(SafeUInt<4U>::fromConst<8U>()).prepJmp();
      }
      bothAlignedInReverse.linkToHere();

      // Copy 8 bytes
      uint32_t const copy8InReverse{output_.size()};
      as_.INSTR(LDD_Ea_deref_Ab_off10sx_preinc).setEa(scratchReg).setAb(srcReg).setOff10sx(SafeInt<10U>::fromConst<-8>())();
      as_.INSTR(ADDI_Dc_Da_const16sx).setDc(sizeReg).setDa(sizeReg).setConst16sx(SafeInt<16U>::fromConst<-8>())();
      as_.INSTR(STD_deref_Ab_off10sx_Ea_preinc).setAb(dstReg).setOff10sx(SafeInt<10U>::fromConst<-8>()).setEa(scratchReg)();
      as_.INSTR(JGEU_Da_const4zx_disp15sx2).setDa(sizeReg).setConst4zx(SafeUInt<4U>::fromConst<8U>()).prepJmp().linkToBinaryPos(copy8InReverse);

      if (lessThan8Forward.isInitialized()) {
        assert(sizeToCopy == 8U);
        lessThan8Forward.linkToHere();
      }
      toBytewiseCopyInReverse.linkToHere();
    }

    // Check if (remaining) size is at least 1
    RelPatchObj const quickFinishedInReverse{
        as_.INSTR(JLTU_Da_const4zx_disp15sx2).setDa(sizeReg).setConst4zx(SafeUInt<4U>::fromConst<1U>()).prepJmp()};

    // Copy 1 byte
    uint32_t const copy1InReverse{output_.size()};
    as_.INSTR(LDBU_Da_deref_Ab_off10sx_preinc).setDa(scratchReg).setAb(srcReg).setOff10sx(SafeInt<10U>::fromConst<-1>())();
    as_.INSTR(STB_deref_Ab_off10sx_Da_preinc).setAb(dstReg).setOff10sx(SafeInt<10U>::fromConst<-1>()).setDa(scratchReg)();
    as_.INSTR(JNED_Da_const4sx_disp15sx2).setDa(sizeReg).setConst4sx(SafeInt<4U>::fromConst<1U>()).prepJmp().linkToBinaryPos(copy1InReverse);

    quickFinishedInReverse.linkToHere();
    finishedForward.linkToHere();
  }

  quickFinishedForward.linkToHere();
}
void Backend::emitMemcpyNoBoundsCheck(REG const dstReg, REG const srcReg, REG const sizeReg, REG const scratchReg, bool const canOverlap) const {
  REG const extScratchReg{RegUtil::getOtherExtReg(scratchReg)};

  as_.INSTR(MOVD_Da_Ab).setDa(scratchReg).setAb(srcReg)();
  as_.INSTR(MOVD_Da_Ab).setDa(extScratchReg).setAb(dstReg)();

  RelPatchObj const reverse{canOverlap ? as_.prepareJump(JumpCondition::u32LtReg(scratchReg, extScratchReg)) : RelPatchObj{}};
  // src >= dst

  // Check if (remaining) size is at least 8
  RelPatchObj const lessThan8Forward{as_.INSTR(JLTU_Da_const4zx_disp15sx2).setDa(sizeReg).setConst4zx(SafeUInt<4U>::fromConst<8U>()).prepJmp()};
  // If one is aligned and the other is not (LSB is not the same for src and dst), we can only do a bytewise copy
  as_.INSTR(XOR_Da_Db).setDa(scratchReg).setDb(extScratchReg)();
  RelPatchObj const toBytewiseCopyForward{as_.INSTR(JNZT_Da_n_disp15sx2).setDa(scratchReg).setN(SafeUInt<5U>::fromConst<0U>()).prepJmp()};
  // Alignment is the same for dst and src, copy a single byte so it's aligned if they are unaligned
  RelPatchObj const bothAlignedForward{as_.INSTR(JZT_Da_n_disp15sx2).setDa(extScratchReg).setN(SafeUInt<5U>::fromConst<0U>()).prepJmp()};
  as_.INSTR(LDBU_Dc_deref_Ab_postinc).setDc(scratchReg).setAb(srcReg)();
  as_.INSTR(ADDI_Dc_Da_const16sx).setDc(sizeReg).setDa(sizeReg).setConst16sx(SafeInt<16U>::fromConst<-1>())();
  as_.INSTR(STB_deref_Ab_Da_postinc).setAb(dstReg).setDa(scratchReg)();
  // Check if we are now below 8 bytes
  RelPatchObj const lessThan82Forward{as_.INSTR(JLTU_Da_const4zx_disp15sx2).setDa(sizeReg).setConst4zx(SafeUInt<4U>::fromConst<8U>()).prepJmp()};
  bothAlignedForward.linkToHere();
  // IDEA: Could maybe use quadword LD.DD/ST.DD on TC4x?
  // CAUTION: LD.DD/ST.DD have a different alignment when the address points to flash compared to RAM (see
  // https://jira.cc.bmwgroup.net/browse/CASP-3155) What if integrator provides a pointer to flash as linked memory?
  // Copy 8 bytes
  uint32_t const copy8Forward{output_.size()};
  as_.INSTR(LDD_Ea_deref_Ab_off10sx_postinc).setEa(scratchReg).setAb(srcReg).setOff10sx(SafeInt<10U>::fromConst<8>())();
  as_.INSTR(ADDI_Dc_Da_const16sx).setDc(sizeReg).setDa(sizeReg).setConst16sx(SafeInt<16U>::fromConst<-8>())();
  as_.INSTR(STD_deref_Ab_off10sx_Ea_postinc).setAb(dstReg).setOff10sx(SafeInt<10U>::fromConst<8>()).setEa(scratchReg)();
  RelPatchObj const toCopy8Forward{as_.INSTR(JGEU_Da_const4zx_disp15sx2).setDa(sizeReg).setConst4zx(SafeUInt<4U>::fromConst<8U>()).prepJmp()};
  toCopy8Forward.linkToBinaryPos(copy8Forward);

  lessThan8Forward.linkToHere();
  lessThan82Forward.linkToHere();
  toBytewiseCopyForward.linkToHere();

  // Check if (remaining) size is at least 1
  RelPatchObj const quickFinishedForward{as_.INSTR(JLTU_Da_const4zx_disp15sx2).setDa(sizeReg).setConst4zx(SafeUInt<4U>::fromConst<1U>()).prepJmp()};

  // Copy 1 byte
  uint32_t const copy1Forward{output_.size()};
  as_.INSTR(LDBU_Dc_deref_Ab_postinc).setDc(scratchReg).setAb(srcReg)();
  as_.INSTR(STB_deref_Ab_Da_postinc).setAb(dstReg).setDa(scratchReg)();
  as_.INSTR(JNED_Da_const4sx_disp15sx2).setDa(sizeReg).setConst4sx(SafeInt<4U>::fromConst<1U>()).prepJmp().linkToBinaryPos(copy1Forward);
  if (canOverlap) {
    RelPatchObj const finishedForward{as_.INSTR(J_disp24sx2).prepJmp()};
    // src < dst
    reverse.linkToHere();
    // src in scratchReg, dst in extScratchReg
    as_.INSTR(ADD_Da_Db).setDa(scratchReg).setDb(sizeReg)();
    as_.INSTR(MOVA_Aa_Db).setAa(srcReg).setDb(scratchReg)();
    as_.INSTR(ADD_Da_Db).setDa(extScratchReg).setDb(sizeReg)();
    as_.INSTR(MOVA_Aa_Db).setAa(dstReg).setDb(extScratchReg)();

    // Check if (remaining) size is at least 8
    RelPatchObj const lessThan8InReverse{as_.INSTR(JLTU_Da_const4zx_disp15sx2).setDa(sizeReg).setConst4zx(SafeUInt<4U>::fromConst<8U>()).prepJmp()};
    // If one is aligned and the other is not (LSB is not the same for src and dst), we can only do a bytewise copy
    as_.INSTR(XOR_Da_Db).setDa(scratchReg).setDb(extScratchReg)();
    RelPatchObj const toBytewiseCopyInReverse{as_.INSTR(JNZT_Da_n_disp15sx2).setDa(scratchReg).setN(SafeUInt<5U>::fromConst<0U>()).prepJmp()};
    // Alignment is the same for dst and src, copy a single byte so it's aligned if they are unaligned
    RelPatchObj const bothAlignedInReverse{as_.INSTR(JZT_Da_n_disp15sx2).setDa(extScratchReg).setN(SafeUInt<5U>::fromConst<0U>()).prepJmp()};
    as_.INSTR(LDBU_Da_deref_Ab_off10sx_preinc).setDa(scratchReg).setAb(srcReg).setOff10sx(SafeInt<10U>::fromConst<-1>())();
    as_.INSTR(ADDI_Dc_Da_const16sx).setDc(sizeReg).setDa(sizeReg).setConst16sx(SafeInt<16U>::fromConst<-1>())();
    as_.INSTR(STB_deref_Ab_off10sx_Da_preinc).setAb(dstReg).setOff10sx(SafeInt<10U>::fromConst<-1>()).setDa(scratchReg)();
    // Check if we are now below 8 bytes
    RelPatchObj const lessThan82InReverse{as_.INSTR(JLTU_Da_const4zx_disp15sx2).setDa(sizeReg).setConst4zx(SafeUInt<4U>::fromConst<8U>()).prepJmp()};
    bothAlignedInReverse.linkToHere();
    // Copy 8 bytes
    uint32_t const copy8InReverse{output_.size()};
    as_.INSTR(LDD_Ea_deref_Ab_off10sx_preinc).setEa(scratchReg).setAb(srcReg).setOff10sx(SafeInt<10U>::fromConst<-8>())();
    as_.INSTR(ADDI_Dc_Da_const16sx).setDc(sizeReg).setDa(sizeReg).setConst16sx(SafeInt<16U>::fromConst<-8>())();
    as_.INSTR(STD_deref_Ab_off10sx_Ea_preinc).setAb(dstReg).setOff10sx(SafeInt<10U>::fromConst<-8>()).setEa(scratchReg)();
    as_.INSTR(JGEU_Da_const4zx_disp15sx2).setDa(sizeReg).setConst4zx(SafeUInt<4U>::fromConst<8U>()).prepJmp().linkToBinaryPos(copy8InReverse);

    lessThan8InReverse.linkToHere();
    lessThan82InReverse.linkToHere();
    toBytewiseCopyInReverse.linkToHere();

    // Check if (remaining) size is at least 1
    RelPatchObj const quickFinishedInReverse{
        as_.INSTR(JLTU_Da_const4zx_disp15sx2).setDa(sizeReg).setConst4zx(SafeUInt<4U>::fromConst<1U>()).prepJmp()};

    // Copy 1 byte
    uint32_t const copy1InReverse{output_.size()};
    as_.INSTR(LDBU_Da_deref_Ab_off10sx_preinc).setDa(scratchReg).setAb(srcReg).setOff10sx(SafeInt<10U>::fromConst<-1>())();
    as_.INSTR(STB_deref_Ab_off10sx_Da_preinc).setAb(dstReg).setOff10sx(SafeInt<10U>::fromConst<-1>()).setDa(scratchReg)();
    as_.INSTR(JNED_Da_const4sx_disp15sx2).setDa(sizeReg).setConst4sx(SafeInt<4U>::fromConst<1U>()).prepJmp().linkToBinaryPos(copy1InReverse);

    quickFinishedInReverse.linkToHere();
    finishedForward.linkToHere();
  }
  quickFinishedForward.linkToHere();
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
  RegElement const scratchRegElem{common_.reqScratchRegProt(MachineType::I32, regAllocTracker, false)};

  // Saturate indexReg to numBranchTargets
  as_.MOVimm(scratchRegElem.reg, numBranchTargets);
  RelPatchObj const inRange{as_.INSTR(JGEU_Da_Db_disp15sx2).setDa(scratchRegElem.reg).setDb(indexReg).prepJmp()};
  as_.MOVimm(indexReg, numBranchTargets);
  inRange.linkToHere();

  RelPatchObj const toTableStart{as_.loadPCRelAddr(WasmABI::REGS::addrScrReg[0], WasmABI::REGS::addrScrReg[1])};
  // addrScrReg[0] now points to table start, now load delta from table start to indexReg by accessing table

  as_.INSTR(ADDSCA_Ac_Ab_Da_nSc)
      .setAc(WasmABI::REGS::addrScrReg[1])
      .setAb(WasmABI::REGS::addrScrReg[0])
      .setDa(indexReg)
      .setNSc(SafeUInt<2>::fromConst<2U>())();
  as_.INSTR(LDW_Dc_deref_Ab).setDc(scratchRegElem.reg).setAb(WasmABI::REGS::addrScrReg[1])();
  // addrScrReg[1] now contains the offset of the branch from table start, addrScrReg[0] still contains the table start
  // address

  // Calculate the resulting address with the branch sequence
  as_.INSTR(ADDSCA_Ac_Ab_Da_nSc)
      .setAc(WasmABI::REGS::addrScrReg[0])
      .setAb(WasmABI::REGS::addrScrReg[0])
      .setDa(scratchRegElem.reg)
      .setNSc(SafeUInt<2>::fromConst<0>())();
  as_.INSTR(JI_Aa).setAa(WasmABI::REGS::addrScrReg[0])();

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

void Backend::copyValueOfElemToAddrReg(REG const addrReg, StackElement const &elem) const {
  assert(!RegUtil::isDATA(addrReg) && "Only address register allowed");

  VariableStorage const addrStorage{moduleInfo_.getStorage(elem)};

  switch (addrStorage.type) {
  case StorageType::CONSTANT: {
    as_.MOVimm(addrReg, elem.data.constUnion.u32);
    break;
  }
  case StorageType::REGISTER: {
    as_.INSTR(MOVA_Aa_Db).setAa(addrReg).setDb(addrStorage.location.reg)();
    break;
  }
  default: { // Memory
    RegDisp<16U> const srcRegDisp{getMemRegDisp<16U>(addrStorage, addrReg)};
    as_.emitLoadDerefOff16sx(addrReg, srcRegDisp.reg, srcRegDisp.disp);
    break;
  }
  }
}

void Backend::emitExtensionRequestFunction() {
  moduleInfo_.helperFunctionBinaryPositions.extensionRequest = output_.size();
  constexpr REG helperReg{REG::D15};
  // Spill D0
  as_.INSTR(STW_deref_Ab_off10sx_Da_preinc).setAb(REG::SP).setOff10sx(SafeInt<10>::fromConst<-4>()).setDa(helperReg)();

  // Align stack pointer to 16-word boundary for STLCX/STUCX/LDLCX/LDUCX
  as_.INSTR(MOVD_Da_Ab).setDa(helperReg).setAb(REG::SP)();
  as_.andWordDcDaConst9zx(helperReg, helperReg, SafeUInt<9U>::fromConst<0b11'1111U>());
  as_.INSTR(MOVA_Aa_Db).setAa(WasmABI::REGS::memSize).setDb(helperReg)();
  as_.INSTR(SUBA_Ac_Aa_Ab).setAc(REG::SP).setAa(REG::SP).setAb(WasmABI::REGS::memSize)();

  // Reserve space on stack and spill all volatile registers since we will call a native function
  as_.INSTR(LEA_Aa_deref_Ab_off16sx)
      .setAa(REG::SP)
      .setAb(REG::SP)
      .setOff16sx(SafeInt<16U>::fromConst<-(16 + (1 * static_cast<int32_t>(NativeABI::contextRegisterSize)))>())();

  // Store alignment difference on stack
  as_.storeWordDerefARegDisp16sxDReg(helperReg, REG::SP, SafeInt<16U>::fromConst<8>());

  // We can use REGS::memSize as scratch register since it will be clobbered and re-setup anyway
  as_.checkStackFence(helperReg, WasmABI::REGS::memSize); // SP change
  as_.INSTR(STLCX_Ab_off10sx).setAb(REG::SP).setOff10sx(SafeInt<10>::fromConst<16>())();

  {
    uint32_t const basedataLength{moduleInfo_.getBasedataLength()};
    {
      // Load arguments for the extension helper
      as_.INSTR(MOVD_Da_Ab).setDa(NABI::paramRegs[0]).setAb(WasmABI::REGS::memLdStReg)();
      as_.INSTR(MOV_Da_const4sx).setDa(RegUtil::getOtherExtReg(NABI::paramRegs[0])).setConst4sx(SafeInt<4U>::fromConst<0>())(); // paramRegs[1]

      as_.MOVimm(NABI::paramRegs[2], basedataLength);

      as_.INSTR(MOVAA_Aa_Ab).setAa(NABI::addrParamRegs[0]).setAb(WasmABI::REGS::linMem)();
    }

    // Call extension request
    as_.INSTR(LDA_Aa_deref_Ab_off16sx)
        .setAa(NABI::addrParamRegs[1])
        .setAb(WasmABI::REGS::linMem)
        .setOff16sx(SafeInt<16U>::fromConst<-BD::FromEnd::memoryHelperPtr>())();

    // Native call will clobber WasmABI::REGS::linMem
    as_.INSTR(STA_deref_Ab_Aa).setAb(REG::SP).setAa(WasmABI::REGS::linMem)();

    // Needs a call, because native function will return with RET
    as_.INSTR(CALLI_Aa).setAa(NABI::addrParamRegs[1])();

    // Move return value to another register for now
    as_.INSTR(MOVD_Da_Ab).setDa(helperReg).setAb(NABI::addrRetReg)();
    // Restore old WasmABI::REGS::linMem for traps because the native call clobbered it
    as_.INSTR(LDA_Ac_deref_Ab).setAc(WasmABI::REGS::linMem).setAb(REG::SP)();

    //  Check the return value. If it's zero extension of memory failed
    as_.cTRAP(TrapCode::LINMEM_COULDNOTEXTEND, JumpCondition::i32EqConst4sx(helperReg, SafeInt<4U>::fromConst<0>()));

    // Check if the return value is all ones: In this case the module tried to access memory beyond the allowed number
    // of (Wasm) pages
    as_.cTRAP(TrapCode::LINMEM_OUTOFBOUNDSACCESS, JumpCondition::i32EqConst4sx(helperReg, SafeInt<4U>::fromConst<-1>()));

    // If all succeeded, the return value now points to the start of the job memory
    as_.INSTR(MOVA_Aa_Db).setAa(WasmABI::REGS::linMem).setDb(helperReg)();

    // Calculate the new base of the linear memory by adding basedataLength to the new memory base and store it in
    // REGS::linMem
    as_.addImmToReg(WasmABI::REGS::linMem, basedataLength);
  }

  // Spill the new WasmABI::REGS::linMem register so it will not be reverted my LDLCX
  as_.INSTR(STA_deref_Ab_Aa).setAb(REG::SP).setAa(WasmABI::REGS::linMem)();
  // Restore the link register and all other previously spilled registers, then unwind the stack
  as_.INSTR(LDLCX_Ab_off10sx).setAb(REG::SP).setOff10sx(SafeInt<10>::fromConst<16>())();
  // Restore the new WasmABI::REGS::linMem register
  as_.INSTR(LDA_Ac_deref_Ab).setAc(WasmABI::REGS::linMem).setAb(REG::SP)();

  // Load alignment difference and add it to the stack pointer
  as_.INSTR(LDA_Aa_deref_Ab_off16sx).setAa(WasmABI::REGS::memSize).setAb(REG::SP).setOff16sx(SafeInt<16U>::fromConst<8>())();
  as_.INSTR(ADDA_Aa_Ab).setAa(REG::SP).setAb(WasmABI::REGS::memSize)();

  as_.INSTR(LEA_Aa_deref_Ab_off16sx)
      .setAa(REG::SP)
      .setAb(REG::SP)
      .setOff16sx(SafeInt<16U>::fromConst<16 + (1 * static_cast<int32_t>(NativeABI::contextRegisterSize))>())();

  // Load the actual memory size, maybe it changed
  as_.INSTR(LDA_Aa_deref_Ab_off16sx)
      .setAa(WasmABI::REGS::memSize)
      .setAb(WasmABI::REGS::linMem)
      .setOff16sx(SafeInt<16U>::fromConst<-BD::FromEnd::actualLinMemByteSize>())();

  // WasmABI::REGS::memLdStReg points to end of data (as offset, after last byte)
  // WasmABI::REGS::memSize is actual full size of linear memory

  // Restore helper reg
  as_.INSTR(LDW_Dc_deref_Ab_postinc).setDc(helperReg).setAb(REG::SP)();

  as_.INSTR(FRET)();
}

RelPatchObj Backend::prepareLinMemAddr(REG const tempDReg, REG addressDReg, uint32_t const offset, uint8_t const memObjSize) const {
  assert(moduleInfo_.helperFunctionBinaryPositions.extensionRequest != 0xFF'FF'FF'FF && "Extension request wrapper has not been produced yet");
  assert(RegUtil::isDATA(tempDReg) && "tempDReg must be a data register");
  assert(((addressDReg == REG::NONE) || RegUtil::isDATA(addressDReg)) && "addressDReg must be a data register");

  if (offset >= (1_U32 << 30_U32)) {
    as_.TRAP(TrapCode::LINMEM_OUTOFBOUNDSACCESS);
    return RelPatchObj();
  }

  if (addressDReg == REG::NONE) {
    addressDReg = tempDReg;
    as_.INSTR(MOVD_Da_Ab).setDa(tempDReg).setAb(WasmABI::REGS::memLdStReg)();
  }

  // Do not add if highest bit of address is already set. We could produce an overflow and any address above 2GB is not
  // supported on TriCore anyway We can use JNZT addressReg, 31, ... or JLTZ addressReg, ... (CAUTION: Only 32 bytes
  // range, call emitLinBenBoundsCheck right after this function)
  RelPatchObj const directErr{as_.INSTR(JLTZ_Db_disp4zx2).setDb(addressDReg).prepJmp()};
  as_.addImmToReg(WasmABI::REGS::memLdStReg, offset + memObjSize); // Add immediate offset
  return directErr;
}

void Backend::emitLinMemBoundsCheck(REG const tempDReg, RelPatchObj const *const toExtensionRequest) const {
  as_.INSTR(GEA_Dc_Aa_Ab).setDc(tempDReg).setAa(WasmABI::REGS::memSize).setAb(WasmABI::REGS::memLdStReg)();
  RelPatchObj const withinBounds{as_.INSTR(JNZ_Db_disp4zx2).setDb(tempDReg).prepJmp()}; // Can use 16-bit variant because it's a short jump

  if ((toExtensionRequest != nullptr) && toExtensionRequest->isInitialized()) {
    toExtensionRequest->linkToHere();
  }
  as_.INSTR(FCALL_disp24sx2).prepJmp().linkToBinaryPos(moduleInfo_.helperFunctionBinaryPositions.extensionRequest);

  withinBounds.linkToHere();
}

StackElement Backend::executeLinearMemoryLoad(OPCode const opcode, uint32_t const offset, Stack::iterator const addrElem,
                                              StackElement const *const targetHint) {
  assert(moduleInfo_.hasMemory && "Memory not defined");
  // coverity[autosar_cpp14_a8_5_2_violation]
  constexpr auto memObjSizes = make_array(4_U8, 8_U8, 4_U8, 8_U8, 1_U8, 1_U8, 2_U8, 2_U8, 1_U8, 1_U8, 2_U8, 2_U8, 4_U8, 4_U8);

  // coverity[autosar_cpp14_a8_5_2_violation]
  constexpr auto signExtends = make_array(false, false, false, false, true, false, true, false, true, false, true, false, true, false);

  uint8_t const memObjSize{memObjSizes[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_LOAD)]};
  MachineType const resultType{getLoadResultType(opcode)};
  bool const signExtend{signExtends[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_LOAD)]};

  copyValueOfElemToAddrReg(WasmABI::REGS::memLdStReg, *addrElem);
  VariableStorage const addressStorage{moduleInfo_.getStorage(*addrElem)};
  REG const addressDReg{(addressStorage.type == StorageType::REGISTER) ? addressStorage.location.reg : REG::NONE};
  RegAllocTracker regAllocTracker{};

  StackElement const *const verifiedTargetHint{(getUnderlyingRegIfSuitable(targetHint, resultType, RegMask::none()) != REG::NONE) ? targetHint
                                                                                                                                  : nullptr};
  RegElement const targetRegElem{common_.reqScratchRegProt(resultType, verifiedTargetHint, regAllocTracker, false)};

  regAllocTracker.writeProtRegs.mask(mask(addressDReg, false));
  REG const checkHelperReg{common_.reqScratchRegProt(MachineType::I32, regAllocTracker, false).reg};

  // We use the targetRegElem directly as scratch register
  RelPatchObj const directErr{prepareLinMemAddr(targetRegElem.reg, addressDReg, offset, memObjSize)};
  // WasmABI::REGS::memLdStReg now points to end of data that should be accessed (as offset, after last byte)
  // WasmABI::REGS::memSize is actual full size of linear memory
  emitLinMemBoundsCheck(checkHelperReg, &directErr);

  ///< Compile-time constant optimization for LdSt address:
  // if (address is compile-time constant) && (found it aligned)
  //    Don't emit alignment check relative patch at runtime
  //    if(addrOffset is in_range of load.offset)
  //      Emit the load directly with the compile-time address offset, which means `add memLdStReg, linMem` is unnecessary.
  //      Use compileTimeAddrOffset as total offset to linMem directly.
  //    else
  //      Use register to store the offset
  // else
  //     Not compile-time constant: Emit alignment check relative patch at runtime
  //     Constant && notAligned: Not handled separately with the situation that will not be emitted normally(same with not constant)
  //
  if (addressStorage.type == StorageType::CONSTANT) {
    int64_t const compileTimeAddrOffset{static_cast<int64_t>(static_cast<uint32_t>(addrElem->data.constUnion.u32 + offset))};
    assert(compileTimeAddrOffset >= 0 && "always");
    if (memObjSize == 1U) {
      SignedInRangeCheck<16U> const inRangeCheck{SignedInRangeCheck<16U>::check(compileTimeAddrOffset)};
      if (inRangeCheck.inRange()) {
        if (signExtend) {
          as_.INSTR(LDB_Da_deref_Ab_off16sx).setDa(targetRegElem.reg).setAb(WasmABI::REGS::linMem).setOff16sx(inRangeCheck.safeInt())();
        } else {
          as_.loadByteUnsignedDRegDerefARegDisp16sx(targetRegElem.reg, WasmABI::REGS::linMem, inRangeCheck.safeInt());
        }

      } else {
        // Calculate the actual pointer
        as_.INSTR(ADDA_Aa_Ab).setAa(WasmABI::REGS::memLdStReg).setAb(WasmABI::REGS::linMem)();
        if (signExtend) {
          as_.INSTR(LDB_Da_deref_Ab_off16sx).setDa(targetRegElem.reg).setAb(WasmABI::REGS::memLdStReg).setOff16sx(SafeInt<16U>::fromConst<-1>())();
        } else {
          as_.loadByteUnsignedDRegDerefARegDisp16sx(targetRegElem.reg, WasmABI::REGS::memLdStReg, SafeInt<16U>::fromConst<-1>());
        }
      }

      // No alignment check needed, we are done

      if (resultType == MachineType::I64) {
        if (signExtend) { // Sign extend 32B to 64B
          as_.INSTR(MUL_Ec_Da_const9sx).setEc(targetRegElem.reg).setDa(targetRegElem.reg).setConst9sx(SafeInt<9U>::fromConst<1>())();
        } else { // Zero extend 32B to 64B
          as_.INSTR(MOV_Da_const4sx).setDa(RegUtil::getOtherExtReg(targetRegElem.reg)).setConst4sx(SafeInt<4U>::fromConst<0>())();
        }
      }

      return targetRegElem.elem;
    }
    // write else branch directly to reduce depth of nest
    // Alignment of the base of linear memory is guaranteed
    if ((compileTimeAddrOffset % 2) == 0) { // compileTimeAligned
      if (memObjSize == 2U) {
        SignedInRangeCheck<16U> const inRangeCheck{SignedInRangeCheck<16U>::check(compileTimeAddrOffset)};
        if (inRangeCheck.inRange()) {
          if (signExtend) {
            as_.loadHalfwordDRegDerefARegDisp16sx(targetRegElem.reg, WasmABI::REGS::linMem, inRangeCheck.safeInt());
          } else {
            as_.INSTR(LDHU_Da_deref_Ab_off16sx).setDa(targetRegElem.reg).setAb(WasmABI::REGS::linMem).setOff16sx(inRangeCheck.safeInt())();
          }
        } else {
          // Calculate the actual pointer
          as_.INSTR(ADDA_Aa_Ab).setAa(WasmABI::REGS::memLdStReg).setAb(WasmABI::REGS::linMem)();
          if (signExtend) {
            as_.loadHalfwordDRegDerefARegDisp16sx(targetRegElem.reg, WasmABI::REGS::memLdStReg, SafeInt<16U>::fromConst<-2>());
          } else {
            as_.INSTR(LDHU_Da_deref_Ab_off16sx).setDa(targetRegElem.reg).setAb(WasmABI::REGS::memLdStReg).setOff16sx(SafeInt<16U>::fromConst<-2>())();
          }
        }
      } else if (memObjSize == 4U) {
        SignedInRangeCheck<16U> const inRangeCheck{SignedInRangeCheck<16U>::check(compileTimeAddrOffset)};
        if (inRangeCheck.inRange()) {
          as_.loadWordDRegDerefARegDisp16sx(targetRegElem.reg, WasmABI::REGS::linMem, inRangeCheck.safeInt());
        } else {
          // Calculate the actual pointer
          as_.INSTR(ADDA_Aa_Ab).setAa(WasmABI::REGS::memLdStReg).setAb(WasmABI::REGS::linMem)();
          as_.loadWordDRegDerefARegDisp16sx(targetRegElem.reg, WasmABI::REGS::memLdStReg, SafeInt<16U>::fromConst<-4>());
        }
      } else { // memObjSize == 8U
        SignedInRangeCheck<10U> const inRangeCheck{SignedInRangeCheck<10U>::check(compileTimeAddrOffset)};
        if (inRangeCheck.inRange()) {
          as_.INSTR(LDD_Ea_deref_Ab_off10sx).setDa(targetRegElem.reg).setAb(WasmABI::REGS::linMem).setOff10sx(inRangeCheck.safeInt())();
        } else {
          // Calculate the actual pointer
          as_.INSTR(ADDA_Aa_Ab).setAa(WasmABI::REGS::memLdStReg).setAb(WasmABI::REGS::linMem)();
          as_.INSTR(LDD_Ea_deref_Ab_off10sx).setDa(targetRegElem.reg).setAb(WasmABI::REGS::memLdStReg).setOff10sx(SafeInt<10>::fromConst<-8>())();
        }
      }

      // No alignment check needed, we are done

      if ((resultType == MachineType::I64) && (memObjSize <= 4U)) {
        if (signExtend) { // Sign extend 32B to 64B
          as_.INSTR(MUL_Ec_Da_const9sx).setEc(targetRegElem.reg).setDa(targetRegElem.reg).setConst9sx(SafeInt<9U>::fromConst<1>())();
        } else { // Zero extend 32B to 64B
          as_.INSTR(MOV_Da_const4sx).setDa(RegUtil::getOtherExtReg(targetRegElem.reg)).setConst4sx(SafeInt<4U>::fromConst<0>())();
        }
      }

      return targetRegElem.elem;
    }
  }

  // Emit alignment check relative patch at runtime
  // coverity[autosar_cpp14_a8_5_2_violation]
  auto const emitAlignmentCheck = [this, targetReg = checkHelperReg, addressDReg, offset]() -> RelPatchObj {
    if (addressDReg == REG::NONE) {
      as_.INSTR(MOVD_Da_Ab).setDa(targetReg).setAb(WasmABI::REGS::memLdStReg)();
      // if (targetReg[0] != 0) go to unaligned load
      return as_.INSTR(JNZT_Da_n_disp15sx2).setDa(targetReg).setN(SafeUInt<5U>::fromConst<0>()).prepJmp();
    } else {
      // if ((addressDReg + offset)[0] != 0) go to unaligned load
      bool const needFlipAlignmentCheck{(offset % 2U) == 1U};
      return as_.INSTR(needFlipAlignmentCheck ? JZT_Da_n_disp15sx2 : JNZT_Da_n_disp15sx2)
          .setDa(addressDReg)
          .setN(SafeUInt<5U>::fromConst<0>())
          .prepJmp();
    }
  };

  // WasmABI::REGS::memLdStReg now points to end of data that should be accessed (as pointer, NOT offset, after last byte)
  as_.INSTR(ADDA_Aa_Ab).setAa(WasmABI::REGS::memLdStReg).setAb(WasmABI::REGS::linMem)();
  if (memObjSize == 1U) {
    if (signExtend) {
      as_.INSTR(LDB_Da_deref_Ab_off16sx).setDa(targetRegElem.reg).setAb(WasmABI::REGS::memLdStReg).setOff16sx(SafeInt<16U>::fromConst<-1>())();
    } else {
      as_.loadByteUnsignedDRegDerefARegDisp16sx(targetRegElem.reg, WasmABI::REGS::memLdStReg, SafeInt<16U>::fromConst<-1>());
    }
  } else if (memObjSize == 2U) {
    RelPatchObj const unaligned{emitAlignmentCheck()};
    { // Aligned
      if (signExtend) {
        as_.loadHalfwordDRegDerefARegDisp16sx(targetRegElem.reg, WasmABI::REGS::memLdStReg, SafeInt<16U>::fromConst<-2>());
      } else {
        as_.INSTR(LDHU_Da_deref_Ab_off16sx).setDa(targetRegElem.reg).setAb(WasmABI::REGS::memLdStReg).setOff16sx(SafeInt<16U>::fromConst<-2>())();
      }
    }
    RelPatchObj const end{as_.INSTR(J_disp24sx2).prepJmp()};
    unaligned.linkToHere();
    { // Unaligned
      if (unalignedAccessCodePositions_.load2 == UINT32_MAX) {
        RelPatchObj const skip{as_.INSTR(J_disp24sx2).prepJmp()};
        unalignedAccessCodePositions_.load2 = output_.size();
        // Push register to the stack
        constexpr REG helperReg{REG::D15};
        as_.INSTR(STW_deref_Ab_off10sx_Da_preinc).setAb(REG::SP).setOff10sx(SafeInt<10>::fromConst<-4>()).setDa(helperReg)();
        // --- actual implementation
        as_.loadWordDRegDerefARegDisp16sx(helperReg, WasmABI::REGS::memLdStReg,
                                          SafeInt<16U>::fromConst<-3>()); // Overflow by 1
        as_.INSTR(signExtend ? EXTR_Dc_Da_pos_width : EXTRU_Dc_Da_pos_width)
            .setDc(helperReg)
            .setDa(helperReg)
            .setPos(SafeUInt<5>::fromConst<8U>())
            .setWidth(SafeUInt<5U>::fromConst<16U>())();
        // --- actual implementation
        // Pass result back in memLdStReg
        as_.INSTR(MOVA_Aa_Db).setAa(WasmABI::REGS::memLdStReg).setDb(helperReg)();
        // Pop register from the stack
        as_.INSTR(LDW_Dc_deref_Ab_postinc).setDc(helperReg).setAb(REG::SP)();
        as_.INSTR(FRET)();
        skip.linkToHere();
      }

      as_.INSTR(FCALL_disp24sx2).prepJmp().linkToBinaryPos(unalignedAccessCodePositions_.load2);
      as_.INSTR(MOVD_Da_Ab).setDa(targetRegElem.reg).setAb(WasmABI::REGS::memLdStReg)();
    }
    end.linkToHere();

  } else if (memObjSize == 4U) {
    RelPatchObj const unaligned{emitAlignmentCheck()};
    { // Aligned
      as_.loadWordDRegDerefARegDisp16sx(targetRegElem.reg, WasmABI::REGS::memLdStReg, SafeInt<16U>::fromConst<-4>());
    }
    RelPatchObj const end{as_.INSTR(J_disp24sx2).prepJmp()};
    unaligned.linkToHere();
    { // Unaligned
      if (unalignedAccessCodePositions_.load4 == UINT32_MAX) {
        RelPatchObj const skip{as_.INSTR(J_disp24sx2).prepJmp()};
        unalignedAccessCodePositions_.load4 = output_.size();
        // Push registers to the stack
        as_.INSTR(STD_deref_Ab_off10sx_Ea_preinc).setAb(REG::SP).setOff10sx(SafeInt<10>::fromConst<-8>()).setEa(REG::D0)();
        // --- actual implementation
        as_.INSTR(LDW_Da_deref_Ab_off10sx_preinc).setDa(REG::D0).setAb(WasmABI::REGS::memLdStReg).setOff10sx(SafeInt<10>::fromConst<-5>())();
        as_.INSTR(LDBU_Dc_deref_A15_off4zx).setDc(REG::D1).setOff4zx(SafeUInt<4U>::fromConst<4>())(); // No overflow
        as_.INSTR(DEXTR_Dc_Da_Db_pos).setDc(REG::D0).setDa(REG::D1).setDb(REG::D0).setPos(SafeUInt<5>::fromConst<24U>())();
        // --- actual implementation
        // Pass result back in memLdStReg
        as_.INSTR(MOVA_Aa_Db).setAa(WasmABI::REGS::memLdStReg).setDb(REG::D0)();
        // Pop registers from the stack
        as_.INSTR(LDD_Ea_deref_Ab_off10sx_postinc).setEa(REG::D0).setAb(REG::SP).setOff10sx(SafeInt<10>::fromConst<8>())();
        as_.INSTR(FRET)();
        skip.linkToHere();
      }

      as_.INSTR(FCALL_disp24sx2).prepJmp().linkToBinaryPos(unalignedAccessCodePositions_.load4);
      as_.INSTR(MOVD_Da_Ab).setDa(targetRegElem.reg).setAb(WasmABI::REGS::memLdStReg)();
    }
    end.linkToHere();
  } else { // memObjSize == 8U
    RegAllocTracker targetRegAllocTracker{};
    targetRegAllocTracker.writeProtRegs = mask(targetRegElem.reg, true);
    REG const extraReg{common_.reqScratchRegProt(MachineType::I64, targetRegAllocTracker, false).reg};
    RelPatchObj const unaligned{emitAlignmentCheck()};
    { // Aligned
      as_.INSTR(LDD_Ea_deref_Ab_off10sx).setDa(targetRegElem.reg).setAb(WasmABI::REGS::memLdStReg).setOff10sx(SafeInt<10>::fromConst<-8>())();
    }
    RelPatchObj const end{as_.INSTR(J_disp24sx2).prepJmp()};
    unaligned.linkToHere();
    { // Unaligned
      as_.INSTR(LDD_Ea_deref_Ab_off10sx).setEa(extraReg).setAb(WasmABI::REGS::memLdStReg).setOff10sx(SafeInt<10>::fromConst<-9>())();
      as_.INSTR(DEXTR_Dc_Da_Db_pos)
          .setDc(targetRegElem.reg)
          .setDa(RegUtil::getOtherExtReg(extraReg))
          .setDb(extraReg)
          .setPos(SafeUInt<5>::fromConst<24U>())();
      // Overflow by 1
      as_.INSTR(LDD_Ea_deref_Ab_off10sx).setEa(extraReg).setAb(WasmABI::REGS::memLdStReg).setOff10sx(SafeInt<10>::fromConst<-7>())();
      as_.INSTR(DEXTR_Dc_Da_Db_pos)
          .setDc(RegUtil::getOtherExtReg(targetRegElem.reg))
          .setDa(RegUtil::getOtherExtReg(extraReg))
          .setDb(extraReg)
          .setPos(SafeUInt<5>::fromConst<8U>())();
    }
    end.linkToHere();
  }

  if ((resultType == MachineType::I64) && (memObjSize <= 4U)) {
    if (signExtend) { // Sign extend 32B to 64B
      as_.INSTR(MUL_Ec_Da_const9sx).setEc(targetRegElem.reg).setDa(targetRegElem.reg).setConst9sx(SafeInt<9U>::fromConst<1>())();
    } else { // Zero extend 32B to 64B
      as_.INSTR(MOV_Da_const4sx).setDa(RegUtil::getOtherExtReg(targetRegElem.reg)).setConst4sx(SafeInt<4U>::fromConst<0>())();
    }
  }

  return targetRegElem.elem;
}

void Backend::executeLinearMemoryStore(OPCode const opcode, uint32_t const offset) {
  assert(moduleInfo_.hasMemory && "Memory not defined");
  // coverity[autosar_cpp14_a8_5_2_violation]
  constexpr auto memObjSizes = make_array(4_U8, 8_U8, 4_U8, 8_U8, 1_U8, 2_U8, 1_U8, 2_U8, 4_U8);
  uint8_t const memObjSize{memObjSizes[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_STORE)]};

  Stack::iterator const valueIt{common_.condenseValentBlockBelow(stack_.end())};
  Stack::iterator const addrIt{common_.condenseValentBlockBelow(valueIt)};
  copyValueOfElemToAddrReg(WasmABI::REGS::memLdStReg, *addrIt);
  VariableStorage const addressStorage{moduleInfo_.getStorage(*addrIt)};
  REG const addressDReg{(addressStorage.type == StorageType::REGISTER) ? addressStorage.location.reg : REG::NONE};
  common_.removeReference(addrIt);
  static_cast<void>(stack_.erase(addrIt));

  RegAllocTracker regAllocTracker{};
  regAllocTracker.writeProtRegs = mask(addressDReg, false);
  REG const valueReg{common_.liftToRegInPlaceProt(*valueIt, false, regAllocTracker).reg};

  common_.removeReference(valueIt);
  static_cast<void>(stack_.erase(valueIt));

  REG const scrReg{common_.reqScratchRegProt(MachineType::I32, regAllocTracker, false).reg};

  RelPatchObj const directErr{prepareLinMemAddr(scrReg, addressDReg, offset, memObjSize)};
  // WasmABI::REGS::memLdStReg now points to end of data that should be accessed (as offset, after last byte)
  // WasmABI::REGS::memSize is actual full size of linear memory
  emitLinMemBoundsCheck(scrReg, &directErr);

  ///< Compile-time constant optimization for LdSt address:
  // if (address is compile-time constant) && (found it aligned)
  //    Don't emit alignment check relative patch at runtime
  //    if(addrOffset is in_range of store.offset)
  //      Emit the store directly with the compile-time address offset, which means `add memLdStReg, linMem` is unnecessary.
  //      Use compileTimeAddrOffset as total offset to linMem directly.
  //    else
  //      Use register to store the offset
  // else
  //     Not compile-time constant: Emit alignment check relative patch at runtime
  //     Constant && notAligned: Not handled separately with the situation that will not be emitted normally(same with not constant)
  //
  if (addressStorage.type == StorageType::CONSTANT) { // compile-time constant address, alignment checkable
    int64_t const compileTimeAddrOffset{static_cast<int64_t>(static_cast<uint32_t>(addrIt->data.constUnion.u32 + offset))};
    assert(compileTimeAddrOffset >= 0 && "always");
    if (memObjSize == 1U) {
      SignedInRangeCheck<16U> const inRangeCheck{SignedInRangeCheck<16U>::check(compileTimeAddrOffset)};
      if (inRangeCheck.inRange()) {
        as_.storeByteDerefARegDisp16sxDReg(valueReg, WasmABI::REGS::linMem, inRangeCheck.safeInt());
      } else {
        as_.INSTR(ADDA_Aa_Ab).setAa(WasmABI::REGS::memLdStReg).setAb(WasmABI::REGS::linMem)(); // Calculate the actual pointer
        as_.storeByteDerefARegDisp16sxDReg(valueReg, WasmABI::REGS::memLdStReg, SafeInt<16U>::fromConst<-1>());
      }
      return; // No alignment check needed, we are done
    }
    // write else branch directly to reduce depth of nest
    // Alignment of the base of linear memory is guaranteed
    if ((compileTimeAddrOffset % 2) == 0) { // compileTimeAligned
      if (memObjSize == 2U) {
        SignedInRangeCheck<16U> const inRangeCheck{SignedInRangeCheck<16U>::check(compileTimeAddrOffset)};
        if (inRangeCheck.inRange()) {
          as_.storeHalfwordDerefARegDisp16sxDReg(valueReg, WasmABI::REGS::linMem, inRangeCheck.safeInt());
        } else {
          as_.INSTR(ADDA_Aa_Ab).setAa(WasmABI::REGS::memLdStReg).setAb(WasmABI::REGS::linMem)(); // Calculate the actual pointer
          as_.storeHalfwordDerefARegDisp16sxDReg(valueReg, WasmABI::REGS::memLdStReg, SafeInt<16U>::fromConst<-2>());
        }
      } else if (memObjSize == 4U) {
        SignedInRangeCheck<16U> const inRangeCheck{SignedInRangeCheck<16U>::check(compileTimeAddrOffset)};
        if (inRangeCheck.inRange()) {
          as_.storeWordDerefARegDisp16sxDReg(valueReg, WasmABI::REGS::linMem, inRangeCheck.safeInt());
        } else {
          as_.INSTR(ADDA_Aa_Ab).setAa(WasmABI::REGS::memLdStReg).setAb(WasmABI::REGS::linMem)(); // Calculate the actual pointer
          as_.storeWordDerefARegDisp16sxDReg(valueReg, WasmABI::REGS::memLdStReg, SafeInt<16U>::fromConst<-4>());
        }
      } else { // memObjSize == 8U
        SignedInRangeCheck<10U> const inRangeCheck{SignedInRangeCheck<10U>::check(compileTimeAddrOffset)};
        if (inRangeCheck.inRange()) {
          as_.INSTR(STD_deref_Ab_off10sx_Ea).setAb(WasmABI::REGS::linMem).setOff10sx(inRangeCheck.safeInt()).setDa(valueReg)();
        } else {
          as_.INSTR(ADDA_Aa_Ab).setAa(WasmABI::REGS::memLdStReg).setAb(WasmABI::REGS::linMem)(); // Calculate the actual pointer
          as_.INSTR(STD_deref_Ab_off10sx_Ea).setAb(WasmABI::REGS::memLdStReg).setOff10sx(SafeInt<10>::fromConst<-8>()).setDa(valueReg)();
        }
      }
      return; // No alignment check needed, we are done
    }
  }

  // Emit alignment check relative patch at runtime
  // coverity[autosar_cpp14_a8_5_2_violation]
  auto const emitAlignmentCheck = [this, scrReg, addressDReg, offset]() -> RelPatchObj {
    if (addressDReg == REG::NONE) {
      as_.INSTR(MOVD_Da_Ab).setDa(scrReg).setAb(WasmABI::REGS::memLdStReg)();
      // if (targetReg[0] != 0) go to unaligned load
      return as_.INSTR(JNZT_Da_n_disp15sx2).setDa(scrReg).setN(SafeUInt<5U>::fromConst<0>()).prepJmp();
    } else {
      // if ((addressDReg + offset)[0] != 0) go to unaligned load
      bool const needFlipAlignmentCheck{(offset % 2U) == 1U};
      return as_.INSTR(needFlipAlignmentCheck ? JZT_Da_n_disp15sx2 : JNZT_Da_n_disp15sx2)
          .setDa(addressDReg)
          .setN(SafeUInt<5U>::fromConst<0>())
          .prepJmp();
    }
  };

  // WasmABI::REGS::memLdStReg now points to end of data that should be accessed (as pointer, NOT offset, after last byte)
  as_.INSTR(ADDA_Aa_Ab).setAa(WasmABI::REGS::memLdStReg).setAb(WasmABI::REGS::linMem)();
  static_assert(REG::A15 == WasmABI::REGS::memLdStReg, "");
  if (memObjSize == 1U) {
    as_.INSTR(STB_deref_Ab_off16sx_Da).setAb(WasmABI::REGS::memLdStReg).setOff16sx(SafeInt<16U>::fromConst<-1>()).setDa(valueReg)();
  } else if (memObjSize == 2U) {
    RelPatchObj const unaligned{emitAlignmentCheck()};
    { // Aligned branch at runtime
      as_.INSTR(STH_deref_Ab_off16sx_Da).setAb(WasmABI::REGS::memLdStReg).setOff16sx(SafeInt<16U>::fromConst<-2>()).setDa(valueReg)();
    }
    RelPatchObj const end{as_.INSTR(J_disp24sx2).prepJmp()};
    unaligned.linkToHere();
    { // Unaligned branch at runtime
      if (unalignedAccessCodePositions_.store2 == UINT32_MAX) {
        RelPatchObj const skip{as_.INSTR(J_disp24sx2).prepJmp()};
        unalignedAccessCodePositions_.store2 = output_.size();
        // Push registers to the stack
        REG constexpr helperReg{REG::D0};
        as_.INSTR(STW_deref_Ab_off10sx_Da_preinc).setAb(REG::SP).setOff10sx(SafeInt<10>::fromConst<-4>()).setDa(helperReg)();
        // Retrieve value to store from cmpRes
        as_.INSTR(MOVD_Da_Ab).setDa(helperReg).setAb(WasmABI::REGS::cmpRes)();
        // --- actual implementation
        as_.INSTR(STB_deref_Ab_off16sx_Da).setAb(WasmABI::REGS::memLdStReg).setOff16sx(SafeInt<16U>::fromConst<-2>()).setDa(helperReg)();
        as_.INSTR(SH_Da_const4sx).setDa(helperReg).setConst4sx(SafeInt<4U>::fromConst<-8>())();
        as_.INSTR(STB_deref_Ab_off16sx_Da).setAb(WasmABI::REGS::memLdStReg).setOff16sx(SafeInt<16U>::fromConst<-1>()).setDa(helperReg)();
        // --- actual implementation
        // Pop registers from the stack
        as_.INSTR(LDW_Dc_deref_Ab_postinc).setDc(helperReg).setAb(REG::SP)();
        as_.INSTR(FRET)();
        skip.linkToHere();
      }

      // Pass value to store in cmpRes
      as_.INSTR(MOVA_Aa_Db).setAa(WasmABI::REGS::cmpRes).setDb(valueReg)();
      as_.INSTR(FCALL_disp24sx2).prepJmp().linkToBinaryPos(unalignedAccessCodePositions_.store2);
    }
    end.linkToHere();
  } else if (memObjSize == 4U) {
    RelPatchObj const unaligned{emitAlignmentCheck()};
    { // Aligned branch at runtime
      as_.storeWordDerefARegDisp16sxDReg(valueReg, WasmABI::REGS::memLdStReg, SafeInt<16U>::fromConst<-4>());
    }
    RelPatchObj const end{as_.INSTR(J_disp24sx2).prepJmp()};
    unaligned.linkToHere();
    { // Unaligned branch at runtime
      if (unalignedAccessCodePositions_.store4 == UINT32_MAX) {
        RelPatchObj const skip{as_.INSTR(J_disp24sx2).prepJmp()};
        unalignedAccessCodePositions_.store4 = output_.size();
        REG constexpr helperReg{REG::D0};
        // Push registers to the stack
        as_.INSTR(STW_deref_Ab_off10sx_Da_preinc).setAb(REG::SP).setOff10sx(SafeInt<10>::fromConst<-4>()).setDa(helperReg)();
        // Retrieve value to store from cmpRes
        as_.INSTR(MOVD_Da_Ab).setDa(helperReg).setAb(WasmABI::REGS::cmpRes)();
        // --- actual implementation
        as_.INSTR(STB_deref_Ab_off10sx_Da_preinc).setAb(WasmABI::REGS::memLdStReg).setOff10sx(SafeInt<10>::fromConst<-4>()).setDa(helperReg)();
        as_.INSTR(SH_Da_const4sx).setDa(helperReg).setConst4sx(SafeInt<4U>::fromConst<-8>())();
        as_.INSTR(STH_deref_Ab_off16sx_Da).setAb(WasmABI::REGS::memLdStReg).setOff16sx(SafeInt<16U>::fromConst<1>()).setDa(helperReg)();
        as_.INSTR(SH_Dc_Da_const9sx).setDc(helperReg).setDa(helperReg).setConst9sx(SafeInt<9U>::fromConst<-16>())();
        as_.INSTR(STB_deref_A15_off4zx_Da).setOff4zx(SafeUInt<4U>::fromConst<3U>()).setDa(helperReg)();
        // --- actual implementation
        // Pop registers from the stack
        as_.INSTR(LDW_Dc_deref_Ab_postinc).setDc(helperReg).setAb(REG::SP)();
        as_.INSTR(FRET)();
        skip.linkToHere();
      }

      // Pass value to store in cmpRes
      as_.INSTR(MOVA_Aa_Db).setAa(WasmABI::REGS::cmpRes).setDb(valueReg)();
      as_.INSTR(FCALL_disp24sx2).prepJmp().linkToBinaryPos(unalignedAccessCodePositions_.store4);
    }
    end.linkToHere();
  } else { // memObjSize == 8U
    RelPatchObj const unaligned{emitAlignmentCheck()};
    { // Aligned branch at runtime
      as_.INSTR(STD_deref_Ab_off10sx_Ea).setAb(WasmABI::REGS::memLdStReg).setOff10sx(SafeInt<10>::fromConst<-8>()).setDa(valueReg)();
    }
    RelPatchObj const end{as_.INSTR(J_disp24sx2).prepJmp()};
    unaligned.linkToHere();
    { // Unaligned branch at runtime
      if (unalignedAccessCodePositions_.store8 == UINT32_MAX) {
        RelPatchObj const skip{as_.INSTR(J_disp24sx2).prepJmp()};
        unalignedAccessCodePositions_.store8 = output_.size();
        // Push registers to the stack
        as_.INSTR(STD_deref_Ab_off10sx_Ea_preinc).setAb(REG::SP).setOff10sx(SafeInt<10>::fromConst<-8>()).setEa(REG::D0)();
        as_.INSTR(STD_deref_Ab_off10sx_Ea_preinc).setAb(REG::SP).setOff10sx(SafeInt<10>::fromConst<-8>()).setEa(REG::D2)();
        // Retrieve value to store from stack (we pushed 16 bytes and 4 bytes were pushed by FCALL)
        as_.INSTR(LDD_Ea_deref_Ab_off10sx).setEa(REG::D0).setAb(REG::SP).setOff16sx(SafeInt<16U>::fromConst<16 + 4>())();
        // --- actual implementation
        as_.INSTR(STB_deref_Ab_off10sx_Da_preinc).setAb(WasmABI::REGS::memLdStReg).setOff10sx(SafeInt<10>::fromConst<-8>()).setDa(REG::D0)();
        as_.INSTR(DEXTR_Dc_Da_Db_pos).setDc(REG::D2).setDa(RegUtil::getOtherExtReg(REG::D0)).setDb(REG::D0).setPos(SafeUInt<5>::fromConst<24U>())();
        as_.storeWordDerefARegDisp16sxDReg(REG::D2, WasmABI::REGS::memLdStReg, SafeInt<16U>::fromConst<1>());
        as_.INSTR(SH_Dc_Da_const9sx).setDc(REG::D2).setDa(RegUtil::getOtherExtReg(REG::D0)).setConst9sx(SafeInt<9U>::fromConst<-8>())();
        as_.INSTR(STH_deref_Ab_off16sx_Da).setAb(WasmABI::REGS::memLdStReg).setOff16sx(SafeInt<16U>::fromConst<5>()).setDa(REG::D2)();
        as_.INSTR(SH_Dc_Da_const9sx).setDc(REG::D2).setDa(REG::D2).setConst9sx(SafeInt<9U>::fromConst<-16>())();
        as_.INSTR(STB_deref_A15_off4zx_Da).setOff4zx(SafeUInt<4U>::fromConst<7U>()).setDa(REG::D2)();
        // --- actual implementation
        // Pop registers from the stack
        as_.INSTR(LDD_Ea_deref_Ab_off10sx_postinc).setEa(REG::D2).setAb(REG::SP).setOff10sx(SafeInt<10>::fromConst<8>())();
        as_.INSTR(LDD_Ea_deref_Ab_off10sx_postinc).setEa(REG::D0).setAb(REG::SP).setOff10sx(SafeInt<10>::fromConst<8>())();

        // Return address (4B) and input value (8B) are still on the stack, A11 contains the return address
        as_.INSTR(FRET)();
        skip.linkToHere();
      }

      // Pass value to store on stack
      as_.INSTR(STD_deref_Ab_off10sx_Ea_preinc).setAb(REG::SP).setOff10sx(SafeInt<10>::fromConst<-8>()).setEa(valueReg)();
      as_.INSTR(FCALL_disp24sx2).prepJmp().linkToBinaryPos(unalignedAccessCodePositions_.store8);
      as_.INSTR(LDD_Ea_deref_Ab_off10sx_postinc).setAb(REG::SP).setOff10sx(SafeInt<10>::fromConst<8>()).setEa(valueReg)();
    }
    end.linkToHere();
  }
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

  constexpr REG dstReg{WasmABI::REGS::addrScrReg[0]};
  copyValueOfElemToAddrReg(dstReg, *dst);
  constexpr REG srcReg{WasmABI::REGS::addrScrReg[1]};
  copyValueOfElemToAddrReg(srcReg, *src);

  common_.removeReference(size);
  common_.removeReference(src);
  common_.removeReference(dst);
  static_cast<void>(stack_.erase(size));
  static_cast<void>(stack_.erase(src));
  static_cast<void>(stack_.erase(dst));

  regAllocTracker = RegAllocTracker();
  regAllocTracker.writeProtRegs = mask(sizeReg, false);
  REG const scratchReg{common_.reqScratchRegProt(MachineType::I64, regAllocTracker, false).reg};
  REG const extendScratchReg{RegUtil::getOtherExtReg(scratchReg)};
  // if src + size is larger then the length of mem.data then trap
  // if dst + size is larger then the length of mem.data then trap
  // can be combined
  // max(src, dst) + size is larger then the length of mem.data then trap
  as_.INSTR(MOVD_Da_Ab).setDa(scratchReg).setAb(srcReg)();
  as_.INSTR(MOVD_Da_Ab).setDa(extendScratchReg).setAb(dstReg)();
  as_.INSTR(MAXU_Dc_Da_Db).setDc(scratchReg).setDa(scratchReg).setDb(extendScratchReg)();
  as_.INSTR(ADD_Da_Db).setDa(scratchReg).setDb(sizeReg)();
  // check overflow: if ((max(src,dst) + size) < size) trap;
  as_.cTRAP(TrapCode::LINMEM_OUTOFBOUNDSACCESS, JumpCondition::u32LtReg(scratchReg, sizeReg));
  as_.INSTR(MOVA_Aa_Db).setAa(WasmABI::REGS::memLdStReg).setDb(scratchReg)(); // scratchReg and memLdStReg <- max(src, dst) + size
  RelPatchObj const directErr{prepareLinMemAddr(extendScratchReg, scratchReg, 0U, 0U)};
  emitLinMemBoundsCheck(scratchReg, &directErr);

  as_.INSTR(ADDA_Aa_Ab).setAa(srcReg).setAb(WasmABI::REGS::linMem)();
  as_.INSTR(ADDA_Aa_Ab).setAa(dstReg).setAb(WasmABI::REGS::linMem)();
  constexpr bool canOverlap{true};
  if (sizeIsConstant) {
    emitMemcpyWithConstSizeNoBoundsCheck(dstReg, srcReg, sizeReg, sizeValue, scratchReg, canOverlap);
  } else {
    emitMemcpyNoBoundsCheck(dstReg, srcReg, sizeReg, scratchReg, canOverlap);
  }
}

void Backend::executeLinearMemoryFill(Stack::iterator const dst, Stack::iterator const value, Stack::iterator const size) {
  constexpr REG dstAReg{WasmABI::REGS::addrScrReg[2]};

  RegAllocTracker regAllocTracker{};
  regAllocTracker.futureLifts = mask(value.unwrap());
  copyValueOfElemToAddrReg(dstAReg, *dst);
  REG const sizeReg{common_.liftToRegInPlaceProt(*size, true, regAllocTracker).reg};
  REG const valueReg{common_.liftToRegInPlaceProt(*value, true, regAllocTracker).reg};

  common_.removeReference(size);
  common_.removeReference(value);
  common_.removeReference(dst);
  static_cast<void>(stack_.erase(size));
  static_cast<void>(stack_.erase(value));
  static_cast<void>(stack_.erase(dst));

  // TriCore use address reg to store dst. So we can remove reference before reqScratchRegProt to free one slot.
  REG const scratchDReg{common_.reqScratchRegProt(MachineType::I32, regAllocTracker, false).reg};

  { // bound check
    // if dst + size is larger then the length of mem.data then trap
    as_.INSTR(MOVD_Da_Ab).setDa(scratchDReg).setAb(dstAReg)();
    as_.INSTR(ADD_Da_Db).setDa(scratchDReg).setDb(sizeReg)();
    // check overflow: if ((max(src,dst) + size) < size) trap;
    as_.cTRAP(TrapCode::LINMEM_OUTOFBOUNDSACCESS, JumpCondition::u32LtReg(scratchDReg, sizeReg));
    as_.INSTR(MOVA_Aa_Db).setAa(WasmABI::REGS::memLdStReg).setDb(scratchDReg)(); // memLdStReg <- dst + size
    RelPatchObj const directErr{prepareLinMemAddr(scratchDReg, scratchDReg, 0U, 0U)};
    emitLinMemBoundsCheck(scratchDReg, &directErr);
  }
  as_.INSTR(ADDA_Aa_Ab).setAa(dstAReg).setAb(WasmABI::REGS::linMem)();
  { // align dst
    as_.INSTR(MOVD_Da_Ab).setDa(scratchDReg).setAb(dstAReg)();
    RelPatchObj const is2ByteAligned{
        as_.prepareJump(JumpCondition::bitFalse(scratchDReg, SafeInt<4U>::fromConst<0>()))}; // last bit is zero means it is aligned.
    as_.INSTR(ADDI_Dc_Da_const16sx).setDc(sizeReg).setDa(sizeReg).setConst16sx(SafeInt<16U>::fromConst<-1>())();
    as_.INSTR(STB_deref_Ab_Da_postinc).setAb(dstAReg).setDa(valueReg)();
    is2ByteAligned.linkToHere();
  }
  {
    // multiple bytes set
    constexpr uint32_t step{4U};
    constexpr uint32_t log2step{log2Constexpr(step)};

    // prepare loop
    // coverity[autosar_cpp14_m5_19_1_violation]
    as_.INSTR(SH_Dc_Da_const9sx).setDc(scratchDReg).setDa(sizeReg).setConst9sx(SafeInt<9U>::fromConst<static_cast<int32_t>(-log2step)>())();
    RelPatchObj const isSizeLessThanStep{as_.prepareJump(JumpCondition::i32EqConst4sx(scratchDReg, SafeInt<4U>::fromConst<0>()))};
    // prepare data
    as_.INSTR(COPY_BYTE_TO_ALL_Dc_Da).setDc(valueReg).setDa(valueReg)();
    uint32_t const multipleByteSetLoopStart{output_.size()};
    as_.INSTR(STW_deref_Ab_Da_postinc).setAb(dstAReg).setDa(valueReg)();
    as_.INSTR(JNED_Da_const4sx_disp15sx2)
        .setDa(scratchDReg)
        .setConst4sx(SafeInt<4U>::fromConst<1U>())
        .prepJmp()
        .linkToBinaryPos(multipleByteSetLoopStart);

    as_.andWordDcDaConst9zx(sizeReg, sizeReg, SafeUInt<9U>::fromConst<step - 1U>());
    isSizeLessThanStep.linkToHere();
  }
  { // byte set
    RelPatchObj const isSizeZero{as_.prepareJump(JumpCondition::i32EqConst4sx(sizeReg, SafeInt<4U>::fromConst<0>()))};
    uint32_t const fill1{output_.size()};
    as_.INSTR(STB_deref_Ab_Da_postinc).setAb(dstAReg).setDa(valueReg)();
    as_.INSTR(JNED_Da_const4sx_disp15sx2).setDa(sizeReg).setConst4sx(SafeInt<4U>::fromConst<1U>()).prepJmp().linkToBinaryPos(fill1);
    isSizeZero.linkToHere();
  }
}

void Backend::executeGetMemSize() const {
  assert(moduleInfo_.hasMemory && "No memory defined");

  RegAllocTracker regAllocTracker{};
  RegElement const targetRegElem{common_.reqScratchRegProt(MachineType::I32, regAllocTracker, false)};
  as_.loadWordDRegDerefARegDisp16sx(targetRegElem.reg, WasmABI::REGS::linMem, SafeInt<16U>::fromConst<-BD::FromEnd::linMemWasmSize>());
  common_.pushAndUpdateReference(targetRegElem.elem);
}

void Backend::executeMemGrow() {
  assert(moduleInfo_.hasMemory && "No memory defined");

  Stack::iterator const deltaElement{common_.condenseValentBlockBelow(stack_.end())};

  RegAllocTracker regAllocTracker{};
  RegElement const gpOutputRegElem{common_.reqScratchRegProt(MachineType::I32, regAllocTracker, false)};
  as_.loadWordDRegDerefARegDisp16sx(gpOutputRegElem.reg, WasmABI::REGS::linMem, SafeInt<16U>::fromConst<-BD::FromEnd::linMemWasmSize>());

  RegElement const intermRegElem{common_.reqScratchRegProt(MachineType::I32, regAllocTracker, false)};
  SignedInRangeCheck<16U> const rangeCheck{SignedInRangeCheck<16U>::check(bit_cast<int32_t>(deltaElement->data.constUnion.u32))};
  if ((deltaElement->type == StackType::CONSTANT_I32) && rangeCheck.inRange()) {
    as_.INSTR(ADDI_Dc_Da_const16sx).setDc(gpOutputRegElem.reg).setDa(gpOutputRegElem.reg).setConst16sx(rangeCheck.safeInt())();
  } else {
    Assembler::PreparedArgs const prep{
        as_.loadArgsToRegsAndPrepDest(MachineType::INVALID, deltaElement.unwrap(), nullptr, &intermRegElem.elem, mask(gpOutputRegElem.reg, false))};
    as_.INSTR(ADD_Da_Db).setDa(gpOutputRegElem.reg).setDb(prep.arg0.reg)();
  }

  // Retrieve the PSW register from the core registers, overflow flag is signed but will also be usable for us since max
  // is 1 << 16 anyway
  constexpr uint16_t pswCROffset{0xFE04U};
  as_.INSTR(MFCR_Dc_const16).setDc(intermRegElem.reg).setConst16(SafeUInt<16U>::fromConst<static_cast<uint32_t>(pswCROffset)>())();
  // Overflow bit (V) is bit 30 in the PSW register
  RelPatchObj const error{as_.INSTR(JNZT_Da_n_disp15sx2)
                              .setDa(intermRegElem.reg)
                              .setN(SafeUInt<5U>::fromConst<30U>())
                              .setDisp15sx2(SafeInt<16U>::fromConst<2>())
                              .prepJmp()};

  //
  uint32_t const maxMemorySize{moduleInfo_.memoryHasSizeLimit ? moduleInfo_.memoryMaximumSize : (1_U32 << 16_U32)};
  as_.MOVimm(intermRegElem.reg, maxMemorySize);

  RelPatchObj const noError{as_.INSTR(JGEU_Da_Db_disp15sx2).setDa(intermRegElem.reg).setDb(gpOutputRegElem.reg).prepJmp()};
  //

  error.linkToHere();
  as_.MOVimm(gpOutputRegElem.reg, 0xFF'FF'FF'FFU);
  RelPatchObj const toEnd{as_.INSTR(J_disp24sx2).prepJmp()};

  noError.linkToHere();

  as_.loadWordDRegDerefARegDisp16sx(intermRegElem.reg, WasmABI::REGS::linMem, SafeInt<16U>::fromConst<-BD::FromEnd::linMemWasmSize>());
  as_.storeWordDerefARegDisp16sxDReg(gpOutputRegElem.reg, WasmABI::REGS::linMem, SafeInt<16U>::fromConst<-BD::FromEnd::linMemWasmSize>());
  as_.INSTR(MOV_Da_Db).setDa(gpOutputRegElem.reg).setDb(intermRegElem.reg)();

  toEnd.linkToHere();
  common_.replaceAndUpdateReference(deltaElement, gpOutputRegElem.elem);
}

void Backend::executeTrap(TrapCode const code) const {
  as_.TRAP(code);
}

void Backend::emitMoveImpl(VariableStorage const &dstStorage, VariableStorage const &srcStorage, bool const unconditional,
                           bool const presFlags) const {
  static_cast<void>(presFlags);
  assert(dstStorage.type != StorageType::CONSTANT && dstStorage.type != StorageType::INVALID && srcStorage.type != StorageType::INVALID &&
         "Invalid source or destination for emitMove");
  assert(dstStorage.machineType == srcStorage.machineType && "Source and destination must have the same width");

  if ((!unconditional) && dstStorage.equals(srcStorage)) {
    return;
  }
  MachineType const machineType{dstStorage.machineType};
  bool const is64{MachineTypeUtil::is64(machineType)};

  if (dstStorage.type == StorageType::REGISTER) { // X -> REGISTER
    REG const dstReg{dstStorage.location.reg};
    if (srcStorage.type == StorageType::CONSTANT) { // CONSTANT -> REGISTER
      if (is64) {
        uint64_t const constant{(machineType == MachineType::F64) ? srcStorage.location.constUnion.rawF64() : srcStorage.location.constUnion.u64};
        as_.MOVimm(dstReg, static_cast<uint32_t>(constant));
        as_.MOVimm(RegUtil::getOtherExtReg(dstReg), static_cast<uint32_t>(constant >> 32LLU));
      } else {
        uint32_t const constant{(machineType == MachineType::F32) ? srcStorage.location.constUnion.rawF32() : srcStorage.location.constUnion.u32};
        as_.MOVimm(dstReg, constant);
      }
    } else if (srcStorage.type == StorageType::REGISTER) { // REGISTER -> REGISTER
      REG const srcReg{srcStorage.location.reg};
      as_.INSTR(MOV_Da_Db).setDa(dstReg).setDb(srcReg)();
      if (is64) {
        as_.INSTR(MOV_Da_Db).setDa(RegUtil::getOtherExtReg(dstReg)).setDb(RegUtil::getOtherExtReg(srcReg))();
      }
    } else { // MEMORY -> REGISTER
      if (is64) {
        RegDisp<10U> const srcRegDisp{getMemRegDisp<10U>(srcStorage, WasmABI::REGS::addrScrReg[2])};
        as_.INSTR(LDD_Ea_deref_Ab_off10sx).setEa(dstReg).setAb(srcRegDisp.reg).setOff10sx(srcRegDisp.disp)();
      } else {
        RegDisp<16U> const srcRegDisp{getMemRegDisp<16U>(srcStorage, WasmABI::REGS::addrScrReg[2])};
        UnsignedInRangeCheck<10> const rangeCheck10{UnsignedInRangeCheck<10>::check(static_cast<uint32_t>(srcRegDisp.disp.value()))};
        UnsignedInRangeCheck<6> const rangeCheck6{UnsignedInRangeCheck<6>::check(static_cast<uint32_t>(srcRegDisp.disp.value()))};
        if (((dstReg == REG::D15) && (srcRegDisp.reg == REG::SP)) &&
            (((srcRegDisp.disp.value() >= 0) && rangeCheck10.inRange()) && ((srcRegDisp.disp.value() % 4) == 0))) {
          as_.INSTR(LDW_D15_deref_A10_const8zxls2).setConst8zxls2(rangeCheck10.safeInt())();
        } else if ((dstReg == REG::D15) && (((srcRegDisp.disp.value() >= 0) && rangeCheck6.inRange()) && ((srcRegDisp.disp.value() % 4) == 0))) {
          as_.INSTR(LDW_D15_deref_Ab_off4srozxls2).setAb(srcRegDisp.reg).setOff4srozxls2(rangeCheck6.safeInt())();
        } else {
          as_.loadWordDRegDerefARegDisp16sx(dstReg, srcRegDisp.reg, srcRegDisp.disp);
        }
      }
    }
  } else { // X -> MEMORY

    if (srcStorage.type == StorageType::CONSTANT) { // CONSTANT -> MEMORY
      if (is64) {
        RegDisp<10U> const dstRegDisp{getMemRegDisp<10U>(dstStorage, WasmABI::REGS::addrScrReg[2])};
        uint64_t const constant{(machineType == MachineType::F64) ? srcStorage.location.constUnion.rawF64() : srcStorage.location.constUnion.u64};
        as_.MOVimm(WasmABI::REGS::addrScrReg[0], static_cast<uint32_t>(constant));
        as_.MOVimm(WasmABI::REGS::addrScrReg[1], static_cast<uint32_t>(constant >> 32U));
        as_.INSTR(STDA_deref_Ab_off10sx_Pa).setAb(dstRegDisp.reg).setOff10sx(dstRegDisp.disp).setPa(WasmABI::REGS::addrScrReg[0])();
      } else {
        RegDisp<16U> const dstRegDisp{getMemRegDisp<16U>(dstStorage, WasmABI::REGS::addrScrReg[2])};
        uint32_t const constant{(machineType == MachineType::F32) ? srcStorage.location.constUnion.rawF32() : srcStorage.location.constUnion.u32};
        as_.MOVimm(WasmABI::REGS::addrScrReg[0], static_cast<uint32_t>(constant));
        as_.emitStoreDerefOff16sx(dstRegDisp.reg, WasmABI::REGS::addrScrReg[0], dstRegDisp.disp);
      }
    } else if (srcStorage.type == StorageType::REGISTER) { // REGISTER -> MEMORY
      REG const srcReg{srcStorage.location.reg};
      if (is64) {
        RegDisp<10U> const dstRegDisp{getMemRegDisp<10U>(dstStorage, WasmABI::REGS::addrScrReg[2])};
        as_.INSTR(STD_deref_Ab_off10sx_Ea).setAb(dstRegDisp.reg).setOff10sx(dstRegDisp.disp).setEa(srcReg)();
      } else {
        RegDisp<16U> const dstRegDisp{getMemRegDisp<16U>(dstStorage, WasmABI::REGS::addrScrReg[2])};
        as_.storeWordDerefARegDisp16sxDReg(srcReg, dstRegDisp.reg, dstRegDisp.disp);
      }
    } else { // MEMORY -> MEMORY

      if (is64) {
        RegDisp<10U> const dstRegDisp{getMemRegDisp<10U>(dstStorage, WasmABI::REGS::addrScrReg[2])};
        RegDisp<10U> const srcRegDisp{getMemRegDisp<10U>(srcStorage, WasmABI::REGS::addrScrReg[2])};
        as_.INSTR(LDDA_Pa_deref_Ab_off10sx).setPa(WasmABI::REGS::addrScrReg[0]).setAb(srcRegDisp.reg).setOff10sx(srcRegDisp.disp)();
        as_.INSTR(STDA_deref_Ab_off10sx_Pa).setAb(dstRegDisp.reg).setOff10sx(dstRegDisp.disp).setPa(WasmABI::REGS::addrScrReg[0])();
      } else {
        RegDisp<16U> const dstRegDisp{getMemRegDisp<16U>(dstStorage, WasmABI::REGS::addrScrReg[2])};
        RegDisp<16U> const srcRegDisp{getMemRegDisp<16U>(srcStorage, WasmABI::REGS::addrScrReg[2])};
        as_.emitLoadDerefOff16sx(WasmABI::REGS::addrScrReg[0], srcRegDisp.reg, srcRegDisp.disp);
        as_.emitStoreDerefOff16sx(dstRegDisp.reg, WasmABI::REGS::addrScrReg[0], dstRegDisp.disp);
      }
    }
  }
}

StackElement Backend::emitSelectImm(OPCodeTemplate const opCode, bool const is64, StackElement &regElement, StackElement const &immElement,
                                    REG const condReg, StackElement const *const targetHint, RegAllocTracker &regAllocTracker) {
  if (!is64) {
    int32_t const immValue{bit_cast<int32_t>(immElement.data.constUnion.u32)};
    SignedInRangeCheck<9U> const rangeCheck{SignedInRangeCheck<9U>::check(immValue)};
    if (rangeCheck.inRange()) {
      REG const targetReg{common_.liftToRegInPlaceProt(regElement, true, targetHint, regAllocTracker).reg};
      as_.INSTR(opCode).setDc(targetReg).setDa(targetReg).setConst9sx(rangeCheck.safeInt()).setDd(condReg)();
      return regElement;
    }
  } else {
    int32_t const immValueLow{bit_cast<int32_t>(static_cast<uint32_t>(immElement.data.constUnion.u64))};
    int32_t const immValueHigh{bit_cast<int32_t>(static_cast<uint32_t>(immElement.data.constUnion.u64 >> 32LLU))};

    SignedInRangeCheck<9U> const rangeCheckLow{SignedInRangeCheck<9U>::check(immValueLow)};
    SignedInRangeCheck<9U> const rangeCheckHigh{SignedInRangeCheck<9U>::check(immValueHigh)};
    if (rangeCheckLow.inRange() && rangeCheckHigh.inRange()) {
      REG const targetReg{common_.liftToRegInPlaceProt(regElement, true, targetHint, regAllocTracker).reg};
      as_.INSTR(opCode).setDc(targetReg).setDa(targetReg).setConst9sx(rangeCheckLow.safeInt()).setDd(condReg)();
      as_.INSTR(opCode)
          .setDc(RegUtil::getOtherExtReg(targetReg))
          .setDa(RegUtil::getOtherExtReg(targetReg))
          .setConst9sx(rangeCheckHigh.safeInt())
          .setDd(condReg)();
      return regElement;
    }
  }
  return StackElement::invalid();
}

StackElement Backend::emitSelect(StackElement &truthyResult, StackElement &falsyResult, StackElement &condElem,
                                 StackElement const *const targetHint) {
  MachineType const resultType{moduleInfo_.getMachineType(&truthyResult)};
  bool const is64{MachineTypeUtil::is64(resultType)};

  RegAllocTracker regAllocTracker{};
  regAllocTracker.futureLifts = mask(&truthyResult) | mask(&falsyResult);
  Common::LiftedReg const condReg{common_.liftToRegInPlaceProt(condElem, false, targetHint, regAllocTracker)};

  StackElement targetElement{StackElement::invalid()};

  if ((falsyResult.type == StackType::CONSTANT_I32) || (falsyResult.type == StackType::CONSTANT_I64)) {
    targetElement = emitSelectImm(SEL_Dc_Da_Dd_const9sx, is64, truthyResult, falsyResult, condReg.reg, targetHint, regAllocTracker);
  } else if ((truthyResult.type == StackType::CONSTANT_I32) || (truthyResult.type == StackType::CONSTANT_I64)) {
    targetElement = emitSelectImm(SELN_Dc_Da_Dd_const9sx, is64, falsyResult, truthyResult, condReg.reg, targetHint, regAllocTracker);
  } else {
    // pass
  }
  if (targetElement.type != StackType::INVALID) {
    /// imm value can be encoded in instruction
    return targetElement;
  }

  Common::LiftedReg const truthyReg{common_.liftToRegInPlaceProt(truthyResult, false, targetHint, regAllocTracker)};
  Common::LiftedReg const falsyReg{common_.liftToRegInPlaceProt(falsyResult, false, targetHint, regAllocTracker)};

  REG const targetHintReg{getUnderlyingRegIfSuitable(targetHint, resultType, RegMask{})};

  REG targetReg{REG::NONE};

  // prefer to use target hint if possible.
  // otherwise try to reuse input regs if writable.
  // the worst case is req scratch reg.
  if (is64) {
    // coverity[autosar_cpp14_a8_5_2_violation]
    auto const overlapWithCondReg = [&condReg](REG const reg) VB_NOEXCEPT -> bool {
      // in 64 bits select, the instruction sequences is:
      // read ExtReg<truthyReg>, ExtReg<falsyReg>, condReg
      // write ExtReg<targetReg>
      // read truthyReg, falsyReg, condReg
      // write targetReg
      // so we should make sure:
      //  - `ExtReg<targetReg> != truthyReg`, truthyReg is 64bit data, must not be ExtReg
      //  - `ExtReg<targetReg> != falsyReg`, falsyReg is 64bit data, must not be ExtReg
      //  - `ExtReg<targetReg> != condReg`
      return condReg.reg == RegUtil::getOtherExtReg(reg);
    };
    // coverity[autosar_cpp14_a4_5_1_violation]
    if ((targetHintReg != REG::NONE) && !overlapWithCondReg(targetHintReg)) {
      targetReg = targetHintReg;
      // coverity[autosar_cpp14_a4_5_1_violation]
    } else if (truthyReg.writable && !overlapWithCondReg(truthyReg.reg)) {
      targetReg = truthyReg.reg;
      // coverity[autosar_cpp14_a4_5_1_violation]
    } else if (falsyReg.writable && !overlapWithCondReg(falsyReg.reg)) {
      targetReg = falsyReg.reg;
    } else {
      targetReg = common_.reqScratchRegProt(resultType, regAllocTracker, false).reg;
    }
    as_.INSTR(SEL_Dc_Da_Db_Dd)
        .setDc(RegUtil::getOtherExtReg(targetReg))
        .setDa(RegUtil::getOtherExtReg(truthyReg.reg))
        .setDb(RegUtil::getOtherExtReg(falsyReg.reg))
        .setDd(condReg.reg)();
    as_.INSTR(SEL_Dc_Da_Db_Dd).setDc(targetReg).setDa(truthyReg.reg).setDb(falsyReg.reg).setDd(condReg.reg)();
  } else {
    if ((targetHintReg != REG::NONE)) {
      targetReg = targetHintReg;
    } else if (condReg.writable) {
      targetReg = condReg.reg;
    } else if (truthyReg.writable) {
      targetReg = truthyReg.reg;
    } else if (falsyReg.writable) {
      targetReg = falsyReg.reg;
    } else {
      targetReg = common_.reqScratchRegProt(resultType, regAllocTracker, false).reg;
    }
    as_.INSTR(SEL_Dc_Da_Db_Dd).setDc(targetReg).setDa(truthyReg.reg).setDb(falsyReg.reg).setDd(condReg.reg)();
  }

  if (targetHintReg == targetReg) {
    targetElement = common_.getResultStackElement(targetHint, resultType);
  } else {
    targetElement = StackElement::scratchReg(targetReg, MachineTypeUtil::toStackTypeFlag(resultType));
  }
  return targetElement;
}

#if ENABLE_EXTENSIONS
void Backend::updateRegPressureHistogram() const VB_NOEXCEPT {
  auto isScratchRegInUse = [this](REG const reg) VB_NOEXCEPT -> bool {
    assert((vb::tc::WasmABI::getRegPos(reg) >= moduleInfo_.getNumStaticallyAllocatedGPRs()) && "Cannot be used for local regs");

    Stack::iterator const refToLastOccurrence = moduleInfo_.getReferenceToLastOccurrenceOnStack(reg);
    if (!refToLastOccurrence.isEmpty()) {
      return true;
    }

    if (!RegUtil::canBeExtReg(reg)) {
      // It's a secondary reg, let's check if a 64-bit value is in the primary one
      REG const primReg = RegUtil::getOtherExtReg(reg);
      if (!isStaticallyAllocatedReg(primReg)) {
        Stack::iterator const refToLastOccurrencePrim = moduleInfo_.getReferenceToLastOccurrenceOnStack(primReg);
        StackElement const *const actualTestElem = refToLastOccurrencePrim.raw();

        if (actualTestElem != nullptr) {
          MachineType const type = moduleInfo_.getMachineType(actualTestElem);
          if (MachineTypeUtil::is64(type)) {
            return true;
          }
        }
      }
    }

    return false;
  };

  uint32_t numFreeRegs = 0U;
  // Start at numStaticallyAllocatedRegs so we do not iterate registers with locals
  for (uint32_t regPos = moduleInfo_.getNumStaticallyAllocatedGPRs(); regPos < static_cast<uint32_t>(WasmABI::dr.size()); regPos++) {
    if (!isScratchRegInUse(WasmABI::dr[regPos])) {
      numFreeRegs++;
    }
  }
  assert(numFreeRegs <= static_cast<uint32_t>(WasmABI::dr.size()));

  compiler_.getAnalytics()->updateRegPressureHistogram(true, numFreeRegs);
}
#endif

RegAllocCandidate Backend::getRegAllocCandidate(MachineType const type, RegMask const protRegs) const VB_NOEXCEPT {
  assert((!protRegs.allMarked()) && "BLOCKALL not allowed for scratch register request");

#if ENABLE_EXTENSIONS
  if (compiler_.getAnalytics() != nullptr) {
    updateRegPressureHistogram();
  }
#endif

  // Number of actual register-allocated locals and the length
  // (number) of allocatable register array for that type
  uint32_t const numStaticallyAllocatedRegs{getNumStaticallyAllocatedDr()};
  constexpr uint32_t numTotalRegs{static_cast<uint32_t>(WasmABI::dr.size())};

  bool isUsed{false};
  REG chosenReg{REG::NONE};
  bool const is64{(type == MachineType::I64) || (type == MachineType::F64)};

  if (is64) {
    // Start at numStaticallyAllocatedRegs so we do not iterate registers with locals
    for (uint32_t regPos{numStaticallyAllocatedRegs}; regPos < numTotalRegs; regPos++) {
      REG const currentReg{WasmABI::dr[regPos]};

      bool const canBeExtendedReg{RegUtil::canBeExtReg(currentReg)};
      if (!canBeExtendedReg) {
        continue;
      }

      REG const currentSecReg{RegUtil::getOtherExtReg(currentReg)};
      assert((currentSecReg == WasmABI::dr[regPos + 1U]) && "Primary and secondary reg not in order");
      if (protRegs.contains(currentReg) || protRegs.contains(currentSecReg)) {
        continue;
      }

      Stack::iterator const refToLastOccurrence{moduleInfo_.getReferenceToLastOccurrenceOnStack(currentReg)};
      Stack::iterator const secRefToLastOccurrence{moduleInfo_.getReferenceToLastOccurrenceOnStack(currentSecReg)};
      if (refToLastOccurrence.isEmpty() && secRefToLastOccurrence.isEmpty()) {
        chosenReg = currentReg;
        break;
      }
    }

    // There is no free 64-bit scratchReg here, find the first occurrence of extend register on the stack
    if (chosenReg == REG::NONE) {
      isUsed = true;
      for (StackElement const &elem : stack_) {
        if ((elem.getBaseType() == StackType::SCRATCHREGISTER) && RegUtil::isDATA(elem.data.variableData.location.reg)) {
          REG const current{elem.data.variableData.location.reg};
          REG const other{RegUtil::getOtherExtReg(current)};
          bool const isCurrentExt{RegUtil::canBeExtReg(current)};
          if (!protRegs.contains(current) && !protRegs.contains(other)) {
            chosenReg = isCurrentExt ? current : other;
            break;
          }
        }
      }
    }
  } else {
    // Start at numStaticallyAllocatedRegs so we do not iterate registers with locals
    for (uint32_t regPos{numStaticallyAllocatedRegs}; regPos < numTotalRegs; regPos++) {
      REG const currentReg{WasmABI::dr[regPos]};
      if (protRegs.contains(currentReg)) {
        continue;
      }

      bool const canBeExtendedReg{RegUtil::canBeExtReg(currentReg)};
      Stack::iterator const refToLastOccurrence{moduleInfo_.getReferenceToLastOccurrenceOnStack(currentReg)};
      REG const otherReg{RegUtil::getOtherExtReg(currentReg)};
      bool const empty{refToLastOccurrence.isEmpty()};
      bool otherIsEmptyOrLocalOr32b{true};

      if ((!canBeExtendedReg) && (!isStaticallyAllocatedReg(otherReg))) {
        // Here we have to check whether a 64b value is loaded already, otherwise it's irrelevant because it's
        // guaranteed to at most have a 32b value loaded
        Stack::iterator const otherRefToLastOccurrence{moduleInfo_.getReferenceToLastOccurrenceOnStack(otherReg)};

        if ((!otherRefToLastOccurrence.isEmpty()) && ((otherRefToLastOccurrence->type == StackType::SCRATCHREGISTER_I64) ||
                                                      (otherRefToLastOccurrence->type == StackType::SCRATCHREGISTER_F64))) {
          otherIsEmptyOrLocalOr32b = false;
        }
      }

      // If the register is not on the stack at all, we choose the current register and mark it as unused
      if (empty && otherIsEmptyOrLocalOr32b) {
        // Primary empty, secondary empty
        chosenReg = currentReg;
        break;
      }

      assert((empty || otherIsEmptyOrLocalOr32b) && "Cannot be non-empty if other has 64b scratch register");
    }

    // There is no free 32-bit scratchReg here, find the first occurrence of register on the stack
    if (chosenReg == REG::NONE) {
      isUsed = true;
      for (StackElement const &elem : stack_) {
        if (((elem.getBaseType() == StackType::SCRATCHREGISTER) && RegUtil::isDATA(elem.data.variableData.location.reg)) &&
            !protRegs.contains(elem.data.variableData.location.reg)) {
          chosenReg = elem.data.variableData.location.reg;
          break;
        }
      }
    }
  }

  assert((chosenReg != REG::NONE) && "No register found");
  return {chosenReg, isUsed};
}

bool Backend::isWritableScratchReg(StackElement const *const pElem) const VB_NOEXCEPT {
  if (pElem == nullptr) {
    return false;
  }
  if (pElem->getBaseType() != StackType::SCRATCHREGISTER) {
    return false;
  }

  bool const is64{(pElem->type == StackType::SCRATCHREGISTER_I64) || (pElem->type == StackType::SCRATCHREGISTER_F64)};

  REG const reg{pElem->data.variableData.location.reg};

  bool const canBeExtendedReg{RegUtil::canBeExtReg(pElem->data.variableData.location.reg)};
  assert((!is64 || canBeExtendedReg) && "Register not suitable for 64-bit value");

  if (!common_.isWritableScratchReg(pElem)) {
    return false;
  }

  if (is64) {
    assert(canBeExtendedReg && "Register not suitable for 64-bit value");

    // Check if secondary register is a scratch register
    REG const secondaryExtReg{RegUtil::getOtherExtReg(reg)};
    if (isStaticallyAllocatedReg(secondaryExtReg)) {
      return false;
    }

    Stack::iterator const refToLastOccurrence{moduleInfo_.getReferenceToLastOccurrenceOnStack(secondaryExtReg)};
    if (!refToLastOccurrence.isEmpty()) {
      return false;
    }
  } else {
    if (canBeExtendedReg) {
      // This is the primary, the other one definitely does not hold a 64-bit value
      return common_.isWritableScratchReg(pElem);
    } else {
      REG const primaryExtReg{RegUtil::getOtherExtReg(reg)};
      if (isStaticallyAllocatedReg(primaryExtReg)) {
        // A local is never 64 bit
        return common_.isWritableScratchReg(pElem);
      } else {
        // Check whether the first scratch reg is an extended reg and contains a 64-bit value
        Stack::iterator const refToLastOccurrence{moduleInfo_.getReferenceToLastOccurrenceOnStack(primaryExtReg)};

        if (!refToLastOccurrence.isEmpty()) {
          MachineType const actualWasmType{moduleInfo_.getMachineType(refToLastOccurrence.raw())};
          if (MachineTypeUtil::is64(actualWasmType)) {
            return false;
          }
        }
      }
    }
  }

  return true;
}

void Backend::spillFromStack(StackElement const &source, RegMask protRegs, bool const forceToStack, bool const presFlags,
                             Stack::iterator const pExcludedZoneBottom, Stack::iterator const pExcludedZoneTop) {
  if (source.getBaseType() == StackType::SCRATCHREGISTER) {
    bool const canBeExtendedReg{RegUtil::canBeExtReg(source.data.variableData.location.reg)};
    REG const otherReg{RegUtil::getOtherExtReg(source.data.variableData.location.reg)};

    if (!canBeExtendedReg) {
      // spill self and other if other is 64b and not a local
      if (!isStaticallyAllocatedReg(otherReg)) {
        Stack::iterator const otherElem{moduleInfo_.getReferenceToLastOccurrenceOnStack(otherReg)};

        if (!otherElem.isEmpty()) {
          MachineType const otherType{moduleInfo_.getMachineType(otherElem.raw())};

          if (MachineTypeUtil::is64(otherType)) {
            protRegs = protRegs | mask(otherReg, true);
            common_.spillFromStackImpl(*otherElem, protRegs, forceToStack, presFlags, pExcludedZoneBottom, pExcludedZoneTop);
          }
        }
      }
    } else {
      bool const is64{(source.type == StackType::SCRATCHREGISTER_I64) || (source.type == StackType::SCRATCHREGISTER_F64)};
      // spill self and other if self is 64b
      if (is64) {
        protRegs = protRegs | mask(otherReg, false);
        Stack::iterator const otherElem{moduleInfo_.getReferenceToLastOccurrenceOnStack(otherReg)};
        if (!otherElem.isEmpty()) {
          common_.spillFromStackImpl(*otherElem, protRegs, forceToStack, presFlags, pExcludedZoneBottom, pExcludedZoneTop);
        }
      }
    }
  }

  if (!moduleInfo_.getReferenceToLastOccurrenceOnStack(source).isEmpty()) {
    common_.spillFromStackImpl(source, protRegs, forceToStack, presFlags, pExcludedZoneBottom, pExcludedZoneTop);
  }
}

bool Backend::checkIfEnforcedTargetIsOnlyInArgs(Span<Stack::iterator> const &args, StackElement const *const enforcedTarget) const VB_NOEXCEPT {
  if (enforcedTarget == nullptr) {
    return true;
  }
  bool const isScrReg{enforcedTarget->getBaseType() == StackType::SCRATCHREGISTER};
  if (!isScrReg) {
    return common_.checkIfEnforcedTargetIsOnlyInArgs(args, enforcedTarget);
  } else {
    bool const is64{(enforcedTarget->type == StackType::SCRATCHREGISTER_I64) || (enforcedTarget->type == StackType::SCRATCHREGISTER_F64)};
    REG const enforcedReg{enforcedTarget->data.variableData.location.reg};
    bool const canBeExtendedReg{RegUtil::canBeExtReg(enforcedReg)};
    if (is64) {
      assert(canBeExtendedReg && "Must be extendable");
      StackElement const otherElem{StackElement::scratchReg(RegUtil::getOtherExtReg(enforcedReg), StackType::SCRATCHREGISTER)};
      return common_.checkIfEnforcedTargetIsOnlyInArgs(args, enforcedTarget) && common_.checkIfEnforcedTargetIsOnlyInArgs(args, &otherElem);
    } else {
      if (canBeExtendedReg) {
        return common_.checkIfEnforcedTargetIsOnlyInArgs(args, enforcedTarget);
      } else {
        StackElement const otherElem{StackElement::scratchReg(RegUtil::getOtherExtReg(enforcedReg), StackType::SCRATCHREGISTER)};
        return common_.checkIfEnforcedTargetIsOnlyInArgs(args, enforcedTarget) && common_.checkIfEnforcedTargetIsOnlyInArgs(args, &otherElem);
      }
    }
  }
}

RegMask Backend::mask(StackElement const *const elementPtr) const VB_NOEXCEPT {
  if (elementPtr == nullptr) {
    return RegMask::none();
  }

  VariableStorage const storage{moduleInfo_.getStorage(*elementPtr)};
  return mask(storage);
}

RegMask Backend::mask(VariableStorage const &storage) const VB_NOEXCEPT {
  if (storage.type == StorageType::REGISTER) {
    REG const reg{storage.location.reg};
    return mask(reg, MachineTypeUtil::is64(storage.machineType));
  }
  return RegMask::none();
}

RegMask Backend::mask(REG const reg, bool const is64) const VB_NOEXCEPT {
  if (reg == REG::NONE) {
    return RegMask();
  }
  RegMask mask{RegMask(reg)};
  if (is64) {
    assert(RegUtil::canBeExtReg(reg) && "Register not usable as extended register");
    mask.mask(RegMask(RegUtil::getOtherExtReg(reg)));
  }
  return mask;
}

StackElement Backend::emitDeferredAction(OPCode const opcode, StackElement *const arg0Ptr, StackElement *const arg1Ptr,
                                         StackElement const *const targetHint) {
  if ((opcode >= OPCode::I32_EQZ) && (opcode <= OPCode::F64_GE)) {
    return emitComparisonImpl(opcode, arg0Ptr, arg1Ptr, targetHint).elem;
  } else {
    switch (opcode) {
    case OPCode::I32_CLZ: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(I__CLZ_Dc_Da);
      return as_.selectInstr(ops, arg0Ptr, arg1Ptr, targetHint, RegMask::none());
    }
    case OPCode::I32_CTZ: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I32, arg0Ptr, nullptr, targetHint, RegMask::none(), true, false)};
      as_.INSTR(BIT_REFLECT_Dc_Da).setDc(prep.dest.reg).setDa(prep.arg0.reg)();
      as_.INSTR(CLZ_Dc_Da).setDc(prep.dest.reg).setDa(prep.dest.reg)();
      return prep.dest.elem;
    }
    case OPCode::I32_POPCNT: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(I__POPCNTW_Dc_Da);
      return as_.selectInstr(ops, arg0Ptr, arg1Ptr, targetHint, RegMask::none());
    }

    case OPCode::I32_ADD: {
      bool const arg0IsBigConst{((arg0Ptr->type == StackType::CONSTANT_I32) &&
                                 !(SignedInRangeCheck<16U>::check(bit_cast<int32_t>(arg0Ptr->data.constUnion.u32)).inRange())) &&
                                (arg1Ptr->getBaseType() != StackType::CONSTANT)};
      bool const arg1IsBigConst{((arg1Ptr->type == StackType::CONSTANT_I32) &&
                                 !(SignedInRangeCheck<16U>::check(bit_cast<int32_t>(arg1Ptr->data.constUnion.u32)).inRange())) &&
                                (arg0Ptr->getBaseType() != StackType::CONSTANT)};
      if (arg0IsBigConst || arg1IsBigConst) {
        // coverity[autosar_cpp14_a8_5_2_violation]
        auto const args = make_array(arg0Ptr, arg1Ptr);
        uint32_t const constIdx{(arg0Ptr->type == StackType::CONSTANT_I32) ? 0_U32 : 1_U32};

        Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I32, args[constIdx ^ 1U], nullptr, targetHint)};
        uint32_t const constToAdd{args[constIdx]->data.constUnion.u32};
        REG srcReg{prep.arg0.reg};

        if ((constToAdd & 0xFFFFU) != 0U) {
          as_.INSTR(ADDI_Dc_Da_const16sx).setDc(prep.dest.reg).setDa(srcReg).setConst16sx(Instruction::lower16sx(constToAdd))();
          srcReg = prep.dest.reg;
        }
        SafeUInt<16U> const reducedHighPortionToAdd{SafeUInt<32U>::fromAny(constToAdd + 0x8000U).rightShift<16U>()};
        if (reducedHighPortionToAdd.value() != 0U) {
          as_.INSTR(ADDIH_Dc_Da_const16).setDc(prep.dest.reg).setDa(srcReg).setConst16(reducedHighPortionToAdd)();
        }
        return prep.dest.elem;
      } else {
        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto ops = make_array(I__ADD_Da_const4sx, I__ADD_Da_Db, I__ADD_Da_D15_const4sx, I__ADD_D15_Da_const4sx, I__ADD_Da_D15_Db,
                                        I__ADD_D15_Da_Db, I__ADDI_Dc_Da_const16sx, I__ADD_Dc_Da_Db);
        return as_.selectInstr(ops, arg0Ptr, arg1Ptr, targetHint, RegMask::none());
      }
    }
    case OPCode::I32_SUB: {
      bool const arg0IsConst{arg0Ptr->type == StackType::CONSTANT_I32};
      bool const arg1IsConst{arg1Ptr->type == StackType::CONSTANT_I32};
      if (arg1IsConst) {
        Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I32, arg0Ptr, nullptr, targetHint)};
        uint32_t const constToAdd{0U - arg1Ptr->data.constUnion.u32};
        as_.addImmToReg(prep.arg0.reg, constToAdd, prep.dest.reg);
        return prep.dest.elem;
      } else if (arg0IsConst && SignedInRangeCheck<9>::check(bit_cast<int32_t>(arg0Ptr->data.constUnion.u32)).inRange()) {
        SignedInRangeCheck<9> const rangeCheck{SignedInRangeCheck<9>::check(bit_cast<int32_t>(arg0Ptr->data.constUnion.u32))};

        Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I32, arg1Ptr, nullptr, targetHint)};

        as_.INSTR(RSUB_Dc_Da_const9sx).setDc(prep.dest.reg).setDa(prep.arg0.reg).setConst9sx(rangeCheck.safeInt())();
        return prep.dest.elem;
      } else {
        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto ops = make_array(I__SUB_Da_Db, I__SUB_Dc_D15_Db, I__SUB_D15_Da_Db, I__SUB_Dc_Da_Db);
        return as_.selectInstr(ops, arg0Ptr, arg1Ptr, targetHint, RegMask::none());
      }
    }
    case OPCode::I32_MUL: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(I__MUL_Da_Db, I__MUL_Dc_Da_const9sx, I__MUL_Dc_Da_Db);
      return as_.selectInstr(ops, arg0Ptr, arg1Ptr, targetHint, RegMask::none());
    }

    case OPCode::I32_DIV_S:
    case OPCode::I32_DIV_U:
    case OPCode::I32_REM_S:
    case OPCode::I32_REM_U: {
      bool const isDiv{(opcode == OPCode::I32_DIV_S) || (opcode == OPCode::I32_DIV_U)};
      bool const isSigned{(opcode == OPCode::I32_DIV_S) || (opcode == OPCode::I32_REM_S)};

      DivRemAnalysisResult const analysisResult{analyzeDivRem(arg0Ptr, arg1Ptr)};

#if TC_USE_DIV
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I64, arg0Ptr, arg1Ptr, targetHint)};
#else
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I64, arg0Ptr, arg1Ptr, targetHint, RegMask::none(), false, true)};
#endif

      REG const targetReg{isDiv ? prep.dest.reg : prep.dest.secReg};

      if (!analysisResult.mustNotBeDivZero) {
        as_.cTRAP(TrapCode::DIV_ZERO, JumpCondition::i32EqConst4sx(prep.arg1.reg, SafeInt<4U>::fromConst<0>()));
      }

      // coverity[autosar_cpp14_a8_5_2_violation]
      auto const emitInstrsDivRemCore = [this, isSigned, &prep]() {
#if TC_USE_DIV
        OPCodeTemplate const op{isSigned ? DIV_Ec_Da_Db : DIVU_Ec_Da_Db};
        as_.INSTR(op).setEc(prep.dest.reg).setDa(prep.arg0.reg).setDb(prep.arg1.reg)();
#else
        OPCodeTemplate const dvinitIns = isSigned ? DVINIT_Ec_Da_Db : DVINITU_Ec_Da_Db;
        OPCodeTemplate const dvstepIns = isSigned ? DVSTEP_Ec_Ed_Db : DVSTEPU_Ec_Ed_Db;
        as_.INSTR(dvinitIns).setEc(prep.dest.reg).setDa(prep.arg0.reg).setDb(prep.arg1.reg)();
        as_.INSTR(dvstepIns).setEc(prep.dest.reg).setEd(prep.dest.reg).setDb(prep.arg1.reg)();
        as_.INSTR(dvstepIns).setEc(prep.dest.reg).setEd(prep.dest.reg).setDb(prep.arg1.reg)();
        as_.INSTR(dvstepIns).setEc(prep.dest.reg).setEd(prep.dest.reg).setDb(prep.arg1.reg)();
        as_.INSTR(dvstepIns).setEc(prep.dest.reg).setEd(prep.dest.reg).setDb(prep.arg1.reg)();
#endif
      };

      if (analysisResult.mustNotBeOverflow) {
        emitInstrsDivRemCore();
      } else {
        RegAllocTracker regAllocTracker{};
        regAllocTracker.writeProtRegs = mask(prep.arg0.reg, false) | mask(prep.arg1.reg, false);
        REG const tempReg{common_.reqScratchRegProt(MachineType::I32, targetHint, regAllocTracker, false).reg};
        as_.MOVimm(tempReg, 0x80'00'00'00U);
        RelPatchObj const dividendNotHighBit{as_.INSTR(JNE_Da_Db_disp15sx2).setDa(prep.arg0.reg).setDb(tempReg).prepJmp()};

        RelPatchObj const divisorNotNegOne{
            as_.INSTR(JNE_Da_const4sx_disp15sx2).setDa(prep.arg1.reg).setConst4sx(SafeInt<4U>::fromConst<-1>()).prepJmp()};

        if (opcode == OPCode::I32_DIV_S) {
          as_.TRAP(TrapCode::DIV_OVERFLOW);
        } else {
          as_.MOVimm(targetReg, (opcode == OPCode::I32_REM_U) ? 0x80'00'00'00U : 0U);
        }

        RelPatchObj const end{as_.INSTR(J_disp24sx2).prepJmp()};
        dividendNotHighBit.linkToHere();
        divisorNotNegOne.linkToHere();

        emitInstrsDivRemCore();

        end.linkToHere();
      }
      return StackElement::scratchReg(targetReg, StackType::SCRATCHREGISTER_I32);
    }
    case OPCode::I32_AND:
    case OPCode::I32_OR: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(make_array(I__AND_D15_const8zx, I__AND_Da_Db, I__AND_Dc_Da_Db, I__AND_Dc_Da_const9zx),
                                      make_array(I__OR_D15_const8zx, I__OR_Da_Db, I__OR_Dc_Da_Db, I__OR_Dc_Da_const9zx));
      return as_.selectInstr(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_AND)], arg0Ptr, arg1Ptr, targetHint,
                             RegMask::none());
    }
    case OPCode::I32_XOR: {
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(I__XOR_Da_Db, I__XOR_Dc_Da_const9zx, I__XOR_Dc_Da_Db);
      return as_.selectInstr(ops, arg0Ptr, arg1Ptr, targetHint, RegMask::none());
    }
    case OPCode::I32_SHL:
    case OPCode::I32_SHR_S:
    case OPCode::I32_SHR_U: {
      bool const arg1IsConst{(arg1Ptr->type == StackType::CONSTANT_I32)};
      bool const leftShift{(opcode == OPCode::I32_SHL)};

      if (arg1IsConst) {
        uint32_t const shiftCount{arg1Ptr->data.constUnion.u32 & 0x1FU};
        int32_t const adjustedShiftCount{leftShift ? static_cast<int32_t>(shiftCount) : -static_cast<int32_t>(shiftCount)};
        StackElement const adjustedShiftCountElem{StackElement::i32Const(static_cast<uint32_t>(adjustedShiftCount))};
        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto ops = make_array(make_array(I__SH_Da_const4sx, I__SH_Dc_Da_const9sx), make_array(I__SHA_Da_const4sx, I__SHA_Dc_Da_const9sx),
                                        make_array(I__SH_Da_const4sx, I__SH_Dc_Da_const9sx));
        return as_.selectInstr(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_SHL)], arg0Ptr, &adjustedShiftCountElem,
                               targetHint, RegMask::none());
      } else {
        RegAllocTracker regAllocTracker{};
        regAllocTracker.writeProtRegs = mask(arg0Ptr);
        REG const arg1Reg{common_.liftToRegInPlaceProt(*arg1Ptr, true, targetHint, regAllocTracker).reg};

        as_.andWordDcDaConst9zx(arg1Reg, arg1Reg, SafeUInt<9U>::fromConst<0x1FU>());
        if (!leftShift) {
          as_.INSTR(RSUB_Da).setDa(arg1Reg)();
        }
        StackElement const adjustedShiftCountElem{StackElement::scratchReg(arg1Reg, MachineTypeUtil::toStackTypeFlag(MachineType::I32))};
        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto ops = make_array(make_array(I__SH_Dc_Da_Db), make_array(I__SHA_Dc_Da_Db), make_array(I__SH_Dc_Da_Db));
        return as_.selectInstr(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_SHL)], arg0Ptr, &adjustedShiftCountElem,
                               targetHint, RegMask::none());
      }
    }

    case OPCode::I32_ROTL:
    case OPCode::I32_ROTR: {
      bool const arg1IsConst{(arg1Ptr->type == StackType::CONSTANT_I32)};

      if (arg1IsConst) {
        uint32_t const count{arg1Ptr->data.constUnion.u32};

        Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I32, arg0Ptr, nullptr, targetHint)};

        if (opcode == OPCode::I32_ROTL) {
          as_.INSTR(DEXTR_Dc_Da_Db_pos).setDc(prep.dest.reg).setDa(prep.arg0.reg).setDb(prep.arg0.reg).setPos(SafeUInt<5>::max() & count)();
        } else {
          as_.INSTR(DEXTR_Dc_Da_Db_pos).setDc(prep.dest.reg).setDa(prep.arg0.reg).setDb(prep.arg0.reg).setPos(SafeUInt<5>::max() & (0U - count))();
        }

        return prep.dest.elem;
      } else {
        RegAllocTracker regAllocTracker{};
        regAllocTracker.writeProtRegs = mask(arg0Ptr);
        REG const arg1Reg{common_.liftToRegInPlaceProt(*arg1Ptr, true, targetHint, regAllocTracker).reg};

        Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I32, arg0Ptr, nullptr, targetHint, mask(arg1Reg, false))};

        if (opcode == OPCode::I32_ROTL) {
          // Truncate count because rotation count > 31 is undefined in TriCore
          as_.andWordDcDaConst9zx(arg1Reg, arg1Reg, SafeUInt<9U>::fromConst<0x1FU>());
          as_.INSTR(DEXTR_Dc_Da_Db_Dd).setDc(prep.dest.reg).setDa(prep.arg0.reg).setDb(prep.arg0.reg).setDd(arg1Reg)();
        } else {
          as_.INSTR(RSUB_Dc_Da_const9sx).setDc(arg1Reg).setDa(arg1Reg).setConst9sx(SafeInt<9>::fromConst<32>())();
          // Truncate count because rotation count > 31 is undefined in TriCore
          as_.andWordDcDaConst9zx(arg1Reg, arg1Reg, SafeUInt<9U>::fromConst<0x1FU>());
          as_.INSTR(DEXTR_Dc_Da_Db_Dd).setDc(prep.dest.reg).setDa(prep.arg0.reg).setDb(prep.arg0.reg).setDd(arg1Reg)();
        }
        return prep.dest.elem;
      }
    }

    case OPCode::I64_CLZ: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I64, arg0Ptr, nullptr, targetHint)};

      RelPatchObj const higherIsNotZero{
          as_.INSTR(JNE_Da_const4sx_disp15sx2).setDa(prep.arg0.secReg).setConst4sx(SafeInt<4U>::fromConst<0>()).prepJmp()};
      // 32 MSB are zero
      as_.INSTR(CLZ_Dc_Da).setDc(prep.dest.reg).setDa(prep.arg0.reg)();
      as_.INSTR(ADDI_Dc_Da_const16sx).setDc(prep.dest.reg).setDa(prep.dest.reg).setConst16sx(SafeInt<16U>::fromConst<32>())();

      RelPatchObj const finally{as_.INSTR(J_disp24sx2).prepJmp()};
      higherIsNotZero.linkToHere();
      // 32 MSB are not zero
      as_.INSTR(CLZ_Dc_Da).setDc(prep.dest.reg).setDa(prep.arg0.secReg)();

      finally.linkToHere();
      as_.INSTR(MOV_Da_const4sx).setDa(prep.dest.secReg).setConst4sx(SafeInt<4U>::fromConst<0>())();

      return prep.dest.elem;
    }
    case OPCode::I64_CTZ: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I64, arg0Ptr, nullptr, targetHint, RegMask::none(), true, false)};
      // coverity[autosar_cpp14_a8_5_2_violation]
      auto const regCtz = [this](REG const dest, REG const src) {
        as_.INSTR(MOVU_Dc_const16zx).setDc(dest).setConst16zx(SafeUInt<16>::fromConst<32U>())();
        RelPatchObj const zero{as_.INSTR(JEQ_Da_const4sx_disp15sx2).setDa(src).setConst4sx(SafeInt<4U>::fromConst<0>()).prepJmp()};
        // TODO(Congcong): Optimize with HighTec Clang - had a better implementation
        as_.INSTR(RSUB_Dc_Da_const9sx).setDc(dest).setDa(src).setConst9sx(SafeInt<9>::fromConst<0>())();
        as_.INSTR(AND_Da_Db).setDa(dest).setDb(src)();
        as_.INSTR(CLZ_Dc_Da).setDc(dest).setDa(dest)();
        as_.INSTR(RSUB_Dc_Da_const9sx).setDc(dest).setDa(dest).setConst9sx(SafeInt<9>::fromConst<31>())();
        zero.linkToHere();
      };

      RelPatchObj const lowerIsNotZero{as_.INSTR(JNE_Da_const4sx_disp15sx2).setDa(prep.arg0.reg).setConst4sx(SafeInt<4U>::fromConst<0>()).prepJmp()};
      // Lower is zero
      // coverity[autosar_cpp14_a4_5_1_violation]
      regCtz(prep.dest.reg, prep.arg0.secReg);
      as_.INSTR(ADDI_Dc_Da_const16sx).setDc(prep.dest.reg).setDa(prep.dest.reg).setConst16sx(SafeInt<16U>::fromConst<32>())();
      RelPatchObj const finally{as_.INSTR(J_disp24sx2).prepJmp()};
      lowerIsNotZero.linkToHere();
      // Lower is not zero
      // coverity[autosar_cpp14_a4_5_1_violation]
      regCtz(prep.dest.reg, prep.arg0.reg);

      finally.linkToHere();
      as_.INSTR(MOV_Da_const4sx).setDa(prep.dest.secReg).setConst4sx(SafeInt<4U>::fromConst<0>())();

      return prep.dest.elem;
    }
    case OPCode::I64_POPCNT: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I64, arg0Ptr, nullptr, targetHint)};
      as_.INSTR(POPCNTW_Dc_Da).setDc(prep.dest.reg).setDa(prep.arg0.reg)();
      as_.INSTR(POPCNTW_Dc_Da).setDc(prep.dest.secReg).setDa(prep.arg0.secReg)();
      as_.INSTR(ADD_Da_Db).setDa(prep.dest.reg).setDb(prep.dest.secReg)();

      as_.INSTR(MOV_Da_const4sx).setDa(prep.dest.secReg).setConst4sx(SafeInt<4U>::fromConst<0>())();

      return prep.dest.elem;
    }
    case OPCode::I64_ADD:
    case OPCode::I64_SUB: {
      StackElement targetElement{};

      if (opcode == OPCode::I64_ADD) {
        targetElement = emitI64AddImm(*arg0Ptr, *arg1Ptr, targetHint, true);
      } else {
        if (arg1Ptr->type == StackType::CONSTANT_I64) {
          // try convert `sub a, imm` to `add a, -imm`
          targetElement = emitI64AddImm(*arg0Ptr, StackElement::i64Const(0U - arg1Ptr->data.constUnion.u64), targetHint, false);
        }
      }
      if (targetElement.type != StackType::INVALID) {
        return targetElement;
      }

      /// operand can't be encoded as imm, use registers
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I64, arg0Ptr, arg1Ptr, targetHint)};

      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto xops = make_array(ADDX_Dc_Da_Db, SUBX_Dc_Da_Db);
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto cops = make_array(ADDC_Dc_Da_Db, SUBC_Dc_Da_Db);

      as_.INSTR(xops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_ADD)])
          .setDc(prep.dest.reg)
          .setDa(prep.arg0.reg)
          .setDb(prep.arg1.reg)();
      as_.INSTR(cops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_ADD)])
          .setDc(prep.dest.secReg)
          .setDa(prep.arg0.secReg)
          .setDb(prep.arg1.secReg)();

      return prep.dest.elem;
    }
    case OPCode::I64_MUL: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I64, arg0Ptr, arg1Ptr, targetHint, RegMask::none(), true, true)};

      as_.INSTR(MULU_Ec_Da_Db).setEc(prep.dest.reg).setDa(prep.arg0.reg).setDb(prep.arg1.reg)();
      as_.INSTR(MADD_Dc_Dd_Da_Db).setDc(prep.dest.secReg).setDd(prep.dest.secReg).setDa(prep.arg0.reg).setDb(prep.arg1.secReg)();
      as_.INSTR(MADD_Dc_Dd_Da_Db).setDc(prep.dest.secReg).setDd(prep.dest.secReg).setDa(prep.arg0.secReg).setDb(prep.arg1.reg)();

      return prep.dest.elem;
    }

    case OPCode::I64_DIV_S:
    case OPCode::I64_DIV_U:
    case OPCode::I64_REM_S:
    case OPCode::I64_REM_U: {
      DivRemAnalysisResult const analysisResult{analyzeDivRem(arg0Ptr, arg1Ptr)};

      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I64, arg0Ptr, arg1Ptr, targetHint)};

      // coverity[autosar_cpp14_a8_5_2_violation]
      auto const emitInstrsDivRemCore = [this, opcode, &prep]() {
        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto ops = make_array(DIV64_Ec_Ea_Eb, DIV64U_Ec_Ea_Eb, REM64_Ec_Ea_Eb, REM64U_Ec_Ea_Eb);
        as_.INSTR(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_DIV_S)])
            .setEc(prep.dest.reg)
            .setEa(prep.arg0.reg)
            .setEb(prep.arg1.reg)();
      };

      if (!analysisResult.mustNotBeDivZero) {
        // check divisor not zero
        RelPatchObj const lowDivisorNotZero{
            as_.INSTR(JNE_Da_const4sx_disp15sx2).setDa(prep.arg1.reg).setConst4sx(SafeInt<4U>::fromConst<0>()).prepJmp()};
        RelPatchObj const highDivisorNotZero{
            as_.INSTR(JNE_Da_const4sx_disp15sx2).setDa(prep.arg1.secReg).setConst4sx(SafeInt<4U>::fromConst<0>()).prepJmp()};
        as_.TRAP(TrapCode::DIV_ZERO);
        lowDivisorNotZero.linkToHere();
        highDivisorNotZero.linkToHere();
      }

      if (analysisResult.mustNotBeOverflow) {
        emitInstrsDivRemCore();
      } else {
        RegAllocTracker regAllocTracker{};
        regAllocTracker.writeProtRegs = mask(prep.arg0.reg, true) | mask(prep.arg1.reg, true);
        REG const tempReg{common_.reqScratchRegProt(MachineType::I32, targetHint, regAllocTracker, false).reg};

        // Dividend not 0x8000'0000'0000'0000
        RelPatchObj const lowDividendNotZero{
            as_.INSTR(JNE_Da_const4sx_disp15sx2).setDa(prep.arg0.reg).setConst4sx(SafeInt<4U>::fromConst<0>()).prepJmp()};
        as_.MOVimm(tempReg, 0x80'00'00'00U);
        RelPatchObj const highDividendNotHighBit{as_.INSTR(JNE_Da_Db_disp15sx2).setDa(prep.arg0.secReg).setDb(tempReg).prepJmp()};
        //

        // Divisor not -1
        RelPatchObj const lowDivisorNotNegOne{
            as_.INSTR(JNE_Da_const4sx_disp15sx2).setDa(prep.arg1.reg).setConst4sx(SafeInt<4U>::fromConst<-1>()).prepJmp()};
        RelPatchObj const highDivisorNotNegOne{
            as_.INSTR(JNE_Da_const4sx_disp15sx2).setDa(prep.arg1.secReg).setConst4sx(SafeInt<4U>::fromConst<-1>()).prepJmp()};
        //

        if (opcode == OPCode::I64_DIV_S) {
          as_.TRAP(TrapCode::DIV_OVERFLOW);
        } else {
          as_.MOVimm(prep.dest.reg, 0U);
          as_.MOVimm(prep.dest.secReg, (opcode == OPCode::I64_REM_U) ? 0x80'00'00'00U : 0U);
        }

        RelPatchObj const end{as_.INSTR(J_disp24sx2).prepJmp()};
        //
        lowDividendNotZero.linkToHere();
        highDividendNotHighBit.linkToHere();
        //
        lowDivisorNotNegOne.linkToHere();
        highDivisorNotNegOne.linkToHere();

        emitInstrsDivRemCore();

        end.linkToHere();
      }
      return prep.dest.elem;
    }
    case OPCode::I64_AND:
    case OPCode::I64_OR:
    case OPCode::I64_XOR: {
      StackElement const targetElement{emitI64AndOrImm(opcode, *arg0Ptr, *arg1Ptr, targetHint)};

      if (targetElement.type != StackType::INVALID) {
        return targetElement;
      } else {
        Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I64, arg0Ptr, arg1Ptr, targetHint)};

        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto ops = make_array(AND_Dc_Da_Db, OR_Dc_Da_Db, XOR_Dc_Da_Db);
        as_.INSTR(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_AND)])
            .setDc(prep.dest.reg)
            .setDa(prep.arg0.reg)
            .setDb(prep.arg1.reg)();
        as_.INSTR(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_AND)])
            .setDc(prep.dest.secReg)
            .setDa(prep.arg0.secReg)
            .setDb(prep.arg1.secReg)();

        return prep.dest.elem;
      }
    }
    case OPCode::I64_SHL:
    case OPCode::I64_SHR_S:
    case OPCode::I64_SHR_U:
    case OPCode::I64_ROTL:
    case OPCode::I64_ROTR: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I64, arg0Ptr, arg1Ptr, targetHint)};
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto fncs = make_array(aux::MappedFncs::I64_SHL, aux::MappedFncs::I64_SHR_S, aux::MappedFncs::I64_SHR_U, aux::MappedFncs::I64_ROTL,
                                       aux::MappedFncs::I64_ROTR);
      simpleNativeFncCall(prep.dest.reg, true, prep.arg0.reg, true, prep.arg1.reg, true,
                          fncs[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_SHL)]);

      return prep.dest.elem;
    }

    case OPCode::F32_ABS: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::F32, arg0Ptr, nullptr, targetHint)};

      as_.INSTR(INSERT_Dc_Da_const4_pos_width)
          .setDc(prep.dest.reg)
          .setDa(prep.arg0.reg)
          .setConst4sx(SafeInt<4U>::fromConst<0>())
          .setPos(SafeUInt<5>::fromConst<31U>())
          .setWidth(SafeUInt<5U>::fromConst<1U>())();

      return prep.dest.elem;
    }
    case OPCode::F32_NEG: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::F32, arg0Ptr, nullptr, targetHint)};

      as_.INSTR(INSNT_Dc_Da_pos1_Db_pos2)
          .setDc(prep.dest.reg)
          .setDa(prep.arg0.reg)
          .setPos1(SafeUInt<5>::fromConst<31U>())
          .setDb(prep.arg0.reg)
          .setPos2(SafeUInt<5>::fromConst<31U>())();

      return prep.dest.elem;
    }

    case OPCode::F32_CEIL:
    case OPCode::F32_FLOOR:
    case OPCode::F32_TRUNC:
    case OPCode::F32_NEAREST:
    case OPCode::F32_SQRT: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::F32, arg0Ptr, nullptr, targetHint)};

      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto fncs = make_array(aux::MappedFncs::F32_CEIL, aux::MappedFncs::F32_FLOOR, aux::MappedFncs::F32_TRUNC,
                                       aux::MappedFncs::F32_NEAREST, aux::MappedFncs::F32_SQRT);
      simpleNativeFncCall(prep.dest.reg, false, prep.arg0.reg, false, REG::NONE, false,
                          fncs[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::F32_CEIL)]);

      return prep.dest.elem;
    }

    case OPCode::F32_ADD:
    case OPCode::F32_SUB: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::F32, arg0Ptr, arg1Ptr, targetHint)};

#if TC_USE_HARD_F32_ARITHMETICS
      auto const ops = make_array(ADDF_Dc_Dd_Da, SUBF_Dc_Dd_Da);
      as_.INSTR(ops[opcode - OPCode::F32_ADD]).setDc(prep.dest.reg).setDd(prep.arg0.reg).setDa(prep.arg1.reg)();
#else
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto fncs = make_array(aux::MappedFncs::F32_ADD, aux::MappedFncs::F32_SUB);
      simpleNativeFncCall(prep.dest.reg, false, prep.arg0.reg, false, prep.arg1.reg, false,
                          fncs[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::F32_ADD)]);
#endif

      return prep.dest.elem;
    }

    case OPCode::F32_MUL:
    case OPCode::F32_DIV: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::F32, arg0Ptr, arg1Ptr, targetHint)};

#if TC_USE_HARD_F32_ARITHMETICS
      auto const ops = make_array(MULF_Dc_Da_Db, DIVF_Dc_Da_Db);
      as_.INSTR(ops[opcode - OPCode::F32_MUL]).setDc(prep.dest.reg).setDa(prep.arg0.reg).setDb(prep.arg1.reg)();
#else
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto fncs = make_array(aux::MappedFncs::F32_MUL, aux::MappedFncs::F32_DIV);
      simpleNativeFncCall(prep.dest.reg, false, prep.arg0.reg, false, prep.arg1.reg, false,
                          fncs[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::F32_MUL)]);
#endif

      return prep.dest.elem;
    }
    case OPCode::F32_MIN:
    case OPCode::F32_MAX: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::F32, arg0Ptr, arg1Ptr, targetHint)};

      RegAllocTracker regAllocTracker{};
      regAllocTracker.writeProtRegs = mask(prep.arg0.reg, false) | mask(prep.arg1.reg, false);
      REG const helperReg{common_.reqScratchRegProt(MachineType::I32, targetHint, regAllocTracker, false).reg};
      as_.INSTR(CMPF_Dc_Da_Db).setDc(helperReg).setDa(prep.arg0.reg).setDb(prep.arg1.reg)();

      as_.andWordDcDaConst9zx(helperReg, helperReg, SafeUInt<9U>::fromConst<static_cast<uint32_t>(CMPFFLAGS::UNORD)>());

      RelPatchObj const unordered{as_.INSTR(JNE_Da_const4sx_disp15sx2).setDa(helperReg).setConst4sx(SafeInt<4U>::fromConst<0>()).prepJmp()};
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(MINF_Dc_Da_Db, MAXF_Dc_Da_Db);
      as_.INSTR(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::F32_MIN)])
          .setDc(prep.dest.reg)
          .setDa(prep.arg0.reg)
          .setDb(prep.arg1.reg)();

      RelPatchObj const branchObj{as_.INSTR(J_disp24sx2).prepJmp()};

      unordered.linkToHere();

      // At least one is NaN, return NAN canonical

      as_.MOVimm(prep.dest.reg, 0x7FC00000U);

      branchObj.linkToHere();

      return prep.dest.elem;
    }
    case OPCode::F32_COPYSIGN: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::F32, arg0Ptr, arg1Ptr, targetHint)};

      // ins.t   %d2, %d4, 31, %d5, 31
      as_.INSTR(INST_Dc_Da_pos1_Db_pos2)
          .setDc(prep.dest.reg)
          .setDa(prep.arg0.reg)
          .setPos1(SafeUInt<5>::fromConst<31U>())
          .setDb(prep.arg1.reg)
          .setPos2(SafeUInt<5>::fromConst<31U>())();

      return prep.dest.elem;
    }

    case OPCode::F64_ABS: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::F64, arg0Ptr, nullptr, targetHint)};

      as_.INSTR(INSERT_Dc_Da_const4_pos_width)
          .setDc(prep.dest.secReg)
          .setDa(prep.arg0.secReg)
          .setConst4sx(SafeInt<4U>::fromConst<0>())
          .setPos(SafeUInt<5>::fromConst<31U>())
          .setWidth(SafeUInt<5U>::fromConst<1U>())();

      return prep.dest.elem;
    }
    case OPCode::F64_NEG: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::F64, arg0Ptr, nullptr, targetHint)};

      as_.INSTR(INSNT_Dc_Da_pos1_Db_pos2)
          .setDc(prep.dest.secReg)
          .setDa(prep.arg0.secReg)
          .setPos1(SafeUInt<5>::fromConst<31U>())
          .setDb(prep.arg0.secReg)
          .setPos2(SafeUInt<5>::fromConst<31U>())();

      return prep.dest.elem;
    }
    case OPCode::F64_CEIL:
    case OPCode::F64_FLOOR:
    case OPCode::F64_TRUNC:
    case OPCode::F64_NEAREST:
    case OPCode::F64_SQRT: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::F64, arg0Ptr, nullptr, targetHint)};

      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto fncs = make_array(aux::MappedFncs::F64_CEIL, aux::MappedFncs::F64_FLOOR, aux::MappedFncs::F64_TRUNC,
                                       aux::MappedFncs::F64_NEAREST, aux::MappedFncs::F64_SQRT);
      simpleNativeFncCall(prep.dest.reg, true, prep.arg0.reg, true, REG::NONE, false,
                          fncs[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::F64_CEIL)]);

      return prep.dest.elem;
    }
    case OPCode::F64_ADD:
    case OPCode::F64_SUB: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::F64, arg0Ptr, arg1Ptr, targetHint)};
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(ADDDF_Ec_Ed_Ea, SUBDF_Ec_Ed_Ea);
      as_.INSTR(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::F64_ADD)])
          .setEc(prep.dest.reg)
          .setEd(prep.arg0.reg)
          .setEa(prep.arg1.reg)();
      f64NanToCanonical(prep.dest.reg);

      return prep.dest.elem;
    }
    case OPCode::F64_MUL:
    case OPCode::F64_DIV: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::F64, arg0Ptr, arg1Ptr, targetHint)};
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(MULDF_Ec_Ea_Eb, DIVDF_Ec_Ea_Eb);
      as_.INSTR(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::F64_MUL)])
          .setEc(prep.dest.reg)
          .setEa(prep.arg0.reg)
          .setEb(prep.arg1.reg)();
      f64NanToCanonical(prep.dest.reg);

      return prep.dest.elem;
    }
    case OPCode::F64_MIN:
    case OPCode::F64_MAX: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::F64, arg0Ptr, arg1Ptr, targetHint)};

      RegAllocTracker regAllocTracker{};
      regAllocTracker.writeProtRegs = mask(prep.arg0.reg, true) | mask(prep.arg1.reg, true);
      REG const helperReg{common_.reqScratchRegProt(MachineType::I32, targetHint, regAllocTracker, false).reg};
      as_.INSTR(CMPDF_Dc_Ea_Eb).setDc(helperReg).setEa(prep.arg0.reg).setEb(prep.arg1.reg)();

      as_.andWordDcDaConst9zx(helperReg, helperReg, SafeUInt<9U>::fromConst<static_cast<uint32_t>(CMPFFLAGS::UNORD)>());

      RelPatchObj const unordered{as_.INSTR(JNE_Da_const4sx_disp15sx2).setDa(helperReg).setConst4sx(SafeInt<4U>::fromConst<0>()).prepJmp()};
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(MINDF_Ec_Ea_Eb, MAXDF_Ec_Ea_Eb);
      as_.INSTR(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::F64_MIN)])
          .setEc(prep.dest.reg)
          .setEa(prep.arg0.reg)
          .setEb(prep.arg1.reg)();

      RelPatchObj const branchObj{as_.INSTR(J_disp24sx2).prepJmp()};

      unordered.linkToHere();

      // At least one is NaN, return NAN canonical

      as_.MOVimm(prep.dest.reg, 0U);

      as_.MOVimm(prep.dest.secReg, 0x7F'F8'00'00U);

      branchObj.linkToHere();

      return prep.dest.elem;
    }
    case OPCode::F64_COPYSIGN: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::F64, arg0Ptr, arg1Ptr, targetHint)};

      if (prep.dest.reg != prep.arg0.reg) {
        as_.INSTR(MOV_Da_Db).setDa(prep.dest.reg).setDb(prep.arg0.reg)();
      }

      // ins.t   %d3, %d3, 31, %d7, 31
      as_.INSTR(INST_Dc_Da_pos1_Db_pos2)
          .setDc(prep.dest.secReg)
          .setDa(prep.arg0.secReg)
          .setPos1(SafeUInt<5>::fromConst<31U>())
          .setDb(prep.arg1.secReg)
          .setPos2(SafeUInt<5>::fromConst<31U>())();

      return prep.dest.elem;
    }

    case OPCode::I32_WRAP_I64: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I32, arg0Ptr, nullptr, targetHint)};

      if (prep.arg0.reg != prep.dest.reg) {
        as_.INSTR(MOV_Da_Db).setDa(prep.dest.reg).setDb(prep.arg0.reg)();
      }

      return prep.dest.elem;
    }

    case OPCode::I32_TRUNC_F32_S:
    case OPCode::I32_TRUNC_F32_U: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I32, arg0Ptr, nullptr, targetHint)};

      { // Compare bounds

        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto rawUpperLimits = make_array(FloatTruncLimitsExcl::I32_F32_S_MAX, FloatTruncLimitsExcl::I32_F32_U_MAX);
        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto rawLowerLimits = make_array(FloatTruncLimitsExcl::I32_F32_S_MIN, FloatTruncLimitsExcl::I32_F32_U_MIN);

        RegAllocTracker regAllocTracker{};
        regAllocTracker.writeProtRegs = mask(prep.arg0.reg, false);
        REG const helperReg{common_.reqScratchRegProt(MachineType::F32, targetHint, regAllocTracker, false).reg};
        as_.MOVimm(helperReg, rawUpperLimits[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_TRUNC_F32_S)]);

        emitCMPF32(helperReg, prep.arg0.reg, helperReg);
        constexpr uint32_t immCond{static_cast<uint32_t>(CMPFFLAGS::GT) | static_cast<uint32_t>(CMPFFLAGS::EQ) |
                                   static_cast<uint32_t>(CMPFFLAGS::UNORD)};
        as_.andWordDcDaConst9zx(helperReg, helperReg, SafeUInt<9U>::fromConst<immCond>());
        as_.cTRAP(TrapCode::TRUNC_OVERFLOW, JumpCondition::i32NeConst4sx(helperReg, SafeInt<4U>::fromConst<0>()));

        as_.MOVimm(helperReg, rawLowerLimits[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_TRUNC_F32_S)]);

        emitCMPF32(helperReg, prep.arg0.reg, helperReg);

        constexpr uint32_t bitToCheck{log2Constexpr(static_cast<uint32_t>(CMPFFLAGS::GT))};
        as_.cTRAP(TrapCode::TRUNC_OVERFLOW, JumpCondition::bitFalse(helperReg, SafeInt<4U>::fromConst<bitToCheck>()));
      }

#if TC_USE_HARD_F32_TO_I32_CONVERSIONS
      auto const ops = make_array(FTOIZ_Dc_Da, FTOUZ_Dc_Da);
      as_.INSTR(ops[opcode - OPCode::I32_TRUNC_F32_S]).setDc(prep.dest.reg).setDa(prep.arg0.reg)();
#else
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto fncs = make_array(aux::MappedFncs::I32_TRUNC_F32_S, aux::MappedFncs::I32_TRUNC_F32_U);
      simpleNativeFncCall(prep.dest.reg, false, prep.arg0.reg, false, REG::NONE, false,
                          fncs[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_TRUNC_F32_S)]);
#endif
      return prep.dest.elem;
    }

    case OPCode::I32_TRUNC_F64_S:
    case OPCode::I32_TRUNC_F64_U: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I32, arg0Ptr, nullptr, targetHint)};

      { // Compare bounds

        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto rawUpperLimits = make_array(FloatTruncLimitsExcl::I32_F64_S_MAX, FloatTruncLimitsExcl::I32_F64_U_MAX);
        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto rawLowerLimits = make_array(FloatTruncLimitsExcl::I32_F64_S_MIN, FloatTruncLimitsExcl::I32_F64_U_MIN);

        RegAllocTracker regAllocTracker{};
        regAllocTracker.writeProtRegs = mask(prep.arg0.reg, false);
        REG const helperReg{common_.reqScratchRegProt(MachineType::F64, targetHint, regAllocTracker, false).reg};
        uint64_t const rawUpperLimit{rawUpperLimits[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_TRUNC_F64_S)]};
        as_.MOVimm(helperReg, static_cast<uint32_t>(rawUpperLimit));
        as_.MOVimm(RegUtil::getOtherExtReg(helperReg), static_cast<uint32_t>(rawUpperLimit >> 32LLU));

        emitCMPF64(helperReg, prep.arg0.reg, helperReg);
        constexpr uint32_t immCond{static_cast<uint32_t>(CMPFFLAGS::GT) | static_cast<uint32_t>(CMPFFLAGS::EQ) |
                                   static_cast<uint32_t>(CMPFFLAGS::UNORD)};
        as_.andWordDcDaConst9zx(helperReg, helperReg, SafeUInt<9U>::fromConst<immCond>());
        as_.cTRAP(TrapCode::TRUNC_OVERFLOW, JumpCondition::i32NeConst4sx(helperReg, SafeInt<4U>::fromConst<0>()));

        // Second Comparison
        uint64_t const rawLowerLimit{rawLowerLimits[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_TRUNC_F64_S)]};
        as_.MOVimm(helperReg, static_cast<uint32_t>(rawLowerLimit));
        as_.MOVimm(RegUtil::getOtherExtReg(helperReg), static_cast<uint32_t>(rawLowerLimit >> 32LLU));

        emitCMPF64(helperReg, prep.arg0.reg, helperReg);

        constexpr uint32_t bitToCheck{log2Constexpr(static_cast<uint32_t>(CMPFFLAGS::GT))};
        as_.cTRAP(TrapCode::TRUNC_OVERFLOW, JumpCondition::bitFalse(helperReg, SafeInt<4U>::fromConst<bitToCheck>()));
      }
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(DFTOIZ_Dc_Ea, DFTOUZ_Dc_Ea);
      as_.INSTR(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_TRUNC_F64_S)]).setDc(prep.dest.reg).setEa(prep.arg0.reg)();

      return prep.dest.elem;
    }

    case OPCode::I64_EXTEND_I32_S:
    case OPCode::I64_EXTEND_I32_U: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I64, arg0Ptr, nullptr, targetHint)};

      if (opcode == OPCode::I64_EXTEND_I32_S) {
        as_.INSTR(MOV_Ec_Db).setEc(prep.dest.reg).setDb(prep.arg0.reg)();
      } else {
        if (prep.arg0.reg != prep.dest.reg) {
          as_.INSTR(MOV_Da_Db).setDa(prep.dest.reg).setDb(prep.arg0.reg)();
        }
        as_.INSTR(MOV_Da_const4sx).setDa(prep.dest.secReg).setConst4sx(SafeInt<4U>::fromConst<0>())();
      }

      return prep.dest.elem;
    }
    case OPCode::I32_EXTEND8_S:
    case OPCode::I32_EXTEND16_S: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I32, arg0Ptr, nullptr, targetHint)};

      SafeUInt<5U> const width{(opcode == OPCode::I32_EXTEND8_S) ? SafeUInt<5U>::fromConst<8U>() : SafeUInt<5U>::fromConst<16U>()};
      as_.INSTR(EXTR_Dc_Da_pos_width).setDc(prep.dest.reg).setDa(prep.arg0.reg).setPos(SafeUInt<5>::fromConst<0U>()).setWidth(width)();

      return prep.dest.elem;
    }
    case OPCode::I64_EXTEND8_S:
    case OPCode::I64_EXTEND16_S: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I64, arg0Ptr, nullptr, targetHint)};

      SafeUInt<5U> const width{(opcode == OPCode::I64_EXTEND8_S) ? SafeUInt<5U>::fromConst<8U>() : SafeUInt<5U>::fromConst<16U>()};
      as_.INSTR(EXTR_Dc_Da_pos_width).setDc(prep.dest.reg).setDa(prep.arg0.reg).setPos(SafeUInt<5>::fromConst<0U>()).setWidth(width)();
      as_.INSTR(MOV_Ec_Db).setEc(prep.dest.reg).setDb(prep.dest.reg)();

      return prep.dest.elem;
    }
    case OPCode::I64_EXTEND32_S: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I64, arg0Ptr, nullptr, targetHint)};

      as_.INSTR(MOV_Ec_Db).setEc(prep.dest.reg).setDb(prep.arg0.reg)();

      return prep.dest.elem;
    }
    case OPCode::I64_TRUNC_F32_S:
    case OPCode::I64_TRUNC_F32_U: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I64, arg0Ptr, nullptr, targetHint)};

      { // Compare bounds

        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto rawUpperLimits = make_array(FloatTruncLimitsExcl::I64_F32_S_MAX, FloatTruncLimitsExcl::I64_F32_U_MAX);
        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto rawLowerLimits = make_array(FloatTruncLimitsExcl::I64_F32_S_MIN, FloatTruncLimitsExcl::I64_F32_U_MIN);

        RegAllocTracker regAllocTracker{};
        regAllocTracker.writeProtRegs = mask(prep.arg0.reg, false);
        REG const helperReg{common_.reqScratchRegProt(MachineType::F32, targetHint, regAllocTracker, false).reg};
        as_.MOVimm(helperReg, rawUpperLimits[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_TRUNC_F32_S)]);

        emitCMPF32(helperReg, prep.arg0.reg, helperReg);
        constexpr uint32_t immCond{static_cast<uint32_t>(CMPFFLAGS::GT) | static_cast<uint32_t>(CMPFFLAGS::EQ) |
                                   static_cast<uint32_t>(CMPFFLAGS::UNORD)};
        as_.andWordDcDaConst9zx(helperReg, helperReg, SafeUInt<9U>::fromConst<immCond>());
        as_.cTRAP(TrapCode::TRUNC_OVERFLOW, JumpCondition::i32NeConst4sx(helperReg, SafeInt<4U>::fromConst<0>()));

        // Second Comparison
        as_.MOVimm(helperReg, rawLowerLimits[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_TRUNC_F32_S)]);

        emitCMPF32(helperReg, prep.arg0.reg, helperReg);

        constexpr uint32_t bitToCheck{log2Constexpr(static_cast<uint32_t>(CMPFFLAGS::GT))};
        as_.cTRAP(TrapCode::TRUNC_OVERFLOW, JumpCondition::bitFalse(helperReg, SafeInt<4U>::fromConst<bitToCheck>()));
      }

      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto fncs = make_array(aux::MappedFncs::I64_TRUNC_F32_S, aux::MappedFncs::I64_TRUNC_F32_U);
      simpleNativeFncCall(prep.dest.reg, true, prep.arg0.reg, false, REG::NONE, false,
                          fncs[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_TRUNC_F32_S)]);

      return prep.dest.elem;
    }
    case OPCode::I64_TRUNC_F64_S:
    case OPCode::I64_TRUNC_F64_U: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I64, arg0Ptr, nullptr, targetHint)};

      { // Compare bounds

        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto rawUpperLimits = make_array(FloatTruncLimitsExcl::I64_F64_S_MAX, FloatTruncLimitsExcl::I64_F64_U_MAX);
        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto rawLowerLimits = make_array(FloatTruncLimitsExcl::I64_F64_S_MIN, FloatTruncLimitsExcl::I64_F64_U_MIN);

        RegAllocTracker regAllocTracker{};
        regAllocTracker.writeProtRegs = mask(prep.arg0.reg, true);
        REG const helperReg{common_.reqScratchRegProt(MachineType::F64, targetHint, regAllocTracker, false).reg};
        uint64_t const rawUpperLimit{rawUpperLimits[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_TRUNC_F64_S)]};
        as_.MOVimm(helperReg, static_cast<uint32_t>(rawUpperLimit));
        as_.MOVimm(RegUtil::getOtherExtReg(helperReg), static_cast<uint32_t>(rawUpperLimit >> 32LLU));

        emitCMPF64(helperReg, prep.arg0.reg, helperReg);
        constexpr uint32_t immCond{static_cast<uint32_t>(CMPFFLAGS::GT) | static_cast<uint32_t>(CMPFFLAGS::EQ) |
                                   static_cast<uint32_t>(CMPFFLAGS::UNORD)};
        as_.andWordDcDaConst9zx(helperReg, helperReg, SafeUInt<9U>::fromConst<immCond>());
        as_.cTRAP(TrapCode::TRUNC_OVERFLOW, JumpCondition::i32NeConst4sx(helperReg, SafeInt<4U>::fromConst<0>()));

        // Second Comparison
        uint64_t const rawLowerLimit{rawLowerLimits[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_TRUNC_F64_S)]};
        as_.MOVimm(helperReg, static_cast<uint32_t>(rawLowerLimit));
        as_.MOVimm(RegUtil::getOtherExtReg(helperReg), static_cast<uint32_t>(rawLowerLimit >> 32LLU));

        emitCMPF64(helperReg, prep.arg0.reg, helperReg);

        constexpr uint32_t bitToCheck{log2Constexpr(static_cast<uint32_t>(CMPFFLAGS::GT))};
        as_.cTRAP(TrapCode::TRUNC_OVERFLOW, JumpCondition::bitFalse(helperReg, SafeInt<4U>::fromConst<bitToCheck>()));
      }
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(DFTOLZ_Ec_Ea, DFTOULZ_Ec_Ea);
      as_.INSTR(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_TRUNC_F64_S)]).setEc(prep.dest.reg).setEa(prep.arg0.reg)();

      return prep.dest.elem;
    }

    case OPCode::F32_CONVERT_I32_S:
    case OPCode::F32_CONVERT_I32_U: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::F32, arg0Ptr, nullptr, targetHint)};
#if TC_USE_HARD_F32_TO_I32_CONVERSIONS
      auto const ops = make_array(ITOF_Dc_Da, UTOF_Dc_Da);
      as_.INSTR(ops[opcode - OPCode::F32_CONVERT_I32_S]).setDc(prep.dest.reg).setDa(prep.arg0.reg)();
#else
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto fncs = make_array(aux::MappedFncs::F32_CONVERT_I32_S, aux::MappedFncs::F32_CONVERT_I32_U);
      simpleNativeFncCall(prep.dest.reg, false, prep.arg0.reg, false, REG::NONE, false,
                          fncs[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::F32_CONVERT_I32_S)]);
#endif
      return prep.dest.elem;
    }
    case OPCode::F32_CONVERT_I64_S:
    case OPCode::F32_CONVERT_I64_U: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::F32, arg0Ptr, nullptr, targetHint)};

      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto fncs = make_array(aux::MappedFncs::F32_CONVERT_I64_S, aux::MappedFncs::F32_CONVERT_I64_U);
      simpleNativeFncCall(prep.dest.reg, false, prep.arg0.reg, true, REG::NONE, false,
                          fncs[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::F32_CONVERT_I64_S)]);

      return prep.dest.elem;
    }
    case OPCode::F32_DEMOTE_F64: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::F32, arg0Ptr, nullptr, targetHint)};
      as_.INSTR(DFTOF_Dc_Ea).setDc(prep.dest.reg).setEa(prep.arg0.reg)();
      f32NanToCanonical(prep.dest.reg);
      return prep.dest.elem;
    }

    case OPCode::F64_CONVERT_I32_S:
    case OPCode::F64_CONVERT_I32_U: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::F64, arg0Ptr, nullptr, targetHint)};
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(ITODF_Ec_Da, UTODF_Ec_Da);
      as_.INSTR(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::F64_CONVERT_I32_S)]).setEc(prep.dest.reg).setDa(prep.arg0.reg)();

      return prep.dest.elem;
    }
    case OPCode::F64_CONVERT_I64_S:
    case OPCode::F64_CONVERT_I64_U: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::F64, arg0Ptr, nullptr, targetHint)};
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops = make_array(LTODF_Ec_Ea, ULTODF_Ec_Ea);
      as_.INSTR(ops[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::F64_CONVERT_I64_S)]).setEc(prep.dest.reg).setEa(prep.arg0.reg)();

      return prep.dest.elem;
    }
    case OPCode::F64_PROMOTE_F32: {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::F64, arg0Ptr, nullptr, targetHint)};

      as_.INSTR(FTODF_Ec_Da).setEc(prep.dest.reg).setDa(prep.arg0.reg)();
      f64NanToCanonical(prep.dest.reg);

      return prep.dest.elem;
    }

    case OPCode::I32_REINTERPRET_F32:
    case OPCode::I64_REINTERPRET_F64:
    case OPCode::F32_REINTERPRET_I32:
    case OPCode::F64_REINTERPRET_I64: {
      StorageType const storageType{moduleInfo_.getStorage(*arg0Ptr).type};
      if (storageType == StorageType::CONSTANT) {
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
          UNREACHABLE(return StackElement{}, "Unknown OPCode");
        }
          // GCOVR_EXCL_STOP
        }
      } else {
        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto types = make_array(MachineType::I32, MachineType::I64, MachineType::F32, MachineType::F64);

        Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(
            types[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_REINTERPRET_F32)], arg0Ptr, nullptr, targetHint)};

        if (prep.dest.reg != prep.arg0.reg) {
          bool const is64{(opcode == OPCode::I64_REINTERPRET_F64) || (opcode == OPCode::F64_REINTERPRET_I64)};
          if (is64) {
            as_.INSTR(MOV_Ec_Da_Db).setEc(prep.dest.reg).setDa(prep.arg0.secReg).setDb(prep.arg0.reg)();
          } else {
            as_.INSTR(MOV_Da_Db).setDa(prep.dest.reg).setDb(prep.arg0.reg)();
          }
        }

        return prep.dest.elem;
      }
    }

    // GCOVR_EXCL_START
    default: {
      UNREACHABLE(return StackElement{}, "Unknown instruction");
    }
      // GCOVR_EXCL_STOP
    }
  }
}

Backend::I64OperandConstAnalyze const Backend::analyzeImm64OperandConst(StackElement const &arg0, StackElement const &arg1,
                                                                        bool const commutative) VB_NOEXCEPT {
  I64OperandConstAnalyze i64OperandConstAnalyze{};

  SignedInRangeCheck<9U> arg0LowCheck{SignedInRangeCheck<9U>::invalid()};
  SignedInRangeCheck<9U> arg0HighCheck{SignedInRangeCheck<9U>::invalid()};
  SignedInRangeCheck<9U> arg1LowCheck{SignedInRangeCheck<9U>::invalid()};
  SignedInRangeCheck<9U> arg1HighCheck{SignedInRangeCheck<9U>::invalid()};

  if ((arg0.type == StackType::CONSTANT_I64) && commutative) {
    arg0LowCheck = SignedInRangeCheck<9U>::check(bit_cast<int32_t>(static_cast<uint32_t>(arg0.data.constUnion.u64)));
    i64OperandConstAnalyze.arg0LowIsDirectConst = arg0LowCheck.inRange();
    arg0HighCheck = SignedInRangeCheck<9U>::check(bit_cast<int32_t>(static_cast<uint32_t>(arg0.data.constUnion.u64 >> 32LLU)));
    i64OperandConstAnalyze.arg0HighIsDirectConst = arg0HighCheck.inRange();
  }

  if (arg1.type == StackType::CONSTANT_I64) {
    arg1LowCheck = SignedInRangeCheck<9U>::check(bit_cast<int32_t>(static_cast<uint32_t>(arg1.data.constUnion.u64)));
    i64OperandConstAnalyze.arg1LowIsDirectConst = arg1LowCheck.inRange();
    arg1HighCheck = SignedInRangeCheck<9U>::check(bit_cast<int32_t>(static_cast<uint32_t>(arg1.data.constUnion.u64 >> 32LLU)));
    i64OperandConstAnalyze.arg1HighIsDirectConst = arg1HighCheck.inRange();
  }

  i64OperandConstAnalyze.arg0IsDirectConst = i64OperandConstAnalyze.arg0LowIsDirectConst && i64OperandConstAnalyze.arg0HighIsDirectConst;
  i64OperandConstAnalyze.arg1IsDirectConst = i64OperandConstAnalyze.arg1LowIsDirectConst && i64OperandConstAnalyze.arg1HighIsDirectConst;

  if (i64OperandConstAnalyze.arg0IsDirectConst) {
    i64OperandConstAnalyze.immElement = &arg0;
    i64OperandConstAnalyze.regElement = &arg1;
  } else if (i64OperandConstAnalyze.arg1IsDirectConst) {
    i64OperandConstAnalyze.immElement = &arg1;
    i64OperandConstAnalyze.regElement = &arg0;
  } else if (i64OperandConstAnalyze.arg0LowIsDirectConst) {
    i64OperandConstAnalyze.immElement = &arg0;
    i64OperandConstAnalyze.regElement = &arg1;
  } else if (i64OperandConstAnalyze.arg1LowIsDirectConst) {
    i64OperandConstAnalyze.immElement = &arg1;
    i64OperandConstAnalyze.regElement = &arg0;
  } else if (i64OperandConstAnalyze.arg0HighIsDirectConst) {
    i64OperandConstAnalyze.immElement = &arg0;
    i64OperandConstAnalyze.regElement = &arg1;
  } else if (i64OperandConstAnalyze.arg1HighIsDirectConst) {
    i64OperandConstAnalyze.immElement = &arg1;
    i64OperandConstAnalyze.regElement = &arg0;
  } else {
    // pass
  }

  if (i64OperandConstAnalyze.immElement != nullptr) {
    uint64_t const rawValue{i64OperandConstAnalyze.immElement->data.constUnion.u64};
    int32_t const rawLow{bit_cast<int32_t>(static_cast<uint32_t>(rawValue))};
    int32_t const rawHigh{bit_cast<int32_t>(static_cast<uint32_t>(rawValue >> 32LLU))};

    if (i64OperandConstAnalyze.immElement == &arg0) {
      if (arg0LowCheck.inRange()) {
        i64OperandConstAnalyze.rawLow.safeValue = arg0LowCheck.safeInt();
      } else {
        i64OperandConstAnalyze.rawLow.rawValue = rawLow;
      }

      if (arg0HighCheck.inRange()) {
        i64OperandConstAnalyze.rawHigh.safeValue = arg0HighCheck.safeInt();
      } else {
        i64OperandConstAnalyze.rawHigh.rawValue = rawHigh;
      }

    } else if (i64OperandConstAnalyze.immElement == &arg1) {
      if (arg1LowCheck.inRange()) {
        i64OperandConstAnalyze.rawLow.safeValue = arg1LowCheck.safeInt();
      } else {
        i64OperandConstAnalyze.rawLow.rawValue = rawLow;
      }

      if (arg1HighCheck.inRange()) {
        i64OperandConstAnalyze.rawHigh.safeValue = arg1HighCheck.safeInt();
      } else {
        i64OperandConstAnalyze.rawHigh.rawValue = rawHigh;
      }
    } else {
      // pass
    }
  }

  return i64OperandConstAnalyze;
}

Backend::U64OperandConstAnalyze const Backend::analyzeUnsignedImm64OperandConst(StackElement const &arg0, StackElement const &arg1,
                                                                                bool const commutative) VB_NOEXCEPT {
  U64OperandConstAnalyze u64OperandConstAnalyze{};

  UnsignedInRangeCheck<9U> arg0LowCheck{UnsignedInRangeCheck<9U>::invalid()};
  UnsignedInRangeCheck<9U> arg0HighCheck{UnsignedInRangeCheck<9U>::invalid()};
  UnsignedInRangeCheck<9U> arg1LowCheck{UnsignedInRangeCheck<9U>::invalid()};
  UnsignedInRangeCheck<9U> arg1HighCheck{UnsignedInRangeCheck<9U>::invalid()};

  if ((arg0.type == StackType::CONSTANT_I64) && commutative) {
    arg0LowCheck = UnsignedInRangeCheck<9U>::check(static_cast<uint32_t>(arg0.data.constUnion.u64));
    u64OperandConstAnalyze.arg0LowIsDirectConst = arg0LowCheck.inRange();
    arg0HighCheck = UnsignedInRangeCheck<9U>::check(static_cast<uint32_t>(arg0.data.constUnion.u64 >> 32LLU));
    u64OperandConstAnalyze.arg0HighIsDirectConst = arg0HighCheck.inRange();
  }

  if (arg1.type == StackType::CONSTANT_I64) {
    arg1LowCheck = UnsignedInRangeCheck<9U>::check(static_cast<uint32_t>(arg1.data.constUnion.u64));
    u64OperandConstAnalyze.arg1LowIsDirectConst = arg1LowCheck.inRange();
    arg1HighCheck = UnsignedInRangeCheck<9U>::check(static_cast<uint32_t>(arg1.data.constUnion.u64 >> 32LLU));
    u64OperandConstAnalyze.arg1HighIsDirectConst = arg1HighCheck.inRange();
  }

  u64OperandConstAnalyze.arg0IsDirectConst = u64OperandConstAnalyze.arg0LowIsDirectConst && u64OperandConstAnalyze.arg0HighIsDirectConst;
  u64OperandConstAnalyze.arg1IsDirectConst = u64OperandConstAnalyze.arg1LowIsDirectConst && u64OperandConstAnalyze.arg1HighIsDirectConst;

  if (u64OperandConstAnalyze.arg0IsDirectConst) {
    u64OperandConstAnalyze.immElement = &arg0;
    u64OperandConstAnalyze.regElement = &arg1;
  } else if (u64OperandConstAnalyze.arg1IsDirectConst) {
    u64OperandConstAnalyze.immElement = &arg1;
    u64OperandConstAnalyze.regElement = &arg0;
  } else if (u64OperandConstAnalyze.arg0LowIsDirectConst) {
    u64OperandConstAnalyze.immElement = &arg0;
    u64OperandConstAnalyze.regElement = &arg1;
  } else if (u64OperandConstAnalyze.arg1LowIsDirectConst) {
    u64OperandConstAnalyze.immElement = &arg1;
    u64OperandConstAnalyze.regElement = &arg0;
  } else if (u64OperandConstAnalyze.arg0HighIsDirectConst) {
    u64OperandConstAnalyze.immElement = &arg0;
    u64OperandConstAnalyze.regElement = &arg1;
  } else if (u64OperandConstAnalyze.arg1HighIsDirectConst) {
    u64OperandConstAnalyze.immElement = &arg1;
    u64OperandConstAnalyze.regElement = &arg0;
  } else {
    // pass
  }

  if (u64OperandConstAnalyze.immElement != nullptr) {
    uint64_t const rawValue{u64OperandConstAnalyze.immElement->data.constUnion.u64};
    uint32_t const rawLow{static_cast<uint32_t>(rawValue)};
    uint32_t const rawHigh{static_cast<uint32_t>(rawValue >> 32LLU)};

    if (u64OperandConstAnalyze.immElement == &arg0) {
      if (arg0LowCheck.inRange()) {
        u64OperandConstAnalyze.rawLow.safeValue = arg0LowCheck.safeInt();
      } else {
        u64OperandConstAnalyze.rawLow.rawValue = rawLow;
      }

      if (arg0HighCheck.inRange()) {
        u64OperandConstAnalyze.rawHigh.safeValue = arg0HighCheck.safeInt();
      } else {
        u64OperandConstAnalyze.rawHigh.rawValue = rawHigh;
      }

    } else if (u64OperandConstAnalyze.immElement == &arg1) {
      if (arg1LowCheck.inRange()) {
        u64OperandConstAnalyze.rawLow.safeValue = arg1LowCheck.safeInt();
      } else {
        u64OperandConstAnalyze.rawLow.rawValue = rawLow;
      }

      if (arg1HighCheck.inRange()) {
        u64OperandConstAnalyze.rawHigh.safeValue = arg1HighCheck.safeInt();
      } else {
        u64OperandConstAnalyze.rawHigh.rawValue = rawHigh;
      }
    } else {
      // pass
    }
  }

  return u64OperandConstAnalyze;
}

StackElement Backend::emitI64AddImm(StackElement const &arg0, StackElement const &arg1, StackElement const *const targetHint,
                                    bool const commutative) {
  I64OperandConstAnalyze const i64OperandConstAnalyze{analyzeImm64OperandConst(arg0, arg1, commutative)};

  if ((i64OperandConstAnalyze.immElement != nullptr) && (i64OperandConstAnalyze.regElement != nullptr)) {
    Assembler::PreparedArgs const prep{
        as_.loadArgsToRegsAndPrepDest(MachineType::I64, i64OperandConstAnalyze.regElement, nullptr, targetHint, RegMask::none(), true)};

    if (i64OperandConstAnalyze.arg0IsDirectConst || i64OperandConstAnalyze.arg1IsDirectConst) {
      as_.INSTR(ADDX_Dc_Da_const9sx).setDc(prep.dest.reg).setDa(prep.arg0.reg).setConst9sx(i64OperandConstAnalyze.rawLow.safeValue)();
      as_.INSTR(ADDC_Dc_Da_const9sx).setDc(prep.dest.secReg).setDa(prep.arg0.secReg).setConst9sx(i64OperandConstAnalyze.rawHigh.safeValue)();

    } else if (i64OperandConstAnalyze.arg0LowIsDirectConst || i64OperandConstAnalyze.arg1LowIsDirectConst) {
      as_.INSTR(ADDX_Dc_Da_const9sx).setDc(prep.dest.reg).setDa(prep.arg0.reg).setConst9sx(i64OperandConstAnalyze.rawLow.safeValue)();
      emitMoveImpl(VariableStorage::reg(MachineType::I32, prep.dest.secReg),
                   VariableStorage::i32Const(static_cast<uint32_t>(i64OperandConstAnalyze.rawHigh.rawValue)), false);
      as_.INSTR(ADDC_Dc_Da_Db).setDc(prep.dest.secReg).setDa(prep.arg0.secReg).setDb(prep.dest.secReg)();

    } else {
      emitMoveImpl(VariableStorage::reg(MachineType::I32, prep.dest.secReg),
                   VariableStorage::i32Const(static_cast<uint32_t>(i64OperandConstAnalyze.rawLow.rawValue)), false);
      as_.INSTR(ADDX_Dc_Da_Db).setDc(prep.dest.reg).setDa(prep.arg0.reg).setDb(prep.dest.secReg)();
      as_.INSTR(ADDC_Dc_Da_const9sx).setDc(prep.dest.secReg).setDa(prep.arg0.secReg).setConst9sx(i64OperandConstAnalyze.rawHigh.safeValue)();
    }
    return prep.dest.elem;
  } else {
    return StackElement::invalid();
  }
}

StackElement Backend::emitI64AndOrImm(OPCode const opcode, StackElement const &arg0, StackElement const &arg1, StackElement const *const targetHint) {
  U64OperandConstAnalyze const u64OperandConstAnalyze{analyzeUnsignedImm64OperandConst(arg0, arg1, true)};
  // coverity[autosar_cpp14_a8_5_2_violation]
  auto const emitImmInstruction = [this, opcode](REG const Dc, REG const Da, SafeUInt<9> const imm) {
    if (opcode == OPCode::I64_AND) {
      as_.andWordDcDaConst9zx(Dc, Da, imm);
    } else if (opcode == OPCode::I64_OR) {
      as_.orWordDcDaConst9zx(Dc, Da, imm);
    } else {
      as_.INSTR(XOR_Dc_Da_const9zx).setDc(Dc).setDa(Da).setConst9zx(imm)();
    }
  };

  // coverity[autosar_cpp14_a8_5_2_violation]
  constexpr auto regOps = make_array(AND_Dc_Da_Db, OR_Dc_Da_Db, XOR_Dc_Da_Db);
  OPCodeTemplate const regOpcode{regOps[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_AND)]};

  if ((u64OperandConstAnalyze.immElement != nullptr) && (u64OperandConstAnalyze.regElement != nullptr)) {
    Assembler::PreparedArgs const prep{
        as_.loadArgsToRegsAndPrepDest(MachineType::I64, u64OperandConstAnalyze.regElement, nullptr, targetHint, RegMask::none(), true)};

    if (u64OperandConstAnalyze.arg0IsDirectConst || u64OperandConstAnalyze.arg1IsDirectConst) {
      // coverity[autosar_cpp14_a4_5_1_violation]
      emitImmInstruction(prep.dest.reg, prep.arg0.reg, u64OperandConstAnalyze.rawLow.safeValue);
      // coverity[autosar_cpp14_a4_5_1_violation]
      emitImmInstruction(prep.dest.secReg, prep.arg0.secReg, u64OperandConstAnalyze.rawHigh.safeValue);
    } else if (u64OperandConstAnalyze.arg0LowIsDirectConst || u64OperandConstAnalyze.arg1LowIsDirectConst) {
      // coverity[autosar_cpp14_a4_5_1_violation]
      emitImmInstruction(prep.dest.reg, prep.arg0.reg, u64OperandConstAnalyze.rawLow.safeValue);
      emitMoveImpl(VariableStorage::reg(MachineType::I32, prep.dest.secReg), VariableStorage::i32Const(u64OperandConstAnalyze.rawHigh.rawValue),
                   false);
      as_.INSTR(regOpcode).setDc(prep.dest.secReg).setDa(prep.arg0.secReg).setDb(prep.dest.secReg)();
    } else {
      emitMoveImpl(VariableStorage::reg(MachineType::I32, prep.dest.secReg), VariableStorage::i32Const(u64OperandConstAnalyze.rawLow.rawValue),
                   false);
      as_.INSTR(regOpcode).setDc(prep.dest.reg).setDa(prep.arg0.reg).setDb(prep.dest.secReg)();
      // coverity[autosar_cpp14_a4_5_1_violation]
      emitImmInstruction(prep.dest.secReg, prep.arg0.secReg, u64OperandConstAnalyze.rawHigh.safeValue);
    }
    return prep.dest.elem;
  } else {
    return StackElement::invalid();
  }
}

RegElement Backend::emitI64EqImm(OPCode const opcode, StackElement const &arg0, StackElement const &arg1, StackElement const *const targetHint) {
  I64OperandConstAnalyze const i64OperandConstAnalyze{analyzeImm64OperandConst(arg0, arg1, true)};

  if ((i64OperandConstAnalyze.immElement != nullptr) && (i64OperandConstAnalyze.regElement != nullptr)) {
    Assembler::PreparedArgs const prep{
        as_.loadArgsToRegsAndPrepDest(MachineType::I32, i64OperandConstAnalyze.regElement, nullptr, targetHint, RegMask::none(), true)};

    // coverity[autosar_cpp14_a8_5_2_violation]
    constexpr auto ops2Imm = make_array(ANDEQ_Dc_Da_const9sx, ORNE_Dc_Da_const9sx);

    uint32_t const index{static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_EQ)};

    if (i64OperandConstAnalyze.arg0IsDirectConst || i64OperandConstAnalyze.arg1IsDirectConst) {
      OPCodeTemplate const opHighImm{ops2Imm[index]};
      if (opcode == OPCode::I64_EQ) {
        as_.eqWordDcDaConst9sx(prep.dest.reg, prep.arg0.reg, i64OperandConstAnalyze.rawLow.safeValue);
      } else {
        as_.INSTR(NE_Dc_Da_const9sx).setDc(prep.dest.reg).setDa(prep.arg0.reg).setConst9sx(i64OperandConstAnalyze.rawLow.safeValue)();
      }

      as_.INSTR(opHighImm).setDc(prep.dest.reg).setDa(prep.arg0.secReg).setConst9sx(i64OperandConstAnalyze.rawHigh.safeValue)();
    } else if (i64OperandConstAnalyze.arg0LowIsDirectConst || i64OperandConstAnalyze.arg1LowIsDirectConst) {
      OPCodeTemplate const opLowImm{ops2Imm[index]};
      emitMoveImpl(VariableStorage::reg(MachineType::I32, prep.dest.reg),
                   VariableStorage::i32Const(static_cast<uint32_t>(i64OperandConstAnalyze.rawHigh.rawValue)), false);

      if (opcode == OPCode::I64_EQ) {
        as_.eqWordDcDaDb(prep.dest.reg, prep.arg0.secReg, prep.dest.reg);
      } else {
        as_.INSTR(NE_Dc_Da_Db).setDc(prep.dest.reg).setDa(prep.arg0.secReg).setDb(prep.dest.reg)();
      }
      as_.INSTR(opLowImm).setDc(prep.dest.reg).setDa(prep.arg0.reg).setConst9sx(i64OperandConstAnalyze.rawLow.safeValue)();
    } else {
      OPCodeTemplate const opHighImm{ops2Imm[index]};
      emitMoveImpl(VariableStorage::reg(MachineType::I32, prep.dest.reg),
                   VariableStorage::i32Const(static_cast<uint32_t>(i64OperandConstAnalyze.rawLow.rawValue)), false);
      if (opcode == OPCode::I64_EQ) {
        as_.eqWordDcDaDb(prep.dest.reg, prep.arg0.reg, prep.dest.reg);
      } else {
        as_.INSTR(NE_Dc_Da_Db).setDc(prep.dest.reg).setDa(prep.arg0.reg).setDb(prep.dest.reg)();
      }

      as_.INSTR(opHighImm).setDc(prep.dest.reg).setDa(prep.arg0.secReg).setConst9sx(i64OperandConstAnalyze.rawHigh.safeValue)();
    }
    return {prep.dest.elem, prep.dest.reg};
  } else {
    return {StackElement::invalid(), REG::NONE};
  }
}

void Backend::simpleNativeFncCall(REG const destReg, bool const destIs64, REG const arg0Reg, bool const arg0Is64, REG const arg1Reg,
                                  bool const arg1Is64, aux::MappedFncs const mappedFnc) const {
  assert((arg1Reg != REG::NONE || !arg1Is64) && "REG::NONE cannot be 64b");
  constexpr int32_t extraForSPAlignmentDiff{8};
  SafeInt<5U> const extraForDest{(destReg == REG::NONE) ? SafeInt<5U>::fromConst<0>() : SafeInt<5U>::fromConst<8>()};
  SafeInt<10U> const unalignedIncreaseStackSize{
      SafeInt<9U>::fromConst<(1 * static_cast<int32_t>(NativeABI::contextRegisterSize)) + extraForSPAlignmentDiff>() + extraForDest};

  as_.INSTR(LEA_Aa_deref_Ab_off16sx).setAa(REG::SP).setAb(REG::SP).setOff16sx(static_cast<SafeInt<16U>>(-unalignedIncreaseStackSize))();

  // Align stack pointer to 16-word boundary for STLCX/STUCX/LDLCX/LDUCX
  constexpr REG helperReg{REG::D15};
  as_.INSTR(MOVA_Aa_Db).setAa(WasmABI::REGS::addrScrReg[0]).setDb(helperReg)();
  as_.INSTR(MOVD_Da_Ab).setDa(helperReg).setAb(REG::SP)();
  as_.andWordDcDaConst9zx(helperReg, helperReg, SafeUInt<9U>::fromConst<0b11'1111U>());
  as_.INSTR(MOVA_Aa_Db).setAa(WasmABI::REGS::addrScrReg[1]).setDb(helperReg)();
  as_.INSTR(SUBA_Ac_Aa_Ab).setAc(REG::SP).setAa(REG::SP).setAb(WasmABI::REGS::addrScrReg[1])();

  uint32_t const maxIncreaseStackSize{static_cast<uint32_t>(unalignedIncreaseStackSize.value()) + 0b11'1111U}; // max alignment is 0b11'1111U
  if (moduleInfo_.currentState.checkedStackFrameSize < (moduleInfo_.fnc.stackFrameSize + maxIncreaseStackSize)) {
    moduleInfo_.currentState.checkedStackFrameSize = moduleInfo_.fnc.stackFrameSize + static_cast<uint32_t>(unalignedIncreaseStackSize.value());
    as_.checkStackFence(helperReg, WasmABI::REGS::addrScrReg[2]);
  }
  as_.INSTR(MOVD_Da_Ab).setDa(helperReg).setAb(WasmABI::REGS::addrScrReg[0])();

  // Store context
  as_.INSTR(STLCX_Ab_off10sx).setAb(REG::SP).setOff10sx(SafeInt<10>::fromConst<0>())();

  // Load arguments
  constexpr REG arg0CC{REG::D4};
  REG const arg1CC{(arg0Is64 || arg1Is64) ? REG::D6 : REG::D5};
  bool const arg1OverlapsArg0CC{(arg1Reg != REG::NONE) && (((arg1Reg == arg0CC) || (arg1Is64 && (RegUtil::getOtherExtReg(arg1Reg) == arg0CC))) ||
                                                           (arg0Is64 && (arg1Reg == RegUtil::getOtherExtReg(arg0CC))))};

  // Temporarily save arg1 in address register(s)
  if (arg1OverlapsArg0CC) {
    as_.INSTR(MOVA_Aa_Db).setAa(REG::A4).setDb(arg1Reg)();
    if (arg1Is64) {
      as_.INSTR(MOVA_Aa_Db).setAa(REG::A5).setDb(RegUtil::getOtherExtReg(arg1Reg))();
    }
  }

  if ((arg0Reg != REG::NONE) && (arg0Reg != arg0CC)) {
    if (!arg0Is64) {
      as_.INSTR(MOV_Da_Db).setDa(arg0CC).setDb(arg0Reg)();
    } else {
      as_.INSTR(MOV_Ec_Da_Db).setEc(arg0CC).setDa(RegUtil::getOtherExtReg(arg0Reg)).setDb(arg0Reg)();
    }
  }

  assert((arg1Reg != arg1CC || !arg1OverlapsArg0CC) && "Cannot overlap while already being correct");
  if ((arg1Reg != REG::NONE) && (arg1Reg != arg1CC)) {
    if (!arg1OverlapsArg0CC) {
      if (!arg1Is64) {
        as_.INSTR(MOV_Da_Db).setDa(arg1CC).setDb(arg1Reg)();
      } else {
        as_.INSTR(MOV_Ec_Da_Db).setEc(arg1CC).setDa(RegUtil::getOtherExtReg(arg1Reg)).setDb(arg1Reg)();
      }
    } else {
      as_.INSTR(MOVD_Da_Ab).setDa(arg1CC).setAb(REG::A4)();
      if (arg1Is64) {
        as_.INSTR(MOVD_Da_Ab).setDa(RegUtil::getOtherExtReg(arg1CC)).setAb(REG::A5)();
      }
    }
  }

  // Call function
#if TC_LINK_AUX_FNCS_DYNAMICALLY
  SafeUInt<8U> const mappedFncIdx{SafeUInt<8U>::fromAny(static_cast<uint8_t>(mappedFnc))};
  as_.INSTR(LDA_Aa_deref_Ab_off16sx)
      .setAa(WasmABI::REGS::addrScrReg[0])
      .setAb(WasmABI::REGS::linMem)
      .setOff16sx(SafeInt<16U>::fromConst<-BD::FromEnd::arrDynSimpleFncCallsPtr>())();
  SafeUInt<10U> const functionByteOffset{mappedFncIdx.leftShift<2U>()};
  as_.emitLoadDerefOff16sx(WasmABI::REGS::addrScrReg[0], WasmABI::REGS::addrScrReg[0], static_cast<SafeInt<16U>>(functionByteOffset));
  as_.INSTR(CALLI_Aa).setAa(WasmABI::REGS::addrScrReg[0])();
#else
  uint32_t const rawAddr = getSoftfloatImplementationFunctionPtr(mappedFnc);
  if (Instruction::fitsAbsDisp24sx2(rawAddr)) {
    as_.INSTR(CALLA_absdisp24sx2).setAbsDisp24sx2(rawAddr)();
  } else {
    as_.MOVimm(WasmABI::REGS::addrScrReg[0], rawAddr);
    as_.INSTR(CALLI_Aa).setAa(WasmABI::REGS::addrScrReg[0])();
  }
#endif

  // Save return value
  if (destReg != REG::NONE) {
    if (destIs64) {
      as_.INSTR(STD_deref_Ab_off10sx_Ea)
          .setAb(REG::SP)
          .setOff10sx(SafeInt<10>::fromConst<1 * static_cast<int32_t>(NativeABI::contextRegisterSize)>())
          .setEa(REG::D2)();
    } else {
      as_.storeWordDerefARegDisp16sxDReg(REG::D2, REG::SP, SafeInt<16U>::fromConst<1 * static_cast<int32_t>(NativeABI::contextRegisterSize)>());
    }
  }

  // Restore context
  as_.INSTR(LDLCX_Ab_off10sx).setAb(REG::SP).setOff10sx(SafeInt<10>::fromConst<0>())();

  // Restore return value to correct register
  if (destReg != REG::NONE) {
    if (destIs64) {
      as_.INSTR(LDD_Ea_deref_Ab_off10sx)
          .setEa(destReg)
          .setAb(REG::SP)
          .setOff10sx(SafeInt<10>::fromConst<1 * static_cast<int32_t>(NativeABI::contextRegisterSize)>())();
    } else {
      as_.loadWordDRegDerefARegDisp16sx(destReg, REG::SP, SafeInt<16U>::fromConst<1 * static_cast<int32_t>(NativeABI::contextRegisterSize)>());
    }
  }

  as_.INSTR(ADDA_Aa_Ab).setAa(REG::SP).setAb(WasmABI::REGS::addrScrReg[1])();
  as_.INSTR(LEA_Aa_deref_Ab_off16sx).setAa(REG::SP).setAb(REG::SP).setOff16sx(static_cast<SafeInt<16U>>(unalignedIncreaseStackSize))();
}

RegElement Backend::emitComparisonImpl(OPCode const opcode, StackElement const *const arg0Ptr, StackElement const *const arg1Ptr,
                                       StackElement const *const targetHint) {
  moduleInfo_.lastBC = BCforOPCode(opcode);
  switch (opcode) {
  case OPCode::I32_EQZ: {
    Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I32, arg0Ptr, nullptr, targetHint)};
    as_.eqWordDcDaConst9sx(prep.dest.reg, prep.arg0.reg, SafeInt<9>::fromConst<0>());
    return {prep.dest.elem, prep.dest.reg};
  }
  case OPCode::I32_EQ:
  case OPCode::I32_NE: {
    SignedInRangeCheck<9U> const arg0IsDirectConst{checkStackElemSignedConstInRange<9U>(*arg0Ptr)};
    SignedInRangeCheck<9U> const arg1IsDirectConst{checkStackElemSignedConstInRange<9U>(*arg1Ptr)};

    if (arg0IsDirectConst.inRange() || arg1IsDirectConst.inRange()) {
      StackElement const *const regElement{arg0IsDirectConst.inRange() ? arg1Ptr : arg0Ptr};
      SafeInt<9> const immValue{arg0IsDirectConst.inRange() ? arg0IsDirectConst.safeInt() : arg1IsDirectConst.safeInt()};

      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I32, regElement, nullptr, targetHint)};
      if (opcode == OPCode::I32_EQ) {
        as_.eqWordDcDaConst9sx(prep.dest.reg, prep.arg0.reg, immValue);
      } else {
        as_.INSTR(NE_Dc_Da_const9sx).setDc(prep.dest.reg).setDa(prep.arg0.reg).setConst9sx(immValue)();
      }

      return {prep.dest.elem, prep.dest.reg};
    } else {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I32, arg0Ptr, arg1Ptr, targetHint)};
      if (opcode == OPCode::I32_EQ) {
        as_.eqWordDcDaDb(prep.dest.reg, prep.arg0.reg, prep.arg1.reg);
      } else {
        as_.INSTR(NE_Dc_Da_Db).setDc(prep.dest.reg).setDa(prep.arg0.reg).setDb(prep.arg1.reg)();
      }
      return {prep.dest.elem, prep.dest.reg};
    }
  }

  case OPCode::I32_LT_S:
  case OPCode::I32_LT_U:
  case OPCode::I32_GT_S:
  case OPCode::I32_GT_U:
  case OPCode::I32_LE_S:
  case OPCode::I32_LE_U:
  case OPCode::I32_GE_S:
  case OPCode::I32_GE_U: {
    bool const reversed{(opcode >= OPCode::I32_GT_S) && (opcode <= OPCode::I32_LE_U)};
    StackElement const *const firstArg{reversed ? arg1Ptr : arg0Ptr};
    StackElement const *const secondArg{reversed ? arg0Ptr : arg1Ptr};

    bool const isSigned{(static_cast<uint32_t>(opcode) & 0b1U) == 0U};
    // coverity[autosar_cpp14_a8_5_2_violation]
    constexpr auto constInRangeOps = make_array(LT_Dc_Da_const9sx, LTU_Dc_Da_const9zx, LT_Dc_Da_const9sx, LTU_Dc_Da_const9zx, GE_Dc_Da_const9sx,
                                                GEU_Dc_Da_const9zx, GE_Dc_Da_const9sx, GEU_Dc_Da_const9zx);
    if (isSigned) {
      SignedInRangeCheck<9> const secondArgIsDirectConstant{checkStackElemSignedConstInRange<9>(*secondArg)};
      if (secondArgIsDirectConstant.inRange()) {
        Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I32, firstArg, nullptr, targetHint)};
        OPCodeTemplate const instruction{constInRangeOps[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_LT_S)]};
        if (instruction == LT_Dc_Da_const9sx) {
          as_.ltWordDcDaConst9sx(prep.dest.reg, prep.arg0.reg, secondArgIsDirectConstant.safeInt());
        } else {
          as_.INSTR(instruction).setDc(prep.dest.reg).setDa(prep.arg0.reg).setConst9sx(secondArgIsDirectConstant.safeInt())();
        }

        return {prep.dest.elem, prep.dest.reg};
      }
    } else {
      UnsignedInRangeCheck<9> const secondArgIsDirectConstant{checkStackElemUnsignedConstInRange<9>(*secondArg)};
      if (secondArgIsDirectConstant.inRange()) {
        Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I32, firstArg, nullptr, targetHint)};
        as_.INSTR(constInRangeOps[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_LT_S)])
            .setDc(prep.dest.reg)
            .setDa(prep.arg0.reg)
            .setConst9zx(secondArgIsDirectConstant.safeInt())();
        return {prep.dest.elem, prep.dest.reg};
      }
    }
    Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I32, firstArg, secondArg, targetHint)};
    // coverity[autosar_cpp14_a8_5_2_violation]
    constexpr auto regOps = make_array(LT_Dc_Da_Db, LTU_Dc_Da_Db, LT_Dc_Da_Db, LTU_Dc_Da_Db, GE_Dc_Da_Db, GEU_Dc_Da_Db, GE_Dc_Da_Db, GEU_Dc_Da_Db);
    OPCodeTemplate const instruction{regOps[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_LT_S)]};
    if (instruction == LT_Dc_Da_Db) {
      as_.ltWordDcDaDb(prep.dest.reg, prep.arg0.reg, prep.arg1.reg);
    } else {
      as_.INSTR(instruction).setDc(prep.dest.reg).setDa(prep.arg0.reg).setDb(prep.arg1.reg)();
    }
    return {prep.dest.elem, prep.dest.reg};
  }
  case OPCode::I64_EQZ: {
    Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I32, arg0Ptr, nullptr, targetHint)};
    as_.INSTR(OR_Dc_Da_Db).setDc(prep.dest.reg).setDa(prep.arg0.reg).setDb(prep.arg0.secReg)();
    as_.eqWordDcDaConst9sx(prep.dest.reg, prep.dest.reg, SafeInt<9>::fromConst<0>());
    return {prep.dest.elem, prep.dest.reg};
  }
  case OPCode::I64_EQ:
  case OPCode::I64_NE: {
    RegElement const regElemImm{emitI64EqImm(opcode, *arg0Ptr, *arg1Ptr, targetHint)};

    if (regElemImm.elem.type != StackType::INVALID) {
      return regElemImm;
    } else {
      Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I32, arg0Ptr, arg1Ptr, targetHint, RegMask::none(), true, true)};
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto ops2 = make_array(ANDEQ_Dc_Da_Db, ORNE_Dc_Da_Db);
      if (opcode == OPCode::I64_EQ) {
        as_.eqWordDcDaDb(prep.dest.reg, prep.arg0.reg, prep.arg1.reg);
      } else {
        as_.INSTR(NE_Dc_Da_Db).setDc(prep.dest.reg).setDa(prep.arg0.reg).setDb(prep.arg1.reg)();
      }

      as_.INSTR(ops2[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_EQ)])
          .setDc(prep.dest.reg)
          .setDa(prep.arg0.secReg)
          .setDb(prep.arg1.secReg)();
      return {prep.dest.elem, prep.dest.reg};
    }
  }
  case OPCode::I64_LT_S: // 0 0
  case OPCode::I64_LT_U: // 0 0
  case OPCode::I64_GT_S: // 1 1
  case OPCode::I64_GT_U: // 1 1
  case OPCode::I64_LE_S: // 1 0
  case OPCode::I64_LE_U: // 1 0
  case OPCode::I64_GE_S: // 0 1
  case OPCode::I64_GE_U: // 0 1
  {
    // auto ops1 = make_array(ANDLTU_Dc_Da_const9zx, ANDLTU_Dc_Da_const9zx, ANDLTU_Dc_Da_const9zx,
    // ANDLTU_Dc_Da_const9zx, ANDGEU_Dc_Da_const9zx, ANDGEU_Dc_Da_const9zx,
    //                                          ANDGEU_Dc_Da_const9zx, ANDGEU_Dc_Da_const9zx);
    // auto ops2 = make_array(ORLT_Dc_Da_const9sx, ORLTU_Dc_Da_const9zx, ORLT_Dc_Da_const9sx, ORLTU_Dc_Da_const9zx,
    // ORLT_Dc_Da_const9sx, ORLTU_Dc_Da_const9zx, ORLT_Dc_Da_const9sx,
    //                                          ORLTU_Dc_Da_const9zx);

    Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I32, arg0Ptr, arg1Ptr, targetHint, RegMask::none(), true, true)};

    // coverity[autosar_cpp14_a8_5_2_violation]
    auto ops1 = make_array(ANDLTU_Dc_Da_Db, ANDLTU_Dc_Da_Db, ANDLTU_Dc_Da_Db, ANDLTU_Dc_Da_Db, ANDGEU_Dc_Da_Db, ANDGEU_Dc_Da_Db, ANDGEU_Dc_Da_Db,
                           ANDGEU_Dc_Da_Db);
    // coverity[autosar_cpp14_a8_5_2_violation]
    auto ops2 =
        make_array(ORLT_Dc_Da_Db, ORLTU_Dc_Da_Db, ORLT_Dc_Da_Db, ORLTU_Dc_Da_Db, ORLT_Dc_Da_Db, ORLTU_Dc_Da_Db, ORLT_Dc_Da_Db, ORLTU_Dc_Da_Db);

    // coverity[autosar_cpp14_a8_5_2_violation]
    constexpr auto reversedOps1 = make_array(false, false, true, true, true, true, false, false);
    // coverity[autosar_cpp14_a8_5_2_violation]
    constexpr auto reversedOps2 = make_array(false, false, true, true, false, false, true, true);

    bool const op1reversed{reversedOps1[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_LT_S)]};
    bool const op2reversed{reversedOps2[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_LT_S)]};

    as_.eqWordDcDaDb(prep.dest.reg, prep.arg0.secReg, prep.arg1.secReg);
    as_.INSTR(ops1[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_LT_S)])
        .setDc(prep.dest.reg)
        .setDa(op1reversed ? prep.arg1.reg : prep.arg0.reg)
        .setDb(op1reversed ? prep.arg0.reg : prep.arg1.reg)();
    as_.INSTR(ops2[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I64_LT_S)])
        .setDc(prep.dest.reg)
        .setDa(op2reversed ? prep.arg1.secReg : prep.arg0.secReg)
        .setDb(op2reversed ? prep.arg0.secReg : prep.arg1.secReg)();
    return {prep.dest.elem, prep.dest.reg};
  }
  case OPCode::F32_EQ: // EQ
  case OPCode::F32_NE: // LT, GT or UNORD
  case OPCode::F32_LT: // LT
  case OPCode::F32_GT: // GT
  case OPCode::F32_LE: // LT or EQ
  case OPCode::F32_GE: // GT or EQ
  case OPCode::F64_EQ:
  case OPCode::F64_NE:
  case OPCode::F64_LT:
  case OPCode::F64_GT:
  case OPCode::F64_LE:
  case OPCode::F64_GE: {
    bool const isf32{opcode <= OPCode::F32_GE};
    // coverity[autosar_cpp14_a8_5_2_violation]
    constexpr auto cmpFlags =
        make_array(SafeUInt<9U>::fromConst<static_cast<uint32_t>(CMPFFLAGS::EQ)>(),
                   SafeUInt<9U>::fromConst<static_cast<uint32_t>(CMPFFLAGS::LT) | static_cast<uint32_t>(CMPFFLAGS::GT) |
                                           static_cast<uint32_t>(CMPFFLAGS::UNORD)>(),
                   SafeUInt<9U>::fromConst<static_cast<uint32_t>(CMPFFLAGS::LT)>(), SafeUInt<9U>::fromConst<static_cast<uint32_t>(CMPFFLAGS::GT)>(),
                   SafeUInt<9U>::fromConst<static_cast<uint32_t>(CMPFFLAGS::LT) | static_cast<uint32_t>(CMPFFLAGS::EQ)>(),
                   SafeUInt<9U>::fromConst<static_cast<uint32_t>(CMPFFLAGS::GT) | static_cast<uint32_t>(CMPFFLAGS::EQ)>());

    Assembler::PreparedArgs const prep{as_.loadArgsToRegsAndPrepDest(MachineType::I32, arg0Ptr, arg1Ptr, targetHint)};
    if (isf32) {
      as_.INSTR(CMPF_Dc_Da_Db).setDc(prep.dest.reg).setDa(prep.arg0.reg).setDb(prep.arg1.reg)();

    } else {
      as_.INSTR(CMPDF_Dc_Ea_Eb).setDc(prep.dest.reg).setEa(prep.arg0.reg).setEb(prep.arg1.reg)();
    }
    SafeUInt<9> const immCond{cmpFlags[(static_cast<size_t>(opcode) - static_cast<size_t>(OPCode::F32_EQ)) % cmpFlags.size()]};
    as_.andWordDcDaConst9zx(prep.dest.reg, prep.dest.reg, immCond);
    as_.INSTR(NE_Dc_Da_const9sx).setDc(prep.dest.reg).setDa(prep.dest.reg).setConst9sx(SafeInt<9>::fromConst<0>())();
    return {prep.dest.elem, prep.dest.reg};
  }
  // GCOVR_EXCL_START
  default: {
    UNREACHABLE(return RegElement{}, "Unknown OPCode");
  }
    // GCOVR_EXCL_STOP
  }
}

void Backend::emitCMPF64(REG const target, REG const arg0, REG const arg1) {
  as_.INSTR(CMPDF_Dc_Ea_Eb).setDc(target).setEa(arg0).setEb(arg1)();
}

void Backend::emitCMPF32(REG const target, REG const arg0, REG const arg1) {
  as_.INSTR(CMPF_Dc_Da_Db).setDc(target).setDa(arg0).setDb(arg1)();
}

bool Backend::emitComparison(OPCode const opcode, StackElement const *const arg0Ptr, StackElement const *const arg1Ptr) {
  RegElement const resultRegElem{emitComparisonImpl(opcode, arg0Ptr, arg1Ptr)};
  as_.INSTR(MOVA_Aa_Db).setAa(WasmABI::REGS::cmpRes).setDb(resultRegElem.reg)();
  return false; // Never reversed on TriCore
}

void Backend::emitBranch(StackElement *const targetBlockElem, BC const branchCond, bool const isNegative) {
  assert(((moduleInfo_.lastBC == branchCond) || (moduleInfo_.lastBC == negateBC(branchCond)) || (branchCond == BC::UNCONDITIONAL)) &&
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

  BC const effectiveBC{isNegative ? negateBC(branchCond) : branchCond};
  BC const negEffBC{negateBC(effectiveBC)};

  OPCodeTemplate const negEffJmpInstr{(negEffBC == moduleInfo_.lastBC) ? JNZA_Aa_disp15sx2 : JZA_Aa_disp15sx2};

  if (targetBlockElem != nullptr) {
    // Targeting a block, loop or ifblock
    if (branchCond == BC::UNCONDITIONAL) {
      as_.setStackFrameSize(targetBlockElem->data.blockInfo.entryStackFrameSize, true);
      RelPatchObj const branchObj{as_.INSTR(J_disp24sx2).prepJmp()};

      linkBranchToBlock(branchObj, targetBlockElem);
    } else {
      RelPatchObj const conditionRelPatchObj{as_.INSTR(negEffJmpInstr).setAa(WasmABI::REGS::cmpRes).prepJmp()};
      as_.setStackFrameSize(targetBlockElem->data.blockInfo.entryStackFrameSize, true);
      RelPatchObj const branchObj{as_.INSTR(J_disp24sx2).prepJmp()};
      conditionRelPatchObj.linkToHere();

      linkBranchToBlock(branchObj, targetBlockElem);
    }
  } else {
    // Targeting the function
    if (branchCond == BC::UNCONDITIONAL) {
      emitReturnAndUnwindStack(true);
    } else {
      // Negated condition -> jump over
      RelPatchObj const relPatchObj{as_.INSTR(negEffJmpInstr).setAa(WasmABI::REGS::cmpRes).prepJmp()};
      emitReturnAndUnwindStack(true);
      relPatchObj.linkToHere();
    }
  }
}

void Backend::emitReturnAndUnwindStack(bool const temporary) {
  // No stack fence check needed because it will always make the stack frame smaller
  as_.setStackFrameSize(moduleInfo_.fnc.paramWidth + returnAddrWidth, temporary, true);
  as_.INSTR(FRET)();
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
  assert(newOffset <= moduleInfo_.fnc.stackFrameSize + StackElement::tempStackSlotSize);
  if (newOffset > moduleInfo_.fnc.stackFrameSize) {
    uint32_t const newAlignedStackFrameSize{as_.alignStackFrameSize(newOffset + 32U)};
    as_.setStackFrameSize(newAlignedStackFrameSize);
    if (moduleInfo_.currentState.checkedStackFrameSize < newAlignedStackFrameSize) {
      moduleInfo_.currentState.checkedStackFrameSize = newAlignedStackFrameSize;
      as_.INSTR(MOVA_Aa_Db).setAa(WasmABI::REGS::addrScrReg[1]).setDb(REG::D0)();
      as_.checkStackFence(REG::D0, WasmABI::REGS::addrScrReg[0]); // SP change
      as_.INSTR(MOVD_Da_Ab).setDa(REG::D0).setAb(WasmABI::REGS::addrScrReg[1])();
    }
  }

  StackElement const tempStackElement{
      StackElement::tempResult(type, VariableStorage::stackMemory(type, newOffset), moduleInfo_.getStackMemoryReferencePosition())};
  return tempStackElement;
}

void Backend::spillAllVariables(Stack::iterator const below) {
  for (uint32_t i{0U}; i < moduleInfo_.fnc.numLocals; i++) {
    spillFromStack(StackElement::local(i), RegMask::none(), true, false, below);
  }
  // coverity[autosar_cpp14_a8_5_2_violation]
  auto const spillFromStackCallback = [this, below](StackElement const &element) {
    spillFromStack(element, RegMask::none(), true, false, below);
  };
  // coverity[autosar_cpp14_a5_1_4_violation]
  iterateScratchRegsAndGlobals(FunctionRef<void(StackElement const &)>(spillFromStackCallback));
}

void Backend::iterateScratchRegsAndGlobals(FunctionRef<void(StackElement const &)> const &lambda) const {
  for (uint32_t regPos{getNumStaticallyAllocatedDr()}; regPos < WasmABI::dr.size(); regPos++) {
    REG const reg{WasmABI::dr[regPos]};
    Stack::iterator const refToLastOccurrence{moduleInfo_.getReferenceToLastOccurrenceOnStack(reg)};
    if (!refToLastOccurrence.isEmpty()) {
      MachineType const type{moduleInfo_.getMachineType(refToLastOccurrence.raw())};
      lambda(StackElement::scratchReg(reg, MachineTypeUtil::toStackTypeFlag(type) | StackType::SCRATCHREGISTER));
    }
  }
  for (uint32_t globalIdx{0U}; globalIdx < moduleInfo_.numNonImportedGlobals; globalIdx++) {
    lambda(StackElement::global(globalIdx));
  }
}

uint32_t Backend::reserveStackFrame(uint32_t const width) {
  uint32_t const newOffset{common_.getCurrentMaximumUsedStackFramePosition() + width};
  assert(newOffset <= moduleInfo_.fnc.stackFrameSize + width);
  if (newOffset > moduleInfo_.fnc.stackFrameSize) {
    uint32_t const newAlignedStackFrameSize{as_.alignStackFrameSize(newOffset + 32U)};
    as_.setStackFrameSize(newAlignedStackFrameSize);
    if (moduleInfo_.currentState.checkedStackFrameSize < newAlignedStackFrameSize) {
      moduleInfo_.currentState.checkedStackFrameSize = newAlignedStackFrameSize;
      as_.INSTR(MOVA_Aa_Db).setAa(WasmABI::REGS::addrScrReg[1]).setDb(REG::D0)();
      as_.checkStackFence(REG::D0, WasmABI::REGS::addrScrReg[0]); // SP change
      as_.INSTR(MOVD_Da_Ab).setDa(REG::D0).setAb(WasmABI::REGS::addrScrReg[1])();
    }
  }
  return newOffset;
}

#if INTERRUPTION_REQUEST
void Backend::checkForInterruptionRequest(REG const scrReg) const {
  as_.loadByteUnsignedDRegDerefARegDisp16sx(scrReg, WasmABI::REGS::linMem, SafeInt<16U>::fromConst<-BD::FromEnd::statusFlags>());

  RelPatchObj const notTriggered{as_.INSTR(JEQ_Da_const4sx_disp15sx2).setDa(scrReg).setConst4sx(SafeInt<4U>::fromConst<0>()).prepJmp()};
  // Retrieve the trapCode from the actual flag
  if (scrReg != WasmABI::REGS::trapReg) {
    as_.INSTR(MOV_Da_Db).setDa(WasmABI::REGS::trapReg).setDb(scrReg)();
  }
  as_.TRAP(TrapCode::NONE);
  notTriggered.linkToHere();
}
#endif

void Backend::f64NanToCanonical(REG const distReg) {
  as_.INSTR(MINDF_Ec_Ea_Eb).setEc(distReg).setEa(distReg).setEb(distReg)();
}

void Backend::f32NanToCanonical(REG const distReg) {
  as_.INSTR(MINF_Dc_Da_Db).setDc(distReg).setDa(distReg).setDb(distReg)();
}

void Backend::execPadding(uint32_t const paddingSize) {
  assert(paddingSize == 0U || paddingSize == 2U);
  if (paddingSize == 2U) {
    as_.INSTR(NOP)();
  }
}

uint32_t Backend::getParamPos(REG const reg, bool const import) const VB_NOEXCEPT {
  if (import) {
    return NativeABI::getNativeParamPos(reg);
  } else {
    uint32_t const pos{WasmABI::getRegPos(reg) - moduleInfo_.getLocalStartIndexInGPRs()};
    if (pos < WasmABI::regsForParams) {
      return pos;
    } else {
      return static_cast<uint32_t>(UINT8_MAX);
    }
  }
}

void Backend::swapReg(REG const reg1, REG const reg2) {
  as_.INSTR(XOR_Da_Db).setDa(reg1).setDb(reg2)();
  as_.INSTR(XOR_Da_Db).setDa(reg2).setDb(reg1)();
  as_.INSTR(XOR_Da_Db).setDa(reg1).setDb(reg2)();
}

REG Backend::getUnderlyingRegIfSuitable(StackElement const *const element, MachineType const dstMachineType,
                                        RegMask const regMask) const VB_NOEXCEPT {
  if (element == nullptr) {
    return REG::NONE;
  }
  VariableStorage const targetHintStorage{moduleInfo_.getStorage(*element)};
  if (targetHintStorage.type != StorageType::REGISTER) {
    return REG::NONE;
  }
  if (regMask.contains(targetHintStorage.location.reg) ||
      (MachineTypeUtil::is64(dstMachineType) && regMask.contains(RegUtil::getOtherExtReg(targetHintStorage.location.reg)))) {
    return REG::NONE;
  }
  // tricore ISA put all data in DR, no need to distinguish data types.
  bool const isContainable{MachineTypeUtil::getSize(dstMachineType) <= MachineTypeUtil::getSize(targetHintStorage.machineType)};
  return isContainable ? targetHintStorage.location.reg : REG::NONE;
}

bool Backend::hasEnoughScratchRegForScheduleInstruction(OPCode const opcode) const VB_NOEXCEPT {
  bool const isDiv32{opcodeIsDivInt32(opcode)};
  bool const isLoad32{opcodeIsLoad32(opcode)};

  uint32_t const numStaticallyAllocatedRegs{getNumStaticallyAllocatedDr()};
  constexpr uint32_t numTotalRegs{static_cast<uint32_t>(WasmABI::dr.size())};
  uint32_t availableRegsCount{0U};
  if (isDiv32 || isLoad32) {
    for (uint32_t regPos{numStaticallyAllocatedRegs}; regPos < numTotalRegs; regPos++) {
      REG const currentReg{WasmABI::dr[regPos]};

      bool const canBeExtendedReg{RegUtil::canBeExtReg(currentReg)};
      Stack::iterator const refToLastOccurrence{moduleInfo_.getReferenceToLastOccurrenceOnStack(currentReg)};
      REG const otherReg{RegUtil::getOtherExtReg(currentReg)};
      bool const empty{refToLastOccurrence.isEmpty()};
      bool otherIsEmptyOrLocalOr32b{true};

      if ((!canBeExtendedReg) && (!isStaticallyAllocatedReg(otherReg))) {
        Stack::iterator const otherRefToLastOccurrence{moduleInfo_.getReferenceToLastOccurrenceOnStack(otherReg)};

        if ((!otherRefToLastOccurrence.isEmpty()) && ((otherRefToLastOccurrence->type == StackType::SCRATCHREGISTER_I64) ||
                                                      (otherRefToLastOccurrence->type == StackType::SCRATCHREGISTER_F64))) {
          otherIsEmptyOrLocalOr32b = false;
        }
      }

      if (empty && otherIsEmptyOrLocalOr32b) {
        availableRegsCount++;
      }
    }
  } else {
    for (uint32_t regPos{numStaticallyAllocatedRegs}; regPos < numTotalRegs; regPos++) {
      REG const currentReg{WasmABI::dr[regPos]};

      bool const canBeExtendedReg{RegUtil::canBeExtReg(currentReg)};
      if (!canBeExtendedReg) {
        continue;
      }

      REG const currentSecReg{RegUtil::getOtherExtReg(currentReg)};
      assert((currentSecReg == WasmABI::dr[regPos + 1U]) && "Primary and secondary reg not in order");

      Stack::iterator const refToLastOccurrence{moduleInfo_.getReferenceToLastOccurrenceOnStack(currentReg)};
      Stack::iterator const secRefToLastOccurrence{moduleInfo_.getReferenceToLastOccurrenceOnStack(currentSecReg)};
      if (refToLastOccurrence.isEmpty() && secRefToLastOccurrence.isEmpty()) {
        availableRegsCount++;
      }
    }
  }
  return availableRegsCount > minimalNumRegsReservedForCondense;
}

} // namespace tc
} // namespace vb

#endif
