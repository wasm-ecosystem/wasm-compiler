

(module
  (func (export "valid_block") (result i32)
    block (result i32)
        unreachable
        block (result i32)
    		unreachable
        end
    end
  )
  (func (export "valid_fnc") (result i32)
    unreachable
    block (result i32)
      unreachable
    end
  )
  (func (export "valid_loop") (result i32)
    unreachable
    loop (result i32)
      unreachable
    end
  )
  (func (export "valid_if_else") (result i32)
    unreachable
    if (result i32)
      unreachable
    else
      unreachable
    end
  )
)

(module
  (func (export "no_trap")
    i32.const 0
    if
      block
        unreachable
      end
    else
    end
  )
)
(assert_return (invoke "no_trap"))

(assert_invalid
(module
  (func (export "invalid_block") (result i32)
    block (result i32)
        unreachable
        block (result i32)
        end
    end
  )
)
"Invalid module"
)
 
(assert_invalid
(module
  (func (export "invalid_fnc") (result i32)
    unreachable
    block (result i32)
    end
  )
)
"Invalid module"
)

(assert_invalid
(module
  (func (export "invalid_loop") (result i32)
    unreachable
    loop (result i32)
    end
  )
)
"Invalid module"
)
 
(assert_invalid
(module
  (func (export "invalid_if_else") (result i32)
    unreachable
    if (result i32)
    else
    end
  )
)
"Invalid module"
)

(module

  (func (result i32)

        unreachable
        i32.trunc_f32_u

        block (result i32) 
            i32.const 0
        end
        i32.add
    
  )
)

(module

  (func (result i32)

        unreachable
        i32.trunc_f32_u

        loop (result i32) 
            i32.const 0
        end
        i32.add
    
  )
)

(module

  (func (result i32)

        unreachable
        i32.trunc_f32_u

        if (result i32) 
            i32.const 0
        else
            i32.const 1
        end
        i32.add
    
  )
)

(module

  (func (result i32 i32)

        unreachable
        i32.trunc_f32_u

        if (result i32 i32) 
            i32.const 0
            i32.const 0
        else
            i32.const 1
            i32.const 0
        end
        i32.add
    
  )
)

(module

  (func (result i64)
    
    i32.const 15
    i64.load16_u offset=4
    unreachable
  )

  (func (result i32)
    
    i32.const 15
    i32.load16_u offset=4
    unreachable
  )
  
  (memory (;0;) 16 17)

 )

(module
  (func (export "test") (param i32)
    local.get 0
    i64.load
    i32.wrap_i64
    unreachable)
  (memory 1)
)

(assert_trap (invoke "test" (i32.const 4)) "unreachable")
(assert_trap (invoke "test" (i32.const 0xFFFFFF)) "out of bounds linked memory access")

(module
  (type (;0;) (func (result i32)))
  (func (;0;) (type 0) (result i32)
    i32.const 0
    if (result i32)  ;; label = @1
      i32.const 4
      return
    else
      i32.const 3
    end
    drop
    i32.const 3)
  (func (;1;) (type 0) (result i32)
    i32.const 0
    if (result i32)  ;; label = @1
      i32.const 4
      return
    else
      i32.const 3
    end)
  (export "pruned-branch-return-1" (func 0))
  (export "pruned-branch-return-2" (func 1))
)

(assert_return (invoke "pruned-branch-return-1") (i32.const 3))
(assert_return (invoke "pruned-branch-return-2") (i32.const 3))

(module
  (type (;0;) (func (result i32)))
  (func (;0;) (type 0) (result i32)
    i32.const 0
    if  ;; label = @1
      loop  ;; label = @2
        global.get 0
        if  ;; label = @3
          global.get 0
          i32.const 1
          i32.sub
          global.set 0
          br 1 (;@2;)
        else
          i32.const 6
          return
        end
        unreachable
      end
      unreachable
    end
    i32.const 3)
  (memory (;0;) 16 17)
  (global (;0;) (mut i32) (i32.const 10))
  (export "pass-down-prunedUnreachable" (func 0))
)

(assert_return (invoke "pass-down-prunedUnreachable") (i32.const 3))

;; Valid module
(module
  (func (result i32)
    unreachable
    select
    ;; ANY match i32 arg
    i32.const 1
    i32.const 1
    select
  )
)
(module
  (func (result i32)
    unreachable
    select
    ;; ANY

    i32.const 1 ;; consumed by select as index(i32)
    ;; ANY match ANY arg
    select
  )
)
