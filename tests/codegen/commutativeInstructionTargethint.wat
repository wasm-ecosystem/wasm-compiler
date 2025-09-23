(module

 ;; CHECK-LABEL: Function[0] Body
  (func (result i32)
    (local i32)
    i32.const 1
    i32.const 2
    i32.add
    

    i32.const 4
    i32.const 5

    i32.add

    ;; X86_64:            add eax, [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]]
    i32.add
    
    return 
  )

  (memory 1 100)

)