///
/// @file x86_64_cc.hpp
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
#ifndef X8664_CC_HPP
#define X8664_CC_HPP

#include <cstdint>

#include "x86_64_encoding.hpp"

#include "src/config.hpp"
#include "src/core/common/util.hpp"

#define COMMA ,

#if !LINEAR_MEMORY_BOUNDS_CHECKS
// coverity[autosar_cpp14_m16_0_6_violation]
#define JUST_NO_LINMEM_BOUNDS_CHECKS(X) X
#else
#define JUST_NO_LINMEM_BOUNDS_CHECKS(X)
#endif

namespace vb {
namespace x86_64 {

///
/// @brief Size of the automatic return address in bytes on the stack the CALL instruction pushes
///
constexpr uint32_t returnAddrWidth{8U};

namespace WasmABI {

/// @brief Number of register for GPR that will be reserved as "scratch" registers and can be used for various
/// calculations on the fly and to hold variables Those registers will be taken from the end of the gpr arrays
constexpr uint32_t resScratchRegsGPR{4U};

/// @brief Number of register for FPR that will be reserved as "scratch" registers and can be used for various
/// calculations on the fly and to hold variables Those registers will be taken from the end of the fpr arrays
constexpr uint32_t resScratchRegsFPR{4U};

/// @brief At most, regsForParams (N) parameters will be allocated in registers, the other parameters will be passed on
/// the stack
///
/// This also implicitly defines the calling convention that is used by the Wasm functions on the machine code level.
/// Any registers after that will also be used as scratch registers.
constexpr uint32_t regsForParams{4U};

/// @brief At most, gpRegsForReturnValues (N) return values will be allocated in general purpose registers, the other return values will be passed on
/// the stack
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr uint32_t gpRegsForReturnValues{2U};

/// @brief At most, fpRegsForReturnValues (N) return values will be allocated in floating point registers, the other return values will be passed on
/// the stack
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr uint32_t fpRegsForReturnValues{2U};

namespace REGS {

#if LINEAR_MEMORY_BOUNDS_CHECKS
/// @brief Cache of linear memory size in bytes (minus 8) for increased performance for memory bounds checks
constexpr REG memSize{REG::SI};
#endif

constexpr REG linMem{REG::B};  ///< Pointer to base of linear memory
constexpr REG trapReg{REG::A}; ///< Register for internal usage where the trap indicator will be passed
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr REG trapPosReg{REG::C}; ///< Register indicating where trap happened as position in bytecode (set only in debug mode)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr REG indirectCallReg{REG::D};        ///< Register for internal usage where the indirect call index will be passed
constexpr REG stacktraceCollectorRet{REG::D}; ///< Register for internal usage where the return address will be kept
                                              ///< while executing the stacktrace collector
// coverity[autosar_cpp14_a8_5_2_violation]
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr auto gpRetRegs = make_array(REG::A, REG::C); ///< General purpose return registers of Wasm functions
// coverity[autosar_cpp14_a8_5_2_violation]
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr auto fpRetRegs = make_array(REG::XMM0, REG::XMM1); ///< Floating point return registers of Wasm functions
constexpr REG moveHelper{REG::XMM15};                        ///< Helper register for memory->memory emitMove

} // namespace REGS

// The order is defined by the following rules:
// 1. Return value register (that should be the same as in the native ABI) should be among the reserved scratch
// registers
// 2. The parameter registers should be as congruent as possible with the native ABI parameter registers (here: R0-R7,
// F0-F7)
//    Since R0 and F0 should be allocated as reserved scratch regs according to rule 1, we replace it with some other
//    volatile register (according to native ABI)
// 3. The rest will simply be allocated in order (or otherwise arbitrarily)

// NOTE: in linux x18 is used as temporary register, see gcc\config\aarch64\aarch64.h, on other platforms it is reserved
// and should not be used

///
/// @brief Array of usable general purpose registers with no dedicated content (unlike SP, LR, memSize etc.)
///
// coverity[autosar_cpp14_a8_5_2_violation]
constexpr auto gpr =
    make_array(REG::BP, REG::DI, JUST_NO_LINMEM_BOUNDS_CHECKS(REG::SI COMMA) REG::R9, REG::R10, REG::R11, REG::R12, REG::R13, REG::R14,
               REG::R15,                         //
               REG::A, REG::D, REG::C, REG::R8); // <-- Last 4 reserved as scratch registers

///
/// @brief Array of usable floating point registers with no dedicated content
///
// coverity[autosar_cpp14_a8_5_2_violation]
constexpr auto fpr = make_array(REG::XMM4, REG::XMM5, REG::XMM6, REG::XMM7, REG::XMM8, REG::XMM9, REG::XMM10, REG::XMM11, REG::XMM12, REG::XMM13,
                                REG::XMM14,                                  //
                                REG::XMM0, REG::XMM1, REG::XMM2, REG::XMM3); // <-- Last 4 reserved as scratch registers
static_assert((fpr.size() <= UINT8_MAX) && (gpr.size() <= UINT8_MAX), "Array too long");

constexpr uint32_t numGPR{static_cast<uint32_t>(gpr.size())}; ///< Total number of GPRs available for allocation
constexpr uint32_t numFPR{static_cast<uint32_t>(fpr.size())}; ///< Total number of FPRs available for allocation

///
/// @brief Get the position in the gpr or fpr array for a register
///
/// @param reg Register to look up
/// @return Position of this register in the gpr or fpr array
uint32_t getRegPos(REG const reg) VB_NOEXCEPT;

///
/// @brief Check whether a register is a reserved scratch register
///
/// @param reg Register to check
/// @return true if register is a reserved scratch register, false otherwise
bool isResScratchReg(REG const reg) VB_NOEXCEPT;

} // namespace WasmABI

// Definition of the calling convention the native C++ code is using and corresponds with the AArch64 ABI
// This is necessary because we are going to call imported (native) C++ functions from Wasm code
// gpParams and fpParams define (in order) in which registers GP and FP parameters for function calls are passed
namespace NativeABI {

///
/// @brief Whether FPR and GPR share number of parameters passed in registers or not
///
/// If 4 params are passed as registers, this can mean that 4 GPR and 4 FPR can be used (SEPARATE) or that a total of 4
/// params are passed as registers (e.g. 1 GPR and 3 FPR) and the other parameters are passed on the stack
///
enum class RegArgAllocation : uint8_t { SEPARATE, MUTUAL };

///
/// @brief Whether stack-passed parameters are allocated left-to-right (LTR) or right-to-left (RTL)
///
enum class StackOrder : uint8_t { LTR, RTL };

#ifdef VB_POSIX
///
/// @brief General purpose registers for passing params in the native ABI
///
// coverity[autosar_cpp14_a8_5_2_violation]
constexpr auto gpParams = make_array(REG::DI, REG::SI, REG::D, REG::C, REG::R8, REG::R9);
///
/// @brief Floating point registers for passing params in the native ABI
///
// coverity[autosar_cpp14_a8_5_2_violation]
constexpr auto flParams = make_array(REG::XMM0, REG::XMM1, REG::XMM2, REG::XMM3, REG::XMM4, REG::XMM5, REG::XMM6, REG::XMM7);
///
/// @brief Nonvolatile registers in the native ABI
///
// coverity[autosar_cpp14_a8_5_2_violation]
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr auto nonvolRegs = make_array(REG::B, REG::BP, REG::R12, REG::R13, REG::R14, REG::R15);
///
/// @brief Volatile registers in the native ABI
///
// coverity[autosar_cpp14_a8_5_2_violation]
constexpr auto volRegs =
    make_array(REG::A, REG::C, REG::D, REG::DI, REG::SI, REG::R8, REG::R9, REG::R10, REG::R11, REG::XMM0, REG::XMM1, REG::XMM2, REG::XMM3, REG::XMM4,
               REG::XMM5, REG::XMM6, REG::XMM7, REG::XMM8, REG::XMM9, REG::XMM10, REG::XMM11, REG::XMM12, REG::XMM13, REG::XMM14, REG::XMM15);
/// @brief General purpose return register in the native ABI
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr REG gpRetReg{REG::A};
/// @brief Floating point return register in the native ABI
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr REG fpRetReg{REG::XMM0};

constexpr uint32_t shadowSpaceSize{0U}; ///< Shadow space size in bytes (only used on Windows)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr StackOrder stackOrder{StackOrder::RTL};                        ///< Stack parameter allocation order
constexpr RegArgAllocation regArgAllocation{RegArgAllocation::SEPARATE}; ///< Type of register parameter allocation
#elif defined VB_WIN32
///
/// @brief General purpose registers for passing params in the native ABI
///
constexpr auto gpParams = make_array(REG::C, REG::D, REG::R8, REG::R9);
///
/// @brief Floating point registers for passing params in the native ABI
///
constexpr auto flParams = make_array(REG::XMM0, REG::XMM1, REG::XMM2, REG::XMM3);
///
/// @brief Nonvolatile registers in the native ABI
///
constexpr auto nonvolRegs = make_array(REG::B, REG::BP, REG::DI, REG::SI, REG::R12, REG::R13, REG::R14, REG::R15, REG::XMM6, REG::XMM7, REG::XMM8,
                                       REG::XMM9, REG::XMM10, REG::XMM11, REG::XMM12, REG::XMM13, REG::XMM14, REG::XMM15);
///
/// @brief Volatile registers in the native ABI
///
constexpr auto volRegs =
    make_array(REG::A, REG::C, REG::D, REG::R8, REG::R9, REG::R10, REG::R11, REG::XMM0, REG::XMM1, REG::XMM2, REG::XMM3, REG::XMM4, REG::XMM5);
/// @brief General purpose return register in the native ABI
constexpr REG gpRetReg = REG::A;
/// @brief Floating point return register in the native ABI
constexpr REG fpRetReg = REG::XMM0;

constexpr uint32_t shadowSpaceSize = 32;                                ///< Shadow space size in bytes (only used on Windows)
constexpr StackOrder stackOrder = StackOrder::RTL;                      ///< Stack parameter allocation order
constexpr RegArgAllocation regArgAllocation = RegArgAllocation::MUTUAL; ///< Type of register parameter allocation
#else
#ifdef JIT_TARGET_X86_64
static_assert(false, "OS not supported");
#endif
#endif

#if (defined VB_POSIX) || (defined VB_WIN32)
static_assert((regArgAllocation != RegArgAllocation::MUTUAL) || (gpParams.size() == flParams.size()), "Mutual abi args mismatch");
#endif
///
/// @brief Check whether a register is a volatile register in the native ABI
///
/// @param reg Register to check
/// @return True if the register is volatile
bool isVolatileReg(REG const reg) VB_NOEXCEPT;

///
/// @brief Check whether a register can be a parameter in the native ABI
///
/// @param reg Register to check
/// @return True if the register can be a parameter
bool canBeParam(REG const reg) VB_NOEXCEPT;

///
/// @brief Get the position in the gpParams or fpRetReg array for a register
///
/// @param reg Register to look up
/// @return Position of this register in the gpr or fpr array. UINT8_MAX if the register is not a parameter
uint32_t getNativeParamPos(REG const reg) VB_NOEXCEPT;

} // namespace NativeABI

///
/// @brief List of registers that can be used as scratch registers during a function call
///
/// Can be used during indirect calls, imported calls and Wasm calls
/// These registers are never params (neither in WasmABI nor in NativeABI), return registers or indirect call index
/// registers and are thus never used during function calls
///
// coverity[autosar_cpp14_a8_5_2_violation]
constexpr auto callScrRegs = make_array(REG::R13, REG::R14, REG::R15);
static_assert(callScrRegs.size() >= 3, "Minimum 3 scratch registers needed for calls");

/// @brief Registers used for stacktrace collection during trap handling
namespace StackTrace {
/// @brief frameRefReg is used to traverse and read each frame's backtrace information.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr REG frameRefReg{callScrRegs[0]};
/// @brief counterReg is used as a loop counter during stacktrace collection.
/// It keeps track of how many frames remain to be collected.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr REG counterReg{callScrRegs[1]};
/// @brief scratchReg is a general-purpose scratch register for temporary data.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr REG scratchReg{callScrRegs[2]};
} // namespace StackTrace
} // namespace x86_64
} // namespace vb
#endif
