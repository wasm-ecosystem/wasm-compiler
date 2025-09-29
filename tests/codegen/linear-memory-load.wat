(module
     ;; CHECK-LABEL: Function[0] Body
    (func (param i32 i32) (result i32)
    (local.set 0 (i32.const 0x10))
    ;; AARCH64:         mov w[[L0:[0-9]+]], #0x10
    (local.set 1 (i32.const 0x11))
    ;; AARCH64:         mov w[[L1:[0-9]+]], #0x11

    local.get 1
    ;; X86_64_NO_LINEAR_MEMORY_BOUNDS_CHECKS:  mov ebp,  dword ptr [rbx + [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]] + [[NUM:0x[0-9a-f]+]]]
    ;; X86_64_NO_LINEAR_MEMORY_BOUNDS_CHECKS:  mov ebp,  dword ptr [rbx + rbp + [[NUM:[0-9a-f]+]]]
    ;; AARCH64:         add  x[[L0]], x[[L1]], #0xc
    ;; AARCH64:         ldr  w[[L0]], [[[LINEAR_MEMORY_REG:x[0-9]+]], x[[L0]]]
    ;; AARCH64:         add  x[[L0]], x[[L0]], #4
    ;; AARCH64:         ldr  w[[L0]], [[[LINEAR_MEMORY_REG:x[0-9]+]], x[[L0]]]
    ;; TRICORE: ld.w  d8, [a15]#-4
    ;; TRICORE: ld.w  d8, [a15]#-4
    
    i32.load offset=12
    i32.load offset=4

    local.tee 0
    )

    ;; CHECK-LABEL: Function[1] Body
    (func (result i32)
      (local i32)
      i32.const 1
      ;; X86_64_NO_LINEAR_MEMORY_BOUNDS_CHECKS:  mov ebp,  dword ptr [rbx + [[NUM:[0-9a-f]+]]]
      ;; AARCH64: ldr  w19, [[[REG:x[0-9]+]], [[REG:x[0-9]+]]]

      ;; TRICORE: ld.w  d[[#]], [a2]#4
      i32.const 4
      i32.load

      ;; X86_64_NO_LINEAR_MEMORY_BOUNDS_CHECKS: add ebp, 1
      ;; AARCH64: add w19, w19, #1
      ;; TRICORE: add d8, #1
      i32.add

      local.tee 0
      return
    )

    ;; CHECK-LABEL: Function[2] Body
    (func (result i32)
      i32.const 1

      i32.const 4
      ;; X86_64_NO_LINEAR_MEMORY_BOUNDS_CHECKS:  mov eax,  dword ptr [rbx + [[NUM:[0-9a-f]+]]]
      ;; AARCH64: ldr  w0, [[[REG:x[0-9]+]], [[REG:x[0-9]+]]]

      ;; TRICORE: ld.w  d[[#]], [a2]#4
      i32.load
      ;; X86_64_NO_LINEAR_MEMORY_BOUNDS_CHECKS: add eax, 1
      ;; AARCH64: add w0, w0, #1
      ;; TRICORE: add d2, #1
      i32.add
      return
    )

    ;; CHECK-LABEL: Function[3] Body
    (func (result i32)
      (local i32)
      i32.const 0
      ;; X86_64_NO_LINEAR_MEMORY_BOUNDS_CHECKS: mov  [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], dword ptr [rbx + 0x100]
      i32.load offset=0x100
      i32.const 0
      ;; X86_64_NO_LINEAR_MEMORY_BOUNDS_CHECKS: mov  [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], dword ptr [rbx + 0x200]
      i32.load offset=0x200
      local.set 0
      return
    )

     ;; CHECK-LABEL: Function[4] Body
    (func (result i32 i32)
      (local i32)
      i32.const 0
      ;; X86_64_NO_LINEAR_MEMORY_BOUNDS_CHECKS: mov  [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], dword ptr [rbx + 0x100]
      i32.load offset=0x100
      i32.const 0
      ;; X86_64_NO_LINEAR_MEMORY_BOUNDS_CHECKS: mov  [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], dword ptr [rbx + 0x200]
      i32.load offset=0x200
    )

  ;; CHECK-LABEL: Function[5] Body
  (func $range_check (param i32) (result i32)
    local.get 0
    ;; x86_64_LINEAR_MEMORY_BOUNDS_CHECKS:  cmp  rsi, [[L0:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]]
    ;; x86_64_LINEAR_MEMORY_BOUNDS_CHECKS:  jge
    ;; x86_64_LINEAR_MEMORY_BOUNDS_CHECKS:  push  rdi
    ;; x86_64_LINEAR_MEMORY_BOUNDS_CHECKS:  push  rdi
    ;; x86_64_LINEAR_MEMORY_BOUNDS_CHECKS:  lea  rdi, [[[L0]] + 4]
    ;; x86_64_LINEAR_MEMORY_BOUNDS_CHECKS:  call
    ;; x86_64_LINEAR_MEMORY_BOUNDS_CHECKS:  pop  rdi
    ;; x86_64_LINEAR_MEMORY_BOUNDS_CHECKS:  pop  rdi
    ;; AARCH64_LINEAR_MEMORY_BOUNDS_CHECKS: cmp  x27, [[L0:x[0-9]+]]
    ;; AARCH64_LINEAR_MEMORY_BOUNDS_CHECKS: b.ge
    ;; AARCH64_LINEAR_MEMORY_BOUNDS_CHECKS: stp  x30, x0, [x29, #-[[NUM:0x[0-9a-f]+]]]
    ;; AARCH64_LINEAR_MEMORY_BOUNDS_CHECKS: add  x0, [[L0]], #4
    ;; AARCH64_LINEAR_MEMORY_BOUNDS_CHECKS: bl
    ;; AARCH64_LINEAR_MEMORY_BOUNDS_CHECKS: ldp  x30, x0, [x29, #-[[NUM]]]
    ;; TRICORE:      mov.a  a15, [[L0:d[0-9]+]]
    ;; TRICORE:      jltz  [[L0]],
    ;; TRICORE:      add.a  a15, #4
    ;; TRICORE:      ge.a  [[TMP:d[0-9]+]], a3, a15
    ;; TRICORE:      jnz  [[TMP]],
    ;; TRICORE:      fcall
    ;; TRICORE:      add.a  a15, a2
    i32.load offset=0
  )

  (memory 1 100)
)
