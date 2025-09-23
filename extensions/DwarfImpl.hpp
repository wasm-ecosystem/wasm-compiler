///
/// @file DwarfImpl.hpp
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
#ifndef EXTENSIONS_DWARF_IMPL_HPP
#define EXTENSIONS_DWARF_IMPL_HPP

#include <cstdint>
#include <iostream>
#include <map>
#include <ostream>
#include <stack>
#include <string>
#include <vector>

#include "extensions/DwarfDebugInfo.hpp"
#include "extensions/DwarfDebugLine.hpp"

#include "src/core/compiler/common/StackElement.hpp"
#include "src/extensions/IDwarf.hpp"

namespace vb {
namespace extension {

/// @brief Storage of DebugInfo
struct Storage {
  DebugLine debugLine_; ///< .debug_line section
  DebugInfo debugInfo_; ///< .debug_info section
};

/// @brief Dwarf5 Generator
class Dwarf5Generator final : public IDwarf5Generator {
private:
  Storage storage_{};                                            ///< storage for debug information
  uint32_t currentSourceOffset_ = 0U;                            ///< current source offset in wasm bytecode, used by DWARF state machine
  uint32_t currentDestinationOffset_ = 0U;                       ///< current destination offset in output binary, used by DWARF state machine
  std::map<StackElement const *, uint32_t> pendingDeferActions_; ///< mapping between defer actions and their source offsets
  std::stack<uint32_t> sourceOffsetStack_; ///< stack of source offsets, used to track the current source offset in the DWARF state machine

  /// @brief set sourceOffset and destinationOffset mapping in debug line section
  void addSourceDestinationMap(uint32_t const sourceOffset, uint32_t const destinationOffset);

public:
  /// @brief see @b IDwarf5Generator
  void registerPendingDeferAction(StackElement const *stackElement, uint32_t const sourceOffset) override;
  /// @brief see @b IDwarf5Generator
  void startOp(StackElement const *stackElement) override;
  /// @brief see @b IDwarf5Generator
  void startOp(uint32_t const sourceOffset) override;
  /// @brief see @b IDwarf5Generator
  void finishOp() override;
  /// @brief see @b IDwarf5Generator
  void record(uint32_t const destinationOffset) override;

  /// @brief see @b IDwarf5Generator
  void startFunction(uint32_t const destinationOffset) override;
  /// @brief see @b IDwarf5Generator
  void finishFunction(uint32_t const destinationOffset) override;

  /// @brief set file name in debug info
  void setWasmFileName(std::string fileName) {
    storage_.debugLine_.fileName_ = std::move(fileName);
  }

  /// @brief get bytes dwarf
  std::vector<uint8_t> toDwarfObject() const;
  /// @brief dump dwarf information
  void dump(std::ostream &os = std::cout) const;
  /// @brief get lists of all instructions
  std::vector<uint32_t> getInstructions() const;
};

} // namespace extension
} // namespace vb

#endif // EXTENSIONS_DWARF_IMPL_HPP
