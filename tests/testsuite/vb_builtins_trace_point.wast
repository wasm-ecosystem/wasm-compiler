
(module
  (import "spectest" "setTraceBuffer" (func $setTraceBuffer (param $ptr i32) (param $size i32)))
  (import "builtin" "tracePoint" (func $tracePoint (param i32)))
  
  (memory 1)

  (export "setTraceBuffer" (func $setTraceBuffer))

  (func $empty)
  (func (export "tracePointFromLocal") (param i32)
    local.get 0
    call $tracePoint
  )
  (func (export "tracePointFromConst")
    i32.const 200
    call $tracePoint
  )
  (func (export "tracePointFromStack") (param i32)
    local.get 0
    call $empty
    call $tracePoint
  )
  (func (export "readTraceBuffer") (param $base i32) (param $index i32) (result i32)
    local.get $index
    i32.const 8
    i32.mul
    i32.const 4
    i32.add
    local.get $base
    i32.add
    i32.load offset=8
  )
  (func (export "getSize") (param $base i32) (result i32)
    local.get $base
    i32.load offset=4
  )

  (func (export "largeAmountLocals") (result i32)
    (local i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    i32.const 10
    local.set 0
    i32.const 1
    local.set 1
    i32.const 2
    local.set 2
    i32.const 3
    local.set 3
    i32.const 4
    local.set 4

    local.get 0
    local.get 1
    local.get 2
    local.get 3
    local.get 4

    i32.const 100
    local.set 0
    i32.const 100
    local.set 1
    i32.const 100
    local.set 2
    i32.const 100
    local.set 3
    i32.const 100
    local.set 4

    i32.const 0
    call $tracePoint

    i32.add
    i32.add
    i32.add
    i32.add
    
  )
)

(invoke "setTraceBuffer" (i32.const 0) (i32.const 8))
(invoke "tracePointFromLocal" (i32.const 100))
(invoke "tracePointFromConst")
(invoke "tracePointFromStack" (i32.const 300))
(assert_return (invoke "getSize" (i32.const 0)) (i32.const 3))
(assert_return (invoke "readTraceBuffer" (i32.const 0) (i32.const 0)) (i32.const 100))
(assert_return (invoke "readTraceBuffer" (i32.const 0) (i32.const 1)) (i32.const 200))
(assert_return (invoke "readTraceBuffer" (i32.const 0) (i32.const 2)) (i32.const 300))

;; overflow
(invoke "setTraceBuffer" (i32.const 1024) (i32.const 6))
(invoke "tracePointFromLocal" (i32.const 100))
(invoke "tracePointFromLocal" (i32.const 200))
(assert_return (invoke "getSize" (i32.const 1024)) (i32.const 2))
(invoke "tracePointFromLocal" (i32.const 300))
(assert_return (invoke "getSize" (i32.const 1024)) (i32.const 2))
(assert_return (invoke "readTraceBuffer" (i32.const 1024) (i32.const 0)) (i32.const 100))
(assert_return (invoke "readTraceBuffer" (i32.const 1024) (i32.const 1)) (i32.const 200))
(assert_return (invoke "readTraceBuffer" (i32.const 1024) (i32.const 2)) (i32.const 0))
(assert_return (invoke "largeAmountLocals") (i32.const 20))
