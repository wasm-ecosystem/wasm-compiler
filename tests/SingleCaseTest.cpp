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
#include <sstream>
#include <string>
#include <unordered_map>

#include "tests/SingleCaseTest.hpp"
#include "tests/TestData.hpp"
#include "tests/testimports.hpp"

#include "src/config.hpp"
//
#include "src/WasmModule/WasmModule.hpp"
#include "src/core/common/TrapCode.hpp"
#include "src/core/compiler/Compiler.hpp"
#include "src/core/runtime/Runtime.hpp"
#include "src/core/runtime/TrapException.hpp"
#include "src/utils/StackTop.hpp"

#ifdef VB_WIN32_OR_POSIX

vb::WasmModule::ReallocFunction reallocFunction{&std::realloc};
vb::WasmModule::MallocFunction mallocFunction{&std::malloc};
vb::WasmModule::FreeFunction freeFunction{&std::free};

#elif (CXX_TARGET == ISA_TRICORE)
#include "src/utils/ExecutableMemoryNoMMU.hpp"
namespace vb {

alignas(8) std::array<uint8_t, 1024U * 390U> jobMemoryRegion{};

alignas(8) std::array<uint8_t, 1024U * 255U> binaryMemoryRegion{};

bool regionInUse1{false};
bool regionInUse2{false};

void *reallocDispatch(void *const ptr, size_t const size) {
  static_cast<void>(size);
  if (ptr == nullptr) {
    if (!regionInUse1) {
      regionInUse1 = true;
      return jobMemoryRegion.data();
    } else if (!regionInUse2) {
      regionInUse2 = true;
      return binaryMemoryRegion.data();
    } else {
      assert(false && "No memory region available for realloc");
    }
  } else if (ptr == static_cast<void *>(jobMemoryRegion.data())) {
    if (size > jobMemoryRegion.size()) {
      assert(false && "Reallocating job memory region to larger size than available");
    }

  } else if (ptr == static_cast<void *>(binaryMemoryRegion.data())) {
    if (size > binaryMemoryRegion.size()) {
      assert(false && "Reallocating binary memory region to larger size than available");
    }
  } else {
    assert(false && "Reallocating memory region that is not job or binary memory region");
  }

  return ptr;
}

void freeDispatch(void *const ptr) {
  if (ptr == static_cast<void *>(jobMemoryRegion.data())) {
    regionInUse1 = false;
  } else if (ptr == static_cast<void *>(binaryMemoryRegion.data())) {
    regionInUse2 = false;
  } else {
    free(ptr);
  }
}

vb::WasmModule::MallocFunction mallocFunction{&std::malloc};
vb::WasmModule::ReallocFunction reallocFunction{&reallocDispatch};
vb::WasmModule::FreeFunction freeFunction{&freeDispatch};

} // namespace vb

#else
static_assert(false, "unsupported target");
#endif

namespace vb {

TestResult &TestResult::operator+=(TestResult const &rh) noexcept {
  executedTests += rh.executedTests;
  failedTests += rh.failedTests;
  totalTests += rh.totalTests;
  return *this;
}
std::ostream &operator<<(std::ostream &outputStream, TestResult const &v) {
  outputStream << v.executedTests - v.failedTests << "/" << v.executedTests << " tests successfully executed. (" << v.totalTests - v.executedTests
               << " skipped)";
  return outputStream;
}

TestData::TestData(const void *data, std::size_t const len) {
  if (len == 0) {
    m_memObj = Span<const uint8_t>(nullptr, 0);
    return;
  }
  p_data.resize(len);
  std::memcpy(p_data.data(), data, len);
  m_memObj.reset(p_data.data(), static_cast<uint32_t>(len));
}

// -------------------------------------------

inline uint32_t lexical_cast_u32(std::string const &str) {
  return static_cast<uint32_t>(std::stoul(str));
}
inline uint64_t lexical_cast_u64(std::string const &str) {
  return static_cast<uint64_t>(std::stoull(str));
}

std::ostream &operator<<(std::ostream &os, CommandType type) {
  return os << static_cast<uint32_t>(type);
}

SingleCaseTest::SingleCaseTest(std::string const &_testcaseName) : testcaseName(_testcaseName) {
}

extern "C" {
// message display for MCUs without stdout
uint32_t TEST_MESSAGE_SIZE = 0U;
char TEST_MESSAGE[200]; // NOLINT(modernize-avoid-c-arrays)
int32_t DEBUGGER_NOTIFICATION(int32_t x) {
  return x + 1;
}
}

// Buffer to store last stacktrace
std::vector<uint32_t> lastStacktrace{};

void SingleCaseTest::testFailed(uint32_t const line, std::string const &message) {
  testResult.failedTests++;
  std::stringstream ss;
  ss << "Test " << testcaseName.c_str() << " at line " << std::dec << line << " failed: " << message << std::endl;

  std::string const failedMessage = ss.str();

  const size_t messageSize = std::min(failedMessage.size(), static_cast<size_t>(sizeof(TEST_MESSAGE)));

  memcpy(TEST_MESSAGE, failedMessage.c_str(), messageSize);

  TEST_MESSAGE_SIZE = static_cast<uint32_t>(messageSize);

  static_cast<void>(DEBUGGER_NOTIFICATION(1));

  std::cout << failedMessage;
  TEST_MESSAGE_SIZE = 0U;
}

class DummyLogger : public ILogger {
public:
  ///
  /// @brief Log const char*
  ///
  /// @param message
  /// @return const ILogger&
  ///
  inline ILogger &operator<<(char const *const message) override {
    static_cast<void>(message);
    return *this;
  }
  ///
  /// @brief Log message in Span format
  ///
  /// @param message
  /// @return const ILogger&
  ///
  inline ILogger &operator<<(Span<char const> const &message) override {
    static_cast<void>(message);
    return *this;
  }
  ///
  /// @brief log error code
  ///
  /// @param errorCode
  /// @return const ILogger&
  ///
  inline ILogger &operator<<(uint32_t const errorCode) override {
    static_cast<void>(errorCode);
    return *this;
  }

  /// @brief The type of function which can be executed by ILogger
  using ILoggerFunc = ILogger &(*)(ILogger &logger);
  ///
  /// @brief Allows usage of vb::endStatement
  ///
  /// @param fnc Function to be executed with the corresponding ILogger
  /// @return ILogger&
  inline ILogger &operator<<(ILoggerFunc const fnc) override {
    return ILogger::operator<<(fnc);
  }

  ///
  /// @brief Mark this statement as finished
  ///
  /// @param level Log level
  ///
  inline void endStatement(LogLevel const level) override {
    static_cast<void>(level);
  }
};

TestResult SingleCaseTest::testFromStream(TestLoader *loader, bool const enableDebugMode, bool const enableStacktrace,
                                          bool const forceHighRegisterPressure) {
  static_cast<void>(enableDebugMode);
  assert(loader != nullptr);
  vb::WasmModule::initEnvironment(mallocFunction, reallocFunction, freeFunction);
  std::unique_ptr<WasmModule> wasmModule;
  std::array<uint8_t, 512> linkedBuffer{};
  for (size_t i = 0; i < linkedBuffer.size(); i++) {
    linkedBuffer[i] = static_cast<uint8_t>(i);
  }
  DummyLogger logger{};
  uint8_t const *const stackTop = pCast<uint8_t const *>(getStackTop());
  vb::Span<vb::NativeSymbol const> const importFunctions{vb::Span<NativeSymbol const>(spectestImports.data(), spectestImports.size())};

  std::unique_ptr<Command> command;
  while (nullptr != (command = loader->getNextCommand())) {
    testResult.totalTests++;
    CommandType const type = command->getType();
    uint32_t const line = command->getLine();
    // fixme
    // std::cout << "run line " << line << std::endl;
    try {
      switch (type) {
      case CommandType::MODULE: {
        bool debugBuild;
#ifdef JIT_TARGET_TRICORE
        debugBuild = false;
#else
        debugBuild = enableDebugMode;
#endif
        wasmModule = std::make_unique<WasmModule>(logger, debugBuild);
        auto moduleCommand = static_cast<ModuleCommand *>(command.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        Span<const uint8_t> const &bytecode = moduleCommand->getByteCode();

        if (testcaseName.rfind("vb_stacktrace") == 0 || enableStacktrace) {
          wasmModule->setStacktraceRecordCount(8U);
        }
        try {
          vb::WasmModule::CompileResult const compileResult{wasmModule->compile(bytecode, importFunctions, forceHighRegisterPressure)};

          wasmModule->initFromCompiledBinary(compileResult.getModule().span(), importFunctions, compileResult.getDebugSymbol().span());

          wasmModule->start(stackTop);

          wasmModule->linkMemory(Span<uint8_t const>(linkedBuffer.data(), static_cast<uint32_t>(linkedBuffer.size())));

        } catch (ValidationException const &e) {
          testFailed(line, e.what());
          wasmModule = nullptr;
        } catch (LinkingException const &e) {
          testFailed(line, e.what());
          wasmModule = nullptr;
        } catch (FeatureNotSupportedException const &e) {
          static_cast<void>(e);
          wasmModule = nullptr;
        } catch (ImplementationLimitationException const &e) {
          std::cout << "ImplementationLimitationException: " << e.what() << "\n";
          wasmModule = nullptr;
        }

        break;
      }

      case CommandType::ASSERT_RETURN:
      case CommandType::ACTION:
      case CommandType::ASSERT_TRAP:
      case CommandType::ASSERT_EXHAUSTION: {
        if (wasmModule == nullptr) {
          continue;
        };

        auto const assertCommand = static_cast<AssertCommand *>(command.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        Action const action = assertCommand->getAction();
        ActionType const actionType = action.type;
        std::string const field = action.field;

        spectest::ImportsMaker::setLastStacktraceBuffer(&lastStacktrace);

        wasmModule->linkMemory(Span<uint8_t const>(linkedBuffer.data(), static_cast<uint32_t>(linkedBuffer.size())));

        switch (actionType) {
        case ActionType::GET: {
          std::string returnType;
          std::string returnValue;
          for (auto const &expected : assertCommand->getExpected()) {
            returnType = expected.type;
            returnValue = expected.value;
          }

          if (returnType == "i32") {
            uint32_t const expectedReturnValue = lexical_cast_u32(returnValue);
            uint32_t const actualReturnValue = wasmModule->getExportedGlobalByName<uint32_t>(field.c_str()).getValue();
            if (expectedReturnValue != actualReturnValue) {
              std::stringstream ss;
              ss << std::hex << "Expected 0x" << expectedReturnValue << " but got 0x" << actualReturnValue << "\n";
              testFailed(line, ss.str());
            }
          } else if (returnType == "i64") {
            uint64_t const expectedReturnValue = lexical_cast_u64(returnValue);
            uint64_t const actualReturnValue = wasmModule->getExportedGlobalByName<uint64_t>(field.c_str()).getValue();
            if (expectedReturnValue != actualReturnValue) {
              std::stringstream ss;
              ss << std::hex << "Expected 0x" << expectedReturnValue << " but got 0x" << actualReturnValue << "\n";
              testFailed(line, ss.str());
            }
          } else if (returnType == "f32") {
            uint32_t const rawExpectedReturnValue = lexical_cast_u32(returnValue);
            float const actualReturnValue = wasmModule->getExportedGlobalByName<float>(field.c_str()).getValue();
            uint32_t const rawActualReturnValue = bit_cast<uint32_t>(actualReturnValue);
            if (rawExpectedReturnValue != rawActualReturnValue) {
              std::stringstream ss;
              ss << std::hex << "Expected 0x" << rawExpectedReturnValue << " but got 0x" << rawActualReturnValue << "\n";
              testFailed(line, ss.str());
            }
          } else if (returnType == "f64") {
            uint64_t const rawExpectedReturnValue = lexical_cast_u64(returnValue);
            double const actualReturnValue = wasmModule->getExportedGlobalByName<double>(field.c_str()).getValue();
            uint64_t const rawActualReturnValue = bit_cast<uint64_t>(actualReturnValue);
            if (rawExpectedReturnValue != rawActualReturnValue) {
              std::stringstream ss;
              ss << std::hex << "Expected 0x" << rawExpectedReturnValue << " but got 0x" << rawActualReturnValue << "\n";
              testFailed(line, ss.str());
            }
          }

          break;
        }

        case ActionType::INVOKE: {
          std::vector<uint8_t> serializationData(action.args.size() * 8);

          uint8_t *serializationPtr = serializationData.data();

          for (auto const &actionArg : action.args) {
            std::string const argType = actionArg.type;
            std::string const argValue = actionArg.value;
            if (argType == "i32") {
              uint32_t const argCast = lexical_cast_u32(argValue);
              std::memcpy(serializationPtr, &argCast, sizeof(argCast));
            } else if (argType == "i64") {
              uint64_t const argCast = lexical_cast_u64(argValue);
              std::memcpy(serializationPtr, &argCast, sizeof(argCast));
            } else if (argType == "f32") {
              uint32_t rawArg = lexical_cast_u32(argValue);
              std::memcpy(serializationPtr, &rawArg, sizeof(rawArg));
            } else if (argType == "f64") {
              uint64_t const rawArg = lexical_cast_u64(argValue);
              std::memcpy(serializationPtr, &rawArg, sizeof(rawArg));
            } else {
              assert(false);
            }
            serializationPtr += 8;
          }

          Span<char const> const functionName{vb::Span<char const>(field.c_str(), field.length())};
          Span<char const> const functionSignature{wasmModule->getFunctionSignatureByName(functionName)};
          size_t const numReturnValues{functionSignature.size() - action.args.size() - 2};
          std::vector<uint8_t> results{};
          results.resize(numReturnValues * 8U);

          if (type == CommandType::ASSERT_TRAP) {
            try {
              wasmModule->callRawExportedFunctionByName(functionName, stackTop, serializationData.data(), results.data());
              testFailed(line, "No trap, but trap expected");
            } catch (TrapException &trapException) { // SUCCESS
              std::string const trapText = assertCommand->getText();
              TrapCode const expectedTrapCode = getTrapCodeFromTrapText(trapText);
              if (!isExpectedTrap(trapException.getTrapCode(), expectedTrapCode)) {
                std::stringstream ss;
                ss << "Expected trap code " << static_cast<uint32_t>(expectedTrapCode) << "(" << trapText << "), but got "
                   << static_cast<uint32_t>(trapException.getTrapCode()) << "\n";
                testFailed(line, ss.str());
              }

              lastStacktrace.clear();
              wasmModule->iterateStacktraceRecords(FunctionRef<void(uint32_t)>([](uint32_t const fncIndex) {
                lastStacktrace.push_back(fncIndex);
              }));
            }
          } else if (type == CommandType::ASSERT_EXHAUSTION) {
            try {
              wasmModule->callRawExportedFunctionByName(functionName, stackTop, serializationData.data(), results.data());
              testFailed(line, "No trap, but trap expected");
            } catch (TrapException &e) {
              if (e.getTrapCode() == TrapCode::STACKFENCEBREACHED) {
                lastStacktrace.clear();
                wasmModule->iterateStacktraceRecords(FunctionRef<void(uint32_t)>([](uint32_t const fncIndex) {
                  lastStacktrace.push_back(fncIndex);
                }));
              } else {
                throw;
              }
            }
          } else {
            try {
              wasmModule->callRawExportedFunctionByName(functionName, stackTop, serializationData.data(), results.data());
              uint8_t const *resultPtr = results.data();
              for (auto const &expected : assertCommand->getExpected()) {
                std::string const resultType = expected.type;
                std::string const resultValue = expected.value;
                if (resultType == "i32") {
                  uint32_t const expectedResultValue = lexical_cast_u32(resultValue);
                  uint32_t actualResultValue = 0U;
                  std::memcpy(&actualResultValue, resultPtr, sizeof(actualResultValue));
                  if (expectedResultValue != actualResultValue) {
                    std::stringstream ss;
                    ss << std::hex << "Expected 0x" << expectedResultValue << " but got 0x" << actualResultValue << "\n";
                    testFailed(line, ss.str());
                  }
                } else if (resultType == "i64") {
                  uint64_t const expectedResultValue = lexical_cast_u64(resultValue);
                  uint64_t actualResultValue = 0U;
                  std::memcpy(&actualResultValue, resultPtr, sizeof(actualResultValue));
                  if (expectedResultValue != actualResultValue) {
                    std::stringstream ss;
                    ss << std::hex << "Expected 0x" << expectedResultValue << " but got 0x" << actualResultValue << "\n";
                    testFailed(line, ss.str());
                  }
                } else if (resultType == "f32") {
                  bool const resultValueIsSomeNan = resultValue == "nan:canonical" || resultValue == "nan:arithmetic" || resultValue == "nan";
                  uint32_t const rawExpectedResultValue = !resultValueIsSomeNan ? lexical_cast_u32(resultValue) : 0;
                  uint32_t rawActualResultValue = 0U;
                  std::memcpy(&rawActualResultValue, resultPtr, sizeof(rawActualResultValue));

                  if (!resultValueIsSomeNan) {
                    if (rawExpectedResultValue != rawActualResultValue) {
                      std::stringstream ss;
                      ss << std::hex << "Expected (float) 0x" << rawExpectedResultValue << " but got 0x" << rawActualResultValue << "\n";
                      testFailed(line, ss.str());
                    }
                  } else {
                    uint32_t const resultValueExponent = (rawActualResultValue >> 23U) & 0xFFU;
                    uint32_t const resultValueFraction = rawActualResultValue & 0x7F'FFFFU;

                    bool correctNan = true;
                    if (resultValueExponent != 0xFF || resultValueFraction == 0x00U) {
                      correctNan = false;
                    }
                    if (resultValue == "nan:canonical" && !(resultValueFraction == 1U << 22U)) {
                      correctNan = false;
                    }
                    if (resultValue == "nan:arithmetic" && !(resultValueFraction >= 1U << 22U)) {
                      correctNan = false;
                    }

                    if (!correctNan) {
                      std::stringstream ss;
                      ss << std::hex << "Expected " << resultValue << " but got 0x" << rawActualResultValue << "\n";
                      testFailed(line, ss.str());
                    }
                  }
                } else if (resultType == "f64") {
                  bool const resultValueIsSomeNan = resultValue == "nan:canonical" || resultValue == "nan:arithmetic" || resultValue == "nan";
                  uint64_t const rawExpectedResultValue = !resultValueIsSomeNan ? lexical_cast_u64(resultValue) : 0;
                  uint64_t rawActualResultValue = 0U;
                  std::memcpy(&rawActualResultValue, resultPtr, sizeof(rawActualResultValue));

                  if (!resultValueIsSomeNan) {
                    if (rawExpectedResultValue != rawActualResultValue) {
                      std::stringstream ss;
                      ss << std::hex << "Expected (double) 0x" << rawExpectedResultValue << " but got 0x" << rawActualResultValue << "\n";
                      testFailed(line, ss.str());
                    }
                  } else {
                    uint64_t const resultValueExponent = (rawActualResultValue >> 52U) & 0x7FF_U64;
                    uint64_t const resultValueFraction = rawActualResultValue & 0xF'FFFF'FFFF'FFFF_U64;

                    bool correctNan = true;
                    if (resultValueExponent != 0x7FF_U64 || resultValueFraction == 0x00U) {
                      correctNan = false;
                    }
                    if (resultValue == "nan:canonical" && !(resultValueFraction == 1_U64 << 51U)) {
                      correctNan = false;
                    }
                    if (resultValue == "nan:arithmetic" && !(resultValueFraction >= 1_U64 << 51U)) {
                      correctNan = false;
                    }

                    if (!correctNan) {
                      std::stringstream ss;
                      ss << std::hex << "Expected " << resultValue << " but got 0x" << rawActualResultValue << "\n";
                      testFailed(line, ss.str());
                    }
                  }
                } else {
                  assert(false);
                }
                resultPtr += 8;
              }
            } catch (TrapException &e) {
              testFailed(line, e.what());
            }
          }

          break;
        }
        default: {
          std::cout << "Unknown action type: " << testcaseName.c_str() << " " << line << " " << type << "\n";
          exit(-1);
        }
        }
        break;
      }

      case CommandType::ASSERT_INVALID:
      case CommandType::ASSERT_MALFORMED: {
        auto invalidCommand = static_cast<InvalidCommand *>(command.get()); // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        Span<const uint8_t> const &bytecode = invalidCommand->getByteCode();
        if (bytecode.data() == nullptr) {
          continue;
        }

        wasmModule = std::make_unique<WasmModule>(logger, enableDebugMode);
        if (enableStacktrace) {
          wasmModule->setStacktraceRecordCount(8U);
        }

        try {
          wasmModule->compile(bytecode, importFunctions, forceHighRegisterPressure);
          testFailed(line, "Compilation should fail but didn't");
        } catch (ValidationException const &e) {
          // SUCCESS
          static_cast<void>(e);
        } catch (FeatureNotSupportedException const &e) {
          // IGNORE
          static_cast<void>(e);
        } catch (ImplementationLimitationException const &e) {
          // IGNORE
          static_cast<void>(e);
        }
        break;
      }
      case CommandType::ASSERT_UNINSTANTIABLE:
      case CommandType::ASSERT_UNLINKABLE:
      case CommandType::REGISTER: {
        continue;
      }

      default: {
        std::cout << "Unknown test type: " << testcaseName.c_str() << " " << line << " " << type << "\n";
        exit(-1);
      }
      }
    } catch (std::exception const &e) {
      std::stringstream ss;
      ss << "unknown error: " << e.what() << std::endl;
      testFailed(line, ss.str());
    }
    testResult.executedTests++;
  } // namespace vb
  vb::WasmModule::destroyEnvironment();
  return testResult;
}

TrapCode SingleCaseTest::getTrapCodeFromTrapText(std::string const &text) {
  static const std::unordered_map<std::string, TrapCode> trapCodeMap = {
      {"unreachable", TrapCode::UNREACHABLE},
      {"builtin trap", TrapCode::BUILTIN_TRAP},
      {"runtime interrupt request", TrapCode::RUNTIME_INTERRUPT_REQUESTED},
      {"out of bounds memory access", TrapCode::LINMEM_OUTOFBOUNDSACCESS},
      {"out of bounds linear memory access", TrapCode::LINMEM_OUTOFBOUNDSACCESS},
      {"out of bounds linked memory access", TrapCode::LINKEDMEMORY_OUTOFBOUNDS},
      {"indirect call type mismatch", TrapCode::INDIRECTCALL_WRONGSIG},
      {"undefined element", TrapCode::INDIRECTCALL_OUTOFBOUNDS},
      {"integer overflow", TrapCode::DIV_OVERFLOW},
      {"integer divide by zero", TrapCode::DIV_ZERO},
      {"invalid conversion to integer", TrapCode::TRUNC_OVERFLOW},
      {"unknown import", TrapCode::CALLED_FUNCTION_NOT_LINKED},
      {"called function not linked", TrapCode::CALLED_FUNCTION_NOT_LINKED},
      {"indirect call not linked", TrapCode::INDIRECTCALL_WRONGSIG},
  };
  auto const &found = trapCodeMap.find(text);
  if (found == trapCodeMap.end()) {
    std::cerr << "Unknown trap: " << text << std::endl;
    std::terminate();
  }
  return found->second;
}
bool SingleCaseTest::isExpectedTrap(TrapCode const trapCode1, TrapCode const trapCode2) noexcept {
  if (trapCode1 != trapCode2) {
    ///< check if synonyms
    // n2n mapping walk around since different backend memoryAlloc maximum size may different.
    // Like: LINMEM_OUTOFBOUNDSACCESS may trapped as LINMEM_COULDNOTEXTEND on tricore in some cases
    return ((trapCode1 == TrapCode::LINKEDMEMORY_OUTOFBOUNDS) || (trapCode1 == TrapCode::LINMEM_OUTOFBOUNDSACCESS) ||
            (trapCode1 == TrapCode::LINMEM_COULDNOTEXTEND)) &&
           ((trapCode2 == TrapCode::LINKEDMEMORY_OUTOFBOUNDS) || (trapCode2 == TrapCode::LINMEM_OUTOFBOUNDSACCESS) ||
            (trapCode2 == TrapCode::LINMEM_COULDNOTEXTEND));
  }
  return true;
}
} // namespace vb
