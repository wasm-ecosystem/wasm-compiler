///
/// @file SignalFunctionWrapper_unix.hpp
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
#ifndef FUNCTIONWRAPPER_UNIX_HPP
#define FUNCTIONWRAPPER_UNIX_HPP

#include "src/config.hpp"
#ifdef VB_POSIX

#include <cstdint>

#include "MemUtils.hpp"
#include "OSAPIChecker.hpp"
#include "RAIISignalHandler.hpp"

#include "src/config.hpp"
#include "src/core/runtime/Runtime.hpp"

namespace vb {

#if !ACTIVE_STACK_OVERFLOW_CHECK
///
/// @brief Secondary stack to run signal handler when signal handler is triggered
///
class SecondaryStack final {
public:
  ///
  /// @brief default constructor
  ///
  SecondaryStack() = default;
  SecondaryStack(SecondaryStack const &) = delete;
  ///
  /// @brief default move constructor
  ///
  ///
  SecondaryStack(SecondaryStack &&) VB_NOEXCEPT = default;
  SecondaryStack &operator=(SecondaryStack const &other) & = delete;
  ///
  /// @brief Default move assignment
  ///
  /// @return SecondaryStack&
  ///
  SecondaryStack &operator=(SecondaryStack &&) &VB_NOEXCEPT = default;
  ///
  /// @brief Destructor, release the secondary stack
  ///
  ~SecondaryStack() VB_NOEXCEPT;
  ///
  /// @brief If the secondary stack is nullptr
  ///
  /// @return true
  /// @return false
  ///
  inline bool hasStack() const VB_NOEXCEPT {
    return ptr_ != nullptr;
  }
  ///
  /// @brief Init secondary stack for current thread
  /// @throws std::runtime_error set secondary stack failed
  ///
  void init();
  ///
  /// @brief release secondary stack for current thread
  /// @throws std::runtime_error release secondary stack failed
  ///
  void release() const VB_NOEXCEPT;
  ///
  /// @brief get address of secondary stack
  ///
  /// @return void*
  ///
  inline const void *get() const VB_NOEXCEPT {
    return ptr_;
  }
  ///
  /// @brief get size of secondary stack in current thread
  ///
  /// @return uint32_t
  ///
  static inline uint32_t getSecondaryStackSize() VB_NOEXCEPT {
    return secondaryStackSize_;
  }

private:
  thread_local static uint32_t secondaryStackSize_; ///< Size of secondary stack in current thread
  void *ptr_;                                       ///< Address of secondary stack
};
#endif
///
/// @brief Signal function wrapper for unix
///
class SignalFunctionWrapperUnix final {
public:
#if !LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK
  ///
  /// @brief Set If signal mask of current thread is volatile
  ///
  /// @param isVolatile
  /// @note The signal mask is assumed not volatile by default to avoid backup signal mask before each Wasm function
  /// call
  ///
  static inline void setSigMaskVolatile(bool const isVolatile) VB_NOEXCEPT {
    sigMaskIsVolatile_ = isVolatile;
  }
#endif

#if !ACTIVE_STACK_OVERFLOW_CHECK
  ///
  /// @brief Set If stack top of current thread is volatile
  ///
  /// @note The stack top is assumed not volatile by default to avoid get stack top by OS API before each Wasm function
  /// call
  ///
  static inline void setStackTopVolatile(bool const isVolatile) VB_NOEXCEPT {
    stackTopIsVolatile_ = isVolatile;
  }
#endif
  ///
  /// @brief Unix implementation of Calling (Wrapped) Wasm function with Signal handler
  ///
  /// @tparam FunctionType The type of wrapped function
  /// @tparam FunctionArguments The argument types of wrapped function
  /// @param function The wrapped function
  /// @param args The arguments of wrapped function
  ///
  template <class FunctionType, typename... FunctionArguments>
  // coverity[autosar_cpp14_a8_4_7_violation]
  static void call_raw(FunctionType const &function, FunctionArguments &&...args) VB_THROW {
#if !LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK

    // Register the signal handler
    RAIISignalHandler const raiiSignalHandler{&memorySignalHandler, &divSignalHandler};
    static_cast<void>(raiiSignalHandler);

    if ((!sigMaskInitialized_) || sigMaskIsVolatile_) {
      // Store current sigmask to sigMask_ and clear the new one
      int32_t const error{pthread_sigmask(SIG_SETMASK, nullptr, &sigMask_)};
      checkSysCallReturn("pthread_sigmask", error);
      sigMaskInitialized_ = true;
    }

    if ((!sigMaskInitialized_) || sigMaskIsVolatile_) {
      // Store current sigmask to sigMask_ and clear the new one
      int32_t const error{pthread_sigmask(SIG_SETMASK, nullptr, &sigMask_)};
      // coverity[autosar_cpp14_a15_0_2_violation]
      checkSysCallReturn("pthread_sigmask", error);
      sigMaskInitialized_ = true;
    }

#if !ACTIVE_STACK_OVERFLOW_CHECK
    if (!secondaryStack_.hasStack()) {
      // coverity[autosar_cpp14_a15_0_2_violation]
      secondaryStack_.init();
    }
#ifdef __linux__
    if ((stackTop_ == nullptr) || stackTopIsVolatile_) {
      // coverity[autosar_cpp14_a15_0_2_violation]
      MemUtils::StackInfo const stackInfo{MemUtils::getStackInfo()};
      stackTop_ = stackInfo.stackTop;
    }
#endif
#endif

#endif
    return function(std::forward<FunctionArguments>(args)...);
  }

  ///
  /// @brief Unix implementation of Calling (Wrapped) Wasm function with Signal handler
  ///
  /// @tparam NumReturnValue The number of return values of Wasm function
  /// @tparam FunctionType The type of wrapped function
  /// @tparam FunctionArguments The argument types of wrapped function
  /// @param function The wrapped function
  /// @param args The arguments of wrapped function
  ///
  template <size_t NumReturnValue, class FunctionType, typename... FunctionArguments>
  // coverity[autosar_cpp14_a14_1_1_violation]
  // coverity[autosar_cpp14_a13_3_1_violation]
  static std::array<WasmValue, NumReturnValue> call_raw(FunctionType const &function, FunctionArguments &&...args) VB_THROW {
#if !LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK

    // Register the signal handler
    RAIISignalHandler const raiiSignalHandler{&memorySignalHandler, &divSignalHandler};
    static_cast<void>(raiiSignalHandler);

    if ((!sigMaskInitialized_) || sigMaskIsVolatile_) {
      // Store current sigmask to sigMask_ and clear the new one
      int32_t const error{pthread_sigmask(SIG_SETMASK, nullptr, &sigMask_)};
      checkSysCallReturn("pthread_sigmask", error);
      sigMaskInitialized_ = true;
    }

    if ((!sigMaskInitialized_) || sigMaskIsVolatile_) {
      // Store current sigmask to sigMask_ and clear the new one
      int32_t const error{pthread_sigmask(SIG_SETMASK, nullptr, &sigMask_)};
      // coverity[autosar_cpp14_a15_0_2_violation]
      checkSysCallReturn("pthread_sigmask", error);
      sigMaskInitialized_ = true;
    }

#if !ACTIVE_STACK_OVERFLOW_CHECK
    if (!secondaryStack_.hasStack()) {
      // coverity[autosar_cpp14_a15_0_2_violation]
      secondaryStack_.init();
    }
#ifdef __linux__
    if ((stackTop_ == nullptr) || stackTopIsVolatile_) {
      // coverity[autosar_cpp14_a15_0_2_violation]
      MemUtils::StackInfo const stackInfo{MemUtils::getStackInfo()};
      stackTop_ = stackInfo.stackTop;
    }
#endif
#endif

#endif
    // coverity[autosar_cpp14_a15_0_2_violation]
    return function(std::forward<FunctionArguments>(args)...);
  }

  ///
  /// @brief set Persistent Handler for unix
  ///
  // coverity[autosar_cpp14_a15_4_2_violation]
  // coverity[autosar_cpp14_m0_1_8_violation]
  // coverity[autosar_cpp14_a15_4_4_violation]
  inline static void setPersistentHandler() {
#if !LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK
    RAIISignalHandler::setPersistentHandlerMode(&memorySignalHandler, &divSignalHandler);
#endif
  }

private:
#if !LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK

  thread_local static sigset_t sigMask_;        ///< Signal mask of current thread
  thread_local static bool sigMaskInitialized_; ///< If signal mask is initialized_
  thread_local static bool sigMaskIsVolatile_;  ///< If signal mask of current thread is volatile
  /**
   * @brief Handles a Wasm trap triggered by a Unix signal, setting the return address and trap parameters in the context.
   * @param uc Pointer to the signal handler context (ucontext_t*), used to modify the return address and parameters.
   * @param trapCode Trap code indicating the type of exception, see TrapCode enum for details.
   *
   * @note This function is intended for internal use by signal handlers and should not be called directly from outside.
   */
  static void handleTrap(ucontext_t *const uc, uint32_t const trapCode) VB_NOEXCEPT;
  ///
  /// @brief Unix memory signal handler for SIGSEGV and SIGBUS
  ///
  /// @param signalId
  /// @param si
  /// @param ptr
  ///
  static void memorySignalHandler(int32_t const signalId, siginfo_t *const si, void *const ptr) VB_NOEXCEPT;
  ///
  /// @brief Unix division signal handler for SIGFPE
  ///
  /// @param signalId
  /// @param si
  /// @param ptr
  ///
  static void divSignalHandler(int32_t const signalId, siginfo_t *const si, void *const ptr) VB_NOEXCEPT;
#endif

#if !ACTIVE_STACK_OVERFLOW_CHECK
  thread_local static SecondaryStack secondaryStack_; ///< Secondary stack of current thread
  thread_local static bool stackTopIsVolatile_;       ///< If stack stop is volatile
#ifdef __linux__
  thread_local static void *stackTop_; ///< Top of current stack

#endif //__linux__

  ///
  /// @brief check if current signal is current by stack overflow
  ///
  /// @param uc Signal context
  /// @return true Current fault is stack overflow
  /// @return false Current fault is not stack overflow
  ///
  static bool isStackoverflow(ucontext_t const *const uc) VB_NOEXCEPT;
#endif
};

} // namespace vb

#endif
#endif
