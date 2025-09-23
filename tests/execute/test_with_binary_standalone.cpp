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
#include <cstdlib>

#include "RunBinaryTestCase.hpp"
#include "tests/SingleCaseTest.hpp"
#include "tests/execute/TestHelper.hpp"
#include "tests/loader/stream_loader.hpp"

extern void const *pTestcase;
extern size_t testcaseSize;

extern "C" {
uint8_t TEST_DONE = 0;
}

int main() {
  vb::TestHelper<void const *, uint32_t> const testHelper(vb::runTest);
  uint32_t const totalFailedTests = testHelper.runAllTests(pTestcase, static_cast<uint32_t>(testcaseSize));
  TEST_DONE = 1;
  return static_cast<int>(totalFailedTests);
}
