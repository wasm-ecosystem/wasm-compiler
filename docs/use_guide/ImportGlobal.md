# Register Imported Globals

Use `vb::GlobalSymbol` to provide imported global values to a Wasm module.

Imported globals are not declared with a macro. Instead, you create `vb::GlobalSymbol`
values and pass them to the compile or load step.

Each imported global needs three things:

1. the module name
2. the field name
3. the value

The module name and field name must match the names used by the Wasm module.

## Choose the matching factory

Create each global with the factory that matches its Wasm type:

```cpp
vb::GlobalSymbol::fromInt32(...)
vb::GlobalSymbol::fromUInt32(...)
vb::GlobalSymbol::fromInt64(...)
vb::GlobalSymbol::fromUInt64(...)
vb::GlobalSymbol::fromFloat32(...)
vb::GlobalSymbol::fromFloat64(...)
```

In practice, that means:

1. use `fromInt32()` or `fromUInt32()` for `i32`
2. use `fromInt64()` or `fromUInt64()` for `i64`
3. use `fromFloat32()` for `f32`
4. use `fromFloat64()` for `f64`

## Declare the globals

Create the imported globals as normal values:

```cpp
std::array<vb::GlobalSymbol, 4> linkedGlobals{
  vb::GlobalSymbol::fromInt32("env", "threshold", 10),
  vb::GlobalSymbol::fromInt64("env", "limit", 100),
  vb::GlobalSymbol::fromFloat32("env", "ratio", 0.5F),
  vb::GlobalSymbol::fromFloat64("env", "scale", 1.25),
};
```

The module name and field name must match the Wasm import declaration exactly.

## Register them with the compiler

Pass the globals as a `Span<GlobalSymbol const>` when you compile or load the module:

```cpp
vb::Span<vb::GlobalSymbol const> const importGlobals{
  linkedGlobals.data(), static_cast<uint32_t>(linkedGlobals.size())};

auto binary = compiler.compile(bytecode, linkedFunctions, importGlobals);
```

If you use `WasmModule` instead of `Compiler`, the idea is the same: pass the same
`importGlobals` span to the `compile(...)` overload that accepts linked globals.

## Optimization note

There is a small compiler optimization for imported globals, but it is intentionally
limited.

If the Wasm module declares an imported global as immutable, the compiler can treat
`global.get` as a constant value.

This optimization is limited to simple `i32` and `i64` comparison-style checks, for
example:

```wat
if (global_i32 > 10)
if (global_i32 < 10)
if (global_i32 == 10)
if (global_i64 == 42)
if (global_i32 >= 10)
if (global_i32 <= 10)
```

This is not a general-purpose optimization pass. Based on the current compiler code,
only a limited set of `i32` and `i64` comparison cases are optimized out. You should
not rely on floating-point comparisons or more complex expressions being folded.

If the imported global is mutable, it is not treated as a compile-time constant, so
those checks will not be optimized away.
