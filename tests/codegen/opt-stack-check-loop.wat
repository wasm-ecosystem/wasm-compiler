(module
  (memory 1)
  (func $callee/0 (param i32))
  (func $callee-24_i/1 (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32))

  ;; CHECK-LABEL: Function[2] Body
  (func $loop-can-inherit-parent-block/2
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

    loop
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
  (func $after-loop-can-inherit-loop/3
    ;; TRICORE:     sub.a  sp, #[[#]]
    loop
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      call $callee-24_i/1
      ;; since there are no enough stack, before function call, stack size needs to be increased
      ;; TRICORE:     sub.a  sp, #[[#]]
      ;; TRICORE:     ge.a  d[[#]], a[[#]], sp
      ;; TRICORE:     fcall #[[#]]
    end

    i32.const 0x2222 call $callee/0 ;; use 0x2222 as mark
    ;; call other function to decrease stack size
    ;; TRICORE:     mov.u  d[[#]], #0x2222

    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    call $callee-24_i/1
    ;; here stack size is checked before
    ;; TRICORE:     sub.a  sp, #[[#]]
    ;; TRICORE-NOT: ge.a  d[[#]], a[[#]], sp
    ;; TRICORE:     fcall #[[#]]
  )

  ;; CHECK-LABEL: Function[4] Body
  (func $after-loop-can-inherit-loop-after-br/4
    (local i32)
    ;; TRICORE:     sub.a  sp, #[[#]]
    loop
      local.get 0
      br_if 0
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
      call $callee-24_i/1
      ;; since there are no enough stack, before function call, stack size needs to be increased
      ;; TRICORE:     sub.a  sp, #[[#]]
      ;; TRICORE:     ge.a  d[[#]], a[[#]], sp
      ;; TRICORE:     fcall #[[#]]
    end

    i32.const 0x2222 call $callee/0 ;; use 0x2222 as mark
    ;; call other function to decrease stack size
    ;; TRICORE:     mov.u  d[[#]], #0x2222

    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    memory.size memory.size memory.size memory.size memory.size memory.size memory.size memory.size
    call $callee-24_i/1
    ;; loop reach end with checking stack size, here stack size is checked before
    ;; TRICORE:     sub.a  sp, #[[#]]
    ;; TRICORE-NOT: ge.a  d[[#]], a[[#]], sp
    ;; TRICORE:     fcall #[[#]]
  )
)

;; CHECK-LABEL: Initial Linear Memory Data
