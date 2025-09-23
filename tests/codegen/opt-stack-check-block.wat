(module
  (memory 1)
  (func $callee/0 (param i32))
  (func $callee-24_i/1 (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32))

  ;; CHECK-LABEL: Function[2] Body
  (func $block-can-inherit-parent-block/2
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     sub  sp, sp, #[[#]]
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    call $callee-24_i/1
    ;; since there are no enough stack, before function call, stack size needs to be increased
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     sub  sp, sp, #[[#]]
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     cmp  sp, x[[#]]
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     bl [[#]]

    i32.const 0x2222 call $callee/0 ;; use 0x2222 as mark
    ;; call other function to decrease stack size
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     mov  w19, #0x2222

    block
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      call $callee-24_i/1
      ;; since increased stack size has already check it, it does not need to be checked again.
      ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     sub  sp, sp, #[[#]]
      ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK-NOT: cmp  sp, x[[#]]
      ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     bl [[#]]
    end
  )
  ;; CHECK-LABEL: Function[3] Body
  (func $after-block-can-inherit-block/3
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     sub  sp, sp, #[[#]]
    block
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      call $callee-24_i/1
      ;; since there are no enough stack, before function call, stack size needs to be increased
      ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     sub  sp, sp, #[[#]]
      ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     cmp  sp, x[[#]]
      ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     bl [[#]]
    end

    i32.const 0x2222 call $callee/0 ;; use 0x2222 as mark
    ;; call other function to decrease stack size
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     mov  w19, #0x2222

    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    call $callee-24_i/1
    ;; here stack size is be checked before
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     sub  sp, sp, #[[#]]
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK-NOT: cmp  sp, x[[#]]
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     bl [[#]]
  )

  ;; CHECK-LABEL: Function[4] Body
  (func $after-block-cannot-inherit-block-after-br/4
    (local i32)
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     sub  sp, sp, #[[#]]
    block
      local.get 0
      br_if 0
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      call $callee-24_i/1
      ;; since there are no enough stack, before function call, stack size needs to be increased
      ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     sub  sp, sp, #[[#]]
      ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     cmp  sp, x[[#]]
      ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     bl [[#]]
    end

    i32.const 0x2222 call $callee/0 ;; use 0x2222 as mark
    ;; call other function to decrease stack size
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     mov  w19, #0x2222

    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    call $callee-24_i/1
    ;; block may reach end without checking stack size, here stack size check is still needed.
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     sub  sp, sp, #[[#]]
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     cmp  sp, x[[#]]
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     bl [[#]]
  )

  ;; CHECK-LABEL: Function[5] Body
  (func $after-block-cannot-inherit-block-after-inner-br/5
    (local i32)
    block
      loop
        local.get 0
        if
          local.get 0
          br_table 0 1 2
          memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
          memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
          call $callee-24_i/1
        end
      end
    end

    i32.const 0x2222 call $callee/0 ;; use 0x2222 as mark
    ;; call other function to decrease stack size
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     mov  w19, #0x2222

    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    call $callee-24_i/1
    ;; block may reach end without checking stack size, here stack size check is still needed.
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     sub  sp, sp, #[[#]]
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     cmp  sp, x[[#]]
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     bl [[#]]
  )

  ;; CHECK-LABEL: Function[6] Body
  (func $after-block-inherit-block-before-inner-br/6
    (local i32)
    block
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      call $callee-24_i/1
      local.get 0
      if
        local.get 0
        br_table 0 1 2
        memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
        memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
        memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
        memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
        call $callee-24_i/1
        call $callee-24_i/1
      end
    end

    i32.const 0x2222 call $callee/0 ;; use 0x2222 as mark
    ;; call other function to decrease stack size
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     mov  w19, #0x2222

    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    call $callee-24_i/1
    ;; block may reach end without checking stack size, here stack size check is still needed.
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     sub  sp, sp, #[[#]]
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK-NOT: cmp  sp, x[[#]]
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     bl [[#]]

    i32.const 0x3333 call $callee/0 ;; use 0x3333 as mark
    ;; call other function to decrease stack size
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     mov  w19, #0x3333

    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    call $callee-24_i/1
    call $callee-24_i/1
    ;; larger stack size is after br, here it should be checked again.
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     sub  sp, sp, #[[#]]
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     cmp  sp, x[[#]]
    ;; AARCH64_ACTIVE_STACK_OVERFLOW_CHECK:     bl [[#]]
  )
)

;; CHECK-LABEL: Initial Linear Memory Data
