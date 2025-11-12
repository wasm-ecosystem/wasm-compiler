///
/// @file x86_64_call_dispatch.hpp
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

#ifndef X86_64_CALL_DISPATCH_HPP
#define X86_64_CALL_DISPATCH_HPP

#include <cassert>

#include "src/config.hpp"
#ifdef JIT_TARGET_X86_64

#include <cstdint>

#include "x86_64_assembler.hpp"
#include "x86_64_cc.hpp"

#include "src/core/compiler/backend/x86_64/x86_64_backend.hpp"
#include "src/core/compiler/common/RegMask.hpp"
#include "src/core/compiler/common/RegisterCopyResolver.hpp"
#include "src/core/compiler/common/Stack.hpp"
namespace vb {
namespace x86_64 {
/// @brief Base class for call dispatch on x86_64 architecture
class CallBase {
protected:
  /// @brief Constructor for CallBase
  /// @param backend Reference to the x86_64 backend
  /// @param sigIndex Function signature index
  /// @param of_stackParams Offset for stack parameters
  /// @param stackParamWidth Width of stack parameters
  /// @param stackReturnWidth Width of stack return values
  CallBase(x86_64_Backend &backend, uint32_t const sigIndex, uint32_t const of_stackParams, uint32_t const stackParamWidth,
           uint32_t const stackReturnWidth) VB_THROW : backend_(backend),
                                                       sigIndex_(sigIndex),
                                                       of_stackParams_(of_stackParams),
                                                       stackParamWidth_(stackParamWidth),
                                                       numReturnValues_(backend.moduleInfo_.getNumReturnValuesForSignature(sigIndex)),
                                                       stackReturnWidth_(stackReturnWidth),
                                                       of_stacktraceRecordAndDebugInfo_(0U),
                                                       of_jobMemoryPtrPtr_(0U) {
    prepareStackFrame();
  }

  CallBase(CallBase const &) = delete;
  CallBase(CallBase &&) = delete;
  CallBase &operator=(CallBase const &) & = delete;
  CallBase &operator=(CallBase &&) & = delete;
  ~CallBase() = default;

public:
  /// @brief Get job memory pointer pointer offset
  inline uint32_t getJobMemoryPtrPtrOffset() const VB_NOEXCEPT {
    return of_jobMemoryPtrPtr_;
  }

  /// @brief Emit function call wrapper with stack trace information
  void emitFncCallWrapper(uint32_t const fncIndex, FunctionRef<void()> const &emitFunctionCallLambda);

  /// @brief Update new stackFrame size. Check overflow if needed
  /// @param newAlignedStackFrameSize aligned new stackFrame size
  void updateStackFrameSizeHelper(uint32_t const newAlignedStackFrameSize);

private:
  /// @brief Prepare stack frame for function call
  void prepareStackFrame();

protected:
  x86_64_Backend &backend_; ///< Reference to the x86_64 backend
  uint32_t const sigIndex_; ///< Function signature index

  uint32_t const of_stackParams_; ///< Offset for stack parameters

  uint32_t const stackParamWidth_;  ///< Width of stack parameters
  uint32_t const numReturnValues_;  ///< Number of return values
  uint32_t const stackReturnWidth_; ///< Width of stack return values

private:
  // init by prepareStackFrame
  uint32_t of_stacktraceRecordAndDebugInfo_; ///< Offset for stacktrace record
  uint32_t of_jobMemoryPtrPtr_;              ///< Offset for job memory pointer
};

/// @brief Direct V2 import call handler for x86_64
// coverity[autosar_cpp14_m3_4_1_violation]
class DirectV2Import final : public CallBase {
public:
  /// @brief Constructor for DirectV2Import
  /// @param backend Reference to the x86_64 backend
  /// @param sigIndex Function signature index
  DirectV2Import(x86_64_Backend &backend, uint32_t const sigIndex) VB_THROW
      : CallBase(backend, sigIndex, NativeABI::shadowSpaceSize, backend.moduleInfo_.getNumParamsForSignature(sigIndex) * 8U,
                 backend.moduleInfo_.getNumReturnValuesForSignature(sigIndex) * 8U) {
  }
  /// @brief Iterate through function parameters
  void iterateParams(Stack::iterator const paramsBase);
  /// @brief Iterate through function results
  void iterateResults();
};

/// @brief Base class for V1 calling convention on x86_64
class V1CallBase : public CallBase {
protected:
  /// @brief Constructor for V1CallBase
  /// @param backend Reference to the x86_64 backend
  /// @param sigIndex Function signature index
  /// @param of_stackParams Offset for stack parameters
  /// @param stackParamWidth Width of stack parameters
  V1CallBase(x86_64_Backend &backend, uint32_t const sigIndex, uint32_t const of_stackParams, uint32_t const stackParamWidth) VB_THROW
      : CallBase(backend, sigIndex, of_stackParams, stackParamWidth, backend.common_.getStackReturnValueWidth(sigIndex)) {
  }

  V1CallBase(V1CallBase const &) = delete;
  V1CallBase(V1CallBase &&) = delete;
  V1CallBase &operator=(V1CallBase const &) & = delete;
  V1CallBase &operator=(V1CallBase &&) & = delete;
  ~V1CallBase() = default;

public:
  /// @brief Iterate through function parameters (base implementation)
  Stack::iterator iterateParamsBase(Stack::iterator const paramsBase, RegMask const &availableLocalsRegMask, bool const isImported);
  /// @brief Iterate through function results
  void iterateResults();

  ///
  /// @brief Resolve register copy operations for both GPR and FPR copy resolvers
  ///
  void resolveRegisterCopies() VB_THROW;

private:
  /// @brief Size for GPR copy resolver
  static constexpr uint32_t gprResolverSize{
      static_cast<uint32_t>(std::max(NativeABI::gpParams.size(), static_cast<size_t>(WasmABI::regsForParams) + 1U))}; // +1 for the call indirect reg
protected:
  /// @param gprCopyResolver GPR register copy resolver
  RegisterCopyResolver<gprResolverSize> gprCopyResolver;
  /// @param fprCopyResolver FPR register copy resolver
  RegisterCopyResolver<std::max(NativeABI::flParams.size(), static_cast<size_t>(WasmABI::regsForParams))> fprCopyResolver;
  RegStackTracker tracker{}; ///< Register stack tracker
};

/// @brief Import call V1 handler for x86_64
// coverity[autosar_cpp14_m3_4_1_violation]
class ImportCallV1 final : public V1CallBase {
public:
  using V1CallBase::V1CallBase;
  /// @brief Constructor for ImportCallV1
  /// @param backend Reference to the x86_64 backend
  /// @param sigIndex Function signature index
  ImportCallV1(x86_64_Backend &backend, uint32_t const sigIndex) VB_THROW
      : V1CallBase(backend, sigIndex, NativeABI::shadowSpaceSize, backend.getStackParamWidth(sigIndex, true)) {
  }

  /// @brief Iterate through imported function parameters
  inline Stack::iterator iterateParams(Stack::iterator const paramsBase, RegMask const &availableLocalsRegMask) {
    return V1CallBase::iterateParamsBase(paramsBase, availableLocalsRegMask, true);
  }
  /// @brief Prepare context for import call
  void prepareCtx();
};

/// @brief Internal call handler for x86_64
// coverity[autosar_cpp14_m3_4_1_violation]
class InternalCall final : public V1CallBase {
public:
  using V1CallBase::V1CallBase;
  /// @brief Constructor for InternalCall
  /// @param backend Reference to the x86_64 backend
  /// @param sigIndex Function signature index
  InternalCall(x86_64_Backend &backend, uint32_t const sigIndex) VB_THROW
      : V1CallBase(backend, sigIndex, 0U, backend.getStackParamWidth(sigIndex, false)) {
  }

  /// @brief Handle indirect call register
  void handleIndirectCallReg(Stack::iterator const indirectCallIndex, RegMask const &availableLocalsRegMask) VB_NOEXCEPT;

  /// @brief Iterate through internal function parameters
  inline Stack::iterator iterateParams(Stack::iterator const paramsBase, RegMask const &availableLocalsRegMask) {
    return V1CallBase::iterateParamsBase(paramsBase, availableLocalsRegMask, false);
  }
};

} // namespace x86_64
} // namespace vb

#endif
#endif
