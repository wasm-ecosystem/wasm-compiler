///
/// @file DwarfDebugInfo.hpp
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
#ifndef SRC_CORE_COMPILER_ANALYTICS_DWARF_DEBUG_INFO_HPP
#define SRC_CORE_COMPILER_ANALYTICS_DWARF_DEBUG_INFO_HPP

#include <cstdint>
#include <vector>

namespace vb {
namespace extension {

/// @brief information needed by debug info section
struct DebugInfo final {
  /// @brief function information for DWARF debug info
  struct Function {
    uint32_t lowPC;  ///< Low program counter (start address)
    uint32_t highPC; ///< High program counter (end address)
  };
  std::vector<Function> functions_; ///< List of functions with their address ranges
};

} // namespace extension
} // namespace vb

#endif // SRC_CORE_COMPILER_ANALYTICS_DWARF_DEBUG_INFO_HPP
