///
/// @file aarch64_backend.hpp
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
#ifndef AARCH64_BACKEND_HPP
#define AARCH64_BACKEND_HPP

#include <cassert>

#include "src/config.hpp"
#ifdef JIT_TARGET_AARCH64

#include <cstdint>

#include "aarch64_assembler.hpp"
#include "aarch64_cc.hpp"

#include "src/core/common/util.hpp"
#include "src/core/compiler/common/Common.hpp"
#include "src/core/compiler/common/MemWriter.hpp"
#include "src/core/compiler/common/RegMask.hpp"
#include "src/core/compiler/common/Stack.hpp"

namespace vb {

class Compiler;

namespace aarch64 {

class AArch64_Backend;

///
/// @brief Tracker object to keep track of allocated registers and stack bytes for arguments/parameters
class RegStackTracker final {
public:
  uint32_t allocatedGPR = 0U;        ///< Number of allocated general purpose registers
  uint32_t allocatedFPR = 0U;        ///< Number of allocated floating point registers
  uint32_t allocatedStackBytes = 0U; ///< Number of bytes allocated on the stack
};

///
/// @brief Temporary Register Manager
/// CAUTION: While this object is live, jobMem and memSize/landingPadHelper regs MUST NOT be used. They might be
/// clobbered and must be restored when the registers are not needed anymore by calling recoverTempGPRs
class TempRegManager final {
public:
  ///
  /// @brief Construct a new TempRegManager
  ///
  /// @param backend Reference to the AArch64 backend
  inline explicit TempRegManager(AArch64_Backend const &backend) VB_NOEXCEPT : clobberedExtraReg_(false),
                                                                               clobberedLinMemReg_(false),
                                                                               backend_(backend) {
  }

  ///
  /// @brief Get a temporary general purpose reg,
  ///
  /// @return REG
  REG getTempGPR() {
    if (!clobberedExtraReg_) {
      clobberedExtraReg_ = true;
#if LINEAR_MEMORY_BOUNDS_CHECKS
      return WasmABI::REGS::memSize;
#else
      return WasmABI::REGS::landingPadHelper;
#endif
    }

    // linMem hasn't been used yet, let's use it as a temporary register
    if (!clobberedLinMemReg_) {
      clobberedLinMemReg_ = true;
      return WasmABI::REGS::linMem;
    }
    UNREACHABLE(_, "Can only request two registers");
  }

  ///
  /// @brief Recover original values to temp GPRs
  void recoverTempGPRs();

private:
  bool clobberedExtraReg_;         ///< Whether the extra reg (landingPad or memSize reg) was clobbered and needs to be restored
  bool clobberedLinMemReg_;        ///< Whether linMem reg was clobbered and needs to be restored
  AArch64_Backend const &backend_; ///< Reference to backend
};

///
/// @brief AArch64 compiler backend class
///
class AArch64_Backend final {
  friend class CallBase;
  friend class DirectV2Import;
  friend class V1CallBase;
  friend class ImportCallV1;
  friend class InternalCall;

public:
  ///
  /// @brief Construct a new AArch64_Backend instance
  ///
  /// @param stack Reference to the compiler stack
  /// @param moduleInfo Reference to the WebAssembly module's moduleInfo
  /// @param memory Reference to the compiler memory
  /// @param output Reference to the output binary
  /// @param common Reference to the common instance
  /// @param compiler Reference to the compiler instance
  AArch64_Backend(Stack &stack, ModuleInfo &moduleInfo, MemWriter &memory, MemWriter &output, Common &common, Compiler &compiler) VB_NOEXCEPT;

  ///
  /// @brief Allocates reg for global. It should be called before compilation code section.
  ///
  /// @param type MachineType of the param or local variable
  REG allocateRegForGlobal(MachineType const type) VB_NOEXCEPT;

  ///
  /// @brief Allocates a parameter or local to an actual location in memory or in a register and stores its
  /// representation (LocalDef) in the compiler memory NOTE: All params have to be allocated before the non-param local
  /// variable and they must not be interleaved
  ///
  /// @param type MachineType of the param or local variable
  /// @param isParam Whether this represents a parameter (true) or a non-param local variable (false) NOTE: Once false
  /// is passed, true cannot be passed anymore for this function
  /// @param multiplicity Multiplicity of this local variable (How many params with these inputs should be allocated at
  /// once)
  void allocateLocal(MachineType const type, bool const isParam, uint32_t const multiplicity = 1U);

  ///
  /// @brief Opens the function context and starts the function body
  ///
  /// Should be called after all params and locals have been allocated via allocateLocal
  /// This will also go through previously emitted branches to that function and patch them, set up the index,
  /// initialize local variables and set up the stackframe
  ///
  void enteredFunction();

  ///
  /// @brief Produces a wrapper function that conforms to the native calling convention and calls a function defined or
  /// declared in the WebAssembly module with the given function index so that exported Wasm functions can be called
  /// from C++
  ///
  /// @param fncIndex Index of the Wasm function to call (Non-imported or imported function)
  void emitFunctionEntryPoint(uint32_t const fncIndex);

  ///
  /// @brief Produces a wrapper function that conforms to the WebAssembly calling convention, converts it to the native
  /// calling convention and calls the imported function at the given function index
  ///
  /// @param fncIndex Index of the imported function to call (NOTE: Must be an imported function)
  void emitWasmToNativeAdapter(uint32_t const fncIndex);

  /// @brief emit code snippet, which can get parameters from native ABI and put them to corresponding register
  /// according to wasm ABI.
  void emitNativeTrapAdapter() const;

  /// @brief emit code snippet, which can collect stack trace and store them in linear memory.
  /// @param stacktraceRecordCount max stack trace count
  /// pre: stacktraceRecordCount > 0
  void emitStackTraceCollector(uint32_t const stacktraceRecordCount) const;

  /// @brief emit code snippet, to unwind stack and back to trapHandler set at the function entry point.
  void emitTrapHandler() const;

#if !LINEAR_MEMORY_BOUNDS_CHECKS
  ///
  /// @brief Wrapper for a landing pad that saves the (native) volatile scratch registers, calls a given C++ function,
  /// restores the context and returns to the WebAssembly code
  ///
  /// The target address can be retrieved via Runtime::prepareLandingPad
  ///
  void emitLandingPad();
#endif

#if LINEAR_MEMORY_BOUNDS_CHECKS
  ///
  /// @brief Produces a wrapper function that enables an easy call of the extensionRequestHelper function from the
  /// WebAssembly context and handles potential reallocations and traps with the appropriate trapCode if reallocation
  /// failed
  ///
  /// The wrapper itself conforms to the native calling convention, but assumes all of the registers except the first
  /// two parameter registers are nonvolatile and will thus spill all nonvolatile registers so the (native)
  /// extensionRequestHelper function can be called This wrapper takes the maximally accessed address as first argument
  ///
  void emitExtensionRequestFunction();
#endif

  ///
  /// @brief Produces machine code for a function call
  /// Consumes all arguments for the function from the compiler stack and loads them according to the calling convention
  ///
  /// Can call either imported or non-imported WebAssembly functions
  ///
  /// @param fncIndex WebAssembly function index to call
  void execDirectFncCall(uint32_t const fncIndex);

  ///
  /// @brief Produces machine code for an indirect function call, consuming an I32 index from the compiler stack
  /// indexing onto a given table Consumes all arguments for the function from the compiler stack and loads them
  /// according to the calling convention
  ///
  /// Validates that the target conforms to the given signature index, traps otherwise or if the index is out of bounds
  /// Can call either imported or non-imported WebAssembly functions
  ///
  /// @param sigIndex Signature index of the function that will be called
  /// @param tableIndex Index of the WebAssembly table where the function is located
  void execIndirectWasmCall(uint32_t const sigIndex, uint32_t const tableIndex);

  ///
  /// @brief Produces machine code for a Wasm table branch instruction, consuming an I32 that indexes onto a vector of
  /// block elements for a return MachineType
  ///
  /// @param numBranchTargets Number of branch targets excluding the default branch target
  /// @param getNextTableBranchDepthLambda Lambda to retrieve the next branch target, can be called numBranchTargets + 1
  /// times
  void executeTableBranch(uint32_t const numBranchTargets, FunctionRef<Stack::iterator()> const &getNextTableBranchDepthLambda);

  ///
  /// @brief Produces machine code for a memory load instruction, while consuming the address from the compiler stack
  /// and pushes the load target back onto the stack
  ///
  /// @param opcode A memory load instruction
  /// @param offset Immediate offset of the memory load instruction
  /// @param addrElem Address element to load from
  /// @param targetHint Target hint
  /// @return StackElement Resulting StackElement where the result of this LOAD operation is stored
  StackElement executeLinearMemoryLoad(OPCode const opcode, uint32_t const offset, Stack::iterator const addrElem,
                                       StackElement const *const targetHint);

  ///
  /// @brief Produces machine code for a memory store instruction, while consuming the address and the data to store
  /// from the compiler stack
  ///
  /// @param opcode A memory store instruction
  /// @param offset Immediate offset of the memory store instruction
  void executeLinearMemoryStore(OPCode const opcode, uint32_t const offset);

  ///
  /// @brief Produces machine code for a memory copy instruction
  /// @pre dst src size should be should be i32.
  /// @post stack will be handled correctly.
  ///
  /// @param dst dest offset in linear memory
  /// @param src source offset in linear memory
  /// @param size copy length
  void executeLinearMemoryCopy(Stack::iterator const dst, Stack::iterator const src, Stack::iterator const size);

  ///
  /// @brief Produces machine code for a memory copy instruction
  /// @pre dst value size should be should be i32.
  /// @post stack will be handled correctly.
  ///
  /// @param dst dest offset in linear memory
  /// @param value value to fill in memory
  /// @param size copy length
  void executeLinearMemoryFill(Stack::iterator const dst, Stack::iterator const value, Stack::iterator const size);

  ///
  /// @brief Pushes the current Wasm memory size in pages to the compiler stack
  ///
  void executeGetMemSize() const;

  ///
  /// @brief Consumes an I32 element from the top of the stack, grows the memory according to Wasm's memory.grow
  /// instruction and pushes the old size as I32 to the stack
  ///
  void executeMemGrow();

  ///
  /// @brief Produces machine code that raises a trap when executed
  ///
  /// @param code Identifier of the reason for the trap
  void executeTrap(TrapCode const code) const;

  ///
  /// @brief emitMove variant of VariableStorage
  /// @see emitMove
  ///
  /// @param dstStorage Destination; where to move the source
  /// @param srcStorage Source; what to move
  /// @param unconditional Whether to even emit an instruction if the source and destination represent the same location
  /// @param presFlags Whether to forcefully preserve CPU flags (e.g. for usage between a comparison and a conditional branch)
  void emitMoveImpl(VariableStorage const &dstStorage, VariableStorage const &srcStorage, bool const unconditional,
                    bool const presFlags = false) const;

  ///
  /// @brief emitMove variant of int
  /// @see emitMove
  /// @param dstStorage Destination; where to move the source
  /// @param srcStorage Source; what to move
  /// @param unconditional Whether to even emit an instruction if the source and destination represent the same location
  /// @param presFlags Whether to forcefully preserve CPU flags (e.g. for usage between a comparison and a conditional branch)
  void emitMoveIntImpl(VariableStorage const &dstStorage, VariableStorage const &srcStorage, bool const unconditional,
                       bool const presFlags = false) const;

  ///
  /// @brief emitMove variant of float
  /// @see emitMove
  /// @param dstStorage Destination; where to move the source
  /// @param srcStorage Source; what to move
  /// @param unconditional Whether to even emit an instruction if the source and destination represent the same location
  /// @param presFlags Whether to forcefully preserve CPU flags (e.g. for usage between a comparison and a conditional branch)
  void emitMoveFloatImpl(VariableStorage const &dstStorage, VariableStorage const &srcStorage, bool const unconditional,
                         bool const presFlags = false) const;

  ///
  /// @brief Emits machine code of compare result for the given BranchCondition (BC) that is the result of a previous
  /// comparison NOTE: This does not modify StackElements on the stacks or truncate the stack
  ///
  /// This chooses either the given truthyResult or the falsyResult depending on the current CPU status flags and the
  /// given BranchCondition
  ///
  /// @param branchCond BranchCondition for which hto select the truthyResult
  /// @param targetHint A target hint that can be used as a scratch register or temporary variable location if the type
  /// matches
  /// @return StackElement Resulting StackElement where the result of this SELECT operation is stored
  StackElement emitCmpResult(BC const branchCond, StackElement const *const targetHint) const;

  ///@brief emit instruction for Wasm Opcode Select
  /// @param truthyResult Result that will be selected if the current CPU status flags match the given BranchCondition
  /// @param falsyResult Result that will be selected if the current CPU status flags do not match the given
  /// BranchCondition (or match the inverted one)
  /// @param condElem The condition for select instruction
  /// @param targetHint A target hint that can be used as a scratch register or temporary variable location if the type
  /// matches
  /// @return StackElement Resulting StackElement where the result of this SELECT operation is stored
  StackElement emitSelect(StackElement const &truthyResult, StackElement const &falsyResult, StackElement &condElem,
                          StackElement const *const targetHint);

  ///
  /// @brief Emits machine code for a WebAssembly comparison or other deferred action (except for SELECT)
  /// NOTE: This does not modify StackElements on the stacks or truncate the stack
  ///
  /// @param opcode WebAssembly instruction for which to emit machine code
  /// @param arg0Ptr First input operand for the instruction
  /// @param arg1Ptr Second input operand for the instruction (can be nullptr if the instruction is unary)
  /// @param targetHint A target hint that can be used as a scratch register or temporary variable location if the type
  /// matches
  /// @return StackElement Resulting StackElement where the result of this WebAssembly instruction is stored
  StackElement emitDeferredAction(OPCode const opcode, StackElement *const arg0Ptr, StackElement *const arg1Ptr,
                                  StackElement const *const targetHint);

  ///
  /// @brief Produces machine code for a comparison between two StackElements
  ///
  /// Uses instructions which are inherently non-commutative (CMP), but makes them commutative and
  /// returns whether the commutation ("reversion") was used
  ///
  /// @param opcode Wasm comparison opcode (e.g. OPCode::I32_EQZ) that describes the comparison to perform
  /// @param arg0Ptr Left argument for the comparison
  /// @param arg1Ptr Right argument for the comparison
  /// @return bool Whether the input arguments were swapped (must then adjust the condition code in the following
  /// conditional branch accordingly)
  bool emitComparison(OPCode const opcode, StackElement const *const arg0Ptr, StackElement const *const arg1Ptr);

  ///
  /// @brief Produces machine code for an (optionally conditional) branch that branches to a target block element either
  /// representing one of the block types (IFBLOCK, BLOCK, LOOP)
  ///
  /// @param targetBlockElem Target block element to branch to
  /// @param branchCond Condition when to branch (BC::UNCONDITIONAL for an unconditional branch)
  /// @param isNegative Whether the branchCondition should be negated before emitting the branch
  void emitBranch(StackElement *const targetBlockElem, BC const branchCond, bool const isNegative = false);

  ///
  /// @brief Emits a return instruction and properly unwinds the stack to the entry point of the function
  ///
  /// @param temporary Denotes whether this return is only performed conditionally (temporary = true; e.g. wrapped in a
  /// conditional branch) or unconditionally. This changes whether the moduleInfo's stackFrameSize is adjusted or not.
  void emitReturnAndUnwindStack(bool const temporary = false);

  ///
  /// @brief Return type of emitInstruction that contains a resulting StackElement (representation of variable location) and
  /// whether the instruction switched arguments if it is commutative
  ///
  class ActionResult final {
  public:
    /// @brief Resulting StackElement representing the location where the output of the instruction is placed
    StackElement element;
    /// @brief Whether the instructions input arguments were swapped (only possible if the input sources are set
    /// commutative)
    bool reversed = false;
  };

  /// @brief select and emit instruction
  /// @param instructions see @b AArch64_Backend::selectInstr
  /// @param arg0 first argument
  /// @param arg1 second argument, could be nullptr if unop
  /// @param targetHint see @b AArch64_Backend::selectInstr
  /// @param protRegs see @b AArch64_Backend::selectInstr
  /// @param presFlags see @b AArch64_Backend::selectInstr
  /// @return see @b AArch64_Backend::selectInstr
  ActionResult emitInstruction(Span<AbstrInstr const> const &instructions, StackElement const *const arg0, StackElement const *const arg1,
                               StackElement const *const targetHint, RegMask const protRegs, bool const presFlags) VB_THROW;

  /// @brief wrapper select and emit instruction
  /// @brief select and emit instruction
  /// @param instructions see @b emitInstruction
  /// @param arg0 see @b emitInstruction
  /// @param arg1 see @b emitInstruction
  /// @param targetHint see @b emitInstruction
  /// @param protRegs see @b emitInstruction
  /// @param presFlags see @b emitInstruction
  /// @return see @b emitInstruction
  template <size_t N>
  // coverity[autosar_cpp14_a8_4_7_violation]
  ActionResult emitInstruction(std::array<AbstrInstr const, N> const &instructions, StackElement const *const arg0, StackElement const *const arg1,
                               StackElement const *const targetHint, RegMask const protRegs, bool const presFlags) VB_THROW {
    return emitInstruction(Span<AbstrInstr const>{instructions.data(), instructions.size()}, arg0, arg1, targetHint, protRegs, presFlags);
  }

  ///
  /// @brief Finalizes all pending branches to a block and resolves them so they point to the current position in the
  /// output binary NOTE: This is only used for BLOCK and IFBLOCK elements since pending branches represent forward
  /// branches and branches to a LOOP block are inherently backwards branches. (cf. Wasm spec)
  ///
  /// @param blockElement Block (BLOCK, IFBLOCK) for which to resolve the pending branches
  void finalizeBlock(StackElement const *const blockElement);

  ///
  /// @brief Requests a target location where a source that should be spilled or temporarily moved somewhere else can be
  /// stored without overwriting anything else
  ///
  /// If there is a free scratch register, it will return this (as long as forceToStack is false), otherwise it will
  /// return a location on the execution stack where the data can be stored
  ///
  /// @param source Element for which to look for a new storage location
  /// @param protRegs Protected register mask describing the registers that must not be used
  /// @param forceToStack Whether the storage location should be forced to be one on the stack
  /// @param presFlags Whether to preserve the CPU status flags while potentially modifying the stack pointer to create
  /// a new storage location
  /// @return StackElement New spill target where the data can be moved
  StackElement reqSpillTarget(StackElement const &source, RegMask const protRegs, bool const forceToStack, bool const presFlags);

  ///
  /// @brief Spill all scratch registers, local variables and global variables currently referenced on the stack
  ///
  /// This should be called when a block (IF/BLOCK/LOOP) is entered, so that even those variables currently in use can
  /// be modified within the block
  ///
  /// @param below Spill elements on the stack below this iterator; NOTE: Must point to an actual element on the
  /// stack
  void spillAllVariables(Stack::iterator const below = Stack::iterator()) const;

#if INTERRUPTION_REQUEST
  ///
  /// @brief Produces machine code that checks whether interruption of the execution has been asynchronously requested
  /// and if so, traps
  ///
  /// @param scrReg Scratch register, must define a default because it's called by frontend
  void checkForInterruptionRequest(REG const scrReg = WasmABI::gpr[WasmABI::gpr.size() - 1U]) const;
#endif

  ///
  /// @brief Checks whether a StackElement represents a writable scratch register
  ///
  /// Writable scratch register are StackElements of type SCRATCHREGISTER that do not appear on the stack or only appear
  /// once
  ///
  /// @param pElem StackElement to check
  /// @return bool Whether this StackElement represents a writable scratch register
  bool isWritableScratchReg(StackElement const *const pElem) const VB_NOEXCEPT {
    return common_.isWritableScratchReg(pElem);
  }

  /// @brief see @b Common::spillFromStackImpl
  inline void spillFromStack(StackElement const &source, RegMask const protRegs, bool const forceToStack, bool const presFlags,
                             Stack::iterator const pExcludedZoneBottom = Stack::iterator(),
                             Stack::iterator const pExcludedZoneTop = Stack::iterator()) const {
    if (!moduleInfo_.getReferenceToLastOccurrenceOnStack(source).isEmpty()) {
      common_.spillFromStackImpl(source, protRegs, forceToStack, presFlags, pExcludedZoneBottom, pExcludedZoneTop);
    }
  }

  ///
  /// @brief Check if a given enforced target is only among the input operands and can thus be assumed to be writable
  /// without destroying any relevant/important information
  ///
  /// @param args argument list
  /// @param enforcedTarget Enforced target to compare
  /// @return bool Whether this enforced target is only among the (up to 2) input operands
  inline bool checkIfEnforcedTargetIsOnlyInArgs(Span<Stack::iterator> const &args, StackElement const *const enforcedTarget) const VB_NOEXCEPT {
    return common_.checkIfEnforcedTargetIsOnlyInArgs(args, enforcedTarget);
  }

#if BUILTIN_FUNCTIONS
  ///
  /// @brief Consumes the input valent blocks (or StackElements) from the compiler's stack and emits machine code that
  /// calls a BuiltinFunction with the input parameters NOTE: These functions are not part of the official WebAssembly
  /// specification or any other public convention but are rather provided by this specific implementation as an addon
  /// feature. WebAssembly modules can use builtin functions like they would use any other imported functions and will
  /// stay compliant to the specification.
  ///
  /// @param builtinFunction BuiltinFunction to emit machine code for
  void execBuiltinFncCall(BuiltinFunction const builtinFunction);
#endif

  ///
  /// @brief Get a candidate for register allocation
  ///
  /// @param type Which MachineType this register should be able to hold
  /// @param protRegs Protected register mask
  /// @return RegAllocCandidate Chosen ("best") candidate for register allocation, either representing an empty register
  /// (currentlyInUse = false) or a register that needs to be spilled before usage
  ///
  RegAllocCandidate getRegAllocCandidate(MachineType const type, RegMask const protRegs) const VB_NOEXCEPT;

  ///
  /// @brief Pre-reserve stack frame space for `Block`
  ///
  /// @param width Width in bytes of all the return values passed on the stack
  /// @return uint32_t maximum used stack frame position(include reserved space)
  ///
  uint32_t reserveStackFrame(uint32_t const width);

  ///
  /// @brief Creates a RegMask from an input StackElement
  ///
  /// @param elementPtr Pointer to the input StackElement
  /// @return Protected register mask where the input element's representation is forbidden
  inline RegMask mask(StackElement const *const elementPtr) const VB_NOEXCEPT {
    return moduleInfo_.maskForElement(elementPtr);
  }

  ///
  /// @brief Creates a RegMask from an input VariableStorage
  ///
  /// @param storage Pointer to the input VariableStorage
  /// @return Protected register mask where the input element's representation is forbidden
  inline RegMask mask(VariableStorage const &storage) const VB_NOEXCEPT {
    return (storage.type == StorageType::REGISTER) ? RegMask(storage.location.reg) : RegMask::none();
  }

  ///
  /// @brief Creates a RegMask from an input register
  ///
  /// @param reg Input register
  /// @param is64 Whether this is a 64-bit register. Useless, just to keep the signature consistent
  /// @return Protected register mask where the input register is forbidden
  inline RegMask mask(REG const reg, bool const is64 = false) const VB_NOEXCEPT {
    static_cast<void>(is64);
    return RegMask(reg);
  }

  ///
  /// @brief Get the offset of the current return value in the stack for the WASM ABI
  ///
  /// @param tracker Tracker object to keep track of how many return values have been allocated
  /// @param returnValueType Type of the return value
  /// @return uint32_t Offset in the stack return values
  static uint32_t offsetInStackReturnValues(RegStackTracker &tracker, MachineType const returnValueType) VB_NOEXCEPT;

  ///
  /// @brief Get the register of current return value
  ///
  /// @param returnValueType Which MachineType this return value represents
  /// @param tracker Tracker object to keep track of how many return values have been allocated
  /// @return Register in which a specific return value is passed
  REG getREGForReturnValue(MachineType const returnValueType, RegStackTracker &tracker) const VB_NOEXCEPT;

  ///
  /// @brief Instructions of aarch64 are all 4 bytes, no need to padding to 4Bytes
  ///
  /// @param paddingSize Size of padding
  void execPadding(uint32_t const paddingSize) const VB_NOEXCEPT;
  ///
  /// @brief Return the underlying register if the given element is suitable (matches dstType and is not protected by
  /// regMask)
  ///
  /// @param element To be checked element
  /// @param dstMachineType Dist Type
  /// @param regMask Forbidden register mask
  /// @return Underlying register of element or TReg::NONE if not suitable
  REG getUnderlyingRegIfSuitable(StackElement const *const element, MachineType const dstMachineType, RegMask const regMask) const VB_NOEXCEPT;

  /// @brief Check if there is enough scratch register for shift instruction.
  /// @param opcode instruction opcode
  /// @return Shift instruction will consume one scratch register for storing result.
  /// And minimally 2 scratch registers are needed for follow up condense. For example (select i32_reg, i32 const, i32 const)
  bool hasEnoughScratchRegForScheduleInstruction(OPCode const opcode) const VB_NOEXCEPT;

  ///
  /// @brief Iterate over all GPR and FPR scratch register and globals
  /// NOTE: Lambda will be called irrespective of whether they are currently in use
  ///
  /// @param lambda Function that will be called on a StackElement representing the respective scratch register or
  /// global
  void iterateScratchRegsAndGlobals(FunctionRef<void(StackElement const &)> const &lambda) const;

private:
  /// @brief Widths of certain entries on the stack
  struct Widths final {
    /// @brief Size of the stacktrace record entry on the stack
    static constexpr uint32_t stacktraceRecord{16U};
#if LINEAR_MEMORY_BOUNDS_CHECKS
    /// @brief Size of the cached job memory entry on the stack
    static constexpr uint32_t jobMemoryPtrPtr = 8U;
#else
    /// @brief Size of the cached job memory entry on the stack
    static constexpr uint32_t jobMemoryPtrPtr{0U};
#endif
    /// @brief Size of the debug info on the stack
    static constexpr uint32_t debugInfo{8U};
  };

  ///
  /// @brief Prepares a register holding the resulting address relative to start of linear memory for accessing data
  ///
  /// @param addrElem Input StackElement of type I32 representing the linear memory address
  /// @param offset Offset from the address to access
  /// @param regAllocTracker A tracker to track the protected register mask which prevents the registers which
  /// correspond to the set bits in the mask from being modified
  /// @param targetHint A target hint that can be used as a scratch register or temporary variable location if the type
  /// @return Register holding the resulting address relative to start of linear memory
  Common::LiftedReg prepareLinMemAddrProt(StackElement *const addrElem, uint32_t const offset, RegAllocTracker &regAllocTracker,
                                          StackElement const *const targetHint);

#if LINEAR_MEMORY_BOUNDS_CHECKS
  ///
  /// @brief Perform bounds checks for a register holding an address relative to start of linear memory for accessing
  /// data
  ///
  /// @param addrReg Register holding the address relative to start of linear memory
  /// @param memObjSize Size in bytes of the data that should be accessed, if reg points to the end (byte AFTER the
  /// data, e.g. for memcpy), pass 0 here
  void emitLinMemBoundsCheck(REG const addrReg, uint8_t const memObjSize);
#endif

  ///
  /// @brief Produces a wrapper function that conforms to the WebAssembly calling convention, converts it to the native
  /// calling convention and calls the imported function at the given function index
  ///
  /// @param fncIndex Index of the imported function to call (NOTE: Must be an imported function)
  void emitV1ImportAdapterImpl(uint32_t const fncIndex);
  ///
  /// @brief Produces a wrapper function that conforms to the WebAssembly calling convention, converts it to the native
  /// calling convention and calls the imported function at the given function index
  ///
  /// @param fncIndex Index of the imported function to call (NOTE: Must be an imported function)
  void emitV2ImportAdapterImpl(uint32_t const fncIndex) const;

  ///
  /// @brief Emits a memcpy without a bounds check from an arbitrary absolute address to another absolute address
  ///
  /// @param dstReg GPR holding the absolute destination address (will be written to)
  /// @param srcReg GPR holding the absolute source address (will be written to)
  /// @param sizeReg GPR holding the number of bytes to copy (will be written to)
  /// @param sizeToCopy Number of bytes to copy
  /// @param gpScratchReg GPR scratch register (will be written to)
  /// @param floatScratchReg First FPR scratch register (will be written to)
  /// @param floatScratchReg2 Second FPR scratch register (will be written to)
  /// @param canOverlap Whether the implementation should have a check for potentially overlapping regions. Set to true
  /// if there is a theoretical chance the regions can overlap. Undefined behavior otherwise. (Setting this to false
  /// makes it equivalent to memcpy, otherwise this is equivalent to memmove)
  void emitMemcpyWithConstSizeNoBoundsCheck(REG const dstReg, REG const srcReg, REG const sizeReg, uint32_t const sizeToCopy, REG const gpScratchReg,
                                            REG const floatScratchReg, REG const floatScratchReg2, bool const canOverlap) const;
  ///
  /// @brief Emits a memcpy without a bounds check from an arbitrary absolute address to another absolute address
  ///
  /// @param dstReg GPR holding the absolute destination address (will be written to)
  /// @param srcReg GPR holding the absolute source address (will be written to)
  /// @param sizeReg GPR holding the number of bytes to copy (will be written to)
  /// @param gpScratchReg GPR scratch register (will be written to)
  /// @param floatScratchReg First FPR scratch register (will be written to)
  /// @param floatScratchReg2 Second FPR scratch register (will be written to)
  /// @param canOverlap Whether the implementation should have a check for potentially overlapping regions. Set to true
  /// if there is a theoretical chance the regions can overlap. Undefined behavior otherwise. (Setting this to false
  /// makes it equivalent to memcpy, otherwise this is equivalent to memmove)
  void emitMemcpyNoBoundsCheck(REG const dstReg, REG const srcReg, REG const sizeReg, REG const gpScratchReg, REG const floatScratchReg,
                               REG const floatScratchReg2, bool const canOverlap) const;

  ///
  /// @brief Get the width in bytes of all the parameters that are passed on the stack for a function with a
  /// type/signature index
  ///
  /// @param sigIndex Signature or type index of the function
  /// @param imported Whether this function is an imported function or a function defined in the Wasm module
  /// @return uint32_t Width in bytes of all the parameters passed on the stack for this function signature
  uint32_t getStackParamWidth(uint32_t const sigIndex, bool const imported) const VB_NOEXCEPT;

  ///
  /// @brief Push a stacktrace entry to the stacktrace record stack
  ///
  /// Should be called before a new function is called
  ///
  /// @param fncIndex Function index that is called
  /// @param storeOffsetFromSP Offset from the current stack pointer where an element can be cached (Pointing to 4 free
  /// bytes)
  /// @param offsetToStartOfFrame Offset from the stacktrace entry to the start of the local variables on the stack
  /// @param bytecodePosOfLastParsedInstruction Position in the bytecode of the last parsed instruction
  /// @param scratchReg First scratch register that can be used by this function
  /// @param scratchReg2 Second scratch register that can be used by this function
  /// @param scratchReg3 Third scratch register that can be used by this function
  void tryPushStacktraceAndDebugEntry(uint32_t const fncIndex, SafeUInt<12U> const storeOffsetFromSP, uint32_t const offsetToStartOfFrame,
                                      uint32_t const bytecodePosOfLastParsedInstruction, REG const scratchReg, REG const scratchReg2,
                                      REG const scratchReg3) const;

  ///
  /// @brief Pop a stacktrace entry from the stacktrace record stack
  ///
  /// Should be called after a function returns
  ///
  /// @param storeOffsetFromSP Offset from the current stack pointer where the previously cached element is located (Put
  /// there via tryPushStacktraceAndDebugEntry)
  /// @param scratchReg First scratch register that can be used by this function
  void tryPopStacktraceAndDebugEntry(uint32_t const storeOffsetFromSP, REG const scratchReg) const;

  ///
  /// @brief Patch the function index of the last stacktrace entry if it is 0xFFFF'FFFF (This is the case after an
  /// indirect call)
  ///
  /// @param fncIndex Actual new function index
  /// @param scratchReg Scratch register that can be used by this function
  /// @param scratchReg2 Second scratch register that can be used by this function
  void tryPatchFncIndexOfLastStacktraceEntry(uint32_t const fncIndex, REG const scratchReg, REG const scratchReg2) const;

  ///
  /// @brief Get the offset of the current param in the stack for the calling convention of a specific function
  ///
  /// @param imported Whether this function is a host function (imported) or a Wasm function
  /// @param paramWidth Total width of the parameters passed on stack
  /// @param tracker Tracker object to keep track of how many arguments have been allocated
  /// @param paramType Type of the parameter
  /// @return uint32_t Offset in the stack args
  uint32_t offsetInStackArgs(bool const imported, uint32_t const paramWidth, RegStackTracker &tracker, MachineType const paramType) const VB_NOEXCEPT;

  ///
  /// @brief Get the width of a parameter if it is placed on the stack
  ///
  /// @param imported Whether this function is a host function (imported) or a Wasm function
  /// @param paramType Type of the parameter
  /// @return uint32_t Width in bytes
  static uint32_t widthInStackArgs(bool const imported, MachineType const paramType) VB_NOEXCEPT;

  ///
  /// @brief Get the register in which a specific argument for a function is passed
  ///
  /// @param paramType Which MachineType this argument represents
  /// @param imported Whether this function is a host function (imported) or a Wasm function
  /// @param tracker Tracker object to keep track of how many arguments have been allocated
  /// @return Register in which a specific argument is passed
  REG getREGForArg(MachineType const paramType, bool const imported, RegStackTracker &tracker) const VB_NOEXCEPT;

  ///
  /// @brief Emit the actual function call to a function declared in the WebAssembly module
  ///
  /// This will only emit the actual call sequence, while already expecting that the arguments have been loaded into the
  /// respective storage locations according to the calling convention
  ///
  /// @param fncIndex Function index to call (can be imported or within Wasm)
  /// @param linkRegister Whether to store the return address in the LR register (will execute a tail-call if
  /// linkRegister == false)
  void emitRawFunctionCall(uint32_t const fncIndex, bool const linkRegister = true);

  ///
  /// @brief Register a forward branch that cannot yet be resolved for later patching and store the reference in the
  /// linkVariable
  ///
  /// An arbitrary number of branches to the same future target can be stored by calling registerPendingBranch multiple
  /// times on the same linkVariable which will then produce a singly linked list where the linkVariable represents the
  /// entry point
  ///
  /// @param branchObj RelPatchObj representing a call or a branch instruction to a future target
  /// @param linkVariable Variable storing the entry of the linked list
  static void registerPendingBranch(RelPatchObj const &branchObj, uint32_t &linkVariable);

  ///
  /// @brief Finalizes all branches produced via registerPendingBranch
  ///
  /// @param linkVariable Entry to the linked list of branches, populated by registerPendingBranch
  void finalizeBranch(uint32_t const linkVariable) const;

  ///
  /// @brief Spill or restore an array of registers to the stack at a specific offset from the stack pointer
  ///
  /// @param regs Array of registers
  /// @param restore Whether it should be spilled or restored (restore = false means spill to stack)
  void spillRestoreRegsRaw(Span<REG const> const &regs, bool const restore = false) const;

  ///
  /// @brief Emit instruction sequence for truncating float to int Wasm instructions
  ///
  /// @param argPtr Argument
  /// @param targetHint Target hint
  /// @param isSigned Whether the target int is signed
  /// @param srcIs64 Whether the source float is 64-bit
  /// @param dstIs64 Whether the target int is 64-bit
  /// @return StackElement that contains the result
  StackElement emitInstrsTruncFloatToInt(StackElement *const argPtr, StackElement const *const targetHint, bool const isSigned, bool const srcIs64,
                                         bool const dstIs64);

  ///
  /// @brief Emit instruction sequence for copysign Wasm instructions
  ///
  /// @param arg0Ptr First argument
  /// @param arg1Ptr Second argument
  /// @param targetHint Target hint
  /// @param is64 Whether the operation is 64-bit
  /// @return StackElement that contains the result
  StackElement emitInstrsCopySign(StackElement *const arg0Ptr, StackElement *const arg1Ptr, StackElement const *const targetHint,
                                  bool const is64) const;

  ///
  /// @brief Emit instruction sequence for rotate Wasm instructions
  ///
  /// @param arg0Ptr First argument
  /// @param arg1Ptr Second argument
  /// @param targetHint Target hint
  /// @param is64 Whether the operation is 64-bit
  /// @param isLeft Whether the operation rotates to left
  /// @return StackElement that contains the result
  StackElement emitInstrsRot(StackElement *const arg0Ptr, StackElement *const arg1Ptr, StackElement const *const targetHint, bool const is64,
                             bool const isLeft);

  ///
  /// @brief Emit instruction sequence for popcnt Wasm instructions
  ///
  /// @param argPtr Argument
  /// @param targetHint Target hint
  /// @param is64 Whether the operation is 64-bit
  /// @return StackElement that contains the result
  StackElement emitInstrsPopcnt(StackElement *const argPtr, StackElement const *const targetHint, bool const is64) const;

  ///
  /// @brief Emit instruction sequence for division and remainder Wasm instructions
  ///
  /// @param arg0Ptr First argument
  /// @param arg1Ptr Second argument
  /// @param targetHint Target hint
  /// @param isSigned Whether the operation is signed
  /// @param is64 Whether the operation is 64-bit
  /// @param isDiv Whether the operation is a division
  /// @return StackElement that contains the result
  StackElement emitInstrsDivRem(StackElement *const arg0Ptr, StackElement *const arg1Ptr, StackElement const *const targetHint, bool const isSigned,
                                bool const is64, bool const isDiv) const;

  ///
  /// @brief Calculate jobMem reg based on linMem reg
  void setupJobMemRegFromLinMemReg() const;

  ///
  /// @brief Calculate linMem reg based on jobMem reg
  void setupLinMemRegFromJobMemReg() const;

#if LINEAR_MEMORY_BOUNDS_CHECKS
  ///
  /// @brief Load and calculate memSize reg from job memory
  void setupMemSizeReg() const;

  ///
  /// @brief Cache the pointer to the jobMemoryPtr on the stack so it can be restored after an imported call in case
  /// jobMemory was reallocated during the imported call
  ///
  /// @param spOffset Offset from SP at which to store the 8B pointer
  /// @param scrReg Scratch register this function can use
  void cacheJobMemoryPtrPtr(uint32_t const spOffset, REG const scrReg) const;

  ///
  /// @brief Restore the job memory registers from the cached pointer to the jobMemoryPtr that was placed on the stack
  /// via cacheJobMemoryPtrPtr
  ///
  /// @param spOffset Offset from SP at which the pointer from which it should be restored is stored
  void restoreFromJobMemoryPtrPtr(uint32_t const spOffset) const;
#endif

  ///
  /// @brief Return type of getMemRegDisp, representing a base register and a displacement

  template <size_t bitRange> class RegDisp final {
  public:
    REG reg;                 ///< Base register
    SafeUInt<bitRange> disp; ///< Displacement from the base register
  };

  ///
  /// @brief Resolve a given VariableStorage location to a register and an optional constant offset
  /// @tparam rangeBit Maximum displacement in bits the calling function can handle, larger offsets will be directly
  /// encoded in the returned register
  /// @param storage Input VariableStorage
  /// @param tempRegManager Temporary register manager to request a temporary GPR
  /// @return RegDisp that points to the memory location of the variable and can then be dereferenced
  template <size_t rangeBit> RegDisp<rangeBit> getMemRegDisp(VariableStorage const &storage, TempRegManager &tempRegManager) const VB_THROW {
    // Forward to the other getMemRegDisp with a default temporary register
    return getMemRegDisp<rangeBit>(storage, &tempRegManager, REG::NONE);
  }
  ///
  /// @brief Resolve a given VariableStorage location to a register and an optional constant offset
  /// @tparam rangeBit Maximum displacement in bits the calling function can handle, larger offsets will be directly
  /// encoded in the returned register
  /// @param storage Input VariableStorage
  /// @param tempGPR Temporary GPR
  /// @return RegDisp that points to the memory location of the variable and can then be dereferenced
  template <size_t rangeBit> RegDisp<rangeBit> getMemRegDisp(VariableStorage const &storage, REG const tempGPR) const VB_THROW {
    // Forward to the other getMemRegDisp with a default temporary register
    return getMemRegDisp<rangeBit>(storage, nullptr, tempGPR);
  }

  ///
  /// @brief Resolve a given VariableStorage location to a register and an optional constant offset
  /// @tparam rangeBit Maximum displacement in bits the calling function can handle, larger offsets will be directly
  /// encoded in the returned register
  /// @param storage Input VariableStorage
  /// @param tempRegManager Temporary register manager to request a temporary GPR
  /// @param tempGPR Temporary GPR
  /// @return RegDisp that points to the memory location of the variable and can then be dereferenced
  template <size_t rangeBit>
  RegDisp<rangeBit> getMemRegDisp(VariableStorage const &storage, TempRegManager *const tempRegManager, REG const tempGPR) const VB_THROW {
    assert((tempGPR != REG::NONE) || (tempRegManager != nullptr));

    REG returnReg{REG::NONE};
    uint64_t returnDisp{0U};
    if (storage.type == StorageType::LINKDATA) {
      // Bookkeeping memory is referenced through the R28 register, which points to the start of the jobMemory
      // We can thus calculate the displacement/offset to a variable in that region by adding the offset of the link
      // data and then the offset of the variable within the link data
      returnReg = WasmABI::REGS::jobMem;
      returnDisp = static_cast<uint64_t>(Basedata::FromStart::linkData) + storage.location.linkDataOffset;
    } else if (storage.type == StorageType::STACKMEMORY) {
      returnReg = REG::SP;
      static_assert(ImplementationLimits::maxStackFrameSize <= static_cast<uint32_t>(INT32_MAX), "Max stack frame size too");
      returnDisp =
          // coverity[autosar_cpp14_m5_0_9_violation]
          static_cast<uint64_t>(static_cast<int32_t>(moduleInfo_.fnc.stackFrameSize - storage.location.stackFramePosition));
    } else {
      // GCOVR_EXCL_START
      UNREACHABLE(_, "Unknown StorageType");
      // GCOVR_EXCL_STOP
    }

    UnsignedInRangeCheck<rangeBit> const checkResult{UnsignedInRangeCheck<rangeBit>::check(returnDisp)};
    // If the displacement is greater than the allowed displacement, we add it (or part of it) to a register
    if (!checkResult.inRange()) {
      // We always need a new register for this, because the base registers should not be overwritten, emitMove might
      // need two registers
      REG const tempReg{(tempGPR == REG::NONE) ? tempRegManager->getTempGPR() : tempGPR};

      // Check whether it fits if we null the 12 bytes from position 13-24 since we can use a single instruction for
      // this
      UnsignedInRangeCheck<rangeBit> const checkResult2{UnsignedInRangeCheck<rangeBit>::check(returnDisp & static_cast<uint64_t>(0xFF'00'0F'FFU))};
      if (checkResult2.inRange()) {
        as_.INSTR(ADD_xD_xN_imm12zxols12)
            .setD(tempReg)
            .setN(returnReg)
            .setImm12zxls12(SafeUInt<24U>::max() & (static_cast<uint32_t>(returnDisp) & static_cast<uint32_t>(0x00'FF'F0'00)))();
        return {tempReg, checkResult2.safeInt()};
      } else {
        // Otherwise we load the displacement to the register and add the returnReg to it
        as_.MOVimm64(tempReg, returnDisp);
        as_.INSTR(ADD_xD_xN_xMolsImm6).setD(tempReg).setN(tempReg).setM(returnReg)();
        return {tempReg, SafeUInt<rangeBit>::template fromConst<0U>()};
      }
    }
    return {returnReg, checkResult.safeInt()};
  }

  ///
  /// @brief Get the position in the gpr or fpr array for a register
  ///
  /// @param reg Register to look up
  /// @param import Whether the register is used in an imported function call (NativeABI) or in a Wasm function call (WasmABI)
  /// @return Position of this register in the gpr or fpr array. UINT8_MAX if the register is not a parameter
  uint32_t getParamPos(REG const reg, bool const import) const VB_NOEXCEPT;

  /// @brief Minimal number of registers that should be reserved for condense a vb.
  /// @details Need to keep 2 regs to avoid spill when add mem, mem or select reg, mem, mem.
  static constexpr uint32_t minimalNumRegsReservedForCondense{2U};

  Stack &stack_;           ///< Reference to the compiler stack
  ModuleInfo &moduleInfo_; ///< Reference to the ModuleInfo struct containing information about the WebAssembly module
  MemWriter &memory_;      ///< Reference to the compiler memory
  MemWriter &output_;      ///< Reference to the output binary
  Common &common_;         ///< Reference to the common instance
  Compiler &compiler_;     ///<  Reference to the compiler instance
  AArch64_Assembler as_;   ///< AArch64 assembler instance that emits instructions

  friend AArch64_Assembler; ///< So the assembler can access the compiler reference
  friend TempRegManager;    ///< Should have access to internal functions

  /// @brief The offset between the address where the trap code is stored with and REG::SP.
  static constexpr uint32_t of_trapCodePtr_trapReentryPoint{0U};

#if ENABLE_EXTENSIONS
  ///
  /// @brief Update the pressure histogram when a new register is allocated
  ///
  /// @param isGPR Whether the register is a general purpose register
  void updateRegPressureHistogram(bool const isGPR) const VB_NOEXCEPT;
#endif
};

} // namespace aarch64
} // namespace vb

#endif
#endif
