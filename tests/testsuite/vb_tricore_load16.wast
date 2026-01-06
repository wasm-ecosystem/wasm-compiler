(module
  (memory 1)
  
  (data (i32.const 100) "\ff\7f\80\00\ff\ff\00\80\00\00\00\00\ff\ff\ff\7f")
  
  (func (export "i32_load16_s_misaligned") (result i32)
    i32.const 0
    i32.load16_s offset=101
  )
  
  (func (export "i32_load16_u_misaligned") (result i32)
    i32.const 101
    i32.load16_u 
  )
)

(assert_return (invoke "i32_load16_u_misaligned") (i32.const 32895))


