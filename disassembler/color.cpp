/*
 * Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cassert>
#include <ostream>

#include "disassembler/color.hpp"

#if _WIN32
#include <io.h>
#define ISATTY _isatty
#define FILENO _fileno
#else
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#endif

namespace vb {
namespace disassembler {

inline static bool isTty() noexcept {
  static const bool isTty = 0 != ISATTY(FILENO(stdout));
  return isTty;
}

bool useColor = isTty();

std::ostream &operator<<(std::ostream &os, const TtyControl color) {
  if (useColor) {
    constexpr const char *Reset = "\033[0m";
    constexpr const char *Dim = "\033[2m";
    constexpr const char *UnderLine = "\033[4m";
    constexpr const char *ConsoleGreen = "\033[32m";
    constexpr const char *ConsoleBlue = "\033[34m";
    switch (color) {
    case TtyControl::Reset:
      os << Reset;
      break;
    case TtyControl::Dim:
      os << Dim;
      break;
    case TtyControl::UnderLine:
      os << UnderLine;
      break;
    case TtyControl::Green:
      os << ConsoleGreen;
      break;
    case TtyControl::Blue:
      os << ConsoleBlue;
      break;
    default:
      assert(false && "no known");
    }
  }
  return os;
}

} // namespace disassembler
} // namespace vb
