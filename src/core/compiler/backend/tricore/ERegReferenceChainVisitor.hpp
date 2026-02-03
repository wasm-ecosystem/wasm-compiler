///
/// @file ERegReferenceChainVisitor.hpp
/// @copyright Copyright (C) 2025 Wasm ecosystem contributors
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
#ifndef VB_E_REG_REFERENCE_CHAIN_VISITORS_HPP
#define VB_E_REG_REFERENCE_CHAIN_VISITORS_HPP

#include "src/core/compiler/common/MachineType.hpp"
#include "src/core/compiler/common/ModuleInfo.hpp"
#include "src/core/compiler/common/ReferenceChainVisitor.hpp"
#include "src/core/compiler/common/Stack.hpp"
#include "src/core/compiler/common/util.hpp"

namespace vb {

namespace tc {

/// @brief TriCore visitor that only considers 64-bit occurrences.
class ERegReferenceChainVisitor final : public ReferenceChainVisitor<ERegReferenceChainVisitor> {
public:
  /// @brief Construct with module info used to infer machine types.
  /// @param moduleInfo Module info used for type queries.
  explicit inline ERegReferenceChainVisitor(ModuleInfo const &moduleInfo) VB_NOEXCEPT : moduleInfo_(moduleInfo) {
  }

  /// @brief Visit only 64-bit occurrences.
  /// @param occurrence Current occurrence.
  // coverity[autosar_cpp14_a10_2_1_violation]
  inline bool shouldVisit(Stack::iterator const occurrence) const VB_NOEXCEPT {
    MachineType const machineType{moduleInfo_.getMachineType(occurrence.raw())};
    return MachineTypeUtil::is64(machineType);
  }

private:
  ModuleInfo const &moduleInfo_; ///< Module info for type queries
};
} // namespace tc
} // namespace vb

#endif
