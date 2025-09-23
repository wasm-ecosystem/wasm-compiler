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

#ifndef TESTS_TEST_LOADER
#define TESTS_TEST_LOADER

#include <memory>
#include <string>
#include <vector>

#include "src/core/common/Span.hpp"

namespace vb {

enum class CommandType : uint8_t {
  MODULE = 0,
  ASSERT_RETURN = 1,
  ACTION = 2,
  ASSERT_TRAP = 3,
  ASSERT_EXHAUSTION = 4,
  ASSERT_INVALID = 5,
  ASSERT_UNINSTANTIABLE = 6,
  ASSERT_UNLINKABLE = 7,
  REGISTER = 8,
  ASSERT_MALFORMED = 9,
};

enum class ActionType : uint8_t {
  GET = 0,
  INVOKE = 1,
};

struct Data {
  std::string type;
  std::string value;
  Data() = default;
  Data(std::string _type, std::string _value) : type(_type), value(_value) {
  }
};

struct Action {
  ActionType type = ActionType::GET;
  std::string field;
  std::vector<Data> args;
};

struct Command {
  Command() = default;
  Command(Command const &) = delete;
  Command(Command &&) = delete;
  Command &operator=(Command const &) = delete;
  Command &operator=(Command &&) = delete;
  virtual ~Command() = default;
  virtual CommandType getType() = 0;
  virtual uint32_t getLine() = 0;
};
struct ModuleCommand : public Command {
  ModuleCommand() = default;
  ModuleCommand(ModuleCommand const &) = delete;
  ModuleCommand(ModuleCommand &&) = delete;
  ModuleCommand &operator=(ModuleCommand const &) = delete;
  ModuleCommand &operator=(ModuleCommand &&) = delete;
  ~ModuleCommand() override {
  }
  virtual Span<const uint8_t> getByteCode() = 0;
};
struct AssertCommand : public Command {
  AssertCommand() = default;
  AssertCommand(AssertCommand const &) = delete;
  AssertCommand(AssertCommand &&) = delete;
  AssertCommand &operator=(AssertCommand const &) = delete;
  AssertCommand &operator=(AssertCommand &&) = delete;
  ~AssertCommand() override {
  }
  virtual Action getAction() = 0;
  virtual std::vector<Data> getExpected() = 0;
  virtual std::string getText() = 0;
};
struct InvalidCommand : public Command {
  InvalidCommand() = default;
  InvalidCommand(InvalidCommand const &) = delete;
  InvalidCommand(InvalidCommand &&) = delete;
  InvalidCommand &operator=(InvalidCommand const &) = delete;
  InvalidCommand &operator=(InvalidCommand &&) = delete;
  ~InvalidCommand() override {
  }
  virtual Span<const uint8_t> getByteCode() = 0;
};

class TestLoader {
public:
  TestLoader() = default;
  TestLoader(TestLoader const &) = delete;
  TestLoader(TestLoader &&) = delete;
  TestLoader &operator=(TestLoader const &) = delete;
  TestLoader &operator=(TestLoader &&) = delete;
  virtual ~TestLoader() = default;

  virtual std::unique_ptr<Command> getNextCommand() = 0;
};

} // namespace vb

#endif
