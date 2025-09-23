(module
  ;; CHECK-LABEL: Function[0] Body
  (func $br_if_function (param i32 i32 i32 i32 i32)
    ;; AARCH64:      sub  sp, sp, #0x[[#%x,STACK_SIZE:]]
    local.get 0
    i32.eqz
    br_if 0
    ;; AARCH64:      cbnz  [[REG:w[0-9]+]],
    ;; AARCH64:      add  sp, sp, #0x[[#%x,STACK_SIZE]]
    ;; AARCH64-NEXT: ret
    unreachable
    ;; disable normal return
  )
)

