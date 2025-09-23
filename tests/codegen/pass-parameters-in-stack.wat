(module
  ;; CHECK-LABEL: Function[0] Body
  (func $callee-iiiiiiiii/0 (param i32 i32 i32 i32 i32 i32 i32 i32 i32)
    (local $tmp i32)
    ;; X86_64:       lea  rsp, [rsp - 0x[[#%x,STACK_SIZE:]]]
    ;; AARCH64:      sub  sp, sp, #0x[[#%x,STACK_SIZE:]]
    ;; TRICORE:      sub.a  sp, #[[#%#x,STACK_SIZE:]]

    local.get 0
    local.set $tmp
    ;; X86_64:       mov  [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], ebp
    ;; AARCH64:      mov  [[REG:w[0-9]+]], w19
    ;; TRICORE:      mov  [[REG:d[0-9]+]], d8
    local.get 1
    local.set $tmp
    ;; X86_64-NEXT:  mov  [[REG]], edi
    ;; AARCH64-NEXT: mov  [[REG]], w8
    ;; TRICORE-NEXT: mov  [[REG]], d9
    local.get 2
    local.set $tmp
    ;; X86_64_ACTIVE_STACK_OVERFLOW_CHECK-NEXT:       mov  [[REG]], r9d
    ;; X86_64_NO_ACTIVE_STACK_OVERFLOW_CHECK-NEXT:    mov  [[REG]], esi
    ;; AARCH64-NEXT: mov  [[REG]], w1
    ;; TRICORE-NEXT: mov  [[REG]], d6
    local.get 3
    local.set $tmp
    ;; X86_64_ACTIVE_STACK_OVERFLOW_CHECK-NEXT:       mov  [[REG]], r10d
    ;; X86_64_NO_ACTIVE_STACK_OVERFLOW_CHECK-NEXT:    mov  [[REG]], r9d
    ;; AARCH64-NEXT: mov  [[REG]], w2
    ;; TRICORE-NEXT: mov  [[REG]], d7
    local.get 4
    local.set $tmp
    ;; X86_64-NEXT:  mov  [[REG]], dword ptr [rsp + 0x[[#%x,STACK_SIZE + 40]]]
    ;; AARCH64-NEXT: mov  [[REG]], w3
    ;; TRICORE-NEXT: mov  [[REG]], d10
    local.get 5
    local.set $tmp
    ;; X86_64-NEXT:  mov  [[REG]], dword ptr [rsp + 0x[[#%x,STACK_SIZE + 32]]]
    ;; AARCH64-NEXT: mov  [[REG]], w4
    ;; TRICORE-NEXT: mov  [[REG]], d11
    local.get 6
    local.set $tmp
    ;; X86_64-NEXT:  mov  [[REG]], dword ptr [rsp + 0x[[#%x,STACK_SIZE + 24]]]
    ;; AARCH64-NEXT: mov  [[REG]], w5
    ;; TRICORE-NEXT: mov  [[REG]], d12
    local.get 7
    local.set $tmp
    ;; X86_64-NEXT:  mov  [[REG]], dword ptr [rsp + 0x[[#%x,STACK_SIZE + 16]]]
    ;; AARCH64-NEXT: mov  [[REG]], w6
    ;; TRICORE-NEXT: ld.w [[REG]], [sp]#0x[[#%x,STACK_SIZE + 8]]
    local.get 8
    local.set $tmp
    ;; load from top of stack
    ;; X86_64-NEXT:  mov  [[REG]], dword ptr [rsp + 0x[[#%x,STACK_SIZE + 8]]]
    ;; AARCH64-NEXT: ldr  [[REG]], [sp, #0x[[#%x,STACK_SIZE]]]
    ;; TRICORE-NEXT: ld.w [[REG]], [sp]#0x[[#%x,STACK_SIZE + 4]]
  )

  ;; CHECK-LABEL: Function[1] Body
  (func $caller/1
    i32.const 1
    ;; X86_64-DAG:       mov  ebp, 1
    ;; AARCH64-DAG:      mov  w19, #1
    ;; TRICORE-DAG:      mov  d8, #1
    i32.const 2
    ;; X86_64-DAG:  mov  edi, 2
    ;; AARCH64-DAG: mov  w8, #2
    ;; TRICORE-DAG: mov  d9, #2
    i32.const 3
    ;; X86_64_ACTIVE_STACK_OVERFLOW_CHECK-DAG:     mov  r9d, 3
    ;; X86_64_NO_ACTIVE_STACK_OVERFLOW_CHECK-DAG:  mov  esi, 3
    ;; AARCH64-DAG: mov  w1, #3
    ;; TRICORE-DAG: mov  d6, #3
    i32.const 4
    ;; X86_64_ACTIVE_STACK_OVERFLOW_CHECK-DAG:    mov  r10d, 4
    ;; X86_64_NO_ACTIVE_STACK_OVERFLOW_CHECK-DAG:  mov  r9d, 4
    ;; AARCH64-DAG: mov  w2, #4
    ;; TRICORE-DAG: mov  d7, #4
    i32.const 5
    ;; X86_64-DAG:  mov  dword ptr [rsp + 0x20], 5
    ;; AARCH64-DAG: mov  w3, #5
    ;; TRICORE-DAG: mov  d10, #5
    i32.const 6
    ;; X86_64-DAG:  mov  dword ptr [rsp + 0x18], 6
    ;; AARCH64-DAG: mov  w4, #6
    ;; TRICORE-DAG: mov  d11, #6
    i32.const 7
    ;; X86_64-DAG:  mov  dword ptr [rsp + 0x10], 7
    ;; AARCH64-DAG: mov  w5, #7
    ;; TRICORE-DAG: mov  d12, #7
    i32.const 8
    ;; X86_64-DAG:  mov  dword ptr [rsp + 8], 8
    ;; AARCH64-DAG: mov  w6, #8
    ;; TRICORE-DAG: mov.a  [[REG:a[0-9]+]], #8
    ;; TRICORE-DAG: st.a  [sp]#4, [[REG]]
    i32.const 9
    ;; X86_64-DAG:  mov  dword ptr [rsp], 9

    ;; AARCH64-DAG: mov  w27, #9
    ;; AARCH64-DAG: str  w27, [sp]
    ;; w27 is temp reg for emit move in aarch64, after using it, compiler must recover it back.
    ;; AARCH64_LINEAR_MEMORY_BOUNDS_CHECKS-DAG: ldur  w27, [x29, #-0x[[#%x,LinMemOffset:]]]
    ;; AARCH64_LINEAR_MEMORY_BOUNDS_CHECKS-DAG: sub  x27, x27, #8
    
    ;; TRICORE-DAG: mov.a  [[REG:a[0-9]+]], #9
    ;; TRICORE-DAG: st.a  [sp], [[REG]]
    call $callee-iiiiiiiii/0
    ;; X86_64-NEXT:  call [[#]]
    ;; AARCH64-NEXT: bl [[#]]
    ;; TRICORE-NEXT: fcall #[[#]]

    ;; AARCH64-NEXT: ldr  x30, [sp, #8]
  )
)
