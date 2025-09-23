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

#include <cassert>
#include <cstdint>
#include <iostream>

#include "stream_loader.hpp"

#include "src/core/common/Span.hpp"

namespace vb {
namespace stream {

struct BigEndianReader {
  inline static uint8_t readU8(uint8_t const *const ptr, uint32_t &offset) {
    uint32_t const off = offset;
    offset++;
    return ptr[off];
  }
  inline static uint16_t readU16(uint8_t const *const ptr, uint32_t &offset) {
    uint8_t const f1 = readU8(ptr, offset);
    uint8_t const f2 = readU8(ptr, offset);
    uint32_t const res = (static_cast<uint32_t>(f1) << 8U) + static_cast<uint32_t>(f2);
    return static_cast<uint16_t>(res);
  }
  inline static uint32_t readU32(uint8_t const *const ptr, uint32_t &offset) {
    uint16_t const f1 = readU16(ptr, offset);
    uint16_t const f2 = readU16(ptr, offset);
    return static_cast<uint32_t>(static_cast<uint32_t>(f1) << static_cast<uint32_t>(16)) + static_cast<uint32_t>(f2);
  }
  template <typename T = uint8_t> inline static Span<const T> readSpan(uint8_t const *const ptr, uint32_t &offset) {
    uint32_t const len = readU32(ptr, offset);
    Span<T const> span = Span<T const>(static_cast<T const *>(static_cast<void const *>(ptr + offset)), len);
    offset += len;
    return span;
  }
  inline static std::string readString(uint8_t const *ptr, uint32_t &offset) {
    Span<const char> const span = readSpan<const char>(ptr, offset);
    return std::string(span.data(), span.size());
  }
};

constexpr uint32_t typeOffset = 4U;
constexpr uint32_t lineOffset = typeOffset + 1U;
uint32_t StreamLoader::getCommandLength() {
  uint32_t offset = 0U;
  return BigEndianReader::readU32(ptr, offset);
}
CommandType StreamLoader::getType() {
  uint32_t offset = typeOffset;
  return static_cast<CommandType>(BigEndianReader::readU8(ptr, offset));
}
uint32_t StreamLoader::getLine() {
  uint32_t offset = lineOffset;
  return BigEndianReader::readU32(ptr, offset);
}

constexpr uint32_t byteCodeOffset = lineOffset + 4U;
Span<const uint8_t> StreamModuleCommand::getByteCode() {
  uint32_t offset = byteCodeOffset;
  return BigEndianReader::readSpan(streamLoader.ptr, offset);
}
CommandType StreamModuleCommand::getType() {
  return streamLoader.getType();
}
uint32_t StreamModuleCommand::getLine() {
  return streamLoader.getLine();
}

Span<const uint8_t> StreamInvalidCommand::getByteCode() {
  uint32_t offset = byteCodeOffset;
  return BigEndianReader::readSpan(streamLoader.ptr, offset);
}
CommandType StreamInvalidCommand::getType() {
  return streamLoader.getType();
}
uint32_t StreamInvalidCommand::getLine() {
  return streamLoader.getLine();
}

constexpr uint32_t actionOffset = lineOffset + 4U;
CommandType StreamAssertCommand::getType() {
  return streamLoader.getType();
}
uint32_t StreamAssertCommand::getLine() {
  return streamLoader.getLine();
}
Action StreamAssertCommand::getAction() {
  uint32_t offset = actionOffset;
  uint32_t const actionLength = BigEndianReader::readU32(streamLoader.ptr, offset);
  static_cast<void>(actionLength);
  Action action;
  action.type = static_cast<ActionType>(BigEndianReader::readU8(streamLoader.ptr, offset));
  Span<const char> const fieldSpan = BigEndianReader::readSpan<const char>(streamLoader.ptr, offset);
  action.field = std::string(fieldSpan.data(), fieldSpan.size());
  uint32_t const argsLength = BigEndianReader::readU32(streamLoader.ptr, offset);
  action.args.resize(argsLength);
  for (Data &arg : action.args) {
    arg.type = BigEndianReader::readString(streamLoader.ptr, offset);
    arg.value = BigEndianReader::readString(streamLoader.ptr, offset);
  }
  return action;
}
std::vector<Data> StreamAssertCommand::getExpected() {
  uint32_t offset = actionOffset;
  Span<const uint8_t> const actionSpan = BigEndianReader::readSpan(streamLoader.ptr, offset);
  static_cast<void>(actionSpan);
  uint32_t const argsLength = BigEndianReader::readU32(streamLoader.ptr, offset);
  std::vector<Data> expected(argsLength);
  for (Data &expect : expected) {
    expect.type = BigEndianReader::readString(streamLoader.ptr, offset);
    expect.value = BigEndianReader::readString(streamLoader.ptr, offset);
  }
  return expected;
}
std::string StreamAssertCommand::getText() {
  uint32_t offset = actionOffset;

  static_cast<void>(BigEndianReader::readSpan(streamLoader.ptr, offset));
  uint32_t const argsLength = BigEndianReader::readU32(streamLoader.ptr, offset);
  for (uint32_t i = 0; i < argsLength; ++i) {
    static_cast<void>(BigEndianReader::readString(streamLoader.ptr, offset)); // type
    static_cast<void>(BigEndianReader::readString(streamLoader.ptr, offset)); // value
  }

  return BigEndianReader::readString(streamLoader.ptr, offset);
}

StreamTestLoader::StreamTestLoader(void const *_data) {
  uint8_t const *const data = static_cast<uint8_t const *>(_data);
  uint32_t offset = 0;
  testcaseName_ = BigEndianReader::readString(data, offset);
  nextCommandPtr_ = data + offset;
}

constexpr uint32_t commandLengthOfLength = 4U;
std::unique_ptr<Command> StreamTestLoader::getNextCommand() {
  uint8_t const *const ptr = nextCommandPtr_;
  uint32_t const commandLength = StreamLoader(ptr).getCommandLength();
  if (commandLength == 0U) {
    return nullptr;
  } // length == 0 means end

  nextCommandPtr_ += commandLengthOfLength + commandLength; // offset to nextCommand

  CommandType const type = StreamLoader(ptr).getType();
  switch (type) {
  case CommandType::MODULE: {
    return std::make_unique<StreamModuleCommand>(ptr);
  }

  case CommandType::ASSERT_RETURN:
  case CommandType::ACTION:
  case CommandType::ASSERT_TRAP:
  case CommandType::ASSERT_EXHAUSTION: {
    return std::make_unique<StreamAssertCommand>(ptr);
  }

  case CommandType::ASSERT_INVALID:
  case CommandType::ASSERT_MALFORMED: {
    return std::make_unique<StreamInvalidCommand>(ptr);
  }

  // GCOVR_EXCL_START
  default: {
    assert(false && "unknown test command");
    return nullptr;
  }
    // GCOVR_EXCL_STOP
  }
}

const uint8_t *StreamTestLoader::getNextTestcase() {
  assert(StreamLoader(this->nextCommandPtr_).getCommandLength() == 0U);
  return this->nextCommandPtr_ + commandLengthOfLength;
}

} // namespace stream
} // namespace vb
