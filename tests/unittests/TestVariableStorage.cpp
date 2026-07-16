#include <gtest/gtest.h>

#include "src/core/compiler/common/VariableStorage.hpp"

namespace vb {

namespace {

constexpr TReg firstTestReg{static_cast<TReg>(0U)};
constexpr TReg secondTestReg{static_cast<TReg>(1U)};

} // namespace

TEST(TestVariableStorage, OverlapsWithRejectsUnsupportedLocations) {
  VariableStorage const reg{VariableStorage::reg(MachineType::I32, firstTestReg)};
  VariableStorage const stackSlot{VariableStorage::stackMemory(MachineType::I32, 0U)};

  ASSERT_FALSE(VariableStorage::i32Const(1U).overlapsWith(VariableStorage::i32Const(1U)));
  ASSERT_FALSE(VariableStorage{}.overlapsWith(stackSlot));
  ASSERT_FALSE(reg.overlapsWith(VariableStorage::i32Const(1U)));
}

TEST(TestVariableStorage, OverlapsWithComparesRegistersByLocationOnly) {
  VariableStorage const sameRegI32{VariableStorage::reg(MachineType::I32, firstTestReg)};
  VariableStorage const sameRegI64{VariableStorage::reg(MachineType::I64, firstTestReg)};
  VariableStorage const otherReg{VariableStorage::reg(MachineType::I32, secondTestReg)};

  ASSERT_TRUE(sameRegI32.overlapsWith(sameRegI64));
  ASSERT_TRUE(sameRegI64.overlapsWith(sameRegI32));
  ASSERT_FALSE(sameRegI32.overlapsWith(otherReg));
}

TEST(TestVariableStorage, OverlapsWithHandlesStackMemoryBoundaries) {
  VariableStorage const i32At0{VariableStorage::stackMemory(MachineType::I32, 0U)};
  VariableStorage const i64At0{VariableStorage::stackMemory(MachineType::I64, 0U)};
  VariableStorage const i32At2{VariableStorage::stackMemory(MachineType::I32, 2U)};
  VariableStorage const i32At3{VariableStorage::stackMemory(MachineType::I32, 3U)};
  VariableStorage const i32At4{VariableStorage::stackMemory(MachineType::I32, 4U)};
  VariableStorage const i32At7{VariableStorage::stackMemory(MachineType::I32, 7U)};
  VariableStorage const i32At8{VariableStorage::stackMemory(MachineType::I32, 8U)};

  ASSERT_TRUE(i32At0.overlapsWith(i32At0));
  ASSERT_TRUE(i64At0.overlapsWith(i32At0));
  ASSERT_TRUE(i64At0.overlapsWith(i32At2));
  ASSERT_TRUE(i32At3.overlapsWith(i32At0));
  ASSERT_TRUE(i64At0.overlapsWith(i32At7));

  ASSERT_TRUE(i32At2.overlapsWith(i64At0));
  ASSERT_TRUE(i32At7.overlapsWith(i64At0));

  ASSERT_FALSE(i32At0.overlapsWith(i32At4));
  ASSERT_FALSE(i64At0.overlapsWith(i32At8));
}

TEST(TestVariableStorage, OverlapsWithHandlesLinkDataBoundaries) {
  VariableStorage const i32At16{VariableStorage::linkData(MachineType::I32, 16U)};
  VariableStorage const i64At16{VariableStorage::linkData(MachineType::I64, 16U)};
  VariableStorage const i32At19{VariableStorage::linkData(MachineType::I32, 19U)};
  VariableStorage const i32At20{VariableStorage::linkData(MachineType::I32, 20U)};
  VariableStorage const i32At23{VariableStorage::linkData(MachineType::I32, 23U)};
  VariableStorage const i32At24{VariableStorage::linkData(MachineType::I32, 24U)};

  ASSERT_TRUE(i32At16.overlapsWith(i32At16));
  ASSERT_TRUE(i64At16.overlapsWith(i32At19));
  ASSERT_TRUE(i64At16.overlapsWith(i32At23));
  ASSERT_TRUE(i32At23.overlapsWith(i64At16));

  ASSERT_FALSE(i32At16.overlapsWith(i32At20));
  ASSERT_FALSE(i64At16.overlapsWith(i32At24));
}

TEST(TestVariableStorage, OverlapsWithRejectsDifferentLocationClasses) {
  VariableStorage const stackSlot{VariableStorage::stackMemory(MachineType::I32, 0U)};
  VariableStorage const linkDataSlot{VariableStorage::linkData(MachineType::I32, 0U)};
  VariableStorage const reg{VariableStorage::reg(MachineType::I32, firstTestReg)};

  ASSERT_FALSE(stackSlot.overlapsWith(linkDataSlot));
  ASSERT_FALSE(linkDataSlot.overlapsWith(stackSlot));
  ASSERT_FALSE(reg.overlapsWith(stackSlot));
}

} // namespace vb