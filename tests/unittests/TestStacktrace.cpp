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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "WabtCmd.hpp"
#include "tests/unittests/common.hpp"

#include "src/WasmModule/WasmModule.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/function_traits.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/Compiler.hpp"
#include "src/core/runtime/Runtime.hpp"
#include "src/core/runtime/TrapException.hpp"
#include "src/utils/ExecutableMemory.hpp"

#if CXX_TARGET == JIT_TARGET

using namespace vb;

namespace stacktrace {

void *allocFnc(uint32_t size, void *ctx) noexcept {
  static_cast<void>(ctx);
  return malloc(static_cast<size_t>(size));
}

void freeFnc(void *ptr, void *ctx) noexcept {
  static_cast<void>(ctx);
  free(ptr);
}

void memoryFnc(ExtendableMemory &currentObject, uint32_t minimumLength, void *const ctx) {
  static_cast<void>(ctx);
  if (minimumLength == 0) {
    free(currentObject.data());
  } else {
    minimumLength = std::max(minimumLength, static_cast<uint32_t>(1000U)) * 2U;
    currentObject.reset(pCast<uint8_t *>(realloc(currentObject.data(), minimumLength)), minimumLength);
  }
}

const char *testModuleWithDebugNamesStr = R"(
(module
  (type (;0;) (func))
  (func $long (type 0)
    call $1)
  (func $mid (type 0)
    call $7)
  (func $short (type 0)
    call $trap)
  (func $noTrap (type 0))
  (func $1 (type 0)
    call $2)
  (func $2 (type 0)
    call $3)
  (func $3 (type 0)
    call $4)
  (func $4 (type 0)
    call $5)
  (func $5 (type 0)
    call $6)
  (func $6 (type 0)
    call $7)
  (func $7 (type 0)
    call $8)
  (func $8 (type 0)
    call $trap)
  (func $trap (type 0)
    i32.const 0
    i32.const 0
    i32.div_u
    drop)
  (export "long" (func $long))
  (export "mid" (func $mid))
  (export "short" (func $short))
  (export "noTrap" (func $noTrap))
  (export "trap" (func $trap))
)
)";
// NOLINTNEXTLINE(cert-err58-cpp)
std::vector<uint8_t> const testModuleWithDebugNames = vb::test::WabtCmd::loadWasmFromWat(testModuleWithDebugNamesStr);
Span<const uint8_t> const moduleWithDebugNamesBytecode = Span<const uint8_t>(testModuleWithDebugNames.data(), testModuleWithDebugNames.size());

const char *testModuleWithoutDebugNamesStr = R"((module
  (type (;0;) (func))
  (func (;0;) (type 0)
    call 4)
  (func (;1;) (type 0)
    call 10)
  (func (;2;) (type 0)
    call 12)
  (func (;3;) (type 0))
  (func (;4;) (type 0)
    call 5)
  (func (;5;) (type 0)
    call 6)
  (func (;6;) (type 0)
    call 7)
  (func (;7;) (type 0)
    call 8)
  (func (;8;) (type 0)
    call 9)
  (func (;9;) (type 0)
    call 10)
  (func (;10;) (type 0)
    call 11)
  (func (;11;) (type 0)
    call 12)
  (func (;12;) (type 0)
    i32.const 0
    i32.const 0
    i32.div_u
    drop)
  (export "long" (func 0))
  (export "mid" (func 1))
  (export "short" (func 2))
  (export "noTrap" (func 3))
  (export "trap" (func 12))
)
)";
// NOLINTNEXTLINE(cert-err58-cpp)
std::vector<uint8_t> const testModuleWithoutDebugNames = vb::test::WabtCmd::loadWasmFromWat(testModuleWithoutDebugNamesStr);
Span<const uint8_t> const moduleWithoutDebugNamesBytecode =
    Span<const uint8_t>(testModuleWithoutDebugNames.data(), testModuleWithoutDebugNames.size());

class Logger : public vb::ILogger {
public:
  explicit Logger() noexcept : vb::ILogger() { // NOLINT(readability-redundant-member-init)
  }

  inline Logger &operator<<(char const *const message) override {
    bufferStream_ << message;
    return *this;
  }
  inline Logger &operator<<(uint32_t const number) override {
    bufferStream_ << number;
    return *this;
  }
  inline Logger &operator<<(ILogger &(*fnc)(ILogger &logger)) override {
    static_cast<void>(fnc(*this));
    return *this;
  }
  inline Logger &operator<<(vb::Span<char const> const &message) override {
    static_cast<void>(bufferStream_.write(message.data(), static_cast<std::streamsize>(message.size())));
    return *this;
  }
  inline void endStatement(vb::LogLevel const level) override {
    static_cast<void>(level);
    bufferStream_ << std::endl;
  }
  std::string get() {
    return bufferStream_.str();
  }
  void clear() {
    bufferStream_.str("");
  }

private:
  std::stringstream bufferStream_; ///< Intermediate buffer
};

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(TestStacktrace, stacktracePersistsIfRetrievedMultipleTimes) {
  Compiler compiler = Compiler(memoryFnc, allocFnc, freeFnc, nullptr, memoryFnc);
  compiler.setStacktraceRecordCount(3U);
  ManagedBinary const binaryModule = compiler.compile(moduleWithDebugNamesBytecode);

  ExecutableMemory const executableMemory = ExecutableMemory::make_executable_copy(binaryModule);
  Runtime runtime = test::createRuntime(executableMemory);

  std::vector<uint32_t> stacktrace;
  auto updateStacktrace = [&runtime, &stacktrace]() {
    stacktrace.clear();
    runtime.iterateStacktraceRecords(FunctionRef<void(uint32_t)>([&stacktrace](uint32_t const fncIndex) {
      stacktrace.push_back(fncIndex);
    }));
  };

  runtime.start();

  ASSERT_THROW(SignalFunctionWrapper::call(runtime.getExportedFunctionByName<0>("long")), vb::TrapException);
  updateStacktrace();
  ASSERT_THAT(stacktrace, testing::ElementsAre(12, 11, 10));
  updateStacktrace();
  ASSERT_THAT(stacktrace, testing::ElementsAre(12, 11, 10));
  updateStacktrace();
  ASSERT_THAT(stacktrace, testing::ElementsAre(12, 11, 10));
  updateStacktrace();
  ASSERT_THAT(stacktrace, testing::ElementsAre(12, 11, 10));
}

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(TestStacktrace, stacktraceShowsCorrectFunctionNames) {
  Compiler compiler = Compiler(memoryFnc, allocFnc, freeFnc, nullptr, memoryFnc);
  compiler.setStacktraceRecordCount(16U);
  ManagedBinary const binaryModule = compiler.compile(moduleWithDebugNamesBytecode);

  ExecutableMemory const executableMemory = ExecutableMemory::make_executable_copy(binaryModule);
  Runtime runtime = test::createRuntime(executableMemory);

  runtime.start();

  Logger logger;

  logger.clear();
  runtime.printStacktrace(logger);
  ASSERT_EQ(logger.get(), "No stacktrace records found\n");

  logger.clear();
  ASSERT_THROW(SignalFunctionWrapper::call(runtime.getExportedFunctionByName<0>("long")), vb::TrapException);
  runtime.printStacktrace(logger);
  ASSERT_EQ(logger.get(), "\tat trap (wasm-function[12])\n"
                          "\tat 8 (wasm-function[11])\n"
                          "\tat 7 (wasm-function[10])\n"
                          "\tat 6 (wasm-function[9])\n"
                          "\tat 5 (wasm-function[8])\n"
                          "\tat 4 (wasm-function[7])\n"
                          "\tat 3 (wasm-function[6])\n"
                          "\tat 2 (wasm-function[5])\n"
                          "\tat 1 (wasm-function[4])\n"
                          "\tat long (wasm-function[0])\n");

  logger.clear();
  ASSERT_THROW(SignalFunctionWrapper::call(runtime.getExportedFunctionByName<0>("short")), vb::TrapException);
  runtime.printStacktrace(logger);
  ASSERT_EQ(logger.get(), "\tat trap (wasm-function[12])\n"
                          "\tat short (wasm-function[2])\n");

  logger.clear();
  ASSERT_THROW(SignalFunctionWrapper::call(runtime.getExportedFunctionByName<0>("trap")), vb::TrapException);
  runtime.printStacktrace(logger);
  ASSERT_EQ(logger.get(), "\tat trap (wasm-function[12])\n");
}

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(TestStacktrace, stacktraceIsCorrectWithoutFunctionNames) {
  Compiler compiler = Compiler(memoryFnc, allocFnc, freeFnc, nullptr, memoryFnc);
  compiler.setStacktraceRecordCount(16U);
  ManagedBinary const binaryModule = compiler.compile(moduleWithoutDebugNamesBytecode);

  ExecutableMemory const executableMemory = ExecutableMemory::make_executable_copy(binaryModule);
  Runtime runtime = test::createRuntime(executableMemory);

  runtime.start();

  Logger logger;

  logger.clear();
  runtime.printStacktrace(logger);
  ASSERT_EQ(logger.get(), "No stacktrace records found\n");

  logger.clear();
  ASSERT_THROW(SignalFunctionWrapper::call(runtime.getExportedFunctionByName<0>("long")), vb::TrapException);
  runtime.printStacktrace(logger);
  ASSERT_EQ(logger.get(), "\tat (wasm-function[12])\n"
                          "\tat (wasm-function[11])\n"
                          "\tat (wasm-function[10])\n"
                          "\tat (wasm-function[9])\n"
                          "\tat (wasm-function[8])\n"
                          "\tat (wasm-function[7])\n"
                          "\tat (wasm-function[6])\n"
                          "\tat (wasm-function[5])\n"
                          "\tat (wasm-function[4])\n"
                          "\tat (wasm-function[0])\n");

  logger.clear();
  ASSERT_THROW(SignalFunctionWrapper::call(runtime.getExportedFunctionByName<0>("short")), vb::TrapException);
  runtime.printStacktrace(logger);
  ASSERT_EQ(logger.get(), "\tat (wasm-function[12])\n"
                          "\tat (wasm-function[2])\n");

  logger.clear();
  ASSERT_THROW(SignalFunctionWrapper::call(runtime.getExportedFunctionByName<0>("trap")), vb::TrapException);
  runtime.printStacktrace(logger);
  ASSERT_EQ(logger.get(), "\tat (wasm-function[12])\n");
}

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(TestStacktrace, stacktraceIsCorrect) {
  Compiler compiler = Compiler(memoryFnc, allocFnc, freeFnc, nullptr, memoryFnc);
  compiler.setStacktraceRecordCount(16U);
  ManagedBinary const binaryModule = compiler.compile(moduleWithDebugNamesBytecode);

  ExecutableMemory const executableMemory = ExecutableMemory::make_executable_copy(binaryModule);
  Runtime runtime = test::createRuntime(executableMemory);

  std::vector<uint32_t> stacktrace;
  auto updateStacktrace = [&runtime, &stacktrace]() {
    stacktrace.clear();
    runtime.iterateStacktraceRecords(FunctionRef<void(uint32_t)>([&stacktrace](uint32_t const fncIndex) {
      stacktrace.push_back(fncIndex);
    }));
  };

  updateStacktrace();
  ASSERT_THAT(stacktrace, testing::ElementsAre());

  runtime.start();

  updateStacktrace();
  ASSERT_THAT(stacktrace, testing::ElementsAre());

  ASSERT_THROW(SignalFunctionWrapper::call(runtime.getExportedFunctionByName<0>("long")), vb::TrapException);
  updateStacktrace();
  ASSERT_THAT(stacktrace, testing::ElementsAre(12, 11, 10, 9, 8, 7, 6, 5, 4, 0));

  SignalFunctionWrapper::call(runtime.getExportedFunctionByName<0>("noTrap"));
  updateStacktrace();
  ASSERT_THAT(stacktrace, testing::ElementsAre());

  SignalFunctionWrapper::call(runtime.getExportedFunctionByName<0>("noTrap"));
  updateStacktrace();
  ASSERT_THAT(stacktrace, testing::ElementsAre());

  ASSERT_THROW(SignalFunctionWrapper::call(runtime.getExportedFunctionByName<0>("mid")), vb::TrapException);
  updateStacktrace();
  ASSERT_THAT(stacktrace, testing::ElementsAre(12, 11, 10, 1));

  ASSERT_THROW(SignalFunctionWrapper::call(runtime.getExportedFunctionByName<0>("short")), vb::TrapException);
  updateStacktrace();
  ASSERT_THAT(stacktrace, testing::ElementsAre(12, 2));

  SignalFunctionWrapper::call(runtime.getExportedFunctionByName<0>("noTrap"));
  updateStacktrace();
  ASSERT_THAT(stacktrace, testing::ElementsAre());

  ASSERT_THROW(SignalFunctionWrapper::call(runtime.getExportedFunctionByName<0>("trap")), vb::TrapException);
  updateStacktrace();
  ASSERT_THAT(stacktrace, testing::ElementsAre(12));
}

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(TestStacktrace, truncatedStacktraceIsCorrect) {
  Compiler compiler = Compiler(memoryFnc, allocFnc, freeFnc, nullptr, memoryFnc);
  compiler.setStacktraceRecordCount(3U);
  ManagedBinary const binaryModule = compiler.compile(moduleWithDebugNamesBytecode);

  ExecutableMemory const executableMemory = ExecutableMemory::make_executable_copy(binaryModule);
  Runtime runtime = test::createRuntime(executableMemory);

  std::vector<uint32_t> stacktrace;
  auto updateStacktrace = [&runtime, &stacktrace]() {
    stacktrace.clear();
    runtime.iterateStacktraceRecords(FunctionRef<void(uint32_t)>([&stacktrace](uint32_t const fncIndex) {
      stacktrace.push_back(fncIndex);
    }));
  };

  updateStacktrace();
  ASSERT_THAT(stacktrace, testing::ElementsAre());

  runtime.start();

  updateStacktrace();
  ASSERT_THAT(stacktrace, testing::ElementsAre());

  ASSERT_THROW(SignalFunctionWrapper::call(runtime.getExportedFunctionByName<0>("long")), vb::TrapException);
  updateStacktrace();
  ASSERT_THAT(stacktrace, testing::ElementsAre(12, 11, 10));

  SignalFunctionWrapper::call(runtime.getExportedFunctionByName<0>("noTrap"));
  updateStacktrace();
  ASSERT_THAT(stacktrace, testing::ElementsAre());

  SignalFunctionWrapper::call(runtime.getExportedFunctionByName<0>("noTrap"));
  updateStacktrace();
  ASSERT_THAT(stacktrace, testing::ElementsAre());

  ASSERT_THROW(SignalFunctionWrapper::call(runtime.getExportedFunctionByName<0>("mid")), vb::TrapException);
  updateStacktrace();
  ASSERT_THAT(stacktrace, testing::ElementsAre(12, 11, 10));

  ASSERT_THROW(SignalFunctionWrapper::call(runtime.getExportedFunctionByName<0>("short")), vb::TrapException);
  updateStacktrace();
  ASSERT_THAT(stacktrace, testing::ElementsAre(12, 2));

  SignalFunctionWrapper::call(runtime.getExportedFunctionByName<0>("noTrap"));
  updateStacktrace();
  ASSERT_THAT(stacktrace, testing::ElementsAre());

  ASSERT_THROW(SignalFunctionWrapper::call(runtime.getExportedFunctionByName<0>("trap")), vb::TrapException);
  updateStacktrace();
  ASSERT_THAT(stacktrace, testing::ElementsAre(12));
}

} // namespace stacktrace

#endif // CXX_TARGET == JIT_TARGET
