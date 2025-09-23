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

#ifndef TESTS_LOADER_JSON_LOADER
#define TESTS_LOADER_JSON_LOADER

#include <nlohmann/json.hpp>

#include "tests/TestData.hpp"
#include "tests/loader/test_loader.hpp"

namespace vb {

namespace Json {

struct JsonLoader {
  explicit JsonLoader(nlohmann::json &_commandIt) : command_(_commandIt) {
  }

  CommandType getType();
  uint32_t getLine();
  std::string getText();

  nlohmann::json &command_;
};

struct JsonModuleCommand final : public ModuleCommand {
  explicit JsonModuleCommand(nlohmann::json &_commandIt, std::shared_ptr<TestDataMapping> _testDataMapping)
      : jsonLoader_(_commandIt), testDataMapping_(_testDataMapping) {
  }
  Span<const uint8_t> getByteCode() override;

  CommandType getType() override {
    return jsonLoader_.getType();
  }
  uint32_t getLine() override {
    return jsonLoader_.getLine();
  }

private:
  JsonLoader jsonLoader_;
  std::shared_ptr<TestDataMapping> testDataMapping_;
};

struct JsonAssertCommand final : public AssertCommand {
  explicit JsonAssertCommand(nlohmann::json &_commandIt) : jsonLoader_(_commandIt) {
  }

  Action getAction() override;
  std::vector<Data> getExpected() override;
  CommandType getType() override {
    return jsonLoader_.getType();
  }
  uint32_t getLine() override {
    return jsonLoader_.getLine();
  }
  std::string getText() override {
    return jsonLoader_.getText();
  }

private:
  JsonLoader jsonLoader_;
};

struct JsonInvalidCommand final : public InvalidCommand {
  explicit JsonInvalidCommand(nlohmann::json &_commandIt, std::shared_ptr<TestDataMapping> _testDataMapping)
      : jsonLoader_(_commandIt), testDataMapping_(_testDataMapping) {
  }
  Span<const uint8_t> getByteCode() override;
  CommandType getType() override {
    return jsonLoader_.getType();
  }
  uint32_t getLine() override {
    return jsonLoader_.getLine();
  }

private:
  JsonLoader jsonLoader_;
  std::shared_ptr<TestDataMapping> testDataMapping_;
};

class JsonTestLoader final : public TestLoader {
public:
  explicit JsonTestLoader(nlohmann::json const &_testJson, std::shared_ptr<TestDataMapping> _testData) : testDataMapping_(_testData) {
    reorderCommands(_testJson["commands"]);
  }
  JsonTestLoader(JsonTestLoader const &) = delete;
  JsonTestLoader(JsonTestLoader &&) = delete;
  JsonTestLoader &operator=(JsonTestLoader const &) = delete;
  JsonTestLoader &operator=(JsonTestLoader &&) = delete;
  ~JsonTestLoader() override {
  }

  std::unique_ptr<Command> getNextCommand() override;

private:
  void reorderCommands(nlohmann::json const &commandsRef);

  std::vector<nlohmann::json> commandsRef_;
  std::vector<nlohmann::json>::iterator commandsIt_;

  std::shared_ptr<TestDataMapping> testDataMapping_;
};
} // namespace Json
} // namespace vb

#endif
