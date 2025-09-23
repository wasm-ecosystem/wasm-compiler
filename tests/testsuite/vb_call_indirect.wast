(module
  (type (func (param i64) (result i64)))

  (func (param i64) (result i64)
    
    local.get 0)
  (func (;3;) (result i64)
   

    i64.const 123
    
    i32.const 0
    i32.const 0
    i32.add
    call_indirect (type 0)
    )
  
  (table 1 funcref)
  
  (global (mut i32) (i32.const 10))

  (export "func_0" (func 1))
  (elem (i32.const 0) func 0)
)

(assert_return (invoke "func_0") (i64.const 123))

(module
  (type (;0;) (func))
  (type (;1;) (func (param i32 i64 i32 i64) (result i64)))
  
  (func $goo (param i32 i64 i32 i64) (result i64)
    
    i64.const 0)

  (func (export "foo") (result i64)
    (local i64 i32 i32)
    i64.const 0
    local.set 0
    i32.const 0
    local.set 1
    i32.const 0
    local.set 2
    

    i32.const 0
    i32.const 0
    i32.add

    local.get 0
    i64.eqz
    if  (result i64)
      i64.const 1045965887
    else
       i64.const 0
    end
    local.get 1
    local.get 0
    local.get 1
    call_indirect (type 1)
    return
    )
  (table (;0;) 3 funcref)
  (elem (;0;) (i32.const 0) func $goo $goo)
)

(assert_return (invoke "foo") (i64.const 0))
