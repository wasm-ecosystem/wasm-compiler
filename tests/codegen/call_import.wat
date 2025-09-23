(module
    (import "env" "s" (func $s (param i32 i32 i32 i32) (result i32)))
   
    ;; CHECK-LABEL: Function[0] Body
    (func $reg-to-reg (param i32 i32 i32 i32) (result i32)


    ;; TRICORE: mov  d4, d9
    local.get 0
    ;; AARCH64: mov w0, w8
    ;; TRICORE: mov  d5, d6
    local.get 1
    ;; TRICORE: mov  d6, d7
    ;; X86_64_NO_ACTIVE_STACK_OVERFLOW_CHECK: mov edx, r9d
    local.get 2
    ;; TRICORE: mov  d7, d10
    ;; X86_64_NO_ACTIVE_STACK_OVERFLOW_CHECK: mov ecx, r10d
    local.get 3

    call $s
    )

  (memory (;0;) 2)
  (global (;0;) (mut i32) (i32.const 102808))
)