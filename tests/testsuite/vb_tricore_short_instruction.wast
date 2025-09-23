(module
  (func (export "eq-tricore-16-bit-instruction") (param i32) (result i32) (local i32 i32 i32 i32 i32 i32 i32 i32 i32)
    local.get 9
    i32.const 0
    i32.eq
    local.set 9
    local.get 9
    local.get 0
    i32.eq
    local.set 9
    local.get 9
  )

  (func (export "lt-tricore-16-bit-instruction") (param i32) (result i32) (local i32 i32 i32 i32 i32 i32 i32 i32 i32)
    local.get 9
    i32.const 0
    i32.lt_s
    local.set 9
    local.get 9
    local.get 0
    i32.lt_s
    local.set 9
    local.get 9
  )

  (func (export "or-tricore-16-bit-instruction") (result i32) (local i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    i32.const 0xFF
    local.set 9
    local.get 9
    i32.const 0
    i32.or
    local.set 9
    local.get 9
  )

  (func (export "and-tricore-16-bit-instruction") (result i32) (local i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    i32.const 0xFF
    local.set 9
    local.get 9
    i32.const 0
    i32.and
    local.set 9
    local.get 9
  )
)


(assert_return (invoke "eq-tricore-16-bit-instruction" (i32.const 1)) (i32.const 1))
(assert_return (invoke "lt-tricore-16-bit-instruction" (i32.const 1)) (i32.const 1))
(assert_return (invoke "or-tricore-16-bit-instruction") (i32.const 0xFF))
(assert_return (invoke "and-tricore-16-bit-instruction") (i32.const 0))