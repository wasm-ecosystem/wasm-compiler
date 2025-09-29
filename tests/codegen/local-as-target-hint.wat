(module
    ;; CHECK-LABEL: Function[0] Body
    (func $localInRam (param i32) (local i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
        local.get 29
        i32.const 0
        i32.eq
        ;; X86_64:  sete  byte ptr [rsp + [[NUM:0x[0-9a-f]+]]]
        local.set 29
    )

    ;; CHECK-LABEL: Function[1] Body
    (func $foo  (result i64)
    (local i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64)
    (local $l1 i64)

    i64.const 0xFFFFFFFFFF
    i64.const 0
    i64.add
    ;; X86_64:  mov [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], [[REG]]
    ;; X86_64-NEXT: mov qword ptr [rsp + [[NUM:0x[0-9a-f]+]]], rax
    ;; AARCH64: mov [[REG:w[0-9]+]], [[REG]]
    ;; AARCH64-NEXT: str  x0, [sp, [[OFFSET:#0x[0-9a-fA-F]+]]]
    i32.wrap_i64
    i64.extend_i32_u
    local.tee $l1
  )
)