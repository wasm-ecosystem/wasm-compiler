(module
  (memory 1 100)
;; Test aarch64 linear memory memcpy with constant size
  ;; CHECK-LABEL: Function[0] Body
  (func
    i32.const 7
    i32.const 9
    i32.const 16

    ;; prepare overlap jmp
    ;; AARCH64: b.lo  0x[[#]]

    ;; 1 time 16-byte-copy
    ;; AARCH64-NEXT: ldp  d8, d1, [x8], #0x10
    ;; AARCH64-NEXT: stp  d8, d1, [x1], #0x10
    ;; AARCH64-NEXT: b  0x[[#]]
    memory.copy
  )
  ;; CHECK-LABEL: Function[1] Body
  (func
    i32.const 7
    i32.const 9
    i32.const 35

    ;; 16*2 + 3

    ;; prepare overlap jmp
    ;; AARCH64: b.lo  0x[[#]]

    ;; 2 time 16-byte-copy unrolling
    ;; AARCH64-NEXT: ldp  d8, d1, [x8], #0x10
    ;; AARCH64-NEXT: stp  d8, d1, [x1], #0x10
    ;; AARCH64-NEXT: ldp  d8, d1, [x8], #0x10
    ;; AARCH64-NEXT: stp  d8, d1, [x1], #0x10

    ;; force prepare: sub sizeReg one time
    ;; AARCH64-NEXT: subs  w19, w19, #0x20

    ;; 1-byte-copy with loop
    ;; AARCH64-NEXT: cbz  w19, 0x[[#]]
    ;; AARCH64-NEXT: ldrb  w2, [x8], #1
    ;; AARCH64-NEXT: sub  w19, w19, #1
    ;; AARCH64-NEXT: strb  w2, [x1], #1
    ;; AARCH64-NEXT: cbnz  w19, 0x[[#]]

    memory.copy
  )
  ;; CHECK-LABEL: Function[2] Body
  (func
    i32.const 7
    i32.const 9
    i32.const 34

    ;; 16*2 + 2

    ;; prepare overlap jmp
    ;; AARCH64: b.lo  0x[[#]]

    ;; 2 time 16-byte-copy unrolling
    ;; AARCH64-NEXT: ldp  d8, d1, [x8], #0x10
    ;; AARCH64-NEXT: stp  d8, d1, [x1], #0x10
    ;; AARCH64-NEXT: ldp  d8, d1, [x8], #0x10
    ;; AARCH64-NEXT: stp  d8, d1, [x1], #0x10

    ;; NO MORE sizeReg prepare since 1-byte-copy is unrolling
    ;; AARCH64-NOT: subs  w19, w19, #0x20

    ;; 2 times 1-byte-copy unrolling
    ;; AARCH64-NEXT: ldrb  w2, [x8], #1
    ;; AARCH64-NEXT: strb  w2, [x1], #1
    ;; AARCH64-NEXT: ldrb  w2, [x8], #1
    ;; AARCH64-NEXT: strb  w2, [x1], #1
    ;; AARCH64-NEXT: b  0x[[#]]

    memory.copy
  )
)
