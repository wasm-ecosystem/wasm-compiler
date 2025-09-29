;; This module is aim to assert the register allocation for multiple return values according to the Wasm ABI
(module
  (memory 1)
  ;; CHECK-LABEL: Function[0] Body
  (func $rv-iiifff (param f32 f32 f32) (result i32 i32 i32 f32 f32 f32)
    ;; X86_64:        lea  rsp, [rsp - 0x[[#%x,STACK_SIZE:]]]
    ;; AARCH64:       sub  sp, sp, #0x[[#%x,STACK_SIZE:]]
    ;; TRICORE:       sub.a  sp, #0x[[#%x,STACK_SIZE:]]

    i32.const 10428
    i32.const 30336
    i32.const 109
    local.get 0
    local.get 1
    local.get 2
    ;; X86_64:        mov  eax, 0x28bc
    ;; X86_64-NEXT:   mov  ecx, 0x7680
    ;; X86_64-NEXT:   movss  xmm0, xmm4
    ;; X86_64-NEXT:   movss  xmm1, xmm5
    ;; X86_64-NEXT:   mov  dword ptr [rsp + 0x[[#%x,STACK_SIZE + 8]]], 0x6d
    ;; X86_64-NEXT:   movss  dword ptr [rsp + 0x[[#%x,STACK_SIZE + 16]]], xmm6

    ;; AARCH64:  mov  w0, #0x28bc
    ;; AARCH64-NEXT:  mov  w26, #0x7680
    ;; AARCH64-NEXT:  fmov  s0, s8
    ;; AARCH64-NEXT:  fmov  s26, s1
    
    ;; AARCH64-NEXT:  mov  w27, #0x6d
    ;; AARCH64-NEXT:  str  w27, [sp, #0x[[#%x,STACK_SIZE]]]
    ;; w27 is temp reg for emit move in aarch64, after using it, compiler must recover it back.
    ;; AARCH64_LINEAR_MEMORY_BOUNDS_CHECKS-NEXT: ldur  w27, [x29, #-0x[[#%x,LinMemOffset:]]]
    ;; AARCH64_LINEAR_MEMORY_BOUNDS_CHECKS-NEXT: sub  x27, x27, #8
    ;; AARCH64-NEXT:  str  s2, [sp, #0x[[#%x,STACK_SIZE + 8]]]

    ;; TRICORE:       mov.u  d2, #0x28bc
    ;; TRICORE-NEXT:  mov.u  d3, #0x7680
    ;; TRICORE-NEXT:  movh.a  a12, #0
    ;; TRICORE-NEXT:  lea  a12, [a12]#0x6d
    ;; TRICORE-NEXT:  st.a [sp]#0x[[#%x,STACK_SIZE + 4]], a12
    ;; TRICORE-NEXT:  st.w [sp]#0x[[#%x,STACK_SIZE + 8]], d8
    ;; TRICORE-NEXT:  st.w [sp]#0x[[#%x,STACK_SIZE + 12]], d9
    ;; TRICORE-NEXT:  st.w [sp]#0x[[#%x,STACK_SIZE + 16]], d6
  )

  ;; CHECK-LABEL: Function[1] Body
  (func $br-table (param i32) (result i32 i32)
    block (result i32 i32)
      block (result i32 i32)
        i32.const 1
        i32.const 2
        local.get 0
        br_table 1 0 0

        ;; X86_64:        mov eax, 1
        ;; X86_64-NEXT:   mov ecx, 2

        ;; AARCH64:       mov  w0, #1
        ;; AARCH64-NEXT:  mov  w26, #2

        ;; TRICORE:       mov  d2, #1
        ;; TRICORE-NEXT:  mov  d3, #2
      end
    end
  )
  ;; CHECK-LABEL: Function[2] Body
  (func (param i32 i32) (result i32)
      (local i32)
      local.get 0
      local.get 1
      
      ;; X86_64: add [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]]
      ;; AARCH64: add [[REG:w[0-9]+]], [[REG:w[0-9]+]], [[REG:w[0-9]+]]
      ;; TRICORE: add [[REG:d[0-9]+]], [[REG:d[0-9]+]], [[REG:d[0-9]+]]
      i32.add

      local.get 0
      local.get 1
      ;; X86_64: sub [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]]
      ;; AARCH64: sub [[REG:w[0-9]+]], [[REG:w[0-9]+]], [[REG:w[0-9]+]]
      ;; TRICORE: sub [[REG:d[0-9]+]], [[REG:d[0-9]+]], [[REG:d[0-9]+]]
      i32.sub
      (block (param i32 i32) (result i32)
        i32.mul
      )
  )
)