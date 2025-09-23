(module

  (func $foo  (result f64)
    f64.const 1
    
    f64.const 2
    
    i32.const 22601
    select
    f64.const 3
    
    f64.const 4
    
    i32.const 134217728
    select
    i32.const 134217728
    select
  )
  (export "foo" (func $foo))
)

(assert_return (invoke "foo") (f64.const 1.0))