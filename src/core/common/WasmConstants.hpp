///
/// @file WasmConstants.hpp
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
#ifndef WASMCONSTANTS_HPP
#define WASMCONSTANTS_HPP

#include <cstdint>

#include "src/core/common/util.hpp"
namespace vb {
/// @brief Wasm memory related constants
class WasmConstants final {
public:
  static constexpr uint32_t wasmPageSize{1_U32 << 16_U32};                                           ///< Wasm page size
  static constexpr uint32_t maxWasmPages{1_U32 << 16_U32};                                           ///< Max Wasm pages count
  static constexpr uint64_t maxLinearMemorySize{static_cast<uint64_t>(wasmPageSize) * maxWasmPages}; ///< Max linear memory size
  static constexpr uint64_t maxLinearMemoryOffset{1_U64 << 32_U64}; ///< Max Linear memory offset which can a wasm32 instruction access
};
} // namespace vb

#endif
