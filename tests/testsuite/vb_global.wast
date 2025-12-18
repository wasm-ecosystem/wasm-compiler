;; Carried over from spectest, removed unsupported import global and ref type

(module
  (global (import "spectest" "global_i32") i32)
  (global (import "spectest" "global_i64") i64)

  (global $a i32 (i32.const -2))
  (global (;3;) f32 (f32.const -3))
  (global (;4;) f64 (f64.const -4))
  (global $b i64 (i64.const -5))

  (global $x (mut i32) (i32.const -12))
  (global (;7;) (mut f32) (f32.const -13))
  (global (;8;) (mut f64) (f64.const -14))
  (global $y (mut i64) (i64.const -15))

  (global $z1 i32 (global.get 0))
  (global $z2 i64 (global.get 1))

  (func (export "get-a") (result i32) (global.get $a))
  (func (export "get-b") (result i64) (global.get $b))
  (func (export "get-x") (result i32) (global.get $x))
  (func (export "get-y") (result i64) (global.get $y))
  (func (export "get-z1") (result i32) (global.get $z1))
  (func (export "get-z2") (result i64) (global.get $z2))
  (func (export "set-x") (param i32) (global.set $x (local.get 0)))
  (func (export "set-y") (param i64) (global.set $y (local.get 0)))

  (func (export "get-3") (result f32) (global.get 3))
  (func (export "get-4") (result f64) (global.get 4))
  (func (export "get-7") (result f32) (global.get 7))
  (func (export "get-8") (result f64) (global.get 8))
  (func (export "set-7") (param f32) (global.set 7 (local.get 0)))
  (func (export "set-8") (param f64) (global.set 8 (local.get 0)))

  (memory 1)

  (func $dummy)

  (func (export "as-select-first") (result i32)
    (select (global.get $x) (i32.const 2) (i32.const 3))
  )
  (func (export "as-select-mid") (result i32)
    (select (i32.const 2) (global.get $x) (i32.const 3))
  )
  (func (export "as-select-last") (result i32)
    (select (i32.const 2) (i32.const 3) (global.get $x))
  )

  (func (export "as-loop-first") (result i32)
    (loop (result i32)
      (global.get $x) (call $dummy) (call $dummy)
    )
  )
  (func (export "as-loop-mid") (result i32)
    (loop (result i32)
      (call $dummy) (global.get $x) (call $dummy)
    )
  )
  (func (export "as-loop-last") (result i32)
    (loop (result i32)
      (call $dummy) (call $dummy) (global.get $x)
    )
  )

  (func (export "as-if-condition") (result i32)
    (if (result i32) (global.get $x)
      (then (call $dummy) (i32.const 2))
      (else (call $dummy) (i32.const 3))
    )
  )
  (func (export "as-if-then") (result i32)
    (if (result i32) (i32.const 1)
      (then (global.get $x)) (else (i32.const 2))
    )
  )
  (func (export "as-if-else") (result i32)
    (if (result i32) (i32.const 0)
      (then (i32.const 2)) (else (global.get $x))
    )
  )

  (func (export "as-br_if-first") (result i32)
    (block (result i32)
      (br_if 0 (global.get $x) (i32.const 2))
      (return (i32.const 3))
    )
  )
  (func (export "as-br_if-last") (result i32)
    (block (result i32)
      (br_if 0 (i32.const 2) (global.get $x))
      (return (i32.const 3))
    )
  )

  (func (export "as-br_table-first") (result i32)
    (block (result i32)
      (global.get $x) (i32.const 2) (br_table 0 0)
    )
  )
  (func (export "as-br_table-last") (result i32)
    (block (result i32)
      (i32.const 2) (global.get $x) (br_table 0 0)
    )
  )

  (func $func (param i32 i32) (result i32) (local.get 0))
  (type $check (func (param i32 i32) (result i32)))
  (table funcref (elem $func))
  (func (export "as-call_indirect-first") (result i32)
    (block (result i32)
      (call_indirect (type $check)
        (global.get $x) (i32.const 2) (i32.const 0)
      )
    )
  )
  (func (export "as-call_indirect-mid") (result i32)
    (block (result i32)
      (call_indirect (type $check)
        (i32.const 2) (global.get $x) (i32.const 0)
      )
    )
  )
 (func (export "as-call_indirect-last") (result i32)
    (block (result i32)
      (call_indirect (type $check)
        (i32.const 2) (i32.const 0) (global.get $x)
      )
    )
  )

  (func (export "as-store-first")
    (global.get $x) (i32.const 1) (i32.store)
  )
  (func (export "as-store-last")
    (i32.const 0) (global.get $x) (i32.store)
  )
  (func (export "as-load-operand") (result i32)
    (i32.load (global.get $x))
  )
  (func (export "as-memory.grow-value") (result i32)
    (memory.grow (global.get $x))
  )

  (func $f (param i32) (result i32) (local.get 0))
  (func (export "as-call-value") (result i32)
    (call $f (global.get $x))
  )

  (func (export "as-return-value") (result i32)
    (global.get $x) (return)
  )
  (func (export "as-drop-operand")
    (drop (global.get $x))
  )
  (func (export "as-br-value") (result i32)
    (block (result i32) (br 0 (global.get $x)))
  )

  (func (export "as-local.set-value") (param i32) (result i32)
    (local.set 0 (global.get $x))
    (local.get 0)
  )
  (func (export "as-local.tee-value") (param i32) (result i32)
    (local.tee 0 (global.get $x))
  )
  (func (export "as-global.set-value") (result i32)
    (global.set $x (global.get $x))
    (global.get $x)
  )

  (func (export "as-unary-operand") (result i32)
    (i32.eqz (global.get $x))
  )
  (func (export "as-binary-operand") (result i32)
    (i32.mul
      (global.get $x) (global.get $x)
    )
  )
  (func (export "as-compare-operand") (result i32)
    (i32.gt_u
      (global.get 0) (i32.const 1)
    )
  )
)

(assert_return (invoke "get-a") (i32.const -2))
(assert_return (invoke "get-b") (i64.const -5))
(assert_return (invoke "get-x") (i32.const -12))
(assert_return (invoke "get-y") (i64.const -15))
(assert_return (invoke "get-z1") (i32.const 666))
(assert_return (invoke "get-z2") (i64.const 666))

(assert_return (invoke "get-3") (f32.const -3))
(assert_return (invoke "get-4") (f64.const -4))
(assert_return (invoke "get-7") (f32.const -13))
(assert_return (invoke "get-8") (f64.const -14))

(assert_return (invoke "set-x" (i32.const 6)))
(assert_return (invoke "set-y" (i64.const 7)))

(assert_return (invoke "set-7" (f32.const 8)))
(assert_return (invoke "set-8" (f64.const 9)))

(assert_return (invoke "get-x") (i32.const 6))
(assert_return (invoke "get-y") (i64.const 7))
(assert_return (invoke "get-7") (f32.const 8))
(assert_return (invoke "get-8") (f64.const 9))

(assert_return (invoke "set-7" (f32.const 8)))
(assert_return (invoke "set-8" (f64.const 9)))

(assert_return (invoke "get-x") (i32.const 6))
(assert_return (invoke "get-y") (i64.const 7))
(assert_return (invoke "get-7") (f32.const 8))
(assert_return (invoke "get-8") (f64.const 9))

(assert_return (invoke "as-select-first") (i32.const 6))
(assert_return (invoke "as-select-mid") (i32.const 2))
(assert_return (invoke "as-select-last") (i32.const 2))

(assert_return (invoke "as-loop-first") (i32.const 6))
(assert_return (invoke "as-loop-mid") (i32.const 6))
(assert_return (invoke "as-loop-last") (i32.const 6))

(assert_return (invoke "as-if-condition") (i32.const 2))
(assert_return (invoke "as-if-then") (i32.const 6))
(assert_return (invoke "as-if-else") (i32.const 6))

(assert_return (invoke "as-br_if-first") (i32.const 6))
(assert_return (invoke "as-br_if-last") (i32.const 2))

(assert_return (invoke "as-br_table-first") (i32.const 6))
(assert_return (invoke "as-br_table-last") (i32.const 2))

(assert_return (invoke "as-call_indirect-first") (i32.const 6))
(assert_return (invoke "as-call_indirect-mid") (i32.const 2))
(assert_trap (invoke "as-call_indirect-last") "undefined element")

(assert_return (invoke "as-store-first"))
(assert_return (invoke "as-store-last"))
(assert_return (invoke "as-load-operand") (i32.const 1))
(assert_return (invoke "as-memory.grow-value") (i32.const 1))

(assert_return (invoke "as-call-value") (i32.const 6))

(assert_return (invoke "as-return-value") (i32.const 6))
(assert_return (invoke "as-drop-operand"))
(assert_return (invoke "as-br-value") (i32.const 6))

(assert_return (invoke "as-local.set-value" (i32.const 1)) (i32.const 6))
(assert_return (invoke "as-local.tee-value" (i32.const 1)) (i32.const 6))
(assert_return (invoke "as-global.set-value") (i32.const 6))

(assert_return (invoke "as-unary-operand") (i32.const 0))
(assert_return (invoke "as-binary-operand") (i32.const 36))
(assert_return (invoke "as-compare-operand") (i32.const 1))


(module
  (global (import "spectest" "global_i32") i32)

  (global $a (mut i32) (i32.const 2))
  (global $b (mut i32) (i32.const 1))

  (func $foo
    i32.const 10
    global.set $b
    i32.const 20
    global.set $a
  )

  (func (export "spill-global") (result i32) 
    global.get $a
    call $foo
    i32.const 10
    i32.add
    return
  )
)

(assert_return (invoke "spill-global") (i32.const 12))

(module
  (global (import "spectest" "global_i32") i32)

  (global $a (export "g1") (mut i32) (i32.const 2))
)

(module
  (global (import "spectest" "global_i32") i32)
  (global $first_non_import i32 (i32.const 999))
  
  (func (export "get-import") (result i32)
    global.get 0
  )
  
  (func (export "get-non-import") (result i32)
    global.get 1
  )
)

(assert_return (invoke "get-import") (i32.const 666))
(assert_return (invoke "get-non-import") (i32.const 999))

(module
  (global (import "spectest" "global_i32") i32)
  (global (import "spectest" "global_i64") i64)
  (global (import "spectest" "global_f32") f32)
  (global $boundary i32 (i32.const 111))
  (global $next i32 (i32.const 222))
  
  (func (export "last-import") (result f32)
    global.get 2
  )
  
  (func (export "first-non-import") (result i32)
    global.get 3
  )
  
  (func (export "second-non-import") (result i32)
    global.get 4
  )
)

(assert_return (invoke "last-import") (f32.const 666.6))
(assert_return (invoke "first-non-import") (i32.const 111))
(assert_return (invoke "second-non-import") (i32.const 222))

(module
  (global (import "spectest" "global_i32") i32)
  (global (import "spectest" "global_i64") i64)
  (global $reg_allocated (mut i32) (i32.const 500))
  (global $other_mut (mut i32) (i32.const 600))
  
  (func (export "modify-reg-global") (result i32)
    i32.const 700
    global.set $reg_allocated
    global.get $reg_allocated
  )
  
  (func (export "verify-index") (result i32)
    global.get 2
  )
)

(assert_return (invoke "modify-reg-global") (i32.const 700))
(assert_return (invoke "verify-index") (i32.const 700))

(module
  (global (import "spectest" "global_i32") i32)
  (global (import "spectest" "global_i64") i64)
  (global $spill_test (mut i32) (i32.const 100))
  
  (func $cause_spill (param i32) (result i32)
    local.get 0
    i32.const 1
    i32.add
  )
  
  (func (export "spill-restore-test") (result i32)
    i32.const 999
    global.set $spill_test
    i32.const 5
    call $cause_spill
    drop
    global.get $spill_test
  )
  
  (func (export "verify-import-not-spilled") (result i32)
    global.get 0
    i32.const 10
    call $cause_spill
    drop
    global.get 0
    i32.sub
  )
)

(assert_return (invoke "spill-restore-test") (i32.const 999))
(assert_return (invoke "verify-import-not-spilled") (i32.const 0))

(module
  (global (import "spectest" "global_i32") i32)
  (global (import "spectest" "global_i64") i64)
  (global $from_first_import i32 (global.get 0))
  (global $from_second_import i64 (global.get 1))
  
  (func (export "verify-init-from-import-0") (result i32)
    global.get 2
  )
  
  (func (export "verify-init-from-import-1") (result i64)
    global.get 3
  )
)

(assert_return (invoke "verify-init-from-import-0") (i32.const 666))
(assert_return (invoke "verify-init-from-import-1") (i64.const 666))

(module
  (global $first (mut i32) (i32.const 100))
  (global $second (mut i32) (i32.const 200))
  
  (func (export "no-imports-test") (result i32)
    global.get 0
    global.get 1
    i32.add
  )
)

(assert_return (invoke "no-imports-test") (i32.const 300))

(module
  (global (import "spectest" "global_i32") i32)
  (global (import "spectest" "global_i64") i64)
  
  (func (export "only-imports-i32") (result i32)
    global.get 0
  )
  
  (func (export "only-imports-i64") (result i64)
    global.get 1
  )
)

(assert_return (invoke "only-imports-i32") (i32.const 666))
(assert_return (invoke "only-imports-i64") (i64.const 666))

(module
  (global (import "spectest" "global_i32") i32)
  (global (import "spectest" "global_i64") i64)
  (global (import "spectest" "global_f32") f32)
  (global (import "spectest" "global_f64") f64)
  (global $non0 i32 (i32.const 10))
  (global $non1 i32 (i32.const 20))
  (global $non2 i32 (i32.const 30))
  
  (func (export "access-pattern") (result i32)
    global.get 3
    drop
    global.get 4
    global.get 5
    i32.add
    global.get 6
    i32.add
  )
)

(assert_return (invoke "access-pattern") (i32.const 60))

(module
  (global (import "spectest" "global_i32") i32)
  (global $reg_global (mut i32) (i32.const 555))
  (global $local2 i32 (i32.const 2))
  
  (type $sig (func (param i32) (result i32)))
  
  (func $target (param i32) (result i32)
    local.get 0
    i32.const 10
    i32.add
  )
  
  (table 1 funcref)
  (elem (i32.const 0) $target)
  
  (func (export "indirect-with-spill") (result i32)
    i32.const 777
    global.set $reg_global
    i32.const 5
    i32.const 0
    call_indirect (type $sig)
    drop
    global.get $reg_global
  )
  
  (func (export "verify-offset-in-indirect") (result i32)
    global.get 0
    drop
    global.get 1
    global.get 2
    i32.add
  )
)

(assert_return (invoke "indirect-with-spill") (i32.const 777))
(assert_return (invoke "verify-offset-in-indirect") (i32.const 779))

(module
  (global (import "spectest" "global_i32") i32)
  (global $local1 i32 (i32.const 1))
  (global $local2 i32 (i32.const 2))
  (global $local3 i32 (i32.const 3))
  
  (func (export "interleaved") (result i32)
    global.get 0
    drop
    global.get 1
    global.get 0
    drop
    global.get 2
    i32.add
    global.get 0
    drop
    global.get 3
    i32.add
  )
)

(assert_return (invoke "interleaved") (i32.const 6))

(module
  (global (import "spectest" "global_i32") i32)
  (global $immut1 i32 (i32.const 100))
  (global $mut1 (mut i32) (i32.const 200))
  (global $immut2 i64 (i64.const 300))
  (global $mut2 (mut i64) (i64.const 400))
  
  (func (export "access-mixed-mut") (result i32)
    global.get 1
    global.get 2
    i32.add
  )
  
  (func (export "modify-mut") (result i64)
    i64.const 500
    global.set 4
    global.get 4
  )
)

(assert_return (invoke "access-mixed-mut") (i32.const 300))
(assert_return (invoke "modify-mut") (i64.const 500))