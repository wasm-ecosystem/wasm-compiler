(module

  (func $foo (result i32)
    (local i32)
    i32.const 1
    local.set 0

    i32.const 1
    
    i32.const 2
    local.get 0
    i32.add
    
    i32.const 268435457
    local.tee 0
    
    select
  )
  (export "foo" (func $foo))
)
(assert_return (invoke "foo") (i32.const 1))