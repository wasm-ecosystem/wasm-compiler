///
/// @file basedataoffsets.hpp
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
#ifndef BASEDATAOFFSETS_HPP
#define BASEDATAOFFSETS_HPP

#include <cstdint>

#include "src/config.hpp"
#include "src/core/common/util.hpp"

namespace Basedata {

namespace FromEnd {
#if INTERRUPTION_REQUEST
///
/// @brief How many bytes before the start of the linear memory the status flags are stored
///
constexpr int32_t statusFlags{8};
/// @brief Continuation 0
constexpr int32_t cont0{statusFlags};
#else
/// @brief Continuation 0
constexpr int32_t cont0 = 0;
#endif

#ifdef JIT_TARGET_TRICORE
#if TC_LINK_AUX_FNCS_DYNAMICALLY
///
/// @brief Pointer to an array of auxiliary host calls on TriCore. Only used if aux functions are to be linked
/// dynamically
///
constexpr int32_t arrDynSimpleFncCallsPtr{cont0 + 4};
///
/// @brief How many bytes before the start of the linear memory the PCX register for unwinding in case of a trap is
/// stored, use 8 instead of 4 (only 4 are needed) so everything is 8B aligned again
///
constexpr int32_t unwindPCXI{arrDynSimpleFncCallsPtr + 4};
#else
///
/// @brief How many bytes before the start of the linear memory the PCX register for unwinding in case of a trap is
/// stored, use 8 instead of 4 (only 4 are needed) so everything is 8B aligned again
///
constexpr int32_t unwindPCXI{cont0 + 8};
#endif

///
/// @brief How many bytes before the start of the linear memory the Wasm size in pages is stored
///
constexpr int32_t linMemWasmSize{unwindPCXI + 4};
#else
///
/// @brief How many bytes before the start of the linear memory the Wasm size in pages is stored
///
constexpr int32_t linMemWasmSize{cont0 + 4};
#endif
///
/// @brief How many bytes before the start of the linear memory the actual
///
constexpr int32_t actualLinMemByteSize{linMemWasmSize + 4};

#if (MAX_WASM_STACKSIZE_BEFORE_NATIVE_CALL != 0 && !ACTIVE_STACK_OVERFLOW_CHECK) ||                                                                  \
    (STACKSIZE_LEFT_BEFORE_NATIVE_CALL != 0 && ACTIVE_STACK_OVERFLOW_CHECK)
///
/// @brief How many bytes before the start of the linear memory the native stack fence is stored
///
constexpr int32_t nativeStackFence{actualLinMemByteSize + 8};
/// @brief Continuation 1
constexpr int32_t cont1{nativeStackFence};
#else
/// @brief Continuation 1
constexpr int32_t cont1 = actualLinMemByteSize;
#endif

#ifdef JIT_TARGET_TRICORE
/// @brief How many bytes before the start of the linear memory the jump target for traps is stored
constexpr int32_t trapHandlerPtr{cont1 + 4};
/// @brief How many bytes before the start of the linear memory the stack unwind target for traps is stored
constexpr int32_t trapStackReentry{trapHandlerPtr + 4};
#else
/// @brief How many bytes before the start of the linear memory the jump target for traps is stored
constexpr int32_t trapHandlerPtr{cont1 + 8};
/// @brief How many bytes before the start of the linear memory the stack unwind target for traps is stored
constexpr int32_t trapStackReentry{trapHandlerPtr + 8};
#endif

#if BUILTIN_FUNCTIONS
///
/// @brief How many bytes before the start of the linear memory the length of the linked memory is stored
///
constexpr int32_t linkedMemLen{trapStackReentry + 8};
///
/// @brief How many bytes before the start of the linear memory the pointer to the linked memory is stored
///
constexpr int32_t linkedMemPtr{linkedMemLen + 8};

/// @brief How many bytes before the start of the linear memory the pointer to the trace buffer is stored
constexpr int32_t traceBufferPtr{linkedMemPtr + 8};

/// @brief Continuation 2
constexpr int32_t cont2{traceBufferPtr};
#else
/// @brief Continuation 2
constexpr int32_t cont2 = trapStackReentry;
#endif

///
/// @brief How many bytes before the start of the linear memory a pointer to the Runtime is stored
///
constexpr int32_t runtimePtrOffset{cont2 + 8};

constexpr int32_t customCtxOffset{runtimePtrOffset + 8}; ///< Offset of the custom context pointer from the start of the job memory

///
/// @brief Size of a region in the basedata region that is reserved for spilling some registers
///
#ifdef JIT_TARGET_X86_64
constexpr int32_t spillSize{8};
#elif defined JIT_TARGET_AARCH64
constexpr int32_t spillSize{16};
#else
constexpr int32_t spillSize{0};
#endif

/// @brief How many bytes before the start of the linear memory a region of size spillSize begins where temporary data
/// can be stored
constexpr int32_t spillRegion{customCtxOffset + spillSize};

///
/// @brief How many bytes before the start of the linear memory a pointer to the Runtime is stored
///
constexpr int32_t jobMemoryDataPtrPtr{spillRegion + 8};
///
/// @brief How many bytes before the start of the linear memory a pointer to the memoryHelperPtr function (either memory
/// growth notifier or extension request helper) is stored
///
constexpr int32_t memoryHelperPtr{jobMemoryDataPtrPtr + 8};

#if ACTIVE_STACK_OVERFLOW_CHECK
///
/// @brief How many bytes before the start of the linear memory the stack fence for active stack overflow protection is
/// stored
///
constexpr int32_t stackFence{memoryHelperPtr + 8};
/// @brief Continuation 3
constexpr int32_t cont3{stackFence};
#else
/// @brief Continuation 3
constexpr int32_t cont3{memoryHelperPtr};
#endif

#if !LINEAR_MEMORY_BOUNDS_CHECKS
/// @brief Address where the landing pad should return to
constexpr int32_t landingPadRet{cont3 + 8};
/// @brief Target of the landing pad indirection
constexpr int32_t landingPadTarget{landingPadRet + 8};
/// @brief Continuation 4
constexpr int32_t cont4{landingPadTarget};
#else
constexpr int32_t cont4{cont3};
#endif

constexpr int32_t tableAddressOffset{cont4 + 8}; ///< store the address of wasm table

constexpr int32_t binaryModuleStartAddressOffset{tableAddressOffset + 8}; ///< store the address of binary module start

constexpr int32_t linkStatusAddressOffset{binaryModuleStartAddressOffset + 8}; ///< store the address of linked status

/// @brief Pointer to the last frame for stacktrace collection
constexpr int32_t lastFrameRefPtr{linkStatusAddressOffset + 8};

//*************************************End of location definitions*************************************************** */

static_assert((lastFrameRefPtr % 8) == 0, "Main portion of base data not 8B aligned");

/// @brief How many bytes before the start of the linear memory the base of the stacktrace array is located
constexpr int32_t getStacktraceArrayBase(uint32_t const stacktraceRecordCount) VB_NOEXCEPT {
  uint32_t const rawArrSize{static_cast<uint32_t>(stacktraceRecordCount) * static_cast<uint32_t>(sizeof(uint32_t))};
  uint32_t const paddedArrSize{vb::roundUpToPow2(rawArrSize, 3U)};
  return lastFrameRefPtr + static_cast<int32_t>(paddedArrSize);
}

/// @brief Last continuation
constexpr int32_t getLast(uint32_t const stacktraceRecordCount) VB_NOEXCEPT {
  if (stacktraceRecordCount > 0U) {
    return getStacktraceArrayBase(stacktraceRecordCount);
  } else {
    return lastFrameRefPtr;
  }
}
} // namespace FromEnd

namespace FromStart {
///
/// @brief Offset of the base of the link data (where global variables and pointers to imported variables and functions
/// are stored) from the start of the job memory
///
constexpr uint32_t linkData{8U};
} // namespace FromStart

///
/// @brief Calculate the length of the basedata for a link data length
///
/// @param linkDataLength Length of the link data
/// @param stacktraceRecordCount Number of stack trace entries to record
/// @return uint32_t Length of the basedata
constexpr uint32_t length(uint32_t const linkDataLength, uint32_t const stacktraceRecordCount) VB_NOEXCEPT {
  return (FromStart::linkData + linkDataLength) + static_cast<uint32_t>(FromEnd::getLast(stacktraceRecordCount));
}

} // namespace Basedata

#endif
