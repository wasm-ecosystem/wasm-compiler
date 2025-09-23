///
/// @file BinaryModule.hpp
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
#ifndef BINARY_MODULE_HPP
#define BINARY_MODULE_HPP

#include "src/config.hpp"
#include "src/core/common/Span.hpp"
#include "src/core/common/util.hpp"

namespace vb {

///
/// @brief Parse the Binary Module which is emitted by compiler
/// @note this class only stores pointer of BinaryModule sections, it doesn't hold the storage
///
class BinaryModule final {
public:
  /// @brief Type of the pointer to the trap implementation
  using TrapFncPtr = void (*)(uint8_t *linearMemoryBase, uint32_t trapCode);

  static constexpr uint32_t versionNumber{3U}; ///< Current version of the binary module

  /// @brief Default constructor
  BinaryModule() VB_NOEXCEPT = default;

  ///
  /// @brief Parse a Binary Module
  ///
  /// @param module binary module bytes
  /// @throw vb::RuntimeError start address of the binary module in unaligned
  /// @throw vb::RuntimeError binary module version mismatch
  ///
  void init(Span<uint8_t const> const &module);

  ///@see startAddress_
  ///@return Non nullptr
  inline uint8_t const *getStartAddress() const VB_NOEXCEPT {
    return startAddress_;
  }

  ///@see endAddress_
  ///@return Non nullptr
  inline uint8_t const *getEndAddress() const VB_NOEXCEPT {
    return endAddress_;
  }

  ///@brief convert offset to start to offset to end
  ///@param offsetToStart offset to start
  ///@return offset to end
  inline uint32_t offsetToEnd(uint32_t const offsetToStart) const VB_NOEXCEPT {
    return static_cast<uint32_t>(pSubAddr(endAddress_, startAddress_)) - offsetToStart;
  }

  ///@see moduleBinaryLength_
  inline uint32_t getModuleBinaryLength() const VB_NOEXCEPT {
    return moduleBinaryLength_;
  }
  ///@see stacktraceEntryCount_
  inline uint32_t getStacktraceEntryCount() const VB_NOEXCEPT {
    return stacktraceEntryCount_;
  }

  ///@brief get address of landing pad in passive mode or memory extend function in active mode
  ///@return Non nullptr if job has linear memory
  ///@return nullptr if job doesn't have linear memory
  inline uint8_t const *getLandingPadOrMemoryExtendFncAddress() const VB_NOEXCEPT {
    return landingPadAddress_;
  }
  ///@see linkDataLength_
  inline uint32_t getLinkDataLength() const VB_NOEXCEPT {
    return linkDataLength_;
  }

  ///@see tableStart_
  ///@return Non nullptr
  inline uint8_t const *getTableStart() const VB_NOEXCEPT {
    return tableStart_;
  }

  ///@see linkStatusStart_
  ///@return Non nullptr
  inline uint8_t const *getLinkStatusStart() const VB_NOEXCEPT {
    return linkStatusStart_;
  }

  ///@see
  ///@return Non nullptr
  inline uint8_t const *getExportedFunctionsEnd() const VB_NOEXCEPT {
    return exportedFunctionsEnd_;
  }

  ///@see exportedGlobalsEnd_
  ///@return Non nullptr
  inline uint8_t const *getExportedGlobalsSectionEnd() const VB_NOEXCEPT {
    return exportedGlobalsEnd_;
  }
  ///@see initialMemorySize_
  inline uint32_t getInitialMemorySize() const VB_NOEXCEPT {
    return initialMemorySize_;
  }
  ///
  /// @brief If current module has linear memory
  ///
  /// @return true
  /// @return false
  ///
  inline bool hasLinearMemory() const VB_NOEXCEPT {
    return initialMemorySize_ != 0xFF'FF'FF'FFU;
  }

  ///@see dynamicallyImportedFunctionsSectionEnd_
  ///@return Non nullptr
  inline uint8_t const *getDynamicallyImportedFunctionsSectionEnd() const VB_NOEXCEPT {
    return dynamicallyImportedFunctionsSectionEnd_;
  }
  ///@see mutableGlobalsSectionEnd_
  ///@return Non nullptr
  inline uint8_t const *getMutableGlobalsSectionEnd() const VB_NOEXCEPT {
    return mutableGlobalsSectionEnd_;
  }

  ///@see startFunctionBinaryOffset_
  inline uint32_t getStartFunctionBinaryOffset() const VB_NOEXCEPT {
    return startFunctionBinaryOffset_;
  }
  ///@see functionNameSectionEnd_
  ///@return Non nullptr
  inline uint8_t const *getFunctionNameSectionEnd() const VB_NOEXCEPT {
    return functionNameSectionEnd_;
  }
  ///@see numDataSegments_
  inline uint32_t getNumDataSegments() const VB_NOEXCEPT {
    return numDataSegments_;
  }
  ///@see dataSegmentsEnd_
  ///@return Non nullptr
  inline uint8_t const *getDataSegmentsEnd() const VB_NOEXCEPT {
    return dataSegmentsEnd_;
  }

  ///
  /// @brief If the the Binary Module is debug build or release build
  ///
  /// @return true debug build
  /// @return false release build
  ///
  inline bool debugMode() const VB_NOEXCEPT {
    return debugMode_;
  }

  ///
  /// @brief Get the pointer to the trap implementation
  /// First argument passed to this argument must be the current base of the linear memory, second argument should be
  /// the TrapCode
  ///
  /// @return Pointer to the trap implementation (Takes pointer to linear memory base as first parameter and trapCode as
  /// second parameter)
  inline TrapFncPtr getTrapFnc() const VB_NOEXCEPT {
    uint8_t *const trapFncAddress{pRemoveConst(getStartAddress())};
    return pCast<TrapFncPtr>(trapFncAddress);
  }

  /// @brief get the start address of table entry functions(the C++ to Wasm wrapper function) pointer array
  inline uint8_t const *getTableEntryFunctionsStart() const VB_NOEXCEPT {
    return tableEntryFunctionsStart_;
  }

  /// @brief get the size of Wasm table
  inline uint32_t getTableSize() const VB_NOEXCEPT {
    return tableSize_;
  }

private:
  uint8_t const *startAddress_;                           ///< Start address of the Binary Module
  uint8_t const *endAddress_;                             ///< End address of the Binary Module
  uint8_t const *landingPadAddress_;                      ///< Start address of landing pad
  uint8_t const *tableEntryFunctionsStart_;               ///< Start address of table entry functions section exclude the size field
  uint8_t const *tableStart_;                             ///< Start address of Table
  uint8_t const *linkStatusStart_;                        ///< Start address of Link Status of Imported Functions
  uint8_t const *exportedFunctionsEnd_;                   ///< End address of exported functions section exclude the size field
  uint8_t const *exportedGlobalsEnd_;                     ///< End address of exported globals section exclude the size field
  uint8_t const *dynamicallyImportedFunctionsSectionEnd_; ///< End address of dynamic imported functions section exclude
                                                          ///< the size field
  uint8_t const *mutableGlobalsSectionEnd_;               ///< End address of mutable globals section exclude the size field
  uint8_t const *functionNameSectionEnd_;                 ///< End address of function names section exclude the size field
  uint8_t const *dataSegmentsEnd_;                        ///< End address of function names section exclude the Number of data segments field
  uint32_t initialMemorySize_;                            ///< inital memory size
  uint32_t startFunctionBinaryOffset_;                    ///< offset of start function end to binary module end
  uint32_t numDataSegments_;                              ///< Number of data segments
  uint32_t linkDataLength_;                               ///< Sum of byte-widths of variables kept in the link data.
  uint32_t moduleBinaryLength_;                           ///< Length of the binary module exclude the size of length field itself
  uint32_t stacktraceEntryCount_;                         ///< Stacktrace record count
  uint32_t tableSize_;                                    ///< size of Wasm table
  bool debugMode_;                                        ///< If the the Binary Module is debug build or release build
};

} // namespace vb

#endif
