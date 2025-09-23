///
/// @file SignalFunctionWrapper_win.hpp
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
#include "src/config.hpp"
#ifdef VB_WIN32
#ifndef FUNCTIONWRAPPER_WIN_HPP
#define FUNCTIONWRAPPER_WIN_HPP

#include <cstdint>

#include "OSAPIChecker.hpp"
#include "RAIISignalHandler.hpp"

#include "src/config.hpp"
#include "src/core/runtime/Runtime.hpp"

namespace vb {
///
/// @brief Error code of signal wrapper call
///
///
enum class SignalWrapperErrorCode : uint8_t {
  NONE,         ///< No error
  STACKOVERFLOW ///< Error stack overflow
};

///
/// @brief Signal function wrapper on windows
///
class SignalFunctionWrapperWin final {
public:
#if !ACTIVE_STACK_OVERFLOW_CHECK
  ///
  /// @brief Set If stack top of current thread is volatile
  ///
  /// @note The stack top is assumed not volatile by default to avoid get stack top by OS API before each Wasm function
  /// call
  ///
  static inline void setStackTopVolatile(bool isVolatile) VB_NOEXCEPT {
    stackTopIsVolatile_ = isVolatile;
  }

  ///
  /// @brief Get the error code
  ///
  /// @return SignalWrapperErrorCode
  ///
  static inline SignalWrapperErrorCode getErrorCode() VB_NOEXCEPT {
    return error_;
  }
#endif
  ///
  /// @brief Windows implementation of Calling (Wrapped) Wasm function with Signal handler
  ///
  /// @tparam FunctionType The type of wrapped function
  /// @tparam FunctionArguments The argument types of wrapped function
  /// @param function The wrapped function
  /// @param args The arguments of wrapped function
  ///
  template <class FunctionType, typename... FunctionArguments>
  static void call_raw(FunctionType const &function, FunctionArguments &&...args) VB_THROW {
#if !LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK
    error_ = SignalWrapperErrorCode::NONE;

    // Register the signal handler
    RAIISignalHandler raiiSignalHandler(memorySignalHandler, divSignalHandler);

#if !ACTIVE_STACK_OVERFLOW_CHECK
    if (!stackGuaranteeSet_ || stackTopIsVolatile_) {
      ULONG guaranteeStackSize = 1024;
      BOOL success = SetThreadStackGuarantee(&guaranteeStackSize);

      if (!success) {
        printf("SetThreadStackGuarantee failed with code %d\n", success);
        throw RuntimeError(ErrorCode::SetThreadStackGuarantee_failed);
      }

      stackGuaranteeSet_ = true;
    }
#endif

#endif
    return function(args...);
  }

  ///
  /// @brief Windows implementation of Calling (Wrapped) Wasm function with Signal handler
  ///
  /// @tparam NumReturnValue The number of return values of Wasm function
  /// @tparam FunctionType The type of wrapped function
  /// @tparam FunctionArguments The argument types of wrapped function
  /// @param function The wrapped function
  /// @param args The arguments of wrapped function
  ///
  template <size_t NumReturnValue, class FunctionType, typename... FunctionArguments>
  static std::array<WasmValue, NumReturnValue> call_raw(FunctionType const &function, FunctionArguments &&...args) VB_THROW {
#if !LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK
    error_ = SignalWrapperErrorCode::NONE;

    // Register the signal handler
    RAIISignalHandler raiiSignalHandler(memorySignalHandler, divSignalHandler);

#if !ACTIVE_STACK_OVERFLOW_CHECK
    if (!stackGuaranteeSet_ || stackTopIsVolatile_) {
      ULONG guaranteeStackSize = 1024;
      BOOL success = SetThreadStackGuarantee(&guaranteeStackSize);

      if (!success) {
        printf("SetThreadStackGuarantee failed with code %d\n", success);
        throw RuntimeError(ErrorCode::SetThreadStackGuarantee_failed);
      }

      stackGuaranteeSet_ = true;
    }
#endif

#endif
    return function(args...);
  }

  ///
  /// @brief set Persistent Handler for windows
  ///
  static void inline setPersistentHandler() {
#if !LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK
    RAIISignalHandler::setPersistentHandlerMode(memorySignalHandler, divSignalHandler);
#endif
  }

private:
#if !LINEAR_MEMORY_BOUNDS_CHECKS
  thread_local static SignalWrapperErrorCode error_; ///< Error code of Wasm function call catch by signal handler
#endif

#if !LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK
  /**
   * @brief Handles a Wasm trap triggered by a Windows exception, setting the return address and trap parameters in the exception context.
   * @param pExceptionInfo Pointer to the Windows exception context (PEXCEPTION_POINTERS), used to modify the return address and parameters.
   * @param trapCode Trap code indicating the type of exception, see TrapCode enum for details.
   *
   * @return Returns EXCEPTION_CONTINUE_EXECUTION to resume execution or other status codes as needed.
   *
   * @note This function is intended for internal use by signal handlers and should not be called directly from outside.
   */
  static long handleTrap(PEXCEPTION_POINTERS pExceptionInfo, uint32_t const trapCode) VB_NOEXCEPT;

  static long memorySignalHandler(PEXCEPTION_POINTERS pExceptionInfo) VB_NOEXCEPT; ///< Windows memory signal handler function
#endif
  static long divSignalHandler(PEXCEPTION_POINTERS pExceptionInfo) VB_NOEXCEPT; ///< Windows div signal handler function

#if !ACTIVE_STACK_OVERFLOW_CHECK
  thread_local static bool stackTopIsVolatile_; ///< If stack stop is volatile
  ///
  /// @brief If stack guarantee of current stack is already set,
  /// to reduce calling set stack guarantee OS API multi times
  ///
  thread_local static bool stackGuaranteeSet_;
#endif
};

} // namespace vb

#endif
#endif
