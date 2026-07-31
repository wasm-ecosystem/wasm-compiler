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
#include <new>
#include <vector>

#include "src/core/compiler/backend/RegAdapter.hpp"
#include "src/core/compiler/common/ParallelMoveResolver.hpp"
#include "src/core/compiler/common/VariableStorage.hpp"

#if CXX_TARGET == JIT_TARGET
namespace vb {

class MoveOperation final {
public:
  inline MoveOperation(VariableStorage const &target, VariableStorage const &source) VB_NOEXCEPT : target_(target), source_(source) {
  }

  inline bool operator==(MoveOperation const &other) const VB_NOEXCEPT {
    return target_.equals(other.target_) && source_.equals(other.source_);
  }

private:
  VariableStorage target_;
  VariableStorage source_;
};

class SwapOperation final {
public:
  inline SwapOperation(VariableStorage const &target, VariableStorage const &source, bool const contains64) VB_NOEXCEPT : target_(target),
                                                                                                                          source_(source),
                                                                                                                          contains64_(contains64) {
  }

  inline bool operator==(SwapOperation const &other) const VB_NOEXCEPT {
    return target_.equals(other.target_) && source_.equals(other.source_) && (contains64_ == other.contains64_);
  }

private:
  VariableStorage target_;
  VariableStorage source_;
  bool contains64_;
};

auto const &parallelMoveRegs = NBackend::WasmABI::gpr;

/// @brief Test allocator forwarding to the global operator new, matching the AllocFnc signature.
static void *testCompilerAlloc(uint32_t const size, void *const ctx) VB_NOEXCEPT {
  static_cast<void>(ctx);
  return ::operator new(static_cast<size_t>(size), std::nothrow);
}

/// @brief Test deallocator forwarding to the global operator delete, matching the FreeFnc signature.
static void testCompilerFree(void *const ptr, void *const ctx) VB_NOEXCEPT {
  static_cast<void>(ctx);
  ::operator delete(ptr);
}

TEST(ParallelMoveResolverTest, ResolvesRegisterStackCycleWithTemp) {
  // Mixed register/stack cycle: the temp must break the dependency before the rotation can start.
  ParallelMoveResolver resolver{&testCompilerAlloc, &testCompilerFree, nullptr, 2U};
  VariableStorage const stackSlot{VariableStorage::stackMemory(MachineType::I64, 0U)};
  VariableStorage const sourceReg{VariableStorage::reg(MachineType::I64, parallelMoveRegs[1])};
  VariableStorage const tempReg{VariableStorage::reg(MachineType::I64, parallelMoveRegs[5])};

  resolver.push(stackSlot, sourceReg);
  resolver.push(sourceReg, stackSlot);

  std::vector<MoveOperation> result;
  resolver.resolveLinear(ParallelMoveEmitter([&result](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
    result.emplace_back(targetStorage, sourceStorage);
  }));
  resolver.resolveCycle(ParallelMoveEmitter([&result](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
                          result.emplace_back(targetStorage, sourceStorage);
                        }),
                        ParallelMoveTempProvider([tempReg](VariableStorage const &sourceStorage) {
                          return VariableStorage::reg(sourceStorage.machineType, tempReg.location.reg);
                        }));

  std::vector<MoveOperation> const expected{MoveOperation(tempReg, sourceReg), MoveOperation(sourceReg, stackSlot),
                                            MoveOperation(stackSlot, tempReg)};
  ASSERT_EQ(result, expected);
}

TEST(ParallelMoveResolverTest, ResolvesStackCycleWithTemp) {
  // Pure stack cycle still resolves through an explicit temp register.
  ParallelMoveResolver resolver{&testCompilerAlloc, &testCompilerFree, nullptr, 2U};
  VariableStorage const lowSlot{VariableStorage::stackMemory(MachineType::I32, 0U)};
  VariableStorage const highSlot{VariableStorage::stackMemory(MachineType::I32, 8U)};
  VariableStorage const tempReg{VariableStorage::reg(MachineType::I32, parallelMoveRegs[5])};

  resolver.push(lowSlot, highSlot);
  resolver.push(highSlot, lowSlot);

  std::vector<MoveOperation> result;
  resolver.resolveLinear(ParallelMoveEmitter([&result](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
    result.emplace_back(targetStorage, sourceStorage);
  }));
  resolver.resolveCycle(ParallelMoveEmitter([&result](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
                          result.emplace_back(targetStorage, sourceStorage);
                        }),
                        ParallelMoveTempProvider([tempReg](VariableStorage const &sourceStorage) {
                          return VariableStorage::reg(sourceStorage.machineType, tempReg.location.reg);
                        }));

  std::vector<MoveOperation> const expected{MoveOperation(tempReg, highSlot), MoveOperation(highSlot, lowSlot), MoveOperation(lowSlot, tempReg)};
  ASSERT_EQ(result, expected);
}

TEST(ParallelMoveResolverTest, HandlesNoOpMovesAndSimpleLeaf) {
  // No-op moves are dropped at push time, so only the remaining leaf move should be emitted.
  ParallelMoveResolver resolver{&testCompilerAlloc, &testCompilerFree, nullptr, 2U};
  VariableStorage const targetReg{VariableStorage::reg(MachineType::I64, parallelMoveRegs[0])};
  VariableStorage const sourceReg{VariableStorage::reg(MachineType::I64, parallelMoveRegs[2])};
  VariableStorage const noopStack{VariableStorage::stackMemory(MachineType::I32, 32U)};
  VariableStorage const tempReg{VariableStorage::reg(MachineType::I64, parallelMoveRegs[5])};

  resolver.push(noopStack, noopStack);
  resolver.push(targetReg, sourceReg);

  std::vector<MoveOperation> result;
  resolver.resolveLinear(ParallelMoveEmitter([&result](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
    result.emplace_back(targetStorage, sourceStorage);
  }));
  resolver.resolveCycle(ParallelMoveEmitter([&result](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
                          result.emplace_back(targetStorage, sourceStorage);
                        }),
                        ParallelMoveTempProvider([tempReg](VariableStorage const &sourceStorage) {
                          return VariableStorage::reg(sourceStorage.machineType, tempReg.location.reg);
                        }));

  std::vector<MoveOperation> const expected{MoveOperation(targetReg, sourceReg)};
  ASSERT_EQ(result, expected);
}

TEST(ParallelMoveResolverTest, ResolvesRegisterCycleWithTemp) {
  // Simple two-register cycle: save one source to temp, rotate the other edge, then restore the head.
  ParallelMoveResolver resolver{&testCompilerAlloc, &testCompilerFree, nullptr, 2U};
  VariableStorage const firstReg{VariableStorage::reg(MachineType::I32, parallelMoveRegs[0])};
  VariableStorage const secondReg{VariableStorage::reg(MachineType::I32, parallelMoveRegs[1])};
  VariableStorage const tempReg{VariableStorage::reg(MachineType::I32, parallelMoveRegs[5])};

  resolver.push(firstReg, secondReg);
  resolver.push(secondReg, firstReg);

  std::vector<MoveOperation> result;
  resolver.resolveLinear(ParallelMoveEmitter([&result](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
    result.emplace_back(targetStorage, sourceStorage);
  }));
  resolver.resolveCycle(ParallelMoveEmitter([&result](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
                          result.emplace_back(targetStorage, sourceStorage);
                        }),
                        ParallelMoveTempProvider([tempReg](VariableStorage const &sourceStorage) {
                          return VariableStorage::reg(sourceStorage.machineType, tempReg.location.reg);
                        }));

  std::vector<MoveOperation> const expected{MoveOperation(tempReg, secondReg), MoveOperation(secondReg, firstReg), MoveOperation(firstReg, tempReg)};
  ASSERT_EQ(result, expected);
}

TEST(ParallelMoveResolverTest, ResolvesTwoCyclesSequentially) {
  // Remaining cycles are handled one after another once the previous cycle has been fully rotated out.
  ParallelMoveResolver resolver{&testCompilerAlloc, &testCompilerFree, nullptr, 4U};
  VariableStorage const firstCycleFirstReg{VariableStorage::reg(MachineType::I32, parallelMoveRegs[0])};
  VariableStorage const firstCycleSecondReg{VariableStorage::reg(MachineType::I32, parallelMoveRegs[1])};
  VariableStorage const secondCycleLowSlot{VariableStorage::stackMemory(MachineType::I32, 0U)};
  VariableStorage const secondCycleHighSlot{VariableStorage::stackMemory(MachineType::I32, 8U)};
  VariableStorage const tempReg{VariableStorage::reg(MachineType::I32, parallelMoveRegs[5])};

  resolver.push(firstCycleFirstReg, firstCycleSecondReg);
  resolver.push(firstCycleSecondReg, firstCycleFirstReg);
  resolver.push(secondCycleLowSlot, secondCycleHighSlot);
  resolver.push(secondCycleHighSlot, secondCycleLowSlot);

  std::vector<MoveOperation> result;
  resolver.resolveLinear(ParallelMoveEmitter([&result](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
    result.emplace_back(targetStorage, sourceStorage);
  }));
  resolver.resolveCycle(ParallelMoveEmitter([&result](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
                          result.emplace_back(targetStorage, sourceStorage);
                        }),
                        ParallelMoveTempProvider([tempReg](VariableStorage const &sourceStorage) {
                          return VariableStorage::reg(sourceStorage.machineType, tempReg.location.reg);
                        }));

  std::vector<MoveOperation> const expected{
      MoveOperation(tempReg, firstCycleSecondReg), MoveOperation(firstCycleSecondReg, firstCycleFirstReg), MoveOperation(firstCycleFirstReg, tempReg),
      MoveOperation(tempReg, secondCycleHighSlot), MoveOperation(secondCycleHighSlot, secondCycleLowSlot), MoveOperation(secondCycleLowSlot, tempReg),
  };
  ASSERT_EQ(result, expected);
}

TEST(ParallelMoveResolverTest, ExtendPlaceholderSourceIsReleasedWithExtendRecord) {
  // Extend placeholders are tracking-only records and must stop blocking later moves once the paired Extend move is emitted.
  ParallelMoveResolver resolver{&testCompilerAlloc, &testCompilerFree, nullptr, 3U};
  VariableStorage const extendTargetLow{VariableStorage::reg(MachineType::I64, parallelMoveRegs[0])};
  VariableStorage const extendTargetHigh{VariableStorage::reg(MachineType::I64, parallelMoveRegs[1])};
  VariableStorage const extendSourceLow{VariableStorage::reg(MachineType::I64, parallelMoveRegs[2])};
  VariableStorage const extendSourceHigh{VariableStorage::reg(MachineType::I64, parallelMoveRegs[3])};
  VariableStorage const blockedTarget{VariableStorage::reg(MachineType::I32, parallelMoveRegs[3])};
  VariableStorage const constantSource{VariableStorage::i32Const(11U)};
  VariableStorage const tempReg{VariableStorage::reg(MachineType::I64, parallelMoveRegs[5])};

  resolver.push(extendTargetLow, ParallelMoveTargetType::Extend, extendSourceLow);
  resolver.push(extendTargetHigh, ParallelMoveTargetType::Extend_Placeholder, extendSourceHigh);
  resolver.push(blockedTarget, constantSource);

  std::vector<MoveOperation> result;
  resolver.resolveLinear(ParallelMoveEmitter([&result](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
    result.emplace_back(targetStorage, sourceStorage);
  }));
  resolver.resolveCycle(ParallelMoveEmitter([&result](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
                          result.emplace_back(targetStorage, sourceStorage);
                        }),
                        ParallelMoveTempProvider([tempReg](VariableStorage const &sourceStorage) {
                          return VariableStorage::reg(sourceStorage.machineType, tempReg.location.reg);
                        }));

  std::vector<MoveOperation> const expected{MoveOperation(extendTargetLow, extendSourceLow), MoveOperation(blockedTarget, constantSource)};
  ASSERT_EQ(result, expected);
}

TEST(ParallelMoveResolverTest, HandlesNonParallelSourceLeaf) {
  // Non-location leaves, such as constants, bypass dependency tracking and emit directly.
  ParallelMoveResolver resolver{&testCompilerAlloc, &testCompilerFree, nullptr, 1U};
  VariableStorage const targetReg{VariableStorage::reg(MachineType::I32, parallelMoveRegs[0])};
  VariableStorage const constantSource{VariableStorage::i32Const(7U)};
  VariableStorage const tempReg{VariableStorage::reg(MachineType::I32, parallelMoveRegs[5])};

  resolver.push(targetReg, constantSource);

  std::vector<MoveOperation> result;
  resolver.resolveLinear(ParallelMoveEmitter([&result](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
    result.emplace_back(targetStorage, sourceStorage);
  }));
  resolver.resolveCycle(ParallelMoveEmitter([&result](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
                          result.emplace_back(targetStorage, sourceStorage);
                        }),
                        ParallelMoveTempProvider([tempReg](VariableStorage const &sourceStorage) {
                          return VariableStorage::reg(sourceStorage.machineType, tempReg.location.reg);
                        }));

  std::vector<MoveOperation> const expected{MoveOperation(targetReg, constantSource)};
  ASSERT_EQ(result, expected);
}

TEST(ParallelMoveResolverTest, HandlesNoOpMovesAndSimpleLeafWithSwap) {
  // The swap resolver shares the same non-cycle path: no-op moves are ignored and leaves are emitted as moves.
  ParallelMoveResolver resolver{&testCompilerAlloc, &testCompilerFree, nullptr, 2U};
  VariableStorage const targetReg{VariableStorage::reg(MachineType::I64, parallelMoveRegs[0])};
  VariableStorage const sourceReg{VariableStorage::reg(MachineType::I64, parallelMoveRegs[2])};
  VariableStorage const noopStack{VariableStorage::stackMemory(MachineType::I32, 32U)};

  resolver.push(noopStack, noopStack);
  resolver.push(targetReg, sourceReg);

  std::vector<MoveOperation> moves;
  std::vector<SwapOperation> swaps;
  resolver.resolveLinear(ParallelMoveEmitter([&moves](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
    moves.emplace_back(targetStorage, sourceStorage);
  }));
  resolver.resolveCycle(ParallelMoveEmitter([&moves](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
                          moves.emplace_back(targetStorage, sourceStorage);
                        }),
                        SwapEmitter([&swaps](VariableStorage const &targetStorage, VariableStorage const &sourceStorage, bool const contains64) {
                          swaps.emplace_back(targetStorage, sourceStorage, contains64);
                        }));

  std::vector<MoveOperation> const expectedMoves{MoveOperation(targetReg, sourceReg)};
  ASSERT_EQ(moves, expectedMoves);
  ASSERT_TRUE(swaps.empty());
}

TEST(ParallelMoveResolverTest, ResolvesRegisterCycleWithSwap) {
  // Simple two-register cycle: one swap is enough and no temp move is emitted.
  ParallelMoveResolver resolver{&testCompilerAlloc, &testCompilerFree, nullptr, 2U};
  VariableStorage const firstReg{VariableStorage::reg(MachineType::I32, parallelMoveRegs[0])};
  VariableStorage const secondReg{VariableStorage::reg(MachineType::I32, parallelMoveRegs[1])};

  resolver.push(firstReg, secondReg);
  resolver.push(secondReg, firstReg);

  std::vector<MoveOperation> moves;
  std::vector<SwapOperation> swaps;
  resolver.resolveLinear(ParallelMoveEmitter([&moves](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
    moves.emplace_back(targetStorage, sourceStorage);
  }));
  resolver.resolveCycle(ParallelMoveEmitter([&moves](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
                          moves.emplace_back(targetStorage, sourceStorage);
                        }),
                        SwapEmitter([&swaps](VariableStorage const &targetStorage, VariableStorage const &sourceStorage, bool const contains64) {
                          swaps.emplace_back(targetStorage, sourceStorage, contains64);
                        }));

  std::vector<SwapOperation> const expectedSwaps{SwapOperation(firstReg, secondReg, false)};
  ASSERT_TRUE(moves.empty());
  ASSERT_EQ(swaps, expectedSwaps);
}

TEST(ParallelMoveResolverTest, ResolvesRegisterStackCycleWithSwap) {
  // Mixed register/stack cycle can also be represented by one semantic swap.
  ParallelMoveResolver resolver{&testCompilerAlloc, &testCompilerFree, nullptr, 2U};
  VariableStorage const stackSlot{VariableStorage::stackMemory(MachineType::I64, 0U)};
  VariableStorage const sourceReg{VariableStorage::reg(MachineType::I64, parallelMoveRegs[1])};

  resolver.push(stackSlot, sourceReg);
  resolver.push(sourceReg, stackSlot);

  std::vector<MoveOperation> moves;
  std::vector<SwapOperation> swaps;
  resolver.resolveLinear(ParallelMoveEmitter([&moves](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
    moves.emplace_back(targetStorage, sourceStorage);
  }));
  resolver.resolveCycle(ParallelMoveEmitter([&moves](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
                          moves.emplace_back(targetStorage, sourceStorage);
                        }),
                        SwapEmitter([&swaps](VariableStorage const &targetStorage, VariableStorage const &sourceStorage, bool const contains64) {
                          swaps.emplace_back(targetStorage, sourceStorage, contains64);
                        }));

  std::vector<SwapOperation> const expectedSwaps{SwapOperation(stackSlot, sourceReg, true)};
  ASSERT_TRUE(moves.empty());
  ASSERT_EQ(swaps, expectedSwaps);
}

TEST(ParallelMoveResolverTest, ResolvesThreeRegisterCycleWithSwap) {
  // A three-node cycle is resolved by walking the cycle and emitting adjacent swaps.
  ParallelMoveResolver resolver{&testCompilerAlloc, &testCompilerFree, nullptr, 3U};
  VariableStorage const firstReg{VariableStorage::reg(MachineType::I32, parallelMoveRegs[0])};
  VariableStorage const secondReg{VariableStorage::reg(MachineType::I32, parallelMoveRegs[1])};
  VariableStorage const thirdReg{VariableStorage::reg(MachineType::I32, parallelMoveRegs[2])};

  resolver.push(firstReg, secondReg);
  resolver.push(secondReg, thirdReg);
  resolver.push(thirdReg, firstReg);

  std::vector<MoveOperation> moves;
  std::vector<SwapOperation> swaps;
  resolver.resolveLinear(ParallelMoveEmitter([&moves](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
    moves.emplace_back(targetStorage, sourceStorage);
  }));
  resolver.resolveCycle(ParallelMoveEmitter([&moves](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
                          moves.emplace_back(targetStorage, sourceStorage);
                        }),
                        SwapEmitter([&swaps](VariableStorage const &targetStorage, VariableStorage const &sourceStorage, bool const contains64) {
                          swaps.emplace_back(targetStorage, sourceStorage, contains64);
                        }));

  std::vector<SwapOperation> const expectedSwaps{SwapOperation(firstReg, secondReg, false), SwapOperation(secondReg, thirdReg, false)};
  ASSERT_TRUE(moves.empty());
  ASSERT_EQ(swaps, expectedSwaps);
}

TEST(ParallelMoveResolverTest, ResolvesTwoCyclesSequentiallyWithSwap) {
  // Independent cycles are handled one after another, matching the temp-based resolver behavior.
  ParallelMoveResolver resolver{&testCompilerAlloc, &testCompilerFree, nullptr, 4U};
  VariableStorage const firstCycleFirstReg{VariableStorage::reg(MachineType::I32, parallelMoveRegs[0])};
  VariableStorage const firstCycleSecondReg{VariableStorage::reg(MachineType::I32, parallelMoveRegs[1])};
  VariableStorage const secondCycleLowSlot{VariableStorage::stackMemory(MachineType::I32, 0U)};
  VariableStorage const secondCycleHighSlot{VariableStorage::stackMemory(MachineType::I32, 8U)};

  resolver.push(firstCycleFirstReg, firstCycleSecondReg);
  resolver.push(firstCycleSecondReg, firstCycleFirstReg);
  resolver.push(secondCycleLowSlot, secondCycleHighSlot);
  resolver.push(secondCycleHighSlot, secondCycleLowSlot);

  std::vector<MoveOperation> moves;
  std::vector<SwapOperation> swaps;
  resolver.resolveLinear(ParallelMoveEmitter([&moves](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
    moves.emplace_back(targetStorage, sourceStorage);
  }));
  resolver.resolveCycle(ParallelMoveEmitter([&moves](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
                          moves.emplace_back(targetStorage, sourceStorage);
                        }),
                        SwapEmitter([&swaps](VariableStorage const &targetStorage, VariableStorage const &sourceStorage, bool const contains64) {
                          swaps.emplace_back(targetStorage, sourceStorage, contains64);
                        }));

  std::vector<SwapOperation> const expectedSwaps{SwapOperation(firstCycleFirstReg, firstCycleSecondReg, false),
                                                 SwapOperation(secondCycleLowSlot, secondCycleHighSlot, false)};
  ASSERT_TRUE(moves.empty());
  ASSERT_EQ(swaps, expectedSwaps);
}

TEST(ParallelMoveResolverTest, ResolvesLeafThenCycleWithSwap) {
  // The shared resolver loop should still emit all non-conflicting moves before falling back to swap-based cycle resolution.
  ParallelMoveResolver resolver{&testCompilerAlloc, &testCompilerFree, nullptr, 3U};
  VariableStorage const leafTarget{VariableStorage::reg(MachineType::I32, parallelMoveRegs[2])};
  VariableStorage const constantSource{VariableStorage::i32Const(19U)};
  VariableStorage const firstCycleReg{VariableStorage::reg(MachineType::I32, parallelMoveRegs[0])};
  VariableStorage const secondCycleReg{VariableStorage::reg(MachineType::I32, parallelMoveRegs[1])};

  resolver.push(leafTarget, constantSource);
  resolver.push(firstCycleReg, secondCycleReg);
  resolver.push(secondCycleReg, firstCycleReg);

  std::vector<MoveOperation> moves;
  std::vector<SwapOperation> swaps;
  resolver.resolveLinear(ParallelMoveEmitter([&moves](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
    moves.emplace_back(targetStorage, sourceStorage);
  }));
  resolver.resolveCycle(ParallelMoveEmitter([&moves](VariableStorage const &targetStorage, VariableStorage const &sourceStorage) {
                          moves.emplace_back(targetStorage, sourceStorage);
                        }),
                        SwapEmitter([&swaps](VariableStorage const &targetStorage, VariableStorage const &sourceStorage, bool const contains64) {
                          swaps.emplace_back(targetStorage, sourceStorage, contains64);
                        }));

  std::vector<MoveOperation> const expectedMoves{MoveOperation(leafTarget, constantSource)};
  std::vector<SwapOperation> const expectedSwaps{SwapOperation(firstCycleReg, secondCycleReg, false)};
  ASSERT_EQ(moves, expectedMoves);
  ASSERT_EQ(swaps, expectedSwaps);
}

} // namespace vb
#endif