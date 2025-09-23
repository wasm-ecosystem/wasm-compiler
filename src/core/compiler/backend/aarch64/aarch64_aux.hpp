///
/// @file aarch64_aux.hpp
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
#ifndef AARCH64_AUX_HPP
#define AARCH64_AUX_HPP

#include <cstdint>

#include "src/config.hpp"

namespace vb {
namespace aarch64 {

///
/// @brief Determines if an immediate value can be encoded as the immediate operand of a logical instruction for the
/// given register size
///
/// @param imm Immediate value to encode
/// @param is64 Whether the register is a 64-bit register (otherwise a 32-bit register is assumed)
/// @param encoding Sets this lvalue to the encoded value in the form N:immr:imms if encoding is possible
/// @return bool Whether the immediate can be encoded this way
bool processLogicalImmediate(uint64_t imm, bool const is64, uint64_t &encoding) VB_NOEXCEPT;

} // namespace aarch64
} // namespace vb

#endif
