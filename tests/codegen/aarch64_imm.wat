(module
  
  ;; CHECK-LABEL: Function[0] Body
  (func (result i64)
    ;; AARCH64: mov x0,  #-0x100000000
    ;; AARCH64-NOT: mov
    ;; AARCH64: ret
    i64.const 0xFFFFFFFF00000000
    return
  )
)