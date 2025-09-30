///
/// @file ModuleInfo.cpp
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
#include <cassert>
#include <cstdint>

#include "ModuleInfo.hpp"

#include "src/config.hpp"
#include "src/core/common/FunctionRef.hpp"
#include "src/core/common/SignatureType.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/backend/RegAdapter.hpp"
#include "src/core/compiler/backend/aarch64/aarch64_encoding.hpp"
#include "src/core/compiler/backend/tricore/tricore_encoding.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_encoding.hpp"
#include "src/core/compiler/common/MachineType.hpp"
#include "src/core/compiler/common/RegMask.hpp"
#include "src/core/compiler/common/StackElement.hpp"
#include "src/core/compiler/common/StackType.hpp"
#include "src/core/compiler/common/VariableStorage.hpp"
#include "src/core/compiler/common/util.hpp"

namespace vb {

StorageType ModuleInfo::LocalDef::getInitializedStorageType(TReg const chosenReg, bool const isParam) VB_NOEXCEPT {
  if (isParam) {
    if (chosenReg != TReg::NONE) {
      return StorageType::REGISTER;
    } else {
      return StorageType::STACKMEMORY;
    }
  } else {
    return StorageType::CONSTANT;
  }
}

bool ModuleInfo::functionIsLinked(uint32_t const fncIdx) const VB_NOEXCEPT {
  if (functionIsImported(fncIdx)) {
    // Imported function
    ModuleInfo::ImpFuncDef const impFuncDef{getImpFuncDef(fncIdx)};
    return impFuncDef.linked;
  } else {
    // Non-imported function
    // Return true if defined within the Wasm module, otherwise function is out of bounds
    return fncIdx < numTotalFunctions;
  }
}

uint32_t ModuleInfo::getNumParamsForSignature(uint32_t const sigIndex) const VB_NOEXCEPT {
  uint32_t const typeOffset{typeOffsets[sigIndex]};
  uint32_t const nextTypeOffset{typeOffsets[sigIndex + 1U]};
  uint32_t paramLength{0U};
  uint8_t const *stepPtr{pAddI(pCast<uint8_t *>(types()), typeOffset)};
  SignatureType const paramStart{readFromPtr<SignatureType>(stepPtr)};
  static_cast<void>(paramStart);
  assert(paramStart == SignatureType::PARAMSTART && "Wrong signature start");
  while (pToNum(stepPtr) < pToNum(pAddI(pCast<uint8_t *>(types()), nextTypeOffset))) {
    stepPtr = pAddI(stepPtr, 1);
    SignatureType const signatureType{readFromPtr<SignatureType>(stepPtr)};
    if (signatureType == SignatureType::PARAMEND) {
      break;
    }
    paramLength++;
  }
  assert(stepPtr < pCast<uint8_t *>(types()) + nextTypeOffset && "No closing brace at end of params encountered");
  return paramLength;
}

uint32_t ModuleInfo::getNumReturnValuesForSignature(uint32_t const sigIndex) const VB_NOEXCEPT {
  uint32_t const typeOffset{typeOffsets[sigIndex]};
  uint32_t const nextTypeOffset{typeOffsets[sigIndex + 1U]};
  uint32_t returnValueLength{0U};
  uint8_t *stepPtr{pAddI(pCast<uint8_t *>(types()), nextTypeOffset)};
  while (pToNum(stepPtr) >= pToNum(pAddI(pCast<uint8_t *>(types()), typeOffset))) {
    stepPtr = pSubI(stepPtr, 1);
    SignatureType const signatureType{readFromPtr<SignatureType>(stepPtr)};
    if (signatureType == SignatureType::PARAMEND) {
      break;
    }
    returnValueLength++;
  }
  assert(stepPtr >= pCast<uint8_t *>(types()) + typeOffset && "Wrong signature start");
  return returnValueLength;
}

void ModuleInfo::iterateParamsForSignature(uint32_t const sigIndex, FunctionRef<void(MachineType)> const &lambda, bool const reverse) const {
  if (lambda.notNull()) {
    uint32_t const typeOffset{typeOffsets[sigIndex]};
    uint32_t const nextTypeOffset{typeOffsets[sigIndex + 1U]};
    uint32_t stepOffset{(reverse ? nextTypeOffset : typeOffset)};
    if (reverse) {
      while (stepOffset >= typeOffset) {
        stepOffset--;
        // must use offset, then calculate ptr, because lambda may trigger memoryExtension
        uint8_t const *const stepPtr{pAddI(pCast<uint8_t *>(types()), stepOffset)};
        SignatureType const signatureType{readFromPtr<SignatureType>(stepPtr)};
        if (signatureType == SignatureType::PARAMEND) {
          break;
        }
      }
      assert(readFromPtr<SignatureType>(pAddI(pCast<uint8_t *>(types()), stepOffset)) == SignatureType::PARAMEND && "Param end not found");

      while (stepOffset >= typeOffset) {
        stepOffset--;
        // must use offset, then calculate ptr, because lambda may trigger memoryExtension
        uint8_t const *const stepPtr{pAddI(pCast<uint8_t *>(types()), stepOffset)};
        SignatureType const signatureType{readFromPtr<SignatureType>(stepPtr)};
        if (signatureType == SignatureType::PARAMSTART) {
          break;
        }
        // coverity[autosar_cpp14_a4_5_1_violation]
        lambda(MachineTypeUtil::fromSignatureType(signatureType));
      }
      assert(stepOffset >= typeOffset && "No starting brace for params encountered");
    } else {
      uint8_t const *const initialSignaturePtr{pAddI(pCast<uint8_t *>(types()), stepOffset)};
      SignatureType const initialSignatureType{readFromPtr<SignatureType>(initialSignaturePtr)};
      static_cast<void>(initialSignatureType);
      assert(initialSignatureType == SignatureType::PARAMSTART && "Wrong signature start");

      while (stepOffset < nextTypeOffset) {
        stepOffset++;
        // must use offset, then calculate ptr, because lambda may trigger memoryExtension
        uint8_t const *const stepPtr{pAddI(pCast<uint8_t *>(types()), stepOffset)};
        SignatureType const signatureType{readFromPtr<SignatureType>(stepPtr)};
        if (signatureType == SignatureType::PARAMEND) {
          break;
        }
        // coverity[autosar_cpp14_a4_5_1_violation]
        lambda(MachineTypeUtil::fromSignatureType(signatureType));
      }
      assert(stepOffset < nextTypeOffset && "No closing brace at end of params encountered");
    }
  }
}

void ModuleInfo::iterateResultsForSignature(uint32_t const sigIndex, FunctionRef<void(MachineType)> const &lambda, bool const reverse) const {
  if (lambda.notNull()) {
    uint32_t const typeOffset{typeOffsets[sigIndex]};
    uint32_t const nextTypeOffset{typeOffsets[sigIndex + 1U]};
    uint32_t stepOffset{(reverse ? nextTypeOffset : typeOffset)};
    if (reverse) {
      while (stepOffset >= typeOffset) {
        stepOffset--;
        // must use offset, then calculate ptr, because lambda may trigger memoryExtension
        uint8_t const *const stepPtr{pAddI(pCast<uint8_t *>(types()), stepOffset)};
        SignatureType const signatureType{readFromPtr<SignatureType>(stepPtr)};
        if (signatureType == SignatureType::PARAMEND) {
          break;
        }
        // coverity[autosar_cpp14_a4_5_1_violation]
        lambda(MachineTypeUtil::fromSignatureType(signatureType));
      }
      assert(readFromPtr<SignatureType>(pAddI(pCast<uint8_t *>(types()), stepOffset)) == SignatureType::PARAMEND && "Param end not found");
    } else {
      uint8_t const *const paramStartPtr{pAddI(pCast<uint8_t *>(types()), stepOffset)};
      SignatureType const paramStart{readFromPtr<SignatureType>(paramStartPtr)};
      static_cast<void>(paramStart);
      assert(paramStart == SignatureType::PARAMSTART && "Wrong signature start");

      while (stepOffset < nextTypeOffset) {
        stepOffset++;
        // must use offset, then calculate ptr, because lambda may trigger memoryExtension
        uint8_t const *const stepPtr{pAddI(pCast<uint8_t *>(types()), stepOffset)};
        SignatureType const signatureType{readFromPtr<SignatureType>(stepPtr)};
        if (signatureType == SignatureType::PARAMEND) {
          break;
        }
      }
      assert(readFromPtr<SignatureType>(pAddI(pCast<uint8_t *>(types()), stepOffset)) == SignatureType::PARAMEND && "Param end not found");

      stepOffset++;
      while (stepOffset < nextTypeOffset) {
        // must use offset, then calculate ptr, because lambda may trigger memoryExtension
        uint8_t const *const stepPtr{pAddI(pCast<uint8_t *>(types()), stepOffset)};
        SignatureType const signatureType{readFromPtr<SignatureType>(stepPtr)};
        stepOffset++;
        // coverity[autosar_cpp14_a4_5_1_violation]
        lambda(MachineTypeUtil::fromSignatureType(signatureType));
      }
    }
  }
}

RegMask ModuleInfo::maskForElement(StackElement const *const element) const VB_NOEXCEPT {
  if ((element != nullptr) && (element->type != StackType::INVALID)) {
    VariableStorage const storage{getStorage(*element)};
    if (storage.type == StorageType::REGISTER) {
      return RegMask(storage.location.reg);
    }
  }
  return RegMask::none();
}

VariableStorage ModuleInfo::getStorage(StackElement const &element) const VB_NOEXCEPT {
  StackType const baseType{element.getBaseType()};
  if (static_cast<uint32_t>(baseType) < static_cast<uint32_t>(StackType::DEFERREDACTION)) {
    MachineType const machineType{getMachineType(&element)};
    if ((element.type == StackType::LOCAL)) {
      ModuleInfo::LocalDef const &localDef{localDefs[element.data.variableData.location.localIdx]};
      if (localDef.currentStorageType == StorageType::STACKMEMORY) {
        return VariableStorage::stackMemory(localDef.type, localDef.stackFramePosition);
      } else {
        return VariableStorage::reg(localDef.type, localDef.reg);
      }
    } else if ((element.type == StackType::GLOBAL)) {
      ModuleInfo::GlobalDef const &globalDef{globals[element.data.variableData.location.globalIdx]};
      assert(globalDef.isMutable && "Immutable globals are not allowed on the stack as global reference, reduce them to constants");
      if (globalDef.reg == TReg::NONE) {
        return VariableStorage::linkData(globalDef.type, globalDef.linkDataOffset);
      } else {
        return VariableStorage::reg(globalDef.type, globalDef.reg);
      }
    } else if (baseType == StackType::TEMP_RESULT) {
      StackElement::Data::VariableData::CalculationResult const &calculationResult{element.data.variableData.location.calculationResult};
      if (calculationResult.storageType == StorageType::REGISTER) {
        return VariableStorage::reg(calculationResult.resultLocation.reg, calculationResult.machineType);
      } else if (calculationResult.storageType == StorageType::LINKDATA) {
        return VariableStorage::linkData(calculationResult.resultLocation.linkDataOffset, calculationResult.machineType);
      } else {
        return VariableStorage::stackMemory(calculationResult.resultLocation.stackFramePosition, calculationResult.machineType);
      }
    } else if (baseType == StackType::SCRATCHREGISTER) {
      return VariableStorage::reg(machineType, element.data.variableData.location.reg);
    } else if (baseType == StackType::CONSTANT) {
      VariableStorage res{};
      res.type = StorageType::CONSTANT;
      res.machineType = machineType;
      res.location.constUnion = element.data.constUnion;
      return res;
    } else {
      // pass
    }
  }
  return VariableStorage{StorageType::INVALID};
}

MachineType ModuleInfo::getMachineType(StackElement const *const element) const VB_NOEXCEPT {
  if ((element == nullptr) || (element->getBaseType() == StackType::INVALID)) {
    return MachineType::INVALID;
  }
  StackType const baseType{element->getBaseType()};
  if ((baseType <= StackType::CONSTANT)) {
    // SCRATCHREGISTER, TEMPSTACK or CONSTANT (All those have a flag indicating the Wasm type)
    return MachineTypeUtil::fromStackTypeFlag(element->type);
  } else if (element->type == StackType::LOCAL) {
    assert(element->data.variableData.location.localIdx < fnc.numLocals && "Local out of range");
    return localDefs[element->data.variableData.location.localIdx].type;
  } else if (element->type == StackType::GLOBAL) {
    assert(element->data.variableData.location.globalIdx < numNonImportedGlobals && "Global out of range");
    return globals[element->data.variableData.location.globalIdx].type;
  } else {
    static_cast<void>(0);
  }
  return MachineType::INVALID;
}

} // namespace vb
