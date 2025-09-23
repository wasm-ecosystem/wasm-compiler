(module
    (memory 0)
    (global $g (mut i32) (i32.const 0))
    ;; CHECK-LABEL: Function[0] Body
    (func (param $ptr i32) (param $v1 i32) (param $v2 i32)
        local.get $ptr
        i32.const 0
        i32.store
        ;; AARCH64_PASSIVE:             ldr  wzr, [[[LINEAR_MEM_BASE:x[0-9]+]], [[PTR:x[0-9]+]]]
        ;; AARCH64_PASSIVE:             str  wzr, [[[LINEAR_MEM_BASE:x[0-9]+]], [[PTR]]]

        local.get $ptr
        i32.const 0
        i32.store8
        ;; AARCH64_PASSIVE-NOT:         ldrb  wzr, [[[LINEAR_MEM_BASE]], [[PTR]]]
        ;; AARCH64_PASSIVE:             strb  wzr, [[[LINEAR_MEM_BASE]], [[PTR]]]
        local.get $ptr
        i64.const 0
        i64.store8
        ;; AARCH64_PASSIVE-NOT:         ldrb  wzr, [[[LINEAR_MEM_BASE]], [[PTR]]]
        ;; AARCH64_PASSIVE:             strb  wzr, [[[LINEAR_MEM_BASE]], [[PTR]]]
    )
)
