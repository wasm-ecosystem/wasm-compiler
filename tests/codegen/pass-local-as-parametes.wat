(module
  (func $callee-Ii/0 (param i32 i32)
  )
  ;; CHECK-LABEL: Function[1] Body
  (func $caller/1
    (param $arg0 f32)
    (param $arg1 i32)
    i32.const 0x1234
    local.get $arg1
    ;; $arg1 is stored in the first generic reg, which is the reg used by 1st argument in calling convension.
    ;; store $arg1 to stack before load 0x1234 in 1st parameter reg
    ;; then load $arg1 from stack to 2nd parameter reg.

    ;; X86_64:         mov  dword ptr [rsp + [[OFFSET:(0x)?[0-9a-f]+]]], ebp
    ;; X86_64:         mov  edi, ebp
    ;; X86_64:         mov  ebp, 0x1234
    ;; X86_64:         call

    ;; AARCH64:        str  w19, [sp, [[OFFSET:#0x[0-9]+]]]
    ;; AARCH64:        mov  w8, w19
    ;; AARCH64:        mov  w19, #0x1234
    ;; AARCH64:        bl

    ;;
    
    ;; tricore does not have the concept of generic reg and float reg.
    ;; store $arg0 to stack before load 0x1234 as 1st argument reg.
    ;; then do not need to load $arg1 from stack because it is already in 2nd parameter reg.

    ;; TRICORE:        st.w  [sp]#0x10, d8
    ;; TRICORE:        mov.u  d8, #0x1234
    ;; TRICORE:        fcall

    call $callee-Ii/0
  )

  ;; CHECK-LABEL: Function[2] Body
  (func $callee-fff (param f32 f32 f32)
    
  )

  ;; CHECK-LABEL: Function[3] Body
  (func $caller/fff/0
    (param $arg0 f32)
    (param $arg1 f32)

    ;; X86_64: movd  [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], xmm5
    ;; X86_64-NEXT: movss xmm5, xmm4
    ;; X86_64-NEXT: movd xmm4, [[REG]]

    ;; AARCH64: fmov  [[REG:w[0-9]+]], s1
    ;; AARCH64-NEXT: fmov  s1, s8
    ;; AARCH64-NEXT: fmov  s8, [[REG]]

    ;; TRICORE: xor [[REG1:d[0-9]+]], [[REG2:d[0-9]+]]
    ;; TRICORE-NEXT: xor [[REG2]], [[REG1]]
    ;; TRICORE-NEXT: xor [[REG1]], [[REG2]]
    local.get $arg1
    local.get $arg0
    f32.const 1
    call $callee-fff
  )

  ;; CHECK-LABEL: Function[4] Body
  (func $caller/fff/1
    (param $arg0 f32)
    (param $arg1 f32)
    (param $arg2 f32)
    f32.const 1

    ;; X86_64: movss xmm6, xmm5
    ;; X86_64-NEXT: movss xmm5, xmm4
    ;; AARCH64: fmov  s2, s1
    ;; AARCH64-NEXT: fmov  s1, s8
    ;; TRICORE: mov  d6, d9
    ;; TRICORE-NEXT: mov  d9, d8
    local.get $arg0
    local.get $arg1
    call $callee-fff
  )
  ;; CHECK-LABEL: Function[5] Body
  (func $callee-iiii (param i32 i32 i32 i32) 
    
  )
  ;; CHECK-LABEL: Function[6] Body
  (func $caller-iiii
    (local i32 i32 i32 i32)
    i32.const 0
    local.set 0
    i32.const 1
    local.set 1
    i32.const 2
    local.set 2
    i32.const 3
    local.set 3
    ;; X86_64_NO_ACTIVE_STACK_OVERFLOW_CHECK: xchg ebp, r9d
    ;; X86_64_NO_ACTIVE_STACK_OVERFLOW_CHECK-NEXT: xchg edi, esi
    ;; AARCH64_NO_ACTIVE_STACK_OVERFLOW_CHECK:  eor  w19, w19, w2
    ;; AARCH64_NO_ACTIVE_STACK_OVERFLOW_CHECK-NEXT:  eor  w2, w19, w2
    ;; AARCH64_NO_ACTIVE_STACK_OVERFLOW_CHECK-NEXT:  eor  w19, w19, w2
    ;; AARCH64_NO_ACTIVE_STACK_OVERFLOW_CHECK-NEXT:  eor  w8, w8, w1
    ;; AARCH64_NO_ACTIVE_STACK_OVERFLOW_CHECK-NEXT:  eor  w1, w8, w1
    ;; AARCH64_NO_ACTIVE_STACK_OVERFLOW_CHECK-NEXT:  eor  w8, w8, w1
    ;; TRICORE:       xor d8, d7
    ;; TRICORE-NEXT:  xor d7, d8
    ;; TRICORE-NEXT:  xor d8, d7
    ;; TRICORE-NEXT:  xor d9, d6
    ;; TRICORE-NEXT:  xor d6, d9
    ;; TRICORE-NEXT:  xor d9, d6
    local.get 3
    local.get 2
    local.get 1
    local.get 0
    call $callee-iiii
  )
;; CHECK-LABEL: Function[7] Body
  (func $callee-iiiiii (param i32 i32 i32 i32 i32 i32) 
    
    )
;; CHECK-LABEL: Function[8] Body
  (func $caller-iiiiii
    (local i32 i32 i32 i32 i32 i32)
    i32.const 0
    local.set 0
    i32.const 1
    local.set 1
    i32.const 2
    local.set 2
    i32.const 3
    local.set 3
    i32.const 4
    local.set 4
    i32.const 5
    local.set 5
    ;; AARCH64_NO_ACTIVE_STACK_OVERFLOW_CHECK:  eor  w19, w19, w8
    ;; AARCH64_NO_ACTIVE_STACK_OVERFLOW_CHECK-NEXT:  eor  w8, w19, w8
    ;; AARCH64_NO_ACTIVE_STACK_OVERFLOW_CHECK-NEXT:  eor  w19, w19, w8
    ;; AARCH64_NO_ACTIVE_STACK_OVERFLOW_CHECK-NEXT:  eor  w8, w8, w1
    ;; AARCH64_NO_ACTIVE_STACK_OVERFLOW_CHECK-NEXT:  eor  w1, w8, w1
    ;; AARCH64_NO_ACTIVE_STACK_OVERFLOW_CHECK-NEXT:  eor  w8, w8, w1
    ;; TRICORE: xor d8, d9
    ;; TRICORE-NEXT: xor d9, d8
    ;; TRICORE-NEXT: xor d8, d9
    ;; TRICORE-NEXT: xor d9, d6
    ;; TRICORE-NEXT: xor d6, d9
    ;; TRICORE-NEXT: xor d9, d6

    ;; AARCH64_NO_ACTIVE_STACK_OVERFLOW_CHECK-NEXT:  eor  w2, w2, w4
    ;; AARCH64_NO_ACTIVE_STACK_OVERFLOW_CHECK-NEXT:  eor  w4, w2, w4
    ;; AARCH64_NO_ACTIVE_STACK_OVERFLOW_CHECK-NEXT:  eor  w2, w2, w4
    ;; AARCH64_NO_ACTIVE_STACK_OVERFLOW_CHECK-NEXT:  eor  w4, w4, w3
    ;; AARCH64_NO_ACTIVE_STACK_OVERFLOW_CHECK-NEXT:  eor  w3, w4, w3
    ;; AARCH64_NO_ACTIVE_STACK_OVERFLOW_CHECK-NEXT:  eor  w4, w4, w3
    ;; TRICORE: xor d7, d11
    ;; TRICORE-NEXT: xor d11, d7
    ;; TRICORE-NEXT: xor d7, d11
    ;; TRICORE-NEXT: xor d11, d10
    ;; TRICORE-NEXT: xor d10, d11
    ;; TRICORE-NEXT: xor d11, d10
    local.get 1
    local.get 2
    local.get 0
    local.get 5
    local.get 3
    local.get 4
    call $callee-iiiiii

  )


)
