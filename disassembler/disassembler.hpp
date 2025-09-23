///
/// @file disassembler.hpp
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

#ifndef DISASSEMBLER_HPP
#define DISASSEMBLER_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vb {
namespace disassembler {

std::string getConfiguration();

std::string disassemble(uint8_t const *const binaryData, size_t const binarySize, std::vector<uint32_t> const &instructionAddresses);
template <typename Binary> static std::string disassemble(Binary const &binary, std::vector<uint32_t> const &instructionAddresses) {
  return disassemble(binary.data(), binary.size(), instructionAddresses);
}

std::string disassembleDebugMap(uint8_t const *const binaryData, size_t const binarySize);
template <typename Binary> static std::string disassembleDebugMap(Binary const &binary) {
  return disassembleDebugMap(binary.data(), binary.size());
}

} // namespace disassembler
} // namespace vb

#endif
