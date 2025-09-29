(module
    ;; CHECK-LABEL: Function[0] Body
    (func $I32ToI64Res (result i64)
        i32.const 1
        i32.const 2
        ;; X86_64:  add eax, 2
        ;; X86_64-NOT:  add [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]]
        ;; AARCH64:  add  w0, w0, #2
        ;; AARCH64-NOT: ubfx `[[REG:w[0-9]+]]` `[[REG:w[0-9]+]]`
        ;; TRICORE:         add  d2, #2
        ;; TRICORE-NOT:     mov  d2, [[REG:d[0-9]+]]
        ;; TRICORE-NEXT:    mov  d3, #0
        i32.add
        i64.extend_i32_u
        return
    )

    ;; CHECK-LABEL: Function[1] Body
    (func $I32ToLocal
        (local i64)

        i32.const 1
        i32.const 2
        ;; X86_64:  add ebp, 2
        ;; X86_64-NOT:  add [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]]
        ;; AARCH64:  add  w19, w19, #2
        ;; AARCH64-NOT: ubfx [[REG:w[0-9]+]] [[REG:w[0-9]+]]
        i32.add
        i64.extend_i32_u
        local.set 0
        return
    )

    ;; CHECK-LABEL: Function[2] Body
    (func $signedExtend (param i32) (result i64)
        local.get 0
        ;; X86_64:  popcnt  eax, ebp
        ;; AARCH64:  cnt  v8.8b, v8.8b
        ;; TRICORE:  popcnt.w  d2, d8
        i32.popcnt
        ;; X86_64-NEXT: movsxd  rax, eax
        i64.extend_i32_s
    )
    ;; CHECK-LABEL: Function[3] Body
    (func $compareResult  (result i64)
    i32.const 1
    i32.const 1
    ;; X86_64:  sete  al
    ;; AARCH64: cset  w0, eq
    ;; TRICORE:         eq  d2, d2, #1
    ;; TRICORE-NEXT:    mov  d3, #0
    i32.eq
    i64.extend_i32_u
    )

    ;; CHECK-LABEL: Function[4] Body
    (func $localInRam (param i32) (param i64) (local i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64)
        local.get 29
        i64.const 0
        i64.eq
        ;; X86_64:  sete  [[REG:([abcd]l|[bs]pl|[sd]il)]]
        ;; X86_64-NEXT: mov  qword ptr [rsp + [[NUM:0x[0-9a-f]+]]], [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]]
        ;; AARCH64: str [[REG:x[0-9]+]], [sp, #[[NUM:0x[0-9a-f]+]]]
        i64.extend_i32_u
        local.set 29
    )

    ;; CHECK-LABEL: Function[5] Body
    (func (;1;) (result i32)
    (local i32)
    
        i64.const 10
        ;; AARCH64: rbit  [[REG1:x[0-9]+]], [[REG1:x[0-9]+]]
        ;; AARCH64: lsr  [[REG2:x[0-9]+]], [[REG2:x[0-9]+]], #1
        i64.ctz

        i64.const 0
        i64.const 1
        i64.shr_u
        ;; AARCH64: cmp  [[REG1]], [[REG2]]
        i64.le_u
        local.tee 0
    )

    (global (mut i64) (i64.const 0))

    ;; CHECK-LABEL: Function[6] Body
    (func (result i64)
    
        i64.const 1
        i64.const -9223372036854775808
        i64.eq
        i64.extend_i32_u
        ;; X86_64: sete [[REG:([abcd]l|[bs]pl|[sd]il)]]
        ;; X86_64: mov  qword ptr [rbx - [[NUM:0x[0-9a-f]+]]], [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]]
        global.set 1

        global.get 1
    )

    ;; CHECK-LABEL: Function[7] Body
    (func
  
        (local i64 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
        (local f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32)
        (local $f1 f32)
        f32.const 1
        local.set $f1
        local.get $f1

        i32.reinterpret_f32
        ;; X86_64: mov ebp,  dword ptr [rsp + [[NUM:0x[0-9a-f]+]]]
        ;; AARCH64: ldr w19, [sp, #[[NUM:0x[0-9a-f]+]]]
        i64.extend_i32_u

        ;; X86_64: add  rbp, 5
        ;; AARCH64: add  x19, x19, #5
        i64.const 5

        i64.add

        local.set 0
        return
    )

    ;; CHECK-LABEL: Function[8] Body
    (func (result i64)
         (local $l0 i64)
        i64.const 0x1122334455667788
        local.set $l0
        local.get $l0
        ;; X86_64: mov eax, ebp
        ;; AARCH64: mov w0, w19
        ;; TRICORE-NOT:     mov  d2, [[REG:d[0-9]+]]
        ;; TRICORE:         mov  d3, #0
        i32.wrap_i64
        i64.extend_i32_u
        return
    )

    (func $mix_f32_and_i32 (param i32)
        i32.const 0x123
        local.set 0
        ;; TRICORE:         mov.u  [[PARAM:d[0-9]+]], #0x123
        local.get 0
        f32.reinterpret_i32
        i32.reinterpret_f32
        ;; we can mix f32 and i32 in tricore
        local.set 0
        ;; TRICORE-NEXT:    lea  sp, [sp]#0x[[#%x,STACK_SIZE:]]
    )

    (global (mut i64) (i64.const 0xFFFFFFFF00000000))
)
