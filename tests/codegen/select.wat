(module
  ;; CHECK-LABEL: Function[0] Body
  (func (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
        i32.const 0
        local.get 10
        local.get 1
      ;; X86_64: cmove  [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], {{dword ptr \[.*\]|r[a-z0-9]+}}
      select
    local.set 10
  )
)