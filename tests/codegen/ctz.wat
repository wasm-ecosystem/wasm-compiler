(module
  ;; CHECK-LABEL: Function[0] Body
  (func $ctz/0 (param i32) (result i32)
    local.get 0
    i32.ctz
    ;; TRICORE:      shuffle  [[DST_REG:d[0-9]+]], [[SRC_REG:d[0-9]+]], #-0xe5
    ;; TRICORE-NEXT: clz  [[DST_REG]], [[DST_REG]]
  )
)
