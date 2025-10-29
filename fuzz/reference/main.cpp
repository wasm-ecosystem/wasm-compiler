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

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "src/WasmModule/WasmModule.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/function_traits.hpp"
#include "src/core/common/util.hpp"
#include "src/core/runtime/TrapException.hpp"
#include "src/utils/STDCompilerLogger.hpp"
#include "src/utils/StackTop.hpp"

#if ACTIVE_STACK_OVERFLOW_CHECK
#include "src/utils/StackTop.hpp"
#endif

#if __APPLE__
namespace fs = std::__fs::filesystem;
#else
namespace fs = std::filesystem;
#endif

bool isLogDetails = false;
bool isExitOnFirstError = false;

namespace {

uint32_t functionsExecuted = 0, failedExecutions = 0;
fs::path tempDirPath;
std::vector<std::string> deferredLines;
std::string seed;
bool next = false;
bool reproduceWithModule = false;
bool reproduceWithSeed = false;

int64_t timeTakenGeneratingBinary = 0;
int64_t timeTakenGeneratingReferenceOutput = 0;
int64_t timeTakenExecutingVB = 0;

struct FuzzPaths {
  fs::path fuzzWasmFilePath;
  fs::path referenceOutputFilePath;
};
FuzzPaths fuzzPaths;

struct ExpectedData {
  std::string type;
  std::string value;
  ExpectedData() = default;
  ExpectedData(std::string _type, std::string _value) : type(_type), value(_value) {
  }
};

} // namespace

void generateBinary(fs::path const seedFilePath, fs::path const fuzzWasmFilePath) {
  std::ostringstream shellCommandGenerate;
  shellCommandGenerate << "wasm-opt " << seedFilePath.string() << " -ttf --enable-multivalue  --enable-bulk-memory-opt  -O2 --denan -o "
                       << fuzzWasmFilePath.string();
  int const res = system(shellCommandGenerate.str().c_str());

  if (res == -1) {
    std::cout << "run command failed: " << shellCommandGenerate.str() << std::endl;
    std::exit(-1);
  }
  fuzzPaths.fuzzWasmFilePath = fuzzWasmFilePath;
}

void generateReferenceOutput(fs::path const fuzzWasmFilePath, fs::path const referenceOutputFilePath) {
  std::ostringstream shellCommandReferenceRun;
  shellCommandReferenceRun << "wasm-interp --run-all-exports --dummy-import-func " << fuzzWasmFilePath.string() << " > "
                           << referenceOutputFilePath.string();
  int const res = system(shellCommandReferenceRun.str().c_str());

  if (res == -1) {
    std::cout << "run command failed: " << shellCommandReferenceRun.str() << std::endl;
    std::exit(-1);
  }
  fuzzPaths.referenceOutputFilePath = referenceOutputFilePath;
}

void generate() {
  fs::path const seedFilePath = tempDirPath / "seed.txt";

  std::ofstream out = std::ofstream(seedFilePath.string());
  out << seed;
  out.close();

  fs::path const fuzzWasmFilePath = tempDirPath / "fuzz.wasm";
  auto const binaryClockStart = std::chrono::system_clock::now();
  generateBinary(seedFilePath, fuzzWasmFilePath);
  timeTakenGeneratingBinary += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now() - binaryClockStart).count();

  auto const referenceClockStart = std::chrono::system_clock::now();
  fs::path const referenceOutputFilePath = tempDirPath / "refOut.txt";
  generateReferenceOutput(fuzzWasmFilePath, referenceOutputFilePath);
  timeTakenGeneratingReferenceOutput +=
      std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now() - referenceClockStart).count();
}

static std::vector<uint8_t> loadWasmFile(char const *path) {
  std::ifstream jobStream{path, std::ios::binary};
  assert(jobStream.good());
  return std::vector<uint8_t>{std::istreambuf_iterator<char>{jobStream}, {}};
}

void executionFailed() {
  if (reproduceWithModule) {
    // execute --reproduceWithModule do not need store seeds
    std::cout << "Execution in reproduce mode failed" << std::endl;
    std::exit(-1);
  }

  static std::string lastFailedSeed{};

  if (seed == lastFailedSeed) {
    // avoid to store the same module multiple times.
    return;
  }
  lastFailedSeed = seed;

  failedExecutions++;

  fs::path const failedModulesDirectory = tempDirPath / "failedmodules";
  if (!fs::exists(failedModulesDirectory)) {
    fs::create_directory(failedModulesDirectory);
  }

  std::string newFileName;
  for (int i = 0; fs::exists(failedModulesDirectory / newFileName) && i < 100000; ++i) {
    newFileName = "fuzz_" + std::to_string(i) + ".wasm";
  }
  if (!isExitOnFirstError) {
    fs::copy_file(fuzzPaths.fuzzWasmFilePath, failedModulesDirectory / newFileName);
  }

  std::ofstream file;
  fs::path const failedSeedsPath = tempDirPath / "failedseeds.txt";
  std::cout << "Seed " << seed << " failed, will be written to " << failedSeedsPath << " (" << newFileName << ")\n\n";

  constexpr uint32_t mode = static_cast<uint32_t>(std::ios::out) | static_cast<uint32_t>(std::ios::app);
  file.open(failedSeedsPath.string(), static_cast<std::ios::openmode>(mode));
  if (file.fail()) {
    throw std::ios_base::failure(std::strerror(errno));
  }

  file.exceptions(static_cast<std::ios_base::iostate>(static_cast<uint32_t>(file.exceptions()) | static_cast<uint32_t>(std::ios::failbit) |
                                                      static_cast<uint32_t>(std::ifstream::badbit)));

  auto t = std::time(nullptr);
  auto tm = *std::localtime(&t);
  file << std::put_time(&tm, "%d-%m-%Y %H:%M:%S") << " (" << newFileName << ")\n" << seed << "\n\n";
  file.flush();
  file.close();

  next = true;

  if (isExitOnFirstError) {
    std::exit(-1);
  }
}

namespace FuzzingSupport {

void validateAndLogCall(std::string const &actualLine, std::string const &functionName) {
  if (deferredLines.size() == 0) {
    std::cout << "No log expected, " << functionName << " called: " << actualLine << "\n";
    executionFailed();
  } else if (deferredLines.front() != actualLine) {
    std::cout << "Error: " << functionName << " log mismatch\n";
    std::cout << "\"" << actualLine << "\" expected: \"" << deferredLines.front() << "\"\n";
    executionFailed();
  } else {
    deferredLines.erase(deferredLines.begin());
  }
  if (isLogDetails) {
    std::cout << actualLine << "\n";
  }
}

template <typename T> void logValueImpl(T value, std::string const &typeName) {
  std::string const actualLine = "called host fuzzing-support.log-" + typeName + "(" + typeName + ":" + std::to_string(value) + ") =>";
  validateAndLogCall(actualLine, "log-" + typeName);
}

void logI32(uint32_t value, void *const ctx) {
  static_cast<void>(ctx);
  logValueImpl(value, "i32");
}

void logI64(uint64_t value, void *const ctx) {
  static_cast<void>(ctx);
  logValueImpl(value, "i64");
}

void logF32(float value, void *const ctx) {
  static_cast<void>(ctx);
  logValueImpl(value, "f32");
}

void logF64(double value, void *const ctx) {
  static_cast<void>(ctx);
  logValueImpl(value, "f64");
}

void callExport(uint32_t param1, void *const ctx) {
  static_cast<void>(ctx);
  std::string const actualLine = "called host fuzzing-support.call-export(i32:" + std::to_string(param1) + ") =>";
  validateAndLogCall(actualLine, "call-export");
}

uint32_t sleep(uint32_t param1, uint32_t param2, void *const ctx) {
  static_cast<void>(ctx);
  std::string const actualLine = "called host fuzzing-support.sleep(i32:" + std::to_string(param1) + ", i32:" + std::to_string(param2) + ") => i32:0";
  validateAndLogCall(actualLine, "sleep");
  return 0;
}

uint32_t callExportCatch(uint32_t param1, void *const ctx) {
  static_cast<void>(ctx);
  std::string const actualLine = "called host fuzzing-support.call-export-catch(i32:" + std::to_string(param1) + ") => i32:0";
  validateAndLogCall(actualLine, "call-export-catch");
  return 0;
}

} // namespace FuzzingSupport

void fuzz() {
  std::vector<uint8_t> const bytecode = loadWasmFile(fuzzPaths.fuzzWasmFilePath.string().c_str());

  const auto staticallyLinkedSymbols = vb::make_array(
      DYNAMIC_LINK("fuzzing-support", "log-i32", FuzzingSupport::logI32), DYNAMIC_LINK("fuzzing-support", "log-i64", FuzzingSupport::logI64),
      DYNAMIC_LINK("fuzzing-support", "log-f32", FuzzingSupport::logF32), DYNAMIC_LINK("fuzzing-support", "log-f64", FuzzingSupport::logF64),
      DYNAMIC_LINK("fuzzing-support", "call-export", FuzzingSupport::callExport), DYNAMIC_LINK("fuzzing-support", "sleep", FuzzingSupport::sleep),
      DYNAMIC_LINK("fuzzing-support", "call-export-catch", FuzzingSupport::callExportCatch));
  vb::STDCompilerLogger stdCompilerLogger{};
  vb::WasmModule wasmModule{stdCompilerLogger};
  bool highRegisterPressure;
#ifdef VB_FORCE_HIGH_REGISTER_PRESSURE
  highRegisterPressure = true;
#else
  highRegisterPressure = false;
#endif
  std::ifstream refOutput = std::ifstream(fuzzPaths.referenceOutputFilePath.string());

  auto const executionClockStart = std::chrono::system_clock::now();
  try {
    vb::Span<uint8_t const> const bytecodeSpan{bytecode.data(), bytecode.size()};
    vb::Span<vb::NativeSymbol const> const symbolSpan{staticallyLinkedSymbols.data(), staticallyLinkedSymbols.size()};
    vb::WasmModule::CompileResult const compileResult = wasmModule.compile(bytecodeSpan, symbolSpan, highRegisterPressure);

    wasmModule.initFromCompiledBinary(compileResult.getModule().span(), symbolSpan, compileResult.getDebugSymbol().span());
    if (!reproduceWithModule) {
      generate();
    }

    if (isLogDetails) {
      std::cout << "Executing module with seed: " << seed << "\n";
    }

    // Create runtime
    fuzzPaths.fuzzWasmFilePath = fs::canonical(fuzzPaths.fuzzWasmFilePath);
    uint8_t const *const stackTop{vb::pCast<uint8_t const *>(vb::getStackTop())};
    wasmModule.start(stackTop);

    for (std::string line; getline(refOutput, line);) {
      if (next) {
        next = false;
        break;
      }

      if (line.find("called host", 0) == 0) { // s starts with prefix
        deferredLines.push_back(line);
      } else {
        std::string const funcName = line.substr(0, line.find("()"));

        if (size_t const pos = line.find("=>")) {
          vb::Span<char const> const funcNameSpan{vb::pCast<char const *>(funcName.data()), funcName.length()};
          vb::Span<char const> const functionSignature = wasmModule.getFunctionSignatureByName(funcNameSpan);
          if (line.size() >= pos + 5) {
            std::string const returnType = line.substr(pos + 3, 3);
            if (returnType == "err") {
              // expect err
              try {
                size_t const numReturnValues{functionSignature.size() - 2};
                std::vector<uint8_t> results(numReturnValues * 8);
                wasmModule.callRawExportedFunctionByName(funcNameSpan, stackTop, nullptr, results.data());

                std::cout << "Trap expected, but did not occur. Exiting.\n";
                executionFailed();
              } catch (vb::TrapException &trapException) { // SUCCESS
                switch (trapException.getTrapCode()) {
                case vb::TrapCode::STACKFENCEBREACHED:
                  std::cout << "WARN: " << trapException.what() << "\n";
                  throw;
                default:
                  break;
                }
              } catch (std::exception &e) {
                std::cout << "WARN: " << e.what() << "\n";
                throw;
              } catch (...) {
                std::cout << "Unknown exception" << "\n";
                throw;
              }
            } else {
              std::vector<ExpectedData> expectedReturnValues;
              std::string const returnValueStr = line.substr(pos + 2);
              std::istringstream returnValueStream(returnValueStr);
              for (std::string token; getline(returnValueStream, token, ',');) {
                std::string const type = token.substr(1, 3);
                size_t const valueStart = token.find(':') + 1;
                std::string const value = token.substr(valueStart, token.size() - valueStart);
                expectedReturnValues.push_back({type, value});
              }

              std::vector<uint8_t> results(expectedReturnValues.size() * 8);
              wasmModule.callRawExportedFunctionByName(funcNameSpan, stackTop, nullptr, results.data());
              uint8_t const *resultPtr = results.data();

              for (ExpectedData const &expected : expectedReturnValues) {
                std::string const expectedType = expected.type;
                std::string const expectedValue = expected.value;
                if (expectedType == "i32") {
                  uint32_t actualValue = 0U;
                  std::memcpy(&actualValue, resultPtr, sizeof(actualValue));
                  if (isLogDetails) {
                    std::cout << std::to_string(actualValue) << " expected: " << expectedValue << "\n";
                  }
                  if (expectedValue != std::to_string(actualValue)) {
                    throw std::runtime_error("Wrong i32 return value");
                  }
                } else if (expectedType == "i64") {
                  uint64_t actualValue = 0U;
                  std::memcpy(&actualValue, resultPtr, sizeof(actualValue));
                  if (isLogDetails) {
                    std::cout << std::to_string(actualValue) << " expected: " << expectedValue << "\n";
                  }
                  if (expectedValue != std::to_string(actualValue)) {
                    throw std::runtime_error("Wrong i64 return value");
                  }
                } else if (expectedType == "f32") {
                  float actualValue = 0U;
                  std::memcpy(&actualValue, resultPtr, sizeof(actualValue));
                  if (isLogDetails) {
                    std::cout << std::to_string(actualValue) << " expected: " << expectedValue << "\n";
                  }
                  if (expectedValue != std::to_string(actualValue)) {
                    throw std::runtime_error("Wrong f32 return value");
                  }
                } else if (expectedType == "f64") {
                  double actualValue = 0U;
                  std::memcpy(&actualValue, resultPtr, sizeof(actualValue));
                  if (isLogDetails) {
                    std::cout << std::to_string(actualValue) << " expected: " << expectedValue << "\n";
                  }
                  if (expectedValue != std::to_string(actualValue)) {
                    throw std::runtime_error("Wrong f64 return value");
                  }
                } else {
                  assert(false && "Unreachable");
                }
                resultPtr += 8;
              }
            }
          } else {
            wasmModule.callRawExportedFunctionByName(funcNameSpan, stackTop, nullptr, nullptr);
          }
          if (isLogDetails) {
            std::cout << line << "\n";
          }
          functionsExecuted++;
        } else {
          if (isLogDetails) {
            std::cout << line << "\n";
          }
          throw std::runtime_error("Fail");
        }

        if (deferredLines.size() != 0) {
          std::cout << "Non-consumed lines in buffer. Exiting.\n";
          executionFailed();
        }
      }
    }
  } catch (vb::ImplementationLimitationException const &implementationLimitationException) {
    // skip due to implement limitation
    std::cout << "WARN: " << implementationLimitationException.what() << "\n";
  } catch (vb::TrapException const &trapException) {
    switch (trapException.getTrapCode()) {
    case vb::TrapCode::STACKFENCEBREACHED:
      std::cout << "WARN: " << trapException.what() << "\n";
      break;
    default:
      std::cout << trapException.what() << "\n";
      executionFailed();
      break;
    }
  } catch (std::exception const &e) {
    std::cout << "WARN: " << e.what() << "\n";
    executionFailed();
  }
  deferredLines.clear();

  timeTakenExecutingVB += std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now() - executionClockStart).count();
}

std::string random_string(size_t length, std::function<char()> randChar) {
  std::string str(length, 0);
  std::generate_n(str.begin(), length, randChar);
  return str;
}

int main(int argc, char *argv[]) {
  std::string const identifier = "vb_fuzz";
  std::thread timeoutThread{};
  uint32_t timeout = 0;
  const char *inputTempPath = nullptr;
  vb::WasmModule::initEnvironment(&malloc, &realloc, &free);
  for (int i = 1; i < argc; i++) {
    if (0 == strcmp(argv[i], "--reproduceWithModule")) {
      reproduceWithModule = true;
    } else if (0 == strcmp(argv[i], "--reproduceWithSeed")) {
      reproduceWithSeed = true;
    } else if (0 == strcmp(argv[i], "--exit-on-first-error")) {
      isExitOnFirstError = true;
    } else if (0 == strcmp(argv[i], "--timeout")) {
      i++;
      if (i >= argc) {
        std::cerr << "invalid arguments: --timeout <time(second)>\n";
        std::exit(-1);
      }
      timeout = static_cast<uint32_t>(std::stoul(argv[i]));
    } else {
      inputTempPath = argv[i];
    }
  }
  if (inputTempPath == nullptr) {
    tempDirPath = fs::temp_directory_path() / identifier;
    std::cout << "No path specified. Using " << tempDirPath << "\n";
    create_directory(tempDirPath);
  } else {
    tempDirPath = inputTempPath;
    if (!fs::exists(tempDirPath)) {
      std::cout << tempDirPath << " does not exist. Exiting.\n";
      std::exit(-1);
    }
    if (!reproduceWithModule && !fs::is_directory(tempDirPath)) {
      // only --reproduceWithModule accept a wasm file input.
      std::cout << tempDirPath << " is not a directory. Exiting.\n";
      std::exit(-1);
    }
  }

  if (reproduceWithSeed) {
    isLogDetails = true;
    isExitOnFirstError = true;
    std::ifstream seedFile{tempDirPath / "seed.txt"};
    if (!seedFile) {
      std::cout << "No seed file: " << tempDirPath / "seed.txt" << std::endl;
      std::exit(1);
    }
    seed = std::string{(std::istreambuf_iterator<char>(seedFile)), std::istreambuf_iterator<char>()};
    generate();
    fuzz();
    return 0;
  }

  if (reproduceWithModule) {
    isLogDetails = true;
    isExitOnFirstError = true;
    if (fs::is_regular_file(tempDirPath)) {
      fs::path wasmFilePath = inputTempPath;
      if (fs::path(inputTempPath).extension() == ".wat") {
        wasmFilePath.replace_extension(".wasm");
        std::ostringstream shellCommand;
        shellCommand << "wat2wasm " << inputTempPath << " -o " << wasmFilePath.string();
        int const res = system(shellCommand.str().c_str());
        if (res != 0) {
          std::cerr << "wat2wasm failed for " << inputTempPath << std::endl;
          std::exit(-1);
        }
      } else if (fs::path(inputTempPath).extension() != ".wasm") {
        std::cerr << "Input file must be a .wasm or .wat file" << std::endl;
        std::exit(-1);
      }
      fs::path const refOutDir = fs::temp_directory_path() / "vb_fuzz_reproduce";
      create_directory(refOutDir);
      fuzzPaths.fuzzWasmFilePath = wasmFilePath;
      fs::path const referenceOutputFilePath{refOutDir / "refOut.txt"};
      generateReferenceOutput(fuzzPaths.fuzzWasmFilePath, referenceOutputFilePath);
      fuzz();
      return 0;
    } else if (fs::is_directory(tempDirPath)) {
      fs::path const referenceOutputFilePath{fs::temp_directory_path() / "refOut.txt"};
      for (const fs::directory_entry &entry : fs::directory_iterator(tempDirPath)) {
        if (entry.path().extension() == ".wasm") {
          fuzzPaths.fuzzWasmFilePath = entry.path();
          std::cout << "\nStarting fuzzer with fuzzAssets: " << fuzzPaths.fuzzWasmFilePath << "\n" << std::endl;
          generateReferenceOutput(fuzzPaths.fuzzWasmFilePath, referenceOutputFilePath);
          fuzz();
        }
      }
      return 0;
    }
  }

  std::atomic<bool> continueFuzz{true};

  if (timeout != 0) {
    timeoutThread = std::thread{[timeout, &continueFuzz]() {
      std::this_thread::sleep_for(std::chrono::seconds{timeout});
      std::cout << "finish fuzz due to timeout\n";
      continueFuzz = false;
    }};
  }

  std::cout << "Fuzzing in " << tempDirPath << "\n";

  constexpr std::array<char, 89> charset = {"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                            "abcdefghijklmnopqrstuvwxyz"
                                            "!$%&/()=?*+'#_-.:,;@^<[]{}"};
  constexpr size_t max_index = (sizeof(charset) - 2);

  std::random_device rd;
  std::random_device::result_type const randomNum = rd();
  std::mt19937 mt(randomNum);
  std::uniform_int_distribution<int> dist(0, max_index);

  std::cout << "Starting fuzzer with seed " << randomNum << std::endl;
  uint64_t iteration = 0;

  uint32_t oldFunctionsExecuted = 0;
  int64_t oldTime = 0;
  auto const execStart = std::chrono::system_clock::now();
  while (continueFuzz) {
    seed = identifier + random_string(1000, [&]() -> char {
             return charset[static_cast<size_t>(dist(mt))];
           });
    generate();
    fuzz();

    iteration++;
    if (iteration % 100 == 0) {
      int64_t const newTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - execStart).count();
      std::cout << std::fixed << std::setprecision(1) << functionsExecuted << " function calls (" << iteration << " modules) executed in "
                << static_cast<float>(newTime) / 1000 << " s (" << failedExecutions << " failed) - "
                << static_cast<float>(functionsExecuted - oldFunctionsExecuted) * 1000 / static_cast<float>(newTime - oldTime) << " f/s (last 100), "
                << static_cast<float>(functionsExecuted) * 1000 / static_cast<float>(newTime) << " f/s (all)\n"
                << std::flush;
      float const factor = 100.0F / (1000 * 1000 * static_cast<float>(newTime - oldTime));
      std::cout << static_cast<float>(timeTakenGeneratingBinary) * factor << "% of time spent generating binaries, "
                << static_cast<float>(timeTakenGeneratingReferenceOutput) * factor << "% for executing reference interp, "
                << static_cast<float>(timeTakenExecutingVB) * factor << "% for VB execution\n\n";
      oldFunctionsExecuted = functionsExecuted;
      oldTime = newTime;
      timeTakenGeneratingBinary = 0;
      timeTakenGeneratingReferenceOutput = 0;
      timeTakenExecutingVB = 0;
    }
  }
  timeoutThread.join();
  vb::WasmModule::destroyEnvironment();
  return 0;
}
