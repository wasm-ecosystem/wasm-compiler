(module
    (type $void_func (func))
    (memory 1 100)

    (data (i32.const 0) "\01\00\00\00")
    (data (i32.const 4) "\01\00\00\00")
    (data (i32.const 8) "\01\00\00\00")
    (data (i32.const 12) "\01\00\00\00")
    (data (i32.const 16) "\01\00\00\00")
    (data (i32.const 20) "\01\00\00\00")
    (data (i32.const 24) "\01\00\00\00")

    (table 1 funcref)
    (elem (i32.const 0) $memory-store-24)

    (func $memory-store
        i32.const 0
        i32.const 5
        i32.store
    )

    (func $memory-store-24
        i32.const 24
        i32.const 5
        i32.store
    )

    (func (export "load-before-call")(result i32)
        i32.const 0
        i32.load
        i32.const 0
        i32.load
        call $memory-store
        i32.const 0
        i32.load
        i32.add
        i32.add
        return
    )

    (func (export "load-before-store")(result i32)
        i32.const 4
        i32.load
        i32.const 4
        i32.load

        i32.const 4
        i32.const 5
        i32.store

        i32.const 4
        i32.load

        i32.add
        i32.add
        return
    )

    (func (export "load-before-block")(result i32)
        i32.const 8
        i32.load
        i32.const 8
        i32.load
        block
          i32.const 8
          i32.const 5
          i32.store
        end

        i32.const 8
        i32.load

        i32.add
        i32.add
        return
    )
    
    (func (export "load-before-if-true") (param i32) (result i32)
        i32.const 12
        i32.load
        i32.const 12
        i32.load
        
        local.get 0
        if
          i32.const 12
          i32.const 5
          i32.store
        else
          i32.const 12
          i32.const 6
          i32.store
        end
        
        i32.const 12
        i32.load
        
        i32.add
        i32.add
        return
    )

    (func (export "load-before-if-false") (param i32) (result i32)
        i32.const 16
        i32.load
        i32.const 16
        i32.load
        
        local.get 0
        if
          i32.const 16
          i32.const 5
          i32.store
        else
          i32.const 16
          i32.const 6
          i32.store
        end
        
        i32.const 16
        i32.load
        
        i32.add
        i32.add
        return
    )

    (func (export "load-before-loop") (result i32)
        (local i32)
        i32.const 1
        local.set 0

        i32.const 20
        i32.load
        i32.const 20
        i32.load
        
        (loop
            i32.const 20
            i32.const 5
            i32.store
            
            local.get 0
            i32.const 1
            i32.sub
            local.set 0
            
            local.get 0
            i32.const 0
            i32.ne
            br_if 0
        )
        
        i32.const 20
        i32.load
        
        i32.add
        i32.add
        return
    )
    
    (func (export "load-before-call-indirect")(result i32)
        i32.const 24
        i32.load
        i32.const 24
        i32.load
        
        i32.const 0
        call_indirect (type $void_func)
        
        i32.const 24
        i32.load
        
        i32.add
        i32.add
        return
    )

    (func (export "load-before-return")(result i32)
        i32.const 0xFFFFFF
        i32.load
        i32.const 0
        return
    
    )

    (func (export "load-before-drop")
      i32.const 0xFFFFFF
      i32.load
      drop
    )
    
    (func (export "trap-before-load")(result i32)
      (local i32)
      i32.const 0
      i32.const 0
      i32.div_u

      i32.const 0xFFFFFF
      i32.load
      
      local.set 0
      return
    )

    (func (export "load-before-br")
      block 
        i32.const 1025395043
        i32.load8_s
        br 0 (;@1;)
        unreachable
      end
    )

    (func (export "load-before-br_if")
      block 
        i32.const 1025395043
        i32.load8_s
        i32.const 1
        br_if 0 (;@1;)
        drop
      end
      
    )

    (func (export "load-after-unreachable")  (result i32)
      unreachable
      i32.const 15
      i32.and
      i32.load offset=4
    )

   

    (func (export "load-before-br_table") (result i32)
      (block $case0
        (block $case1
          (block $case2
            i32.const 0xFFFFFF
            i32.load
            (br_table $case0 $case1 $case2 
                      (i32.const 2)        
                      (i32.const 1)
            )
          ) 
          (i32.const 20)  
          return
        ) 
        (i32.const 10) 
        return
      ) 
      (i32.const 5) 
      return
    )

    (func (export "load-before-memory_grow")(result i32)
      i32.const 0xFFFFFF
      i32.load

      i32.const 1
      memory.grow
      return
    )
    
    (func (export "load-before-memory_fill")(result i32)
      i32.const 0xFFFFFF
      i32.load
      
      i32.const 0
      i32.const 1
      i32.const 10
      memory.fill
      i32.const 0
      return
    )
    
    (func (export "load-before-memory_copy")
      i32.const 0xFFFFFF
      i32.load
      
      i32.const 10
      i32.const 0
      i32.const 10
      memory.copy
      return
    )

    (func (export "load-before-div")(result i32)
      i32.const 0xFFFFFF
      i32.load

      i32.const 0
      i32.const 0

      i32.div_u
      i32.add
    )

    (func (export "div-load-block-param") (param i32 i32) (result i32)
      (local i32)
      local.get 0
      local.get 1
      i32.div_u

      i32.const 0xFFFFFFF
      i32.load
      (block (param i32 i32) (result i32)
        i32.add
      )
    )

    (func $return-two-zeros (param i32 i32) (result i32)
      local.get 0
      local.get 1
      i32.add
    )

    (func (export "div-load-call-param") (param i32 i32) (result i32)
      local.get 0
      local.get 1
      i32.div_u

      i32.const 0xFFFFFFF
      i32.load
      call $return-two-zeros
    )
)

(assert_return (invoke "load-before-call") (i32.const 7))
(assert_return (invoke "load-before-store") (i32.const 7))
(assert_return (invoke "load-before-block") (i32.const 7))
(assert_return (invoke "load-before-if-true" (i32.const 1)) (i32.const 7))
(assert_return (invoke "load-before-if-false" (i32.const 0)) (i32.const 8))
(assert_return (invoke "load-before-loop") (i32.const 7))
(assert_return (invoke "load-before-call-indirect") (i32.const 7))
(assert_trap (invoke "load-before-return") "out of bounds memory access")
(assert_trap (invoke "load-before-drop") "out of bounds memory access")
(assert_trap (invoke "trap-before-load") "integer divide by zero")
(assert_trap (invoke "load-before-br") "out of bounds memory access")
(assert_trap (invoke "load-before-br_if") "out of bounds memory access")
(assert_trap (invoke "load-before-br_table") "out of bounds memory access")
(assert_trap (invoke "load-after-unreachable") "unreachable")
(assert_trap (invoke "load-before-memory_grow") "out of bounds memory access")
(assert_trap (invoke "load-before-memory_fill") "out of bounds memory access")
(assert_trap (invoke "load-before-memory_copy") "out of bounds memory access")
(assert_trap (invoke "load-before-div") "out of bounds memory access")
(assert_trap (invoke "div-load-block-param" (i32.const 0) (i32.const 0)) "integer divide by zero")
(assert_trap (invoke "div-load-call-param" (i32.const 0) (i32.const 0)) "integer divide by zero")

;; carried over from fuzz
(module

  (func (;3;)
    i32.const -64
    i32.load8_s offset=4

    global.set 0
  )
  
  (func  (result i32)
    (local i32)
    global.get 0)
  
  (memory (;0;) 1 1)
  (global (;0;) (mut i32) (i32.const 5))
  (export "func_invoker" (func 0))
  (export "func_1" (func 1))
)
(assert_trap (invoke "func_invoker") "out of bounds memory access")
(assert_return (invoke "func_1") (i32.const 5))
