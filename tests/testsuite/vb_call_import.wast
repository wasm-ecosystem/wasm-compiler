(module
  (import "spectest" "print_i64" (func (param i64)))

  (func (param i64 i64 i64 i64 i64 i64 ) (result i64)
    (local i32 i32 i32 f64 i64)
      local.get 0
      call 0
      local.get 10
    )
  (func (result i64)
    i64.const 1
    i64.const 1
    i64.const 1
    i64.const 1
    i64.const 1
    i64.const 1
    call 1
    )
  (export "func_invoker" (func 2))
)

(assert_return (invoke "func_invoker") (i64.const 0))
