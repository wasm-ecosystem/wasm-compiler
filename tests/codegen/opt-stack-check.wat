(module
  (memory 1)
  (func $callee/0)
  (func $callee-24_i/1 (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32))

  ;; CHECK-LABEL: Function[2] Body
  (func $not-check-stack-before-call-if-stack-size-is-decrease/2
    ;; check sp at the beginning of function
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     sub  sp, sp, #[[#]]
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     cmp  sp, x[[#]]
    call $callee/0
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     add sp, sp, #[[#]]
    ;; It does not need to check stack size if it is checked before
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK-NOT: cmp  sp, x[[#]]
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     bl [[#]]
  )

  ;; CHECK-LABEL: Function[3] Body
  (func $check-stack-before-call-if-stack-size-is-increase/3
    ;; check sp at the beginning of function
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     sub  sp, sp, #[[#]]
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     cmp  sp, x[[#]]

    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size 
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    call $callee/0
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     sub  sp, sp, #[[#]]
    ;; It needs to check stack size even if it is checked before because stack size increased.
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     cmp  sp, x[[#]]
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     bl [[#]]

    drop drop drop drop drop drop drop drop drop drop drop drop drop drop drop drop drop drop drop drop drop drop drop drop drop drop drop drop drop drop
    drop drop drop drop drop drop drop drop
  )

  ;; CHECK-LABEL: Function[4] Body
  (func $no-need-to-check-size/4
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     sub  sp, sp, #[[#]]
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    call $callee-24_i/1
    ;; since there are no enough stack, before function call, stack size needs to be increased
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     sub  sp, sp, #[[#]]
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     cmp  sp, x[[#]]
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     bl [[#]]

    call $callee/0
    ;; call other function to decrease stack size
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     add  sp, sp, #[[#]]
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     bl [[#]]

    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    call $callee-24_i/1
    ;; since increased stack size has already check it, it does not need to be checked again.
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     sub  sp, sp, #[[#]]
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK-NOT: cmp  sp, x[[#]]
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     bl [[#]]
  )
)

;; CHECK-LABEL: Initial Linear Memory Data
