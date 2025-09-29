(module
  ;; CHECK-LABEL: Function[0] Body
  (func $select-from-local (param i32 i32 i32) (result i32)
    i32.const 0x1000
    local.set 0
    ;; TRICORE: mov.u  [[L0:d[0-9]+]], #0x1000
    i32.const 0x1001
    local.set 1
    ;; TRICORE: mov.u  [[L1:d[0-9]+]], #0x1001
    i32.const 0x1002
    local.set 2
    ;; TRICORE: mov.u  [[L2:d[0-9]+]], #0x1002
    local.get 0
    local.get 1
    local.get 2
    select
    ;; TRICORE: sel  d2, [[L2]], [[L0]], [[L1]]
  )
  (func $select-from-stack (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    i32.const 0x0
    local.set 10
    ;; TRICORE:         mov.a a12, #0
    ;; TRICORE-NEXT:    st.a [[L10:\[sp\]#0x[0-9a-f]+]], a12
    i32.const 0x1
    local.set 11
    ;; TRICORE:         mov.a a12, #1
    ;; TRICORE-NEXT:    st.a [[L11:\[sp\]#0x[0-9a-f]+]], a12
    i32.const 0x2
    local.set 12
    ;; TRICORE:         mov.a a12, #2
    ;; TRICORE-NEXT:    st.a [[L12:\[sp\]#0x[0-9a-f]+]], a12
    local.get 10
    local.get 11
    local.get 12
    select
    ;; TRICORE-DAG:     ld.w  d2, [[L12]]
    ;; TRICORE-DAG:     ld.w  [[L10_REG:d[0-9]+]], [[L10]]
    ;; TRICORE-DAG:     ld.w  [[L11_REG:d[0-9]+]], [[L11]]
    ;; TRICORE:         sel   d2, d2, [[L10_REG]], [[L11_REG]]
  )
  (func $truthy-is-target-hint (param i32 i32 i32) (result i32)
    i32.const 0x1000
    local.set 0
    ;; TRICORE: mov.u  [[L0:d[0-9]+]], #0x1000
    i32.const 0x1001
    local.set 1
    ;; TRICORE: mov.u  [[L1:d[0-9]+]], #0x1001
    i32.const 0x1002
    local.set 2
    ;; TRICORE: mov.u  [[L2:d[0-9]+]], #0x1002
      local.get 0
      i32.const 1
    i32.add
    ;; TRICORE: addi  d2, [[L0]], #1
    local.get 1
    local.get 2
    select
    ;; TRICORE: sel  d2, [[L2]], d2, [[L1]]
  )
  (func $falsy-is-target-hint (param i32 i32 i32) (result i32)
    i32.const 0x1000
    local.set 0
    ;; TRICORE: mov.u  [[L0:d[0-9]+]], #0x1000
    i32.const 0x1001
    local.set 1
    ;; TRICORE: mov.u  [[L1:d[0-9]+]], #0x1001
    i32.const 0x1002
    local.set 2
    ;; TRICORE: mov.u  [[L2:d[0-9]+]], #0x1002
    local.get 0
      local.get 1
      i32.const 1
    i32.add
    ;; TRICORE: addi  d2, [[L1]], #1
    local.get 2
    select
    ;; TRICORE: sel  d2, [[L2]], [[L0]], d2
  )
  (func $cond-is-target-hint (param i32 i32 i32) (result i32)
    i32.const 0x1000
    local.set 0
    ;; TRICORE: mov.u  [[L0:d[0-9]+]], #0x1000
    i32.const 0x1001
    local.set 1
    ;; TRICORE: mov.u  [[L1:d[0-9]+]], #0x1001
    i32.const 0x1002
    local.set 2
    ;; TRICORE: mov.u  [[L2:d[0-9]+]], #0x1002
    local.get 0
    local.get 1
      local.get 2
      i32.const 1
    i32.add
    ;; TRICORE: addi  d2, [[L2]], #1
    select
    ;; TRICORE: sel  d2, d2, [[L0]], [[L1]]
  )
)
