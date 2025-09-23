# Getting Started with WARP

## General Approach

This library provides two high-level classes:

- WARP Compiler can be used to translate WebAssembly bytecode to machine code and additional link info
- WARP Runtime can then be used to instantiate this WebAssembly modules and execute functions

```mermaid
flowchart LR
    input.wasm --> |Wasm Bytecode| compiler["WARP Compiler"] -->|Machine Code + Link Info| runtime["WARP Runtime"]
```

## Compiling a WebAssembly Module and Executing a Function

```C++
#include <cstdlib>
#include <iostream>

#include "src/core/compiler/Compiler.hpp"

void *allocFnc(uint32_t size, void *ctx) noexcept {
  static_cast<void>(ctx);
  return malloc(static_cast<size_t>(size));
}

void freeFnc(void *ptr, void *ctx) noexcept {
  static_cast<void>(ctx);
  free(ptr);
}

// Function the compiler will use to request memory for its internal bookkeeping and for allocating memory for the output machine code
void memoryAllocFnc(ExtendableMemory &currentObject, uint32_t minimumLength) {
  if (minimumLength == 0) {
    free(currentObject.data());
  } else {
    // Double every time and allocate minimum 256k at once
    minimumLength = std::max(256 * 1024_U32, minimumLength * 2_U32);
    currentObject.reset(vb::pCast<uint8_t *>(realloc(currentObject.data(), minimumLength)), minimumLength);
  }
}

void run(std::vector<uint8_t> const &wasmByteCode) {
    try {
        // Initialize compiler class
        Compiler compiler = Compiler(memoryAllocFnc, allocFnc, freeFnc, nullptr, memoryAllocFnc);

        // Compile and generate a RAII machine code object
        ManagedBinary binaryModule = compiler.compile(wasmByteCode);

        // Copy resulting machine code to an executable memory via utils
        ExecutableMemory const executableMemory = ExecutableMemory::make_executable_copy(binaryModule);

        // Set up a LinearMemoryAllocator and Runtime class
        LinearMemoryAllocator linearMemoryAllocator;
        Runtime runtime(executableMemory, linearMemoryAllocator);

        // Initialize the WebAssembly module, wrap via SignalFunctionWrapper on UNIX systems so we can efficiently handle signals from the MMU
        SignalFunctionWrapper::start(runtime);

        // Retrieve a callable function that is exported under a specific name with numReturnValues and type of function arguments
        // For example: 2 means the WebAssembly function has 2 return values, and the arguments type is <int32_t, int32_t>
        auto exportedFnc = runtime.getExportedFunctionByName<2, int32_t, int32_t>("fncName");
        auto const res = SignalFunctionWrapper::call(exportedFnc, 0, 0);
        std::tuple<double> result = exportedFnc.derefReturnValues<double>(res.data());
        std::cout << "Result is " << std::get<0>(result) << std::endl;
    } catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
    }
}
```

## Build Config

There are a number of flags available to configure how the compiler and runtime operate. Those are defined in [src/config.hpp](src/config.hpp). Using CMake they can be defined using `CMAKE_CXX_FLAGS`.

The following configuration flags are available:

| Flag                                  | Valid Values             | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| ------------------------------------- | ------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| BACKEND                               | aarch64, x86_64, tricore | Choose the ISA the compiler will generate code for. Also supports cross-compilation                                                                                                                                                                                                                                                                                                                                                                             |
| INTERRUPTION_REQUEST                  | 0 or 1                   | Enable interruption of running WebAssembly modules. Impacts performance.                                                                                                                                                                                                                                                                                                                                                                                        |
| BUILTIN_FUNCTIONS                     | 0 or 1                   | Enable support for builtin functions, e.g. for interacting with linked memory and to allow WebAssembly modules to check whether specific imported functions are linked.                                                                                                                                                                                                                                                                                         |
| ACTIVE_STACK_OVERFLOW_CHECK           | 0 or 1                   | Enable explicit stack overflow checks to prevent signals being generated by an overflowing stack or if no MMU is available. Impacts performance.                                                                                                                                                                                                                                                                                                                |
| LINEAR_MEMORY_BOUNDS_CHECKS           | 0 or 1                   | Enable explicit bounds checks when accessing linear memory instead of using guard pages and relying on MMU.                                                                                                                                                                                                                                                                                                                                                     |
| MAX_WASM_STACKSIZE_BEFORE_NATIVE_CALL | 0 to UINT32_MAX          | Set only for passive (non-active) stack overflow protection. Allows to limit the maximum stack size the WebAssembly module is allowed to reach before executing a host function so calling the host function will not lead to a stack overflow.                                                                                                                                                                                                                 |
| STACKSIZE_LEFT_BEFORE_NATIVE_CALL     | 0 to UINT32_MAX          | Set only for active stack overflow check. Limit the maximum stack size the WebAssembly module by defining the minimum remaining stack that is left before executing a host function so calling the host function will not lead to a stack overflow.                                                                                                                                                                                                             |
| EAGER_ALLOCATION                      | 0 or 1                   | Set only for if linear memory bounds checks are disabled. With this feature, the full _formal_ size of the linear memory is immediately marked as accessible, otherwise every time the module accesses a memory page that hasn't been previously accessed, the signal handler is triggered and if it is within the allowed region the page is dynamically marked as accessible. This allows the runtime to keep track (with granularity of system memory pages) |

### Tricore Specific Config

In addition to that, there are architecture-specific configuration flags for TriCore available:

| Flag                               | Valid Values | Description                                                                                                                            |
| ---------------------------------- | ------------ | -------------------------------------------------------------------------------------------------------------------------------------- |
| TC_USE_HARD_F32_TO_I32_CONVERSIONS | 0 or 1       | Use CPU instructions for float to int conversions (soft impl. otherwise)                                                               |
| TC_USE_HARD_F32_ARITHMETICS        | 0 or 1       | Use CPU instructions for other float arithmetic (soft impl. otherwise)                                                                 |
| TC_LINK_AUX_FNCS_DYNAMICALLY       | 0 or 1       | Link soft impl. functions dynamically. Must be turned off if the system uses ASLR since the addresses of the C++ functions will change |
| TC_USE_DIV                         | 0 or 1       | Use div instruction (soft impl. otherwise)                                                                                             |
