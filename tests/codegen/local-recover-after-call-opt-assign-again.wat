;; no need to recover spilled local since it is assigned again.
(module
  (func $callee/0)
  ;; CHECK-LABEL: Function[1] Body
  (func $normal
    (local $arg0 i32)
    (local i32 i32 i32 i32 i32)
    i32.const 0x111
    local.set $arg0
    ;; AARCH64:       mov [[LOCAL_REG:w[0-9]+]], #0x111
    call $callee/0
    ;; AARCH64:       str [[LOCAL_REG]],  [sp, [[OFFSET:#0x[0-9]+]]]
    ;; AARCH64:       bl  0x[[#%x,FUNC_ADDR:]]
    i32.const 0x222
    local.set $arg0
    ;; AARCH64-NOT:   ldr [[LOCAL_REG]], [sp, [[OFFSET]]]
    ;; AARCH64:       mov [[LOCAL_REG]], #0x222
  )
  ;; CHECK-LABEL: Function[2] Body
  (func $block
    (local $arg0 i32)
    (local i32 i32 i32 i32 i32)
    i32.const 0x111
    local.set $arg0
    ;; AARCH64:         mov [[LOCAL_REG:w[0-9]+]], #0x111
    call $callee/0
    ;; AARCH64:         str [[LOCAL_REG]],  [sp, [[OFFSET:#0x[0-9]+]]]
    ;; AARCH64:         bl  0x[[#%x,FUNC_ADDR:]]
    block

      i32.const 0x222
      local.set $arg0
      ;; AARCH64-NOT:   ldr [[LOCAL_REG]], [sp, [[OFFSET]]]
      ;; AARCH64:       mov [[LOCAL_REG]], #0x222
    end
  )
  ;; CHECK-LABEL: Function[3] Body
  (func $br_if
    (local $arg0 i32)
    (local i32 i32 i32 i32 i32)
    i32.const 0x111
    local.set $arg0
    ;; AARCH64:         mov [[LOCAL_REG:w[0-9]+]], #0x111
    call $callee/0
    ;; AARCH64:         str [[LOCAL_REG]],  [sp, [[OFFSET:#0x[0-9]+]]]
    ;; AARCH64:         bl  0x[[#%x,FUNC_ADDR:]]

    block
      local.get 1
      br_if 0
      ;; AARCH64:       ldr [[LOCAL_REG]], [sp, [[OFFSET]]]
      ;; AARCH64:       b.ne 0x[[#%x,BLOCK_END_ADDR:]]
      i32.const 0x222
      local.set $arg0
      ;; AARCH64:       mov [[LOCAL_REG]], #0x222
    end
  )
)
