///
/// @file implementationlimits.hpp
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
#ifndef IMPLEMENTATIONLIMITS_HPP
#define IMPLEMENTATIONLIMITS_HPP

#include <cstdint>

namespace vb {

/// @brief list of implementation limit of this compiler. It's usually due to instruction encoding.
class ImplementationLimits final {
public:
  /// @brief Maximum number of parameters for a Wasm (or imported) function
  static constexpr uint32_t numParams{1_U32 << 7_U32};

  /// @brief Maximum number of results for a Wasm (or imported) function
  static constexpr uint32_t numResults{1_U32 << 7_U32};

  /// @brief Maximum number of local variables (excluding the parameters) in a Wasm function
  static constexpr uint32_t numDirectLocals{1_U32 << 16_U32};

  // constexpr uint32_t numImportedGlobals = 0; // Not supported

  /// @brief Maximum number of (non-imported) global variables in a Wasm module
  static constexpr uint32_t numNonImportedGlobals{1_U32 << 16_U32};

  /// @brief Maximum number of imported functions
  static constexpr uint32_t numImportedFunctions{1_U32 << 16_U32};

  /// @brief Maximum number of non imported functions, i.e. those that are defined in the Wasm module
  static constexpr uint32_t numNonImportedFunctions{1_U32 << 20_U32};

  /// @brief Maximum number of different function signatures (types) used in the Wasm module
  static constexpr uint32_t numTypes{1_U32 << 20_U32};

  /// @brief Maximum length of branch targets in a br_table instruction
  static constexpr uint32_t branchTableLength{1_U32 << 20_U32};

  /// @brief Maximum length of a Wasm table
  static constexpr uint32_t numTableEntries{1_U32 << 20_U32};

  /// @brief Maximum length of strings (module names, function names etc.) in a Wasm module
  static constexpr uint32_t maxStringLength{static_cast<uint32_t>(INT32_MAX)};

  /// @brief Maximum size of a single function's stack frame in bytes
  static constexpr uint32_t maxStackFrameSize{1_U32 << 23_U32};
};

} // namespace vb

#endif
