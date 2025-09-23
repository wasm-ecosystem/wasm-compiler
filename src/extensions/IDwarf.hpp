///
/// @file IDwarf.hpp
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
#ifndef SRC_CORE_COMPILER_EXTENSIONS_IDWARF_HPP
#define SRC_CORE_COMPILER_EXTENSIONS_IDWARF_HPP

#include <cstdint>

#include "src/core/compiler/common/StackElement.hpp"
#include "src/core/compiler/common/StackType.hpp"

namespace vb {
namespace extension {

/// @brief abstract interface for DWARF5 generator in WARP
class IDwarf5Generator {
public:
  /// @brief virtual destructor for IDwarf5Generator
  virtual ~IDwarf5Generator() = default;

  /// @brief prepare defer action for later usage
  virtual void registerPendingDeferAction(StackElement const *stackElement, uint32_t const sourceOffset) = 0;

  /// @brief start code gen for stack element which must be prepared before
  virtual void startOp(StackElement const *stackElement) = 0;
  /// @brief start code gen for op
  virtual void startOp(uint32_t const sourceOffset) = 0;
  /// @brief finish code gen for op
  virtual void finishOp() = 0;

  /// @brief record a location in the debug info
  virtual void record(uint32_t const destinationOffset) = 0;

  /// @brief start code gen for function
  virtual void startFunction(uint32_t const destinationOffset) = 0;
  /// @brief finish code gen for function
  virtual void finishFunction(uint32_t const destinationOffset) = 0;
};

} // namespace extension
} // namespace vb

#endif // SRC_CORE_COMPILER_EXTENSIONS_IDWARF_HPP
