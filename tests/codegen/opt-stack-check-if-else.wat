(module
  (memory 1)
  (func $callee/0 (param i32))
  (func $callee-24_i/1 (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32))

  ;; CHECK-LABEL: Function[2] Body
  (func $if-else-can-inherit-parent-block/2
    (param i32)
    ;; TRICORE:     sub.a  sp, #[[#]]
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    call $callee-24_i/1
    ;; since there are no enough stack, before function call, stack size needs to be increased
    ;; TRICORE:     sub.a  sp, #[[#]]
    ;; TRICORE:     ge.a  d[[#]], a[[#]], sp
    ;; TRICORE:     fcall #[[#]]

    i32.const 0x2222 call $callee/0 ;; use 0x2222 as mark
    ;; call other function to decrease stack size
    ;; TRICORE:     mov.u  d[[#]], #0x2222

    local.get 0
    if
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      call $callee-24_i/1
      ;; since increased stack size has already check it, it does not need to be checked again.
      ;; TRICORE:     sub.a  sp, #[[#]]
      ;; TRICORE-NOT: ge.a  d[[#]], a[[#]], sp
      ;; TRICORE:     fcall #[[#]]
    else
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      call $callee-24_i/1
      ;; since increased stack size has already check it, it does not need to be checked again.
      ;; TRICORE:     sub.a  sp, #[[#]]
      ;; TRICORE-NOT: ge.a  d[[#]], a[[#]], sp
      ;; TRICORE:     fcall #[[#]]
    end
  )
  ;; CHECK-LABEL: Function[3] Body
  (func $after-if-else-use-min-of-then-and-else-block/3
    ;; TRICORE:     sub.a  sp, #[[#]]
    (param i32)
    local.get 0
    if
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      call $callee-24_i/1
      ;; since there are no enough stack, before function call, stack size needs to be increased
      ;; TRICORE:     sub.a  sp, #[[#]]
      ;; TRICORE:     ge.a  d[[#]], a[[#]], sp
      ;; TRICORE:     fcall #[[#]]
    else
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      call $callee-24_i/1
      call $callee-24_i/1
      ;; since there are no enough stack, before function call, stack size needs to be increased
      ;; TRICORE:     sub.a  sp, #[[#]]
      ;; TRICORE:     ge.a  d[[#]], a[[#]], sp
      ;; TRICORE:     fcall #[[#]]
    end

    i32.const 0x2222 call $callee/0 ;; use 0x2222 as mark
    ;; call other function to decrease stack size
    ;; TRICORE:     mov.u  d[[#]], #0x2222

    ;; in then block, 1x stack size is checked.
    ;; in else block, 2x stack size is checked.
    ;; after merging, 1x stack size is checked but 2x stack size is not.

    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    call $callee-24_i/1
    ;; TRICORE:     sub.a  sp, #[[#]]
    ;; TRICORE-NOT: ge.a  d[[#]], a[[#]], sp
    ;; TRICORE:     fcall #[[#]]

    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    call $callee-24_i/1
    call $callee-24_i/1
    ;; TRICORE:     sub.a  sp, #[[#]]
    ;; TRICORE:     ge.a  d[[#]], a[[#]], sp
    ;; TRICORE:     fcall #[[#]]
  )

  ;; CHECK-LABEL: Function[4] Body
  (func $after-true-condi-if-else-can-inherit-then-block/4
    ;; TRICORE:     sub.a  sp, #[[#]]
    i32.const 1
    if
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      call $callee-24_i/1
    else
    end

    i32.const 0x2222 call $callee/0 ;; use 0x2222 as mark
    ;; call other function to decrease stack size
    ;; TRICORE:     mov.u  d[[#]], #0x2222

    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    call $callee-24_i/1
    ;; here stack size may not be checked before
    ;; TRICORE:     sub.a  sp, #[[#]]
    ;; TRICORE-NOT: ge.a  d[[#]], a[[#]], sp
    ;; TRICORE:     fcall #[[#]]
  )
  
  ;; CHECK-LABEL: Function[5] Body
  (func $after-true-condi-if-else-cannot-inherit-else-block/5
    ;; TRICORE:     sub.a  sp, #[[#]]
    i32.const 1
    if
    else
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      call $callee-24_i/1
    end

    i32.const 0x2222 call $callee/0 ;; use 0x2222 as mark
    ;; call other function to decrease stack size
    ;; TRICORE:     mov.u  d[[#]], #0x2222

    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    call $callee-24_i/1
    ;; here stack size may not be checked before
    ;; TRICORE:     sub.a  sp, #[[#]]
    ;; TRICORE:     ge.a  d[[#]], a[[#]], sp
    ;; TRICORE:     fcall #[[#]]
  )


  ;; CHECK-LABEL: Function[6] Body
  (func $after-false-condi-if-else-cannot-inherit-then-block/6
    ;; TRICORE:     sub.a  sp, #[[#]]
    i32.const 0
    if
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      call $callee-24_i/1
    else
    end

    i32.const 0x2222 call $callee/0 ;; use 0x2222 as mark
    ;; call other function to decrease stack size
    ;; TRICORE:     mov.u  d[[#]], #0x2222

    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    call $callee-24_i/1
    ;; here stack size may not be checked before
    ;; TRICORE:     sub.a  sp, #[[#]]
    ;; TRICORE:     ge.a  d[[#]], a[[#]], sp
    ;; TRICORE:     fcall #[[#]]
  )
  
  ;; CHECK-LABEL: Function[7] Body
  (func $after-false-condi-if-else-can-inherit-else-block/7
    ;; TRICORE:     sub.a  sp, #[[#]]
    i32.const 0
    if
    else
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      call $callee-24_i/1
    end

    i32.const 0x2222 call $callee/0 ;; use 0x2222 as mark
    ;; call other function to decrease stack size
    ;; TRICORE:     mov.u  d[[#]], #0x2222

    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    call $callee-24_i/1
    ;; here stack size may not be checked before
    ;; TRICORE:     sub.a  sp, #[[#]]
    ;; TRICORE-NOT: ge.a  d[[#]], a[[#]], sp
    ;; TRICORE:     fcall #[[#]]
  )

  ;; CHECK-LABEL: Function[8] Body
  (func $br-from-if-cannot-inherit/8
    (param i32)
    ;; TRICORE:     sub.a  sp, #[[#]]
    local.get 0
    if
      local.get 0
      br_if 0
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      call $callee-24_i/1
    else
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      call $callee-24_i/1
    end

    i32.const 0x2222 call $callee/0 ;; use 0x2222 as mark
    ;; call other function to decrease stack size
    ;; TRICORE:     mov.u  d[[#]], #0x2222

    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    call $callee-24_i/1
    ;; here stack size may not be checked before
    ;; TRICORE:     sub.a  sp, #[[#]]
    ;; TRICORE:     ge.a  d[[#]], a[[#]], sp
    ;; TRICORE:     fcall #[[#]]
  )


  ;; CHECK-LABEL: Function[9] Body
  (func $br-from-else-cannot-inherit/8
    (param i32)
    ;; TRICORE:     sub.a  sp, #[[#]]
    local.get 0
    if
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      call $callee-24_i/1
    else
      local.get 0
      br_if 0
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      call $callee-24_i/1
    end

    i32.const 0x2222 call $callee/0 ;; use 0x2222 as mark
    ;; call other function to decrease stack size
    ;; TRICORE:     mov.u  d[[#]], #0x2222

    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    call $callee-24_i/1
    ;; here stack size may not be checked before
    ;; TRICORE:     sub.a  sp, #[[#]]
    ;; TRICORE:     ge.a  d[[#]], a[[#]], sp
    ;; TRICORE:     fcall #[[#]]
  )
)

;; CHECK-LABEL: Initial Linear Memory Data
