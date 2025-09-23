///
/// @file RegMask.hpp
/// @copyright Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
/// SPDX-License-Identifier: Apache-2.0
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
#ifndef REGMASK_HPP
#define REGMASK_HPP

#include <cstdint>
#include <limits>

#include "src/core/common/util.hpp"
#include "src/core/compiler/backend/RegAdapter.hpp"

namespace vb {

///
/// @brief Class that represents a mask of registers, where each bit corresponds to a register
///
class RegMask final {
public:
#ifdef JIT_TARGET_TRICORE
  using Type = uint32_t; ///< Underlying type
#else
  using Type = uint64_t; ///< Underlying type
#endif

  /// @brief Construct a RegMask based on a given register
  ///
  /// @param reg Register
  constexpr explicit RegMask(TReg const reg) VB_NOEXCEPT {
    if (reg == TReg::NONE) {
      mask_ = 0U;
    } else {
      mask_ = static_cast<Type>(1U) << static_cast<uint32_t>(reg);
    }
  }

  /// @brief Construct a default RegMask where no register are marked (equivalent to RegMask::none())
  constexpr explicit RegMask() VB_NOEXCEPT : RegMask(TReg::NONE) {
  }

  ///
  /// @brief Construct a RegMask from a given raw input mask
  ///
  /// @param rawMask Raw input mask
  /// @return Constructed RegMask
  constexpr static RegMask fromRaw(Type const rawMask) VB_NOEXCEPT {
    RegMask mask{};
    mask.mask_ = rawMask;
    return mask;
  }

  ///
  /// @brief Construct a RegMask where all registers are marked
  ///
  /// @return Constructed RegMask
  constexpr static RegMask all() VB_NOEXCEPT {
    return RegMask::fromRaw(allMask_);
  }

  ///
  /// @brief Construct a RegMask where no registers are marked
  ///
  /// @return Constructed RegMask
  constexpr static RegMask none() VB_NOEXCEPT {
    return RegMask::fromRaw(static_cast<Type>(0U));
  }

  ///
  /// @brief Get the raw value of the RegMask
  ///
  /// @return Raw Value
  constexpr Type raw() const VB_NOEXCEPT {
    return mask_;
  }

  ///
  /// @brief Determine whether all registers/bits are marked in this RegMask
  ///
  /// @return Whether all registers are marked
  constexpr bool allMarked() const VB_NOEXCEPT {
    return mask_ == allMask_;
  }

  ///
  /// @brief Check if a given register is marked as protected by the given RegMask
  ///
  /// @param reg Input register
  /// @return Whether the given register is protected
  constexpr bool contains(TReg const reg) const VB_NOEXCEPT {
    if (reg == TReg::NONE) {
      return false;
    }
    uint32_t const rawReg{static_cast<uint32_t>(reg)};
    assert((rawReg < (static_cast<uint32_t>(sizeof(Type)) * 8U)) && "Register value out of range for mask");

    Type const testBit{static_cast<Type>(1U) << static_cast<Type>(rawReg)};
    return (mask_ & testBit) != 0U;
  }

  /// @brief add reg into mask
  /// @param maskToAdd target reg mask
  constexpr void mask(RegMask const maskToAdd) VB_NOEXCEPT {
    mask_ = mask_ | maskToAdd.mask_;
  }

  /// @brief remove reg into mask
  /// @param maskToRemove target reg mask
  constexpr void unmask(RegMask const maskToRemove) VB_NOEXCEPT {
    mask_ = mask_ & ~maskToRemove.mask_;
  }

  ///
  /// @brief Count number of masked registers
  ///
  /// @param filterMask Filter mask
  /// @return uint32_t Number of masked registers
  uint32_t maskedRegsCount(Type const filterMask) const VB_NOEXCEPT {
    Type const tmp{mask_ & filterMask};
    return (sizeof(Type) == 4U) ? static_cast<uint32_t>(popcnt(static_cast<uint32_t>(tmp)))
                                : static_cast<uint32_t>(popcntll(static_cast<uint64_t>(tmp)));
  }

private:
  static constexpr Type allMask_{std::numeric_limits<Type>::max()}; ///< All ones
  Type mask_{0U};                                                   ///< Underlying bitmask, each bit corresponds to a register
};

///
/// @brief OR operator for RegMasks
///
/// @param lhs RegMask 1
/// @param rhs RegMask 2
/// @return constexpr RegMask Resulting union of RegMask
inline constexpr RegMask operator|(RegMask const lhs, RegMask const rhs) VB_NOEXCEPT {
  return RegMask::fromRaw(lhs.raw() | rhs.raw());
}

///
/// @brief Tracker object to keep track of protected registers mask
///
class RegAllocTracker final {
public:
  // coverity[autosar_cpp14_a3_1_1_violation]
  RegMask writeProtRegs{vb::RegMask::RegMask::none()}; ///< mask of write protected registers, if liftToReg with targetNeedsToBeWritable = true, the
                                                       ///< register will be added in writeProtRegs
  RegMask readProtRegs{vb::RegMask::RegMask::none()};  ///< mask of read protected registers, if liftToReg with targetNeedsToBeWritable = false, the
                                                       ///< register will be added in readProtRegs
  RegMask futureLifts{vb::RegMask::RegMask::none()};   ///< mask of registers which will be lifted future

  ///
  /// @brief get read and write protected registers mask
  /// @return constexpr RegMask mask for write protected registers and read protected registers
  inline constexpr RegMask readWriteMask() const VB_NOEXCEPT {
    return writeProtRegs | readProtRegs;
  }

  ///
  /// @brief get all protected registers mask
  /// @return constexpr RegMask mask for all protected registers
  inline constexpr RegMask readWriteFutureLiftMask() const VB_NOEXCEPT {
    return writeProtRegs | readProtRegs | futureLifts;
  }
};

} // namespace vb

#endif
