(module
  (func (result i32)
    (local i32 i32 i32 i32 i32 i32 i32)
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    i32.const 0
    i32.load
    i32.add i32.add i32.add i32.add i32.add i32.add i32.add i32.add i32.add 
  )

  (memory (;0;) 1 1)
  (global (;0;) (mut i32) (i32.const 0))
  (export "load_in_high_reg_pressure" (func 0))
)

(assert_return (invoke "load_in_high_reg_pressure") (i32.const 9))
