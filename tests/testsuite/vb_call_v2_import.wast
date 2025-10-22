(module
  (import "spectest" "multiReturn" (func (param i32 i64 i32 f64 f32)(result i32 i64 i32 f64 f32)))

  (func (result i32)
      i32.const 32
      i64.const 64
      i32.const 32
      f64.const 64.64
      f32.const 32.32
      call 0

      f32.const 37.82 ;; + 5.5
      f32.ne
      if
        i32.const 5
        return
      end

      f64.const 69.04 ;; + 4.4
      f64.ne
      if
        i32.const 4
        return
      end

      i32.const 35 ;; + 3
      i32.ne
      if
        i32.const 3
        return
      end

      i64.const 66 ;; + 2
      i64.ne
      if
        i32.const 2
        return
      end

      i32.const 33 ;; + 1
      i32.ne
      if
        i32.const 1
        return
      end

      i32.const 0
      return
    )
  (export "func_invoker" (func 1))
)

(assert_return (invoke "func_invoker") (i32.const 0))
