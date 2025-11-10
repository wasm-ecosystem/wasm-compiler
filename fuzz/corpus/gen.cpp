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
namespace fs = std::filesystem;

#include <cxxopts.hpp>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

void generateBinary(fs::path const seedFilePath, fs::path const fuzzWasmFilePath) {
  std::ostringstream shellCommandGenerate;
  shellCommandGenerate << "wasm-opt " << seedFilePath.string() << " -ttf --enable-multivalue -O2 -o " << fuzzWasmFilePath.string();
  int const res = system(shellCommandGenerate.str().c_str());

  if (res == -1) {
    std::cout << "run command failed: " << shellCommandGenerate.str() << std::endl;
    exit(-1);
  }
}

std::string random_string(size_t length, std::function<char()> randChar) {
  std::string str(length, 0);
  std::generate_n(str.begin(), length, randChar);
  return str;
}

std::string const identifier = "vb_fuzz_afl"; // NOLINT(cert-err58-cpp)
int main(int argc, char *argv[]) {
  int numberOfRandomTestcasesToGenerate = 0;

  // Declare the supported options.
  cxxopts::Options op("test", "A brief description");
  // clang-format off
  op.add_options()("n", "Number of random test cases to generate, ", cxxopts::value<int>()->default_value("1000"))("t,tests", "Spectest directory", cxxopts::value<std::string>())(
      "o,output", "Output directory", cxxopts::value<std::string>())("h,help", "Show this message");
  // clang-format on

  auto result = op.parse(argc, argv);

  if (result.count("help") != 0U) {
    std::cout << op.help() << "\n";
    return 1;
  }

  fs::path outputPath;
  if (result.count("output") != 0U) {
    outputPath = result["output"].as<std::string>();
    if (!(fs::is_directory(outputPath) && fs::exists(outputPath))) {
      std::cout << outputPath << " is not a directory or does not exist. Exiting.\n";
      exit(0);
    }
    if (!fs::is_empty(outputPath)) {
      std::cout << outputPath << " is not empty. Exiting.\n";
      exit(0);
    }
  } else {
    outputPath = fs::temp_directory_path() / identifier;
    std::cout << "No output directory specified. Using " << outputPath << "\n";
    create_directory(outputPath);
  }

  std::random_device rd;
  std::mt19937 mt(rd());
  std::uniform_int_distribution<int> dist(INT8_MIN, INT8_MAX);

  std::cout << "Generating fuzzing corpus in " << outputPath << "\n";

  if (result.count("n") > 0) {
    numberOfRandomTestcasesToGenerate = result["n"].as<int>();
  }

  fs::path const seedFilePath = outputPath / "seed.txt";
  for (int i = 0; i < numberOfRandomTestcasesToGenerate; i++) {
    std::cout << "Generating random test cases: (" << i << "/" << numberOfRandomTestcasesToGenerate << ") ...\r" << std::flush;

    std::string const seed = identifier + random_string(1000, [&]() {
                               return static_cast<char>(dist(mt));
                             });

    std::ofstream out = std::ofstream(seedFilePath.string());
    out << seed;
    out.close();

    generateBinary(seedFilePath, outputPath / ("case" + std::to_string(i) + ".wasm"));
  }
  std::cout << "Generating random test cases: (" << numberOfRandomTestcasesToGenerate << "/" << numberOfRandomTestcasesToGenerate << ") ... Done\n"
            << std::flush;
  fs::remove(seedFilePath);

  if (result.count("tests") != 0U) {
    fs::path const spectestDirectoryPath = result["tests"].as<std::string>();
    std::cout << "Generating corpus from spectest files in " << spectestDirectoryPath << "\n";
    if (!(fs::is_directory(spectestDirectoryPath) && fs::exists(spectestDirectoryPath))) {
      std::cout << spectestDirectoryPath << " is not a directory or does not exist. Exiting.\n";
      exit(0);
    }

    for (const fs::directory_entry &x : fs::directory_iterator(spectestDirectoryPath)) {
      fs::path const filePath = x.path();
      if (fs::is_regular_file(filePath)) {
        if (filePath.extension() == fs::path(".wast")) {
          fs::path const wastPath = fs::canonical(filePath);

          fs::path outputJSONPath = outputPath / wastPath.stem();
          outputJSONPath += ".json";

          std::cout << "Generating corpus from " << wastPath.filename() << "... ";
          std::ostringstream shellCommand;
          shellCommand << "wast2json --disable-bulk-memory -o " << outputJSONPath << " " << wastPath.string();
          [[maybe_unused]] int const res = system(shellCommand.str().c_str());
          std::cout << "Done\n";
          fs::remove(outputJSONPath);
        }
      }
    }

    std::cout << "Deleting wat files from output directory ... ";
    uint32_t numberOfOutputFiles = 0;
    for (const fs::directory_entry &x : fs::directory_iterator(outputPath)) {
      fs::path const filePath = x.path();
      if (filePath.extension() == ".wat") {
        fs::remove(filePath);
      } else {
        numberOfOutputFiles++;
      }
    }
    std::cout << "Done\n";
    std::cout << "Produced a corpus with a total of " << numberOfOutputFiles << " files.\n";
  } else {
    std::cout << "Directory with spectest files not given. Not generating.\n";
  }
}
