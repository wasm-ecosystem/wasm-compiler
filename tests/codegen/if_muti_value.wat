(module
  ;; CHECK-LABEL: Function[0] Body
  (func (param i32) (result i32)
    local.get 0
    i32.const 5
    local.set 0
    i32.const 10
    ;; stack adjustment must be before the comparison to preserve the condition flags.
    ;; X86_64: lea rsp, [rsp - [[NUM:0x[0-9a-f]+]]]
    ;; X86_64: cmp  [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], 0xa
    ;; X86_64-NOT: cmp
    ;; X86_64: jne
    ;; AARCH64: sub sp, sp, #[[NUM:0x[0-9a-fA-F]+]]
    ;; AARCH64: cmp  [[REG:w[0-9]+]], #0xa
    ;; AARCH64-NOT: cmp
    ;; AARCH64: b.ne
    ;; Tricore jumps not rely on condition flags
    ;; TRICORE: eq [[REG:d[0-9]+]], [[REG]], #0xa
    i32.eq
    if (result i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
      i32.const 1
      i32.const 2
      i32.const 3
      i32.const 4
      i32.const 5
      i32.const 6
      i32.const 7
      i32.const 8
      i32.const 9
      i32.const 10
      i32.const 11
      i32.const 12
      i32.const 13
      i32.const 14
      i32.const 15
      i32.const 16
      i32.const 17
      i32.const 18
      i32.const 19
      i32.const 20
    else
      i32.const 3
      i32.const 4
      i32.const 5
      i32.const 6
      i32.const 7
      i32.const 8
      i32.const 9
      i32.const 10
      i32.const 11
      i32.const 12
      i32.const 13
      i32.const 14
      i32.const 15
      i32.const 16
      i32.const 17
      i32.const 18
      i32.const 19
      i32.const 20
      i32.const 21
      i32.const 22
    end

    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add

    return)
)