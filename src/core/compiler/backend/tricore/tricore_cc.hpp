///
/// @file tricore_cc.hpp
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
#ifndef TRICORE_CC_HPP
#define TRICORE_CC_HPP

#include "src/config.hpp"

#ifdef JIT_TARGET_TRICORE

#include "tricore_encoding.hpp"

namespace vb {
namespace tc {

///
/// @brief Size of the automatic return address in bytes on the stack the CALL instruction pushes
///
constexpr uint32_t returnAddrWidth{4U};

constexpr uint32_t stackAlignment{8U}; ///< Stack alignment in bytes

constexpr uint32_t stackAdjustAfterCall{stackAlignment - returnAddrWidth}; ///< Stack adjustment at entry of a function after a fcall

namespace WasmABI {

/// @brief Number of register for GPR that will be reserved as "scratch" registers and can be used for various
/// calculations on the fly and to hold variables Those registers will be taken from the end of the gpr array
constexpr uint32_t resScratchRegsGPR{6U};

/// @brief Number of register for FPR that will be reserved as "scratch" registers
/// NOTE: FPR not available on TriCore, therefore 0
constexpr uint32_t resScratchRegsFPR{0U};

/// @brief At most, regsForParams (N) registers will be allocated in registers, the other parameters will be passed on
/// the stack This also implicitly defines the calling convention that is used by the Wasm functions on the machine code
/// level. Any registers after that will also be used as scratch registers.
constexpr uint32_t regsForParams{7U};

/// @brief At most, gpRegsForReturnValues (N) return values will be allocated in general purpose registers, the other return values will be passed on
/// the stack
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr uint32_t gpRegsForReturnValues{2U};

/// @brief At most, fpRegsForReturnValues (N) return values will be allocated in floating point registers, the other return values will be passed on
/// the stack
/// NOTE: FPR not available on TriCore, therefore 0
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr uint32_t fpRegsForReturnValues{0U};

namespace REGS {

/// @brief Address scratch registers
// coverity[autosar_cpp14_a8_5_2_violation]
constexpr auto addrScrReg = make_array(REG::A12, REG::A13, REG::A7);
static_assert(RegUtil::canBeExtReg(addrScrReg[0]) && (RegUtil::getOtherExtAddrReg(addrScrReg[0]) == addrScrReg[1]),
              "address src reg will be used to load double regs");

constexpr REG linMem{REG::A2};  ///< Pointer to base of linear memory
constexpr REG memSize{REG::A3}; ///< Cache of linear memory size in bytes (minus 11) for increased performance for memory bounds checks
// coverity[autosar_cpp14_a8_5_2_violation]
constexpr auto sysGlobAddrRegs =
    make_array(REG::A0, REG::A1, REG::A8, REG::A9); ///< Global address register used by the system, will not be used by the compiler

constexpr REG cmpRes{REG::A14};     ///< Reserved register for holding boolean results of comparison instructions
constexpr REG memLdStReg{REG::A15}; ///< Reserved register for accesses onto linear memory, will contain the address

constexpr REG trapReg{REG::D0}; ///< Register for internal usage where the trap indicator will be passed
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr REG indirectCallReg{REG::D0}; ///< Register for internal usage where the indirect call index will be passed
// coverity[autosar_cpp14_a8_5_2_violation]
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr auto returnValueRegs = make_array(REG::D2, REG::D3); ///< Return registers

} // namespace REGS

///
/// @brief Array of usable data registers with no dedicated content
///
// coverity[autosar_cpp14_a8_5_2_violation]
constexpr auto dr = make_array(REG::D8, REG::D9, REG::D6, REG::D7, REG::D10, REG::D11, REG::D12, REG::D13, REG::D14, REG::D15, //
                               REG::D0, REG::D1, REG::D2, REG::D3, REG::D4, REG::D5); // <-- Last 6 reserved as scratch regs

static_assert(dr.size() <= UINT8_MAX, "Array too long");
static_assert(resScratchRegsGPR >= 6U, "Need a minimum of 6 scratch registers");

constexpr uint32_t numGPR{static_cast<uint32_t>(dr.size())}; ///< Total number of GPRs available for allocation
constexpr uint32_t numFPR{0_U32};                            ///< Total number of FPRs available for allocation

// coverity[autosar_cpp14_m3_4_1_violation]
constexpr uint32_t scratchRegStart{WasmABI::numGPR - WasmABI::resScratchRegsGPR}; ///< Start of the scratch registers in the dr array

///
/// @brief Get the position in the dr array for a register
///
/// @param dataReg Register to look up
/// @return Position of this register in dr array
uint32_t getRegPos(REG const dataReg) VB_NOEXCEPT;

///
/// @brief Check whether a register is a reserved scratch register
///
/// @param dataReg Register to check
/// @return true if register is a reserved scratch register, false otherwise
bool isResScratchReg(REG const dataReg) VB_NOEXCEPT;

} // namespace WasmABI

namespace NativeABI {

///
/// @brief Size of Upper Context or Lower Context, 16 * 4U
/// Upper Context = { PCXI, PSW, A[10:11], D[8:11], A[12:15], D[12:15] }
/// Lower Context = { PCXI, A[11], A[2:3], D[0:3], A[4:7], D[4:7] }
///
constexpr uint32_t contextRegisterSize{64U};

///
/// @brief Data registers for passing params in the native ABI
///
// coverity[autosar_cpp14_a8_5_2_violation]
constexpr auto paramRegs = make_array(REG::D4, REG::D5, REG::D6, REG::D7);

///
/// @brief Address registers for passing params in the native ABI
///
// coverity[autosar_cpp14_a8_5_2_violation]
constexpr auto addrParamRegs = make_array(REG::A4, REG::A5, REG::A6, REG::A7);

/// @brief General purpose return register in the native ABI
constexpr REG retReg{REG::D2}; ///< Return register for data (int, float ...) values
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr REG addrRetReg{REG::A2}; ///< Return register for address (pointers ...) values

///
/// @brief Check whether a register is a volatile register in the native ABI
///
/// @return True if the register is volatile
constexpr bool isVolatileReg(REG const /*reg*/) VB_NOEXCEPT {
  return true;
}

///
/// @brief Check whether a register can be a parameter in the native ABI
///
/// @param dataReg Register to check
/// @return True if the register can be a parameter
bool canBeParam(REG const dataReg) VB_NOEXCEPT;

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
/// NOTE: These registers are never params (neither in WasmABI nor in NativeABI), return registers or indirect call
/// index registers and are thus never used during function calls NOTE: First register must be extended register
///
// coverity[autosar_cpp14_a8_5_2_violation]
constexpr auto callScrRegs = make_array(REG::D14, REG::D15, REG::D1);
constexpr REG PreferredCallScrReg{callScrRegs[1]}; ///< recommended call scr reg use D15 can emit small JIT code
static_assert(callScrRegs.size() >= 3, "Minimum 3 scratch registers needed for calls");
static_assert(RegUtil::getOtherExtReg(callScrRegs[0]) == callScrRegs[1], "First two callScrRegs do not form an extended register");

/// @brief Registers used for stacktrace collection during trap handling
namespace StackTrace {
/// @brief targetReg points to the memory location where stacktrace records will be written.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr REG targetReg{WasmABI::REGS::addrScrReg[0]};
/// @brief frameRefReg is used to traverse and read each frame's backtrace information.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr REG frameRefReg{WasmABI::REGS::addrScrReg[1]};
/// @brief counterReg is used as a loop counter during stacktrace collection.
/// It keeps track of how many frames remain to be collected.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr REG counterReg{callScrRegs[1]};
/// @brief scratchReg is a general-purpose scratch register for temporary data.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr REG scratchReg{callScrRegs[2]};
} // namespace StackTrace
} // namespace tc
} // namespace vb

#endif
#endif
