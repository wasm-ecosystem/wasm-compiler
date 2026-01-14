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

(module
    (import "env" "invalid" (func $invalid))

    (table $t 1 funcref)
    (elem (i32.const 0) $invalid)

    (func (export "call_indirect_test")
        (call_indirect (i32.const 0))
    )
)
(assert_trap (invoke "call_indirect_test") "called function not linked")

;; Test case: table size > 16 (out of unsigned 4-bit range for tableInitialSize - 1) for tricore
(module
  (type (func (param i64) (result i64)))

  (func $f (param i64) (result i64)
    local.get 0)

  (func (export "test_large_table") (result i64)
    i64.const 42
    i32.const 0
    call_indirect (type 0)
  )

  (func (export "test_index_oob_large") (result i64)
    i64.const 42
    i32.const 20  ;; index 20, but table only has 17 elements
    call_indirect (type 0)
  )

  ;; Table with 17 elements (maxIndex = 16, out of unsigned 4-bit range)
  (table 17 funcref)
  (elem (i32.const 0) func $f $f $f $f $f $f $f $f $f $f $f $f $f $f $f $f $f)
)
(assert_return (invoke "test_large_table") (i64.const 42))
(assert_trap (invoke "test_index_oob_large") "Indirect call out of bounds")

;; Test case: sigIndex >= 8 (out of signed 4-bit range) for tricore
(module
  ;; Define 9 types so sigIndex 8 is used
  (type (;0;) (func))
  (type (;1;) (func (param i32)))
  (type (;2;) (func (param i64)))
  (type (;3;) (func (param f32)))
  (type (;4;) (func (param f64)))
  (type (;5;) (func (param i32 i32)))
  (type (;6;) (func (param i64 i64)))
  (type (;7;) (func (param f32 f32)))
  (type (;8;) (func (param i64) (result i64)))  ;; sigIndex 8, out of signed 4-bit range

  (func $f (param i64) (result i64)
    local.get 0)

  (func $f_wrong (param i32)  ;; type 1, not type 8
    )

  (func (export "test_large_sigindex") (result i64)
    i64.const 99
    i32.const 0
    call_indirect (type 8)
  )

  (func (export "test_sig_mismatch_large") (result i64)
    i64.const 99
    i32.const 1  ;; index 1 has $f_wrong with type 1
    call_indirect (type 8)  ;; expects type 8, but $f_wrong is type 1
  )

  (table 3 funcref)
  (elem (i32.const 0) func $f $f_wrong)
)
(assert_return (invoke "test_large_sigindex") (i64.const 99))
(assert_trap (invoke "test_sig_mismatch_large") "Indirect call performed with wrong signature")

;; Test case: small table - index out of bounds and signature mismatch (in 4-bit range) for tricore
(module
  (type (;0;) (func (param i64) (result i64)))
  (type (;1;) (func (param i32) (result i32)))

  (func $f (param i64) (result i64)
    local.get 0)

  (func $f_i32 (param i32) (result i32)
    local.get 0)

  (func (export "test_index_oob_small") (result i64)
    i64.const 42
    i32.const 5  ;; index 5, but table only has 3 elements
    call_indirect (type 0)
  )

  (func (export "test_sig_mismatch_small") (result i64)
    i64.const 42
    i32.const 1  ;; index 1 has $f_i32 with wrong type
    call_indirect (type 0)  ;; expects type 0 (i64->i64), but $f_i32 is type 1 (i32->i32)
  )

  (table 3 funcref)
  (elem (i32.const 0) func $f $f_i32 $f)
)
(assert_trap (invoke "test_index_oob_small") "Indirect call out of bounds")
(assert_trap (invoke "test_sig_mismatch_small") "Indirect call performed with wrong signature")

;; Test case: both table size > 16 AND sigIndex >= 8 for tricore
(module
  ;; Define 9 types so sigIndex 8 is used
  (type (;0;) (func))
  (type (;1;) (func (param i32)))
  (type (;2;) (func (param i64)))
  (type (;3;) (func (param f32)))
  (type (;4;) (func (param f64)))
  (type (;5;) (func (param i32 i32)))
  (type (;6;) (func (param i64 i64)))
  (type (;7;) (func (param f32 f32)))
  (type (;8;) (func (param i64) (result i64)))  ;; sigIndex 8

  (func $f (param i64) (result i64)
    local.get 0)

  (func (export "test_large_both") (result i64)
    i64.const 77
    i32.const 0
    call_indirect (type 8)
  )

  ;; Table with 17 elements
  (table 17 funcref)
  (elem (i32.const 0) func $f $f $f $f $f $f $f $f $f $f $f $f $f $f $f $f $f)
)
(assert_return (invoke "test_large_both") (i64.const 77))

