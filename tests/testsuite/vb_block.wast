(module

  (func (export "foo") (result i32)
    (local i32)

    i32.const 5
    local.set 0

    local.get 0

    local.get 0
    block (result i32)
      i32.const -32767
      br 0 (;@1;)
    end
    i32.shl
    
    i32.const 4
    i32.xor
    local.tee 0

    i32.xor
    
    )
)
(assert_return (invoke "foo") (i32.const 11))