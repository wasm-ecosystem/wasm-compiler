# Import Function Call

Imported functions have two execution paths in the compiler/runtime:

1. direct Wasm `call`
2. indirect call through table or start-function wrapper

## Overview

The runtime supports two host import ABIs:

1. V1 import: native ABI mapping
2. V2 import: adapter ABI

V2 import uses the following host function shape:

```cpp
void (*)(void *params, void *results, void *ctx)
```

`params` points to a parameter slot array, `results` points to a return slot array,
and `ctx` points to the custom runtime context stored in link data.

For V2, params and results always use 8-byte slots. `i32` / `f32` use 4 bytes payload in one slot,
while `i64` / `f64` use the full slot.

## Direct Call

`OPCode::CALL` eventually dispatches to:

```cpp
compiler_.backend_.execDirectFncCall(calledFunctionIndex);
```

For imported functions, `execDirectFncCall()` distinguishes V1 and V2.
V2 direct call is implemented inside each backend through `DirectV2Import`, not through a shared
`execDirectV2ImportCallImpl` function.

Key sequence:

```cpp
DirectV2Import v2ImportCall{*this, sigIndex};
Stack::iterator const paramsBase{common_.prepareCallParams(...)};
v2ImportCall.iterateParams(paramsBase);
v2ImportCall.emitFncCallWrapper(fncIndex, ... emitRawFunctionCall(fncIndex) ...);
v2ImportCall.iterateResults();
```

## Indirect Call

Indirect calls use the Wasm table and must preserve the Wasm calling convention at the table entry.
Imported functions therefore need a generated Wasm-to-native wrapper.

The frontend generates this wrapper for imported functions that appear in:

1. The start function
2. Table elements used by `call_indirect`

The wrapper entry is:

```cpp
emitWasmToNativeAdapter(fncIndex)
```

The generated wrapper offset is stored in:

```cpp
moduleInfo_.wasmFncBodyBinaryPositions[fncIndex]
```

`emitWasmToNativeAdapter()` performs the high-level dispatch:

1. Verify the target is an imported function
2. Reject builtin imports for indirect call
3. Move globals to link data if required
4. Choose the correct adapter implementation:

   - `emitV1ImportAdapterImpl(fncIndex)`
   - `emitV2ImportAdapterImpl(fncIndex)`

The wrapper repacks Wasm-call arguments into V2 slots, calls the host function, then maps return slots
back to the Wasm return ABI.
