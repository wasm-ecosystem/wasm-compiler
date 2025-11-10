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

#ifndef TEST_HELPER
#define TEST_HELPER

#include <cstdint>
#include <iostream>

#include "tests/SingleCaseTest.hpp"

#include "src/WasmModule/WasmModule.hpp"

namespace vb {

template <class... ArgTypes> class TestHelper final {
public:
  using RunTestType = TestResult (*)(ArgTypes... args, bool const enableDebugMode, bool const enableStacktrace, bool const forceHighRegisterPressure);
  explicit TestHelper(RunTestType runTestImpl) : runTestImpl_(runTestImpl) {
  }

  uint32_t runSingleTest(ArgTypes... args, bool const enableDebugMode, bool const enableStacktrace, bool const forceHighRegisterPressure) const {
    std::cout << "============= TESTING (stacktrace=" << enableStacktrace << ", debug=" << enableDebugMode
              << ", forceHighRegisterPressure=" << forceHighRegisterPressure << ")" << std::endl;
    TestResult const totalTestResult = runTestImpl_(args..., enableDebugMode, enableStacktrace, forceHighRegisterPressure);
    std::cout << totalTestResult << std::endl;
    return totalTestResult.failedTests;
  }

  uint32_t runAllTests(ArgTypes... args) const {
    uint32_t totalFailedTests = 0;
    totalFailedTests += runSingleTest(args..., false, false, false);

#ifdef TEST_VARIANTS
    std::cout << std::endl;
    totalFailedTests += runSingleTest(args..., false, false, true);
    std::cout << std::endl;
    totalFailedTests += runSingleTest(args..., false, true, true);
    std::cout << std::endl;
    totalFailedTests += runSingleTest(args..., false, true, false);
#ifdef VB_WIN32_OR_POSIX
    std::cout << std::endl;
    totalFailedTests += runSingleTest(args..., true, false, false);
    std::cout << std::endl;
    totalFailedTests += runSingleTest(args..., true, true, false);
#endif
#endif

    std::cout << totalFailedTests << " tests failed total" << std::endl;

    return totalFailedTests;
  }

private:
  RunTestType runTestImpl_;
};

} // namespace vb

#endif
