(module

  (func $goo (param i32 i32  i32 i32 i32 i32 i32  i32 i32 i32) (result i32)
   
    local.get 9
    return)
  
  (func $foo (result i32)
    i32.const 1
    i32.const 2
    i32.const 3
    i32.const 4
    i32.const 5
    i32.const 6
    i32.const 7
    i32.const 8
    i32.const 9

    i32.const 100
    i32.const 200
    i32.add
    call $goo
    )
  (export "foo" (func $foo))
 )

(assert_return (invoke "foo") (i32.const 300))

(module

  (func $foo (result i64)
    (local i64)
    i64.const 100
    local.set 0
    local.get 0

    
    i64.const 200 
    call $callee
    
    
    drop
    
    call $callee
    )
  
  (func $callee (param i64) (result i64)
    local.get 0
    )

  (export "func_0_invoker" (func $foo))

)
(assert_return (invoke "func_0_invoker") (i64.const 100))


(module

  (func $foo  (param i64 i32  i32) (result i32)
    (local i64)
      local.get 0
      local.get 2
      local.get 1
      call $goo

    )
  

   (func $goo (param i64 i32  i32) (result i32)
    (local i64)
      local.get 1
    )

  (export "func_invoker" (func $foo))

)
(assert_return (invoke "func_invoker" (i64.const 100) (i32.const 200) (i32.const 300)) (i32.const 300))

(module
  (func $goo (param $0 i32) (param $1 i32) (param $2 i32)  (result i32)
      local.get $0
  )
  (func $foo (result i32)
    (local $0 i32)
    (local $1 i32)
    (local $2 i32)
    (local $3 i32)
    i32.const 5
    local.set $0
    i32.const 1
    local.set $1
    i32.const 2
    local.set $2

    local.get $2
    local.get $0
    local.get $1

    call $goo
  )

  (export "foo" (func $foo))
)

(assert_return (invoke "foo") (i32.const 2))

(module


  (func $callee-float (param f32 f64) (result f64)
    local.get 1
   )

  (func $caller-float (result f64)
    (local f64 f32)

    f64.const 1
    local.set 0

    f32.const 2
    local.set 1
   
    local.get 1
    local.get 0
    call $callee-float

    )

  (export "caller-float" (func $caller-float))

    (func $callee-int (param i32 i64) (result i64)
    local.get 1
   )

  (func $caller-int (result i64)
    (local i64 i32)

    i64.const 0xFFFFFFFFFF
    local.set 0

    i32.const 2
    local.set 1
   
    local.get 1
    local.get 0
    call $callee-int

    )

  (export "caller-int" (func $caller-int))
)

(assert_return (invoke "caller-float") (f64.const 1))
(assert_return (invoke "caller-int") (i64.const 0xFFFFFFFFFF))

(module
  (type (;1;) (func (param i32 ) (result i32)))

  (func $goo (param i32) (result i32)
    local.get 0
  )
  (func $foo (result i32)
    (local i32 )
    i32.const 1
    local.set 0

    i32.const 100
    local.get 0
    if (result i32)  ;; label = @3
      local.get 0
    else
      i32.const 1
    end
    i32.const 10
    select


    local.get 0
    call_indirect (type 0)
    
    
      
  )
  (table (;0;) 2 2 funcref)

  (export "foo" (func $foo))
  (elem (;0;) (i32.const 0) func $goo $goo)
)
(assert_return (invoke "foo") (i32.const 100))

(module
  (type (;1;) (func (result i32)))
  
  (func $callee  (result i32)
   i32.const 1 )
   
  (func $foo (result i32)
    
      (local i64 i32)
      i32.const 0
      local.set 1

      i64.const 1
      local.set 0
    

      local.get 0
      local.get 1
      i64.load16_s offset=4 align=1
      i64.eq
      call_indirect (type 0)
    
  )
  
  (table (;0;) 2 funcref)
  (memory (;0;) 16 17)
  (export "foo" (func $foo))

  (elem (;0;) (i32.const 0) func $callee)
  (data (;0;) (i32.const 3) "6"))

(assert_return (invoke "foo" )(i32.const 1))
