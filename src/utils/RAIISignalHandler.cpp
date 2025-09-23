///
/// @file RAIISignalHandler.cpp
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
#include <csignal>
#include <cstdint>
#include <mutex>

#include "OSAPIChecker.hpp"
#include "RAIISignalHandler.hpp"

#include "src/config.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/util.hpp"
#include "src/utils/OSAPIChecker.hpp"

#ifdef VB_WIN32
#include <iostream>

#include "SignalFunctionWrapper_win.hpp"
#endif
namespace vb {

uint32_t RAIISignalHandler::runningCounter{0U};
std::mutex RAIISignalHandler::handlerMutex{};
bool RAIISignalHandler::raiiSetSignalHandler{true};

#ifdef VB_WIN32
void *RAIISignalHandler::handleMem = nullptr;
#if !ACTIVE_DIV_CHECK
void *RAIISignalHandler::handleDiv = nullptr;
#endif

#else
struct sigaction RAIISignalHandler::saSEGVOld {};
struct sigaction RAIISignalHandler::saSIGFPEOld {};
#ifdef __APPLE__
struct sigaction RAIISignalHandler::saSIGBUSOld {};
#endif
#endif

RAIISignalHandler::~RAIISignalHandler() VB_NOEXCEPT {
  if (raiiSetSignalHandler) {
    unsetSignalHandler();
  }
#if defined(VB_WIN32) && !ACTIVE_STACK_OVERFLOW_CHECK
  if (SignalFunctionWrapperWin::getErrorCode() == SignalWrapperErrorCode::STACKOVERFLOW) {
    _resetstkoflw();
  }
#endif
}

// coverity[autosar_cpp14_m0_1_8_violation]
// coverity[autosar_cpp14_a15_4_4_violation]
void RAIISignalHandler::setSignalHandler(SignalHandler const memorySignalHandler, SignalHandler const divSignalHandler) {
  static_cast<void>(memorySignalHandler);
  static_cast<void>(divSignalHandler);
#ifdef VB_WIN32
#if !LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK
  handleMem = AddVectoredExceptionHandler(1, pCast<PVECTORED_EXCEPTION_HANDLER>(memorySignalHandler));
  if (handleMem == nullptr) {
    std::cout << "AddVectoredExceptionHandler failed" << std::endl;
    throw RuntimeError(ErrorCode::AddVectoredExceptionHandler_failed);
  }
#endif
#if !ACTIVE_DIV_CHECK
  handleDiv = AddVectoredExceptionHandler(1, pCast<PVECTORED_EXCEPTION_HANDLER>(divSignalHandler));
  if (handleDiv == nullptr) {
    std::cout << "AddVectoredExceptionHandler failed" << std::endl;
    throw RuntimeError(ErrorCode::AddVectoredExceptionHandler_failed);
  }
#endif
#else
#if !LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK
  struct sigaction sa {};
  sa.sa_sigaction = pCast<void (*)(int32_t, siginfo_t *, void *)>(memorySignalHandler);
  constexpr uint32_t sa_flags_basic{static_cast<uint32_t>(SA_SIGINFO) | static_cast<uint32_t>(SA_NODEFER)};
#if ACTIVE_STACK_OVERFLOW_CHECK
  sa.sa_flags = static_cast<int32_t>(sa_flags_basic);
#else
  constexpr uint32_t sa_flags{static_cast<uint32_t>(SA_ONSTACK) | sa_flags_basic};
  sa.sa_flags = static_cast<int32_t>(sa_flags);
#endif
  int32_t error{sigfillset(&sa.sa_mask)};
  checkSysCallReturn("RAIISignalHandler::sigfillset", error);
  struct sigaction oldSEGV {};
  error = sigaction(SIGSEGV, &sa, &oldSEGV);
  checkSysCallReturn("RAIISignalHandler::sigaction SIGSEGV", error);
#endif

#if !ACTIVE_DIV_CHECK
  struct sigaction oldFPE {};
  sa.sa_sigaction = pCast<void (*)(int32_t, siginfo_t *, void *)>(divSignalHandler);
  error = sigaction(SIGFPE, &sa, &oldFPE);
  checkSysCallReturn("RAIISignalHandler::sigaction SIGFPE", error);
#endif

#if !LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK
  if (oldSEGV.sa_sigaction != memorySignalHandler) {
    saSEGVOld = oldSEGV;
  }
#endif

#if !ACTIVE_DIV_CHECK
  if (oldFPE.sa_sigaction != divSignalHandler) {
    saSIGFPEOld = oldFPE;
  }
#endif

#ifdef __APPLE__
#if !LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK
  struct sigaction oldSIGBUS {};
  sa.sa_sigaction = pCast<void (*)(int32_t, siginfo_t *, void *)>(memorySignalHandler);
  int32_t const errorMac{sigaction(SIGBUS, &sa, &oldSIGBUS)};
  checkSysCallReturn("RAIISignalHandler::sigaction SIGBUS", errorMac);
  if (oldSIGBUS.sa_sigaction != memorySignalHandler) {
    saSIGBUSOld = oldSIGBUS;
  }
#endif
#endif
#endif
}

void RAIISignalHandler::unsetSignalHandler() VB_NOEXCEPT {
  std::lock_guard<std::mutex> const lock{handlerMutex};
  runningCounter--;

  if (runningCounter == 0U) {
    restoreSignalHandler();
  }
}

// coverity[autosar_cpp14_m0_1_8_violation]
void RAIISignalHandler::restoreSignalHandler() VB_NOEXCEPT {
#ifdef VB_WIN32
#if !LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK
  RemoveVectoredExceptionHandler(handleMem);
#endif
#if !ACTIVE_DIV_CHECK
  RemoveVectoredExceptionHandler(handleDiv);
#endif
#else
#if !LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK
  int32_t const errSEGV{sigaction(SIGSEGV, &saSEGVOld, nullptr)};
  static_cast<void>(errSEGV);
#endif
#if !ACTIVE_DIV_CHECK
  int32_t const errSIGFPE{sigaction(SIGFPE, &saSIGFPEOld, nullptr)};
  static_cast<void>(errSIGFPE);
#endif
#ifdef __APPLE__
#if !LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK
  int32_t const errSIGBUS{sigaction(SIGBUS, &saSIGBUSOld, nullptr)};
  static_cast<void>(errSIGBUS);
#endif
#endif
#endif
}

} // namespace vb
