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
        ;; X86_64-NOT:  mov  {{[a-z0-9]+}}, 3
        ;; X86_64:      movzx  {{[a-z0-9]+}}, word ptr [[[SRC3:[a-z0-9]+]]]
        ;; X86_64-NEXT: movzx  {{[a-z0-9]+}}, word ptr [[[SRC3]] + 1]
        ;; X86_64-NEXT: mov  word ptr [[[DST3:[a-z0-9]+]]], {{[a-z0-9]+}}
        ;; X86_64-NEXT: mov  word ptr [[[DST3]] + 1], {{[a-z0-9]+}}
        memory.copy
    )

    ;; CHECK-LABEL: Function[1] Body
    (func (param i32 i32)
        ;; X86_64-NOT:  mov  {{[a-z0-9]+}}, 0x1f
        ;; X86_64:      movupd  xmm4, xmmword ptr [[[SRC31:[a-z0-9]+]]]
        ;; X86_64-NEXT: movupd  xmm5, xmmword ptr [[[SRC31]] + 0xf]
        ;; X86_64-NEXT: movupd  xmmword ptr [[[DST31:[a-z0-9]+]]], xmm4
        ;; X86_64-NEXT: movupd  xmmword ptr [[[DST31]] + 0xf], xmm5
        (memory.copy (local.get 0) (local.get 1) (i32.const 31))
    )

    ;; CHECK-LABEL: Function[2] Body
    (func (param i32 i32) (param f64 f64 f64 f64) (result f64)
        ;; X86_64-NOT:  mov  {{[a-z0-9]+}}, 0x71
        ;; X86_64:      movupd  xmm8, xmmword ptr [[[SRC113:[a-z0-9]+]]]
        ;; X86_64-NEXT: movupd  xmm9, xmmword ptr [[[SRC113]] + 0x10]
        ;; X86_64-NEXT: movupd  xmm10, xmmword ptr [[[SRC113]] + 0x20]
        ;; X86_64-NEXT: movupd  xmm11, xmmword ptr [[[SRC113]] + 0x30]
        ;; X86_64-NEXT: movupd  xmm12, xmmword ptr [[[SRC113]] + 0x40]
        ;; X86_64-NEXT: movupd  xmm13, xmmword ptr [[[SRC113]] + 0x50]
        ;; X86_64-NEXT: movupd  xmm14, xmmword ptr [[[SRC113]] + 0x60]
        ;; X86_64-NEXT: movupd  xmm0, xmmword ptr [[[SRC113]] + 0x61]
        ;; X86_64-NEXT: movupd  xmmword ptr [[[DST113:[a-z0-9]+]]], xmm8
        ;; X86_64-NEXT: movupd  xmmword ptr [[[DST113]] + 0x10], xmm9
        ;; X86_64-NEXT: movupd  xmmword ptr [[[DST113]] + 0x20], xmm10
        ;; X86_64-NEXT: movupd  xmmword ptr [[[DST113]] + 0x30], xmm11
        ;; X86_64-NEXT: movupd  xmmword ptr [[[DST113]] + 0x40], xmm12
        ;; X86_64-NEXT: movupd  xmmword ptr [[[DST113]] + 0x50], xmm13
        ;; X86_64-NEXT: movupd  xmmword ptr [[[DST113]] + 0x60], xmm14
        ;; X86_64-NEXT: movupd  xmmword ptr [[[DST113]] + 0x61], xmm0
        (memory.copy (local.get 0) (local.get 1) (i32.const 113))
        local.get 2
    )

    ;; CHECK-LABEL: Function[3] Body
    (func (param i32 i32)
        (local f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64)
        ;; X86_64-NOT:  mov  {{[a-z0-9]+}}, 0x40
        ;; X86_64:      movupd  xmm0, xmmword ptr [[[SRC64:[a-z0-9]+]]]
        ;; X86_64-NEXT: movupd  xmm1, xmmword ptr [[[SRC64]] + 0x10]
        ;; X86_64-NEXT: movupd  xmm2, xmmword ptr [[[SRC64]] + 0x20]
        ;; X86_64-NEXT: movupd  xmm3, xmmword ptr [[[SRC64]] + 0x30]
        ;; X86_64-NEXT: movupd  xmmword ptr [[[DST64:[a-z0-9]+]]], xmm0
        ;; X86_64-NEXT: movupd  xmmword ptr [[[DST64]] + 0x10], xmm1
        ;; X86_64-NEXT: movupd  xmmword ptr [[[DST64]] + 0x20], xmm2
        ;; X86_64-NEXT: movupd  xmmword ptr [[[DST64]] + 0x30], xmm3
        (memory.copy (local.get 0) (local.get 1) (i32.const 64))
    )

    ;; CHECK-LABEL: Function[4] Body
    (func (param i32 i32)
        (local f64 f64 f64 f64 f64 f64 f64 f64 f64 f64 f64)
        ;; X86_64-NOT: movupd
        ;; X86_64: mov  {{[a-z0-9]+}}, 0x80
        ;; X86_64-NEXT: cmp  {{[a-z0-9]+}}, {{[a-z0-9]+}}
        ;; X86_64: cmp  {{[a-z0-9]+}}, 8
        ;; X86_64-NOT: movupd
        ;; X86_64: ret
        (memory.copy (local.get 0) (local.get 1) (i32.const 128))
    )

    ;; CHECK-LABEL: Function[5] Body
    (func (param i32 i32)
        ;; X86_64-NOT: mov  {{[a-z0-9]+}}, 0{{[ \t]*$}}
        ;; X86_64: cmova  {{[a-z0-9]+}}, {{[a-z0-9]+}}
        ;; X86_64_PASSIVE-NEXT: cmp  byte ptr [{{[a-z0-9]+}} - 1], 0
        ;; X86_64_ACTIVE-NEXT: cmp  {{[a-z0-9]+}}, {{[a-z0-9]+}}
        ;; X86_64: lea  rsp, [rsp + {{0x[0-9a-f]+}}]
        ;; X86_64-NEXT: ret
        (memory.copy (local.get 0) (local.get 1) (i32.const 0))
    )

    ;; CHECK-LABEL: Function[6] Body
    (func (param i32 i32)
        ;; X86_64-NOT:  mov  {{[a-z0-9]+}}, 1
        ;; X86_64:      movzx  {{[a-z0-9]+}}, byte ptr [{{[a-z0-9]+}}]
        ;; X86_64-NEXT: mov  byte ptr [{{[a-z0-9]+}}], {{[a-z0-9]+}}
        ;; X86_64-NEXT: lea  rsp, [rsp + {{0x[0-9a-f]+}}]
        ;; X86_64-NEXT: ret
        (memory.copy (local.get 0) (local.get 1) (i32.const 1))
    )

    ;; CHECK-LABEL: Function[7] Body
    (func (param i32 i32)
        ;; X86_64-NOT:  mov  {{[a-z0-9]+}}, 7
        ;; X86_64:      mov  [[HEAD7:[a-z0-9]+]], dword ptr [[[SRC7:[a-z0-9]+]]]
        ;; X86_64-NEXT: mov  [[TAIL7:[a-z0-9]+]], dword ptr [[[SRC7]] + 3]
        ;; X86_64-NEXT: mov  dword ptr [[[DST7:[a-z0-9]+]]], [[HEAD7]]
        ;; X86_64-NEXT: mov  dword ptr [[[DST7]] + 3], [[TAIL7]]
        (memory.copy (local.get 0) (local.get 1) (i32.const 7))
    )

    ;; CHECK-LABEL: Function[8] Body
    (func (param i32 i32)
        ;; X86_64-NOT:  mov  {{[a-z0-9]+}}, 0xf
        ;; X86_64:      mov  [[HEAD15:[a-z0-9]+]], qword ptr [[[SRC15:[a-z0-9]+]]]
        ;; X86_64-NEXT: mov  [[TAIL15:[a-z0-9]+]], qword ptr [[[SRC15]] + 7]
        ;; X86_64-NEXT: mov  qword ptr [[[DST15:[a-z0-9]+]]], [[HEAD15]]
        ;; X86_64-NEXT: mov  qword ptr [[[DST15]] + 7], [[TAIL15]]
        (memory.copy (local.get 0) (local.get 1) (i32.const 15))
    )

    ;; CHECK-LABEL: Function[9] Body
    (func (param i32 i32)
        ;; X86_64: cmova  {{[a-z0-9]+}}, {{[a-z0-9]+}}
        ;; X86_64_ACTIVE-NEXT: add  [[END_MAX:r([a-z]+|[0-9]+)]], 0x7fffffff
        ;; X86_64_ACTIVE-NEXT: cmp  {{r([a-z]+|[0-9]+)}}, [[END_MAX]]
        ;; X86_64_PASSIVE-NEXT: cmp  byte ptr [{{r([a-z]+|[0-9]+)}} + 0x7ffffffe], 0
        (memory.copy (local.get 0) (local.get 1) (i32.const 0x7fffffff))
    )

    ;; CHECK-LABEL: Function[10] Body
    (func (param i32 i32)
        ;; X86_64: mov  [[SIZE_HIGH:e[a-z]+|r[0-9]+d]], 0x80000000
        ;; X86_64: cmova  {{[a-z0-9]+}}, {{[a-z0-9]+}}
        ;; X86_64_ACTIVE-NEXT: add  [[END_HIGH:r([a-z]+|[0-9]+)]], {{r([a-z]+|[0-9]+)}}
        ;; X86_64_ACTIVE-NEXT: cmp  {{r([a-z]+|[0-9]+)}}, [[END_HIGH]]
        ;; X86_64_PASSIVE-NEXT: cmp  byte ptr [{{r([a-z]+|[0-9]+)}} + {{r([a-z]+|[0-9]+)}} - 1], 0
        ;; X86_64: cmp  [[SIZE_HIGH]], 8
        ;; X86_64-NEXT: jb
        (memory.copy (local.get 0) (local.get 1) (i32.const 0x80000000))
    )

    ;; CHECK-LABEL: Function[11] Body
    (func (param i32 i32)
        ;; X86_64: mov  [[SIZE_UNSIGNED_MAX:e[a-z]+|r[0-9]+d]], 0xffffffff
        ;; X86_64: cmova  {{[a-z0-9]+}}, {{[a-z0-9]+}}
        ;; X86_64_ACTIVE-NEXT: add  [[END_UNSIGNED_MAX:r([a-z]+|[0-9]+)]], {{r([a-z]+|[0-9]+)}}
        ;; X86_64_ACTIVE-NEXT: cmp  {{r([a-z]+|[0-9]+)}}, [[END_UNSIGNED_MAX]]
        ;; X86_64_PASSIVE-NEXT: cmp  byte ptr [{{r([a-z]+|[0-9]+)}} + {{r([a-z]+|[0-9]+)}} - 1], 0
        ;; X86_64: cmp  [[SIZE_UNSIGNED_MAX]], 8
        ;; X86_64-NEXT: jb
        (memory.copy (local.get 0) (local.get 1) (i32.const 0xffffffff))
    )

)
