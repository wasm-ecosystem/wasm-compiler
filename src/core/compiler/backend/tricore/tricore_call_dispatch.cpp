///
/// @file tricore_call_dispatch.cpp
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
#include <cstdint>

#include "src/core/common/FunctionRef.hpp"
#include "src/core/common/basedataoffsets.hpp"
#include "src/core/compiler/backend/tricore/tricore_assembler.hpp"
#include "src/core/compiler/backend/tricore/tricore_backend.hpp"
#include "src/core/compiler/backend/tricore/tricore_call_dispatch.hpp"
#include "src/core/compiler/backend/tricore/tricore_cc.hpp"
#include "src/core/compiler/backend/tricore/tricore_encoding.hpp"
#include "src/core/compiler/backend/tricore/tricore_instruction.hpp"
#include "src/core/compiler/common/Common.hpp"
#include "src/core/compiler/common/ListIterator.hpp"
#include "src/core/compiler/common/MachineType.hpp"
#include "src/core/compiler/common/ModuleInfo.hpp"
#include "src/core/compiler/common/RegMask.hpp"
#include "src/core/compiler/common/RegisterCopyResolver.hpp"
#include "src/core/compiler/common/SafeInt.hpp"
#include "src/core/compiler/common/Stack.hpp"
#include "src/core/compiler/common/StackElement.hpp"
#include "src/core/compiler/common/StackType.hpp"
#include "src/core/compiler/common/VariableStorage.hpp"

namespace vb {
namespace tc {

void CallBase::emitFncCallWrapper(uint32_t const fncIndex, FunctionRef<void()> const &emitFunctionCallLambda) {
  backend_.tryPushStacktraceEntry(fncIndex, of_stacktraceRecord_, WasmABI::REGS::addrScrReg[0], callScrRegs[0], callScrRegs[1]);
  emitFunctionCallLambda();
  backend_.tryPopStacktraceEntry(of_stacktraceRecord_, callScrRegs[0]);
}

void CallBase::prepareStackFrame() {
  // We are setting up the following stack structure from here on
  // RSP <------------ Stack growth direction (downwards)
  //   <----------------------------------- totalReserved ---------------------> <---- lastMaximumOffset
  // | Stack Params  | Stack Return values | (Stacktrace Record) | jobMemoryPtrPtr(for import) | Padding |

  uint32_t const of_returnValues{stackParamWidth_};
  of_stacktraceRecord_ = of_returnValues + stackReturnWidth_;
  of_jobMemoryPtrPtr_ = of_stacktraceRecord_ + Tricore_Backend::Widths::stacktraceRecord;
  uint32_t const of_lr{of_jobMemoryPtrPtr_ + jobMemoryPtrPtrWidth_};
  uint32_t const of_post{of_lr};

  uint32_t const lastMaximumOffset{backend_.common_.getCurrentMaximumUsedStackFramePosition()};

  // Reduce stack usage ("Red zone") and align stack (without paramWidth) before CALL
  uint32_t const newStackFrameSize{backend_.as_.alignStackFrameSize(of_post + lastMaximumOffset)};
  backend_.as_.setStackFrameSize(newStackFrameSize);
  if (backend_.moduleInfo_.currentState.checkedStackFrameSize < newStackFrameSize) {
    backend_.moduleInfo_.currentState.checkedStackFrameSize = newStackFrameSize;
    // Use D0 instead of callScrRegs because callScrRegs might contain locals here which haven't been spilled yet
    backend_.as_.checkStackFence(REG::D0, WasmABI::REGS::addrScrReg[0]); // SP change
  }
}

void DirectV2Import::iterateParams(Stack::iterator const paramsBase) {
  // spill all locals in regs
  // All tricore regs are regarded as volatile registers since call is not used in function call, hardware won' auto
  // save CSA
  RegMask const availableLocalsRegMask{backend_.common_.saveLocalsAndParamsForFuncCall(true)};

  Stack::iterator currentParam{paramsBase};
  uint32_t offsetInArgs{0U};
  backend_.moduleInfo_.iterateParamsForSignature(
      sigIndex_, FunctionRef<void(MachineType)>([this, &offsetInArgs, &availableLocalsRegMask, &currentParam](MachineType const paramType) {
        VariableStorage const sourceStorage{backend_.common_.getOptimizedSourceStorage(*currentParam, availableLocalsRegMask)};
        uint32_t const offsetFromSP{offsetInArgs};
        offsetInArgs += 8U; // Align to 8
        VariableStorage const targetStorage{VariableStorage::stackMemory(paramType, backend_.moduleInfo_.fnc.stackFrameSize - offsetFromSP)};
        // (reg|stack)->stack
        backend_.emitMoveImpl(targetStorage, sourceStorage, false);
        backend_.common_.removeReference(currentParam);
        currentParam = backend_.stack_.erase(currentParam);
      }));

  backend_.as_.INSTR(MOVAA_Aa_Ab).setAa(NativeABI::addrParamRegs[0]).setAb(REG::SP)();
  uint32_t const of_returnValues{stackParamWidth_};
  backend_.as_.INSTR(LEA_Aa_deref_Ab_off16sx)
      .setAa(NativeABI::addrParamRegs[1])
      .setAb(REG::SP)
      .setOff16sx(SafeInt<16U>::fromUnsafe(static_cast<int32_t>(of_returnValues)))();
  backend_.as_.emitLoadDerefOff16sx(NativeABI::addrParamRegs[2], WasmABI::REGS::linMem,
                                    SafeInt<16U>::fromConst<-Basedata::FromEnd::customCtxOffset>());
}

void DirectV2Import::iterateResults() {
  if (numReturnValues_ > 0U) {
    // update compile stack only, no code emit
    uint32_t offsetInRets{stackParamWidth_};
    backend_.moduleInfo_.iterateResultsForSignature(
        sigIndex_, FunctionRef<void(MachineType)>([this, &offsetInRets](MachineType const machineType) {
          uint32_t const offsetFromSP{offsetInRets};
          offsetInRets += 8U; // Align to 8
          StackElement const returnValueElement{
              StackElement::tempResult(machineType, VariableStorage::stackMemory(machineType, backend_.moduleInfo_.fnc.stackFrameSize - offsetFromSP),
                                       backend_.moduleInfo_.getStackMemoryReferencePosition())};
          backend_.common_.pushAndUpdateReference(returnValueElement);
        }));
  }
}

Stack::iterator V1CallBase::iterateParamsBase(Stack::iterator const paramsBase, RegMask const &availableLocalsRegMask, bool const isImported) {
  Stack::iterator currentParam{paramsBase};
  backend_.moduleInfo_.iterateParamsForSignature(
      sigIndex_, FunctionRef<void(MachineType)>([this, isImported, &availableLocalsRegMask, &currentParam](MachineType const paramType) {
        VariableStorage targetStorage{};
        REG const targetReg{backend_.getREGForArg(paramType, isImported, tracker)};
        VariableStorage const sourceStorage{backend_.common_.getOptimizedSourceStorage(*currentParam, availableLocalsRegMask)};

        if (targetReg != REG::NONE) {
          targetStorage = VariableStorage::reg(paramType, targetReg);
          bool const is64{MachineTypeUtil::is64(paramType)};
          if (sourceStorage.type == StorageType::REGISTER) {
            REG const sourceReg{sourceStorage.location.reg};
            if (sourceReg != targetReg) {
              if (is64) {
                gprCopyResolver.push(targetStorage, ResolverRecord::TargetType::Extend, sourceStorage);
                gprCopyResolver.push(VariableStorage::reg(paramType, RegUtil::getOtherExtReg(targetReg)),
                                     ResolverRecord::TargetType::Extend_Placeholder,
                                     VariableStorage::reg(paramType, RegUtil::getOtherExtReg(sourceReg)));
              } else {
                gprCopyResolver.push(targetStorage, sourceStorage);
              }
            }
          } else {
            if (is64) {
              gprCopyResolver.push(targetStorage, ResolverRecord::TargetType::Extend, sourceStorage);
              gprCopyResolver.push(VariableStorage::reg(paramType, RegUtil::getOtherExtReg(targetReg)),
                                   ResolverRecord::TargetType::Extend_Placeholder, sourceStorage);
            } else {
              gprCopyResolver.push(targetStorage, sourceStorage);
            }
          }
        } else {
          uint32_t const offsetFromSP{backend_.offsetInStackArgs(isImported, stackParamWidth_, tracker, paramType)};
          targetStorage = VariableStorage{VariableStorage::stackMemory(paramType, backend_.moduleInfo_.fnc.stackFrameSize - offsetFromSP)};
          // (reg|stack)->stack
          backend_.emitMoveImpl(targetStorage, sourceStorage, false);
        }

        backend_.common_.removeReference(currentParam);
        currentParam = backend_.stack_.erase(currentParam);
      }));

  return currentParam;
}

void V1CallBase::iterateResults() {
  if (numReturnValues_ > 0U) {
    // update compile stack only, no code emit
    RegStackTracker returnValueTracker{};
    backend_.moduleInfo_.iterateResultsForSignature(
        // coverity[autosar_cpp14_a5_1_9_violation]
        sigIndex_, FunctionRef<void(MachineType)>([this, &returnValueTracker](MachineType const machineType) {
          uint32_t const of_returnValues{stackParamWidth_};
          StackElement returnValueElement{};
          REG const targetReg{backend_.getREGForReturnValue(machineType, returnValueTracker)};
          if (targetReg != REG::NONE) {
            returnValueElement = StackElement::scratchReg(targetReg, MachineTypeUtil::toStackTypeFlag(machineType));
          } else {
            uint32_t const offsetFromSP{of_returnValues + Tricore_Backend::offsetInStackReturnValues(returnValueTracker, machineType)};
            returnValueElement = StackElement::tempResult(
                machineType, VariableStorage::stackMemory(machineType, backend_.moduleInfo_.fnc.stackFrameSize - offsetFromSP),
                backend_.moduleInfo_.getStackMemoryReferencePosition());
          }
          backend_.common_.pushAndUpdateReference(returnValueElement);
        }));
  }
}

void V1CallBase::resolveRegisterCopies() VB_THROW {
  // coverity[autosar_cpp14_a8_5_2_violation]
  auto moveEmitter = [this](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) VB_THROW {
    backend_.emitMoveImpl(targetStorage, sourceStorage, false);
  };

  // Resolve GPR copies with custom swap implementation for TriCore
  gprCopyResolver.resolve(
      // coverity[autosar_cpp14_a5_1_4_violation]
      MoveEmitter(moveEmitter),
      SwapEmitter([this](VariableStorage const &targetStorage, VariableStorage const &sourceStorage, bool const swapContains64) VB_THROW {
        static_cast<void>(swapContains64);
        // GCOVR_EXCL_START
        assert(targetStorage.type == StorageType::REGISTER && sourceStorage.type == StorageType::REGISTER &&
               "SwapEmitter only supports register to register moves");
        // GCOVR_EXCL_STOP
        bool const targetIs64{MachineTypeUtil::is64(targetStorage.machineType)};

        REG const targetReg{targetStorage.location.reg};
        REG const sourceReg{sourceStorage.location.reg};

        backend_.swapReg(targetReg, sourceReg);
        if (targetIs64) {
          REG const otherTargetReg{RegUtil::getOtherExtReg(targetReg)};
          REG const otherSourceReg{RegUtil::getOtherExtReg(sourceReg)};
          backend_.swapReg(otherTargetReg, otherSourceReg);
        }
      }));
}
// coverity[autosar_cpp14_a8_4_7_violation]
void InternalCall::handleIndirectCallReg(Stack::iterator const indirectCallIndex, RegMask const &availableLocalsRegMask) VB_NOEXCEPT {
  constexpr VariableStorage indexTargetStorage{VariableStorage::reg(MachineType::I32, WasmABI::REGS::indirectCallReg)};
  VariableStorage const sourceStorage{backend_.common_.getOptimizedSourceStorage(*indirectCallIndex, availableLocalsRegMask)};

  if (!sourceStorage.inSameLocation(indexTargetStorage)) {
    gprCopyResolver.push(indexTargetStorage, sourceStorage);
  }

  backend_.common_.removeReference(indirectCallIndex);
  static_cast<void>(backend_.stack_.erase(indirectCallIndex));
}

} // namespace tc
} // namespace vb

#endif
