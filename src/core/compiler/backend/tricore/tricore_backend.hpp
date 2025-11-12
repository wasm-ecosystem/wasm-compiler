///
/// @file tricore_backend.hpp
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
#ifndef Tricore_BACKEND_HPP
#define Tricore_BACKEND_HPP

#include "src/config.hpp"
#include "src/core/compiler/common/MachineType.hpp"
#ifdef JIT_TARGET_TRICORE

#include "tricore_assembler.hpp"
#include "tricore_relpatchobj.hpp"

#include "src/core/common/basedataoffsets.hpp"
#include "src/core/common/implementationlimits.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/backend/tricore/tricore_aux.hpp"
#include "src/core/compiler/backend/tricore/tricore_cc.hpp"
#include "src/core/compiler/backend/tricore/tricore_encoding.hpp"
#include "src/core/compiler/common/Common.hpp"
#include "src/core/compiler/common/MemWriter.hpp"
#include "src/core/compiler/common/Stack.hpp"
#include "src/core/compiler/common/StackElement.hpp"
#include "src/core/compiler/common/StackType.hpp"
#include "src/core/compiler/common/VariableStorage.hpp"

namespace vb {

class Compiler;

namespace tc {

///
/// @brief Tracker object to keep track of allocated registers and stack bytes for arguments/parameters
class RegStackTracker final {
public:
  uint32_t allocatedDRs = 0U;        ///< Number of allocated data registers
  REG missedReg = REG::NONE;         ///< Register with a lower index than the chosen register that is still free after this
                                     ///< iteration, if this is REG::NONE, no register below the targetReg is free
  uint32_t allocatedStackBytes = 0U; ///< Number of bytes allocated on the stack
};

///
/// @brief tricore compiler backend class
///
class Tricore_Backend final {
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
  Tricore_Backend(Stack &stack, ModuleInfo &moduleInfo, MemWriter &memory, MemWriter &output, Common &common, Compiler &compiler) VB_NOEXCEPT;

  /// @brief wrapper for adapter DR in tricore backend and GPR/FPR in @b Common.
  /// In tricore, there are no gpr and fpr. Both integer and floating point number are store in data register(DR).
  inline uint32_t getNumStaticallyAllocatedDr() const VB_NOEXCEPT {
    return moduleInfo_.getNumStaticallyAllocatedGPRs();
  }

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

  /// @brief allocate reg for global
  /// @param type wasm type
  REG allocateRegForGlobal(MachineType const type) VB_NOEXCEPT;

  ///
  /// @brief Get the width in bytes of all the parameters that are passed on the stack for a function with a
  /// type/signature index
  ///
  /// @param sigIndex Signature or type index of the function
  /// @param imported Whether this function is an imported function or a function defined in the Wasm module
  /// @return uint32_t Width in bytes of all the parameters passed on the stack for this function signature
  uint32_t getStackParamWidth(uint32_t const sigIndex, bool const imported) const VB_NOEXCEPT;

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
  /// @brief Get the width of a parameter/return value if it is placed on the stack
  ///
  /// @param machineType Type of the parameter/return value
  /// @return uint32_t Width in bytes
  static uint32_t widthInStack(MachineType const machineType) VB_NOEXCEPT;

  ///
  /// @brief Emit the actual function call to a function declared in the WebAssembly module
  ///
  /// This will only emit the actual call sequence, while already expecting that the arguments have been loaded into the
  /// respective storage locations according to the calling convention
  ///
  /// @param fncIndex Function index to call (can be imported or within Wasm)
  void emitRawFunctionCall(uint32_t const fncIndex);

  ///
  /// @brief Get the register in which a specific argument for a function is passed
  ///
  /// @param paramType Which MachineType this argument represents
  /// @param imported Whether this function is a host function (imported) or a Wasm function
  /// @param tracker Tracker object to keep track of how many arguments have been allocated
  /// @return REG in which a specific register argument is passed
  REG getREGForArg(MachineType const paramType, bool const imported, RegStackTracker &tracker) const VB_NOEXCEPT;

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
  /// @brief Produces machine code for a memory copy instruction.
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
  /// @brief emitMove variant
  /// @see emitMove
  ///
  /// @param dstStorage Destination; where to move the source
  /// @param srcStorage Source; what to move
  /// @param unconditional Whether to even emit an instruction if the source and destination represent the same location
  /// @param presFlags Whether to forcefully preserve CPU flags (e.g. for usage between a comparison and a conditional branch)
  void emitMoveImpl(VariableStorage const &dstStorage, VariableStorage const &srcStorage, bool const unconditional,
                    bool const presFlags = false) const;

  ///@brief emit instruction for Wasm Opcode Select
  /// @param truthyResult Result that will be selected if the current CPU status flags match the given BranchCondition
  /// @param falsyResult Result that will be selected if the current CPU status flags do not match the given
  /// BranchCondition (or match the inverted one)
  /// @param condElem The condition for select instruction
  /// @param targetHint A target hint that can be used as a scratch register or temporary variable location if the type
  /// matches
  /// @return StackElement Resulting StackElement where the result of this SELECT operation is stored
  StackElement emitSelect(StackElement &truthyResult, StackElement &falsyResult, StackElement &condElem, StackElement const *const targetHint);

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
  /// @param targetHint targetHint
  /// @return RegElement holding the result of the comparison
  RegElement emitComparisonImpl(OPCode const opcode, StackElement const *const arg0Ptr, StackElement const *const arg1Ptr,
                                StackElement const *const targetHint = nullptr);

  ///
  /// @see emitComparisonImpl but load it into the standard WasmABI::REGS::cmpRes register and fulfill same signature as
  /// on other architectures
  bool emitComparison(OPCode const opcode, StackElement const *const arg0Ptr, StackElement const *const arg1Ptr);

  ///
  /// @brief Produces machine code for an (optionally conditional) branch that branches to a target block element either
  /// representing one of the block types (IFBLOCK, BLOCK, LOOP)
  ///
  /// @param targetBlockElem Target block element to branch to
  /// @param branchCond Condition when to branch (BC::UNCONDITIONAL for an unconditional branch)
  /// @param isNegative Whether the condition should be negated (NOTE: This inverts the condition and does not turn around
  /// the inequality operator)
  /// @throws ImplementationLimitationException Branch distance too large
  void emitBranch(StackElement *const targetBlockElem, BC const branchCond, bool const isNegative = false);

  ///
  /// @brief Emits a return instruction and properly unwinds the stack to the entry point of the function
  ///
  /// @param temporary Denotes whether this return is only performed conditionally (temporary = true; e.g. wrapped in a
  /// conditional branch) or unconditionally. This changes whether the moduleInfo's stackFrameSize is adjusted or not.
  void emitReturnAndUnwindStack(bool const temporary = false);

  ///
  /// @brief Finalizes all pending branches to a block and resolves them so they point to the current position in the
  /// output binary NOTE: This is only used for BLOCK and IFBLOCK elements since pending branches represent forward
  /// branches and branches to a LOOP block are inherently backwards branches. (cf. Wasm spec)
  ///
  /// @param blockElement Block (BLOCK, IFBLOCK) for which to resolve the pending branches
  /// @throws ImplementationLimitationException Branch distance too large
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
  void spillAllVariables(Stack::iterator const below = Stack::iterator());

#if INTERRUPTION_REQUEST
  ///
  /// @brief Produces machine code that checks whether interruption of the execution has been asynchronously requested
  /// and if so, traps
  ///
  /// @param scrReg Scratch register, must define a default because it's called by frontend
  void checkForInterruptionRequest(REG const scrReg = WasmABI::dr[WasmABI::dr.size() - 1U]) const;
#endif

  ///
  /// @brief Checks whether a register is holding a local variable
  ///
  /// @param reg Given register
  /// @return boolean
  ///
  inline bool isStaticallyAllocatedReg(REG const reg) const VB_NOEXCEPT {
    uint32_t const testRegPos{WasmABI::getRegPos(reg)};
    return testRegPos < getNumStaticallyAllocatedDr();
  }

  ///
  /// @brief Checks whether a StackElement represents a writable scratch register
  ///
  /// Writable scratch register are StackElements of type SCRATCHREGISTER that do not appear on the stack or only appear
  /// once
  ///
  /// @param pElem StackElement to check
  /// @return bool Whether this StackElement represents a writable scratch register
  bool isWritableScratchReg(StackElement const *const pElem) const VB_NOEXCEPT;

  /// @brief see @b Common::spillFromStackImpl
  void spillFromStack(StackElement const &source, RegMask protRegs, bool const forceToStack, bool const presFlags,
                      Stack::iterator const pExcludedZoneBottom = Stack::iterator(), Stack::iterator const pExcludedZoneTop = Stack::iterator());

  ///
  /// @brief Check if a given enforced target is only among the input operands and can thus be assumed to be writable
  /// without destroying any relevant/important information
  ///
  /// @param args argument list
  /// @param enforcedTarget Enforced target to compare
  /// @return bool Whether this enforced target is only among the (up to 2) input operands
  bool checkIfEnforcedTargetIsOnlyInArgs(Span<Stack::iterator> const &args, StackElement const *const enforcedTarget) const VB_NOEXCEPT;

  ///
  /// @brief Iterate over all GPR and FPR scratch register and globals
  /// NOTE: Lambda will be called irrespective of whether they are currently in use
  ///
  /// @param lambda Function that will be called on a StackElement representing the respective scratch register or
  /// global
  void iterateScratchRegsAndGlobals(FunctionRef<void(StackElement const &)> const &lambda) const;

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
  /// @return RegMask Protected register mask where the input element's representation is protected
  RegMask mask(StackElement const *const elementPtr) const VB_NOEXCEPT;

  ///
  /// @brief Creates a RegMask from an input VariableStorage
  ///
  /// @param storage Reference to the input VariableStorage
  /// @return RegMask Protected register mask where the input element's representation is protected
  RegMask mask(VariableStorage const &storage) const VB_NOEXCEPT;

  ///
  /// @brief Creates a RegMask from an input register
  ///
  /// @param reg Input register
  /// @param is64 Whether this is a 64-bit (extended) register (pair)
  /// @return RegMask Protected register mask where the input register is protected
  RegMask mask(REG const reg, bool const is64) const VB_NOEXCEPT;

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
  /// @brief Pad the output binary. Tricore has 2Bytes instructions, need to align to 4Bytes
  ///
  /// @param paddingSize Size of padding
  void execPadding(uint32_t const paddingSize);

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
  /// @brief Check if D15 is available
  /// @return bool
  bool isD15Available() const VB_NOEXCEPT {
    bool const isD15ForLocal{moduleInfo_.fnc.numLocalsInGPR == moduleInfo_.getMaxNumsLocalsInGPRs()};
    StackElement const d15{StackElement::scratchReg(REG::D15, MachineTypeUtil::toStackTypeFlag(MachineType::I32))};
    bool const isD15Writable{isWritableScratchReg(&d15)};
    return (!isD15ForLocal && isD15Writable);
  }

private:
  /// @brief Widths of certain entries on the stack
  struct Widths final {
    /// @brief Size of the stacktrace record entry on the stack
    static constexpr uint32_t stacktraceRecord{8U};
    /// @brief Size of the cached job memory entry on the stack
    static constexpr uint32_t jobMemoryPtrPtr{4U};
  };

  ///
  /// @brief Prepares WasmABI::REGS::memLdStReg so it holds the resulting address (of end of data, so byte after)
  /// relative to start of linear memory for accessing data
  /// @note Assumes relative address of start of data is already in WasmABI::REGS::memLdStReg
  /// CAUTION: Call emitLinMemBoundsCheck right after, the instruction represented by RelPatchObj can only target a 32
  /// byte positive displacement!
  ///
  /// @param tempDReg Data scratch register
  /// @param addressDReg If relative address is already in a data register too, can be passed here as optimization
  /// (readonly)
  /// @param offset Offset from the address to access
  /// @param memObjSize Size in bytes of the data that should be accessed, if reg points to the end (byte AFTER the
  /// data, e.g. for memcpy), pass 0 here
  /// @return RelPatchObj that should be linked to a direct linear memory error so it can trap efficiently (e.g. pass to
  /// emitLinMemBoundsCheck)
  RelPatchObj prepareLinMemAddr(REG const tempDReg, REG addressDReg, uint32_t const offset, uint8_t const memObjSize) const;

  ///
  /// @brief Perform bounds checks for a register holding an address relative to start of linear memory for accessing
  /// data
  /// @note Assumes relative address of end of data (byte after) from start of linear memory is in
  /// WasmABI::REGS::memLdStReg
  ///
  /// @param tempDReg Data scratch register
  /// @param toExtensionRequest Optional RelPatchObj that should be linked to calling the extension request function
  /// (normally return of prepareLinMemAddr if this was done before)
  void emitLinMemBoundsCheck(REG const tempDReg, RelPatchObj const *const toExtensionRequest = nullptr) const;

  ///
  /// @brief Push a stacktrace entry to the stacktrace record stack
  ///
  /// Should be called when a new function is called
  ///
  /// @param fncIndex Function index that is called
  /// @param storeOffsetFromSP Offset from the current stack pointer where an element can be cached (Pointing to 4 free
  /// bytes)
  /// @param addrScrReg Address scratch register that can be used by this function
  /// @param scratchReg First scratch register that can be used by this function
  /// @param scratchReg2 Second scratch register that can be used by this function
  void tryPushStacktraceEntry(uint32_t const fncIndex, uint32_t const storeOffsetFromSP, REG const addrScrReg, REG const scratchReg,
                              REG const scratchReg2) const;

  ///
  /// @brief Pop a stacktrace entry from the stacktrace record stack
  ///
  /// Should be called when a function returns
  ///
  /// @param storeOffsetFromSP Offset from the current stack pointer where the previously cached element is located (Put
  /// there via tryPushStacktraceEntry)
  /// @param scratchReg Second scratch register that can be used by this function
  void tryPopStacktraceEntry(uint32_t const storeOffsetFromSP, REG const scratchReg) const;

  ///
  /// @brief Patch the function index of the last stacktrace entry if it is 0xFFFF'FFFF (This is the case after an
  /// indirect call)
  ///
  /// @param fncIndex Actual new function index
  /// @param addrScrReg Address scratch register that can be used by this function
  /// @param scratchReg Scratch register that can be used by this function
  void tryPatchFncIndexOfLastStacktraceEntry(uint32_t const fncIndex, REG const addrScrReg, REG const scratchReg) const;
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

  ///
  /// @brief Perform a simple native call of a C/C++ function, used for calling the softfloat implementations
  /// @note Can only be used to call functions with up to two integer or float inputs (32-bit or 64-bit) and integer or
  /// float returns (32-bit or 64-bit)
  ///
  /// @param destReg Register to put the result of the function call in
  /// @param destIs64 Whether the return value is a 64-bit value
  /// @param arg0Reg Register the first argument for the call is currently stored in
  /// @param arg0Is64 Whether the first argument is a 64-bit value
  /// @param arg1Reg Register the second argument for the call is currently stored in
  /// @param arg1Is64 Whether the second argument is a 64-bit value
  /// @param mappedFnc MappedFnc to call
  ///
  void simpleNativeFncCall(REG const destReg, bool const destIs64, REG const arg0Reg, bool const arg0Is64, REG const arg1Reg, bool const arg1Is64,
                           aux::MappedFncs const mappedFnc) const;

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
  /// @param dstReg address register holding the absolute destination address (will be written to)
  /// @param srcReg address register holding the absolute source address (will be written to)
  /// @param sizeReg data register holding the number of bytes to copy (will be written to)
  /// @param sizeToCopy Number of bytes to copy
  /// @param scratchReg data scratch register (will be written to)
  /// @param canOverlap Whether the implementation should have a check for potentially overlapping regions. Set to true
  /// if there is a theoretical chance the regions can overlap. Undefined behavior otherwise. (Setting this to false
  /// makes it equivalent to memcpy, otherwise this is equivalent to memmove)
  void emitMemcpyWithConstSizeNoBoundsCheck(REG const dstReg, REG const srcReg, REG const sizeReg, uint32_t const sizeToCopy, REG const scratchReg,
                                            bool const canOverlap) const;
  ///
  /// @brief Emits a memcpy without a bounds check from an arbitrary absolute address to another absolute address
  ///
  /// @param dstReg address register holding the absolute destination address (will be written to)
  /// @param srcReg address register holding the absolute source address (will be written to)
  /// @param sizeReg data register holding the number of bytes to copy (will be written to)
  /// @param scratchReg data scratch register (will be written to)
  /// @param canOverlap Whether the implementation should have a check for potentially overlapping regions. Set to true
  /// if there is a theoretical chance the regions can overlap. Undefined behavior otherwise. (Setting this to false
  /// makes it equivalent to memcpy, otherwise this is equivalent to memmove)
  void emitMemcpyNoBoundsCheck(REG const dstReg, REG const srcReg, REG const sizeReg, REG const scratchReg, bool const canOverlap) const;

  /// @brief Emits machine code that copies the value of a StackElement (can be in memory or in any other register) into
  /// an address register of the CPU
  /// @note This will fail on assert if the data is already in an address register
  ///
  /// @param addrReg Target address register of the CPU to put the value into
  /// @param elem StackElement to copy the value from
  void copyValueOfElemToAddrReg(REG const addrReg, StackElement const &elem) const;

  /// @brief if the f64 number in distReg is Nan, convert it to Canonical, otherwise keep the original value
  /// @param distReg the register where the nan located
  // coverity[autosar_cpp14_m3_2_4_violation]
  // coverity[autosar_cpp14_m3_2_2_violation]
  void f64NanToCanonical(REG const distReg);
  /// @brief if the f32 number in distReg is Nan, convert it to Canonical, otherwise keep the original value
  /// @param distReg the register where the nan located
  // coverity[autosar_cpp14_m3_2_4_violation]
  // coverity[autosar_cpp14_m3_2_2_violation]
  void f32NanToCanonical(REG const distReg);

  ///
  /// @brief Return type of getMemRegDisp, representing a base register and a displacement
  template <size_t range> class RegDisp final {
  public:
    REG reg;             ///< Base register
    SafeInt<range> disp; ///< Displacement from the base register
  };

  /// @brief emit instruction of add a i64 value with imm
  /// @param arg0 first operand
  /// @param arg1 second operand
  /// @param targetHint A target hint that can be used as a scratch register or temporary variable location if the type
  /// matches
  /// @param commutative true in case add, false if use this function as alias of sub
  /// @return valid StackElement if operand can be encoded as imm, invalid StackElement if operand can't be encoded as
  /// imm
  StackElement emitI64AddImm(StackElement const &arg0, StackElement const &arg1, StackElement const *const targetHint, bool const commutative);

  /// @brief emit instruction of add a i64 value with imm
  /// @param opcode opCode of and/or/xor
  /// @param arg0 first operand
  /// @param arg1 second operand
  /// @param targetHint A target hint that can be used as a scratch register or temporary variable location if the type
  /// matches
  /// @return valid StackElement if operand can be encoded as imm, invalid StackElement if operand can't be encoded as
  /// imm
  StackElement emitI64AndOrImm(OPCode const opcode, StackElement const &arg0, StackElement const &arg1, StackElement const *const targetHint);

  /// @brief try to emit select instruction with imm encoded in instruction
  /// @param opCode SEL_Dc_Da_Dd_const9sx or SELN_Dc_Da_Dd_const9sx
  /// @param is64 is 64 or 32 bit
  /// @param regElement stack element which will be lift to reg
  /// @param immElement stack element which will be be encoded as imm
  /// @param condReg condition register
  /// @param targetHint target hint
  /// @param regAllocTracker A tracker to track the protected register mask which prevents the registers which
  /// correspond to the set bits in the mask from being modified
  /// @return regElement if imm in range, invalid element if imm out of range
  StackElement emitSelectImm(OPCodeTemplate const opCode, bool const is64, StackElement &regElement, StackElement const &immElement,
                             REG const condReg, StackElement const *const targetHint, RegAllocTracker &regAllocTracker);

  /// @brief emit instruction of eq/ne a i64 value with imm
  /// @param opcode opcode, can be eq or ne
  /// @param arg0 first operand
  /// @param arg1 second operand
  /// @param targetHint A target hint that can be used as a scratch register or temporary variable location if the type
  /// matches
  /// @return target element if imm in range, invalid element if imm out of range
  RegElement emitI64EqImm(OPCode const opcode, StackElement const &arg0, StackElement const &arg1, StackElement const *const targetHint);

  ///
  /// @brief Positions of where the logic for unaligned memory load/store is, can be reused by later code to save space
  class UnalignedAccessCodePositions final {
  public:
    uint32_t load2 = UINT32_MAX; ///< Position of unaligned load 2 bytes memory access
    uint32_t load4 = UINT32_MAX; ///< Position of unaligned load 4 bytes memory access
    uint32_t load8 = UINT32_MAX; ///< Position of unaligned load 8 bytes memory access

    uint32_t store2 = UINT32_MAX; ///< Position of unaligned store 2 bytes memory access
    uint32_t store4 = UINT32_MAX; ///< Position of unaligned store 4 bytes memory access
    uint32_t store8 = UINT32_MAX; ///< Position of unaligned store 8 bytes memory access
  };
  UnalignedAccessCodePositions unalignedAccessCodePositions_; ///< Collection of where the logic for unaligned memory load/store is positioned

  ///
  /// @brief Analyze result of if a pair of I64 StackElements can be encoded with imm
  ///
  ///
  class I64OperandConstAnalyze final {
  public:
    StackElement const *immElement = nullptr; ///< Pointer to imm encodable StackElement, can be nullptr if no StackElement is imm encodable
    StackElement const *regElement = nullptr; ///< Pointer to StackElement which need to be stored in register, can be
                                              ///< nullptr if no StackElement is imm encodable

    /// @brief Store the value of const analyze, store it as safeInt if it's in range safe int, otherwise save the row
    /// value
    // coverity[autosar_cpp14_a11_0_1_violation] Fixme a bug reported to coverity
    union ImmValue {
      SafeInt<9U> safeValue; ///< safeInt variant
      int32_t rawValue = 0;  ///< row value variant
    };
    ImmValue rawLow;                    ///< Low 32bit imm
    ImmValue rawHigh;                   ///< High 32bit imm
    bool arg0LowIsDirectConst = false;  ///< arg0 low 32bit is imm encodable
    bool arg1LowIsDirectConst = false;  ///< arg1 low 32bit is imm encodable
    bool arg0HighIsDirectConst = false; ///< arg0 high 32bit is imm encodable
    bool arg1HighIsDirectConst = false; ///< arg1 high 32bit is imm encodable
    bool arg0IsDirectConst = false;     ///< Hole 64 bit arg0 is imm encodable
    bool arg1IsDirectConst = false;     ///< Hole 64 bit arg1 is imm encodable
  };

  ///
  /// @brief Analyze result of if a pair of U64 StackElements can be encoded with imm
  ///
  ///
  class U64OperandConstAnalyze final {
  public:
    StackElement const *immElement = nullptr; ///< Pointer to imm encodable StackElement, can be nullptr if no StackElement is imm encodable
    StackElement const *regElement = nullptr; ///< Pointer to StackElement which need to be stored in register, can be
                                              ///< nullptr if no StackElement is imm encodable

    /// @brief Store the value of const analyze, store it as safeInt if it's in range safe int, otherwise save the row value
    // coverity[autosar_cpp14_a11_0_1_violation] Fixme a bug reported to coverity
    union ImmValue {
      SafeUInt<9U> safeValue; ///< safeInt variant
      uint32_t rawValue = 0U; ///< row value variant
    };

    ImmValue rawLow;                    ///< Low 32bit imm
    ImmValue rawHigh;                   ///< High 32bit imm
    bool arg0LowIsDirectConst = false;  ///< arg0 low 32bit is imm encodable
    bool arg1LowIsDirectConst = false;  ///< arg1 low 32bit is imm encodable
    bool arg0HighIsDirectConst = false; ///< arg0 high 32bit is imm encodable
    bool arg1HighIsDirectConst = false; ///< arg1 high 32bit is imm encodable
    bool arg0IsDirectConst = false;     ///< Hole 64 bit arg0 is imm encodable
    bool arg1IsDirectConst = false;     ///< Hole 64 bit arg1 is imm encodable
  };

  ///
  /// @brief Analyze if a pair of StackElements can be encoded with Const9sx
  ///
  /// @param arg0 first operand
  /// @param arg1 second operand
  /// @param commutative If arg0 and arg1 are commutative
  /// @return I64OperandConstAnalyze const
  ///
  static I64OperandConstAnalyze const analyzeImm64OperandConst(StackElement const &arg0, StackElement const &arg1,
                                                               bool const commutative) VB_NOEXCEPT;

  ///
  /// @brief Analyze if a pair of StackElements can be encoded with Const9zx
  ///
  /// @param arg0 first operand
  /// @param arg1 second operand
  /// @param commutative If arg0 and arg1 are commutative
  /// @return I64OperandConstAnalyze const
  ///
  static U64OperandConstAnalyze const analyzeUnsignedImm64OperandConst(StackElement const &arg0, StackElement const &arg1,
                                                                       bool const commutative) VB_NOEXCEPT;

  /// @brief emit cmp instruction for f64
  /// @param target target reg
  /// @param arg0 operand 1
  /// @param arg1 operand 2
  void emitCMPF64(REG const target, REG const arg0, REG const arg1);

  /// @brief emit cmp instruction for f32
  /// @param target target reg
  /// @param arg0 operand 1
  /// @param arg1 operand 2
  void emitCMPF32(REG const target, REG const arg0, REG const arg1);

  ///
  /// @brief Check if a Stack element is signed const and the value also in safe range
  ///
  /// @tparam range bit range
  /// @param elem element to check
  /// @return const SignedInRangeCheck<range>
  ///
  template <size_t range> static const SignedInRangeCheck<range> checkStackElemSignedConstInRange(StackElement const &elem) VB_NOEXCEPT {
    if (elem.type == StackType::CONSTANT_I32) {
      return SignedInRangeCheck<range>::check(bit_cast<int32_t>(elem.data.constUnion.u32));
    } else {
      return SignedInRangeCheck<range>::invalid();
    }
  }

  ///
  /// @brief Check if a Stack element is unsigned const and the value also in safe range
  ///
  /// @tparam range bit range
  /// @param elem element to check
  /// @return const SignedInRangeCheck<range>
  ///
  template <size_t range> static const UnsignedInRangeCheck<range> checkStackElemUnsignedConstInRange(StackElement const &elem) VB_NOEXCEPT {
    if (elem.type == StackType::CONSTANT_I32) {
      return UnsignedInRangeCheck<range>::check(elem.data.constUnion.u32);
    } else {
      return UnsignedInRangeCheck<range>::invalid();
    }
  }

  ///
  /// @brief Resolve a given VariableStorage location to a register and an optional constant offset
  /// @tparam maxDisplacementBits Maximum displacement bits the calling function can handle, larger offsets will be
  /// directly encoded in the returned register
  /// @param storage Input VariableStorage
  /// @param addrScrReg Address scratch register
  /// @return RegDisp that points to the memory location of the variable and can then be dereferenced
  /// @throws vb::RuntimeError If not enough memory is available
  template <size_t maxDisplacementBits>
  RegDisp<maxDisplacementBits> getMemRegDisp(VariableStorage const &storage, REG const addrScrReg) const VB_THROW {
    REG returnReg{REG::NONE};
    int32_t returnDisp{0};
    if (storage.type == StorageType::LINKDATA) {
      returnReg = WasmABI::REGS::linMem;
      uint32_t const basedataLength{moduleInfo_.getBasedataLength()};
      returnDisp = bit_cast<int32_t>((Basedata::FromStart::linkData - basedataLength) + storage.location.linkDataOffset);
    } else if (storage.type == StorageType::STACKMEMORY) {
      returnReg = REG::SP;
      static_assert(ImplementationLimits::maxStackFrameSize <= static_cast<uint32_t>(INT32_MAX), "Max stack frame size too");
      uint32_t const distVal{moduleInfo_.fnc.stackFrameSize - storage.location.stackFramePosition};
      returnDisp = static_cast<int32_t>(distVal);
    } else {
      // GCOVR_EXCL_START
      UNREACHABLE(return RegDisp<maxDisplacementBits>{}, "Unknown StorageType");
      // GCOVR_EXCL_STOP
    }

    constexpr uint32_t boundVal{(1_U32 << (maxDisplacementBits - 1U)) - 1U};
    constexpr int32_t upperBound{static_cast<int32_t>(boundVal)};
    constexpr int32_t lowerBound{-upperBound - 1};

    // If the displacement is greater than the allowed displacement, we add it (or part of it) to a register
    SignedInRangeCheck<maxDisplacementBits> const rangeCheck{SignedInRangeCheck<maxDisplacementBits>::check(returnDisp, lowerBound, upperBound)};
    if (!rangeCheck.inRange()) {
      // We always need a new register for this, because linMemReg and SP must not be overwritten
      REG const alternateReg{addrScrReg};

      int32_t toAdd;
      SafeInt<maxDisplacementBits> rest{};
      if (returnDisp > upperBound) {
        toAdd = returnDisp - upperBound;
        rest = SafeInt<maxDisplacementBits>::template fromConst<upperBound>();
        assert(toAdd > 0);
      } else {
        assert(returnDisp < lowerBound);
        toAdd = returnDisp - lowerBound;
        rest = SafeInt<maxDisplacementBits>::template fromConst<lowerBound>();
        assert(toAdd < 0);
      }

      // total_displacement = toAdd + rest
      // addrScrReg = REG::SP + toAdd
      // return {addrScrReg, rest}
      as_.addImmToReg(returnReg, bit_cast<uint32_t>(toAdd), alternateReg);
      return {alternateReg, rest};
    } else {
      return {returnReg, rangeCheck.safeInt()};
    }
  }

  /// @brief wrapper for adapter DR in tricore backend and GPR/FPR in @b Common.
  /// In tricore, there are no gpr and fpr. Both integer and floating point number are store in data register(DR).
  inline uint32_t getNumLocalsInDr() const VB_NOEXCEPT {
    return moduleInfo_.fnc.numLocalsInGPR;
  }

  /// @brief wrapper of increase number of locals in Dr
  inline void increaseNumLocalsInDr() VB_NOEXCEPT {
    moduleInfo_.fnc.numLocalsInGPR++;
  }

  /// @brief wrapper of offset handler in iterate params for signature
  // coverity[autosar_cpp14_a15_4_4_violation]
  template <size_t bits_target> SafeInt<bits_target> selectOffsetRegisterHelper(int32_t &addedOffset, int32_t const currentOffsetUnsafe) {
    SignedInRangeCheck<bits_target> const rangeChecker{SignedInRangeCheck<bits_target>::check(currentOffsetUnsafe)};
    if (!rangeChecker.inRange()) {
      as_.addImmToReg(NativeABI::addrParamRegs[0], bit_cast<uint32_t>(currentOffsetUnsafe));
      addedOffset = addedOffset + currentOffsetUnsafe;
      return SafeInt<bits_target>::template fromConst<0>();
    } else {
      return rangeChecker.safeInt();
    }
  }

  ///
  /// @brief Get the position in the gpr or fpr array for a register
  ///
  /// @param reg Register to look up
  /// @param import Whether the register is used in an imported function call (NativeABI) or in a Wasm function call (WasmABI)
  /// @return Position of this register in the gpr or fpr array. UINT8_MAX if the register is not a parameter
  uint32_t getParamPos(REG const reg, bool const import) const VB_NOEXCEPT;

  /// @brief Swap the values of two registers
  /// @param reg1 First register
  /// @param reg2 Second register
  void swapReg(REG const reg1, REG const reg2);

  /// @brief Minimal number of registers that should be reserved for condense a vb.
  /// @details Need to keep 2 regs to avoid spill when add mem, mem or select reg, mem, mem.
  static constexpr uint32_t minimalNumRegsReservedForCondense{2U};

  Stack &stack_;           ///< Reference to the compiler stack
  ModuleInfo &moduleInfo_; ///< Reference to the ModuleInfo struct containing information about the WebAssembly module
  MemWriter &memory_;      ///< Reference to the compiler memory
  MemWriter &output_;      ///< Reference to the output binary
  Common &common_;         ///< Reference to the common instance
  Compiler &compiler_;     ///< Reference to the compiler instance
  Tricore_Assembler as_;   ///< Tricore assembler instance that emits instructions

  friend Tricore_Assembler; ///< So the assembler can access the compiler reference

  // Friend declarations for call dispatch classes
  friend class CallBase;
  friend class DirectV2Import;
  friend class V1CallBase;
  friend class ImportCallV1;
  friend class InternalCall;

  /// @brief The offset between the address where the trap code is stored with and REG::SP.
  static constexpr uint32_t of_trapCodePtr_trapReentryPoint{0U};

#if ENABLE_EXTENSIONS
  ///
  /// @brief Update the pressure histogram when a new register is allocated
  void updateRegPressureHistogram() const VB_NOEXCEPT;
#endif

  ///
  /// @brief Load and calculate memSize reg from job memory
  inline void setupMemSizeReg() const {
    as_.INSTR(LDA_Aa_deref_Ab_off16sx)
        .setAa(WasmABI::REGS::memSize)
        .setAb(WasmABI::REGS::linMem)
        .setOff16sx(SafeInt<16U>::fromConst<-Basedata::FromEnd::actualLinMemByteSize>())();
  }
};

} // namespace tc
} // namespace vb

#endif
#endif /* X86_H */
