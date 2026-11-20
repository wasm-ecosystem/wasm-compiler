(module
  (memory 1 100)
;; Test aarch64 linear memory memcpy with constant size
  ;; CHECK-LABEL: Function[0] Body
  (func
    i32.const 7
    i32.const 9
    i32.const 16

    ;; AARCH64: ldur  [[FIRST16:q27]], [[[SOURCE16:x[0-9]+]]]{{$}}
    ;; AARCH64-NEXT: stur  [[FIRST16]], [[[DESTINATION16:x[0-9]+]]]{{$}}
    ;; AARCH64-NOT: b.lo
    ;; AARCH64: ret
    memory.copy
  )
  ;; CHECK-LABEL: Function[1] Body
  (func
    i32.const 7
    i32.const 9
    i32.const 35

    ;; AARCH64: ldp  [[FIRST35:q27]], [[SECOND35:q28]], [[[SOURCE35:x[0-9]+]]]{{$}}
    ;; AARCH64-NEXT: ldur  [[TAIL35:q29]], [[[SOURCE35]], #0x13]
    ;; AARCH64-NEXT: stp  [[FIRST35]], [[SECOND35]], [[[DESTINATION35:x[0-9]+]]]{{$}}
    ;; AARCH64-NEXT: stur  [[TAIL35]], [[[DESTINATION35]], #0x13]
    ;; AARCH64-NOT: ldrb
    ;; AARCH64-NOT: b.lo
    ;; AARCH64: ret

    memory.copy
  )
  ;; CHECK-LABEL: Function[2] Body
  (func
    i32.const 7
    i32.const 9
    i32.const 34

    ;; AARCH64: ldp  [[FIRST34:q27]], [[SECOND34:q28]], [[[SOURCE34:x[0-9]+]]]{{$}}
    ;; AARCH64-NEXT: ldur  [[TAIL34:q29]], [[[SOURCE34]], #0x12]
    ;; AARCH64-NEXT: stp  [[FIRST34]], [[SECOND34]], [[[DESTINATION34:x[0-9]+]]]{{$}}
    ;; AARCH64-NEXT: stur  [[TAIL34]], [[[DESTINATION34]], #0x12]
    ;; AARCH64-NOT: ldrb
    ;; AARCH64-NOT: b.lo
    ;; AARCH64: ret

    memory.copy
  )
  ;; CHECK-LABEL: Function[3] Body
  (func (param i32 i32)
    local.get 0
    local.get 1
    i32.const 32
    ;; AARCH64: ldp  [[FIRST32:q27]], [[SECOND32:q28]], [[[SOURCE32:x[0-9]+]]]{{$}}
    ;; AARCH64-NEXT: stp  [[FIRST32]], [[SECOND32]], [[[DESTINATION32:x[0-9]+]]]{{$}}
    ;; AARCH64-NOT: b.lo
    ;; AARCH64: ret
    memory.copy
  )
  ;; CHECK-LABEL: Function[4] Body
  (func (param i32 i32)
    local.get 0
    local.get 1
    i32.const 49
    ;; AARCH64: ldp  [[FIRST49:q27]], [[SECOND49:q28]], [[[SOURCE49:x[0-9]+]]]{{$}}
    ;; AARCH64-NEXT: ldur  [[MIDDLE49:q29]], [[[SOURCE49]], #0x20]
    ;; AARCH64-NEXT: ldur  [[TAIL49:q30]], [[[SOURCE49]], #0x21]
    ;; AARCH64-NEXT: stp  [[FIRST49]], [[SECOND49]], [[[DESTINATION49:x[0-9]+]]]{{$}}
    ;; AARCH64-NEXT: stur  [[MIDDLE49]], [[[DESTINATION49]], #0x20]
    ;; AARCH64-NEXT: stur  [[TAIL49]], [[[DESTINATION49]], #0x21]
    ;; AARCH64-NOT: b.lo
    ;; AARCH64: ret
    memory.copy
  )
  ;; CHECK-LABEL: Function[5] Body
  (func (param i32 i32)
    local.get 0
    local.get 1
    i32.const 64
    ;; AARCH64: ld1  { [[GROUP64:v27\.16b, v28\.16b, v29\.16b, v30\.16b]] }, [[[SOURCE64:x[0-9]+]]]{{$}}
    ;; AARCH64-NEXT: st1  { [[GROUP64]] }, [[[DESTINATION64:x[0-9]+]]]{{$}}
    ;; AARCH64-NOT: b.lo
    ;; AARCH64: ret
    memory.copy
  )
  ;; CHECK-LABEL: Function[6] Body
  (func (param i32 i32)
    local.get 0
    local.get 1
    i32.const 129
    ;; Sizes above 128 bytes always use the fallback, even when SIMD registers are free.
    ;; Prepare the overlap jump, then copy 16-byte chunks with forward/backward D-register loops.
    ;; AARCH64: b.lo  0x[[#]]
    ;; AARCH64: ldp  {{d[0-9]+}}, {{d[0-9]+}}, [{{x[0-9]+}}], #0x10
    ;; AARCH64-NEXT: subs  {{w[0-9]+}}, {{w[0-9]+}}, #0x10
    ;; AARCH64-NEXT: stp  {{d[0-9]+}}, {{d[0-9]+}}, [{{x[0-9]+}}], #0x10
    ;; AARCH64-NEXT: b.pl  0x[[#]]
    ;; AARCH64: ldp  {{d[0-9]+}}, {{d[0-9]+}}, [{{x[0-9]+}}, #-0x10]!
    ;; AARCH64-NEXT: subs  {{w[0-9]+}}, {{w[0-9]+}}, #0x10
    ;; AARCH64-NEXT: stp  {{d[0-9]+}}, {{d[0-9]+}}, [{{x[0-9]+}}, #-0x10]!
    ;; AARCH64-NEXT: b.pl  0x[[#]]
    memory.copy
  )
  ;; CHECK-LABEL: Function[7] Body
  (func (param i32 i32)
    local.get 0
    local.get 1
    i32.const 128
    ;; AARCH64: ld1  { [[HEAD128:v23\.16b, v24\.16b, v25\.16b, v26\.16b]] }, [[[SOURCE128:x[0-9]+]]], #64
    ;; AARCH64-NEXT: ld1  { [[TAIL128:v27\.16b, v28\.16b, v29\.16b, v30\.16b]] }, [[[SOURCE128]]]{{$}}
    ;; AARCH64-NEXT: st1  { [[HEAD128]] }, [[[DESTINATION128:x[0-9]+]]], #64
    ;; AARCH64-NEXT: st1  { [[TAIL128]] }, [[[DESTINATION128]]]{{$}}
    ;; AARCH64-NOT: b.lo
    ;; AARCH64: ret
    memory.copy
  )
  ;; CHECK-LABEL: Function[8] Body
  (func (param i32 i32)
    local.get 0
    local.get 1
    i32.const 113
    ;; AARCH64: ld1  { [[HEAD113:v23\.16b, v24\.16b, v25\.16b, v26\.16b]] }, [[[SOURCE113:x[0-9]+]]]{{$}}
    ;; AARCH64-NEXT: ldp  [[FIFTH113:q27]], [[SIXTH113:q28]], [[[SOURCE113]], #0x40]
    ;; AARCH64-NEXT: ldur  [[SEVENTH113:q29]], [[[SOURCE113]], #0x60]
    ;; AARCH64-NEXT: ldur  [[EIGHTH113:q30]], [[[SOURCE113]], #0x61]
    ;; AARCH64-NEXT: st1  { [[HEAD113]] }, [[[DESTINATION113:x[0-9]+]]]{{$}}
    ;; AARCH64-NEXT: stp  [[FIFTH113]], [[SIXTH113]], [[[DESTINATION113]], #0x40]
    ;; AARCH64-NEXT: stur  [[SEVENTH113]], [[[DESTINATION113]], #0x60]
    ;; AARCH64-NEXT: stur  [[EIGHTH113]], [[[DESTINATION113]], #0x61]
    ;; AARCH64-NOT: b.lo
    ;; AARCH64: ret
    memory.copy
  )
  ;; CHECK-LABEL: Function[9] Body
  (func (param i32 i32)
    (local f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64
           f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64)
    local.get 0
    local.get 1
    i32.const 128
    ;; The fixed eight-register tail includes statically allocated locals: use the mainline helper.
    ;; AARCH64-NOT: ld1
    ;; AARCH64-NOT: ldp  q
    ;; AARCH64: b.lo  0x[[#]]
    ;; AARCH64: ldp  {{d[0-9]+}}, {{d[0-9]+}}, [{{x[0-9]+}}], #0x10
    ;; AARCH64-NEXT: subs  {{w[0-9]+}}, {{w[0-9]+}}, #0x10
    ;; AARCH64-NEXT: stp  {{d[0-9]+}}, {{d[0-9]+}}, [{{x[0-9]+}}], #0x10
    ;; AARCH64-NEXT: b.pl  0x[[#]]
    ;; AARCH64: ldp  {{d[0-9]+}}, {{d[0-9]+}}, [{{x[0-9]+}}, #-0x10]!
    ;; AARCH64-NEXT: subs  {{w[0-9]+}}, {{w[0-9]+}}, #0x10
    ;; AARCH64-NEXT: stp  {{d[0-9]+}}, {{d[0-9]+}}, [{{x[0-9]+}}, #-0x10]!
    ;; AARCH64-NEXT: b.pl  0x[[#]]
    ;; AARCH64-NOT: ld1
    ;; AARCH64-NOT: ldp  q
    ;; AARCH64: ret
    memory.copy
  )
  ;; CHECK-LABEL: Function[10] Body
  (func (param i32 i32)
    (local f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64
           f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64)
    local.get 0
    local.get 1
    i32.const 64
    ;; A different free group is deliberately not considered when the physical tail is unavailable.
    ;; AARCH64-NOT: ld1
    ;; AARCH64-NOT: ldp  q
    ;; AARCH64: b.lo  0x[[#]]
    ;; AARCH64: ldp  {{d[0-9]+}}, {{d[0-9]+}}, [{{x[0-9]+}}], #0x10
    ;; AARCH64-NEXT: subs  {{w[0-9]+}}, {{w[0-9]+}}, #0x10
    ;; AARCH64-NEXT: stp  {{d[0-9]+}}, {{d[0-9]+}}, [{{x[0-9]+}}], #0x10
    ;; AARCH64-NEXT: b.pl  0x[[#]]
    ;; AARCH64: ldp  {{d[0-9]+}}, {{d[0-9]+}}, [{{x[0-9]+}}, #-0x10]!
    ;; AARCH64-NEXT: subs  {{w[0-9]+}}, {{w[0-9]+}}, #0x10
    ;; AARCH64-NEXT: stp  {{d[0-9]+}}, {{d[0-9]+}}, [{{x[0-9]+}}, #-0x10]!
    ;; AARCH64-NEXT: b.pl  0x[[#]]
    ;; AARCH64-NOT: ld1
    ;; AARCH64-NOT: ldp  q
    ;; AARCH64: ret
    memory.copy
  )
  ;; CHECK-LABEL: Function[11] Body
  (func (param i32 i32)
    (local f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64
           f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64)
    local.get 0
    local.get 1
    i32.const 128
    ;; One group is insufficient: the complete fixed eight-register tail must be available.
    ;; AARCH64-NOT: ld1
    ;; AARCH64-NOT: ldp  q
    ;; AARCH64: b.lo  0x[[#]]
    ;; AARCH64: ldp  {{d[0-9]+}}, {{d[0-9]+}}, [{{x[0-9]+}}], #0x10
    ;; AARCH64-NEXT: subs  {{w[0-9]+}}, {{w[0-9]+}}, #0x10
    ;; AARCH64-NEXT: stp  {{d[0-9]+}}, {{d[0-9]+}}, [{{x[0-9]+}}], #0x10
    ;; AARCH64-NEXT: b.pl  0x[[#]]
    ;; AARCH64: ldp  {{d[0-9]+}}, {{d[0-9]+}}, [{{x[0-9]+}}, #-0x10]!
    ;; AARCH64-NEXT: subs  {{w[0-9]+}}, {{w[0-9]+}}, #0x10
    ;; AARCH64-NEXT: stp  {{d[0-9]+}}, {{d[0-9]+}}, [{{x[0-9]+}}, #-0x10]!
    ;; AARCH64-NEXT: b.pl  0x[[#]]
    ;; AARCH64-NOT: ld1
    ;; AARCH64-NOT: ldp  q
    ;; AARCH64: ret
    memory.copy
  )

  ;; Preserve the original constant-copy unrolling and byte-tail checks in the fallback.
  ;; The 26 f64 locals make the fixed four-register SIMD tail unavailable in each case below.
  ;; CHECK-LABEL: Function[12] Body
  (func
    (local f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64
           f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64)
    i32.const 7
    i32.const 9
    i32.const 16

    ;; Prepare the overlap jump.
    ;; AARCH64-NOT: ld1
    ;; AARCH64-NOT: ldp  q
    ;; AARCH64: b.lo  0x[[#]]

    ;; One 16-byte copy, with no byte tail or counted loop.
    ;; AARCH64-NEXT: ldp  [[FALLBACK16_FIRST:d[0-9]+]], [[FALLBACK16_SECOND:d[0-9]+]], [[[FALLBACK16_SOURCE:x[0-9]+]]], #0x10
    ;; AARCH64-NEXT: stp  [[FALLBACK16_FIRST]], [[FALLBACK16_SECOND]], [{{x[0-9]+}}], #0x10
    ;; AARCH64-NEXT: b  0x[[#]]
    ;; AARCH64: ret
    memory.copy
  )
  ;; CHECK-LABEL: Function[13] Body
  (func
    (local f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64
           f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64)
    i32.const 7
    i32.const 9
    i32.const 35

    ;; 16 * 2 + 3: initialize the fallback size register and prepare the overlap jump.
    ;; AARCH64: mov  [[FALLBACK35_SIZE:w[0-9]+]], #0x23
    ;; AARCH64-NOT: ld1
    ;; AARCH64-NOT: ldp  q
    ;; AARCH64: b.lo  0x[[#]]

    ;; Two 16-byte copies are unrolled.
    ;; AARCH64-NEXT: ldp  [[FALLBACK35_FIRST:d[0-9]+]], [[FALLBACK35_SECOND:d[0-9]+]], [[[FALLBACK35_SOURCE:x[0-9]+]]], #0x10
    ;; AARCH64-NEXT: stp  [[FALLBACK35_FIRST]], [[FALLBACK35_SECOND]], [[[FALLBACK35_DESTINATION:x[0-9]+]]], #0x10
    ;; AARCH64-NEXT: ldp  [[FALLBACK35_FIRST]], [[FALLBACK35_SECOND]], [[[FALLBACK35_SOURCE]]], #0x10
    ;; AARCH64-NEXT: stp  [[FALLBACK35_FIRST]], [[FALLBACK35_SECOND]], [[[FALLBACK35_DESTINATION]]], #0x10

    ;; Subtract the unrolled 32 bytes to prepare the remaining byte count.
    ;; AARCH64-NEXT: subs  [[FALLBACK35_SIZE]], [[FALLBACK35_SIZE]], #0x20

    ;; Three remaining bytes exceed the unrolling threshold: copy one byte per loop iteration.
    ;; AARCH64-NEXT: cbz  [[FALLBACK35_SIZE]], 0x[[#]]
    ;; AARCH64-NEXT: ldrb  [[FALLBACK35_BYTE:w[0-9]+]], [[[FALLBACK35_SOURCE]]], #1
    ;; AARCH64-NEXT: sub  [[FALLBACK35_SIZE]], [[FALLBACK35_SIZE]], #1
    ;; AARCH64-NEXT: strb  [[FALLBACK35_BYTE]], [[[FALLBACK35_DESTINATION]]], #1
    ;; AARCH64-NEXT: cbnz  [[FALLBACK35_SIZE]], 0x[[#]]
    ;; AARCH64-NEXT: b  0x[[#]]
    ;; AARCH64: ret
    memory.copy
  )
  ;; CHECK-LABEL: Function[14] Body
  (func
    (local f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64
           f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64)
    i32.const 7
    i32.const 9
    i32.const 34

    ;; 16 * 2 + 2: prepare the overlap jump.
    ;; AARCH64-NOT: ld1
    ;; AARCH64-NOT: ldp  q
    ;; AARCH64: b.lo  0x[[#]]

    ;; Two 16-byte copies are unrolled.
    ;; AARCH64-NEXT: ldp  [[FALLBACK34_FIRST:d[0-9]+]], [[FALLBACK34_SECOND:d[0-9]+]], [[[FALLBACK34_SOURCE:x[0-9]+]]], #0x10
    ;; AARCH64-NEXT: stp  [[FALLBACK34_FIRST]], [[FALLBACK34_SECOND]], [[[FALLBACK34_DESTINATION:x[0-9]+]]], #0x10
    ;; AARCH64-NEXT: ldp  [[FALLBACK34_FIRST]], [[FALLBACK34_SECOND]], [[[FALLBACK34_SOURCE]]], #0x10
    ;; AARCH64-NEXT: stp  [[FALLBACK34_FIRST]], [[FALLBACK34_SECOND]], [[[FALLBACK34_DESTINATION]]], #0x10

    ;; No extra size-register adjustment: both remaining byte copies are also unrolled.
    ;; AARCH64-NOT: subs  {{w[0-9]+}}, {{w[0-9]+}}, #0x20
    ;; AARCH64-NEXT: ldrb  [[FALLBACK34_BYTE:w[0-9]+]], [[[FALLBACK34_SOURCE]]], #1
    ;; AARCH64-NEXT: strb  [[FALLBACK34_BYTE]], [[[FALLBACK34_DESTINATION]]], #1
    ;; AARCH64-NEXT: ldrb  [[FALLBACK34_BYTE]], [[[FALLBACK34_SOURCE]]], #1
    ;; AARCH64-NEXT: strb  [[FALLBACK34_BYTE]], [[[FALLBACK34_DESTINATION]]], #1
    ;; AARCH64-NEXT: b  0x[[#]]
    ;; AARCH64: ret
    memory.copy
  )
)
