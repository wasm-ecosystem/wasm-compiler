(module
  (func $callee-ii/0 (param i32 i32)
  )
  ;; CHECK-LABEL: Function[1] Body
  (func $caller/1
    (param i32)
    i32.const 1
    i32.load
    local.get 0
    ;; X86_64:        xchg  [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]]
    ;; AARCH64:  eor  [[REG1:w[0-9]+]], [[REG1]], [[REG2:w[0-9]+]]
    ;; AARCH64-NEXT:  eor [[REG2]], [[REG1]], [[REG2]]
    ;; AARCH64-NEXT:  eor  [[REG1]], [[REG1]], [[REG2]]

    ;; TRICORE: xor [[REG1:d[0-9]+]], [[REG2:d[0-9]+]]
    ;; TRICORE-NEXT: xor [[REG2]], [[REG1]]
    ;; TRICORE-NEXT: xor [[REG1]], [[REG2]]
    call $callee-ii/0
  )
  (memory 1)
)
