(module
  (type $unary (func (param i32) (result i32)))
  (type $wrong (func (result i64)))
  (type $sum10 (func (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)))
  (type $results10 (func (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    (result i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)))
  (type $sum16 (func (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)))
  (type $results16 (func (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    (result i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)))
  (table 7 funcref)

  (func $increment (type $unary) (param i32) (result i32)
    local.get 0
    i32.const 1
    i32.add)

  (func $wrong_type (type $wrong) (result i64)
    i64.const 0)

  (func $sum10 (type $sum10) (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    local.get 0 local.get 1 i32.add
    local.get 2 i32.add local.get 3 i32.add local.get 4 i32.add
    local.get 5 i32.add local.get 6 i32.add local.get 7 i32.add
    local.get 8 i32.add local.get 9 i32.add)

  (func $countdown10 (type $sum10) (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    (if (result i32) (i32.eqz (local.get 0))
      (then
        local.get 1 local.get 2 i32.add local.get 3 i32.add
        local.get 4 i32.add local.get 5 i32.add local.get 6 i32.add
        local.get 7 i32.add local.get 8 i32.add local.get 9 i32.add)
      (else
        (return_call_indirect (type $sum10)
          (i32.sub (local.get 0) (i32.const 1))
          (local.get 1) (local.get 2) (local.get 3) (local.get 4) (local.get 5)
          (local.get 6) (local.get 7) (local.get 8) (local.get 9) (i32.const 3)))))

  (func $results10 (type $results10) (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    (result i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    local.get 0 local.get 1 local.get 2 local.get 3 local.get 4
    local.get 5 local.get 6 local.get 7 local.get 8 local.get 9)

  (func $sum16 (type $sum16) (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    local.get 0 local.get 1 i32.add local.get 2 i32.add local.get 3 i32.add
    local.get 4 i32.add local.get 5 i32.add local.get 6 i32.add local.get 7 i32.add
    local.get 8 i32.add local.get 9 i32.add local.get 10 i32.add local.get 11 i32.add
    local.get 12 i32.add local.get 13 i32.add local.get 14 i32.add local.get 15 i32.add)

  (func $results16 (type $results16) (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    (result i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    local.get 0 local.get 1 local.get 2 local.get 3 local.get 4 local.get 5 local.get 6 local.get 7
    local.get 8 local.get 9 local.get 10 local.get 11 local.get 12 local.get 13 local.get 14 local.get 15)

  (elem (i32.const 0) $increment $wrong_type $sum10 $countdown10 $results10 $sum16 $results16)

  (func (export "return_call_indirect") (param i32) (result i32)
    (return_call_indirect (type $unary) (local.get 0) (i32.const 0)))

  (func (export "return_call_indirect_wrong_signature") (param i32) (result i32)
    (return_call_indirect (type $unary) (local.get 0) (i32.const 1)))

  (func (export "return_call_indirect_out_of_bounds") (param i32) (result i32)
    (return_call_indirect (type $unary) (local.get 0) (i32.const 7)))

  (func (export "return_call_indirect_shuffled")
    (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    (return_call_indirect (type $sum10)
      (local.get 9) (local.get 8) (local.get 7) (local.get 6) (local.get 5)
      (local.get 4) (local.get 3) (local.get 2) (local.get 1) (local.get 0) (i32.const 2)))

  (func (export "return_call_indirect_fewer_params") (param i32 i32) (result i32)
    (return_call_indirect (type $sum10)
      (local.get 0) (local.get 1) (i32.const 3) (i32.const 4) (i32.const 5)
      (i32.const 6) (i32.const 7) (i32.const 8) (i32.const 9) (i32.const 10) (i32.const 2)))

  (func (export "return_call_indirect_countdown")
    (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    (return_call_indirect (type $sum10)
      (local.get 0) (local.get 1) (local.get 2) (local.get 3) (local.get 4)
      (local.get 5) (local.get 6) (local.get 7) (local.get 8) (local.get 9) (i32.const 3)))

  (func (export "return_call_indirect_stack_results")
    (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    (result i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    (return_call_indirect (type $results10)
      (local.get 9) (local.get 8) (local.get 7) (local.get 6) (local.get 5)
      (local.get 4) (local.get 3) (local.get 2) (local.get 1) (local.get 0) (i32.const 4)))

  (func (export "return_call_indirect_many_stack_params")
    (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    (return_call_indirect (type $sum16)
      (local.get 15) (local.get 14) (local.get 13) (local.get 12) (local.get 11) (local.get 10) (local.get 9) (local.get 8)
      (local.get 7) (local.get 6) (local.get 5) (local.get 4) (local.get 3) (local.get 2) (local.get 1) (local.get 0) (i32.const 5)))

  (func (export "return_call_indirect_many_stack_results")
    (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    (result i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    (return_call_indirect (type $results16)
      (local.get 15) (local.get 14) (local.get 13) (local.get 12) (local.get 11) (local.get 10) (local.get 9) (local.get 8)
      (local.get 7) (local.get 6) (local.get 5) (local.get 4) (local.get 3) (local.get 2) (local.get 1) (local.get 0) (i32.const 6)))
)

(assert_return (invoke "return_call_indirect" (i32.const 41)) (i32.const 42))
;; (assert_trap (invoke "return_call_indirect_wrong_signature" (i32.const 0)) "indirect call type mismatch")
;; (assert_trap (invoke "return_call_indirect_out_of_bounds" (i32.const 0)) "undefined element")
(assert_return (invoke "return_call_indirect_shuffled"
  (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4) (i32.const 5)
  (i32.const 6) (i32.const 7) (i32.const 8) (i32.const 9) (i32.const 10))
  (i32.const 55))
(assert_return (invoke "return_call_indirect_fewer_params" (i32.const 1) (i32.const 2)) (i32.const 55))
(assert_return (invoke "return_call_indirect_countdown"
  (i32.const 10000)
  (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4) (i32.const 5)
  (i32.const 6) (i32.const 7) (i32.const 8) (i32.const 9))
  (i32.const 45))
(assert_return (invoke "return_call_indirect_stack_results"
  (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4) (i32.const 5)
  (i32.const 6) (i32.const 7) (i32.const 8) (i32.const 9) (i32.const 10))
  (i32.const 10) (i32.const 9) (i32.const 8) (i32.const 7) (i32.const 6)
  (i32.const 5) (i32.const 4) (i32.const 3) (i32.const 2) (i32.const 1))
(assert_return (invoke "return_call_indirect_many_stack_params"
  (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4) (i32.const 5) (i32.const 6) (i32.const 7) (i32.const 8)
  (i32.const 9) (i32.const 10) (i32.const 11) (i32.const 12) (i32.const 13) (i32.const 14) (i32.const 15) (i32.const 16))
  (i32.const 136))
(assert_return (invoke "return_call_indirect_many_stack_results"
  (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4) (i32.const 5) (i32.const 6) (i32.const 7) (i32.const 8)
  (i32.const 9) (i32.const 10) (i32.const 11) (i32.const 12) (i32.const 13) (i32.const 14) (i32.const 15) (i32.const 16))
  (i32.const 16) (i32.const 15) (i32.const 14) (i32.const 13) (i32.const 12) (i32.const 11) (i32.const 10) (i32.const 9)
  (i32.const 8) (i32.const 7) (i32.const 6) (i32.const 5) (i32.const 4) (i32.const 3) (i32.const 2) (i32.const 1))

;; Validation: return_call_indirect requires the enclosing function's result arity and types to match the callee type.
(assert_invalid
  (module
    (type $callee (func (result i32)))
    (table 0 funcref)
    (func (result i32 i32)
      (return_call_indirect (type $callee) (i32.const 0))))
  "type mismatch")
(assert_invalid
  (module
    (type $callee (func (result i64)))
    (table 0 funcref)
    (func (result i32)
      (return_call_indirect (type $callee) (i32.const 0))))
  "type mismatch")

;; Validation: all callee parameters and the trailing i32 table index must be present and correctly typed.
(assert_invalid
  (module
    (type $callee (func (param i32 i32)))
    (table 0 funcref)
    (func
      (return_call_indirect (type $callee) (i32.const 1) (i32.const 0))))
  "type mismatch")
(assert_invalid
  (module
    (type $callee (func (param i64)))
    (table 0 funcref)
    (func
      (return_call_indirect (type $callee) (i32.const 1) (i32.const 0))))
  "type mismatch")
(assert_invalid
  (module
    (type $callee (func (param i32)))
    (table 0 funcref)
    (func
      (return_call_indirect (type $callee) (i32.const 1) (i64.const 0))))
  "type mismatch")

;; Imported functions cannot use the no-link tail-jump path. The indirect tail
;; call must still forward its result after the normal call.
(module
  (type $binary (func (param i32 i32) (result i32)))
  (import "spectest" "func-i32-i32" (func $imported (type $binary)))
  (table 1 funcref)
  (elem (i32.const 0) $imported)

  (func (export "return_call_indirect_import") (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.const 0
    return_call_indirect (type $binary))
)

(assert_return (invoke "return_call_indirect_import" (i32.const 1) (i32.const 2)) (i32.const 3))