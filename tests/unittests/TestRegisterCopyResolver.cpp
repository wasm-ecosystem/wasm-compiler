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

#include <gtest/gtest.h>
#include <vector>

#include "src/core/compiler/backend/RegAdapter.hpp"
#include "src/core/compiler/common/RegisterCopyResolver.hpp"
#include "src/core/compiler/common/VariableStorage.hpp"
#if CXX_TARGET == JIT_TARGET
namespace vb {

class SourceDistPair final {
public:
  enum class Type : uint8_t { Input, Move, Swap };

  inline SourceDistPair(TReg const dist, TReg const source, Type const type) VB_NOEXCEPT : dist_(dist), source_(source), type_(type) {
  }

  inline SourceDistPair(TReg const dist, TReg const source) VB_NOEXCEPT : dist_(dist), source_(source), type_(Type::Input) {
  }

  inline bool operator==(const SourceDistPair &other) const VB_NOEXCEPT {
    return (type_ == other.type_) && (dist_ == other.dist_) && (source_ == other.source_);
  }

  inline static SourceDistPair createMove(TReg const dist, TReg const source) VB_NOEXCEPT {
    return SourceDistPair{dist, source, Type::Move};
  }

  inline static SourceDistPair createSwap(TReg const dist, TReg const source) VB_NOEXCEPT {
    return SourceDistPair{dist, source, Type::Swap};
  }

  TReg getDist() const VB_NOEXCEPT {
    return dist_;
  }

  TReg getSource() const VB_NOEXCEPT {
    return source_;
  }

private:
  TReg dist_;
  TReg source_;
  Type type_;
};

struct TestCase final {
  std::vector<SourceDistPair> input;
  std::vector<SourceDistPair> expected;
};

class RegisterAssignmentTest : public testing::TestWithParam<TestCase> {};
auto const &regs = NBackend::WasmABI::gpr;
// NOLINTNEXTLINE(cert-err58-cpp)
INSTANTIATE_TEST_SUITE_P(RegisterCopyResolverTest, RegisterAssignmentTest,
                         testing::Values(TestCase{{{regs[0], regs[1]}, {regs[1], regs[2]}},
                                                  {SourceDistPair::createMove(regs[0], regs[1]), SourceDistPair::createMove(regs[1], regs[2])}},
                                         TestCase{{{regs[1], regs[2]}, {regs[0], regs[1]}},
                                                  {SourceDistPair::createMove(regs[0], regs[1]), SourceDistPair::createMove(regs[1], regs[2])}},
                                         TestCase{{{regs[0], regs[1]}, {regs[1], regs[0]}}, {SourceDistPair::createSwap(regs[0], regs[1])}},
                                         TestCase{
                                             {
                                                 {regs[0], regs[1]},
                                                 {regs[1], regs[2]},
                                                 {regs[2], regs[0]},

                                                 {regs[3], regs[5]},
                                                 {regs[5], regs[4]},
                                                 {regs[4], regs[3]},
                                             },
                                             {SourceDistPair::createSwap(regs[0], regs[1]), SourceDistPair::createSwap(regs[1], regs[2]),
                                              SourceDistPair::createSwap(regs[3], regs[5]), SourceDistPair::createSwap(regs[5], regs[4])},

                                         },
                                         TestCase{
                                             {
                                                 {regs[0], regs[1]},
                                                 {regs[1], regs[2]},
                                                 {regs[2], regs[0]},

                                                 {regs[4], regs[2]},
                                                 {regs[3], regs[2]},
                                             },
                                             {
                                                 SourceDistPair::createMove(regs[4], regs[2]),
                                                 SourceDistPair::createMove(regs[3], regs[2]),
                                                 SourceDistPair::createSwap(regs[0], regs[1]),
                                                 SourceDistPair::createSwap(regs[1], regs[2]),
                                             },

                                         }

                                         ));

TEST_P(RegisterAssignmentTest, TestRegToReg) {
  TestCase const testCase = GetParam();
  RegisterCopyResolver<10U> registerCopyResolver{};

  for (SourceDistPair const &sourceDistPair : testCase.input) {
    registerCopyResolver.push(VariableStorage::reg(sourceDistPair.getDist(), MachineType::I32),
                              VariableStorage::reg(sourceDistPair.getSource(), MachineType::I32));
  }

  std::vector<SourceDistPair> result;

  registerCopyResolver.resolve(
      MoveEmitter([&result](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
        result.emplace_back(SourceDistPair::createMove(targetStorage.location.reg, sourceStorage.location.reg));
      }),
      SwapEmitter([&result](VariableStorage const &targetStorage, VariableStorage const &sourceStorage, bool const swapContains64) {
        static_cast<void>(swapContains64);
        result.emplace_back(SourceDistPair::createSwap(targetStorage.location.reg, sourceStorage.location.reg));
      }));

  ASSERT_EQ(result, testCase.expected);
}
} // namespace vb
#endif