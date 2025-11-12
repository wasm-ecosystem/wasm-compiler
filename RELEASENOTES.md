# Wasm Compiler Release Notes

## 4.0.0

### Feature

- Support multi return values import function(not call_indirect)

### Bug Fixes

- Fixed the problem that load opcode will trigger Implement Limitation on TRICORE.
- Fixed the misbehavior of wasm memcpy on x86 backend
- Fix SPDX format

## 3.0.3

Add u32 and u64 type to WasmValue

## 3.0.2

### Feature

- Add version number to checking to binary module

### Bug Fixes

- Fixed the issue where each memory access on TRICORE would call the memory extend function.
- Fixed the issue where `setMaxRam` will crash when wasm module does not initialize runtime.

## 3.0.1

### Bug Fixes

- Fixed `memory.grow` will provide wrong page size to runtime.

## 3.0.0

### Feature

- Implemented multi values feature.
- Refine signal handler for better working with system coredump. (cherry-pick from 2.3.4)
- Add a WasmModule class wrappers over raw compiler and runtime. It saves integrator from handling low level interfaces.
- Add experimental support for DWARF5 debugging symbols.
- API `abortTrap` is removed.
- Import functions must assign last parameter as a `T*` context.
- Add `builtin.tracePoint` API for tracing without performance overhead.
  - Tracing Extension is designed for record this trace points and write to file.

### Bug Fixes

- Fixed `builtin.isFunctionLinked` return incorrect result when index larger than 256. (cherry-pick from 2.3.5)
- Fixed that `recoverGlobalsToRegs` will dirty write `addrParamRegs[3]` in exported function wrapper (with multi values).
- Fixed emitted instructions for select, keep CPUFlags unchanged.
- Fixed API function can't interrupt a Module when the API is call indirectly

### Breaking Changes

- Unify exported ModuleFunction, RawModuleFunction API.
- Trap code is cleaned at exit of Wasm function call
- Add check of mmap failed in MemUtils
- Added compilerMemoryAllocFnc and compilerMemoryFreeFnc used for compiler-time-stack memory management in Compiler construction, and allow user-defined context (like maxRAM).

#### Tricore backend

- Stop supporting of tc1.6 and only support tc1.8

## 2.2.3

- Replace `vb_MemUtils` with `vb_libutils` for tricore backend.

### Bug Fixes

- Fixed incorrect parameter alignment in runtime.
- TriCore Backend:
  Fix f32NanToCanonical assert failure

## 2.2.2

- Use LINEAR_MEMORY_BOUNDS_CHECKS on real time operation system e.g. QNX by default.
- `builtin.isFunctionLinked` takes table index as parameter instead of function index. This change is for consistency with frontend compiler.

### Bug fixes

- Fix mis use of fucIndex and sigIndex in tricore emitWasmToNativeAdapter, which may lead to crash.

## 2.2.1

### Bug Fixes

- Fixed compile errors in `Exception::what` introduced by `VB_DISABLE_NOEXCEPT`.

## 2.2.0

### Feature

- Streamlined stacktrace implementation.
- Implemented sign extension instructions.
- Implemented bulk memory `memory.copy` and `memory.fill` instructions.
- Added support for debugging Wasm modules.
- Added new builtin function: `copyFromLinkedMemory`.
- add persistent mode of signal handler

### Improvement

- During call import function, only save volatile registers and native ABI parameter registers
- Added compilation option `VB_DISABLE_NOEXCEPT` to disable `noexcept`.
- Optimized emitted jit code for `drop` opcode.
- Changed WasmABI, store and recover in reg local on demand (#118).
- Introduced control flow analysis to reduce check stack fences in active stack overflow check mode.
- Allocated register for the first i32 global.
- Optimized emitted jit code for `br_table` opcode.
- Refactor compile flow of select instruction
- TriCore Backend
  - Optimized emitted jit code for `ctz` opcode.
  - Optimized memory bound check with `FCALL` and `FRET` instructions.
  - Optimized with 16bit instructions for mov/add/ldr/str/eq/lt.
  - Improved performance of 64-bit add/sub/and/or/xor.
  - Improved performance of compare with imm.
  - Supported odd length of linked memory.

### Breaking Changes

- Replaced "eager" parameter of LinearMemoryAllocator with a compile-time config EAGER_ALLOCATION so compiler can take advantage of this setting.

- ILogger receives `Span<char const> const &` as parameter.

### Bug fixes

- Fixed a bug where block-expr following an always-negated if-expr would emit incorrect jit code.
- Fixed a bug where target result types mismatched `br_table` in unreachable block would fail module validation.
- Fixed a bug where high register pressure will crash the compiler.
- TriCore Backend
  - Fixed a bug where parameters reg will be overwritten.
  - Fixed a bug where `load` will overwrite other scratch registers.

## 2.1.3

- Compiler can now take an rvalue reference of the output binary directly.

## 2.1.2

- Fixed incorrect trap error messages.

## 2.1.1

- Fixed a bug where compilation could fail with unknown/missing imports.

## 2.1.0

- Added an option to allow compilation with unknown/missing imports. Calling such a function will produce a trap with TrapCode::CALLED_FUNCTION_NOT_LINKED
- Added a new builtin function isFunctionLinked which takes a function index as parameter and returns 1 if this function is linked (or defined in the Wasm module, i.e. can be called) and 0 otherwise
- Removed config option USE_MOCK_IMPORTS
- Added an optimization so if(-else) statements for which the condition can be evaluated at compile-time only produce one of the two blocks and no branch
- Removed auto-unlink feature and the corresponding entry in config.hpp
- Optimized trap handling and reordered TrapCode enum entries

## 2.0.5

- Linked memory is now auto-unlinked by default when a new API call is performed. This can be configured in config.hpp.
- Improvements for TriCore
  - Remove unneeded destructor, static variables and do not rely on global variables being initialized
  - TriCore will now default to statically linked AUX functions
