# Extensions For WARP

Currently, there are severals kinds of extensions provided by WARP itself.

- Analytics: provide metrics for compiler generated code.
- Dwarf: provide dwarf5 formatted debug info.
- Tracing: make WARP support tracing functions.

## Tracing

For micro bench and performance analysis, we want to add new builtin API to record local time.

### How to use

This extension can be controlled by environment:

- `WARP_TRACING_RECORDER_FILE`: file path to store trace
- `WARP_TRACING_RECORDER_MAX_ITEMS`: max trace items number

The WASM module can call imported API `builtin.tracePoint` with signature `(id:i32)=>void` to add a trace.
The WARP will store the id and cpu cycle when called API. And this extension will read traces and store them to file defined by `WARP_TRACING_RECORDER_FILE`.

### Trace File

The tracing file format:

```
0x00 - 0x0f: ___WARP_TRACE___
0x10 - 0x1f: wasm identifier[u64] time point[u32] trace id[u32]
0x20 - 0x2f: wasm identifier[u64] time point[u32] trace id[u32]
...
0x?0 - 0x?f: wasm identifier[u64] time point[u32] trace id[u32]
```

### Design

#### How to get time point

in x86_64, `rdtsc` instruction can provide the current CPU cycles.
in aarch64. system register `CNTVCT_EL0` can provide the virtual count value.

#### how to convert time point to millisecond

##### aarch64

system register `CNTFRQ_EL0` can provide the frequency of the counter with `Hz` unit. `timestamp(ms) = CNTVCT_EL0 / CNTFRQ_EL0 * 1000`.

##### x86_64

a rough solution is to measure CPU frequency with rdtsc and high resolution time.

or read this information for cpuinfo

```bash
cat /proc/cpuinfo  | grep cpu  | grep Hz
```

But unfortunately, the frequencies in modern CPU are unstable and has different between each code.
If more accurate measurements are needed, **invariant tsc** must be enabled.
