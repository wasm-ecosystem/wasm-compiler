///
/// @file ReferenceChainVisitor.hpp
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
#ifndef VB_REFERENCE_CHAIN_VISITOR_HPP
#define VB_REFERENCE_CHAIN_VISITOR_HPP

#include <type_traits>

#include "Stack.hpp"

#include "src/config.hpp"
#include "src/core/common/FunctionRef.hpp"

namespace vb {

/// @brief Compile-time polymorphism base for visiting a reference chain on the stack.
/// @tparam Sub Concrete visitor type providing shouldVisit().
// coverity[autosar_cpp14_a14_1_1_violation]
template <typename Sub> class ReferenceChainVisitor {
public:
  ReferenceChainVisitor() = default;

  /// @brief Walk the reference chain and invoke fn on each visited occurrence.
  /// @param start Start of the reference chain.
  /// @param fn Callable invoked for each visited occurrence; return false to stop.
  inline void walk(Stack::iterator const start, FunctionRef<bool(Stack::iterator const &)> const &fn) const VB_NOEXCEPT {
    static_assert(std::is_base_of<ReferenceChainVisitor<Sub>, Sub>::value, "Sub must derive from ReferenceChainVisitor<Sub>");
    Sub const &self{static_cast<Sub const &>(*this)};
    Stack::iterator cursor{start};
    while (!cursor.isEmpty()) {
      if (self.shouldVisit(cursor)) {
        bool const continueVisiting{fn(cursor)};
        if (!continueVisiting) {
          break;
        }
      }
      cursor = cursor->data.variableData.indexData.prevOccurrence;
    }
  }

  /// @brief By default visit all occurrences.
  /// @param occurrence Current occurrence.
  // coverity[autosar_cpp14_a10_2_1_violation]
  inline bool shouldVisit(Stack::iterator const occurrence) const VB_NOEXCEPT {
    static_cast<void>(occurrence);
    return true;
  }

protected:
  ReferenceChainVisitor(ReferenceChainVisitor const &) = delete;
  ReferenceChainVisitor &operator=(ReferenceChainVisitor const &) & = delete;
  ReferenceChainVisitor(ReferenceChainVisitor &&) = delete;
  ReferenceChainVisitor &operator=(ReferenceChainVisitor &&) & = delete;
  ~ReferenceChainVisitor() = default;
};

/// @brief Default visitor that visits all occurrences.
class BasicReferenceChainVisitor final : public ReferenceChainVisitor<BasicReferenceChainVisitor> {
public:
  BasicReferenceChainVisitor() = default;
  ~BasicReferenceChainVisitor() = default;

protected:
  BasicReferenceChainVisitor(BasicReferenceChainVisitor const &) = delete;
  BasicReferenceChainVisitor &operator=(BasicReferenceChainVisitor const &) & = delete;
  BasicReferenceChainVisitor(BasicReferenceChainVisitor &&) = delete;
  BasicReferenceChainVisitor &operator=(BasicReferenceChainVisitor &&) & = delete;
};

/// @brief Default reference chain visitor alias.
using DefaultReferenceChainVisitor = BasicReferenceChainVisitor;

} // namespace vb

#endif
