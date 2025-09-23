///
/// @file DwarfDebugLine.hpp
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
#ifndef EXTENSIONS_DWARF_DEBUG_LINE_HPP
#define EXTENSIONS_DWARF_DEBUG_LINE_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace vb {
namespace extension {

/// @brief DebugLineOpCode
struct DebugLineOpCode {
  /// @brief OpCodeKind
  enum class OpCodeKind : uint8_t {
    advance_pc,
    advance_line,
    copy,
  };
  OpCodeKind kind_; ///< kind of op code
  /// @brief AdvancePC
  struct AdvancePC {
    uint32_t offset_; ///< offset in bytes
  };
  /// @brief AdvanceLine
  struct AdvanceLine {
    int32_t offset_; ///< offset in bytes
  };
  /// @brief Copy
  struct Copy {};
  /// @brief DebugLineOpCodeUnion
  union DebugLineOpCodeUnion {
    AdvancePC advancePC_;     ///< DW_LNS_advance_pc
    AdvanceLine advanceLine_; ///< DW_LNS_advance_line
    Copy copy_;               ///< DW_LNS_copy
  };

  DebugLineOpCodeUnion v_; ///< union of debug line op codes
};

/// @brief DebugLine
struct DebugLine {
  std::string fileName_;
  std::vector<DebugLineOpCode> sequences_; ///< sequence of op codes
};

} // namespace extension
} // namespace vb

#endif // EXTENSIONS_DWARF_DEBUG_LINE_HPP
