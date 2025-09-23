///
/// @file ValidateElement.hpp
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
#ifndef VALIDATE_ELEMENT_HPP
#define VALIDATE_ELEMENT_HPP
#include <cassert>
#include <cstdint>

#include "src/core/compiler/common/ListIterator.hpp"
#include "src/core/compiler/common/MachineType.hpp"

namespace vb {
///
/// @brief Type of a validate node
enum class ValidateType : uint8_t { F64, F32, I64, I32, ANY, FUNC, BLOCK, LOOP, IF, ELSE_FENCE, INVALID };
///
/// @brief Type of a ValidateElement
class ValidateElement final {
public:
  ///
  /// @brief Checks whether the @a machineType match validateElement type. Replace Any
  /// @param machineType Expected machineType. Must be number type(F64, F32, I64, I32)
  inline bool numberMatch(MachineType const machineType) VB_NOEXCEPT {
    if (validateType_ == ValidateType::ANY) {
      validateType_ = toValidateType(machineType);
      return true;
    }
    return validateType_ == toValidateType(machineType);
  }
  /// @brief Checks whether the @a validateType match element inner type
  /// @param validateType Expected validateType
  inline bool equals(ValidateType const validateType) const VB_NOEXCEPT {
    return validateType_ == validateType;
  }
  /// @brief Get validate type from number variable type of @a machineType
  /// @param machineType Input machineType.
  static constexpr ValidateType toValidateType(MachineType const machineType) VB_NOEXCEPT {
    switch (machineType) {
    case MachineType::F64:
      return ValidateType::F64;
    case MachineType::F32:
      return ValidateType::F32;
    case MachineType::I64:
      return ValidateType::I64;
    case MachineType::I32:
      return ValidateType::I32;
    default:
      return ValidateType::INVALID; // dummy return
    }
  }
  ///
  /// @brief Whether the validation type is number type
  ///
  /// @return bool Whether type is a number type
  inline static bool isNumber(ValidateType const type) VB_NOEXCEPT {
    return type <= ValidateType::ANY;
  }
  ///
  /// @brief Whether the validation type is number type
  ///
  /// @return bool Whether type is a number type
  inline bool isNumber() const VB_NOEXCEPT {
    return ValidateElement::isNumber(validateType_);
  }
  ///
  /// @brief Generator function for a BLOCK ValidateElement
  ///
  /// @param prevBlock The reference of last validation block-based element(FUNC, BLOCK, LOOP, IF)
  /// @param sigIndex Index of the function type this BLOCK is conforming to
  /// @return ValidateElement Generated BLOCK ValidateElement
  static inline constexpr ValidateElement block(List_iterator<ValidateElement> const prevBlock, uint32_t const sigIndex) VB_NOEXCEPT {
    ValidateElement block{};
    block.validateType_ = ValidateType::BLOCK;
    block.blockInfo.prevBlock = prevBlock;
    block.blockInfo.sigIndex = sigIndex;
    block.blockInfo.formallyUnreachable = false;
    return block;
  }
  ///
  /// @brief Generator function for a LOOP ValidateElement
  ///
  /// @param prevBlock The reference of last validation block-based element(FUNC, BLOCK, LOOP, IF)
  /// @param sigIndex Index of the function type this LOOP is conforming to
  /// @return ValidateElement Generated LOOP ValidateElement
  static inline constexpr ValidateElement loop(List_iterator<ValidateElement> const prevBlock, uint32_t const sigIndex) VB_NOEXCEPT {
    ValidateElement block{};
    block.validateType_ = ValidateType::LOOP;
    block.blockInfo.prevBlock = prevBlock;
    block.blockInfo.sigIndex = sigIndex;
    block.blockInfo.formallyUnreachable = false;
    return block;
  }
  ///
  /// @brief Generator function for a IF ValidateElement
  ///
  /// @param prevBlock The reference of last validation block-based element(FUNC, BLOCK, LOOP, IF)
  /// @param sigIndex Index of the function type this IF is conforming to
  /// @return ValidateElement Generated IF ValidateElement
  static inline constexpr ValidateElement ifblock(List_iterator<ValidateElement> const prevBlock, uint32_t const sigIndex) VB_NOEXCEPT {
    ValidateElement block{};
    block.validateType_ = ValidateType::IF;
    block.blockInfo.prevBlock = prevBlock;
    block.blockInfo.sigIndex = sigIndex;
    block.blockInfo.formallyUnreachable = false;
    return block;
  }
  ///
  /// @brief Generator function for a ELSE_FENCE ValidateElement
  /// @return ValidateElement Generated BLOCK ValidateElement
  static inline constexpr ValidateElement elseFence() VB_NOEXCEPT {
    ValidateElement fence{};
    fence.validateType_ = ValidateType::ELSE_FENCE;
    return fence;
  }
  ///
  /// @brief Generator function for a number ValidateElement
  ///
  /// @param machineType wasm type of the number variable
  /// @return ValidateElement Generated variable ValidateElement(F64, F32, I64, I32)
  static inline constexpr ValidateElement variable(MachineType const machineType) VB_NOEXCEPT {
    ValidateElement variable{};
    variable.validateType_ = ValidateElement::toValidateType(machineType);
    return variable;
  }
  ///
  /// @brief Generator function for a number ValidateElement
  ///
  /// @param validateType validate type of the number variable
  /// @return ValidateElement Generated variable ValidateElement(F64, F32, I64, I32)
  static inline constexpr ValidateElement variable(ValidateType const validateType) VB_NOEXCEPT {
    ValidateElement variable{};
    variable.validateType_ = validateType;
    return variable;
  }

  ValidateType validateType_{ValidateType::INVALID}; ///< Validate type

  /// @brief Data for structural validation stack element (if type is BLOCK, LOOP or IF)
  class BlockInfo final {
  public:
    List_iterator<ValidateElement> prevBlock{nullptr}; ///< prev block iterator(FUNC, BLOCK, LOOP, IF)
    uint32_t sigIndex{UINT32_MAX};                     ///< Index of the function type this structural element is conforming to
    bool formallyUnreachable{false};                   ///< Whether this frame defined by this structural element is marked as unreachable
  };
  BlockInfo blockInfo{}; ///< General information about this structural element (if type is BLOCK, LOOP or IF)
};

} // namespace vb

#endif
