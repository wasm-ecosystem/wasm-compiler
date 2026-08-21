(module
  ;; AArch64 integer immediate cases.
  (func (export "aarch64_logical_i32") (result i32)
    i32.const 0x00030003
  )
  (func (export "aarch64_logical_i64") (result i64)
    i64.const 0x0003000300030003
  )
  (func (export "aarch64_movz_i32") (result i32)
    i32.const 0x00001234
  )
  (func (export "aarch64_movz_i64") (result i64)
    i64.const 0x0000000000001234
  )
  (func (export "aarch64_movz_zero_i64") (result i64)
    i64.const 0
  )
  (func (export "aarch64_movn_i32") (result i32)
    i32.const 0xFFFF1234
  )
  (func (export "aarch64_movn_i64") (result i64)
    i64.const 0xFFFFFFFFFFFF1234
  )
  (func (export "aarch64_movn_ones_i64") (result i64)
    i64.const 0xFFFFFFFFFFFFFFFF
  )
  (func (export "aarch64_multi_movwide_i32") (result i32)
    i32.const 0x00010002
  )
  (func (export "aarch64_multi_movwide_i64") (result i64)
    i64.const 0x0001000200030004
  )

  ;; AArch64 floating-point immediate cases, checked through their bit patterns.
  (func (export "aarch64_float_zero_f32") (result i32)
    f32.const 0
    i32.reinterpret_f32
  )
  (func (export "aarch64_float_zero_f64") (result i64)
    f64.const 0
    i64.reinterpret_f64
  )
  (func (export "aarch64_float_modified_f32") (result i32)
    f32.const 1.0
    i32.reinterpret_f32
  )
  (func (export "aarch64_float_modified_f64") (result i64)
    f64.const 1.0
    i64.reinterpret_f64
  )
  (func (export "aarch64_float_gpr_f32") (result i32)
    f32.const 0x1.000002p+0
    i32.reinterpret_f32
  )
  (func (export "aarch64_float_gpr_f64") (result i64)
    f64.const 0x1.0000000000001p+0
    i64.reinterpret_f64
  )

  ;; TriCore 32-bit immediate cases.
  (func (export "tricore_i32_signed4_pos") (result i32)
    i32.const 7
  )
  (func (export "tricore_i32_signed4_neg") (result i32)
    i32.const -8
  )
  (func (export "tricore_i32_unsigned16") (result i32)
    i32.const 0x0000FFFF
  )
  (func (export "tricore_i32_high16") (result i32)
    i32.const 0x12340000
  )
  (func (export "tricore_i32_signed16") (result i32)
    i32.const -32767
  )
  (func (export "tricore_i32_split") (result i32)
    i32.const 0x12345678
  )
  (func (export "tricore_f32_signed4") (result i32)
    f32.const -nan:0x7FFFF8
    i32.reinterpret_f32
  )
  (func (export "tricore_f32_unsigned16") (result i32)
    f32.const 0x1p-134
    i32.reinterpret_f32
  )
  (func (export "tricore_f32_high16") (result i32)
    f32.const -0x1.9ap-40
    i32.reinterpret_f32
  )
  (func (export "tricore_f32_signed16") (result i32)
    f32.const -nan:0x7F8001
    i32.reinterpret_f32
  )
  (func (export "tricore_f32_split") (result i32)
    f32.const -0x1.ca8642p-113
    i32.reinterpret_f32
  )

  ;; TriCore 64-bit immediate cases.
  (func (export "tricore_i64_signed4") (result i64)
    i64.const 7
  )
  (func (export "tricore_i64_signed16") (result i64)
    i64.const 0x0000000000007FFF
  )
  (func (export "tricore_i64_imask") (result i64)
    i64.const 0x00000000000F0000
  )
  (func (export "tricore_i64_split") (result i64)
    i64.const 0x123456789ABCDEF0
  )
  (func (export "tricore_f64_signed4") (result i64)
    f64.const -nan:0xFFFFFFFFFFFF8
    i64.reinterpret_f64
  )
  (func (export "tricore_f64_signed16") (result i64)
    f64.const -nan:0xFFFFFFFFF8000
    i64.reinterpret_f64
  )
  (func (export "tricore_f64_imask") (result i64)
    f64.const 0x0.0ffff00000005p-1022
    i64.reinterpret_f64
  )
  (func (export "tricore_f64_split") (result i64)
    f64.const 0x1.3456789abcdefp-1005
    i64.reinterpret_f64
  )

  ;; x86-64 integer immediate cases.
  (func (export "x86_i32") (result i32)
    i32.const -1
  )
  (func (export "x86_i64") (result i64)
    i64.const -1
  )
  (func (export "x86_f32") (result i32)
    f32.const 0
    i32.reinterpret_f32
  )
  (func (export "x86_f64") (result i64)
    f64.const -nan:0xFFFFFFFFFFFFF
    i64.reinterpret_f64
  )
)

(assert_return (invoke "aarch64_logical_i32") (i32.const 0x00030003))
(assert_return (invoke "aarch64_logical_i64") (i64.const 0x0003000300030003))
(assert_return (invoke "aarch64_movz_i32") (i32.const 0x00001234))
(assert_return (invoke "aarch64_movz_i64") (i64.const 0x0000000000001234))
(assert_return (invoke "aarch64_movz_zero_i64") (i64.const 0))
(assert_return (invoke "aarch64_movn_i32") (i32.const 0xFFFF1234))
(assert_return (invoke "aarch64_movn_i64") (i64.const 0xFFFFFFFFFFFF1234))
(assert_return (invoke "aarch64_movn_ones_i64") (i64.const 0xFFFFFFFFFFFFFFFF))
(assert_return (invoke "aarch64_multi_movwide_i32") (i32.const 0x00010002))
(assert_return (invoke "aarch64_multi_movwide_i64") (i64.const 0x0001000200030004))

(assert_return (invoke "aarch64_float_zero_f32") (i32.const 0))
(assert_return (invoke "aarch64_float_zero_f64") (i64.const 0))
(assert_return (invoke "aarch64_float_modified_f32") (i32.const 0x3F800000))
(assert_return (invoke "aarch64_float_modified_f64") (i64.const 0x3FF0000000000000))
(assert_return (invoke "aarch64_float_gpr_f32") (i32.const 0x3F800001))
(assert_return (invoke "aarch64_float_gpr_f64") (i64.const 0x3FF0000000000001))

(assert_return (invoke "tricore_i32_signed4_pos") (i32.const 7))
(assert_return (invoke "tricore_i32_signed4_neg") (i32.const -8))
(assert_return (invoke "tricore_i32_unsigned16") (i32.const 0x0000FFFF))
(assert_return (invoke "tricore_i32_high16") (i32.const 0x12340000))
(assert_return (invoke "tricore_i32_signed16") (i32.const -32767))
(assert_return (invoke "tricore_i32_split") (i32.const 0x12345678))
(assert_return (invoke "tricore_f32_signed4") (i32.const 0xFFFFFFF8))
(assert_return (invoke "tricore_f32_unsigned16") (i32.const 0x00008000))
(assert_return (invoke "tricore_f32_high16") (i32.const 0xABCD0000))
(assert_return (invoke "tricore_f32_signed16") (i32.const 0xFFFF8001))
(assert_return (invoke "tricore_f32_split") (i32.const 0x87654321))

(assert_return (invoke "tricore_i64_signed4") (i64.const 7))
(assert_return (invoke "tricore_i64_signed16") (i64.const 0x0000000000007FFF))
(assert_return (invoke "tricore_i64_imask") (i64.const 0x00000000000F0000))
(assert_return (invoke "tricore_i64_split") (i64.const 0x123456789ABCDEF0))
(assert_return (invoke "tricore_f64_signed4") (i64.const 0xFFFFFFFFFFFFFFF8))
(assert_return (invoke "tricore_f64_signed16") (i64.const 0xFFFFFFFFFFFF8000))
(assert_return (invoke "tricore_f64_imask") (i64.const 0x0000FFFF00000005))
(assert_return (invoke "tricore_f64_split") (i64.const 0x0123456789ABCDEF))

(assert_return (invoke "x86_i32") (i32.const -1))
(assert_return (invoke "x86_i64") (i64.const -1))
(assert_return (invoke "x86_f32") (i32.const 0))
(assert_return (invoke "x86_f64") (i64.const 0xFFFFFFFFFFFFFFFF))

;; MachineType::INVALID has no WebAssembly value type and is not representable here.
