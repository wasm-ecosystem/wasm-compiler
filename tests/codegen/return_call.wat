(module
  ;; Callee for Path A: same number of params as caller
  (func $callee_same (param i32) (result i32)
    local.get 0)

  ;; Callee for Path B: more params than caller
  (func $callee_many (param i32 i32 i32) (result i32)
    local.get 0)

  ;; 10-param callee (8 in regs + 2 on stack)
  (func $callee10 (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    local.get 0)

  ;; CHECK-LABEL: Function[3] Body
  ;; Path A: calleeParams (1) <= callerParams (1) → tail jump (b), no call (bl), no ret
  (func $tail_call_same_params (param i32) (result i32)
    local.get 0
    ;; AARCH64: cbz
    ;; AARCH64-NOT:  bl
    ;; AARCH64-NOT:  ret
    ;; AARCH64: add  sp, sp,
    ;; AARCH64-NEXT:  b  {{0x[0-9a-f]+}}
    ;; AARCH64-NOT:  bl
    ;; AARCH64-NOT:  ret
    ;; TRICORE-NOT:  fcall
    ;; TRICORE-NOT:  fret
    ;; TRICORE: lea  sp, [sp]#
    ;; TRICORE-NEXT: j  {{#0x[0-9a-f]+}}
    ;; TRICORE-NOT:  fcall
    ;; TRICORE-NOT:  fret
    ;; X86_64-NOT: call
    ;; X86_64-NOT: ret
    ;; X86_64: lea  rsp, [rsp +
    ;; X86_64-NEXT: jmp  {{0x[0-9a-f]+}}
    return_call $callee_same
  )

  ;; CHECK-LABEL: Function[4] Body
  ;; Path A: calleeStackParamWidth (0) <= callerParamWidth (0) → tail jump (b)
  ;; 3-param callee still fits in regs, no stack params needed
  (func $tail_call_more_params (param i32) (result i32)
    local.get 0
    i32.const 2
    i32.const 3
    ;; AARCH64: cbz
    ;; AARCH64-NOT:  bl
    ;; AARCH64-NOT:  ret
    ;; AARCH64: add  sp, sp,
    ;; AARCH64-NEXT:  b  {{0x[0-9a-f]+}}
    ;; AARCH64-NOT:  bl
    ;; AARCH64-NOT:  ret
    ;; TRICORE-NOT:  fcall
    ;; TRICORE-NOT:  fret
    ;; TRICORE: lea  sp, [sp]#
    ;; TRICORE-NEXT: j  {{#0x[0-9a-f]+}}
    ;; TRICORE-NOT:  fcall
    ;; TRICORE-NOT:  fret
    ;; X86_64-NOT: call
    ;; X86_64-NOT: ret
    ;; X86_64: lea  rsp, [rsp +
    ;; X86_64-NEXT: jmp  {{0x[0-9a-f]+}}
    return_call $callee_many
  )

  ;; CHECK-LABEL: Function[5] Body
  ;; Path A: 10-param caller → 10-param callee, stack param width equal → tail jump (b)
  (func $tail_call_10_same (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    local.get 0 local.get 1 local.get 2 local.get 3
    local.get 4 local.get 5 local.get 6 local.get 7
    local.get 8 local.get 9
    ;; AARCH64: cbz
    ;; AARCH64-NOT:  bl
    ;; AARCH64-NOT:  ret
    ;; AARCH64: add  sp, sp,
    ;; AARCH64-NEXT:  b  {{0x[0-9a-f]+}}
    ;; AARCH64-NOT:  bl
    ;; AARCH64-NOT:  ret
    ;; TRICORE-NOT:  fcall
    ;; TRICORE-NOT:  fret
    ;; TRICORE: lea  sp, [sp]#
    ;; TRICORE-NEXT: j  {{#0x[0-9a-f]+}}
    ;; TRICORE-NOT:  fcall
    ;; TRICORE-NOT:  fret
    ;; X86_64-NOT: call
    ;; X86_64-NOT: ret
    ;; X86_64: lea  rsp, [rsp +
    ;; X86_64-NEXT: jmp  {{0x[0-9a-f]+}}
    return_call $callee10
  )

  ;; CHECK-LABEL: Function[6] Body
  ;; Path B: 5-param caller (all in regs, paramWidth=0) → 10-param callee (2 on stack) → bl + ret
  (func $tail_call_10_insufficient (param i32 i32 i32 i32 i32) (result i32)
    local.get 0 local.get 1 local.get 2 local.get 3
    local.get 4 i32.const 5 i32.const 6 i32.const 7
    i32.const 8 i32.const 9
    ;; No need to save locals
    ;; AARCH64-NOT:  str w19,  [sp, [[OFFSET:#0x[0-9a-f]+]]]
    ;; AARCH64-NOT:  str w8,  [sp, [[OFFSET:#0x[0-9a-f]+]]]
    ;; AARCH64-NOT:  str w1,  [sp, [[OFFSET:#0x[0-9a-f]+]]]
    ;; AARCH64-NOT:  str w2,  [sp, [[OFFSET:#0x[0-9a-f]+]]]
    ;; AARCH64-NOT:  str w3,  [sp, [[OFFSET:#0x[0-9a-f]+]]]
    ;; AARCH64:  add  sp, sp,

    ;; Still need to save LR (str x30) and restore it, then ret
    ;; AARCH64-NEXT:  str x30,

    ;; AARCH64:  bl
    ;; AARCH64:  ldr  x30,
    ;; AARCH64:  ret
    ;; TRICORE: fcall
    ;; TRICORE-NEXT: lea  sp, [sp]#
    ;; TRICORE-NEXT: fret
    ;; X86_64: call  {{0x[0-9a-f]+}}
    ;; X86_64: lea  rsp, [rsp +
    ;; X86_64-NEXT: ret
    return_call $callee10
  )

  ;; CHECK-LABEL: Function[7] Body
  ;; Path A: 12-param caller (paramWidth >= callee's) → 10-param callee → tail jump (b)
  (func $tail_call_10_surplus (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    local.get 0 local.get 1 local.get 2 local.get 3
    local.get 4 local.get 5 local.get 6 local.get 7
    local.get 8 local.get 9
    ;; AARCH64: cbz
    ;; stack param relocation: ldr + str pairs for stack-passed params
    ;; AARCH64:  ldr
    ;; AARCH64-NEXT:  str
    ;; AARCH64-NOT:  bl
    ;; AARCH64-NOT:  ret
    ;; AARCH64: add  sp, sp,
    ;; AARCH64-NEXT:  b  {{0x[0-9a-f]+}}
    ;; TRICORE: ld.a{{.*}}[sp]#
    ;; TRICORE-NEXT: st.a{{.*}}[sp]#
    ;; TRICORE: ld.a{{.*}}[sp]#
    ;; TRICORE-NEXT: st.a{{.*}}[sp]#
    ;; TRICORE-NOT:  fcall
    ;; TRICORE-NOT:  fret
    ;; TRICORE: lea  sp, [sp]#
    ;; TRICORE-NEXT: j  {{#0x[0-9a-f]+}}
    ;; X86_64: movss  [[TMP:xmm[0-9]+]], dword ptr [rsp +
    ;; X86_64-NEXT: movss  dword ptr [rsp +
    ;; X86_64-NOT: call
    ;; X86_64: lea  rsp, [rsp +
    ;; X86_64-NEXT: jmp  {{0x[0-9a-f]+}}
    ;; CHECK: Size of the function body
    return_call $callee10
  )

  ;; CHECK-LABEL: Function[8] Body
  (func $callee64 (param i64 i64) (result i64)
    local.get 0)

  ;; CHECK-LABEL: Function[9] Body
  ;; TriCore placeholder accuracy: 64-bit swap must preserve the low/high half pairing order.
  (func $tail_call_64_swap (param i64 i64) (result i64)
    local.get 1
    local.get 0
    ;; TRICORE: ld.d{{.*}}[sp]#0x88
    ;; TRICORE-NEXT: ld.da{{.*}}[sp]#0x90
    ;; TRICORE-NEXT: st.da{{.*}}[sp]#0x88
    ;; TRICORE-NEXT: st.d{{.*}}[sp]#0x90
    ;; TRICORE-NOT:  fcall
    ;; TRICORE-NOT:  fret
    ;; TRICORE: lea  sp, [sp]#
    ;; TRICORE-NEXT: j  {{#0x[0-9a-f]+}}
    ;; X86_64: xchg  rbp, rdi
    ;; X86_64: lea  rsp, [rsp +
    ;; X86_64-NEXT: jmp  {{0x[0-9a-f]+}}
    return_call $callee64
  )

  ;; CHECK-LABEL: Function[10] Body
  ;; Parameter 8 is stack-passed, but its source is the register result of i32.add.
  (func $tail_call_register_to_stack (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    local.get 0 local.get 1 local.get 2 local.get 3
    local.get 4 local.get 5 local.get 6 local.get 7
    local.get 0
    i32.const 1
    i32.add
    local.get 9
    ;; AARCH64: add  [[AARCH64_TMP:w[0-9]+]], w19, #1
    ;; AARCH64-NEXT: str  [[AARCH64_TMP]], [sp, #{{0x[0-9a-f]+}}]
    ;; TRICORE: add  [[TRICORE_TMP:d[0-9]+]], d8, #1
    ;; TRICORE-NEXT: st.w  [sp]#{{0x[0-9a-f]+}}, [[TRICORE_TMP]]
    ;; X86_64: mov  [[X86_64_TMP:r[0-9]+d]], ebp
    ;; X86_64-NEXT: add  [[X86_64_TMP]], 1
    ;; X86_64-NEXT: mov  dword ptr [rsp + {{0x[0-9a-f]+}}], [[X86_64_TMP]]
    return_call $callee10
  )

  ;; CHECK-LABEL: Function[11] Body
  ;; p0 reads caller stack parameter 8 while p8 reads ebp, forming p0 <- [p8] and [p8] <- ebp.
  ;; targetStackMemoryUsedByOtherParams must defer p8's stack write to the parallel-move resolver.
  (func $tail_call_register_stack_cycle (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    local.get 8
    local.get 1 local.get 2 local.get 3 local.get 4
    local.get 5 local.get 6 local.get 7
    local.get 0
    local.get 9
    ;; X86_64: xchg  dword ptr [rsp + {{0x[0-9a-f]+}}], ebp
    ;; X86_64: lea  rsp, [rsp +
    ;; X86_64-NEXT: jmp  {{0x[0-9a-f]+}}
    return_call $callee10
  )
)
