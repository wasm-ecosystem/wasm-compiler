(module
  ;; CHECK-LABEL: Function[0] Body
  (func $eqz (param i32) (result i32)
    local.get 0
    i32.eqz
    ;; X86_64:            cmp  [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], 0
    ;; X86_64-NEXT:       mov  eax, 0
    ;; X86_64-NEXT:       sete  al
    ;; AARCH64:            cmp  [[REG:w[0-9]+]], #0
    ;; AARCH64-NEXT:       cset  w0, eq
  )
  ;; CHECK-LABEL: Function[1] Body
  (func $eq (param i32) (result i32)
    local.get 0
    i32.const 0x123
    i32.eq
    ;; X86_64:            cmp  [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], 0x123
    ;; X86_64-NEXT:       mov  eax, 0
    ;; X86_64-NEXT:       sete  al
    ;; AARCH64:            cmp  [[REG:w[0-9]+]], #0x123
    ;; AARCH64-NEXT:       cset  w0, eq
  )
  ;; CHECK-LABEL: Function[2] Body
  (func $ne (param i32) (result i32)
    local.get 0
    i32.const 0x123
    i32.ne
    ;; X86_64:            cmp  [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], 0x123
    ;; X86_64-NEXT:       mov  eax, 0
    ;; X86_64-NEXT:       setne  al
    ;; AARCH64:            cmp  [[REG:w[0-9]+]], #0x123
    ;; AARCH64-NEXT:       cset  w0, ne
  )
  ;; CHECK-LABEL: Function[3] Body
  (func $lt_s (param i32) (result i32)
    local.get 0
    i32.const 0x123
    i32.lt_s
    ;; X86_64:            cmp  [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], 0x123
    ;; X86_64-NEXT:       mov  eax, 0
    ;; X86_64-NEXT:       setl  al
    ;; AARCH64:            cmp  [[REG:w[0-9]+]], #0x123
    ;; AARCH64-NEXT:       cset  w0, lt
  )
  ;; CHECK-LABEL: Function[4] Body
  (func $lt_u (param i32) (result i32)
    local.get 0
    i32.const 0x123
    i32.lt_u
    ;; X86_64:            cmp  [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], 0x123
    ;; X86_64-NEXT:       mov  eax, 0
    ;; X86_64-NEXT:       setb  al
    ;; AARCH64:            cmp  [[REG:w[0-9]+]], #0x123
    ;; AARCH64-NEXT:       cset  w0, lo
  )
  ;; CHECK-LABEL: Function[5] Body
  (func $gt_s (param i32) (result i32)
    local.get 0
    i32.const 0x123
    i32.gt_s
    ;; X86_64:            cmp  [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], 0x123
    ;; X86_64-NEXT:       mov  eax, 0
    ;; X86_64-NEXT:       setg  al
    ;; AARCH64:            cmp  [[REG:w[0-9]+]], #0x123
    ;; AARCH64-NEXT:       cset  w0, gt
  )
  ;; CHECK-LABEL: Function[6] Body
  (func $gt_u (param i32) (result i32)
    local.get 0
    i32.const 0x123
    i32.gt_u
    ;; X86_64:            cmp  [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], 0x123
    ;; X86_64-NEXT:       mov  eax, 0
    ;; X86_64-NEXT:       seta  al
    ;; AARCH64:            cmp  [[REG:w[0-9]+]], #0x123
    ;; AARCH64-NEXT:       cset  w0, hi
  )
  ;; CHECK-LABEL: Function[7] Body
  (func $le_s (param i32) (result i32)
    local.get 0
    i32.const 0x123
    i32.le_s
    ;; X86_64:            cmp  [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], 0x123
    ;; X86_64-NEXT:       mov  eax, 0
    ;; X86_64-NEXT:       setle  al
    ;; AARCH64:            cmp  [[REG:w[0-9]+]], #0x123
    ;; AARCH64-NEXT:       cset  w0, le
  )
  ;; CHECK-LABEL: Function[8] Body
  (func $le_u (param i32) (result i32)
    local.get 0
    i32.const 0x123
    i32.le_u
    ;; X86_64:            cmp  [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], 0x123
    ;; X86_64-NEXT:       mov  eax, 0
    ;; X86_64-NEXT:       setbe  al
    ;; AARCH64:            cmp  [[REG:w[0-9]+]], #0x123
    ;; AARCH64-NEXT:       cset  w0, ls
  )
  ;; CHECK-LABEL: Function[9] Body
  (func $ge_s (param i32) (result i32)
    local.get 0
    i32.const 0x123
    i32.ge_s
    ;; X86_64:            cmp  [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], 0x123
    ;; X86_64-NEXT:       mov  eax, 0
    ;; X86_64-NEXT:       setge  al
    ;; AARCH64:            cmp  [[REG:w[0-9]+]], #0x123
    ;; AARCH64-NEXT:       cset  w0, ge
  )
  ;; CHECK-LABEL: Function[10] Body
  (func $ge_u (param i32) (result i32)
    local.get 0
    i32.const 0x123
    i32.ge_u
    ;; X86_64:            cmp  [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], 0x123
    ;; X86_64-NEXT:       mov  eax, 0
    ;; X86_64-NEXT:       setae  al
    ;; AARCH64:            cmp  [[REG:w[0-9]+]], #0x123
    ;; AARCH64-NEXT:       cset  w0, hs
  )
)
