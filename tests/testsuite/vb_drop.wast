(module

  (func $foo (result i32)
    (local i32)
    i32.const 1
    local.set 0

    i32.const 2
    local.get 0
    i32.add

    i32.const 3
    i32.xor
    drop
    i32.const 10


  )

  (func
    (local i32 )

    i32.const 1
    local.set 0
    
    local.get 0
    if
      i64.const 1
      i64.const 2
      i64.add
      drop
    else
      i32.const 0
      drop
    end
    
  )
  (export "foo" (func $foo))
)
(assert_return (invoke "foo") (i32.const 10))
