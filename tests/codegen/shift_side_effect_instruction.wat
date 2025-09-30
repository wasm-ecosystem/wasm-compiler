(module
    (memory 1)
    ;; CHECK-LABEL: Function[0] Body
    (func $x86_64_and_arm64_int (result i32)
        (local i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
        
        ;; X86_64_PASSIVE:         mov  [[REG1:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], dword ptr [rbx + [[NUM:[0-9a-f]+]]]
        ;; X86_64_PASSIVE-NEXT:    movzx [[REG2:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], byte ptr [rbx + [[NUM:[0-9a-f]+]]]
        ;; AARCH64_PASSIVE: ldr [[REG1:w[0-9]+]], [x29, [[REG:x[0-9]+]]]
        ;; AARCH64_PASSIVE: ldrb [[REG2:w[0-9]+]], [x29, [[REG:x[0-9]+]]]
        ;; AARCH64_PASSIVE: ldrh [[REG3:w[0-9]+]], [x29, [[REG:x[0-9]+]]]
        i32.const 1
        i32.load
        i32.const 1
        i32.add

        i32.const 2
        i32.add

        i32.const 2
        i32.load8_u
        ;; X86_64_PASSIVE: add [[REG1]], [[REG2]]
        ;; AARCH64_PASSIVE: add [[REG1]], [[REG1]], [[REG2]]
        i32.add
        ;; X86_64_PASSIVE-NEXT: movzx  [[REG3:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], word ptr [rbx + [[NUM:[0-9a-f]+]]]
        ;; AARCH64_PASSIVE: ldr [[REG4:w[0-9]+]], [x29, [[REG:x[0-9]+]]]
        i32.const 3
        i32.load16_u
        i32.const 4
        i32.load
        i32.add

        
        i32.add
        return
    )

    ;; CHECK-LABEL: Function[1] Body
    (func $x86_64_and_arm64_float (result f32)
        (local f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32)
        
        ;; X86_64_PASSIVE:         movss  [[REG1:xmm[0-9]+]], dword ptr [rbx + [[NUM:[0-9a-f]+]]]
        ;; X86_64_PASSIVE-NEXT:    movss  [[REG2:xmm[0-9]+]], dword ptr [rbx + [[NUM:[0-9a-f]+]]]
        ;; AARCH64_PASSIVE: ldr [[REG1:[sdv][0-9]+]], [x29, [[REG:x[0-9]+]]]
        ;; AARCH64_PASSIVE: ldr [[REG2:[sdv][0-9]+]], [x29, [[REG:x[0-9]+]]]
        ;; AARCH64_PASSIVE: ldr [[REG3:[sdv][0-9]+]], [x29, [[REG:x[0-9]+]]]
        i32.const 1
        f32.load
        f32.const 1
        f32.add

        f32.const 2
        f32.add

        i32.const 2
        f32.load
        ;; X86_64_PASSIVE: addss [[REG1]], [[REG2]]
        ;; AARCH64_PASSIVE: fadd [[REG1]], [[REG1]], [[REG2]]
        f32.add
        ;; X86_64_PASSIVE: movss  [[REG3:xmm[0-9]+]], dword ptr [rbx + [[NUM:[0-9a-f]+]]]
        ;; AARCH64_PASSIVE: ldr [[REG4:[sdv][0-9]+]], [x29, [[REG:x[0-9]+]]]
        i32.const 3
        f32.load
        i32.const 4
        f32.load
        f32.add

        
        f32.add
        return
    )

    ;; CHECK-LABEL: Function[2] Body
    (func $tricore32 (result i32)
        (local i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)

        i32.const 0
        ;;TRICORE:  ld.w  [[REG1:d[0-9]+]], [a2]
        ;;TRICORE:  ld.w  [[REG2:d[0-9]+]], [a2]#4
        ;;TRICORE:  ld.w  [[REG3:d[0-9]+]], [a2]#8
        ;;TRICORE:  ld.w  [[REG4:d[0-9]+]], [a2]#0xc
        i32.load
        i32.const 4
        i32.load
        ;;TRICORE: add [[REG1]], [[REG2]]
        i32.add

        i32.const 8
        i32.load
        i32.const 12
        i32.load
        i32.add
        ;;TRICORE:   ld.w  [[REG5:d[0-9]+]], [a2]#0x10
        i32.const 16
        i32.load

        i32.add
        i32.add
        return
    )

    ;; CHECK-LABEL: Function[3] Body
    (func $tricore64 (result i64)
        (local i64 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
        i64.const 0
        local.set 0

        local.get 0
        i64.const 1
        i64.mul

        i32.const 0
        ;;TRICORE:  ld.d  [[REG:e[0-9]+]], [a2]#0
        i64.load
        
        i64.add



        i32.const 8
        i64.load
        i64.add
        return
    )
)