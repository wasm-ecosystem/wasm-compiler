///
/// @file OSAPIChecker.cpp
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
#include <cstdint>
// coverity[autosar_cpp14_m27_0_1_violation] Need perror
#include <cstdio>
#include <iostream>
#include <ostream>

#include "OSAPIChecker.hpp"

#include "src/core/common/VbExceptions.hpp"

namespace vb {

void checkSysCallReturn(const char *const msg, int32_t const errorCode) {
  if (errorCode != 0) {
    perror(msg);
    std::cout << "error code " << errorCode << &std::endl;
    throw vb::RuntimeError(ErrorCode::Syscall_failed);
  }
}

} // namespace vb
