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

#ifndef TESTS_TEST_DATA
#define TESTS_TEST_DATA

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "src/core/common/Span.hpp"

namespace vb {
struct TestData {
  std::vector<uint8_t> p_data;
  Span<const uint8_t> m_memObj;
  TestData(const void *data, std::size_t len);
};

using TestDataMapping = std::map<std::string, TestData>;

} // namespace vb

#endif
