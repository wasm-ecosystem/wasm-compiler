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

#include <array>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <set>
#include <string>
#include <type_traits>

#ifdef JIT_TARGET_TRICORE

#include "src/core/compiler/backend/tricore/tricore_cc.hpp"
#include "src/core/compiler/backend/tricore/tricore_encoding.hpp"

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(TricoreDr, NextIsExtendRegOfPrev) {
  using namespace vb::tc;
  using namespace vb::tc::WasmABI;

  for (size_t i = 0; i < dr.size() - 1U; i += 2) {
    SCOPED_TRACE("i is " + std::to_string(i));
    EXPECT_EQ(RegUtil::getOtherExtReg(dr[i]), dr[i + 1]);
  }
}

template <class T1, class T2, size_t N> bool operator==(std::array<T1, N> const &a, std::array<T2, N> const &b) {
  for (size_t i = 0; i < N; i++) {
    if (a[i] != b[i]) {
      return false;
    }
  }
  return true;
}

constexpr const size_t maxInRegGlobal = 1U;

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(Tricore, CallRegsIsNotParameterReg) {
  using namespace vb::tc;
  using namespace testing;

  std::set<REG> wasmAbiParamRegs{};
  for (size_t i = 0; i < maxInRegGlobal + WasmABI::regsForParams; i++) {
    wasmAbiParamRegs.insert(WasmABI::dr[i]);
  }

  for (REG const callSrcReg : callScrRegs) {
    SCOPED_TRACE(std::to_string(static_cast<uint32_t>(callSrcReg)));
    EXPECT_THAT(wasmAbiParamRegs, Not(Contains(callSrcReg)));
    EXPECT_THAT(NativeABI::paramRegs, Not(Contains(callSrcReg)));
  }
}

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(Tricore, CallRegsIsNotReturnReg) {
  using namespace vb::tc;
  using namespace testing;

  for (REG const callSrcReg : callScrRegs) {
    SCOPED_TRACE(std::to_string(static_cast<uint32_t>(callSrcReg)));
    EXPECT_THAT(WasmABI::REGS::returnValueRegs, Not(Contains(callSrcReg)));
    EXPECT_NE(NativeABI::retReg, callSrcReg);
  }
}

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(Tricore, CallRegsIsNotIndirectCallReg) {
  using namespace vb::tc;
  using namespace testing;

  for (REG const callSrcReg : callScrRegs) {
    SCOPED_TRACE(std::to_string(static_cast<uint32_t>(callSrcReg)));
    EXPECT_NE(WasmABI::REGS::indirectCallReg, callSrcReg);
  }
}

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(Tricore, ReturnValueArgMustBeReserved) {
  using namespace vb::tc;
  using namespace testing;

  for (REG const returnValueReg : WasmABI::REGS::returnValueRegs) {
    EXPECT_EQ(WasmABI::isResScratchReg(returnValueReg), true);
  }
}

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(Tricore, D4AndD5NotAsParamReg) {
  // use D4 and D5 as wasm internal parameter reg can leads to register overwritten risk when indirect call native import function
  using namespace vb::tc;
  using namespace testing;
  ASSERT_GE(WasmABI::getRegPos(REG::D4), WasmABI::regsForParams);
  ASSERT_GE(WasmABI::getRegPos(REG::D5), WasmABI::regsForParams);
}

TEST(Tricore, StackTrapDontOverwriteGPR0) {
  using namespace vb::tc;
  using namespace testing;
  EXPECT_NE(StackTrace::targetReg, WasmABI::dr[0]);
  EXPECT_NE(StackTrace::frameRefReg, WasmABI::dr[0]);
  EXPECT_NE(StackTrace::counterReg, WasmABI::dr[0]);
  EXPECT_NE(StackTrace::scratchReg, WasmABI::dr[0]);
}

TEST(Tricore, D15IsLastAllowedRegForLocal) {
  using namespace vb::tc;
  using namespace testing;
  uint32_t const position{WasmABI::numGPR - WasmABI::resScratchRegsGPR - 1};
  EXPECT_EQ(WasmABI::dr[position], REG::D15);
}
#endif

#ifdef JIT_TARGET_AARCH64

#include "src/core/compiler/backend/aarch64/aarch64_cc.hpp"
#include "src/core/compiler/backend/aarch64/aarch64_encoding.hpp"

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(Aarch64, ReturnValueArgMustBeReserved) {
  using namespace vb::aarch64;
  using namespace testing;

  for (REG const returnValueReg : WasmABI::REGS::gpRetRegs) {
    EXPECT_EQ(WasmABI::isResScratchReg(returnValueReg), true);
  }

  for (REG const returnValueReg : WasmABI::REGS::fpRetRegs) {
    EXPECT_EQ(WasmABI::isResScratchReg(returnValueReg), true);
  }
}

TEST(Aarch64, StackTrapDontOverwriteGPR0) {
  using namespace vb::aarch64;
  using namespace testing;
  EXPECT_NE(StackTrace::targetReg, WasmABI::gpr[0]);
  EXPECT_NE(StackTrace::frameRefReg, WasmABI::gpr[0]);
  EXPECT_NE(StackTrace::counterReg, WasmABI::gpr[0]);
  EXPECT_NE(StackTrace::scratchReg, WasmABI::gpr[0]);
}

#endif

#ifdef JIT_TARGET_X86_64

#include "src/core/compiler/backend/x86_64/x86_64_cc.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_encoding.hpp"

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(X86_64, ReturnValueArgMustBeReserved) {
  using namespace vb::x86_64;
  using namespace testing;

  for (REG const returnValueReg : WasmABI::REGS::gpRetRegs) {
    EXPECT_EQ(WasmABI::isResScratchReg(returnValueReg), true);
  }

  for (REG const returnValueReg : WasmABI::REGS::fpRetRegs) {
    EXPECT_EQ(WasmABI::isResScratchReg(returnValueReg), true);
  }
}

TEST(X86_64, StackTrapDontOverwriteGPR0) {
  using namespace vb::x86_64;
  using namespace testing;
  EXPECT_NE(StackTrace::frameRefReg, WasmABI::gpr[0]);
  EXPECT_NE(StackTrace::counterReg, WasmABI::gpr[0]);
  EXPECT_NE(StackTrace::scratchReg, WasmABI::gpr[0]);
}
#endif

#ifdef JIT_TARGET_AARCH64

#include "src/core/compiler/backend/aarch64/aarch64_cc.hpp"

// NOLINTNEXTLINE(cert-err58-cpp, cppcoreguidelines-special-member-functions)
TEST(Aarch64, firstGPRIsNonVolatile) {
  // currently we put first global to first gpr, so need to guarantee that first gpr is a non-volatile register
  // see commit 1786f9e650e0c8a0f7590db95c7b005cc0260cd3
  using namespace vb::aarch64;
  using namespace testing;

  EXPECT_THAT(NativeABI::nonvolRegs, Contains(WasmABI::gpr[0]));
}

#endif
