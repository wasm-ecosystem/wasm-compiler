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

#include <cstdint>
#include <iostream>

#include "tests/SingleCaseTest.hpp"
#include "tests/loader/stream_loader.hpp"

namespace vb {

vb::TestResult runTest(void const *const data, uint32_t length, bool const enableDebugMode, bool const enableStacktrace,
                       bool const forceHighRegisterPressure) {
  vb::TestResult testResult{};
  void const *testcasePtr = (data);
  while ((static_cast<uint8_t const *>(testcasePtr) - static_cast<uint8_t const *>(data)) != static_cast<std::ptrdiff_t>(length)) {
    vb::stream::StreamTestLoader loader{static_cast<uint8_t const *>(testcasePtr)};
    std::cout << "Executing spectest: " << loader.getTestcaseName() << "\n";
    vb::SingleCaseTest testcase{loader.getTestcaseName()};
    testResult += testcase.testFromStream(&loader, enableDebugMode, enableStacktrace, forceHighRegisterPressure);
    testcasePtr = loader.getNextTestcase();
  }
  return testResult;
}

} // namespace vb
