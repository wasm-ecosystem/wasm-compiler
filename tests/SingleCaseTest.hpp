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

#ifndef TESTS_SINGLE_CASE_TEST
#define TESTS_SINGLE_CASE_TEST

#include <cstdint>
#include <ostream>

#include "tests/loader/test_loader.hpp"
#include "tests/testimports.hpp"

#include "src/core/common/TrapCode.hpp"

namespace vb {
struct TestResult {
  uint32_t executedTests = 0;
  uint32_t failedTests = 0;
  uint32_t totalTests = 0;
  TestResult &operator+=(TestResult const &rh) noexcept;
  friend std::ostream &operator<<(std::ostream &outputStream, TestResult const &v);
};

class SingleCaseTest {
public:
  explicit SingleCaseTest(std::string const &_testcaseName);
  SingleCaseTest(SingleCaseTest const &) = default;
  SingleCaseTest(SingleCaseTest &&) = default;
  SingleCaseTest &operator=(SingleCaseTest const &) = delete;
  SingleCaseTest &operator=(SingleCaseTest &&) = delete;
  ~SingleCaseTest() = default;

  TestResult testFromStream(TestLoader *loader, bool const enableDebugMode, bool const enableStacktrace, bool const forceHighRegisterPressure);

private:
  static TrapCode getTrapCodeFromTrapText(std::string const &text);
  static bool isExpectedTrap(TrapCode const trapCode1, TrapCode const trapCode2) noexcept;

  void testFailed(uint32_t line, std::string const &message);

  void testCommand(std::string const &type, uint32_t const line);

  std::string testcaseName;

  TestResult testResult{};
  decltype(spectest::ImportsMaker::makeImports()) spectestImports{spectest::ImportsMaker::makeImports()};
};

} // namespace vb

#endif
