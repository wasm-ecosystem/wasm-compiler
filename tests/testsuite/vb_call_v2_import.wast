(module
  (import "spectest" "multiReturn" (func (param i32 i64 i32 f64 f32 i64 f64 i32 i32 i64)(result i32 i64 i32 f64 f32 i64 f64 i32 i32 i64)))

  (func (result i32)
      i32.const 32
      i64.const 64
      i32.const 32
      f64.const 64.64
      f32.const 32.32
      i64.const 128
      f64.const 128.5
      i32.const 128
      i32.const 256
      i64.const 512
      call 0

      i64.const 522 ;; + 10
      i64.ne
      if
        i32.const 10
        return
      end

      i32.const 265 ;; + 9
      i32.ne
      if
        i32.const 9
        return
      end

      i32.const 136
      i32.ne
      if
        i32.const 8
        return
      end

      f64.const 136.0
      f64.ne
      if
        i32.const 7
        return
      end

      i64.const 134
      i64.ne
      if
        i32.const 6
        return
      end

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
