(module
  (func (export "add-low-tricore-imm") (param i64) (result i64)
    local.get 0
    i64.const 0xF0F0F0F000000001
    i64.add
  )

  (func (export "eq-both-tricore-imm") (result i32)
    i64.const 0
    i64.const 1
    i64.eq
  )

  (func (export "eq-low-tricore-imm") (param i64) (result i32)
    local.get 0
    i64.const 0xF0F0F0F000000001
    i64.eq
  )

   (func (export "ne-low-tricore-imm") (param i64) (result i32)
    local.get 0
    i64.const 0xF0F0F0F000000001
    i64.ne
  )

  (func (export "eq-high-tricore-imm") (param i64) (result i32)
    local.get 0
    i64.const 0x00000001F0F0F0F0
    i64.eq
  )

  (func (export "ne-high-tricore-imm") (param i64) (result i32)
    local.get 0
    i64.const 0x00000001F0F0F0F0
    i64.ne
  )

  (func (export "and-high-tricore-imm-op1") (param i64) (result i64)
    local.get 0
    i64.const 0x00000001F0F0F0F0
    i64.and
  )

  (func (export "and-high-tricore-imm-op2") (param i64) (result i64)
    i64.const 0x00000001F0F0F0F0
    local.get 0
    i64.and
  )

  (func (export "and-low-tricore-imm-op1") (param i64) (result i64)
    local.get 0
    i64.const 0xF0F0F0F000000001
    i64.and
  )

  (func (export "and-low-tricore-imm-op2") (param i64) (result i64)
    i64.const 0xF0F0F0F000000001
    local.get 0
    i64.and
  )

  (func (export "or-high-tricore-imm-op1") (param i64) (result i64)
    local.get 0
    i64.const 0x00000001F0F0F0F0
    i64.or
  )

  (func (export "or-high-tricore-imm-op2") (param i64) (result i64)
    i64.const 0x00000001F0F0F0F0
    local.get 0
    i64.or
  )

  (func (export "or-low-tricore-imm-op1") (param i64) (result i64)
    local.get 0
    i64.const 0xF0F0F0F000000001
    i64.or
  )

  (func (export "or-low-tricore-imm-op2") (param i64) (result i64)
    i64.const 0xF0F0F0F000000001
    local.get 0
    i64.or
  )

  (func (export "xor-high-tricore-imm-op1") (param i64) (result i64)
    local.get 0
    i64.const 0x00000001F0F0F0F0
    i64.xor
  )

  (func (export "xor-high-tricore-imm-op2") (param i64) (result i64)
    i64.const 0x00000001F0F0F0F0
    local.get 0
    i64.xor
  )

  (func (export "xor-low-tricore-imm-op1") (param i64) (result i64)
    local.get 0
    i64.const 0xF0F0F0F000000001
    i64.xor
  )

  (func (export "xor-low-tricore-imm-op2") (param i64) (result i64)
    i64.const 0xF0F0F0F000000001
    local.get 0
    i64.xor
  )
)

(assert_return (invoke "add-low-tricore-imm" (i64.const 1)) (i64.const 0xF0F0F0F000000002))
(assert_return (invoke "eq-both-tricore-imm") (i32.const 0))
(assert_return (invoke "eq-low-tricore-imm" (i64.const 1)) (i32.const 0))
(assert_return (invoke "eq-low-tricore-imm" (i64.const 0xF0F0F0F000000001)) (i32.const 1))
(assert_return (invoke "ne-low-tricore-imm" (i64.const 1)) (i32.const 1))
(assert_return (invoke "ne-low-tricore-imm" (i64.const 0xF0F0F0F000000001)) (i32.const 0))
(assert_return (invoke "eq-high-tricore-imm" (i64.const 1)) (i32.const 0))
(assert_return (invoke "eq-high-tricore-imm" (i64.const 0x00000001F0F0F0F0)) (i32.const 1))
(assert_return (invoke "ne-high-tricore-imm" (i64.const 1)) (i32.const 1))
(assert_return (invoke "ne-high-tricore-imm" (i64.const 0x00000001F0F0F0F0)) (i32.const 0))
(assert_return (invoke "and-high-tricore-imm-op1" (i64.const 0)) (i64.const 0))
(assert_return (invoke "and-high-tricore-imm-op2" (i64.const 0)) (i64.const 0))
(assert_return (invoke "and-low-tricore-imm-op1" (i64.const 0)) (i64.const 0))
(assert_return (invoke "and-low-tricore-imm-op2" (i64.const 0)) (i64.const 0))
(assert_return (invoke "or-high-tricore-imm-op1" (i64.const 0)) (i64.const 0x00000001F0F0F0F0))
(assert_return (invoke "or-high-tricore-imm-op2" (i64.const 0)) (i64.const 0x00000001F0F0F0F0))
(assert_return (invoke "or-low-tricore-imm-op1" (i64.const 0)) (i64.const 0xF0F0F0F000000001))
(assert_return (invoke "or-low-tricore-imm-op2" (i64.const 0)) (i64.const 0xF0F0F0F000000001))
(assert_return (invoke "xor-high-tricore-imm-op1" (i64.const 1)) (i64.const 0x1F0F0F0F1))
(assert_return (invoke "xor-high-tricore-imm-op2" (i64.const 1)) (i64.const 0x1F0F0F0F1))
(assert_return (invoke "xor-low-tricore-imm-op1" (i64.const 1)) (i64.const 0xF0F0F0F000000000))
(assert_return (invoke "xor-low-tricore-imm-op2" (i64.const 1)) (i64.const 0xF0F0F0F000000000))

