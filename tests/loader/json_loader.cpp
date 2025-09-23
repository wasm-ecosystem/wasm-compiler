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

#include <set>
#include <string>

#include "json_loader.hpp"

#include "src/core/common/util.hpp"

namespace vb {
namespace Json {

CommandType str2commandType(std::string const &typeString) {
  if (typeString == "module") {
    return CommandType::MODULE;
  } else if (typeString == "assert_return") {
    return CommandType::ASSERT_RETURN;
  } else if (typeString == "action") {
    return CommandType::ACTION;
  } else if (typeString == "assert_trap") {
    return CommandType::ASSERT_TRAP;
  } else if (typeString == "assert_exhaustion") {
    return CommandType::ASSERT_EXHAUSTION;
  } else if (typeString == "assert_invalid") {
    return CommandType::ASSERT_INVALID;
  } else if (typeString == "assert_uninstantiable") {
    return CommandType::ASSERT_UNINSTANTIABLE;
  } else if (typeString == "assert_unlinkable") {
    return CommandType::ASSERT_UNLINKABLE;
  } else if (typeString == "register") {
    return CommandType::REGISTER;
  } else if (typeString == "assert_malformed") {
    return CommandType::ASSERT_MALFORMED;
  } else {
    assert(false);
    exit(-1);
  }
}

ActionType str2actionType(std::string const &str) {
  if (str == "get") {
    return ActionType::GET;
  } else if (str == "invoke") {
    return ActionType::INVOKE;
  }
  assert(false);
  // GCOVR_EXCL_START
  UNREACHABLE(_, "unknown type");
  // GCOVR_EXCL_STOP
}

CommandType JsonLoader::getType() {
  std::string const typeString = command_["type"];
  return str2commandType(typeString);
}
uint32_t JsonLoader::getLine() {
  return command_["line"];
}
std::string JsonLoader::getText() {
  if (command_.contains("text")) {
    assert(command_["text"].is_string());
    return command_["text"];
  }
  return "";
}

Span<const uint8_t> JsonModuleCommand::getByteCode() {
  std::string const filename = jsonLoader_.command_["filename"];
  auto it = testDataMapping_->find(filename);
  assert(it != testDataMapping_->end());
  return it->second.m_memObj;
}

Action JsonAssertCommand::getAction() {
  Action action;
  nlohmann::json &actionJson = jsonLoader_.command_["action"];
  action.type = str2actionType(actionJson["type"]);
  action.field = actionJson["field"];
  for (const nlohmann::json &arg : actionJson["args"]) {
    std::string const argType = arg["type"];
    std::string const argValue = arg["value"];
    action.args.emplace_back(argType, argValue);
  }
  return action;
}

std::vector<Data> JsonAssertCommand::getExpected() {
  std::vector<Data> expected;
  for (const auto &arg : jsonLoader_.command_["expected"]) {
    expected.emplace_back(arg["type"], arg["value"]);
  }
  return expected;
}

Span<const uint8_t> JsonInvalidCommand::getByteCode() {
  std::string const filename = jsonLoader_.command_["filename"];
  if (filename.substr(filename.size() - 5, 5) != ".wasm") {
    return Span<const uint8_t>();
  }
  auto it = testDataMapping_->find(filename);
  if (it == testDataMapping_->end()) {
    throw std::runtime_error(filename.c_str());
  }
  return it->second.m_memObj;
}

std::unique_ptr<Command> JsonTestLoader::getNextCommand() {
  if (commandsIt_ == commandsRef_.end()) {
    return nullptr;
  }
  auto commandsIt = commandsIt_;
  commandsIt_++;
  auto const type = str2commandType((*commandsIt)["type"]);
  switch (type) {
  case CommandType::MODULE: {
    return std::make_unique<JsonModuleCommand>(*commandsIt, testDataMapping_);
  }

  case CommandType::ASSERT_RETURN:
  case CommandType::ACTION:
  case CommandType::ASSERT_TRAP:
  case CommandType::ASSERT_EXHAUSTION: {
    return std::make_unique<JsonAssertCommand>(*commandsIt);
  }

  case CommandType::ASSERT_INVALID:
  case CommandType::ASSERT_MALFORMED: {
    return std::make_unique<JsonInvalidCommand>(*commandsIt, testDataMapping_);
  }

  default:
    return nullptr;
  }
}

void JsonTestLoader::reorderCommands(nlohmann::json const &commandsRef) {
  std::vector<std::string> moduleNames{"null"};
  std::vector<std::vector<nlohmann::json>> orderedCommands(1);
  std::set<std::string> ignores{};
  auto orderedCommandsIt = orderedCommands.rbegin();
  for (auto &command : commandsRef) {
    if (command["type"] == "module") {
      if (command.contains("name") && command.at("name").is_string()) {
        std::string const name = command["name"];
        moduleNames.emplace_back(name);
      } else {
        moduleNames.emplace_back("!___def");
      }
      orderedCommands.emplace_back();
      orderedCommandsIt = orderedCommands.rbegin();
      orderedCommandsIt->emplace_back(command);
    } else if (command.contains("action") && command.at("action").is_object() && command.at("action").contains("module") &&
               command.at("action").at("module").is_string()) {
      std::string const name = command["action"]["module"];
      if (ignores.find(name) != ignores.end()) {
        continue;
      }
      auto it = std::find(moduleNames.begin(), moduleNames.end(), name);
      if (it == moduleNames.end()) {
        throw std::runtime_error("cannot find module: " + name);
      }
      auto distance = static_cast<std::size_t>(std::distance(moduleNames.begin(), it));
      orderedCommands[distance].emplace_back(command);
    } else if (command.contains("type") && command.contains("name") && command["type"] == "register" && command["name"].is_string()) {
      ignores.emplace(command["name"]);
    } else {
      orderedCommandsIt->emplace_back(command);
    }
  }
  for (auto &commandsInModule : orderedCommands) {
    commandsRef_.insert(commandsRef_.end(), commandsInModule.begin(), commandsInModule.end());
  }
  commandsIt_ = commandsRef_.begin();
}

} // namespace Json
} // namespace vb
