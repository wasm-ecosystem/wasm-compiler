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

#ifndef TESTS_LOADER_STREAM_LOADER
#define TESTS_LOADER_STREAM_LOADER

#include <string>

#include "tests/loader/test_loader.hpp"

namespace vb {

namespace stream {

struct StreamLoader {
  // commandLength(4) | type(1) | line(4)
  explicit StreamLoader(uint8_t const *const _ptr) : ptr(_ptr) {
  }

  uint32_t getCommandLength();
  CommandType getType();
  uint32_t getLine();

  uint8_t const *ptr;
};

struct StreamModuleCommand final : public ModuleCommand {
  // commandLength(4) | type(1) | line(4) | bytecode(span)
  explicit StreamModuleCommand(uint8_t const *const _ptr) : streamLoader(_ptr) {
  }
  Span<const uint8_t> getByteCode() override;
  CommandType getType() override;
  uint32_t getLine() override;

private:
  StreamLoader streamLoader;
};

struct StreamInvalidCommand final : public InvalidCommand {
  // commandLength(4) | type(1) | line(4) | bytecode(span)
  explicit StreamInvalidCommand(uint8_t const *const _ptr) : streamLoader(_ptr) {
  }
  Span<const uint8_t> getByteCode() override;
  CommandType getType() override;
  uint32_t getLine() override;

private:
  StreamLoader streamLoader;
};

struct StreamAssertCommand final : public AssertCommand {
  // commandLength(4) | type(1) | line(4) |
  // actionLength(4) | action_type(1) | action_field(span) | action_args_cout(4) | action_args |
  // expect_cout(4) | expect
  // text(span)
  explicit StreamAssertCommand(uint8_t const *const _ptr) : streamLoader(_ptr) {
  }
  Action getAction() override;
  std::vector<Data> getExpected() override;
  CommandType getType() override;
  uint32_t getLine() override;
  std::string getText() override;

private:
  StreamLoader streamLoader;
};

class StreamTestLoader final : public TestLoader {
public:
  explicit StreamTestLoader(void const *_data);
  StreamTestLoader(StreamTestLoader const &) = delete;
  StreamTestLoader(StreamTestLoader &&) = delete;
  StreamTestLoader &operator=(StreamTestLoader const &) = delete;
  StreamTestLoader &operator=(StreamTestLoader &&) = delete;
  ~StreamTestLoader() override {
  }

  std::unique_ptr<Command> getNextCommand() override;

  uint8_t const *getNextTestcase();
  std::string const &getTestcaseName() const {
    return testcaseName_;
  }

private:
  uint8_t const *nextCommandPtr_;
  std::string testcaseName_;
};
} // namespace stream

} // namespace vb

#endif
