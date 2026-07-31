# ParallelMoveResolver

Core design: resolve parallel copies between registers and stack slots without overwriting live sources.

1. Store each move as `(target, source, targetType)` in a array.
2. Build source-usage maps: per-register counters and per-stack-slot counters.
3. Greedy phase: emit any move whose target is not currently used as an unresolved source.
4. After emitting a move, decrement usage for its source and invalidate that record.
5. Repeat greedy emission until no more safe moves exist.
6. If unresolved moves remain, they form at least one cycle.
7. Cycle handling depends on which `resolve` overload is used: temp-based resolution or swap-based resolution.
8. Temp-based resolution breaks one cycle with a backend-provided temp location: `temp <- cycleHead.source`, follows records whose target equals the newly freed location, then closes the cycle with `cycleHead.target <- temp`.
9. Swap-based resolution asks the backend to swap each `target`/`source` pair along the cycle and retires records as their original target value is consumed.
10. TriCore extension: `Extend` and `Extend_Placeholder` records are retired together so paired-register accounting stays correct.

## Why always have a free same-type temp register for return_call parallel moves

1. Target is parameters that definitely not scratch register. The out-degree is 0, therefore it is definitely not on the cycle.
1. All locals that not related to callee parameter are useless in the return_call.(Not `move-record`)
1. So, all scratch registers are idle when processing cycle.

## Why always have a free same-type temp register for wasm internal call parallel moves

1. Always spill all scratch registers beside parameters before call. So only `move-record` can use scratch registers.
1. Target is parameters that definitely not scratch register. The out-degree is 0, therefore it is definitely not on the cycle.
1. So, all scratch registers are idle when processing cycle.

## Why always have a free same-type temp register for import adapter parallel moves

1. All scratch registers are natively available.
