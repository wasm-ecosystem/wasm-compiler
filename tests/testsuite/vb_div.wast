(module
 

  (func $foo (result i32)
    (local i32)

    i32.const 100
    local.set 0
    
    local.get 0

    local.get 0
    i32.const 0x2
    local.tee 0
    i32.div_s

    i32.add)

  (func $goo   (result i64)

    i32.const 1

    i64.extend_i32_s
    block (result i64)  ;; label = @1
      i64.const 100
    end
    i64.rem_u)

  

  
  (export "goo" (func $goo))

 
  (export "foo" (func $foo))
)
(assert_return (invoke "foo") (i32.const 150))
(assert_return (invoke "goo") (i64.const 1))

(module
  (func (export "div-targetHint-mem") (result i64)
    (local i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    (local $l1 i32)
    (local $l2 i64)
    i32.const 2
    local.set $l1
   
    local.get $l1
    local.get $l1
    i32.const 2
    i32.eq
    
    i32.div_u
    i64.extend_i32_u
    local.tee $l2
   )
)

(assert_return (invoke "div-targetHint-mem") (i64.const 2))