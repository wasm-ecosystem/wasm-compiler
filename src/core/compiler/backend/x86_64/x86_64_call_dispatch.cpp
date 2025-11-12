///
/// @file x86_64_call_dispatch.cpp
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

#ifdef JIT_TARGET_X86_64

#include <array>
#include <cassert>
#include <cstdint>

#include "src/core/common/FunctionRef.hpp"
#include "src/core/common/basedataoffsets.hpp"
#include "src/core/compiler/Compiler.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_assembler.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_backend.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_call_dispatch.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_cc.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_encoding.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_instruction.hpp"
#include "src/core/compiler/common/Common.hpp"
#include "src/core/compiler/common/ListIterator.hpp"
#include "src/core/compiler/common/MachineType.hpp"
#include "src/core/compiler/common/ModuleInfo.hpp"
#include "src/core/compiler/common/RegMask.hpp"
#include "src/core/compiler/common/RegisterCopyResolver.hpp"
#include "src/core/compiler/common/Stack.hpp"
#include "src/core/compiler/common/StackElement.hpp"
#include "src/core/compiler/common/StackType.hpp"
#include "src/core/compiler/common/VariableStorage.hpp"

namespace vb {
namespace x86_64 {
namespace BD = Basedata; ///< shortcut of Basedata

void CallBase::updateStackFrameSizeHelper(uint32_t const newAlignedStackFrameSize) {
  backend_.as_.setStackFrameSize(newAlignedStackFrameSize);
  backend_.moduleInfo_.fnc.stackFrameSize = newAlignedStackFrameSize;
}

void CallBase::prepareStackFrame() {
  // RSP <------------ Stack growth direction (downwards)                                          <----lastMaximumOffset
  // | Shadow Space(for imported) | Stack Params | Stack Return values | Stacktrace Record + Debug Info | (JobMemoryPtrPtr) |
  // Padding |

  uint32_t const stacktraceWidth{
      (backend_.compiler_.isStacktraceEnabled() || backend_.compiler_.getDebugMode()) ? x86_64_Backend::Widths::stacktraceRecord : 0U};
  uint32_t const debugInfoWidth{backend_.compiler_.getDebugMode() ? x86_64_Backend::Widths::debugInfo : 0U};

  uint32_t const of_returnValues{of_stackParams_ + stackParamWidth_};
  of_stacktraceRecordAndDebugInfo_ = of_returnValues + stackReturnWidth_;
  of_jobMemoryPtrPtr_ = of_stacktraceRecordAndDebugInfo_ + stacktraceWidth + debugInfoWidth;
  uint32_t const of_post{of_jobMemoryPtrPtr_ + x86_64_Backend::Widths::jobMemoryPtrPtr};

  // Reduce stack usage to minimum required and align stack before call
  uint32_t const lastMaximumOffset{backend_.common_.getCurrentMaximumUsedStackFramePosition()};
  uint32_t const newAlignedStackFrameSize{backend_.as_.alignStackFrameSize(lastMaximumOffset + of_post)};
  updateStackFrameSizeHelper(newAlignedStackFrameSize);
}

void DirectV2Import::iterateParams(Stack::iterator const paramsBase) {
  // Spill all locals in regs
  RegMask const availableLocalsRegMask{backend_.common_.saveLocalsAndParamsForFuncCall(true)};

  Stack::iterator currentParam{paramsBase};
  uint32_t offsetInArgs{of_stackParams_};
  backend_.moduleInfo_.iterateParamsForSignature(
      sigIndex_, FunctionRef<void(MachineType)>([this, &offsetInArgs, &currentParam, &availableLocalsRegMask](MachineType const paramType) {
        VariableStorage const sourceStorage{backend_.common_.getOptimizedSourceStorage(*currentParam, availableLocalsRegMask)};
        uint32_t const offsetFromSP{offsetInArgs};
        offsetInArgs += 8U; // Align to 8
        VariableStorage const targetStorage{VariableStorage::stackMemory(paramType, backend_.moduleInfo_.fnc.stackFrameSize - offsetFromSP)};
        // (reg|stack)->stack
        backend_.emitMoveImpl(targetStorage, sourceStorage, false);
        backend_.common_.removeReference(currentParam);
        currentParam = backend_.stack_.erase(currentParam);
      }));

  RegStackTracker tracker{};
  REG const regForParamsPtr{backend_.getREGForArg(MachineType::I64, true, tracker)};
  assert(regForParamsPtr != REG::NONE && "Should have three regs for params, rets and ctx");
  backend_.as_.INSTR(LEA_r64_m_t).setR(regForParamsPtr).setM4RM(REG::SP, static_cast<int32_t>(of_stackParams_))();

  REG const regForRetsPtr{backend_.getREGForArg(MachineType::I64, true, tracker)};
  assert(regForParamsPtr != REG::NONE && "Should have three regs for params, rets and ctx");
  uint32_t const of_returnValues{of_stackParams_ + stackParamWidth_};
  backend_.as_.INSTR(LEA_r64_m_t).setR(regForRetsPtr).setM4RM(REG::SP, static_cast<int32_t>(of_returnValues))();

  REG const regForCtx{backend_.getREGForArg(MachineType::I64, true, tracker)};
  assert(regForCtx != REG::NONE && "Should have three regs for params, rets and ctx");
  VariableStorage const ctxStorage{
      VariableStorage::linkData(MachineType::I64, backend_.moduleInfo_.getBasedataLength() - static_cast<uint32_t>(BD::FromEnd::customCtxOffset))};
  backend_.emitMoveImpl(VariableStorage::reg(MachineType::I64, regForCtx), ctxStorage, false);
}

void DirectV2Import::iterateResults() {
  if (numReturnValues_ > 0U) {
    uint32_t offsetInRets{of_stackParams_ + stackParamWidth_};
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
      sigIndex_, FunctionRef<void(MachineType)>([this, isImported, &currentParam, &availableLocalsRegMask](MachineType const paramType) {
        VariableStorage targetStorage{};
        REG const targetReg{backend_.getREGForArg(paramType, isImported, tracker)};
        VariableStorage const sourceStorage{backend_.common_.getOptimizedSourceStorage(*currentParam, availableLocalsRegMask)};

        if (targetReg != REG::NONE) {
          bool const sameReg{(sourceStorage.type == StorageType::REGISTER) && (sourceStorage.location.reg == targetReg)};
          if (!sameReg) {
            targetStorage = VariableStorage::reg(paramType, targetReg);

            if (RegUtil::isGPR(targetReg)) {
              gprCopyResolver.push(targetStorage, sourceStorage);
            } else {
              fprCopyResolver.push(targetStorage, sourceStorage);
            }
          }
        } else {
          uint32_t const offsetFromSP{of_stackParams_ + x86_64_Backend::offsetInStackArgs(isImported, stackParamWidth_, tracker)};
          targetStorage = VariableStorage::stackMemory(paramType, backend_.moduleInfo_.fnc.stackFrameSize - offsetFromSP);
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
    RegStackTracker returnValueTracker{};
    backend_.moduleInfo_.iterateResultsForSignature(
        sigIndex_, FunctionRef<void(MachineType)>([this, &returnValueTracker](MachineType const machineType) {
          StackElement returnValueElement{};
          REG const targetReg{backend_.getREGForReturnValue(machineType, returnValueTracker)};
          if (targetReg != REG::NONE) {
            returnValueElement = StackElement::scratchReg(targetReg, MachineTypeUtil::toStackTypeFlag(machineType));
          } else {
            uint32_t const offsetFromSP{of_stackParams_ + stackParamWidth_ +
                                        x86_64_Backend::offsetInStackReturnValues(returnValueTracker, machineType)};
            returnValueElement = StackElement::tempResult(
                machineType, VariableStorage::stackMemory(machineType, backend_.moduleInfo_.fnc.stackFrameSize - offsetFromSP),
                backend_.moduleInfo_.getStackMemoryReferencePosition());
          }
          backend_.common_.pushAndUpdateReference(returnValueElement);
        }));
  }
}

void CallBase::emitFncCallWrapper(uint32_t const fncIndex, FunctionRef<void()> const &emitFunctionCallLambda) {
  backend_.tryPushStacktraceAndDebugEntry(fncIndex, of_stacktraceRecordAndDebugInfo_, 0U, backend_.moduleInfo_.bytecodePosOfLastParsedInstruction,
                                          callScrRegs[0]);
  emitFunctionCallLambda();
  backend_.tryPopStacktraceAndDebugEntry(of_stacktraceRecordAndDebugInfo_, callScrRegs[0]);
}

void ImportCallV1::prepareCtx() {
  REG const targetReg{backend_.getREGForArg(MachineType::I64, true, tracker)};
  VariableStorage const ctxStorage{
      VariableStorage::linkData(MachineType::I64, backend_.moduleInfo_.getBasedataLength() - static_cast<uint32_t>(BD::FromEnd::customCtxOffset))};
  if (targetReg != REG::NONE) {
    gprCopyResolver.push(VariableStorage::reg(MachineType::I64, targetReg), ctxStorage);
  } else {
    uint32_t const offsetFromSP{of_stackParams_ + x86_64_Backend::offsetInStackArgs(true, stackParamWidth_, tracker)};
    VariableStorage const targetStorage{VariableStorage::stackMemory(MachineType::I64, backend_.moduleInfo_.fnc.stackFrameSize - offsetFromSP)};
    backend_.emitMoveImpl(targetStorage, ctxStorage, false, false);
  }
}
// coverity[autosar_cpp14_a8_4_7_violation]
void InternalCall::handleIndirectCallReg(Stack::iterator const indirectCallIndex, RegMask const &availableLocalsRegMask) VB_NOEXCEPT {
  // Set up the indirect call index in WasmABI::REGS::indirectCallReg
  constexpr VariableStorage indexTargetStorage{VariableStorage::reg(MachineType::I32, WasmABI::REGS::indirectCallReg)};
  VariableStorage const sourceStorage{backend_.common_.getOptimizedSourceStorage(*indirectCallIndex, availableLocalsRegMask)};

  if (!sourceStorage.inSameLocation(indexTargetStorage)) {
    gprCopyResolver.push(indexTargetStorage, sourceStorage);
  }

  backend_.common_.removeReference(indirectCallIndex);
  static_cast<void>(backend_.stack_.erase(indirectCallIndex));
}

void V1CallBase::resolveRegisterCopies() VB_THROW {
  // coverity[autosar_cpp14_a8_5_2_violation]
  auto moveEmitter = [this](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) VB_THROW {
    backend_.emitMoveImpl(targetStorage, sourceStorage, false);
  };

  // Resolve GPR copies with XCHG instruction
  gprCopyResolver.resolve(
      // coverity[autosar_cpp14_a5_1_4_violation]
      MoveEmitter(moveEmitter),
      SwapEmitter([this](VariableStorage const &targetStorage, VariableStorage const &sourceStorage, bool const swapContains64) VB_THROW {
        // GCOVR_EXCL_START
        assert(targetStorage.type == StorageType::REGISTER && sourceStorage.type == StorageType::REGISTER &&
               "SwapEmitter only supports register to register moves");
        // GCOVR_EXCL_STOP
        backend_.as_.INSTR(swapContains64 ? XCHG_rm64_r64_t : XCHG_rm32_r32_t).setR4RM(targetStorage.location.reg).setR(sourceStorage.location.reg)();
      }));

  // Resolve FPR copies with temp register swap
  fprCopyResolver.resolve(
      // coverity[autosar_cpp14_a5_1_4_violation]
      MoveEmitter(moveEmitter),
      SwapEmitter([this](VariableStorage const &targetStorage, VariableStorage const &sourceStorage, bool const swapContains64) VB_THROW {
        // GCOVR_EXCL_START
        assert(targetStorage.type == StorageType::REGISTER && sourceStorage.type == StorageType::REGISTER &&
               "SwapEmitter only supports register to register moves");
        // GCOVR_EXCL_STOP
        // Here all values in GPR are passed to callee, callScrReg can be used
        constexpr REG moveHelper{callScrRegs[0]};
        backend_.as_.INSTR(swapContains64 ? MOVQ_rm64_rf : MOVD_rm32_rf).setR4RM(moveHelper).setR(sourceStorage.location.reg)();
        backend_.as_.INSTR(swapContains64 ? MOVSD_rf_rmf : MOVSS_rf_rmf).setR(sourceStorage.location.reg).setR4RM(targetStorage.location.reg)();
        backend_.as_.INSTR(swapContains64 ? MOVQ_rf_rm64 : MOVD_rf_rm32).setR(targetStorage.location.reg).setR4RM(moveHelper)();
      }));
}

} // namespace x86_64
} // namespace vb

#endif
