# Register Import Functions

The runtime supports two host import ABIs:

1. V1 import: native ABI mapping
2. V2 import: adapter ABI

## V1 import functions

V1 import functions follow the C ABI. They are declared with either `STATIC_LINK` or
`DYNAMIC_LINK`, depending on the desired linkage.

```cpp
static inline uint32_t add(uint32_t lhs, uint32_t rhs, void *const ctx) noexcept {
  static_cast<void>(ctx);
  return lhs + rhs;
}

auto imports = vb::make_array(
    STATIC_LINK("env", "add", add),
    DYNAMIC_LINK("env", "add_dynamic", add));
```

For V1, the function signature is derived from the native function type by the
`STATIC_LINK` and `DYNAMIC_LINK` macros.

Use V1 when the number of return values is less than or equal to 1. **It is the preferred
import style in that case because it avoids the V2 adapter path and therefore has better
performance.**

## V2 import functions

`ImportFunctionV2` is used when a host import must return multiple values to Wasm, because
the C ABI used by V1 imports does not support multi-value returns in this interface.

V2 import functions use the following host function shape:

```cpp
void (*)(void *params, void *results, void *ctx)
```

`params` points to a parameter slot array, `results` points to a return slot array, and
`ctx` points to the custom runtime context stored in link data.

For V2, params and results always use 8-byte slots. `i32` and `f32` use 4 bytes payload in
one slot, while `i64` and `f64` use the full slot.

To define a V2 import function:

1. Derive a class from `ImportFunctionV2<ParamsTuple, ReturnsTuple>`.
2. Put the Wasm parameter types into `ParamsTuple` and the Wasm result types into `ReturnsTuple`.
3. Implement a static `call(void *params, void *results, void *ctx)` function.
4. Read arguments with `getParam<Index>(params)`.
5. Write results with `setRet<Index>(results, value)`.
6. Register the function with `generateNativeSymbol(...)`.

Example adapted from `tests/testimports.hpp`:

```cpp
class MultiReturn final : public ImportFunctionV2<std::tuple<uint32_t, uint64_t>, std::tuple<uint32_t, uint64_t>> {
public:
  using ImportFunctionV2::ImportFunctionV2;

  static void call(void *params, void *results, void *ctx) {
    uint32_t const p0 = getParam<0>(params);
    uint64_t const p1 = getParam<1>(params);
    static_cast<void>(ctx);

    setRet<0>(results, p0 + 1U);
    setRet<1>(results, p1 + 2U);
  }
};

auto imports = vb::make_array(
    MultiReturn::generateNativeSymbol("env", "multiReturn", vb::NativeSymbol::Linkage::DYNAMIC, MultiReturn::call));
```

The allowed parameter and return element types for `ImportFunctionV2` are `uint32_t`,
`uint64_t`, `float`, and `double`.# Register Import Functions
