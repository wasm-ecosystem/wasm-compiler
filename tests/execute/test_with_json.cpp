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

#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>

#include "tests/SingleCaseTest.hpp"
#include "tests/base64.hpp"
#include "tests/execute/TestHelper.hpp"
#include "tests/loader/json_loader.hpp"

vb::TestResult runTests(char const *const path, bool const enableDebugMode, bool const enableStacktrace, bool const forceHighRegisterPressure) {
  vb::TestResult totalTestResult;

  std::ifstream testcasesStream{path};
  testcasesStream.exceptions(
      static_cast<std::ios_base::iostate>(static_cast<uint32_t>(std::ifstream::failbit) | static_cast<uint32_t>(std::ifstream::badbit)));
  nlohmann::json testcasesJson;
  testcasesStream >> testcasesJson;
  for (auto testcaseIt = testcasesJson.begin(); testcaseIt != testcasesJson.end(); testcaseIt++) {
    if (testcaseIt.key() == "linking.wast") {
      std::cout << "Skipping test: " << testcaseIt.key() << "\n";
      continue;
    }
    std::cout << "Executing spectest: " << testcaseIt.key() << "\n";

    vb::SingleCaseTest testcase{testcaseIt.key()};
    auto wasmBinaryMapping = std::make_shared<vb::TestDataMapping>();
    for (auto wasmBinaryIt = testcaseIt->begin(); wasmBinaryIt != testcaseIt->end(); wasmBinaryIt++) {
      if (wasmBinaryIt.key() == "wast_json") {
        continue;
      }
      std::vector<uint8_t> wasmBinary = Base64::b64decode(static_cast<std::string>(*wasmBinaryIt));
      vb::TestData data{wasmBinary.data(), wasmBinary.size()};
      wasmBinaryMapping->insert(std::make_pair(wasmBinaryIt.key(), std::move(data)));
    }
    vb::Json::JsonTestLoader loader{testcaseIt->at("wast_json"), wasmBinaryMapping};
    vb::TestResult const testResult = testcase.testFromStream(&loader, enableDebugMode, enableStacktrace, forceHighRegisterPressure);
    totalTestResult += testResult;
  }
  return totalTestResult;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cout << "No directory specified. Aborting.\n";
    std::exit(0);
  }

  vb::TestHelper<char const *> const testHelper(runTests);
  uint32_t const totalFailedTests = testHelper.runAllTests(argv[1]);
  return static_cast<int>(totalFailedTests);
}
