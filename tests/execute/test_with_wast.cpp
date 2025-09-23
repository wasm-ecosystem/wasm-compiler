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

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "TestHelper.hpp"
#include "tests/SingleCaseTest.hpp"
#include "tests/loader/json_loader.hpp"

namespace vb {

TestData loadWasmFile(std::filesystem::path &path) {
  std::ifstream wasmFile{path, std::ios_base::binary};
  wasmFile.exceptions(
      static_cast<std::ios_base::iostate>(static_cast<uint32_t>(std::ifstream::failbit) | static_cast<uint32_t>(std::ifstream::badbit)));
  std::ostringstream oss;
  oss << wasmFile.rdbuf();
  const std::string wasmMemWriter(oss.str());
  return TestData(wasmMemWriter.data(), wasmMemWriter.size());
}

// NOLINTNEXTLINE(cert-err58-cpp)
const std::set<std::string> BlackList{
    "linking.wast",
};

TestResult processWast(std::filesystem::path const &base, std::filesystem::path const &wastPath, bool const enableDebugMode,
                       bool const enableStacktrace, bool const forceHighRegisterPressure) {
  std::string const name = std::filesystem::relative(wastPath, base).string();
  if (BlackList.find(wastPath.filename().string()) != BlackList.end()) {
    std::cout << "Skipping test: " << name << "\n";
    return TestResult();
  }
  std::cout << "Executing spectest: " << name << "\n";

  std::filesystem::path tempDirPath = std::filesystem::temp_directory_path();
  tempDirPath /= "vb_spectest";
  create_directory(tempDirPath);

  std::filesystem::path testJsonPath = tempDirPath;
  testJsonPath /= wastPath.stem();
  testJsonPath += ".json";

  std::ostringstream shellCommand;
  shellCommand << "wast2json -o " << testJsonPath.string() << " " << wastPath.string();

  int const res = system(shellCommand.str().c_str());
  if (res != 0) {
    throw std::runtime_error("wast2json failed");
  }

  nlohmann::json testJson;
  {
    std::ifstream testJsonStream{testJsonPath.string()};
    testJsonStream.exceptions(
        static_cast<std::ios_base::iostate>(static_cast<uint32_t>(std::ifstream::failbit) | static_cast<uint32_t>(std::ifstream::badbit)));
    testJsonStream >> testJson;
  }

  SingleCaseTest testcase(wastPath.filename().string());

  auto wasmBinaryMapping = std::make_shared<vb::TestDataMapping>();
  for (auto &command : testJson["commands"]) {
    std::string const type = command["type"];
    if (type == "module" || type == "assert_invalid" || type == "assert_malformed") {
      std::string const filename = command["filename"];
      std::filesystem::path fullFilePath = tempDirPath;
      fullFilePath /= filename;
      if (fullFilePath.extension() != std::filesystem::path(".wasm")) {
        // Skip anything not a .wasm
        continue;
      }
      TestData data = loadWasmFile(fullFilePath);
      wasmBinaryMapping->insert(std::make_pair(filename, std::move(data)));
    }
  }

  Json::JsonTestLoader loader{testJson, wasmBinaryMapping};
  TestResult const testResult = testcase.testFromStream(&loader, enableDebugMode, enableStacktrace, forceHighRegisterPressure);
  std::filesystem::remove_all(tempDirPath);
  return testResult;
}
} // namespace vb

vb::TestResult runTests(char const *const path, bool const enableDebugMode, bool const enableStacktrace, bool const forceHighRegisterPressure) {
  vb::TestResult totalTestResult;
  std::filesystem::path const root = std::filesystem::path(path);
  try {
    std::vector<std::filesystem::path> workList{root};
    while (!workList.empty()) {
      std::filesystem::path const currentPath = workList.back();
      workList.pop_back();
      if (!std::filesystem::exists(currentPath)) {
        std::cout << currentPath << " does not exist" << std::endl;
        std::terminate();
      }
      if (std::filesystem::is_directory(currentPath)) {
        auto it = std::filesystem::directory_iterator(currentPath);
        for (auto &x : it) {
          std::filesystem::path const p = x.path();
          if (x.is_directory() && p.parent_path().stem() == "proposals") {
            continue;
          }
          workList.push_back(p);
        }
      } else if (std::filesystem::is_regular_file(currentPath)) {
        if (currentPath.extension() == std::filesystem::path(".wast")) {
          vb::TestResult const testResult =
              vb::processWast(root, std::filesystem::canonical(currentPath), enableDebugMode, enableStacktrace, forceHighRegisterPressure);
          totalTestResult += testResult;
        }
      }
    }
  } catch (const std::filesystem::filesystem_error &ex) {
    std::cout << ex.what() << '\n';
  }
  return totalTestResult;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cout << "No directory specified. Aborting.\n";
    return 1;
  }

  vb::TestHelper<char const *> const testHelper(runTests);
  uint32_t const totalFailedTests = testHelper.runAllTests(argv[1]);
  return static_cast<int>(totalFailedTests);
}
