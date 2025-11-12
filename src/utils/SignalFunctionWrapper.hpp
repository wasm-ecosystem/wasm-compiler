///
/// @file SignalFunctionWrapper.hpp
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
#ifndef SIGNAL_FUNCTION_WRAPPER_HPP
#define SIGNAL_FUNCTION_WRAPPER_HPP
#include "src/config.hpp"
#include "src/core/common/util.hpp"
#include "src/core/runtime/Runtime.hpp"

#ifdef VB_POSIX
#include "SignalFunctionWrapper_unix.hpp"
namespace vb {
using SignalFunction = SignalFunctionWrapperUnix; ///< Use unix signal function
}
#elif (defined VB_WIN32)
#include "SignalFunctionWrapper_win.hpp"
namespace vb {
using SignalFunction = SignalFunctionWrapperWin; ///< Use windows signal function
}
#else
static_assert(false, "OS not supported");
#endif

namespace vb {
///
/// @brief A wrapper to run Wasm function inside signal handler
///
/// s This wrapper is used for stack protection, linear memory protection and debugger
///
// coverity[autosar_cpp14_a0_1_6_violation]
// coverity[autosar_cpp14_m3_4_1_violation]
class SignalFunctionWrapper final {
public:
  /// @brief Run module start section
  ///
  /// @param runtime The runtime to start
  /// @throws vb::TrapException Wasm function execution error
  /// @throws std::runtime_error signal handler setup failed
  ///
  static inline void start(Runtime &runtime) {
    trySetRuntime(runtime);
    SignalFunction::call_raw(start_wrapper, runtime);
  }

  ///
  /// @brief set signal handler to as persistent mode
  /// @see RAIISignalHandler::setPersistentHandlerMode
  ///
  static inline void setPersistentHandler() {
    SignalFunction::setPersistentHandler();
  }

  ///
  /// @brief Call a ModuleFunction with signal handler
  ///
  /// @tparam NumReturnValue number of return values of ModuleFunction
  /// @tparam FunctionArguments Argument types of ModuleFunction
  /// @param function The ModuleFunction to run
  /// @param args Arguments of ModuleFunction
  /// @return std::array<WasmValue, NumReturnValue> Array of the return values
  /// @throws vb::TrapException Wasm function execution error
  /// @throws std::runtime_error signal handler setup failed
  ///
  template <size_t NumReturnValue, typename... FunctionArguments>
  static std::array<WasmValue, NumReturnValue> call(ModuleFunction<NumReturnValue, FunctionArguments...> const &function,
                                                    FunctionArguments... args) VB_THROW {
    trySetRuntime(function.getRuntime());
    return SignalFunction::call_raw<NumReturnValue>(function, args...);
  }

  ///
  /// @brief Call a RawModuleFunction with signal handler
  ///
  /// @param function The RawModuleFunction to run
  /// @param serializedArgs address of serialized arguments
  /// @param results Pointer to the area where return values should be stored.
  /// @throws vb::TrapException Wasm function execution error
  /// @throws std::runtime_error signal handler setup failed
  ///
  static void call(RawModuleFunction const &function, uint8_t const *const serializedArgs, uint8_t *const results) {
    trySetRuntime(function.getRuntime());
    return SignalFunction::call_raw(callRawModuleFunction_wrapper, function, serializedArgs, results);
  }

  ///
  /// @brief Call a RawModuleFunction with signal handler
  ///
  /// @param function The RawModuleFunction to run
  /// @param serializedArgs address of serialized arguments
  /// @param results Pointer to the area where return values should be stored.
  /// @throws vb::TrapException Wasm function execution error
  /// @throws std::runtime_error signal handler setup failed
  ///
  static void call(RawModuleFunction const &function, WasmValue const *const serializedArgs, WasmValue *const results) {
    trySetRuntime(function.getRuntime());
    return SignalFunction::call_raw(callRawModuleFunction_wrapper, function, pCast<uint8_t const *const>(serializedArgs),
                                    pCast<uint8_t *const>(results));
  }

#if !LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK
  ///
  /// @brief Get the Reference of Runtime in current thread
  ///
  /// @return Runtime const&
  static inline Runtime const *getRuntime() VB_NOEXCEPT {
    return pRuntime_;
  }
#endif

#if !LINEAR_MEMORY_BOUNDS_CHECKS
  ///
  /// @brief Set the Landing Pad
  ///
  /// @param landingPadData
  static void setLandingPad(uint32_t const landingPadData) VB_NOEXCEPT {
    landingPadData_ = landingPadData;
  }
#endif

private:
  ///
  /// @brief Set the runtime pointer of current thread
  ///
  /// @param runtime
  /// @note If signal handler is not required by current build config, this function will do nothing
  static void trySetRuntime(Runtime const &runtime) VB_NOEXCEPT {
#if !LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK
    pRuntime_ = &runtime;
#else
    static_cast<void>(runtime);
#endif
  }
  ///
  /// @brief wrap the raw module function in a function type
  ///
  /// @param function The RawModuleFunction to run
  /// @param serializedArgs address of serialized arguments
  /// @param results Pointer to the area where return values should be stored.
  /// @throws vb::TrapException Wasm function execution error
  /// @throws std::runtime_error signal handler setup failed
  ///
  static void callRawModuleFunction_wrapper(RawModuleFunction const &function, uint8_t const *const serializedArgs, uint8_t *const results) {
    return function.call(serializedArgs, results);
  }

  ///
  /// @brief static wrapper to call the local thread's runtime.start
  ///
  /// @param runtime
  ///
  static inline void start_wrapper(Runtime &runtime) {
    runtime.start();
  }

#if !LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK
  thread_local static Runtime const *pRuntime_; ///< Thread local pointer of Runtime
                                                ///
  /// @brief Check if pc is in Wasm code range
  ///
  /// @param pc The address of pc
  /// @return True if pc is in Wasm code range
  ///
  static bool pcInWasmCodeRange(void *const pc) VB_NOEXCEPT;
#endif

#if !LINEAR_MEMORY_BOUNDS_CHECKS
  thread_local static uint32_t landingPadData_; ///< The landing pad offset in linear memory
  ///
  /// @brief Get the offset of an address in linear memory
  ///
  /// @param addr The address to get offset
  /// @return Offset in linear memory
  ///
  static int64_t getOffsetInLinearMemoryAllocation(void *const addr) VB_NOEXCEPT;
  ///
  /// @brief probe if landingPadData_ is a valid offset in linear memory
  ///
  static void probeLinearMemoryOffset() VB_NOEXCEPT;
#endif

  friend SignalFunction; ///< Allow OS specific signal function to access private members
};

} // namespace vb
#endif
