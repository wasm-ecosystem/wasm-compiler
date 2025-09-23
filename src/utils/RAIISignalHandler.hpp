///
/// @file RAIISignalHandler.hpp
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
#ifndef RAII_SIGNAL_HANDLER_HPP
#define RAII_SIGNAL_HANDLER_HPP

#include <csignal>
#include <cstdint>
#include <mutex>

#include "src/config.hpp"

#ifdef VB_WIN32
#include "windows_clean.hpp"
#endif

namespace vb {
///
/// @brief Set signal handler in RAII way
///
class RAIISignalHandler final {
public:
#ifdef VB_WIN32
  using SignalHandler = long (*)(PEXCEPTION_POINTERS pExceptionInfo); ///< Type of Windows signal handler
#else
  using SignalHandler = void (*)(int32_t signalId, siginfo_t *si, void *ptr); ///< Type of Unix signal handler
#endif
  ///
  /// @brief Increase signal handler by one, if it equals to 1, set signal handler. Otherwise this signal handler
  ///
  /// @param memorySignalHandler The memory signal handler to be set
  /// @param divSignalHandler The div signal handler to be set
  /// @note This function is thread safe
  /// @throws std::runtime_error signal handler set failed
  ///
  inline explicit RAIISignalHandler(SignalHandler const memorySignalHandler, SignalHandler const divSignalHandler) {
    if (raiiSetSignalHandler) {
      std::lock_guard<std::mutex> const lock{handlerMutex};

      if (runningCounter == 0U) {
        setSignalHandler(memorySignalHandler, divSignalHandler);
      }
      runningCounter++;
    }
  }

  RAIISignalHandler(const RAIISignalHandler &) = delete;
  RAIISignalHandler(RAIISignalHandler &&) = delete;
  RAIISignalHandler &operator=(const RAIISignalHandler &) & = delete;
  RAIISignalHandler &operator=(RAIISignalHandler &&) & = delete;
  ///
  /// @brief reduce counter by one, unset signal handler if it equals to 0
  /// @note This function is thread safe
  ///
  ~RAIISignalHandler() VB_NOEXCEPT;

  ///
  /// @brief Set the Persistent mode, in this mode, the signal handler will be set only once and won't be unset
  /// this would be helpful to reduce the syscall performance cost of set/unset signal handler
  /// @param memorySignalHandler The memory signal handler to be set
  /// @param divSignalHandler The div signal handler to be set
  /// @throws std::runtime_error signal handler set failed
  ///
  static inline void setPersistentHandlerMode(SignalHandler const memorySignalHandler, SignalHandler const divSignalHandler) {
    setSignalHandler(memorySignalHandler, divSignalHandler);
    raiiSetSignalHandler = false;
  }
  /// @brief restore the signal handler to original
  static void restoreSignalHandler() VB_NOEXCEPT;

private:
  ///
  /// @brief implementation of set signal handler on different OS.
  ///
  /// @param memorySignalHandler The memory signal handler to be set
  /// @param divSignalHandler The div signal handler to be set
  /// @throws std::runtime_error signal handler set failed
  ///
  static void setSignalHandler(SignalHandler const memorySignalHandler, SignalHandler const divSignalHandler);

  ///
  /// @brief unset signal handler if runningCounter==0
  ///
  ///
  static void unsetSignalHandler() VB_NOEXCEPT;

  static uint32_t runningCounter; ///< Reference counter of currently set signal handler
  static std::mutex handlerMutex; ///< Mutex to  protect the runningCounter
  ///@see RAIISignalHandler::setPersistentHandlerMode
  static bool raiiSetSignalHandler;

#ifdef VB_WIN32
  static void *handleMem; ///< signal handler handle on Windows
#if !ACTIVE_DIV_CHECK
  static void *handleDiv; ///< signal handler handle on Windows
#endif
#else
  static struct sigaction saSEGVOld;   ///< old signal handler for SIGSEGV
  static struct sigaction saSIGFPEOld; ///< old signal handler for SIGFPE
#ifdef __APPLE__
  static struct sigaction saSIGBUSOld; ///< old signal handler for SIGBUS
#endif
#endif
};
} // namespace vb
#endif
