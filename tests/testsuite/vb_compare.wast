(module
  (func (export "compare_lt_u") (param i32) (result i32)
    local.get 0
    i32.const -1
    i32.lt_u
  )
)

;; 0x7fffffff < static_cast<u32>(-1)
(assert_return (invoke "compare_lt_u" (i32.const 0x7fffffff)) (i32.const 1))
