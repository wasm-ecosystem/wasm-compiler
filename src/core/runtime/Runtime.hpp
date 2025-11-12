///
/// @file Runtime.hpp
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
#ifndef RUNTIME_HPP
#define RUNTIME_HPP

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <type_traits>
#include <utility>

#include "MemoryHelper.hpp"

#include "src/config.hpp"
#include "src/core/common/BinaryModule.hpp"
#include "src/core/common/ExtendableMemory.hpp"
#include "src/core/common/FunctionRef.hpp"
#include "src/core/common/ILogger.hpp"
#include "src/core/common/NativeSymbol.hpp"
#include "src/core/common/SanitizeHelper.hpp"
#include "src/core/common/SignatureType.hpp"
#include "src/core/common/Span.hpp"
#include "src/core/common/TrapCode.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/basedataoffsets.hpp"
#include "src/core/common/util.hpp"

namespace vb {

class RawModuleFunction;
template <size_t NumReturnValue, typename... T> class ModuleFunction;
template <typename T> class ModuleGlobal;

///
/// @brief Runtime class to execute the executable that has been produced by the compiler
///
class Runtime final {
public:
  Runtime(Runtime const &) = delete;
  Runtime(Runtime &&other) VB_NOEXCEPT; ///< Move constructor
  Runtime &operator=(Runtime const &) & = delete;
  Runtime &operator=(Runtime &&other) & VB_NOEXCEPT; ///< Move operator
  ~Runtime() = default;

  ///
  /// @brief user-defined no-throw swap function
  ///
  /// @param lhs Left hand side Object
  /// @param rhs Right hand side Object
  static inline void swap(Runtime &lhs, Runtime &&rhs) VB_NOEXCEPT {
    if (&lhs != &rhs) {
      lhs.disabled_ = rhs.disabled_;
      rhs.disabled_ = true;

      lhs.queuedStartFncOffset_ = rhs.queuedStartFncOffset_;
      // coverity[autosar_cpp14_a8_4_5_violation]
      lhs.binaryModule_ = rhs.binaryModule_;
#if LINEAR_MEMORY_BOUNDS_CHECKS
      lhs.jobMemory_ = std::move(rhs.jobMemory_);
#else
      lhs.jobMemoryStart_ = rhs.jobMemoryStart_;
      lhs.memoryExtendFunction_ = std::move(rhs.memoryExtendFunction_);
      lhs.memoryProbeFunction_ = std::move(rhs.memoryProbeFunction_);
      lhs.memoryShrinkFunction_ = std::move(rhs.memoryShrinkFunction_);
      lhs.memoryUsageFunction_ = std::move(rhs.memoryUsageFunction_);
      rhs.jobMemoryStart_ = nullptr;
#endif
    }
  }

#if LINEAR_MEMORY_BOUNDS_CHECKS

  /// @brief Construct a new default runtime
  // coverity[autosar_cpp14_a12_1_5_violation]
  inline Runtime() VB_NOEXCEPT : disabled_(true), queuedStartFncOffset_(0U), binaryModule_() {
  }

  ///
  /// @brief Construct a new runtime from a binary and a realloc-like function for allocation of the job memory
  ///
  /// @tparam Binary Input binary type. Any class that implements data() (returning a uint8_t *) and size()
  /// @param module Input binary; executable produced by the compiler
  /// @param jobMemoryReallocFnc A realloc-like function for allocation of the job memory
  /// @param ctx Custom context for import functions
  template <typename Binary>
  Runtime(Binary const &module, ReallocFnc jobMemoryReallocFnc, void *const ctx)
      : disabled_(false), queuedStartFncOffset_(0U), jobMemory_{ExtendableMemory(jobMemoryReallocFnc, nullptr, 0U, ctx)}, binaryModule_() {
    initRuntime(Span<const uint8_t>{module.data(), module.size()}, Span<NativeSymbol const>{}, ctx);
  }

  ///
  /// @brief Construct a new runtime instance from a binary, a realloc-like function for allocation of the job memory
  /// and a list of NativeSymbols that should be linked dynamically
  ///
  /// @tparam Binary Input binary type. Any class that implements data() (returning a uint8_t *) and size()
  /// @tparam SymbolList NativeSymbol list type. Any class that implements data() (returning a NativeSymbol *) and
  /// size()
  /// @param module Input binary; executable produced by the compiler
  /// @param jobMemoryReallocFnc A realloc-like function for allocation of the job memory
  /// @param ctx Custom context for callback functions
  /// @param dynamicallyLinkedSymbols List of host functions that should be linked dynamically
  template <typename Binary, typename SymbolList>
  Runtime(Binary const &module, ReallocFnc jobMemoryReallocFnc, SymbolList const &dynamicallyLinkedSymbols, void *const ctx)
      : disabled_(false), queuedStartFncOffset_(0U), jobMemory_{ExtendableMemory(jobMemoryReallocFnc, nullptr, 0U, ctx)}, binaryModule_() {
    initRuntime(Span<const uint8_t>{module.data(), module.size()},
                Span<NativeSymbol const>(dynamicallyLinkedSymbols.data(), dynamicallyLinkedSymbols.size()), ctx);
  }

  ///
  /// @brief Construct a new runtime instance from a binary, a realloc-like function for allocation of the job memory
  /// and a list of NativeSymbols that should be linked dynamically
  ///
  /// @param module Input binary; executable produced by the compiler
  /// @param jobMemoryReallocFnc A realloc-like function for allocation of the job memory
  /// @param dynamicallyLinkedSymbols List of host functions that should be linked dynamically
  /// @param ctx Custom context
  Runtime(Span<uint8_t const> const &module, ReallocFnc const jobMemoryReallocFnc,
          Span<NativeSymbol const> const &dynamicallyLinkedSymbols = Span<NativeSymbol const>(), void *const ctx = nullptr)
      : disabled_(false), queuedStartFncOffset_(0U), jobMemory_{ExtendableMemory(jobMemoryReallocFnc, nullptr, 0U, ctx)}, binaryModule_() {
    initRuntime(module, dynamicallyLinkedSymbols, ctx);
  }

  ///
  /// @brief Initialize a new runtime instance from a binary, a realloc-like function for allocation of the job memory
  /// and a list of NativeSymbols that should be linked dynamically
  ///
  /// @param module Input binary; executable produced by the compiler
  /// @param dynamicallyLinkedSymbols List of host functions that should be linked dynamically
  /// @param ctx Custom context for import functions
  inline void initRuntime(Span<uint8_t const> const &module, Span<NativeSymbol const> const &dynamicallyLinkedSymbols, void *const ctx) {
    binaryModule_.init(module);
    jobMemory_.resize(8U);
    init(dynamicallyLinkedSymbols, ctx);
  }

#else

  /// @brief Construct a new default runtime
  // coverity[autosar_cpp14_a12_1_5_violation] initial member variable with nullptr
  inline Runtime() VB_NOEXCEPT : disabled_(true),
                                 queuedStartFncOffset_(0U),
                                 jobMemoryStart_(nullptr),
                                 memoryExtendFunction_(nullptr),
                                 memoryProbeFunction_(nullptr),
                                 binaryModule_() {
  }

  ///
  /// @brief Construct a new runtime instance from a binary and an allocator
  ///
  /// @tparam Allocator Allocator type. Any class that implements init(..), extend(..) and probe(..), e.g.
  /// LinearMemoryAllocator
  /// @tparam Binary Input binary type. Any class that implements data() (returning a uint8_t *) and size()
  /// @param module Input binary; executable produced by the compiler
  /// @param allocator Allocator object; e.g. an instance of LinearMemoryAllocator
  /// @param ctx Custom context for import functions
  template <typename Allocator, typename Binary>
  Runtime(Binary const &module, Allocator &allocator, void *const ctx)
      : disabled_(false), queuedStartFncOffset_(0U), jobMemoryStart_(nullptr), binaryModule_() {
    initRuntime(Span<uint8_t const>(module.data(), module.size()), allocator, Span<NativeSymbol const>(), ctx);
  }

  ///
  /// @brief Construct a new runtime instance from a binary, an allocator and a list of NativeSymbols that should be
  /// linked dynamically
  ///
  /// @tparam Allocator Allocator type. Any class that implements init(..), extend(..) and probe(..), e.g.
  /// LinearMemoryAllocator
  /// @tparam Binary Input binary type. Any class that implements data() (returning a uint8_t *) and size()
  /// @tparam SymbolList NativeSymbol list type. Any class that implements data() (returning a NativeSymbol *) and
  /// size()
  /// @param module Input binary; executable produced by the compiler
  /// @param allocator Allocator object; e.g. an instance of LinearMemoryAllocator
  /// @param dynamicallyLinkedSymbols List of host functions that should be linked dynamically
  /// @param ctx Custom context for import functions
  template <typename Allocator, typename Binary, typename SymbolList>
  Runtime(Binary const &module, Allocator &allocator, SymbolList const &dynamicallyLinkedSymbols, void *const ctx)
      : disabled_(false), queuedStartFncOffset_(0U), jobMemoryStart_(nullptr), binaryModule_() {
    initRuntime(Span<uint8_t const>(module.data(), module.size()), allocator,
                Span<NativeSymbol const>(dynamicallyLinkedSymbols.data(), dynamicallyLinkedSymbols.size()), ctx);
  }

  ///
  /// @brief Construct a new runtime instance from a binary, an allocator and a list of NativeSymbols that should be
  /// linked dynamically
  ///
  /// @tparam Allocator Allocator type. Any class that implements init(..), extend(..) and probe(..), e.g.
  /// LinearMemoryAllocator
  /// @param module Input binary; executable produced by the compiler
  /// @param allocator Allocator object; e.g. an instance of LinearMemoryAllocator
  /// @param dynamicallyLinkedSymbols List of host functions that should be linked dynamically
  /// @param ctx Custom context for import functions
  template <typename Allocator>
  Runtime(Span<uint8_t const> const &module, Allocator &allocator, Span<NativeSymbol const> const &dynamicallyLinkedSymbols, void *const ctx) VB_THROW
      : disabled_(false),
        queuedStartFncOffset_(0U),
        jobMemoryStart_(nullptr),
        binaryModule_() {
    initRuntime(module, allocator, dynamicallyLinkedSymbols, ctx);
  }

  ///
  /// @brief Initialize a new runtime instance from a binary, an allocator and a list of NativeSymbols that should be
  /// linked dynamically
  ///
  /// @tparam Allocator Allocator type. Any class that implements init(..), extend(..) and probe(..), e.g.
  /// LinearMemoryAllocator
  /// @param module Input binary; executable produced by the compiler
  /// @param allocator Allocator object; e.g. an instance of LinearMemoryAllocator
  /// @param dynamicallyLinkedSymbols List of host functions that should be linked dynamically
  /// @param ctx Custom context for import functions
  template <typename Allocator>
  void initRuntime(Span<uint8_t const> const &module, Allocator &allocator, Span<NativeSymbol const> const &dynamicallyLinkedSymbols,
                   void *const ctx) VB_THROW {
    binaryModule_.init(module);
    jobMemoryStart_ = pCast<uint8_t *>(allocator.init(getBasedataLength(), getInitialLinMemSizeInPages()));
    // coverity[autosar_cpp14_a5_1_4_violation]
    memoryExtendFunction_ = [&allocator](uint32_t const linearMemoryTotalPages) VB_THROW -> bool {
      return allocator.extend(linearMemoryTotalPages);
    };
    // coverity[autosar_cpp14_a5_1_4_violation]
    memoryProbeFunction_ = [&allocator](uint32_t const address) VB_THROW -> bool {
      return allocator.probe(address);
    };
    // coverity[autosar_cpp14_a5_1_4_violation]
    memoryShrinkFunction_ = [&allocator](uint32_t const minimumLength) VB_THROW -> bool {
      return allocator.shrink(minimumLength);
    };
    // coverity[autosar_cpp14_a5_1_4_violation]
    memoryUsageFunction_ = [&allocator](uint32_t const baseDataSize) VB_NOEXCEPT -> uint32_t {
      return static_cast<uint32_t>(allocator.getLinearMemorySize(baseDataSize));
    };
    init(dynamicallyLinkedSymbols, ctx);
  }

#endif

  ///
  /// @brief Execute the start function (from the start section of the WebAssembly module)
  ///
  /// Must be called before any other function is executed. Does nothing if no start section is present in the
  /// WebAssembly module
  ///
  void start();

  ///
  /// @brief Update the pointer to the executable binary
  ///
  /// @param module Binary module onto the executable module after init
  /// @throws std::runtime_error if the new module memory is not 16-byte aligned
  void updateBinaryModule(BinaryModule const &module) const VB_NOEXCEPT;

  ///
  /// @brief Whether this runtime currently has at least one active function frame
  ///
  /// This is true if executed e.g. from within a host function
  ///
  /// @return bool Whether this runtime currently has at least one active function frame
  bool hasActiveFrame() const VB_NOEXCEPT;

  ///
  /// @brief Validate whether a region of the linear memory of this WebAssembly module can be accessed (read or write)
  /// and returns a pointer that can be used to access this region
  ///
  /// This must be called before every access to the linear memory from C++ to make sure the linear memory address is
  /// valid and is allocated. This function will not be able to tell whether there is any data already in the linear
  /// memory at this spot. This should preferably only be used with regions (offset and size) that the WebAssembly
  /// module passes to the C++ context. Not calling this (even before reads) might lead to segfaults or undefined data
  /// since the portion might not be allocated yet. This function returns a pointer that can then be used to access
  /// (read or write) this region via this pointer. The next n bytes can then safely be accessed.
  ///
  /// If a host function is called from within WebAssembly that expects the host function to write 2 32-bit integers as
  /// a total of 8 bytes to the linear memory at offset 300 the function should be called as follows:
  /// getLinearMemoryRegion(300, 8).
  ///
  /// @param offset Offset in the linear memory where the region, that should be accessed, starts
  /// @param size Size of the region that should be accessed in bytes
  /// @return uint8_t* Pointer to the region that can be used to access it from C++
  /// @throws vb::RuntimeError memory out of range
  uint8_t *getLinearMemoryRegion(uint32_t const offset, uint32_t const size) const;

  ///
  /// @brief Get the current formal linear memory size in multiples of WebAssembly page size (64kB)
  ///
  /// @return uint32_t Current linear memory size in multiples of WebAssembly page size
  uint32_t getLinearMemorySizeInPages() const VB_NOEXCEPT;

#if LINEAR_MEMORY_BOUNDS_CHECKS
  ///
  /// @brief Get the current active size of the job memory
  ///
  /// This returns the total active size of the WebAssembly module's linear memory and its basedata section in bytes.
  /// The basedata section is where global variables, pointers to dynamically linked functions and other link data for
  /// proper execution are stored. This size might not be grown in multiples of WebAssembly linear memory page size, but
  /// could e.g. be grown byte-by-byte each time an offset in the linear memory is written to that is beyond the
  /// previously maximum written offset.
  ///
  /// This is not necessarily equal to the size of the allocation since the Runtime can keep track of which addresses
  /// are accessed and could abstract memory accesses. The size of the allocation where the job memory is stored can be
  /// larger than the currently active size.
  ///
  /// This function can be used to get the current RAM usage for a WebAssembly module. Any other memory consumption is
  /// due to the actual Runtime and RawModuleFunction objects.
  ///
  /// @return uint64_t Current size of the job memory in bytes
  uint64_t getMemoryUsage() const VB_NOEXCEPT;

  ///
  /// @brief Get the size of the current allocation. Growth logic for the allocation is defined by the ReallocFnc passed
  /// in the constructor of the runtime.
  ///
  /// @return uint32_t Size of the underlying allocation for the job memory in bytes
  inline uint32_t getAllocationSize() const VB_NOEXCEPT {
    return jobMemory_.size();
  }

  ///
  /// @brief Calls the ReallocFnc with minimumLength equal to the basedata size.
  /// CAUTION: Active data in the linear memory can get lost by this operation, but this call will not lead to segfaults
  /// or inherently unsafe behavior
  ///
  /// This function can be used to shrink the linear memory allocation if the WebAssembly module communicates to the C++
  /// context which portion of the linear memory it does not need anymore and which can therefore be discarded (and
  /// deallocated)
  ///
  void reallocShrinkToBasedataSize();

  ///
  /// @brief Calls the ReallocFnc with minimumLength equal to the current memory usage (basedata size + active portion
  /// of the linear memory)
  ///
  /// This can be used to shrink an allocation to the actually needed size (if more has been allocated) or to move the
  /// job memory somewhere else (e.g. in another statically allocated portion of memory during the process of
  /// defragmentation)
  ///
  void reallocShrinkToActiveSize();
#endif

#if INTERRUPTION_REQUEST
  ///
  /// @brief Request interruption of a WebAssembly function that is currently executing.
  ///
  /// This function can be called from another thread than the currently executing one and will set a flag that is
  /// checked whenever the function execution enters a basic block (e.g. at the start of a loop) and will then terminate
  /// execution by unwinding the stack and throwing a trap with the given trapCode (RUNTIME_INTERRUPT_REQUESTED by
  /// default) NOTE: TrapCode::NONE will not lead to a trap
  ///
  NO_THREAD_SANITIZE void requestInterruption(TrapCode const trapCode = TrapCode::RUNTIME_INTERRUPT_REQUESTED) const VB_NOEXCEPT;
#endif

#if BUILTIN_FUNCTIONS
  ///
  /// @brief Link memory so that it can be accessed (read-only) by the builtin functions (e.g. builtin
  /// getU8FromLinkedMemory) the compiler provides as intrinsics NOTE: If the memory gets deallocated and it is not
  /// unlinked, the WebAssembly module can still access it, possibly provoking a segfault
  ///
  /// This memory will stay linked until it is unlinked by calling unlinkMemory or by linking a memory with size 0 or
  /// base nullptr
  ///
  /// @param base Start of the memory that should be provided to the WebAssembly module
  /// @param length Size of the memory in bytes
  /// @return bool whether the memory was successfully linked. This can currently only fail on TriCore (here the length
  /// must be less than 1GB (2^30), be a multiple of 2 and the base must be aligned to 2 bytes)
  bool linkMemory(uint8_t const *base, uint32_t length) const VB_NOEXCEPT;

  ///
  /// @brief Unlink the linked memory so the WebAssembly module does not have access to it anymore.
  ///
  /// This is equivalent to calling linkMemory with size 0 or base nullptr
  ///
  void unlinkMemory() const VB_NOEXCEPT;

  /// @brief unlink trace buffer
  void clearTraceBuffer() VB_NOEXCEPT;

  /// @brief link trace buffer, the trace will be recorded in buffer
  void setTraceBuffer(Span<uint32_t> buffer) VB_NOEXCEPT;
#endif

  ///
  /// @brief Get the pointer to the trap implementation
  /// First argument passed to this argument must be the current base of the linear memory, second argument should be
  /// the TrapCode
  ///
  /// @return Pointer to the trap implementation (Takes pointer to linear memory base as first parameter and trapCode as
  /// second parameter)
  inline BinaryModule::TrapFncPtr getTrapFnc() const VB_NOEXCEPT {
    return binaryModule_.getTrapFnc();
  }

  ///
  /// @brief Get a pointer to the start of the linear memory
  /// CAUTION: This should not be used to read or write from the linear memory since the address might not be
  /// allocated/probed yet. Use getLinearMemoryRegion instead
  ///
  /// @return uint8_t* Pointer to the start of the linear memory (after the basedata) in the job memory
  uint8_t *unsafe__getLinearMemoryBase() const VB_NOEXCEPT {
    uint8_t *const memoryBase{getLinearMemoryBase()};
    return memoryBase;
  }

#if !LINEAR_MEMORY_BOUNDS_CHECKS
  ///
  /// @brief Calls the memory probe function
  ///
  /// This makes sure that the linear memory is properly allocated at least to the given offset
  ///
  /// @param offset Offset until which the linear memory should be probed
  /// @return bool Whether the probe was successful. If false it returned, memory could not be extended to the given
  /// offset.
  inline bool probeLinearMemory(uint32_t const offset) const VB_NOEXCEPT {
    bool const success{memoryProbeFunction_(offset)};
    updateLinearMemorySizeForDebugger();
    return success;
  }
  /// @brief Calls the memory shrink function
  ///
  /// Try to shrink linear memory to given minimal length
  ///
  /// @param minimumLength Minimal length that required by linear memory
  /// @return bool Whether the shrink was successful. If false it returned, memory could not be shrunken to the given
  /// minimal length.
  inline bool shrinkLinearMemory(uint32_t const minimumLength) const VB_NOEXCEPT {
    bool const success{memoryShrinkFunction_(minimumLength)};
    updateLinearMemorySizeForDebugger();
    return success;
  }
  using LandingPadFnc = void (*)(void); ///< Landing pad function type

  ///
  /// @brief Prepare a landing pad that can be used to leave a signal handler, call another function and then return to
  /// the WebAssembly execution NOTE: This should only be used from within signal handlers that are the result of
  /// executed instructions of the WebAssembly module
  ///
  /// This can be used as a workaround to call functions that are not signal safe from/after a signal handler and then
  /// continue execution of the WebAssembly module. The return address of the signal handler can be set to the landing
  /// pad function address.
  ///
  /// @param targetFnc The function that should be called from this landing pad
  /// @param originalReturnAddress The original return address where execution should return after the target function
  /// has finished
  /// @return LandingPadFnc Pointer to the landing pad that can then be "returned to" from a signal handler
  LandingPadFnc prepareLandingPad(void (*const targetFnc)(), void *const originalReturnAddress) const VB_NOEXCEPT;

  /// @brief Wrapper function to call memoryExtendFunction_
  /// @param size
  /// @return success or not
  inline bool extendMemory(uint32_t const size) const {
    return memoryExtendFunction_(size);
  }
#endif

#if ACTIVE_STACK_OVERFLOW_CHECK
  ///
  /// @brief Set the limit of the stack
  ///
  /// Once the stack has grown beyond this limit, a trap with code STACKFENCEBREACHED is thrown
  ///
  /// @param stackFence Limit until which the stack is allowed to grow during execution of the WebAssembly module
  /// @throws vb::RuntimeError if the stack fence is invalid value
  void setStackFence(void const *const stackFence) const;
#endif

  ///
  /// @brief Print the stacktrace or the current stacktrace records (up to maximum stacktrace record count set during
  /// compilation)
  ///
  /// The stacktrace records get reset every time a function is executed. The stacktrace can be printed after the
  /// WebAssembly has trapped or whenever the WebAssembly has an active function frame (e.g. during a host function
  /// call)
  ///
  /// @param logger logger where to print the stacktrace to
  void printStacktrace(ILogger &logger) const;

  ///
  /// @brief Retrieve a RawModuleFunction for the given name
  ///
  /// @param name Name under which the function has been exported
  /// @param signature Signature of RawModuleFunction, e.g. (ii)i.
  /// the default value should only be used in internal test
  /// @return RawModuleFunction The resulting RawModuleFunction that can be used to call a WebAssembly function of this
  /// module
  /// @throws vb::RuntimeError If the function with the given name does not exist
  RawModuleFunction getRawExportedFunctionByName(Span<char const> const &name, Span<char const> const &signature = Span<char const>()) const;

  ///
  /// @brief Check whether this module has an exported function for the given name
  ///
  /// @param name Name under which the function has been exported
  /// @param length Length of the name (SIZE_MAX means the length should be inferred by the null termination)
  /// @return bool Whether this WebAssembly module has a matching function
  bool hasExportedFunctionWithName(char const *const name, size_t const length = SIZE_MAX) const {
    checkIsReady(false);
    try {
      static_cast<void>(findExportedFunctionByName(name, length));
    } catch (vb::RuntimeError const &) {
      return false;
    }
    // GCOVR_EXCL_START
    catch (...) {
      UNREACHABLE(return false, "Should catch other exception than std::runtime_error");
    }
    // GCOVR_EXCL_STOP
    return true;
  }

  ///
  /// @brief Retrieve a ModuleFunction for a given name
  ///
  /// @tparam NumReturnValue number of return values of the WebAssembly function
  /// @tparam Arguments List of function parameter types of the WebAssembly function
  /// @param name Name under which the function has been exported
  /// @param length Length of the name (SIZE_MAX means the length should be inferred by the null termination)
  /// @return ModuleFunction<NumReturnValue, ArgumentTypes...> The resulting ModuleFunction that can be used to call a WebAssembly function
  /// of this module
  template <size_t NumReturnValue, typename... ArgumentTypes>
  ModuleFunction<NumReturnValue, ArgumentTypes...> getExportedFunctionByName(char const *const name, size_t const length = SIZE_MAX) const VB_THROW {
    checkIsReady();
    return ModuleFunction<NumReturnValue, ArgumentTypes...>(*this, findExportedFunctionByName(name, length));
  }

  ///
  /// @brief Retrieve a ModuleFunction from an exported table for the given index
  ///
  /// @tparam NumReturnValue number of return values of the WebAssembly function
  /// @tparam Arguments List of function parameter types of the WebAssembly function
  /// @param tableIndex Index in the table for which to retrieve the function
  /// @return ModuleFunction<NumReturnValue, ArgumentTypes...> The resulting ModuleFunction that can be used to call a WebAssembly function
  /// of this module
  template <size_t NumReturnValue, typename... ArgumentTypes>
  ModuleFunction<NumReturnValue, ArgumentTypes...> getFunctionByExportedTableIndex(uint32_t const tableIndex) const {
    checkIsReady();
    return ModuleFunction<NumReturnValue, ArgumentTypes...>(*this, findFunctionByExportedTableIndex(tableIndex));
  }

  ///
  /// @brief Retrieve a RawModuleFunction from an exported table for the given index
  ///
  /// @param tableIndex Index in the table for which to retrieve the function
  /// @param signature Signature of RawModuleFunction, e.g. (ii)i
  /// @return RawModuleFunction The resulting RawModuleFunction that can be used to call a WebAssembly function
  /// of this module
  RawModuleFunction getRawFunctionByExportedTableIndex(uint32_t const tableIndex, Span<char const> const &signature) const;

  ///
  /// @brief Retrieve a ModuleGlobal for a given name
  ///
  /// @tparam Type Type of the WebAssembly global to access
  /// @param name Name under which the global variable has been exported
  /// @param length Length of the name (SIZE_MAX means the length should be inferred by the null termination)
  /// @return ModuleGlobal<Type> The resulting ModuleGlobal that can be used to access the WebAssembly global variable
  /// @throw vb::RuntimeError If the global variable with the given name does not exist or has a different type
  template <class Type> ModuleGlobal<Type> getExportedGlobalByName(char const *const name, size_t const length = SIZE_MAX) const {
    checkIsReady();
    return ModuleGlobal<Type>(*this, findExportedGlobalByName(name, length));
  }

  ///
  /// @brief Iterate all recorded stacktrace entries, starting from the most recent one
  ///
  /// @param lambda Lambda which should be executed for every recorded stacktrace entry with the function index as
  /// parameter that represents the function in the entry
  void iterateStacktraceRecords(FunctionRef<void(uint32_t fncIndex)> const &lambda) const;

  ///
  /// @brief Shrink memory with a given minimumLength
  /// @param minimumLength The minimal linear memory length that wasm need
  /// CAUTION: Active data in the linear memory can get lost by this operation, but this call will not lead to segfaults
  /// or inherently unsafe behavior
  ///
  /// This function can be used to shrink the linear memory allocation if the WebAssembly module communicates to the C++
  /// context which portion of the linear memory it does not need anymore and which can therefore be discarded (and
  /// deallocated)
  ///
  void shrinkToSize(uint32_t const minimumLength);

  ///
  /// @brief If the WebAssembly module currently has an active frame/is currently executing: Unwind the stack andd abort
  /// the execution by throwing a trap with the given code NOTE: If the module has an active function frame/is currently
  /// executing, this function will not return, otherwise it does nothing
  ///
  /// This function can be called from a host unction that has been called from WebAssembly to immediately abort the
  /// execution. Note that anything still allocated within this host function will leak memory since this function will
  /// not return.
  ///
  void tryTrap(TrapCode const trapCode) const VB_NOEXCEPT;

  /// @brief get if the runtime has a valid BinaryModule
  inline bool hasBinaryModule() const VB_NOEXCEPT {
    return binaryModule_.getEndAddress() != nullptr;
  }

  ///@brief get the reference of binary module
  inline BinaryModule const &getBinaryModule() const VB_NOEXCEPT {
    return binaryModule_;
  }

private:
  ///
  /// @brief Whether this runtime is disabled (i.e. inactive)
  /// NOTE: Disabled/inactive runtimes can not call any functions
  ///
  bool disabled_;

  ///
  /// @brief Offset from the end of the executable binary of the body of the start function in the start section
  ///
  /// 0xFFFF'FFFF is used to indicate that there is no start function
  /// 0xFFFF'FFFE is used to indicate that the start function has already been executed
  ///
  uint32_t queuedStartFncOffset_;

#if LINEAR_MEMORY_BOUNDS_CHECKS
  ExtendableMemory jobMemory_; ///< ExtendableMemory representing the job memory where basedata (link data, globals,
                               ///< dynamic imports etc.) and the linear memory are stored
#else
  uint8_t *jobMemoryStart_; ///< Start of the job memory after which the basedata (link data, globals, dynamic imports
                            ///< etc.) and the linear memory are stored

  using MemoryExtendFncType = bool(uint32_t linearMemoryTotalPages); ///< Type of the allocator extend function
  using MemoryProbeFncType = bool(uint32_t address);                 ///< Type of the allocator probe function
  using MemoryShrinkFncType = bool(uint32_t address);                ///< Type of the allocator shrink function
  std::function<MemoryExtendFncType> memoryExtendFunction_;          ///< Allocator extend function
  std::function<MemoryProbeFncType> memoryProbeFunction_;            ///< Allocator probe function
  std::function<MemoryShrinkFncType> memoryShrinkFunction_;          ///< Allocator shrink function
  std::function<uint32_t(uint32_t)> memoryUsageFunction_;            ///< Allocator usage function
#endif
  BinaryModule binaryModule_; ///< binary module of current JIT code

  ///
  /// @brief Initialize the runtime
  ///
  /// This performs some checks, sets up the binaryModulePtr and calls initializeModule
  ///
  /// @param dynamicallyLinkedSymbols List of dynamically linked functions
  /// @param ctx Custom context for import functions
  void init(Span<NativeSymbol const> const &dynamicallyLinkedSymbols, void *const ctx);

  ///
  /// @brief This will set up the basedata, allocate memory for the job memory, insert default data into the linear
  /// memory and link dynamically imported functions
  ///
  /// @param dynamicallyLinkedSymbols  List of dynamically linked functions
  /// @param ctx Custom context for import functions
  /// @return uint32_t Offset from the end of the binary where the body of the start section function is located
  /// (0xFFFF'FFFF if this WebAssembly module does not have a start section)
  uint32_t initializeModule(Span<NativeSymbol const> const &dynamicallyLinkedSymbols, void *const ctx);

  ///
  /// @brief Check whether this runtime can be used to perform actions
  ///
  /// @param mustHaveStarted Whether this runtime must have already executed the start section to be considered ready
  /// @throws std::runtime_error If the runtime is disabled (e.g. after being moved) or the start function hasn't been
  /// called yet
  void checkIsReady(bool const mustHaveStarted = true) const;

#if INTERRUPTION_REQUEST
  ///
  /// @brief Reset the status flags (e.g. the interruption flag set by requestInterruption)
  ///
  inline void resetStatusFlags() const VB_NOEXCEPT {
    writeToPtr<uint32_t>(pSubI(getLinearMemoryBase(), Basedata::FromEnd::statusFlags), 0_U32);
  }
#endif

#if !LINEAR_MEMORY_BOUNDS_CHECKS
  ///
  /// @brief Update the linear memory size in job memory
  ///
  void updateLinearMemorySizeForDebugger() const VB_NOEXCEPT;
#endif

  ///
  /// @brief Find a WebAssembly function that is exported under the given name
  ///
  /// @param name Export name of the WebAssembly function
  /// @param nameLength Optional length of the name, if SIZE_MAX is passed, the length is inferred from the null
  /// termination
  /// @throws vb::RuntimeError If the function with the given name is not found
  /// @return uint32_t Offset from the end of the executable binary where the function data starts
  uint32_t findExportedFunctionByName(char const *const name, size_t nameLength = SIZE_MAX) const;

  ///
  /// @brief Find a WebAssembly function for a given index in an exported table
  ///
  /// @param tableIndex Index of the function in the exported table
  /// @return uint32_t Offset from the end of the executable binary where the function data starts
  uint32_t findFunctionByExportedTableIndex(uint32_t const tableIndex) const;

  ///
  /// @brief Find a WebAssembly global variable that is exported under the given name
  ///
  /// @param name Export name of the WebAssembly global variable
  /// @param nameLength Optional length of the name, if SIZE_MAX is passed, the length is inferred from the null
  /// termination
  /// @return uint32_t Offset from the end of the executable binary where the global variable data starts
  uint32_t findExportedGlobalByName(char const *const name, size_t nameLength = SIZE_MAX) const;

  ///
  /// @brief Get the initial formal size of the linear memory in multiples of WebAssembly page size (64kB)
  ///
  /// @return uint32_t Initial formal size of the linear memory in multiples of WebAssembly page size (64kB)
  uint32_t getInitialLinMemSizeInPages() const VB_NOEXCEPT;

  ///
  /// @brief Get the length of the basedata of the module
  ///
  /// @return uint32_t Length of the basedata section in bytes in the job memory
  inline uint32_t getBasedataLength() const VB_NOEXCEPT {
    uint32_t const linkDataLength{binaryModule_.getLinkDataLength()};
    return Basedata::length(linkDataLength, binaryModule_.getStacktraceEntryCount());
  }

  ///
  /// @brief Get the base of the job memory
  ///
  /// @return uint8_t* Pointer to the base of the job memory
  uint8_t *getMemoryBase() const VB_NOEXCEPT;

  ///
  /// @brief Get the base of the linear memory
  /// NOTE: This is equivalent to: getMemoryBase() + getBasedataLength()
  ///
  /// @return uint8_t* Pointer to the base of the linear memory
  uint8_t *getLinearMemoryBase() const VB_NOEXCEPT;

  ///
  /// @brief Get the end of the executable binary
  ///
  /// @return uint8_t* Pointer to the end of the executable binary
  inline uint8_t *getBinaryModulePtr() const VB_NOEXCEPT {
    return pRemoveConst(binaryModule_.getEndAddress());
  }

  ///
  /// @brief Update the reference to the runtime instance in the basedata which can be used by the MemoryHelper
  ///
  void updateRuntimeReference() const VB_NOEXCEPT;

  ///
  /// @brief Set the pointer to the memory helper in the basedata so the WebAssembly code can call it
  ///
  void setMemoryHelperPtr() const VB_NOEXCEPT;

  ///
  /// @brief Clear all recorded stacktrace entries
  ///
  void resetStacktraceAndDebugRecords() const VB_NOEXCEPT;

  ///
  /// @brief Reset the pointer for stack unwinds during traps
  ///
  void resetTrapInfo() const VB_NOEXCEPT;

  ///
  /// @brief Prepare the status of the runtime for execution of a WebAssembly function
  ///
  void prepareForFunctionCall() const VB_NOEXCEPT;

  ///
  /// @brief De-multiplex trapcodes
  ///
  /// TrapCodes can be multiplexed for performance reasons (e.g. only after the trap it is checked whether a memory has
  /// even been linked)
  ///
  /// @param trapCode TrapCode to demux
  /// @return TrapCode Effective (demuxed) TrapCode
  TrapCode demuxTrapCode(TrapCode const trapCode) const VB_NOEXCEPT;

  ///
  /// @brief Demux and throw a trapcode for a given TrapCode
  ///
  /// Will do nothing if TrapCode::NONE is passed
  ///
  /// @throw vb::TrapException If the TrapCode is not NONE, this function will throw a TrapException with the given TrapCode
  /// @param trapCode TrapCode which should be handled
  void handleTrapCode(TrapCode const trapCode) const;

  ///
  /// @brief Function signature for the wrapper functions that can be used to call actual WebAssembly function bodies
  /// from C++
  ///
  /// Pass pointer to serialized parameters (1st arg), pointer to the start of the linear memory (2nd arg), pointer
  /// to a trapCode variable (3rd arg), and pointer to serialized returnValues (4th arg). Parameters are serialized into a contiguous array of 8 byte
  /// elements irrespective of the parameter type
  ///
  using WasmWrapper = void (*)(void const *, uint8_t *, TrapCode *, void *const);

  ///
  /// @brief Invoke the C++ wrapper for a WebAssembly function
  ///
  /// @param fncPtr Pointer to the start of the function body
  /// @param serArgs Pointer to the serialized arguments
  /// @param linMemStart Pointer to the start of the linear memory
  /// @param results Pointer to the area where return values should be stored.
  void invokeWasmWrapperAndCheckTrap(WasmWrapper fncPtr, void const *const serArgs, uint8_t *const linMemStart, void *const results) const {
    TrapCode trapCode{TrapCode::NONE};
    fncPtr(serArgs, linMemStart, &trapCode, results);
    handleTrapCode(trapCode);
  }

  ///
  /// @brief Prepare the runtime for the function call and invoke the C++ wrapper for a WebAssembly function
  ///
  /// @param functionCallWrapper Raw pointer to the start of the function body
  /// @param serArgs Pointer to the serialized arguments
  /// @param results Pointer to the area where return values should be stored.
  /// @throws vb::TrapException If the function call leads to a trap
  void invokeWasmWrapper(uint8_t const *const functionCallWrapper, void const *const serArgs, void *const results) const {
    uint8_t *const nonConstFncPtr{pRemoveConst(functionCallWrapper)}; // NOLINT(cppcoreguidelines-pro-type-const-cast)
    WasmWrapper const fncPtr{pCast<WasmWrapper>(nonConstFncPtr)};
    prepareForFunctionCall();
    invokeWasmWrapperAndCheckTrap(fncPtr, serArgs, this->getLinearMemoryBase(), results);
  }

  template <typename T> friend class ModuleGlobal;                             ///< So ModuleGlobals can access the executable binary
  template <size_t NumReturnValue, typename... T> friend class ModuleFunction; ///< So ModuleFunctions can access the executable binary
  friend class RawModuleFunction;                                              ///< So RawModuleFunctions can access the executable binary
  friend MemoryHelper;                                                         ///< So the MemoryHelper can call the memory extend function
};

///
/// @brief Class that represents a reference to a global variable of the WebAssembly module
///
/// This can be used to read (and, if mutable, modify) the value of global variables
///
/// @tparam Type The type of the global variable
template <typename Type> class ModuleGlobal final {
  static_assert(std::is_arithmetic<Type>::value, "Global type must be arithmetic");

public:
  ///
  /// @brief Modifies the value of the global variable
  ///
  /// @param value
  void setValue(Type const value) const {
    writeToPtr<Type>(pRemoveConst(getPtr(true)), value); // NOLINT(cppcoreguidelines-pro-type-const-cast)
  }

  ///
  /// @brief Reads the value of the global variable
  ///
  /// @return Type Current value of the global variable
  Type getValue() const {
    return readFromPtr<Type>(getPtr(false));
  }

private:
  Runtime const *pRuntime_;     ///< Reference to the runtime instance
  uint32_t const binaryOffset_; ///< Offset from the end of the executable binary where the global data is stored

  ///
  /// @brief Construct a new instance of a ModuleGlobal
  ///
  /// @param runtime Reference to the runtime instance
  /// @param globOffset Offset from the end of the executable binary where the global data is stored
  ModuleGlobal(Runtime const &runtime, uint32_t const globOffset) : pRuntime_(&runtime), binaryOffset_(globOffset) {
    uint8_t const *stepPtr{pSubI(pRuntime_->getBinaryModulePtr(), binaryOffset_ + 2U)};
    SignatureType const signatureType = readNextValue<SignatureType>(&stepPtr);
    if (!ValidateSignatureType<Type>::validate(signatureType)) {
      throw RuntimeError(ErrorCode::Global_type_mismatch);
    }
  }

  ///
  /// @brief Get the raw pointer to the global variable
  ///
  /// @param willWrite Whether this pointer should be written to
  /// @return uint8_t const* Raw pointer to where the global variable is stored
  uint8_t const *getPtr(bool const willWrite) const {
    uint8_t const *stepPtr = pSubI(pRuntime_->getBinaryModulePtr(), binaryOffset_ + 3U);
    bool const isMutable = readNextValue<bool>(&stepPtr);

    if (!isMutable) {
      if (willWrite) {
        throw RuntimeError(ErrorCode::Global_is_immutable_and_cannot_be_written);
      }
      return pSubI(stepPtr, sizeof(Type));
    } else {
      uint32_t const linkDataOffset = readNextValue<uint32_t>(&stepPtr);
      return pAddI(pRuntime_->getMemoryBase(), Basedata::FromStart::linkData + linkDataOffset);
    }
  }

  friend Runtime; ///< So that only runtime can create ModuleGlobal instances
};

/// @brief union of all possible WASMTYPE
// coverity[autosar_cpp14_a11_0_1_violation]
union WasmValue {
  int32_t i32;  ///< i32
  uint32_t u32; ///< u32
  int64_t i64;  ///< i64
  uint64_t u64; ///< u64
  float f32;    ///< f32
  double f64;   ///< f64

  // coverity[autosar_cpp14_a12_1_5_violation]
  WasmValue() VB_NOEXCEPT : i32(0) {
  }
  ///
  /// @brief construct a WasmValue
  /// @param v int32_t value
  ///
  explicit WasmValue(int32_t const v) VB_NOEXCEPT : i32(v) {
  }
  ///
  /// @brief construct a WasmValue
  /// @param v uint32_t value
  ///
  explicit WasmValue(uint32_t const v) VB_NOEXCEPT : u32(v) {
  }
  ///
  /// @brief construct a WasmValue
  /// @param v int64_t value
  ///
  explicit WasmValue(int64_t const v) VB_NOEXCEPT : i64(v) {
  }
  ///
  /// @brief construct a WasmValue
  /// @param v uint64_t value
  ///
  explicit WasmValue(uint64_t const v) VB_NOEXCEPT : u64(v) {
  }
  ///
  /// @brief construct a WasmValue
  /// @param v float value
  ///
  explicit WasmValue(float const v) VB_NOEXCEPT : f32(v) {
  }
  ///
  /// @brief construct a WasmValue
  /// @param v double value
  ///
  explicit WasmValue(double const v) VB_NOEXCEPT : f64(v) {
  }

  ///
  /// @brief construct a WasmValue with enum underlying type
  /// @param v enum value
  ///
  template <class T, class = std::enable_if_t<std::is_enum<T>::value>>
  // coverity[autosar_cpp14_a7_1_8_violation]
  explicit WasmValue(T const v) VB_NOEXCEPT : WasmValue(static_cast<std::underlying_type_t<T>>(v)) {
  }
};

/// @brief function information in module
// coverity[autosar_cpp14_m3_4_1_violation]
class FunctionInfo final {
public:
  ///
  /// @brief constructor
  ///
  /// @param binaryModulePtr Pointer to the end of the executable binary
  /// @param binaryOffset Offset from the end of the executable binary where the function data is stored
  FunctionInfo(uint8_t const *const binaryModulePtr, uint32_t const binaryOffset) VB_NOEXCEPT;

  /// @brief Get the signature of the function
  /// @return Span<char const> Signature of the function
  Span<char const> signature() const VB_NOEXCEPT {
    return signature_;
  }

  /// @brief Get the pointer to the function
  /// @return uint8_t const* Pointer to the jit code of the exported function wrapper
  uint8_t const *fncPtr() const VB_NOEXCEPT {
    return fncPtr_;
  }

  ///
  /// @brief Validate the number of return values
  ///
  /// @throws RuntimeError if the number of return values does not match the expected one.
  template <size_t NumReturnValue> void validateNumReturnValue() const VB_THROW {
    SignatureType const paramEnd{getSignatureType(signature_.size() - NumReturnValue - 1U)};
    if (paramEnd != SignatureType::PARAMEND) {
      throw RuntimeError(ErrorCode::Function_signature_mismatch__signature_size_mismatch);
    }
  }

  /// @brief Dereference and validate the return value at the given index.
  /// @tparam Index The index of the return value.
  /// @tparam ExpectedTypes The expected types of the return values.
  /// @param ptr The pointer where return values should deref from.
  /// @param results A tuple where return values should deref to.
  /// @throws RuntimeError if the return value type does not match the expected type.
  template <size_t Index, typename... ExpectedTypes>
  typename std::enable_if<(Index < sizeof...(ExpectedTypes)), void>::type
  derefAndValidateReturnValueImpl(uint8_t const *ptr, std::tuple<ExpectedTypes...> &results) const VB_THROW {
    using T = typename std::tuple_element<Index, std::tuple<ExpectedTypes...>>::type;
    SignatureType const signatureType{getSignatureType(signature_.size() - sizeof...(ExpectedTypes) + Index)};
    if ((!ValidateSignatureType<T>::validate(signatureType))) {
      throw RuntimeError(ErrorCode::Function_signature_mismatch__wrong_return_type);
    }

    std::memcpy(&std::get<Index>(results), ptr + Index * sizeof(WasmValue), sizeof(T));
    derefAndValidateReturnValueImpl<Index + 1, ExpectedTypes...>(ptr, results);
  }

  /// @brief Dereference and validate the return value at the given index.
  /// @tparam Index The index of the return value.
  /// @tparam ExpectedTypes The expected types of the return values.
  /// @param ptr The pointer where return values should deref from.
  /// @param results A tuple where return values should deref to.
  template <size_t Index, typename... ExpectedTypes>
  typename std::enable_if<Index == sizeof...(ExpectedTypes), void>::type
  derefAndValidateReturnValueImpl(uint8_t const *ptr, std::tuple<ExpectedTypes...> &results) const VB_NOEXCEPT {
    static_cast<void>(ptr);
    static_cast<void>(results);
  }

  /// @brief Validates the parameter type against the expected signature.
  ///
  /// @throws VB_THROW if the validation fails.
  template <class... ParameterTypes> void validateParameterTypes() const VB_THROW {
    validateParameterImpl<0, ParameterTypes...>();
    SignatureType const type{getSignatureType<sizeof...(ParameterTypes) + 1U>()};
    if (type != SignatureType::PARAMEND) {
      throw RuntimeError(ErrorCode::Function_signature_mismatch__signature_size_mismatch);
    }
  }

  /// @brief Validates the signature against the expected signature.
  ///
  /// @throws VB_THROW if the validation fails.
  void validateSignatures(Span<char const> const &expectedSignature) const VB_THROW;

private:
  /// @brief Validates the parameter type at the given index.
  /// @tparam Index The index of the parameter to validate.
  /// @tparam T The expected type of the parameter.
  /// @tparam ExpectedTypes The expected types of the remaining parameter.
  /// @throws RuntimeError if the parameter type does not match the expected type.
  template <size_t Index, class T, class... ExpectedTypes> void validateParameterImpl() const VB_THROW {
    SignatureType const signatureType{getSignatureType<Index + 1U>()};
    if ((!ValidateSignatureType<T>::validate(signatureType))) {
      throw RuntimeError(ErrorCode::Function_signature_mismatch__wrong_parameter_type);
    }
    validateParameterImpl<Index + 1, ExpectedTypes...>();
  }

  /// @brief Validates the parameter type at the given index.
  /// @tparam Index The index of the parameter to validate.
  // coverity[autosar_cpp14_m0_1_8_violation]
  template <size_t Index> void validateParameterImpl() const VB_NOEXCEPT {
  }

  /// @brief Get the signature type at the given offset.
  /// @tparam Offset The offset of the signature type to retrieve.
  /// @return SignatureType The signature type at the given offset.
  template <size_t Offset> SignatureType getSignatureType() const VB_THROW {
    if (Offset >= signature_.size()) {
      throw RuntimeError(ErrorCode::Function_signature_mismatch__signature_size_mismatch);
    }
    return getSignatureTypeUnsafe(Offset);
  }

  /// @brief Get the signature type at the given offset.
  /// @param offset The offset of the signature type to retrieve.
  /// @return SignatureType The signature type at the given offset.
  SignatureType getSignatureType(size_t const offset) const VB_THROW {
    if (offset >= signature_.size()) {
      throw RuntimeError(ErrorCode::Function_signature_mismatch__signature_size_mismatch);
    }
    return getSignatureTypeUnsafe(offset);
  }

  /// @brief Get the signature type at the given offset.
  /// @tparam Offset The offset of the signature type to retrieve.
  /// @return SignatureType The signature type at the given offset.
  SignatureType getSignatureTypeUnsafe(size_t const offset) const VB_NOEXCEPT {
    // coverity[autosar_cpp14_a7_2_1_violation]
    return static_cast<SignatureType>(static_cast<uint8_t>(signature_[offset]));
  }

private:
  Span<char const> signature_; ///< Signature of the function
  uint8_t const *fncPtr_;      ///< Pointer to the jit code of the exported function wrapper
};

///
/// @brief Class that represents a raw callable reference to an exported function of the WebAssembly module
///
/// RawModuleFunction can be used if the arguments should be serialized manually or the signature is not known yet when
/// the function reference is retrieved
///
class RawModuleFunction final {
public:
  ///
  /// @brief Call the WebAssembly function
  ///
  /// @param serArgs Pointer to the raw serialized arguments
  /// @param results Pointer to the area where return values should be stored.
  void call(void const *const serArgs, void *const results) const {
    FunctionInfo const info{pRuntime_->getBinaryModulePtr(), binaryOffset_};
    return pRuntime_->invokeWasmWrapper(info.fncPtr(), serArgs, results);
  }

  ///
  /// @brief Get the signature of this function
  ///
  /// @return Signature of the function
  Span<char const> signature() const VB_NOEXCEPT {
    FunctionInfo const info{pRuntime_->getBinaryModulePtr(), binaryOffset_};
    return info.signature();
  }

  ///
  /// @brief Get the functionInfo of this function
  ///
  /// @return FunctionInfo
  FunctionInfo info() const VB_NOEXCEPT {
    return FunctionInfo(pRuntime_->getBinaryModulePtr(), binaryOffset_);
  }

  ///
  /// @brief Call the WebAssembly function
  /// NOTE: This is a shortcut for call()
  ///
  /// @param serializedArgs Pointer to the raw serialized arguments
  /// @param results Pointer to the area where return values should be stored.
  void operator()(uint8_t const *const serializedArgs, uint8_t *const results) const {
    return call(serializedArgs, results);
  }

  ///
  /// @brief Retrieve the reference to the runtime
  ///
  /// @return Runtime const& Reference to the runtime
  Runtime const &getRuntime() const VB_NOEXCEPT {
    return *pRuntime_;
  }

private:
  Runtime const *pRuntime_;     ///< Reference to the runtime instance
  uint32_t const binaryOffset_; ///< Offset from the end of the executable binary where the data for this function is stored

  friend Runtime; ///< So that only the runtime can create RawModuleFunction instances

  ///
  /// @brief Construct a new RawModuleFunction instance
  ///
  /// @param runtime Reference to the runtime
  /// @param fncOffset Offset from the end of the executable binary where the data for this function is stored
  inline RawModuleFunction(Runtime const &runtime, uint32_t const fncOffset) VB_NOEXCEPT : pRuntime_(&runtime), binaryOffset_(fncOffset) {
  }
};

///
/// @brief ModuleFunction class that can call WebAssembly functions
///
/// Templated function wrapper checking correct input arguments and return type at compile time
///
/// @tparam NumReturnValue number of return values of the WebAssembly function
/// @tparam Arguments List of function parameter types of the WebAssembly function
// coverity[autosar_cpp14_a14_1_1_violation]
template <size_t NumReturnValue, typename... ArgumentTypes> class ModuleFunction final {
public:
  ///
  /// @brief Calls the underlying WebAssembly function with the given arguments
  ///
  /// @param args Function arguments to pass to the WebAssembly function
  /// @return std::array<WasmValue, NumReturnValue> Array of the return values
  /// @throws vb::TrapException If the function call leads to a trap
  std::array<WasmValue, NumReturnValue> call(ArgumentTypes... args) const VB_THROW {
    FunctionInfo const info{pRuntime_->getBinaryModulePtr(), binaryOffset_};
    std::array<WasmValue const, sizeof...(ArgumentTypes)> const serializedArgs{make_args<ArgumentTypes...>(args...)};
    std::array<WasmValue, NumReturnValue> results{};
    pRuntime_->invokeWasmWrapper(info.fncPtr(), serializedArgs.data(), results.data());
    return results;
  }

  ///
  /// @brief Calls the underlying WebAssembly function with the given arguments
  ///
  /// This is a shortcut for call(...)
  ///
  /// @param args Function arguments to pass to the WebAssembly function
  /// @return std::array<WasmValue, NumReturnValue> Array of the return values
  inline std::array<WasmValue, NumReturnValue> operator()(ArgumentTypes... args) const VB_THROW {
    return call(args...);
  }

  ///
  /// @brief Retrieve the reference to the runtime
  ///
  /// @return Runtime const& Reference to the runtime
  inline Runtime const &getRuntime() const VB_NOEXCEPT {
    return *pRuntime_;
  }

  ///
  /// @brief Dereference return value
  /// @tparam ReturnValueTypes List of function return value types of the WebAssembly function
  /// @param ptr The pointer where return values should deref from.
  /// @return std::tuple<ReturnValueTypes...> dereferenced return values
  template <typename... ReturnValueTypes> std::tuple<ReturnValueTypes...> const derefReturnValues(WasmValue const *ptr) const {
    FunctionInfo const info{pRuntime_->getBinaryModulePtr(), binaryOffset_};
    std::tuple<ReturnValueTypes...> results{};
    info.derefAndValidateReturnValueImpl<0, ReturnValueTypes...>(pCast<uint8_t const *>(ptr), results);
    return results;
  }

private:
  Runtime const *pRuntime_;     ///< Reference to the runtime instance
  uint32_t const binaryOffset_; ///< Offset from the end of the executable binary where the data for this function is stored

  friend Runtime; ///< So that only the runtime can create ModuleFunction instances

  ///
  /// @brief Construct a new ModuleFunction instance
  ///
  /// @param runtime Reference to the runtime
  /// @param fncOffset Offset from the end of the executable binary where the data for this function is stored
  ModuleFunction(Runtime const &runtime, uint32_t const fncOffset) VB_THROW : pRuntime_(&runtime), binaryOffset_(fncOffset) {
    FunctionInfo const info{pRuntime_->getBinaryModulePtr(), binaryOffset_};
    info.validateParameterTypes<ArgumentTypes...>();
    info.validateNumReturnValue<NumReturnValue>();
  };

  ///
  /// @brief Creates an std::array from a list of elements as function arguments
  ///
  /// @tparam Arguments List of function parameter types of the WebAssembly function
  /// @return std::array<WasmValue, sizeof...(Arguments)> Array of WasmValue with the elements in place
  template <class... Arguments> constexpr auto make_args(Arguments... args) const VB_NOEXCEPT -> std::array<WasmValue const, sizeof...(Arguments)> {
    return {{WasmValue(args)...}};
  }
};

} // namespace vb

#endif /* RUNTIME_H */
