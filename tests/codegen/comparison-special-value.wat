(module
  ;; CHECK-LABEL: Function[0] Body
  (func $lt_u/0 (param i32) (result i32)
    local.get 0
    i32.const -1
    i32.lt_u
    ;; -1 cannot be encoded in unsigned immediate number
    ;; TRICORE:      mov  [[IMM_REG:d[0-9]+]], #-1
    ;; TRICORE-NEXT: lt.u  [[DST_REG:d[0-9]+]], [[LOCAL_REG:d[0-9]+]], [[IMM_REG]]
  )
  ;; CHECK-LABEL: Function[1] Body
  (func $lt_s/0 (param i32) (result i32)
    local.get 0
    i32.const -1
    i32.lt_s
    ;; -1 can be encoded in signed immediate number
    ;; TRICORE:      lt  [[DST_REG:d[0-9]+]], [[LOCAL_REG:d[0-9]+]], #-1
  )
)
