(module
  (import "spectest" "func-i64-i64" (func $1 (param i64 i64)))
  (func (export "overwritten") (result i32)
    (local i32 i32 i32 i32 i32 i32 i32)
    i32.const 1234
    local.set 1

    i64.const 1
    i64.const 2
    call $1

    local.get 1
  )
)

(assert_return (invoke "overwritten") (i32.const 1234))
