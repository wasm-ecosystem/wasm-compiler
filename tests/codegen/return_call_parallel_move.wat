;; Codegen coverage for the AArch64 return_call (tail call) parallel-move temp
;; selection in Backend::execReturnCall Path A.

(module
  (func $callee2 (param i32 i32) (result i32) local.get 0)
  (func $callee2f (param f32 f32) (result f32) local.get 0)
  (func $callee3 (param i32 i32 i32) (result i32) local.get 0)
  (func $callee4 (param i32 i32 i32 i32) (result i32) local.get 0)

  ;; CHECK-LABEL: Function[4] Body
  ;; Case 1: a single 2-element cycle (swap of the two params). The first free
  ;; GPR candidate does not alias the cycle, so it is used directly as the temp.
  (func $case1_first_scratch_reg (param i32 i32) (result i32)
    local.get 1
    local.get 0
    ;; AARCH64:       cbz
    ;; AARCH64:       mov  [[TMP:w0]], w8
    ;; AARCH64-NEXT:  mov  w8, w19
    ;; AARCH64-NEXT:  mov  w19, [[TMP]]
    ;; AARCH64-NOT:   str  {{w[0-9]+}}, [sp
    ;; AARCH64:       add  sp, sp,
    ;; AARCH64-NEXT:  b  {{0x[0-9a-f]+}}
    ;; TRICORE:       mov  d0, d9
    ;; TRICORE-NEXT:  mov  d9, d8
    ;; TRICORE-NEXT:  mov  d8, d0
    ;; TRICORE-NOT:   fcall
    ;; TRICORE:       lea  sp, [sp]#
    ;; TRICORE-NEXT:  j  {{#0x[0-9a-f]+}}
    ;; X86_64:        mov  eax, edi
    ;; X86_64-NEXT:   mov  edi, ebp
    ;; X86_64-NEXT:   mov  ebp, eax
    ;; X86_64-NOT:    call
    ;; X86_64:        lea  rsp, [rsp +
    ;; X86_64-NEXT:   jmp  {{0x[0-9a-f]+}}
    return_call $callee2)

  ;; CHECK-LABEL: Function[5] Body
  ;; Case 2: the same direct-temp path for FPR values.
  (func $case2_first_float_scratch_reg (param f32 f32) (result f32)
    local.get 1
    local.get 0
    ;; AARCH64:       cbz
    ;; AARCH64:       fmov  [[TMP:s0]], s1
    ;; AARCH64-NEXT:  fmov  s1, s8
    ;; AARCH64-NEXT:  fmov  s8, [[TMP]]
    ;; AARCH64-NOT:   str  {{s[0-9]+}}, [sp
    ;; AARCH64:       add  sp, sp,
    ;; AARCH64-NEXT:  b  {{0x[0-9a-f]+}}
    ;; TRICORE:       mov  d0, d9
    ;; TRICORE-NEXT:  mov  d9, d8
    ;; TRICORE-NEXT:  mov  d8, d0
    ;; TRICORE-NOT:   fcall
    ;; TRICORE:       lea  sp, [sp]#
    ;; TRICORE-NEXT:  j  {{#0x[0-9a-f]+}}
    ;; X86_64:        movss  xmm0, xmm5
    ;; X86_64-NEXT:   movss  xmm5, xmm4
    ;; X86_64-NEXT:   movss  xmm4, xmm0
    ;; X86_64-NOT:    call
    ;; X86_64:        lea  rsp, [rsp +
    ;; X86_64-NEXT:   jmp  {{0x[0-9a-f]+}}
    return_call $callee2f)

  ;; CHECK-LABEL: Function[6] Body
  ;; Case 3 (GPR, "second candidate"): 3-element cycle where the expression result
  ;; occupies the first free GPR candidate register, so `canUseAsTemp` rejects it.
  ;; The resolver skips it and uses the *next* free GPR as temp ([[TMP]] ≠ [[V]]).
  (func $case3_first_scratch_reg_not_free (param i32 i32) (result i32)
    (i32.add (local.get 0) (local.get 1))
    (local.get 0)
    (local.get 1)
    ;; AARCH64:       add  [[V:w[0-9]+]], w19, w8
    ;; AARCH64-NEXT:  mov  [[TMP:w0]], [[V]]
    ;; AARCH64-NEXT:  mov  [[V]], w8
    ;; AARCH64-NEXT:  mov  w8, w19
    ;; AARCH64-NEXT:  mov  w19, [[TMP]]
    ;; AARCH64-NOT:   str  {{w[0-9]+}}, [sp
    ;; AARCH64:       add  sp, sp,
    ;; AARCH64-NEXT:  b  {{0x[0-9a-f]+}}
    ;; TRICORE:       add  d15, d8, d9
    ;; TRICORE-NEXT:  mov  d6, d9
    ;; TRICORE-NEXT:  mov  d9, d8
    ;; TRICORE-NEXT:  mov  d8, d15
    ;; TRICORE-NOT:   fcall
    ;; TRICORE:       lea  sp, [sp]#
    ;; TRICORE-NEXT:  j  {{#0x[0-9a-f]+}}
    ;; X86_64:        mov  [[TMP:(r9d|esi)]], ebp
    ;; X86_64-NEXT:   add  [[TMP]], edi
    ;; X86_64-NEXT:   mov  eax, [[TMP]]
    ;; X86_64-NEXT:   mov  [[TMP]], edi
    ;; X86_64-NEXT:   mov  edi, ebp
    ;; X86_64-NEXT:   mov  ebp, eax
    ;; X86_64-NOT:    call
    ;; X86_64:        lea  rsp, [rsp +
    ;; X86_64-NEXT:   jmp  {{0x[0-9a-f]+}}
    return_call $callee3)

  ;; CHECK-LABEL: Function[7] Body
  ;; Case 4: two independent 2-element cycles (swap params 0<->1 and 2<->3).
  ;; Each cycle is resolved separately with a register temp, so there are two
  ;; back-to-back swap sequences and no stack spill.
  (func $case4_multiple_cycles (param i32 i32 i32 i32) (result i32)
    local.get 1
    local.get 0
    local.get 3
    local.get 2
    ;; AARCH64:       mov  [[T1:w0]], w8
    ;; AARCH64-NEXT:  mov  w8, w19
    ;; AARCH64-NEXT:  mov  w19, [[T1]]
    ;; AARCH64-NEXT:  mov  [[T2:w0]], w2
    ;; AARCH64-NEXT:  mov  w2, w1
    ;; AARCH64-NEXT:  mov  w1, [[T2]]
    ;; AARCH64-NOT:   str  {{w[0-9]+}}, [sp
    ;; AARCH64:       add  sp, sp,
    ;; AARCH64-NEXT:  b  {{0x[0-9a-f]+}}
    ;; TRICORE:       mov  d0, d9
    ;; TRICORE-NEXT:  mov  d9, d8
    ;; TRICORE-NEXT:  mov  d8, d0
    ;; TRICORE-NEXT:  mov  d0, d7
    ;; TRICORE-NEXT:  mov  d7, d6
    ;; TRICORE-NEXT:  mov  d6, d0
    ;; TRICORE-NOT:   fcall
    ;; TRICORE:       lea  sp, [sp]#
    ;; TRICORE-NEXT:  j  {{#0x[0-9a-f]+}}
    ;; X86_64:        mov  eax, edi
    ;; X86_64-NEXT:   mov  edi, ebp
    ;; X86_64-NEXT:   mov  ebp, eax
    ;; X86_64-NEXT:   mov  eax, [[T2SRC:(r10d|r9d)]]
    ;; X86_64-NEXT:   mov  [[T2SRC]], [[T2DST:(r9d|esi)]]
    ;; X86_64-NEXT:   mov  [[T2DST]], eax
    ;; X86_64-NOT:    call
    ;; X86_64:        lea  rsp, [rsp +
    ;; X86_64-NEXT:   jmp  {{0x[0-9a-f]+}}
    return_call $callee4)

  ;; CHECK-LABEL: Function[8] Body
  ;; Case 5: the callee takes more params than the caller, yet none of them spill
  ;; to the stack (both fit in registers, so caller/callee stackParamWidth == 0).
  ;; The 0 <= 0 guard still selects Path A (TailJump): the extra callee args are
  ;; filled from registers and the function ends in `add sp, sp` + `b`, with no
  ;; stack store and no regular call.
  (func $case5_more_callee_params_no_stack (param i32) (result i32)
    local.get 0
    local.get 0
    local.get 0
    ;; AARCH64:       cbz
    ;; AARCH64:       mov  w8, w19
    ;; AARCH64-NEXT:  mov  w1, w19
    ;; AARCH64-NOT:   str  {{w[0-9]+}}, [sp
    ;; AARCH64:       add  sp, sp,
    ;; AARCH64-NEXT:  b  {{0x[0-9a-f]+}}
    ;; TRICORE:       mov  d9, d8
    ;; TRICORE-NEXT:  mov  d6, d8
    ;; TRICORE-NOT:   fcall
    ;; TRICORE:       lea  sp, [sp]#
    ;; TRICORE-NEXT:  j  {{#0x[0-9a-f]+}}
    ;; X86_64:        mov  edi, ebp
    ;; X86_64-NEXT:   mov  {{(r9d|esi)}}, ebp
    ;; X86_64-NOT:    call
    ;; X86_64:        lea  rsp, [rsp +
    ;; X86_64-NEXT:   jmp  {{0x[0-9a-f]+}}
    return_call $callee3)
)
