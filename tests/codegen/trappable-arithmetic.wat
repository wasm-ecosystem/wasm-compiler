(module
;; CHECK-LABEL: Function[0] Body
  (func
    (local i32)
    

    f32.const 0
    ;; X86_64:  cvtss2si  rbp, xmm{{[0-9]+}}
    ;; AARCH64: fcvtzu  w19, s{{[0-9]+}}
    i32.trunc_f32_u

    local.set 0
  )
;; CHECK-LABEL: Function[1] Body
  (func (result i32)
    (local i32)
    

    f32.const 0
    ;; X86_64:  cvtss2si  rax, xmm{{[0-9]+}}
    ;; AARCH64: fcvtzu  w0, s{{[0-9]+}}
    i32.trunc_f32_u

    return
  )

)
