(module
    (memory 0)
    (global $g (mut i32) (i32.const 0))
    ;; CHECK-LABEL: Function[0] Body
    (func (param $ptr i32) (param $size i32)
        i32.const 0x1234
        global.set $g
        ;; AARCH64_PASSIVE:  #0x1234
        local.get $ptr
        i32.const 0
        local.get $size
        memory.fill
        ;; AARCH64_PASSIVE-NEXT:    mov  w[[REG_SIZE:[0-9]+]], [[REG:w[0-9]+]]
        ;; AARCH64_PASSIVE-NEXT:    mov  w[[REG_PTR:[0-9]+]], [[REG:w[0-9]+]]
        ;; AARCH64_PASSIVE-NEXT:    add  x[[REG_PTR]], x[[REG_PTR]], [[REG_LINEAR_MEMORY_BASE:x[0-9]+]]
        ;; AARCH64_PASSIVE-NEXT:    add  [[SCRATCH_REG:x[0-9]+]], x[[REG_PTR]], x[[REG_SIZE]]
        ;; AARCH64_PASSIVE-NEXT:    ldrb  wzr, [[[SCRATCH_REG]], #-1]!
        ;; AARCH64_PASSIVE-NEXT:    subs  w[[REG_SIZE]], w[[REG_SIZE]], #0x10
        ;; AARCH64_PASSIVE-NEXT:    b.mi  0x[[#%x,LESS_THAN_16:]]
        ;; AARCH64_PASSIVE-NEXT:    subs  w[[REG_SIZE]], w[[REG_SIZE]], #0x10
        ;; AARCH64_PASSIVE-NEXT:    stp  xzr, xzr, [x[[REG_PTR]]], #0x10
        ;; AARCH64_PASSIVE-NEXT:    b.pl  0x[[#%x,FILL_16:]]
        ;; AARCH64_PASSIVE-NEXT:    add  w[[REG_SIZE]], w[[REG_SIZE]], #0x10
        ;; AARCH64_PASSIVE-NEXT:    cbz  w[[REG_SIZE]], 0x[[#%x,FINISH:]]
        ;; AARCH64_PASSIVE-NEXT:    sub  w[[REG_SIZE]], w[[REG_SIZE]], #1
        ;; AARCH64_PASSIVE-NEXT:    strb  wzr, [x[[REG_PTR]]], #1
        ;; AARCH64_PASSIVE-NEXT:    cbnz  w[[REG_SIZE]], 0x[[#%x,COPY_1:]]
    )
)
