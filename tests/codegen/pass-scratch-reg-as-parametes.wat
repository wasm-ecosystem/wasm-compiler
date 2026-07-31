(module
  (func $callee-ii/0 (param i32 i32)
  )
  ;; CHECK-LABEL: Function[1] Body
  (func $caller/1
    (param i32)
    i32.const 1
    i32.ctz
    local.get 0
    ;; X86_64:        xchg  [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]]
    ;; AARCH64:  mov  [[REG:w[0-9]+]], w8
    ;; AARCH64-NEXT:  mov  w8, w19
    ;; AARCH64-NEXT:  mov  w19, [[REG]]

    ;; TRICORE: mov [[REG:d[0-9]+]], d9
    ;; TRICORE-NEXT: mov d9, d8
    ;; TRICORE-NEXT: mov d8, [[REG]]
    call $callee-ii/0
  )
)
