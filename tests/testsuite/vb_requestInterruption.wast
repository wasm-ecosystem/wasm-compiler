(module
  (import "spectest" "requestInterruption" (func $requestInterruption))
  (import "spectest" "requestInterruptionTrapCodeNone" (func $requestInterruptionTrapCodeNone))
  
  (type $void_to_void (func))
  (table 1 funcref)
  (elem (i32.const 0) $requestInterruption)
  (func (export "requestInterruption")
    call $requestInterruption
  )
  (func (export "requestInterruption_ret") (result i32)
    call $requestInterruption
    i32.const 1
  )
  (func (export "requestInterruption_loop")
    loop
    call $requestInterruption
    br 0
    end
  )
  (func (export "requestInterruptionTrapCodeNone") (result i32)
    call $requestInterruptionTrapCodeNone
    i32.const 1
  )
  (func (export "requestInterruption_indirect")
    i32.const 0
    call_indirect (type $void_to_void)
  )
)


(assert_trap (invoke "requestInterruption") "runtime interrupt request")
(assert_trap (invoke "requestInterruption_loop") "runtime interrupt request")
(assert_trap (invoke "requestInterruption_indirect") "runtime interrupt request")
(assert_return (invoke "requestInterruptionTrapCodeNone") (i32.const 1))
