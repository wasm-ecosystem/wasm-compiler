(module
  (func $f (local i32)
    local.get 0
    if
    end
    i32.const 42
    local.set 0)
  (func $f2 (local i32 i32 i32)
    local.get 0 
    if
    end
    local.get 1 
    if
    end
    local.get 2 
    if
    end
    i32.const 3
    local.set 0
    i32.const 2
    local.set 1
    i32.const 0
    local.set 2)
  (func (export "check_local_value_after_call") (param i32) (result i32)
    i32.const 20
    local.set 0
    call $f
    local.get 0)
  (func (export "check_multi_local_value_after_call")
    (param i32 i32 i32) (result i32 i32 i32)
    i32.const 20
    local.set 0
    i32.const 20
    local.set 1
    i32.const 20
    local.set 2
    call $f
    local.get 0
    local.get 1
    local.get 2))
(assert_return (invoke "check_local_value_after_call" (i32.const 10)) (i32.const 20))
(assert_return (invoke "check_multi_local_value_after_call" (i32.const 10) (i32.const 10) (i32.const 10)) (i32.const 20) (i32.const 20) (i32.const 20))
