;; This case is designed to ensure that when loading multi return values, the current one does not dirty write the previous return register.
(module

  (func (result i64 i64)
    (local i32 i64)

    block (result   i64 i64)  ;; label = @1
      i64.const 0x1000
      i64.const 0x2000

      local.get 1
      f32.const 0(;=-1.593;)
      f64.promote_f32
      i64.trunc_f64_s
      i64.rotl

      i64.const -128
      i64.const -60
      local.get 0
      if (result i32)  ;; label = @2
        i32.const 0
      else
        i32.const 18
      end
      select
      i64.or
      i32.wrap_i64
      br_if 0 (;@1;)
    end
  )

  (export "func" (func 0))
)

(assert_return (invoke "func")  (i64.const 0x1000) (i64.const 0x2000))
