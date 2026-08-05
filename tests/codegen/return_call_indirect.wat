(module
  (type $unary (func (param i32) (result i32)))
  (type $sum10 (func (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)))
  (table 2 funcref)

  (func $callee (type $unary)
    local.get 0)

  (func $sum10 (type $sum10)
    local.get 0)

  (elem (i32.const 0) $callee $sum10)

  ;; CHECK-LABEL: Function[2] Body
  ;; no-link branch rather than a normal indirect call.
  (func $tail_call_indirect (param i32 i32) (result i32)
    local.get 1
    local.get 0
    ;; AARCH64: cbz
    ;; AARCH64: cmp
    ;; AARCH64: ldur
    ;; AARCH64: ldr
    ;; AARCH64-NOT: blr
    ;; AARCH64-NOT: ret
    ;; AARCH64: br
    ;; TRICORE: jlt.u
    ;; TRICORE: ld.a
    ;; TRICORE-NOT: fcalli
    ;; TRICORE-NOT: fret
    ;; TRICORE: ji
    ;; X86_64: cmp
    ;; X86_64: mov
    ;; X86_64-NOT: call
    ;; X86_64-NOT: ret
    ;; X86_64: jmp
    return_call_indirect (type $unary)
  )

  ;; CHECK-LABEL: Function[3] Body
  (func $tail_call_indirect_fallback (param i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.const 3
    i32.const 4
    i32.const 5
    i32.const 6
    i32.const 7
    i32.const 8
    i32.const 9
    i32.const 10
    i32.const 1
    ;; No need to save locals before the fallback call.
    ;; AARCH64-NOT:  str w19,  [sp, [[OFFSET:#0x[0-9a-f]+]]]
    ;; AARCH64-NOT:  str w8,  [sp, [[OFFSET:#0x[0-9a-f]+]]]
    ;; AARCH64-NOT:  str w1,  [sp, [[OFFSET:#0x[0-9a-f]+]]]
    ;; AARCH64-NOT:  str w2,  [sp, [[OFFSET:#0x[0-9a-f]+]]]
    ;; AARCH64-NOT:  str w3,  [sp, [[OFFSET:#0x[0-9a-f]+]]]
    ;; AARCH64: blr
    ;; AARCH64: ret

    ;; TRICORE: jge.u
    ;; TRICORE-NOT: st.w  [sp]{{.*}}, d8
    ;; TRICORE: fcalli
    ;; TRICORE: fret

    ;; X86_64: cmp
    ;; X86_64-NOT: mov dword ptr [rsp{{.*}}, ebp
    ;; X86_64: mov
    ;; X86_64: call
    ;; X86_64: ret
    return_call_indirect (type $sum10)
  )
)