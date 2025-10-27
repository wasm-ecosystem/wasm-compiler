;; Test X86-64 linear memory memcpy with constant size
(module
    (memory  1)

;; Overlap

    ;; CHECK-LABEL: Function[0] Body
    (func
        i32.const 7
        i32.const 9
        ;; src(9) > dst(7)
        i32.const 3
        ;; copy8ByteCount == 0, copy1ByteCount == 3
        ;; unrolling 1 byte copy loop

        ;; X86_64:      jmp  0x[[#]]
        ;; X86_64-NEXT: jmp  0x[[#]]
        ;; X86_64-NEXT: add  [[REG1:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], rbp
        ;; X86_64-NEXT: add  [[REG2:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], rbp
        ;; X86_64-NEXT: movabs  rbp, 0xfffffffffffffffd
        ;; X86_64-NEXT: mov  [[REG3:(r[0-9]+(b|d))]], byte ptr [[[REG1]] + rbp]
        ;; X86_64-NEXT: mov  byte ptr [[[REG2]] + rbp], [[REG3]]
        ;; X86_64-NEXT: mov  [[REG3]], byte ptr [[[REG1]] + rbp + 1]
        ;; X86_64-NEXT: mov  byte ptr [[[REG2]] + rbp + 1], [[REG3]]
        ;; X86_64-NEXT: mov  [[REG3]], byte ptr [[[REG1]] + rbp + 2]
        ;; X86_64-NEXT: mov  byte ptr [[[REG2]] + rbp + 2], [[REG3]]
        memory.copy

;; TODO(): more test
    )
)
