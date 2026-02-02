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

(module
    ;; CHECK-LABEL: Function[0] Body
    (func $goo (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
                local.get 0
        local.get 1
        i32.add
        local.get 2
        i32.add
        local.get 3
        i32.add
        local.get 4
        i32.add
        local.get 5
        i32.add
        local.get 6
        i32.add
        local.get 7
        i32.add
        local.get 8
        i32.add
        local.get 9
        i32.add
        local.get 10
        i32.add
    )
    ;; CHECK-LABEL: Function[1] Body
    (func $foo (export "foo") (result i32)
        (local $l0 i32) (local $l1 i32) (local $l2 i32) (local $l3 i32) (local $l4 i32) (local $l5 i32) (local $l6 i32) (local $l7 i32) (local $l8 i32) (local $l9 i32)
        (local i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
        i32.const 0
        local.set 0
        i32.const 1
        local.set 1
        i32.const 2
        local.set 2
        i32.const 3
        local.set 3
        i32.const 4
        local.set 4
        i32.const 5
        local.set 5
        i32.const 6
        local.set 6
        i32.const 7
        local.set 7
        i32.const 8
        local.set 8
        i32.const 9
        local.set 9
        i32.const 100
        i32.const 100
        i32.store
        block (result i32)
        i32.const 100
        i32.load

        local.get 0
        local.get 1
        local.get 2
        local.get 3
        local.get 4
        local.get 5
        local.get 6
        local.get 7
        

        local.get 1
        local.get 1
        i32.add

        local.get 1
        local.get 1
        i32.add

        
        local.get 1
        local.get 1
        i32.add

        
        local.get 1
        local.get 1
        i32.add

       

        local.get 1
        local.get 1
        i32.add

        local.get 1
        local.get 1
        i32.add

        local.get 1
        local.get 1
        i32.add

        i32.add

        i32.add

        i32.add

        i32.add

        i32.add
        i32.add

        i32.const 1
        local.get 2
        i32.add

        i32.const 1
        local.get 2
        i32.add

        call $goo

        i32.add

        end
    )
    (memory 1)
)
(assert_return (invoke "foo") (i32.const 148))

(module

  (func $foo  (result i32  i64 i64 )
    (local f64)
    (local i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    i32.const 0
    local.set 1
    i32.const 0
    local.set 2
    i32.const 0
    local.set 3
    i32.const 0
    local.set 4
    i32.const 0
    local.set 5
    i32.const 0
    local.set 6
    i32.const 0
    local.set 7
    i32.const 0
    local.set 8
    i32.const 0
    local.set 9
    i32.const 0
    local.set 10
    i32.const 0
    local.set 11
    i32.const 0
    local.set 12
    i32.const 0
    local.set 13
    i32.const 0
    local.set 14
    i32.const 0
    local.set 15
    i32.const 0
    local.set 16
    i32.const 0
    local.set 17
    i32.const 0
    local.set 18
    i32.const 0
    local.set 19
    i32.const 0
    local.set 20

    f64.const 0
    local.set 0

    block (result i32 i64 i64 )  ;; label = @1
      
      i32.const 672608812

      i64.const 6727
      i64.const 87
      
      local.get 0
      i32.trunc_f64_u
      i32.const 7
      i32.rem_u
      br_if 0 (;@1;)
    end)

  (export "func_16" (func $foo))

)

(assert_return (invoke "func_16") (i32.const 672608812) (i64.const 6727) (i64.const 87))


(module

  (import "spectest" "func-i32-i32" (func (param i32 i32) (result i32)))

  (func $target-reg-is-res-scratch-reg-tricore (param i32) (param i64) (result i32)
    (local i64 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    
    local.get 0

    local.get 1
    i64.const 2
    i64.mul
    i32.wrap_i64
    call 0
  )
  (export "target-reg-is-res-scratch-reg-tricore" (func $target-reg-is-res-scratch-reg-tricore))
)

(assert_return (invoke "target-reg-is-res-scratch-reg-tricore" (i32.const 1) (i64.const 2)) (i32.const 5))
