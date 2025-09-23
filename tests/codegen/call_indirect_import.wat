(module
  (type $log_type (func (param i32 i32)))
  ;; X86_64:  mov  esi, edi
  ;; X86_64:  mov  edi, ebp
  ;; AARCH64: mov  w0, w19
  ;; AARCH64: mov  w1, w8
  ;; TRICORE: mov  d4, d8
  ;; TRICORE: mov  d5, d9
  (import "env" "log" (func $log (param i32 i32)))

  (table 1 funcref)
  (elem (i32.const 0) $log)

  (func
    i32.const 42
    i32.const 10
    i32.const 0
    call_indirect (type $log_type)
  )
)