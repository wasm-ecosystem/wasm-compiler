;;carry over from fuzz, wrong register save/restore when tricore unaligned i64.store
(module
  (func (result i32)
      call 1
      i32.const 0x555
      i64.const 0x33
      i64.store
  )
  (func (result i32)
    i32.const 0xFF0)
  (memory 1)
  (export "unaligned-store" (func 0))
)

(assert_return (invoke "unaligned-store") (i32.const 0xFF0))