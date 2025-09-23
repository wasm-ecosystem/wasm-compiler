(module
  (func $foo (result i32)
     
    i32.const 0
    if (result i32 i32)  ;; label = @2
      i32.const 0
      if (result i32 i32)  ;; label = @3
        i32.const 1
        i32.const -32767
      else
          i32.const 100
          return
      end
    else
      i32.const 1
      i32.const 1361069348
    end
    drop
    drop
    
    i32.const 200
    )

  (export "func" (func $foo))

)
(assert_return (invoke "func") (i32.const 200))