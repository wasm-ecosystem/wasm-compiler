(module
    (memory 0)
    (global $g (mut i32) (i32.const 0))
    ;; CHECK-LABEL: Function[0] Body
    (func (param $ptr i32)
        local.get $ptr
        i32.const 0
        i32.store
        ;; AARCH64_PASSIVE:         ldr  wzr, [[[LINEAR_MEM_BASE:x[0-9]+]], [[PTR:x[0-9]+]]]
        ;; AARCH64_PASSIVE-NEXT:    str  wzr, [[[LINEAR_MEM_BASE]], [[PTR]]]
        ;; AARCH64_ACTIVE:          str  wzr, [[[LINEAR_MEM_BASE:x[0-9]+]], [[PTR:x[0-9]+]]]
    )
    ;; CHECK-LABEL: Function[1] Body
    (func (param $ptr i32)
        local.get $ptr
        i64.const 0
        i64.store
        ;; AARCH64_PASSIVE:         ldr  xzr, [[[LINEAR_MEM_BASE:x[0-9]+]], [[PTR:x[0-9]+]]]
        ;; AARCH64_PASSIVE-NEXT:    str  xzr, [[[LINEAR_MEM_BASE]], [[PTR]]]
        ;; AARCH64_ACTIVE:          str  xzr, [[[LINEAR_MEM_BASE:x[0-9]+]], [[PTR:x[0-9]+]]]
    )
    ;; CHECK-LABEL: Function[2] Body
    (func (param $ptr i32)
        local.get $ptr
        f32.const 0x0p+0
        f32.store
        ;; AARCH64_PASSIVE:         ldr  wzr, [[[LINEAR_MEM_BASE:x[0-9]+]], [[PTR:x[0-9]+]]]
        ;; AARCH64_PASSIVE-NEXT:    str  wzr, [[[LINEAR_MEM_BASE]], [[PTR]]]
        ;; AARCH64_ACTIVE:          str  wzr, [[[LINEAR_MEM_BASE:x[0-9]+]], [[PTR:x[0-9]+]]]
    )
    ;; CHECK-LABEL: Function[3] Body
    (func (param $ptr i32)
        local.get $ptr
        f64.const 0x0p+0
        f64.store
        ;; AARCH64_PASSIVE:         ldr  xzr, [[[LINEAR_MEM_BASE:x[0-9]+]], [[PTR:x[0-9]+]]]
        ;; AARCH64_PASSIVE-NEXT:    str  xzr, [[[LINEAR_MEM_BASE]], [[PTR]]]
        ;; AARCH64_ACTIVE:          str  xzr, [[[LINEAR_MEM_BASE:x[0-9]+]], [[PTR:x[0-9]+]]]
    )
)
