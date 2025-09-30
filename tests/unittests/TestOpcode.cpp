/*
 * Copyright (C) 2025 Wasm ecosystem
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

#include <gtest/gtest.h>
#include <vector>

#include "src/core/compiler/common/OPCode.hpp"
#if CXX_TARGET == JIT_TARGET
namespace vb {

class OpcodeType final {
public:
  enum class Type : uint8_t { Input, Move, Swap };

  inline OpcodeType(vb::OPCode const opCode, bool const isDivInt32, bool const isDivInt, bool const isLoadFloat, bool const isLoad32) VB_NOEXCEPT
      : opCode_(opCode),
        isDivInt32_(isDivInt32),
        isDivInt_(isDivInt),
        isLoadFloat_(isLoadFloat),
        isLoad32_(isLoad32) {
  }

  inline vb::OPCode getOpcode() const VB_NOEXCEPT {
    return opCode_;
  }
  inline bool isDivInt32() const VB_NOEXCEPT {
    return isDivInt32_;
  }
  inline bool isDivInt() const VB_NOEXCEPT {
    return isDivInt_;
  }
  inline bool isLoadFloat() const VB_NOEXCEPT {
    return isLoadFloat_;
  }
  inline bool isLoad32() const VB_NOEXCEPT {
    return isLoad32_;
  }

private:
  vb::OPCode opCode_;
  bool isDivInt32_;
  bool isDivInt_;
  bool isLoadFloat_;
  bool isLoad32_;
};

class OpcodeTest : public testing::TestWithParam<OpcodeType> {};

// NOLINTNEXTLINE(cert-err58-cpp)
INSTANTIATE_TEST_SUITE_P(OpcodeTest, OpcodeTest,
                         testing::Values(
                             // Load opcodes
                             OpcodeType{OPCode::I32_LOAD, false, false, false, true}, OpcodeType{OPCode::I64_LOAD, false, false, false, false},
                             OpcodeType{OPCode::F32_LOAD, false, false, true, true}, OpcodeType{OPCode::F64_LOAD, false, false, true, false},
                             OpcodeType{OPCode::I32_LOAD8_S, false, false, false, true}, OpcodeType{OPCode::I32_LOAD8_U, false, false, false, true},
                             OpcodeType{OPCode::I32_LOAD16_S, false, false, false, true}, OpcodeType{OPCode::I32_LOAD16_U, false, false, false, true},
                             OpcodeType{OPCode::I64_LOAD8_S, false, false, false, false}, OpcodeType{OPCode::I64_LOAD8_U, false, false, false, false},
                             OpcodeType{OPCode::I64_LOAD16_S, false, false, false, false},
                             OpcodeType{OPCode::I64_LOAD16_U, false, false, false, false},
                             OpcodeType{OPCode::I64_LOAD32_S, false, false, false, false},
                             OpcodeType{OPCode::I64_LOAD32_U, false, false, false, false},
                             // Division opcodes
                             OpcodeType{OPCode::I32_DIV_S, true, true, false, false}, OpcodeType{OPCode::I32_DIV_U, true, true, false, false},
                             OpcodeType{OPCode::I32_REM_S, true, true, false, false}, OpcodeType{OPCode::I32_REM_U, true, true, false, false},
                             OpcodeType{OPCode::I64_DIV_S, false, true, false, false}, OpcodeType{OPCode::I64_DIV_U, false, true, false, false},
                             OpcodeType{OPCode::I64_REM_S, false, true, false, false}, OpcodeType{OPCode::I64_REM_U, false, true, false, false}));

TEST_P(OpcodeTest, TestOpCodeIsDivAndLoad) {
  OpcodeType const testCase = GetParam();

  EXPECT_EQ(vb::opcodeIsDivInt32(testCase.getOpcode()), testCase.isDivInt32());
  EXPECT_EQ(vb::opcodeIsDivInt(testCase.getOpcode()), testCase.isDivInt());
  if (testCase.isDivInt32()) {
    ASSERT_TRUE(vb::opcodeIsDivInt(testCase.getOpcode()));
  }
  EXPECT_EQ(vb::opcodeIsLoadFloat(testCase.getOpcode()), testCase.isLoadFloat());
  EXPECT_EQ(vb::opcodeIsLoad32(testCase.getOpcode()), testCase.isLoad32());
}
} // namespace vb
#endif