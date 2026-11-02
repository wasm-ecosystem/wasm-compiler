(module
  (func $mixed_float_callee (param f32 f32 f64) (result f64)
    local.get 2)

  (func $mixed_float_tail_call (param f32 f64 f32) (result f64)
    local.get 2
    local.get 2
    local.get 1
    return_call $mixed_float_callee)

  (func (export "mixed_float_tail_call") (result f64)
    f32.const -0x1.fffb32p+14
    f64.const 0x1p+43
    f32.const 0x1.fffffep+127
    call $mixed_float_tail_call)
)

(assert_return (invoke "mixed_float_tail_call") (f64.const 8796093022208))
