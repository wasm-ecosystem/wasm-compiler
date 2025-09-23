/*
 * Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <array>
#include <cstdint>

#include "base64.hpp"

namespace Base64 {
// const char encodeMap[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
constexpr std::array<uint32_t, 256> decodeMap = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  62, 0,  0,  0,  63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0, 0, 0,  0,  0,  0,  0,  0,  1,  2,  3,  4,  5,  6,  7,  8,
    9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0,  0,  0,  0,  0, 0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
    40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0};

uint16_t get_shl(char index, uint32_t offset) {
  return static_cast<uint16_t>(decodeMap[static_cast<uint8_t>(index)] << offset);
}
uint16_t get_shr(char index, uint32_t offset) {
  return static_cast<uint16_t>(decodeMap[static_cast<uint8_t>(index)] >> offset);
}

std::vector<uint8_t> b64decode(std::string const &str) {
  if (str.empty()) {
    return std::vector<uint8_t>{};
  }
  std::size_t i = 0;
  std::size_t const k = str.length() / 4;
  std::vector<uint8_t> decodeData{};
  decodeData.reserve(k * 3);
  for (; static_cast<int>(i) < static_cast<int>(k) - 1; i++) {
    // d1
    decodeData.push_back(static_cast<uint8_t>(get_shl(str[i * 4], 2) + get_shr(str[i * 4 + 1], 4)));
    // d2
    decodeData.push_back(static_cast<uint8_t>(get_shl(str[i * 4 + 1], 4) + get_shr(str[i * 4 + 2], 2)));
    // d3
    decodeData.push_back(static_cast<uint8_t>(get_shl(str[i * 4 + 2], 6) + get_shr(str[i * 4 + 3], 0)));
  }
  // d1
  if (str[i * 4 + 1] == '=') {
    return decodeData;
  }
  decodeData.push_back(static_cast<uint8_t>(get_shl(str[i * 4], 2) + get_shr(str[i * 4 + 1], 4)));
  // d2
  if (str[i * 4 + 2] == '=') {
    return decodeData;
  }
  decodeData.push_back(static_cast<uint8_t>(get_shl(str[i * 4 + 1], 4) + get_shr(str[i * 4 + 2], 2)));
  // d3
  if (str[i * 4 + 3] == '=') {
    return decodeData;
  }
  decodeData.push_back(static_cast<uint8_t>(get_shl(str[i * 4 + 2], 6) + get_shr(str[i * 4 + 3], 0)));

  return decodeData;
}
} // namespace Base64
