(module
  (func (;0;) (result i64)
    (local i32)
    i32.const 11
    i32.load8_u
    i32.const 32777
    i32.load8_u
    i32.const 9
    i32.load8_u
    i32.const 8
    i32.load8_u
    i32.const 7
    i32.load8_u
    i32.const 5
    i32.load8_u
    i32.const 4
    i32.load8_u
    i32.const 3
    i32.load8_u
    i32.const 2
    i32.load8_u
    i32.const 1
    i32.load8_u
    i32.const 0
    i32.load8_u
    i32.const 5381
    i32.xor
    local.tee 0
    i32.xor
    i32.const 3
    i64.load
    i32.const 3
    i64.load
    i32.const 1
    select
    return
  )

  (memory (;0;) 1 1)
  (export "hashMemory" (func 0))
  (data (;0;) (i32.const 0) "RF\7f")
)