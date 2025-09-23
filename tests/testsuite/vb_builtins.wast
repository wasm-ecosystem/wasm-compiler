
;; the harness links the following 512 bytes of data with data Bi = i % 256

(module
  (import "builtin" "trap" (func $trap))

  (import "builtin" "getLengthOfLinkedMemory" (func $getLengthOfLinkedMemory (result i32)))

  (import "builtin" "getU8FromLinkedMemory" (func $getU8FromLinkedMemory (param i32) (result i32)))
  (import "builtin" "getI8FromLinkedMemory" (func $getI8FromLinkedMemory (param i32) (result i32)))
  (import "builtin" "getU16FromLinkedMemory" (func $getU16FromLinkedMemory (param i32) (result i32)))
  (import "builtin" "getI16FromLinkedMemory" (func $getI16FromLinkedMemory (param i32) (result i32)))
  (import "builtin" "getU32FromLinkedMemory" (func $getU32FromLinkedMemory (param i32) (result i32)))
  (import "builtin" "getI32FromLinkedMemory" (func $getI32FromLinkedMemory (param i32) (result i32)))
  (import "builtin" "getU64FromLinkedMemory" (func $getU64FromLinkedMemory (param i32) (result i64)))
  (import "builtin" "getI64FromLinkedMemory" (func $getI64FromLinkedMemory (param i32) (result i64)))
  (import "builtin" "getF32FromLinkedMemory" (func $getF32FromLinkedMemory (param i32) (result f32)))
  (import "builtin" "getF64FromLinkedMemory" (func $getF64FromLinkedMemory (param i32) (result f64)))

  (func (export "trap_wrapper") (call $trap))

  (func (export "getLengthOfLinkedMemory_wrapper") (result i32) (call $getLengthOfLinkedMemory))

  (func (export "getU8_wrapper_param") (param $x i32) (result i32) (call $getU8FromLinkedMemory (local.get $x)))
  (func (export "getI8_wrapper_param") (param $x i32) (result i32) (call $getI8FromLinkedMemory (local.get $x)))
  (func (export "getU16_wrapper_param") (param $x i32) (result i32) (call $getU16FromLinkedMemory (local.get $x)))
  (func (export "getI16_wrapper_param") (param $x i32) (result i32) (call $getI16FromLinkedMemory (local.get $x)))
  (func (export "getU32_wrapper_param") (param $x i32) (result i32) (call $getU32FromLinkedMemory (local.get $x)))
  (func (export "getI32_wrapper_param") (param $x i32) (result i32) (call $getI32FromLinkedMemory (local.get $x)))
  (func (export "getU64_wrapper_param") (param $x i32) (result i64) (call $getU64FromLinkedMemory (local.get $x)))
  (func (export "getI64_wrapper_param") (param $x i32) (result i64) (call $getI64FromLinkedMemory (local.get $x)))
  (func (export "getF32_wrapper_param") (param $x i32) (result f32) (call $getF32FromLinkedMemory (local.get $x)))
  (func (export "getF64_wrapper_param") (param $x i32) (result f64) (call $getF64FromLinkedMemory (local.get $x)))

  (func (export "getU8_const_low_param") (result i32) (call $getU8FromLinkedMemory (i32.const 50)))
  (func (export "getI8_const_low_param") (result i32) (call $getI8FromLinkedMemory (i32.const 50)))
  (func (export "getU16_const_low_param") (result i32) (call $getU16FromLinkedMemory (i32.const 50)))
  (func (export "getI16_const_low_param") (result i32) (call $getI16FromLinkedMemory (i32.const 50)))
  (func (export "getU32_const_low_param") (result i32) (call $getU32FromLinkedMemory (i32.const 50)))
  (func (export "getI32_const_low_param") (result i32) (call $getI32FromLinkedMemory (i32.const 50)))
  (func (export "getU64_const_low_param") (result i64) (call $getU64FromLinkedMemory (i32.const 50)))
  (func (export "getI64_const_low_param") (result i64) (call $getI64FromLinkedMemory (i32.const 50)))
  (func (export "getF32_const_low_param") (result f32) (call $getF32FromLinkedMemory (i32.const 50)))
  (func (export "getF64_const_low_param") (result f64) (call $getF64FromLinkedMemory (i32.const 50)))

  (func (export "getU8_const_mid_param") (result i32) (call $getU8FromLinkedMemory (i32.const 350)))
  (func (export "getI8_const_mid_param") (result i32) (call $getI8FromLinkedMemory (i32.const 350)))
  (func (export "getU16_const_mid_param") (result i32) (call $getU16FromLinkedMemory (i32.const 350)))
  (func (export "getI16_const_mid_param") (result i32) (call $getI16FromLinkedMemory (i32.const 350)))
  (func (export "getU32_const_mid_param") (result i32) (call $getU32FromLinkedMemory (i32.const 350)))
  (func (export "getI32_const_mid_param") (result i32) (call $getI32FromLinkedMemory (i32.const 350)))
  (func (export "getU64_const_mid_param") (result i64) (call $getU64FromLinkedMemory (i32.const 350)))
  (func (export "getI64_const_mid_param") (result i64) (call $getI64FromLinkedMemory (i32.const 350)))
  (func (export "getF32_const_mid_param") (result f32) (call $getF32FromLinkedMemory (i32.const 350)))
  (func (export "getF64_const_mid_param") (result f64) (call $getF64FromLinkedMemory (i32.const 350)))

  (func (export "getU8_const_high_param") (result i32) (call $getU8FromLinkedMemory (i32.const 500)))
  (func (export "getI8_const_high_param") (result i32) (call $getI8FromLinkedMemory (i32.const 500)))
  (func (export "getU16_const_high_param") (result i32) (call $getU16FromLinkedMemory (i32.const 500)))
  (func (export "getI16_const_high_param") (result i32) (call $getI16FromLinkedMemory (i32.const 500)))
  (func (export "getU32_const_high_param") (result i32) (call $getU32FromLinkedMemory (i32.const 500)))
  (func (export "getI32_const_high_param") (result i32) (call $getI32FromLinkedMemory (i32.const 500)))
  (func (export "getU64_const_high_param") (result i64) (call $getU64FromLinkedMemory (i32.const 500)))
  (func (export "getI64_const_high_param") (result i64) (call $getI64FromLinkedMemory (i32.const 500)))
  (func (export "getF32_const_high_param") (result f32) (call $getF32FromLinkedMemory (i32.const 500)))
  (func (export "getF64_const_high_param") (result f64) (call $getF64FromLinkedMemory (i32.const 500)))

  (func (export "getU8_const_maximum_param") (result i32) (call $getU8FromLinkedMemory (i32.const 511)))
  (func (export "getI8_const_maximum_param") (result i32) (call $getI8FromLinkedMemory (i32.const 511)))
  (func (export "getU16_const_maximum_param") (result i32) (call $getU16FromLinkedMemory (i32.const 510)))
  (func (export "getI16_const_maximum_param") (result i32) (call $getI16FromLinkedMemory (i32.const 510)))
  (func (export "getU32_const_maximum_param") (result i32) (call $getU32FromLinkedMemory (i32.const 508)))
  (func (export "getI32_const_maximum_param") (result i32) (call $getI32FromLinkedMemory (i32.const 508)))
  (func (export "getU64_const_maximum_param") (result i64) (call $getU64FromLinkedMemory (i32.const 504)))
  (func (export "getI64_const_maximum_param") (result i64) (call $getI64FromLinkedMemory (i32.const 504)))
  (func (export "getF32_const_maximum_param") (result f32) (call $getF32FromLinkedMemory (i32.const 508)))
  (func (export "getF64_const_maximum_param") (result f64) (call $getF64FromLinkedMemory (i32.const 504)))

  (func (export "getU8_const_toohigh_param") (result i32) (call $getU8FromLinkedMemory (i32.const 512)))
  (func (export "getI8_const_toohigh_param") (result i32) (call $getI8FromLinkedMemory (i32.const 512)))
  (func (export "getU16_const_toohigh_param") (result i32) (call $getU16FromLinkedMemory (i32.const 511)))
  (func (export "getI16_const_toohigh_param") (result i32) (call $getI16FromLinkedMemory (i32.const 511)))
  (func (export "getU32_const_toohigh_param") (result i32) (call $getU32FromLinkedMemory (i32.const 509)))
  (func (export "getI32_const_toohigh_param") (result i32) (call $getI32FromLinkedMemory (i32.const 509)))
  (func (export "getU64_const_toohigh_param") (result i64) (call $getU64FromLinkedMemory (i32.const 505)))
  (func (export "getI64_const_toohigh_param") (result i64) (call $getI64FromLinkedMemory (i32.const 505)))
  (func (export "getF32_const_toohigh_param") (result f32) (call $getF32FromLinkedMemory (i32.const 509)))
  (func (export "getF64_const_toohigh_param") (result f64) (call $getF64FromLinkedMemory (i32.const 505)))

  (func (export "getU8_const_far_param") (result i32) (call $getU8FromLinkedMemory (i32.const -1)))
  (func (export "getI8_const_far_param") (result i32) (call $getI8FromLinkedMemory (i32.const -1)))
  (func (export "getU16_const_far_param") (result i32) (call $getU16FromLinkedMemory (i32.const -1)))
  (func (export "getI16_const_far_param") (result i32) (call $getI16FromLinkedMemory (i32.const -1)))
  (func (export "getU32_const_far_param") (result i32) (call $getU32FromLinkedMemory (i32.const -1)))
  (func (export "getI32_const_far_param") (result i32) (call $getI32FromLinkedMemory (i32.const -1)))
  (func (export "getU64_const_far_param") (result i64) (call $getU64FromLinkedMemory (i32.const -1)))
  (func (export "getI64_const_far_param") (result i64) (call $getI64FromLinkedMemory (i32.const -1)))
  (func (export "getF32_const_far_param") (result f32) (call $getF32FromLinkedMemory (i32.const -1)))
  (func (export "getF64_const_far_param") (result f64) (call $getF64FromLinkedMemory (i32.const -1)))
)

(assert_trap (invoke "trap_wrapper") "builtin trap")

(assert_return (invoke "getLengthOfLinkedMemory_wrapper") (i32.const 0x200))

(assert_return (invoke "getU8_wrapper_param" (i32.const 50)) (i32.const 0x32))
(assert_return (invoke "getI8_wrapper_param" (i32.const 50)) (i32.const 0x32))
(assert_return (invoke "getU16_wrapper_param" (i32.const 50)) (i32.const 0x3332))
(assert_return (invoke "getI16_wrapper_param" (i32.const 50)) (i32.const 0x3332))
(assert_return (invoke "getU32_wrapper_param" (i32.const 50)) (i32.const 0x35343332))
(assert_return (invoke "getI32_wrapper_param" (i32.const 50)) (i32.const 0x35343332))
(assert_return (invoke "getU64_wrapper_param" (i32.const 50)) (i64.const 0x3938373635343332))
(assert_return (invoke "getI64_wrapper_param" (i32.const 50)) (i64.const 0x3938373635343332))
(assert_return (invoke "getF32_wrapper_param" (i32.const 50)) (f32.const 6.71297243570734281092882156372E-7))
(assert_return (invoke "getF64_wrapper_param" (i32.const 50)) (f64.const 4.66376857016656175685505444808E-33))
(assert_return (invoke "getU8_const_low_param") (i32.const 0x32))
(assert_return (invoke "getI8_const_low_param") (i32.const 0x32))
(assert_return (invoke "getU16_const_low_param") (i32.const 0x3332))
(assert_return (invoke "getI16_const_low_param") (i32.const 0x3332))
(assert_return (invoke "getU32_const_low_param") (i32.const 0x35343332))
(assert_return (invoke "getI32_const_low_param") (i32.const 0x35343332))
(assert_return (invoke "getU64_const_low_param") (i64.const 0x3938373635343332))
(assert_return (invoke "getI64_const_low_param") (i64.const 0x3938373635343332))
(assert_return (invoke "getF32_const_low_param") (f32.const 6.71297243570734281092882156372E-7))
(assert_return (invoke "getF64_const_low_param") (f64.const 4.66376857016656175685505444808E-33))

(assert_return (invoke "getU8_wrapper_param" (i32.const 350)) (i32.const 0x5E))
(assert_return (invoke "getI8_wrapper_param" (i32.const 350)) (i32.const 0x5E))
(assert_return (invoke "getU16_wrapper_param" (i32.const 350)) (i32.const 0x5F5E))
(assert_return (invoke "getI16_wrapper_param" (i32.const 350)) (i32.const 0x5F5E))
(assert_return (invoke "getU32_wrapper_param" (i32.const 350)) (i32.const 0x61605F5E))
(assert_return (invoke "getI32_wrapper_param" (i32.const 350)) (i32.const 0x61605F5E))
(assert_return (invoke "getU64_wrapper_param" (i32.const 350)) (i64.const 0x6564636261605F5E))
(assert_return (invoke "getI64_wrapper_param" (i32.const 350)) (i64.const 0x6564636261605F5E))
(assert_return (invoke "getF32_wrapper_param" (i32.const 350)) (f32.const 2.58683912662022094848E20))
(assert_return (invoke "getF64_wrapper_param" (i32.const 350)) (f64.const 2.64378862377478026681916569123E180))
(assert_return (invoke "getU8_const_mid_param") (i32.const 0x5E))
(assert_return (invoke "getI8_const_mid_param") (i32.const 0x5E))
(assert_return (invoke "getU16_const_mid_param") (i32.const 0x5F5E))
(assert_return (invoke "getI16_const_mid_param") (i32.const 0x5F5E))
(assert_return (invoke "getU32_const_mid_param") (i32.const 0x61605F5E))
(assert_return (invoke "getI32_const_mid_param") (i32.const 0x61605F5E))
(assert_return (invoke "getU64_const_mid_param") (i64.const 0x6564636261605F5E))
(assert_return (invoke "getI64_const_mid_param") (i64.const 0x6564636261605F5E))
(assert_return (invoke "getF32_const_mid_param") (f32.const 2.58683912662022094848E20))
(assert_return (invoke "getF64_const_mid_param") (f64.const 2.64378862377478026681916569123E180))

(assert_return (invoke "getU8_wrapper_param" (i32.const 500)) (i32.const 0xF4))
(assert_return (invoke "getI8_wrapper_param" (i32.const 500)) (i32.const 0xFFFFFFF4))
(assert_return (invoke "getU16_wrapper_param" (i32.const 500)) (i32.const 0xF5F4))
(assert_return (invoke "getI16_wrapper_param" (i32.const 500)) (i32.const 0xFFFFF5F4))
(assert_return (invoke "getU32_wrapper_param" (i32.const 500)) (i32.const 0xF7F6F5F4))
(assert_return (invoke "getI32_wrapper_param" (i32.const 500)) (i32.const 0xF7F6F5F4))
(assert_return (invoke "getU64_wrapper_param" (i32.const 500)) (i64.const 0xFBFAF9F8F7F6F5F4))
(assert_return (invoke "getI64_wrapper_param" (i32.const 500)) (i64.const 0xFBFAF9F8F7F6F5F4))
(assert_return (invoke "getF32_wrapper_param" (i32.const 500)) (f32.const -1.00179183533134041903964218615E34))
(assert_return (invoke "getF64_wrapper_param" (i32.const 500)) (f64.const -1.64308766832114935039124975337E289))
(assert_return (invoke "getU8_const_high_param") (i32.const 0xF4))
(assert_return (invoke "getI8_const_high_param") (i32.const 0xFFFFFFF4))
(assert_return (invoke "getU16_const_high_param") (i32.const 0xF5F4))
(assert_return (invoke "getI16_const_high_param") (i32.const 0xFFFFF5F4))
(assert_return (invoke "getU32_const_high_param") (i32.const 0xF7F6F5F4))
(assert_return (invoke "getI32_const_high_param") (i32.const 0xF7F6F5F4))
(assert_return (invoke "getU64_const_high_param") (i64.const 0xFBFAF9F8F7F6F5F4))
(assert_return (invoke "getI64_const_high_param") (i64.const 0xFBFAF9F8F7F6F5F4))
(assert_return (invoke "getF32_const_high_param") (f32.const -1.00179183533134041903964218615E34))
(assert_return (invoke "getF64_const_high_param") (f64.const -1.64308766832114935039124975337E289))

(assert_return (invoke "getU8_wrapper_param" (i32.const 511)) (i32.const 0xFF))
(assert_return (invoke "getI8_wrapper_param" (i32.const 511)) (i32.const 0xFFFFFFFF))
(assert_return (invoke "getU16_wrapper_param" (i32.const 510)) (i32.const 0xFFFE))
(assert_return (invoke "getI16_wrapper_param" (i32.const 510)) (i32.const 0xFFFFFFFE))
(assert_return (invoke "getU32_wrapper_param" (i32.const 508)) (i32.const 0xFFFEFDFC))
(assert_return (invoke "getI32_wrapper_param" (i32.const 508)) (i32.const 0xFFFEFDFC))
(assert_return (invoke "getU64_wrapper_param" (i32.const 504)) (i64.const 0xFFFEFDFCFBFAF9F8))
(assert_return (invoke "getI64_wrapper_param" (i32.const 504)) (i64.const 0xFFFEFDFCFBFAF9F8))
(assert_return (invoke "getF32_wrapper_param" (i32.const 508)) (f32.const -nan:0x7EFDFC))
(assert_return (invoke "getF64_wrapper_param" (i32.const 504)) (f64.const -nan:0xEFDFCFBFAF9F8))
(assert_return (invoke "getU8_const_maximum_param") (i32.const 0xFF))
(assert_return (invoke "getI8_const_maximum_param") (i32.const 0xFFFFFFFF))
(assert_return (invoke "getU16_const_maximum_param") (i32.const 0xFFFE))
(assert_return (invoke "getI16_const_maximum_param") (i32.const 0xFFFFFFFE))
(assert_return (invoke "getU32_const_maximum_param") (i32.const 0xFFFEFDFC))
(assert_return (invoke "getI32_const_maximum_param") (i32.const 0xFFFEFDFC))
(assert_return (invoke "getU64_const_maximum_param") (i64.const 0xFFFEFDFCFBFAF9F8))
(assert_return (invoke "getI64_const_maximum_param") (i64.const 0xFFFEFDFCFBFAF9F8))
(assert_return (invoke "getF32_const_maximum_param") (f32.const -nan:0x7EFDFC))
(assert_return (invoke "getF64_const_maximum_param") (f64.const -nan:0xEFDFCFBFAF9F8))

(assert_trap (invoke "getU8_wrapper_param" (i32.const 512)) "out of bounds linked memory access")
(assert_trap (invoke "getI8_wrapper_param" (i32.const 512)) "out of bounds linked memory access")
(assert_trap (invoke "getU16_wrapper_param" (i32.const 511)) "out of bounds linked memory access")
(assert_trap (invoke "getI16_wrapper_param" (i32.const 511)) "out of bounds linked memory access")
(assert_trap (invoke "getU32_wrapper_param" (i32.const 509)) "out of bounds linked memory access")
(assert_trap (invoke "getI32_wrapper_param" (i32.const 509)) "out of bounds linked memory access")
(assert_trap (invoke "getU64_wrapper_param" (i32.const 505)) "out of bounds linked memory access")
(assert_trap (invoke "getI64_wrapper_param" (i32.const 505)) "out of bounds linked memory access")
(assert_trap (invoke "getF32_wrapper_param" (i32.const 509)) "out of bounds linked memory access")
(assert_trap (invoke "getF64_wrapper_param" (i32.const 505)) "out of bounds linked memory access")
(assert_trap (invoke "getU8_const_toohigh_param") "out of bounds linked memory access")
(assert_trap (invoke "getI8_const_toohigh_param") "out of bounds linked memory access")
(assert_trap (invoke "getU16_const_toohigh_param") "out of bounds linked memory access")
(assert_trap (invoke "getI16_const_toohigh_param") "out of bounds linked memory access")
(assert_trap (invoke "getU32_const_toohigh_param") "out of bounds linked memory access")
(assert_trap (invoke "getI32_const_toohigh_param") "out of bounds linked memory access")
(assert_trap (invoke "getU64_const_toohigh_param") "out of bounds linked memory access")
(assert_trap (invoke "getI64_const_toohigh_param") "out of bounds linked memory access")
(assert_trap (invoke "getF32_const_toohigh_param") "out of bounds linked memory access")
(assert_trap (invoke "getF64_const_toohigh_param") "out of bounds linked memory access")

(assert_trap (invoke "getU8_wrapper_param" (i32.const -1)) "out of bounds linked memory access")
(assert_trap (invoke "getI8_wrapper_param" (i32.const -1)) "out of bounds linked memory access")
(assert_trap (invoke "getU16_wrapper_param" (i32.const -1)) "out of bounds linked memory access")
(assert_trap (invoke "getI16_wrapper_param" (i32.const -1)) "out of bounds linked memory access")
(assert_trap (invoke "getU32_wrapper_param" (i32.const -1)) "out of bounds linked memory access")
(assert_trap (invoke "getI32_wrapper_param" (i32.const -1)) "out of bounds linked memory access")
(assert_trap (invoke "getU64_wrapper_param" (i32.const -1)) "out of bounds linked memory access")
(assert_trap (invoke "getI64_wrapper_param" (i32.const -1)) "out of bounds linked memory access")
(assert_trap (invoke "getF32_wrapper_param" (i32.const -1)) "out of bounds linked memory access")
(assert_trap (invoke "getF64_wrapper_param" (i32.const -1)) "out of bounds linked memory access")
(assert_trap (invoke "getU8_const_far_param") "out of bounds linked memory access")
(assert_trap (invoke "getI8_const_far_param") "out of bounds linked memory access")
(assert_trap (invoke "getU16_const_far_param") "out of bounds linked memory access")
(assert_trap (invoke "getI16_const_far_param") "out of bounds linked memory access")
(assert_trap (invoke "getU32_const_far_param") "out of bounds linked memory access")
(assert_trap (invoke "getI32_const_far_param") "out of bounds linked memory access")
(assert_trap (invoke "getU64_const_far_param") "out of bounds linked memory access")
(assert_trap (invoke "getI64_const_far_param") "out of bounds linked memory access")
(assert_trap (invoke "getF32_const_far_param") "out of bounds linked memory access")
(assert_trap (invoke "getF64_const_far_param") "out of bounds linked memory access")

(assert_invalid
  (module
    (import "builtin" "getU8FromLinkedMemory" (func $getU8FromLinkedMemory (param i32) (result i32)))
    (export "reexport" (func $getU8FromLinkedMemory))
  )
  "cannot export builtins"
)

(assert_invalid
  (module
    (type $type (func (param i32) (result i32)))
    (import "builtin" "getU8FromLinkedMemory" (func $getU8FromLinkedMemory (param i32) (result i32)))
    (func (result i32)
    	i32.const 1
    	i32.const 0
    	call_indirect (type $type)
    )
    (table 10 funcref)
    (elem (i32.const 0) $getU8FromLinkedMemory)
  )
  "cannot indirectly call builtin functions"
)

(module
  (type (func))

  (import "builtin" "isFunctionLinked" (func (param i32) (result i32)))
  (import "builtin" "trap" (func))
  (import "spectest" "requestInterruption" (func $requestInterruption))
  (import "unknown" "import" (func $unknown))

  (func (export "test_const0") (result i32)
    i32.const 0
    call 0
  )
  (func (export "test_const1") (result i32)
    i32.const 1
    call 0
  )
  (func (export "test_const2") (result i32)
    i32.const 2
    call 0
  )
  (func (export "test_const3") (result i32)
    i32.const 3
    call 0
  )
  (func (export "test_const100") (result i32)
    i32.const 100
    call 0
  )
  (func (export "test_constmax") (result i32)
    i32.const 0xFFFFFFFF
    call 0
  )

  (func (export "test_from_input") (param i32) (result i32)
    local.get 0
    call 0
  )

  (func (export "call_unknown_direct")
    call $unknown
  )
  (func (export "call_unknown_indirect")
    i32.const 0
    call_indirect 0
  )
  (table 3 funcref)
  (elem (i32.const 1) $unknown $requestInterruption)
)

(assert_return (invoke "test_const0") (i32.const 0))
(assert_return (invoke "test_const1") (i32.const 0))
(assert_return (invoke "test_const2") (i32.const 1))
(assert_return (invoke "test_const3") (i32.const 0))
(assert_return (invoke "test_const100") (i32.const 0))
(assert_return (invoke "test_constmax") (i32.const 0))

(assert_return (invoke "test_from_input" (i32.const 0)) (i32.const 0))
(assert_return (invoke "test_from_input" (i32.const 1)) (i32.const 0))
(assert_return (invoke "test_from_input" (i32.const 2)) (i32.const 1))
(assert_return (invoke "test_from_input" (i32.const 3)) (i32.const 0))
(assert_return (invoke "test_from_input" (i32.const 100)) (i32.const 0))
(assert_return (invoke "test_from_input" (i32.const 0xFFFFFFFF)) (i32.const 0))

(assert_trap (invoke "call_unknown_direct") "called function not linked")
(assert_trap (invoke "call_unknown_indirect") "indirect call not linked")

;;test table size larger than arm64 and tricore imm
(module
  (type (func))

  (import "builtin" "isFunctionLinked" (func (param i32) (result i32)))
  (import "builtin" "trap" (func))
  (import "spectest" "requestInterruption" (func $requestInterruption))
  (import "unknown" "import" (func $unknown))


  (func (export "test_from_input") (param i32) (result i32)
    local.get 0
    call 0
  )

  (table 6000 funcref)
  (elem (i32.const 5001) $unknown $requestInterruption)
)

(assert_return (invoke "test_from_input" (i32.const 5000)) (i32.const 0))
(assert_return (invoke "test_from_input" (i32.const 5001)) (i32.const 0))
(assert_return (invoke "test_from_input" (i32.const 5002)) (i32.const 1))
(assert_return (invoke "test_from_input" (i32.const 5003)) (i32.const 0))

(module
  (import "builtin" "getU8FromLinkedMemory" (func $getU8FromLinkedMemory (param i32) (result i32)))
  (import "builtin" "copyFromLinkedMemory" (func $copyFromLinkedMemory (param i32 i32 i32)))

  (func $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (param $dst i32) (param $src i32) (param $sz i32) (result i32)
    (local $acc i32)

    (call $copyFromLinkedMemory (local.get $dst) (local.get $src) (local.get $sz))

    block
      loop
        ;; Check that sz local is non-zero
        local.get $sz
        i32.eqz
        br_if 1

        ;; Load byte from linear memory
        local.get $dst
        i32.load8_u

        ;; Add to accumulator
        local.get $acc
        i32.add
        local.set $acc
        
        ;; Increment dst local
        local.get $dst
        i32.const 1
        i32.add
        local.set $dst

        ;; Increment src local
        local.get $src
        i32.const 1
        i32.add
        local.set $src

        ;; Decrement sz local
        local.get $sz
        i32.const 1
        i32.sub
        local.set $sz

        br 0
      end
    end
    local.get $acc
  )

  (func (export "copy_wrapper_param") (param $dst i32) (param $src i32) (param $sz i32) (result i32) (
    call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (local.get $dst) (local.get $src) (local.get $sz)
  ))

  (func (export "copy_const_low_low_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 0) (i32.const 0) (i32.const 50)))
  (func (export "copy_const_low_mid_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 0) (i32.const 300) (i32.const 50)))
  (func (export "copy_const_low_high_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 0) (i32.const 450) (i32.const 50)))
  (func (export "copy_const_low_maximum_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 0) (i32.const 462) (i32.const 50)))
  (func (export "copy_const_low_toohigh_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 0) (i32.const 463) (i32.const 50)))
  (func (export "copy_const_low_far_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 0) (i32.const -1) (i32.const 50)))

  (func (export "copy_const_mid0_low_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 16330) (i32.const 0) (i32.const 50)))
  (func (export "copy_const_mid0_mid_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 16330) (i32.const 300) (i32.const 50)))
  (func (export "copy_const_mid0_high_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 16330) (i32.const 450) (i32.const 50)))
  (func (export "copy_const_mid0_maximum_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 16330) (i32.const 462) (i32.const 50)))
  (func (export "copy_const_mid0_toohigh_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 16330) (i32.const 463) (i32.const 50)))
  (func (export "copy_const_mid0_far_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 16330) (i32.const -1) (i32.const 50)))

  ;; Check that accesses spanning a memory page (OS not Wasm page) works on the first try
  (func (export "copy_const_mid1_low_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 16380) (i32.const 0) (i32.const 50)))
  (func (export "copy_const_mid1_mid_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 16380) (i32.const 300) (i32.const 50)))
  (func (export "copy_const_mid1_high_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 16380) (i32.const 450) (i32.const 50)))
  (func (export "copy_const_mid1_maximum_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 16380) (i32.const 462) (i32.const 50)))
  (func (export "copy_const_mid1_toohigh_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 16380) (i32.const 463) (i32.const 50)))
  (func (export "copy_const_mid1_far_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 16380) (i32.const -1) (i32.const 50)))

  ;; Access the already accessed page
  (func (export "copy_const_mid2_low_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 16384) (i32.const 0) (i32.const 50)))
  (func (export "copy_const_mid2_mid_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 16384) (i32.const 300) (i32.const 50)))
  (func (export "copy_const_mid2_high_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 16384) (i32.const 450) (i32.const 50)))
  (func (export "copy_const_mid2_maximum_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 16384) (i32.const 462) (i32.const 50)))
  (func (export "copy_const_mid2_toohigh_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 16384) (i32.const 463) (i32.const 50)))
  (func (export "copy_const_mid2_far_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 16384) (i32.const -1) (i32.const 50)))

  ;; Check that accessing a new memory page (OS not Wasm page) works on the first try
  (func (export "copy_const_mid3_low_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 32768) (i32.const 0) (i32.const 16)))
  (func (export "copy_const_mid3_mid_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 32768) (i32.const 300) (i32.const 16)))
  (func (export "copy_const_mid3_high_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 32768) (i32.const 450) (i32.const 16)))
  (func (export "copy_const_mid3_maximum_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 32768) (i32.const 496) (i32.const 16)))
  (func (export "copy_const_mid3_toohigh_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 32768) (i32.const 497) (i32.const 16)))
  (func (export "copy_const_mid3_far_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 32768) (i32.const -1) (i32.const 16)))

  (func (export "copy_const_high_low_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 450) (i32.const 0) (i32.const 50)))
  (func (export "copy_const_high_mid_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 450) (i32.const 300) (i32.const 50)))
  (func (export "copy_const_high_high_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 450) (i32.const 450) (i32.const 50)))
  (func (export "copy_const_high_maximum_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 450) (i32.const 462) (i32.const 50)))
  (func (export "copy_const_high_toohigh_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 450) (i32.const 463) (i32.const 50)))
  (func (export "copy_const_high_far_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 450) (i32.const -1) (i32.const 50)))

  (func (export "copy_const_maximum_low_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 65486) (i32.const 0) (i32.const 50)))
  (func (export "copy_const_maximum_mid_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 65486) (i32.const 300) (i32.const 50)))
  (func (export "copy_const_maximum_high_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 65486) (i32.const 450) (i32.const 50)))
  (func (export "copy_const_maximum_maximum_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 65486) (i32.const 462) (i32.const 50)))
  (func (export "copy_const_maximum_toohigh_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 65486) (i32.const 463) (i32.const 50)))
  (func (export "copy_const_maximum_far_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 65486) (i32.const -1) (i32.const 50)))

  (func (export "copy_const_toohigh_low_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 65487) (i32.const 0) (i32.const 50)))
  (func (export "copy_const_toohigh_mid_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 65487) (i32.const 300) (i32.const 50)))
  (func (export "copy_const_toohigh_high_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 65487) (i32.const 450) (i32.const 50)))
  (func (export "copy_const_toohigh_maximum_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 65487) (i32.const 462) (i32.const 50)))
  (func (export "copy_const_toohigh_toohigh_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 65487) (i32.const 463) (i32.const 50)))
  (func (export "copy_const_toohigh_far_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 65487) (i32.const -1) (i32.const 50)))

  (func (export "copy_const_far_low_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const -1) (i32.const 0) (i32.const 50)))
  (func (export "copy_const_far_mid_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const -1) (i32.const 300) (i32.const 50)))
  (func (export "copy_const_far_high_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const -1) (i32.const 450) (i32.const 50)))
  (func (export "copy_const_far_maximum_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const -1) (i32.const 462) (i32.const 50)))
  (func (export "copy_const_far_toohigh_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const -1) (i32.const 463) (i32.const 50)))
  (func (export "copy_const_far_far_param") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const -1) (i32.const -1) (i32.const 50)))

  (func (export "copy_const_low_low_0") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 0) (i32.const 0) (i32.const 0)))
  (func (export "copy_const_low_low_4gb") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 0) (i32.const 0) (i32.const -1)))

  (func (export "copy_const_far_far_0") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const -1) (i32.const -1) (i32.const 0)))
  (func (export "copy_const_far_far_4gb") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const -1) (i32.const -1) (i32.const -1)))

  (func (export "copy_const_aligned_aligned_1") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 4) (i32.const 4) (i32.const 1)))
  (func (export "copy_const_unaligned_aligned_1") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 3) (i32.const 4) (i32.const 1)))
  (func (export "copy_const_aligned_unaligned_1") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 4) (i32.const 3) (i32.const 1)))
  (func (export "copy_const_unaligned_unaligned_1") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 3) (i32.const 3) (i32.const 1)))

  (func (export "copy_const_aligned_aligned_15") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 4) (i32.const 4) (i32.const 15)))
  (func (export "copy_const_unaligned_aligned_15") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 3) (i32.const 4) (i32.const 15)))
  (func (export "copy_const_aligned_unaligned_15") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 4) (i32.const 3) (i32.const 15)))
  (func (export "copy_const_unaligned_unaligned_15") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 3) (i32.const 3) (i32.const 15)))

  (func (export "copy_const_aligned_aligned_16") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 4) (i32.const 4) (i32.const 16)))
  (func (export "copy_const_unaligned_aligned_16") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 3) (i32.const 4) (i32.const 16)))
  (func (export "copy_const_aligned_unaligned_16") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 4) (i32.const 3) (i32.const 16)))
  (func (export "copy_const_unaligned_unaligned_16") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 3) (i32.const 3) (i32.const 16)))

  (func (export "copy_const_aligned_aligned_17") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 4) (i32.const 4) (i32.const 17)))
  (func (export "copy_const_unaligned_aligned_17") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 3) (i32.const 4) (i32.const 17)))
  (func (export "copy_const_aligned_unaligned_17") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 4) (i32.const 3) (i32.const 17)))
  (func (export "copy_const_unaligned_unaligned_17") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 3) (i32.const 3) (i32.const 17)))

  (func (export "copy_const_aligned_aligned_4gb") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 4) (i32.const 4) (i32.const -1)))
  (func (export "copy_const_unaligned_aligned_4gb") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 3) (i32.const 4) (i32.const -1)))
  (func (export "copy_const_aligned_unaligned_4gb") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 4) (i32.const 3) (i32.const -1)))
  (func (export "copy_const_unaligned_unaligned_4gb") (result i32) (call $copyFromLinkedMemoryAndCalculateSumOfLinearMemory (i32.const 3) (i32.const 3) (i32.const -1)))

  (memory 1)
)

(assert_return (invoke "copy_wrapper_param" (i32.const 0) (i32.const 0) (i32.const 0)) (i32.const 0x00))
(assert_return (invoke "copy_wrapper_param" (i32.const 1) (i32.const 0) (i32.const 0)) (i32.const 0x00))
(assert_return (invoke "copy_wrapper_param" (i32.const 0) (i32.const 1) (i32.const 0)) (i32.const 0x00))
(assert_return (invoke "copy_wrapper_param" (i32.const 1) (i32.const 1) (i32.const 0)) (i32.const 0x00))

(assert_return (invoke "copy_wrapper_param" (i32.const 0) (i32.const 0) (i32.const 1)) (i32.const 0x00))
(assert_return (invoke "copy_wrapper_param" (i32.const 1) (i32.const 0) (i32.const 1)) (i32.const 0x00))
(assert_return (invoke "copy_wrapper_param" (i32.const 0) (i32.const 1) (i32.const 1)) (i32.const 0x01))
(assert_return (invoke "copy_wrapper_param" (i32.const 1) (i32.const 1) (i32.const 1)) (i32.const 0x01))

(assert_return (invoke "copy_wrapper_param" (i32.const 0) (i32.const 0) (i32.const 2)) (i32.const 0x01))
(assert_return (invoke "copy_wrapper_param" (i32.const 1) (i32.const 0) (i32.const 2)) (i32.const 0x01))
(assert_return (invoke "copy_wrapper_param" (i32.const 0) (i32.const 1) (i32.const 2)) (i32.const 0x03))
(assert_return (invoke "copy_wrapper_param" (i32.const 1) (i32.const 1) (i32.const 2)) (i32.const 0x03))

(assert_return (invoke "copy_wrapper_param" (i32.const 0) (i32.const 0) (i32.const 7)) (i32.const 0x15))
(assert_return (invoke "copy_wrapper_param" (i32.const 1) (i32.const 0) (i32.const 7)) (i32.const 0x15))
(assert_return (invoke "copy_wrapper_param" (i32.const 0) (i32.const 1) (i32.const 7)) (i32.const 0x1c))
(assert_return (invoke "copy_wrapper_param" (i32.const 1) (i32.const 1) (i32.const 7)) (i32.const 0x1c))

(assert_return (invoke "copy_wrapper_param" (i32.const 0) (i32.const 0) (i32.const 8)) (i32.const 0x1c))
(assert_return (invoke "copy_wrapper_param" (i32.const 1) (i32.const 0) (i32.const 8)) (i32.const 0x1c))
(assert_return (invoke "copy_wrapper_param" (i32.const 0) (i32.const 1) (i32.const 8)) (i32.const 0x24))
(assert_return (invoke "copy_wrapper_param" (i32.const 1) (i32.const 1) (i32.const 8)) (i32.const 0x24))

(assert_return (invoke "copy_wrapper_param" (i32.const 0) (i32.const 0) (i32.const 9)) (i32.const 0x24))
(assert_return (invoke "copy_wrapper_param" (i32.const 1) (i32.const 0) (i32.const 9)) (i32.const 0x24))
(assert_return (invoke "copy_wrapper_param" (i32.const 0) (i32.const 1) (i32.const 9)) (i32.const 0x2d))
(assert_return (invoke "copy_wrapper_param" (i32.const 1) (i32.const 1) (i32.const 9)) (i32.const 0x2d))

(assert_return (invoke "copy_wrapper_param" (i32.const 0) (i32.const 0) (i32.const 15)) (i32.const 0x69))
(assert_return (invoke "copy_wrapper_param" (i32.const 1) (i32.const 0) (i32.const 15)) (i32.const 0x69))
(assert_return (invoke "copy_wrapper_param" (i32.const 0) (i32.const 1) (i32.const 15)) (i32.const 0x78))
(assert_return (invoke "copy_wrapper_param" (i32.const 1) (i32.const 1) (i32.const 15)) (i32.const 0x78))

(assert_return (invoke "copy_wrapper_param" (i32.const 0) (i32.const 0) (i32.const 16)) (i32.const 0x78))
(assert_return (invoke "copy_wrapper_param" (i32.const 1) (i32.const 0) (i32.const 16)) (i32.const 0x78))
(assert_return (invoke "copy_wrapper_param" (i32.const 0) (i32.const 1) (i32.const 16)) (i32.const 0x88))
(assert_return (invoke "copy_wrapper_param" (i32.const 1) (i32.const 1) (i32.const 16)) (i32.const 0x88))

(assert_return (invoke "copy_wrapper_param" (i32.const 0) (i32.const 0) (i32.const 17)) (i32.const 0x88))
(assert_return (invoke "copy_wrapper_param" (i32.const 1) (i32.const 0) (i32.const 17)) (i32.const 0x88))
(assert_return (invoke "copy_wrapper_param" (i32.const 0) (i32.const 1) (i32.const 17)) (i32.const 0x99))
(assert_return (invoke "copy_wrapper_param" (i32.const 1) (i32.const 1) (i32.const 17)) (i32.const 0x99))

(assert_return (invoke "copy_wrapper_param" (i32.const 0) (i32.const 256) (i32.const 256)) (i32.const 0x7f80))
(assert_return (invoke "copy_wrapper_param" (i32.const 1) (i32.const 256) (i32.const 256)) (i32.const 0x7f80))
(assert_return (invoke "copy_wrapper_param" (i32.const 256) (i32.const 256) (i32.const 256)) (i32.const 0x7f80))

(assert_return (invoke "copy_wrapper_param" (i32.const 0) (i32.const 0) (i32.const 512)) (i32.const 0xff00))
(assert_return (invoke "copy_wrapper_param" (i32.const 1) (i32.const 0) (i32.const 512)) (i32.const 0xff00))
(assert_return (invoke "copy_wrapper_param" (i32.const 256) (i32.const 0) (i32.const 512)) (i32.const 0xff00))

(assert_trap (invoke "copy_wrapper_param" (i32.const 0) (i32.const 1) (i32.const 512)) "out of bounds linked memory access")
(assert_trap (invoke "copy_wrapper_param" (i32.const 1) (i32.const 1) (i32.const 512)) "out of bounds linked memory access")
(assert_trap (invoke "copy_wrapper_param" (i32.const 256) (i32.const 1) (i32.const 512)) "out of bounds linked memory access")

(assert_return (invoke "copy_wrapper_param" (i32.const 0) (i32.const 512) (i32.const 0)) (i32.const 0))
(assert_trap (invoke "copy_wrapper_param" (i32.const 0) (i32.const 513) (i32.const 0)) "out of bounds linked memory access")

(assert_trap (invoke "copy_wrapper_param" (i32.const -1) (i32.const 0) (i32.const 0)) "out of bounds linear memory access")

(assert_trap (invoke "copy_wrapper_param" (i32.const 0) (i32.const 0) (i32.const -1)) "out of bounds memory access")
(assert_trap (invoke "copy_wrapper_param" (i32.const 1) (i32.const 0) (i32.const -1)) "out of bounds linear memory access")
(assert_trap (invoke "copy_wrapper_param" (i32.const 0) (i32.const 1) (i32.const -1)) "out of bounds memory access")
(assert_trap (invoke "copy_wrapper_param" (i32.const 1) (i32.const 1) (i32.const -1)) "out of bounds linear memory access")

(assert_trap (invoke "copy_wrapper_param" (i32.const -1) (i32.const 0) (i32.const 1)) "out of bounds linear memory access")
(assert_trap (invoke "copy_wrapper_param" (i32.const -1) (i32.const 1) (i32.const 1)) "out of bounds linear memory access")
(assert_trap (invoke "copy_wrapper_param" (i32.const -1) (i32.const 1) (i32.const 1)) "out of bounds linear memory access")

(assert_trap (invoke "copy_wrapper_param" (i32.const 1) (i32.const -1) (i32.const 1)) "out of bounds linked memory access")
(assert_trap (invoke "copy_wrapper_param" (i32.const 0) (i32.const -1) (i32.const 1)) "out of bounds linked memory access")
(assert_trap (invoke "copy_wrapper_param" (i32.const 1) (i32.const -1) (i32.const 1)) "out of bounds linked memory access")

(assert_return (invoke "copy_const_low_low_param") (i32.const 0x4C9))
(assert_return (invoke "copy_const_low_mid_param") (i32.const 0xD61))
(assert_return (invoke "copy_const_low_high_param") (i32.const 0x2AAD))
(assert_return (invoke "copy_const_low_maximum_param") (i32.const 0x2D05))
(assert_trap (invoke "copy_const_low_toohigh_param") "out of bounds linked memory access")
(assert_trap (invoke "copy_const_low_far_param") "out of bounds linked memory access")

(assert_return (invoke "copy_const_mid0_low_param") (i32.const 0x4C9))
(assert_return (invoke "copy_const_mid0_mid_param") (i32.const 0xD61))
(assert_return (invoke "copy_const_mid0_high_param") (i32.const 0x2AAD))
(assert_return (invoke "copy_const_mid0_maximum_param") (i32.const 0x2D05))
(assert_trap (invoke "copy_const_mid0_toohigh_param") "out of bounds linked memory access")
(assert_trap (invoke "copy_const_mid0_far_param") "out of bounds linked memory access")

(assert_return (invoke "copy_const_mid1_low_param") (i32.const 0x4C9))
(assert_return (invoke "copy_const_mid1_mid_param") (i32.const 0xD61))
(assert_return (invoke "copy_const_mid1_high_param") (i32.const 0x2AAD))
(assert_return (invoke "copy_const_mid1_maximum_param") (i32.const 0x2D05))
(assert_trap (invoke "copy_const_mid1_toohigh_param") "out of bounds linked memory access")
(assert_trap (invoke "copy_const_mid1_far_param") "out of bounds linked memory access")

(assert_return (invoke "copy_const_mid2_low_param") (i32.const 0x4C9))
(assert_return (invoke "copy_const_mid2_mid_param") (i32.const 0xD61))
(assert_return (invoke "copy_const_mid2_high_param") (i32.const 0x2AAD))
(assert_return (invoke "copy_const_mid2_maximum_param") (i32.const 0x2D05))
(assert_trap (invoke "copy_const_mid2_toohigh_param") "out of bounds linked memory access")
(assert_trap (invoke "copy_const_mid2_far_param") "out of bounds linked memory access")

(assert_return (invoke "copy_const_mid3_low_param") (i32.const 0x78))
(assert_return (invoke "copy_const_mid3_mid_param") (i32.const 0x338))
(assert_return (invoke "copy_const_mid3_high_param") (i32.const 0xC98))
(assert_return (invoke "copy_const_mid3_maximum_param") (i32.const 0xF78))
(assert_trap (invoke "copy_const_mid3_toohigh_param") "out of bounds linked memory access")
(assert_trap (invoke "copy_const_mid3_far_param") "out of bounds linked memory access")

(assert_return (invoke "copy_const_high_low_param") (i32.const 0x4C9))
(assert_return (invoke "copy_const_high_mid_param") (i32.const 0xD61))
(assert_return (invoke "copy_const_high_high_param") (i32.const 0x2AAD))
(assert_return (invoke "copy_const_high_maximum_param") (i32.const 0x2D05))
(assert_trap (invoke "copy_const_high_toohigh_param") "out of bounds linked memory access")
(assert_trap (invoke "copy_const_high_far_param") "out of bounds linked memory access")

(assert_return (invoke "copy_const_maximum_low_param") (i32.const 0x4C9))
(assert_return (invoke "copy_const_maximum_mid_param") (i32.const 0xD61))
(assert_return (invoke "copy_const_maximum_high_param") (i32.const 0x2AAD))
(assert_return (invoke "copy_const_maximum_maximum_param") (i32.const 0x2D05))
(assert_trap (invoke "copy_const_maximum_toohigh_param") "out of bounds linked memory access")
(assert_trap (invoke "copy_const_maximum_far_param") "out of bounds linked memory access")

(assert_trap (invoke "copy_const_toohigh_low_param") "out of bounds linear memory access")
(assert_trap (invoke "copy_const_toohigh_mid_param") "out of bounds linear memory access")
(assert_trap (invoke "copy_const_toohigh_high_param") "out of bounds linear memory access")
(assert_trap (invoke "copy_const_toohigh_maximum_param") "out of bounds linear memory access")
(assert_trap (invoke "copy_const_toohigh_toohigh_param") "out of bounds memory access")
(assert_trap (invoke "copy_const_toohigh_far_param") "out of bounds memory access")

(assert_trap (invoke "copy_const_far_low_param") "out of bounds linear memory access")
(assert_trap (invoke "copy_const_far_mid_param") "out of bounds linear memory access")
(assert_trap (invoke "copy_const_far_high_param") "out of bounds linear memory access")
(assert_trap (invoke "copy_const_far_maximum_param") "out of bounds linear memory access")
(assert_trap (invoke "copy_const_far_toohigh_param") "out of bounds linear memory access")
(assert_trap (invoke "copy_const_far_far_param") "out of bounds linear memory access")

(assert_return (invoke "copy_const_low_low_0") (i32.const 0))
(assert_trap (invoke "copy_const_low_low_4gb") "out of bounds memory access")

(assert_trap (invoke "copy_const_far_far_0") "out of bounds memory access")
(assert_trap (invoke "copy_const_far_far_4gb") "out of bounds linear memory access")

(assert_return (invoke "copy_const_aligned_aligned_1") (i32.const 0x04))
(assert_return (invoke "copy_const_unaligned_aligned_1") (i32.const 0x04))
(assert_return (invoke "copy_const_aligned_unaligned_1") (i32.const 0x03))
(assert_return (invoke "copy_const_unaligned_unaligned_1") (i32.const 0x03))

(assert_return (invoke "copy_const_aligned_aligned_15") (i32.const 0xA5))
(assert_return (invoke "copy_const_unaligned_aligned_15") (i32.const 0xA5))
(assert_return (invoke "copy_const_aligned_unaligned_15") (i32.const 0x96))
(assert_return (invoke "copy_const_unaligned_unaligned_15") (i32.const 0x96))

(assert_return (invoke "copy_const_aligned_aligned_16") (i32.const 0xB8))
(assert_return (invoke "copy_const_unaligned_aligned_16") (i32.const 0xB8))
(assert_return (invoke "copy_const_aligned_unaligned_16") (i32.const 0xA8))
(assert_return (invoke "copy_const_unaligned_unaligned_16") (i32.const 0xA8))

(assert_return (invoke "copy_const_aligned_aligned_17") (i32.const 0xCC))
(assert_return (invoke "copy_const_unaligned_aligned_17") (i32.const 0xCC))
(assert_return (invoke "copy_const_aligned_unaligned_17") (i32.const 0xBB))
(assert_return (invoke "copy_const_unaligned_unaligned_17") (i32.const 0xBB))

(assert_trap (invoke "copy_const_aligned_aligned_4gb") "out of bounds linear memory access")
(assert_trap (invoke "copy_const_unaligned_aligned_4gb") "out of bounds linear memory access")
(assert_trap (invoke "copy_const_aligned_unaligned_4gb") "out of bounds linear memory access")
(assert_trap (invoke "copy_const_unaligned_unaligned_4gb") "out of bounds linear memory access")
