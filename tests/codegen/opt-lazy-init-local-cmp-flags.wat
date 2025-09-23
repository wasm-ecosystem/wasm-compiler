(module
  ;; CHECK-LABEL: Function[0] Body
  (func $lazy-init-pres-flags (param i32)
    (local i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    local.get 0
    i32.const 0x222
    i32.eq
    ;; X86_64:        cmp    [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], 0x222

    ;; lazy init
    ;; X86_64-NOT:    and

    ;; X86_64:        jne
    if
    end
  )
)
