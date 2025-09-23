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

#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "src/core/common/util.hpp"
#include "src/core/runtime/Runtime.hpp"

namespace vb {

struct TestFunctionInfo : public ::testing::Test {
  std::vector<uint8_t> createFunctionBinary(std::string const &signature) {
    // FunctionCallWrapper | FunctionCallWrapperSize | Padding | Signature | SignatureLength
    std::vector<uint8_t> ret{};
    const size_t signatureLengthWithPadding = static_cast<size_t>(roundUpToPow2(static_cast<uint32_t>(signature.size()), 2U));
    ret.resize(4U + signatureLengthWithPadding + 4U);

    ret[0U] = 0x00U;
    ret[1U] = 0x00U;
    ret[2U] = 0x00U;
    ret[3U] = 0x00U;

    for (size_t i = 0; i < signature.size(); i++) {
      ret[4U + i] = static_cast<uint8_t>(signature[i]);
    }

    writeToPtr<uint32_t>(&ret[4U + signatureLengthWithPadding], static_cast<uint32_t>(signature.size()));

    return ret;
  }

  template <class... Arguments> constexpr auto make_args(Arguments... args) const VB_NOEXCEPT -> std::array<WasmValue const, sizeof...(Arguments)> {
    return {{WasmValue(args)...}};
  }
};

TEST_F(TestFunctionInfo, Base) {
  const std::string signature{"()i"};
  const std::vector<uint8_t> binary = createFunctionBinary(signature);
  const FunctionInfo info{vb::pAddI(binary.data(), binary.size()), 0U};

  EXPECT_EQ(info.fncPtr(), binary.data());
  const std::string infoSignature{info.signature().data(), info.signature().size()};
  EXPECT_EQ(infoSignature, signature);
}

TEST_F(TestFunctionInfo, derefAndValidateReturnValue) {
  const std::string signature{"()iIFf"};
  const std::vector<uint8_t> binary = createFunctionBinary(signature);
  const FunctionInfo info{vb::pAddI(binary.data(), binary.size()), 0U};
  const std::array<WasmValue const, 4> resultsData = make_args<int32_t, int64_t, double, float>(1, 2, 3.1, 3.3F);

  std::tuple<int32_t, int64_t, double, float> results1{};
  EXPECT_NO_THROW((info.derefAndValidateReturnValueImpl<0, int32_t, int64_t, double, float>(pCast<uint8_t const *>(resultsData.data()), results1)));
  EXPECT_EQ(std::get<0>(results1), 1);
  EXPECT_EQ(std::get<1>(results1), 2);
  EXPECT_EQ(std::get<2>(results1), 3.1);
  EXPECT_EQ(std::get<3>(results1), 3.3F);

  std::tuple<int32_t, int32_t> results2{};
  EXPECT_THROW((info.derefAndValidateReturnValueImpl<0, int32_t, int32_t>(pCast<uint8_t const *>(resultsData.data()), results2)), RuntimeError);
}

TEST_F(TestFunctionInfo, ValidateEmptyReturn) {
  const std::string signature{"(ii)"};
  const std::vector<uint8_t> binary = createFunctionBinary(signature);
  const FunctionInfo info{vb::pAddI(binary.data(), binary.size()), 0U};
  const std::array<WasmValue const, 0> resultsData = make_args<>();

  std::tuple<> results0{};
  EXPECT_NO_THROW(info.derefAndValidateReturnValueImpl<0>(pCast<uint8_t const *>(resultsData.data()), results0));

  std::tuple<int32_t> results1{};
  EXPECT_THROW((info.derefAndValidateReturnValueImpl<0, int32_t>(pCast<uint8_t const *>(resultsData.data()), results1)), RuntimeError);
}

TEST_F(TestFunctionInfo, ValidateEmptyParameters) {
  const std::string signature{"()"};
  const std::vector<uint8_t> binary = createFunctionBinary(signature);
  const FunctionInfo info{vb::pAddI(binary.data(), binary.size()), 0U};

  EXPECT_NO_THROW((info.validateParameterTypes<>()));
  EXPECT_THROW((info.validateParameterTypes<int32_t>()), RuntimeError);
  EXPECT_THROW((info.validateParameterTypes<int32_t, int32_t>()), RuntimeError);
}

Span<const char> operator""_span(const char *str, std::size_t len) {
  return Span<const char>{str, len};
}

TEST_F(TestFunctionInfo, ValidateParameters) {
  const std::string signature{"(iI)"};
  const std::vector<uint8_t> binary = createFunctionBinary(signature);
  const FunctionInfo info{vb::pAddI(binary.data(), binary.size()), 0U};

  EXPECT_THROW((info.validateParameterTypes<>()), RuntimeError);
  EXPECT_THROW((info.validateParameterTypes<int32_t>()), RuntimeError);
  EXPECT_NO_THROW((info.validateParameterTypes<int32_t, int64_t>()));
  EXPECT_THROW((info.validateParameterTypes<int32_t, int32_t>()), RuntimeError);
  EXPECT_THROW((info.validateParameterTypes<int32_t, int64_t, int32_t>()), RuntimeError);
}

} // namespace vb
