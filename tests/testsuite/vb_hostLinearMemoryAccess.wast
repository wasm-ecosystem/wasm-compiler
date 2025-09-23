(module
  (import "spectest" "getU32FromLinearMemory" (func $getU32FromLinearMemory (param i32) (result i32)))
  (import "spectest" "getU32FromLinearMemoryContextAtMem" (func $getU32FromLinearMemoryContextAtMem (param i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)))
  (export "getU32FromLinearMemory" (func $getU32FromLinearMemory))
  (export "getU32FromLinearMemoryContextAtMem" (func $getU32FromLinearMemoryContextAtMem))

  (func (export "writeU32ToLinearMemory") (param i32 i32)
    local.get 0
    local.get 1
    i32.store
  )

  (func (export "getU32FromLinearMemoryViaCallIndirect") (param i32) (result i32)
    local.get 0
    i32.const 0
    call_indirect (type $getU32Type)
  )

  (func (export "getU32FromLinearMemoryContextAtMemViaCallIndirect") (param i32) (result i32)
    local.get 0
    i32.const 5
    i32.const 5
    i32.const 5
    i32.const 5
    i32.const 5
    i32.const 5
    i32.const 5
    i32.const 5
    i32.const 1
    call_indirect (type $getU32ContextAtMemType)
  )

    (func (export "getU32FromLinearMemoryContextAtMemWrapper") (param i32) (result i32)
    local.get 0
    i32.const 5
    i32.const 5
    i32.const 5
    i32.const 5
    i32.const 5
    i32.const 5
    i32.const 5
    i32.const 5
    call $getU32FromLinearMemoryContextAtMem
  )



  (type $getU32Type (func (param i32) (result i32)))
  (type $getU32ContextAtMemType (func (param i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)))
  (table 2 funcref)
  (elem (i32.const 0) $getU32FromLinearMemory $getU32FromLinearMemoryContextAtMem)
  
  (memory 1)
  (data (i32.const 4) "abcd")
)

(assert_return (invoke "getU32FromLinearMemory" (i32.const 0)) (i32.const 0))
(assert_return (invoke "getU32FromLinearMemory" (i32.const 1)) (i32.const 0x61000000))
(assert_return (invoke "getU32FromLinearMemory" (i32.const 2)) (i32.const 0x62610000))
(assert_return (invoke "getU32FromLinearMemory" (i32.const 3)) (i32.const 0x63626100))
(assert_return (invoke "getU32FromLinearMemory" (i32.const 4)) (i32.const 0x64636261))
(assert_return (invoke "getU32FromLinearMemory" (i32.const 5)) (i32.const 0x646362))
(assert_return (invoke "getU32FromLinearMemory" (i32.const 6)) (i32.const 0x6463))
(assert_return (invoke "getU32FromLinearMemory" (i32.const 7)) (i32.const 0x64))
(assert_return (invoke "getU32FromLinearMemory" (i32.const 8)) (i32.const 0))

(assert_return (invoke "writeU32ToLinearMemory" (i32.const 0) (i32.const 0x1234)))
(assert_return (invoke "getU32FromLinearMemory" (i32.const 0)) (i32.const 0x1234))

(assert_return (invoke "writeU32ToLinearMemory" (i32.const 120) (i32.const 0x1234)))
(assert_return (invoke "getU32FromLinearMemory" (i32.const 120)) (i32.const 0x1234))

;; Test call_indirect functionality
(assert_return (invoke "writeU32ToLinearMemory" (i32.const 200) (i32.const 0x1234)))

(assert_return (invoke "getU32FromLinearMemoryViaCallIndirect" (i32.const 200)) (i32.const 0x1234))
(assert_return (invoke "getU32FromLinearMemoryContextAtMemViaCallIndirect" (i32.const 200)) (i32.const 0x1234))
(assert_return (invoke "getU32FromLinearMemoryContextAtMemWrapper" (i32.const 200)) (i32.const 0x1234))
(assert_return (invoke "getU32FromLinearMemoryContextAtMem" (i32.const 200) (i32.const 5) (i32.const 5) (i32.const 5) (i32.const 5) (i32.const 5) (i32.const 5) (i32.const 5) (i32.const 5)) (i32.const 0x1234))
