///
/// @file Compiler.hpp
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
#ifndef COMPILER_HPP
#define COMPILER_HPP

#include <cassert>
#include <cstdint>

#include "common/ManagedBinary.hpp"
#include "common/MemWriter.hpp"
#include "frontend/Frontend.hpp"

#include "src/core/common/ILogger.hpp"
#include "src/core/common/Span.hpp"
#include "src/core/compiler/backend/PlatformAdapter.hpp"
#include "src/core/compiler/common/ModuleInfo.hpp"
#include "src/core/compiler/common/Stack.hpp"
#include "src/core/compiler/frontend/ValidationStack.hpp"
#include "src/extensions/IAnalytics.hpp"

#if ENABLE_EXTENSIONS
#include "src/extensions/IDwarf.hpp"
#endif

namespace vb {

class Frontend;

///
/// @brief Compiler class
///
/// Compiles WebAssembly bytecode into an executable that can be executed by the corresponding Runtime
///
class Compiler final {
public:
  ///
  /// @brief Construct a new Compiler instance
  ///
  /// @param compilerMemoryReallocFnc ReallocFnc for internal compiler memory
  /// @param compilerMemoryAllocFnc AllocFnc for internal compiler memory
  /// @param compilerMemoryFreeFnc FreeFnc for internal compiler memory
  /// @param ctx User defined context
  /// @param binaryMemoryReallocFnc ReallocFnc for output memory
  /// @param allowUnknownImports Whether unknown imports are allowed. If this is true, any imports that are not
  /// explicitly linked will lead to a trap, otherwise compilation will fail.
  Compiler(ReallocFnc const compilerMemoryReallocFnc, AllocFnc const compilerMemoryAllocFnc, FreeFnc const compilerMemoryFreeFnc, void *const ctx,
           ReallocFnc const binaryMemoryReallocFnc, bool const allowUnknownImports = false);

  ///
  /// @brief Construct a new Compiler instance
  ///
  /// @param compilerMemoryReallocFnc ReallocFnc for internal compiler memory
  /// @param compilerMemoryAllocFnc AllocFnc for internal compiler memory
  /// @param compilerMemoryFreeFnc FreeFnc for internal compiler memory
  /// @param ctx User defined context
  /// @param binaryMemory output binary memory
  /// @param allowUnknownImports Whether unknown imports are allowed. If this is true, any imports that are not
  /// explicitly linked will lead to a trap, otherwise compilation will fail.
  Compiler(ReallocFnc const compilerMemoryReallocFnc, AllocFnc const compilerMemoryAllocFnc, FreeFnc const compilerMemoryFreeFnc, void *const ctx,
           ExtendableMemory &&binaryMemory, bool const allowUnknownImports = false);

  ///
  /// @brief Start compilation without linked NativeSymbols
  ///
  /// @tparam Binary Any type that implements data() and size() where data() returns a pointer to uint8_t
  /// @param bytecode Bytecode instance
  /// @return ManagedBinary Managed RAII output binary
  template <typename Binary> ManagedBinary compile(Binary bytecode) {
    return compile(Span<uint8_t const>(bytecode.data(), bytecode.size()), Span<NativeSymbol const>());
  }

  ///
  /// @brief Start compilation with linked NativeSymbols
  ///
  /// @tparam Binary Any type that implements data() and size() where data() returns a pointer to uint8_t
  /// @tparam SymbolList Any type that implements data() and size() where data() returns a pointer to NativeSymbol
  /// @param bytecode Bytecode instance
  /// @param symbolList SymbolList instance
  /// @return ManagedBinary Managed RAII output binary
  template <typename Binary, typename SymbolList> ManagedBinary compile(Binary bytecode, SymbolList const &symbolList) {
    return compile(Span<uint8_t const>(bytecode.data(), bytecode.size()), Span<NativeSymbol const>(symbolList.data(), symbolList.size()));
  }

  ///
  /// @brief Start compilation
  ///
  /// @param bytecode uint8_t bytecode Span
  /// @param symbolList NativeSymbol SPan
  /// @return ManagedBinary Managed RAII output binary
  /// @throws vb::RuntimeError If compilation failed
  ManagedBinary compile(Span<uint8_t const> const &bytecode, Span<NativeSymbol const> const &symbolList = Span<NativeSymbol const>());

  ///
  /// @brief Force high register pressure for testing
  /// This will leave the least number of registers available as scratch registers, irrespective of how many locals are
  /// used and thus simulates high register pressure NOTE: ONLY FOR TESTING, WILL IMPACT PERFORMANCE
  void forceHighRegisterPressureForTesting() VB_NOEXCEPT;

  ///
  /// @brief Enable debug mode
  ///
  /// This will disable optimizations, will compile WebAssembly instructions in order and will generate a debug map that
  /// maps bytecode offsets to output binary offsets
  ///
  /// @param debugMapReallocFnc ReallocFnc for debug map memory
  void enableDebugMode(ReallocFnc const debugMapReallocFnc = nullptr) VB_NOEXCEPT;

  ///
  /// @brief Disable debug mode
  ///
  void disableDebugMode() VB_NOEXCEPT;

  /// @brief Get the current debug mode setting
  ///
  /// @return bool
  bool getDebugMode() const VB_NOEXCEPT {
    return debugMode_;
  }

#if ENABLE_EXTENSIONS
  /// @brief see @b dwarfGenerator_
  void setDwarfGenerator(extension::IDwarf5Generator *const dwarfGenerator) VB_NOEXCEPT {
    dwarfGenerator_ = dwarfGenerator;
  }
  /// @brief see @b dwarfGenerator_
  extension::IDwarf5Generator *getDwarfGenerator() const VB_NOEXCEPT {
    return dwarfGenerator_;
  }
  /// @brief see @b analytics_
  void setAnalytics(extension::IAnalytics *const analytics) VB_NOEXCEPT {
    analytics_ = analytics;
  }
  /// @brief see @b analytics_
  extension::IAnalytics *getAnalytics() const VB_NOEXCEPT {
    return analytics_;
  }
#endif

  ///
  /// @brief Retrieve the debug map after compilation
  ///
  /// Debug map is an array of non-padded structs containing two uint32_t, with one being the input bytecode binary
  /// offset and the other being the output binary offset NOTE: This will std::move the memory out of the compiler and
  /// is thus only callable once
  ///
  /// @return ManagedBinary Managed RAII debug map binary
  ManagedBinary retrieveDebugMap() VB_NOEXCEPT {
    return debugMap_.toManagedBinary();
  }

  ///
  /// @brief Set number of stacktrace records that are stored and are available after a trap
  ///
  /// Exceeding this number by a deeply nested function call and returning from it will make the records greater than
  /// this number available again.
  ///
  /// @param count Maximum number of stacktrace records (maximum allowed value is 50)
  /// @throws std::runtime_error If maximum value (50) is exceeded
  inline void setStacktraceRecordCount(uint8_t const count) {
    if (count > 50U) {
      throw RuntimeError(ErrorCode::Maximum_stack_trace_record_count_is_50);
    }
    stacktraceRecordCount_ = count;
  }

  ///
  /// @brief Whether the stacktrace shall be recorded, either if stacktrace is explicitly enabled or debugMode is
  /// enabled (debugMode needs stacktrace too)
  ///
  /// @return Whether stacktrace shall be recorded
  inline bool shallRecordStacktrace() const VB_NOEXCEPT {
    return isStacktraceEnabled() || getDebugMode();
  }

  ///
  /// @brief Get the number of stacktrace records that are stored
  ///
  /// @return Maximum number of stacktrace records
  inline uint32_t getStacktraceRecordCount() const VB_NOEXCEPT {
    return stacktraceRecordCount_;
  }

  ///
  /// @brief Check if the stacktrace is enabled
  ///
  /// @return Whether the stacktrace is enabled
  inline bool isStacktraceEnabled() const VB_NOEXCEPT {
    return stacktraceRecordCount_ > 0U;
  }

  ///
  /// @brief Set an abstract logger for logging output with descriptions why compilation failed
  ///
  /// @param loggerIn Logger interface class
  inline void setLogger(ILogger *const loggerIn) VB_NOEXCEPT {
    this->logger_ = loggerIn;
  }

private:
  ///
  /// @brief Logs messages to the logger interface
  ///
  /// @return Pointer to logger interface
  inline ILogger *logging() const VB_NOEXCEPT {
    return logger_;
  }

  Stack stack_;                     ///< Compiler stack
  ValidationStack validationStack_; ///< Wasm Validation stack
  MemWriter memory_;                ///< Internal compiler memory
  MemWriter output_;                ///< Compiler output binary

  ModuleInfo moduleInfo_; ///< Information about the WebAssembly module
  TBackend backend_;      ///< Compiler backend
  ILogger *logger_;       ///< Pointer to an ILogger interface conforming class

  bool debugMode_;     ///< Whether debug mode is enabled
  MemWriter debugMap_; ///< The output debug map that maps bytecode offsets to output binary offsets for debugging

  bool forceHighRegisterPressureForTesting_; ///< Whether to force (simulate) high register pressure for testing (ONLY
                                             ///< FOR TESTING)

  uint32_t stacktraceRecordCount_; ///< Number of stacktrace records to keep track of, i.e. number that will be
                                   ///< available for printing

  bool allowUnknownImports_; ///< Whether unknown imports are allowed. If this is true, unknown imports lead to a trap
                             ///< when called

  Common common_; ///< Instance of common utility library
#if ENABLE_EXTENSIONS
  extension::IDwarf5Generator *dwarfGenerator_; ///< Dwarf generator for analytics
  extension::IAnalytics *analytics_;            ///< Analytics interface for collecting data
#endif

  friend Common;   ///< So Common can access the memories
  friend Frontend; ///< So Frontend can log and access memories
};

} // namespace vb

#endif /* COMPILER_H */
