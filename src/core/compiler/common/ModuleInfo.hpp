///
/// @file ModuleInfo.hpp
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
#ifndef MODULEINFO_HPP
#define MODULEINFO_HPP

#include <array>
#include <cassert>
#include <cstdint>

#include "BuiltinFunction.hpp"
#include "MemWriter.hpp"
#include "VariableStorage.hpp"
#include "util.hpp"

#include "src/core/common/FunctionRef.hpp"
#include "src/core/common/NativeSymbol.hpp"
#include "src/core/common/basedataoffsets.hpp"
#include "src/core/common/implementationlimits.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/backend/aarch64/aarch64_encoding.hpp"
#include "src/core/compiler/common/BranchCondition.hpp"
#include "src/core/compiler/common/RegMask.hpp"
#include "src/core/compiler/common/Stack.hpp"
#include "src/core/compiler/common/StackElement.hpp"
#include "src/core/compiler/common/StackType.hpp"

namespace vb {

///
/// @brief Class to reference an array of given type at a specific offset in a MemWriter
///
/// @tparam Type Type (or type of array) to reference
// coverity[autosar_cpp14_a14_1_1_violation]
template <class Type> class OffsetHandler final {
public:
  ///
  /// @brief Set the offset where the type or the array of the given type is stored
  ///
  /// @param offset Offset at which the data is stored
  /// @param memory Referenced MemWriter
  void setOffset(uint32_t const offset, MemWriter const &memory) VB_NOEXCEPT {
    offset_ = offset;
    memory_ = &memory;
  }

  ///
  /// @brief Get the stored offset
  ///
  /// @return uint32_t Stored offset
  inline uint32_t getOffset() const VB_NOEXCEPT {
    return offset_;
  }

  ///
  /// @brief Get the pointer to the data
  ///
  /// @return Type* Pointer to the data
  Type *getPtr() const VB_NOEXCEPT {
    if (memory_ == nullptr) {
      return nullptr;
    }
    uint8_t *const rawPtr{pAddI(memory_->base(), offset_)};
    return pCast<Type *>(rawPtr);
  }

  ///
  /// @brief Call operator to quickly retrieve the pointer to the data
  ///
  /// @return Type* Pointer to the data
  Type *operator()() const VB_NOEXCEPT {
    Type *const res{getPtr()};
    return res;
  }

  ///
  /// @brief Array index operator for convenient access
  ///
  /// @return Type& Reference to the stored data at this offset
  Type &operator[](uint32_t const index) const VB_NOEXCEPT {
    assert(memory_ != nullptr);
    return *(getPtr() + index);
  }

  ///
  /// @brief Check if this OffsetHandler has been initialized
  ///
  /// @return bool Whether this OffsetHandler has been initialized
  bool initialized() const VB_NOEXCEPT {
    return memory_ != nullptr;
  };

private:
  uint32_t offset_ = 0U;              ///< Stored offset where the data is stored in the MemWriter
  const MemWriter *memory_ = nullptr; ///< Pointer to the MemWriter representing the memory where the data is stored
};

///
/// @brief Struct that stores data about the module
///
/// This is used by the compiler to keep track of previously parsed infos without needing to re-parse sections of the
/// WebAssembly module
///
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
class ModuleInfo final {
public:
  BranchCondition lastBC = BranchCondition::UNCONDITIONAL; ///< Last branch condition

  NativeSymbol const *importSymbols = nullptr; ///< Data to the array of NativeSymbols that are provided to the compiler for linkage

  uint32_t numTypes = 0U;              ///< Number of function types defined in the Wasm module
  OffsetHandler<uint32_t> typeOffsets; ///< Offsets in the "types" OffsetHandler where the specific function type signatures start
  OffsetHandler<uint8_t> types;        ///< Reference to the stored types in the memory, sequence like (i)I(FFFf)f(iIfF)i for three function types

  ///
  /// @brief Definition of a function that is imported into the WebAssembly module
  ///
  class ImpFuncDef final {
  public:
    ///
    /// @brief Index of the imported NativeSymbol that is representing this imported function
    ///
    uint32_t symbolIndex = 0U;

    ///
    /// @brief At which offset in the link data (inside of the job memory) the pointer to the dynamically linked
    /// imported function is stored
    ///
    /// Variable is ignored if statically linked
    ///
    uint32_t linkDataOffset = 0U;

    ///
    /// @brief Index of the function type this function is conforming to
    ///
    uint32_t sigIndex = 0U;

    ///
    /// @brief Which builtin function this imported function is representing
    ///
    /// If this function is not representing a builtin function, this is set to BuiltinFunction::UNKNOWN which in turn
    /// means that this is a regular imported C++ function
    ///
    BuiltinFunction builtinFunction = {};

    ///
    /// @brief Whether this function is linked
    ///
    bool linked = false;
    ///
    /// @brief Whether this import function is v2 or v1
    ///
    NativeSymbol::ImportFnVersion importFnVersion = NativeSymbol::ImportFnVersion::V1;
  };

  ///
  /// @brief Definition of a function that is not imported, but defined within the WebAssembly module
  ///
  struct FuncDef final {
    uint32_t sigIndex; ///< Index of the function type this function is conforming to
  };
  uint32_t numTotalFunctions = 0U;         ///< Total number of functions (imported and non-imported) within this WebAssembly module
  uint32_t numImportedFunctions = 0U;      ///< Number of imported functions within this WebAssembly module
  uint32_t numFunctionBodiesProduced = 0U; ///< Counter/number of functions the compiler has already emitted the function body (as machine code) for

  ///
  /// @brief Array of ImpFuncDefs and FuncDefs
  ///
  /// The first numImportedFunctions are ImpFuncDefs, after that (numTotalFunctions - numImportedFunctions) FuncDefs are
  /// stored in here. Not necessarily aligned (defined as uint8_t). Use memcpy/readFromPtr to access without violating
  /// the strict aliasing and pointer alignment rule.
  ///
  OffsetHandler<uint8_t> fncDefs;

  ///
  /// @brief Get the ImpFncDef for a given function index within the WebAssembly module
  /// CAUTION: Undefined behavior if the function with this index is not an imported one
  ///
  /// @param fncIndex Index of an imported function within the WebAssembly module
  /// @return ImpFuncDef Definition of the imported function
  inline ImpFuncDef getImpFuncDef(uint32_t const fncIndex) const VB_NOEXCEPT {
    assert(fncIndex < numImportedFunctions && "Function index not referencing an imported function");
    return readFromPtr<ImpFuncDef>(pAddI(fncDefs(), static_cast<uint32_t>(sizeof(ImpFuncDef)) * fncIndex));
  }

  ///
  /// @brief Get the FncDef for a given function index within the WebAssembly module
  /// CAUTION: Undefined behavior if the function with this index is an imported one
  ///
  /// @param fncIndex Index of a non-imported function within the WebAssembly module
  /// @return FuncDef Definition of the non-imported function
  inline FuncDef getFuncDef(uint32_t const fncIndex) const VB_NOEXCEPT {
    assert(fncIndex >= numImportedFunctions && fncIndex < numTotalFunctions && "Function index out of range");
    return readFromPtr<FuncDef>(pAddI(fncDefs(), (static_cast<uint32_t>(sizeof(ImpFuncDef)) * numImportedFunctions) +
                                                     (static_cast<uint32_t>(sizeof(FuncDef)) * (fncIndex - numImportedFunctions))));
  }

  ///
  /// @brief Check whether a function is an imported function
  ///
  /// @param fncIndex Function index
  /// @return true if the function is an imported function
  inline bool functionIsImported(uint32_t const fncIndex) const VB_NOEXCEPT {
    return fncIndex < numImportedFunctions;
  }

  ///
  /// @brief Get the signature index for a function
  ///
  /// @param fncIndex Function index
  /// @return Signature index of the function
  inline uint32_t getFncSigIndex(uint32_t const fncIndex) const VB_NOEXCEPT {
    if (functionIsImported(fncIndex)) {
      return getImpFuncDef(fncIndex).sigIndex;
    } else {
      return getFuncDef(fncIndex).sigIndex;
    }
  }

  ///
  /// @brief Check whether a function is an builtin function
  ///
  /// @param fncIndex Function index
  /// @return true if the function is an builtin function
  inline bool functionIsBuiltin(uint32_t const fncIndex) const VB_NOEXCEPT {
    if (functionIsImported(fncIndex)) {
      ModuleInfo::ImpFuncDef const impFuncDef{getImpFuncDef(fncIndex)};
      return impFuncDef.builtinFunction != BuiltinFunction::UNDEFINED;
    }
    return false;
  }

  /// @brief Check if a function is a V2 import
  /// @param fncIndex The function index to check
  /// @return True if the function is an imported function using V2 API
  inline bool functionIsV2Import(uint32_t const fncIndex) const VB_NOEXCEPT {
    if (functionIsImported(fncIndex)) {
      return getImpFuncDef(fncIndex).importFnVersion == NativeSymbol::ImportFnVersion::V2;
    }
    return false;
  }

  /// @brief Definition of an element in the table of this WebAssembly module
  struct TableElement final {
    uint32_t fncIndex;            ///< Function index of the function in this WebAssembly module
    uint32_t exportWrapperOffset; ///< Offset of the C++ to Wasm wrapper function in output binary
  };

  bool hasTable = false;                     ///< Whether this WebAssembly module has a table
  bool tableHasSizeLimit = false;            ///< Whether the table of this WebAssembly module has a size limit
  bool tableIsExported = false;              ///< Whether the table of this WebAssembly module is exported
  uint32_t tableInitialSize = 0U;            ///< Initial size of the table of this WebAssembly module
  uint32_t tableMaximumSize = 0U;            ///< Maximum size of the table of this WebAssembly module
  OffsetHandler<TableElement> tableElements; ///< Array of the function indices (as uint32_t) of the elements in the table
                                             ///< of this WebAssembly module

  bool hasMemory = false;          ///< Whether this WebAssembly module has a linear memory
  bool memoryHasSizeLimit = false; ///< Whether the linear memory of this WebAssembly module has a size limit
  bool memoryIsExported = false;   ///< Whether the linear memory of this WebAssembly module is exported
  uint32_t memoryInitialSize = 0U; ///< Initial size of the linear memory of this WebAssembly module in multiples of 64kB (Wasm page size)
  uint32_t memoryMaximumSize = 0U; ///< Maximum size of the linear memory of this WebAssembly module in multiples of 64kB (Wasm page size)

  ///
  /// @brief Definition of a global variable that is defined in this WebAssembly module
  /// NOTE: Imported global variables are currently not supported
  ///
  class GlobalDef final {
  public:
    TReg reg{};                   ///<  CPU register this variable is stored in (Index defined by the backend, if type is REGISTER)
    uint32_t linkDataOffset = 0U; ///< At which offset in the link data within the job memory this variable is stored
    ConstUnion initialValue = {}; ///< Initial value of this global variable
    bool isMutable = false;       ///< Whether this global variable is mutable or constant
    MachineType type = {};        ///< Type (I32, I64, F32 or F64) of this global variable
  };
  uint32_t numNonImportedGlobals = 0U; ///< Number of non-imported global variables in this WebAssembly module
  OffsetHandler<GlobalDef> globals;    ///< Array of GlobalDefs representing all global variables in this WebAssembly module

  uint32_t numGlobalsInGPR = 0U; ///< Number of globals that are stored in general purpose CPU registers

  uint32_t numExports = 0U;       ///< Total number of exports from this WebAssembly module (Exported tables, global variables,
                                  ///< memories and functions)
  OffsetHandler<uint8_t> exports; ///< Data for all exports, serialized and deserialized by the compiler frontend

  bool hasStartFunction = false;    ///< Whether this WebAssembly module has a start function (in the start section)
  uint32_t startFunctionIndex = 0U; ///< Index of the start function (in the start section) of this WebAssembly module

  ///
  /// @brief Definition of a local variable
  ///
  class LocalDef final {
  public:
    MachineType type = MachineType::INVALID;               ///< Type (I32, I64, F32 or F64) of this local variable within a function
    StorageType currentStorageType = StorageType::INVALID; ///< VariableStorage describing where this local variable is
                                                           ///< stored (i.e. in a register or on stack)
    TReg reg{};                       ///<  CPU register this variable is stored in (Index defined by the backend, if type is REGISTER)
    uint32_t stackFramePosition = 0U; ///< Offset in the current stack frame (if type is STACKMEMORY)

    /// @brief modify local storage type after initialized.
    void markLocalInitialized() VB_NOEXCEPT {
      assert(currentStorageType == StorageType::CONSTANT && "uninitialized local variable must be constant storage");
      currentStorageType = (reg == TReg::NONE) ? StorageType::STACKMEMORY : StorageType::REGISTER;
    }

    /// @brief get initialize status of local variable at the beginning of function.
    static StorageType getInitializedStorageType(TReg const chosenReg, bool const isParam) VB_NOEXCEPT;
  };

  ///
  /// @brief Array of LocalDefs for the current function
  ///
  /// Offset is the same for all functions, content will be reset for each parsed function
  ///
  OffsetHandler<LocalDef> localDefs;

  ControlFlowState currentState{}; ///< current control flow related state

  uint32_t numDataSegments = UINT32_MAX; ///< Number of data segments (defined in the data section) of this WebAssembly module

  uint32_t linkDataLength = 0U; ///< Length of the link data in the job memory in bytes

  ///
  /// @brief Function specific properties
  ///
  /// These properties will be reset and initialized by the frontend when entering/parsing a new function body
  ///
  class FunctionInfo final {
  public:
    uint32_t index = 0U;                ///< Index of the current function
    uint32_t numParams = 0U;            ///< Number of parameters of the current function
    uint32_t numLocals = 0U;            ///< Number of locals of the current function
    uint32_t numLocalsInGPR = 0U;       ///< Number of locals of the current function that are stored in general purpose CPU registers
    uint32_t numLocalsInFPR = 0U;       ///< Number of locals of the current function that are stored in floating point CPU registers
    uint32_t paramWidth = 0U;           ///< Total width/size of all stack-passed parameters of the current function in bytes
    uint32_t directLocalsWidth = 0U;    ///< Total width/size of all locals of the current function that are stored on the stack in bytes
    uint32_t stackFrameSize = 0U;       ///< Total size of the current stack frame in bytes (Parameters, potential return
                                        ///< address, locals and stack variables)
    Stack::iterator lastBlockReference; ///< Iterator of the StackElement that represents the last block (BLOCK, LOOP, IFBLOCK) in
                                        ///< this function body (empty iterator means there is no open block)

    bool unreachable = false;        ///< Whether the current function frame has been marked unreachable
    bool properlyTerminated = false; ///< Whether the current function has been properly terminated (i.e. with an END
                                     ///< instruction and with the correct number of elements still on the stack)
  };
  FunctionInfo fnc; ///< FunctionInfo instance for the current function

  OffsetHandler<uint32_t> wasmFncBodyBinaryPositions; ///< Array of the offsets in the output binary where each function
                                                      ///< body of the internally-defined WebAssembly function starts

  /// @brief Positions of the different helper functions (not functions that are defined in the Wasm module)
  class HelperFunctionPositions final {
  public:
#if LINEAR_MEMORY_BOUNDS_CHECKS
    /// @brief Offset in the output binary where the wrapper for calling the extension request (MemoryHelper) from Wasm
    /// starts This extension request wrapper first checks whether the accessed address (passed as argument to the
    /// wrapper) really doesn't lie within the allowed region. If it does after all, it simply returns. Otherwise it
    /// calls the extension request and tries to extend the current memory allocation for the job memory.
    uint32_t extensionRequest = 0xFF'FF'FF'FFU;
#else
    /// @brief Offset in the output binary where the landing pad starts
    /// This allows to exit a signal handler, trampoline back to another function and properly return to the WebAssembly
    /// context afterwards
    uint32_t landingPad = 0xFF'FF'FF'FFU;
#endif
    /// @brief Offset in the output binary where the stacktrace collector function starts
    /// This function collects the stacktrace entries from the stack and copies the function indices into the respective
    /// stacktrace buffer in the job memory
    uint32_t genericTrapHandler = 0xFF'FF'FF'FFU;
#if BUILTIN_FUNCTIONS
    uint32_t builtinTracePointHandler = 0xFF'FF'FF'FFU; ///< Offset in the output binary where the builtin trace point handler starts
#endif
  };
  HelperFunctionPositions helperFunctionBinaryPositions; ///< Positions of helper functions in the output binary

  ///
  /// @brief An array of iterator (element on the stack) for each local and global variable and stack elements pointing to
  /// the last inserted/occurrence element of a given type on the stack
  ///
  /// Stack elements representing the same variable (and for temporary stack variables also those of the same storage
  /// type) are represented as a doubly-linked list. This allows to quickly find and replace all copies/representations
  /// of a specific variable on the stack, e.g. when spilling a variable
  OffsetHandler<Stack::iterator> referencesToLastOccurrenceOnStack;

  std::array<uint32_t, 8U> referenceMap{}; ///< Helper array for more efficient accesses in getReferenceToLastOccurrenceOnStack

  ///
  /// @brief Set up memory for the array that will hold the references
  ///
  /// @param compilerMemory Reference to the compiler memory, will allocate memory starting from the current cursor
  inline void setupReferenceMap(MemWriter &compilerMemory) {
    static_cast<void>(compilerMemory.alignForType<Stack::iterator>());
    referencesToLastOccurrenceOnStack.setOffset(compilerMemory.size(), compilerMemory);

    static_assert((static_cast<uint32_t>(sizeof(Stack::iterator)) *
                   ((((NBackend::totalNumRegs + ImplementationLimits::numParams) + ImplementationLimits::numDirectLocals) +
                     ImplementationLimits::numNonImportedGlobals) +
                    1U)) <= UINT32_MAX,
                  "Index too large");

    uint32_t const numEntries{((NBackend::totalNumRegs + fnc.numLocals) + numNonImportedGlobals) + 1_U32 /* TEMPSTACK */};
    uint32_t const bytesToReserve{static_cast<uint32_t>(sizeof(Stack::iterator)) * numEntries};

    // Reserve space for references
    compilerMemory.step(bytesToReserve);

    for (uint32_t i{0U}; i < numEntries; ++i) {
      referencesToLastOccurrenceOnStack[i] = Stack::iterator();
    }

    referenceMap[static_cast<uint32_t>(StackType::SCRATCHREGISTER)] = 0U;
    referenceMap[static_cast<uint32_t>(StackType::LOCAL)] = NBackend::totalNumRegs;
    referenceMap[static_cast<uint32_t>(StackType::GLOBAL)] = NBackend::totalNumRegs + fnc.numLocals;
    referenceMap[getStackMemoryInReferenceMapIndex()] = NBackend::totalNumRegs + fnc.numLocals + numNonImportedGlobals;
  }

  ///
  /// @brief Get the reference position of the last/highest occurrence of the given StackElement
  ///
  /// @param element StackElement for which to find the reference to the last occurrence on stack
  /// @return Position on the referencesToLastOccurrenceOnStack of the given StackElement
  inline uint32_t getReferencePosition(StackElement const &element) const VB_NOEXCEPT {
    StackType const baseType{element.getBaseType()};
    assert((baseType == StackType::LOCAL) || (baseType == StackType::GLOBAL) || (baseType == StackType::SCRATCHREGISTER) ||
           (baseType == StackType::TEMP_RESULT));

    if (baseType == StackType::TEMP_RESULT) {
      return element.data.variableData.location.calculationResult.referencePosition;
    } else {
      uint32_t const index{static_cast<uint32_t>(baseType)};
      uint32_t refPos{referenceMap[index]};
      static_assert(sizeof(element.data.variableData.location.globalIdx) == sizeof(uint32_t), "Not same size");
      static_assert(sizeof(element.data.variableData.location.localIdx) == sizeof(uint32_t), "Not same size");
      static_assert(sizeof(element.data.variableData.location.reg) == sizeof(uint32_t), "Not same size");

      uint32_t const data{readFromPtr<uint32_t>(pCast<uint8_t const *>(&element.data.variableData.location))};
      refPos += data;
      return refPos;
    }
  }

  ///
  /// @brief Get the reference position of the last/highest occurrence of the given REG
  ///
  /// @param reg REG for which to find the reference to the last occurrence on stack
  /// @return Position on the referencesToLastOccurrenceOnStack of the given REG
  inline uint32_t getReferencePosition(TReg const reg) const VB_NOEXCEPT {
    return referenceMap[static_cast<uint32_t>(StackType::SCRATCHREGISTER)] + static_cast<uint32_t>(reg);
  }

  /// @brief get reference position for stack memory
  inline uint32_t getStackMemoryReferencePosition() const VB_NOEXCEPT {
    return referenceMap[getStackMemoryInReferenceMapIndex()];
  }

  ///
  /// @brief Get the reference describing the iterator on the stack of the last/highest occurrence of the given
  /// StackElement
  ///
  /// The reference is the iterator on the stack pointing to the start/end of the linked list of copies of the same
  /// variables on the stack (i.e. the last/highest occurrence of an element of the given type on the stack)
  ///
  /// @param element StackElement for which to find the reference to the last occurrence on stack
  /// @return Reference to the variable where the last occurrence (stack::iterator) for the given element is stored
  inline Stack::iterator &getReferenceToLastOccurrenceOnStack(StackElement const &element) const VB_NOEXCEPT {
    return referencesToLastOccurrenceOnStack[getReferencePosition(element)];
  }

  ///
  /// @brief Get the reference describing the iterator on the stack of the last/highest occurrence of the given
  /// REG
  ///
  /// The reference is the iterator on the stack pointing to the start/end of the linked list of copies of the same
  /// variables on the stack (i.e. the last/highest occurrence of an element of the given type on the stack)
  ///
  /// @param reg REG for which to find the reference to the last occurrence on stack
  /// @return Reference to the variable where the last occurrence (stack::iterator) for the given element is stored
  inline Stack::iterator &getReferenceToLastOccurrenceOnStack(TReg const reg) const VB_NOEXCEPT {
    return referencesToLastOccurrenceOnStack[getReferencePosition(reg)];
  }

  /// @brief see @b getReferenceToLastOccurrenceOnStack. It is specialization of stack memory.
  inline Stack::iterator &getReferenceToLastOccurrenceOnStackForStackMemory() const VB_NOEXCEPT {
    return referencesToLastOccurrenceOnStack[getStackMemoryReferencePosition()];
  }

  uint32_t stacktraceRecordCount = 0U; ///< Count how many stacktrace records are stored

  bool debugMode = false;                              ///< Whether debug mode is enabled
  uint32_t bytecodePosOfLastParsedInstruction = 0U;    ///< Position in the input Wasm bytecode of the last parsed instruction
  uint32_t outputSizeBeforeLastParsedInstruction = 0U; ///< Size of the (compiled) output binary before the last parsed instruction

  bool forceHighRegisterPressureForTesting = false; ///< Whether to force high register pressure (ONLY FOR TESTING)

  ///
  /// @brief Get the width of the always present portion of the stack frame in bytes
  ///
  /// This represents storage locations for parameters, the extra reserved stack width, local variables and, depending
  /// on the specific CPU architecture, a return address that is stored on the stack
  ///
  /// @return uint32_t How many bytes of the stack frame will always be present until the function body is closed
  inline uint32_t getFixedStackFrameWidth() const VB_NOEXCEPT {
    return fnc.paramWidth + roundUpToPow2(NBackend::returnAddrWidth, 3U) + fnc.directLocalsWidth;
  };

  ///
  /// @brief Get the length of the basedata for this module
  ///
  /// @return uint32_t Bytes of the basedata
  inline uint32_t getBasedataLength() const VB_NOEXCEPT {
    return Basedata::length(linkDataLength, stacktraceRecordCount);
  }

  ///
  /// @brief Get the MachineType of a StackElement
  ///
  /// @param element StackElement for which the underlying MachineType should be retrieved
  /// @return MachineType Underlying MachineType of the StackElement
  MachineType getMachineType(StackElement const *const element) const VB_NOEXCEPT;
  ///
  /// @brief Get the storage location of a StackElement
  ///
  /// @param element StackElement for which the underlying storage location (in form of a VariableStorage) should be
  /// retrieved
  /// @return VariableStorage Underlying VariableStorage describing where the StackElement is actually stored (e.g.
  /// whether it is just a constant, it's on stack, in job memory or in a CPU register)
  VariableStorage getStorage(StackElement const &element) const VB_NOEXCEPT;

  ///
  /// @brief Get the number of parameters for a given function signature
  ///
  /// @param sigIndex Signature/type index
  /// @return uint32_t Number of parameter for the function signature/type index
  uint32_t getNumParamsForSignature(uint32_t const sigIndex) const VB_NOEXCEPT;

  ///
  /// @brief Get the number of return values for a given function signature
  ///
  /// @param sigIndex Signature/type index
  /// @return uint32_t Number of return values for the function signature/type index
  uint32_t getNumReturnValuesForSignature(uint32_t const sigIndex) const VB_NOEXCEPT;

  ///
  /// @brief Iterate parameters of the signature with an optional lambda
  ///
  /// @param sigIndex Index of the function signature/type
  /// @param lambda Lambda which to execute for each function parameter, returns true to break the iteration
  /// @param reverse Whether to reverse the iteration order (if reverse = true parameters will be iterated from the last
  /// parameter to the first one)
  void iterateParamsForSignature(uint32_t const sigIndex, FunctionRef<void(MachineType)> const &lambda = FunctionRef<void(MachineType)>(nullptr),
                                 bool const reverse = false) const;

  ///
  /// @brief Iterate return values of the signature with lambda
  ///
  /// @param sigIndex Index of the function signature/type
  /// @param lambda Lambda which to execute for each function return value, returns true to break the iteration
  /// @param reverse Whether to reverse the iteration order (if reverse = true return values will be iterated from the last return value to the first
  /// one)
  void iterateResultsForSignature(uint32_t const sigIndex, FunctionRef<void(MachineType)> const &lambda, bool const reverse = false) const;

  ///
  /// @brief Get the Link Status of a function
  ///
  /// @param fncIdx Index of the function
  /// @return bool Returns true if the function is linked or the function is defined within the Wasm module, returns 0
  /// if the function is not linked or out of bounds.
  ///
  bool functionIsLinked(uint32_t const fncIdx) const VB_NOEXCEPT;

  ///
  /// @brief Turn a given StackElement into a RegMask where the register, the data of the StackElement is currently
  /// stored in, is masked
  ///
  /// Returns RegMask::none() (an empty RegMask with no bits/registers masked) if the StackElement does not currently
  /// reside in a CPU register
  ///
  /// @param element StackElement for which a RegMask should be generated for which the register where it is currently
  /// stored is masked
  /// @return RegMask Resulting RegMask for which the register is masked where the StackElement is currently stored
  RegMask maskForElement(StackElement const *const element) const VB_NOEXCEPT;

  /// @brief get number of statically allocated GPR
  inline uint32_t getNumStaticallyAllocatedGPRs() const VB_NOEXCEPT {
    if (forceHighRegisterPressureForTesting) {
      return NBackend::WasmABI::numGPR - NBackend::WasmABI::resScratchRegsGPR;
    }
    return numGlobalsInGPR + fnc.numLocalsInGPR;
  }

  /// @brief get number of statically allocated FPR
  inline uint32_t getNumStaticallyAllocatedFPRs() const VB_NOEXCEPT {
    if (forceHighRegisterPressureForTesting) {
      return NBackend::WasmABI::numFPR - NBackend::WasmABI::resScratchRegsFPR;
    }
    return fnc.numLocalsInFPR;
  }

  /// @brief get max allowed GPR number of locals stored in reg.
  inline uint32_t getMaxNumsLocalsInGPRs() const VB_NOEXCEPT {
    return NBackend::WasmABI::numGPR - (NBackend::WasmABI::resScratchRegsGPR + numGlobalsInGPR);
  }

  /// @brief get max allowed FPR number of locals stored in reg.
  inline uint32_t getMaxNumsLocalsInFPRs() const VB_NOEXCEPT {
    return NBackend::WasmABI::numFPR - NBackend::WasmABI::resScratchRegsFPR;
  }

  /// @brief get start index of local in GPR.
  inline uint32_t getLocalStartIndexInGPRs() const VB_NOEXCEPT {
    return numGlobalsInGPR;
  }

  /// @brief get start index of local in FPR.
  inline constexpr uint32_t getLocalStartIndexInFPRs() const VB_NOEXCEPT {
    return 0U;
  }

  /// @brief Get the stack frame size before executing a return instruction
  inline uint32_t getStackFrameSizeBeforeReturn() const VB_NOEXCEPT {
    return fnc.paramWidth + NBackend::returnAddrWidth;
  }

private:
  /// @brief get reference map index for stack memory element
  inline static constexpr uint32_t getStackMemoryInReferenceMapIndex() VB_NOEXCEPT {
    constexpr uint32_t index{2U};
    static_assert(static_cast<uint32_t>(StackType::SCRATCHREGISTER) != index, "please select another index for stack memory");
    static_assert(static_cast<uint32_t>(StackType::LOCAL) != index, "please select another index for stack memory");
    static_assert(static_cast<uint32_t>(StackType::GLOBAL) != index, "please select another index for stack memory");
    return index;
  }
};

} // namespace vb

#endif
