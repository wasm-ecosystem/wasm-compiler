;; Test X86-64 div shouldn't spill eax register when operand is in eax
(module
  ;; CHECK-LABEL: Function[0] Body    
  (func (param i32) (result i32)

    local.get 0
    i32.const 1
    i32.add
    ;; X86_64: add eax, 1
    ;; X86_64-NOT: mov [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], eax
    ;; X86_64: idiv [[REG2:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]]
    
    local.get 0
    i32.div_s
    return
  )
)