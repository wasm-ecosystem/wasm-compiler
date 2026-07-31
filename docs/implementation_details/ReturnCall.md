# ReturnCall

## Definition

The `return_call` instruction is introduced by the WebAssembly 2.0 specification. For a call chain such as
`caller0 -> caller1 -> return_call callee`, the specification requires the result types of `caller1` to match the result types of
`callee`.

Because `caller1` returns directly to `caller0` after the `return_call`, the backend can reduce the overhead of the call. When the
stack layout is compatible, the backend can replace the call with a jump. Otherwise, it still emits a normal function call, but avoids
saving the current function context that will not be used again.

## Design in wasm-compiler

The implementation classifies `return_call` into three cases. Cases A-1 and A-2 use Path A, which replaces the call with a tail jump.
Case B uses Path B, which emits a lightweight normal call.

The classification is based on the wasm-compiler internal ABI: stack parameters and stack return values are prepared and released by the
caller. Therefore, the stack return slots and stack parameter slots of `caller1` are part of `caller0`'s frame:

```text
higher addresses
┌──────────────────────────────────────────────┐
│ caller0 stack frame                          │
│                                              │
│  caller0-owned area for call to caller1      │
│  ┌────────────────────────────────────────┐  │
│  │ return slots expected from caller1     │  │
│  ├────────────────────────────────────────┤  │
│  │ stack parameters passed to caller1     │  │
│  └────────────────────────────────────────┘  │
├──────────────────────────────────────────────┤
│ caller1 local frame                          │ ← current frame before return_call
└──────────────────────────────────────────────┘
lower addresses
```

If `caller1` tail-jumps to `callee`, `callee` reuses `caller1`'s frame instead of receiving a new call frame. That is safe only when the
callee's caller-visible stack writes still match the slots that `caller0` will read after `caller1` returns.

### RC-Case A-1: Equal stack-parameter width

Classification standard: the callee is not imported, and `calleeStackParamWidth == callerStackParamWidth`.

Reason: imported functions use a different ABI, so only internal non-imported callee can reuse the current frame. When the total
stack-parameter widths are equal, the callee's stack parameter area has the same size as the area that `caller0` already reserved for
`caller1`. This keeps any stack return slots next to a caller-visible parameter area with the expected width.

> **Note:** Only the total width must match; the number of parameters does not have to match. For example, on macOS AArch64, stack parameters may
> occupy either 4 bytes or 8 bytes, while other backends usually use 8-byte-aligned stack slots.

### RC-Case A-2: Smaller callee stack-parameter width without stack returns

Classification standard: the callee is not imported, `stackReturnValuesWidth == 0U`, and
`calleeStackParamWidth < callerStackParamWidth`.

Reason: the callee needs less stack-parameter space than `caller1`, so it can fit inside the caller-visible stack parameter area that
already exists. Because the callee has no stack return values, it will not write caller-visible return slots next to this smaller parameter
area. The extra stack parameter slots reserved for `caller1` are simply unused by `callee`.

### RC-Case B: Lightweight call

Classification standard: any case not covered by A-1 or A-2. This includes imported callee, callee whose stack-parameter width is larger
than the caller's, and callee with stack return values whose stack-parameter width is smaller than the caller's.

For example, assume `caller1` has one stack return value and three stack parameters, while `callee` has one stack return value and one stack
parameter:

```text
caller0's view after calling caller1
┌──────────────────────────────────────────────┐
│ ret-caller1                                  │ ← caller0 will read this as the return value
├──────────────────────────────────────────────┤
│ param-a                                      │
├──────────────────────────────────────────────┤
│ param-b                                      │
├──────────────────────────────────────────────┤
│ param-c                                      │ ← caller1 stack-parameter area ends here
└──────────────────────────────────────────────┘

unsafe tail-jump layout for callee
┌──────────────────────────────────────────────┐
│ ret-caller1                                  │ ← stale from caller0's point of view
├──────────────────────────────────────────────┤
│ param-a                                      │
├──────────────────────────────────────────────┤
│ ret-callee                                   │ ← callee writes next to its smaller parameter area
├──────────────────────────────────────────────┤
│ param-callee                                 │ ← callee reuses only the last parameter slot
└──────────────────────────────────────────────┘
```

When `caller0` later reads `ret-caller1`, it would read stale data because `callee` wrote its stack return value into a different slot.

Result: the backend uses Path B. It emits a normal function call, but still takes advantage of `return_call` by not preserving the current
function context that becomes dead after the call, such as by spilling locals. After the callee returns, the backend moves any return values
from the callee's caller frame into the frame expected by the original caller, then returns and unwinds the current stack frame.

## Implementation Details

The dispatch can be represented as a three-dimensional matrix:

- Axis 1: whether the callee is imported.
- Axis 2: whether the callee has stack return values.
- Axis 3: how the callee's total stack-parameter width compares with the caller's total stack-parameter width.

### Layer 1: Non-imported callee

| Callee has stack return values | Callee width == caller width | Callee width < caller width | Callee width > caller width |
| --- | --- | --- | --- |
| No | Path A: tail jump | Path A: tail jump | Path B: lightweight call |
| Yes | Path A: tail jump | Path B: lightweight call | Path B: lightweight call |

### Layer 2: Imported callee

Always Path B: lightweight call
