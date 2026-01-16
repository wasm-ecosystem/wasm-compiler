(module
    (import "env" "api-II" (func $api-II (param i64 i64) (result i64)))
    ;; CHECK-LABEL: Function[0] Body
    (func $callee-II (param i32 i32) (result i32)
        local.get 0
    )
    ;; CHECK-LABEL: Function[1] Body
    (func $targetHintMatched (result i32)
        (local i32 i32)
        i32.const 0
        local.set 0
        i32.const 0
        local.set 1
        ;; X86_64_PASSIVE: add  ebp, 1
        ;; AARCH64_PASSIVE: add  w19, w19, #1
        ;; TRICORE: add  d8, #1
        i32.const 1
        local.get 0
        i32.add
        ;; X86_64_PASSIVE: add  edi, 2
        ;; AARCH64_PASSIVE: add  w8, w8, #2
        ;; TRICORE: add  d9, #2
        i32.const 2
        local.get 1
        i32.add
        call $callee-II
    )


    ;; CHECK-LABEL: Function[2] Body
    (func $targetHintSpilled (result i32)
        (local i32 i32)
        i32.const 0
        local.set 0
        i32.const 0
        local.set 1

        local.get 0
        local.get 1
        ;;-----------------
        ;; X86_64_PASSIVE: add  ebp, 1
        ;; AARCH64_PASSIVE: add  w19, w19, #1
        ;; TRICORE: add  d8, #1
        i32.const 1
        local.get 0
        i32.add
        ;; X86_64_PASSIVE: add  edi, 2
        ;; AARCH64_PASSIVE: add  w8, w8, #2
        ;; TRICORE: add  d9, #2
        i32.const 2
        local.get 1
        i32.add
        call $callee-II
        drop
        i32.add
    )

    ;; CHECK-LABEL: Function[3] Body
    (func $targetHintUsedByOther (result i32)
        (local i32 i32)
        i32.const 0
        local.set 0
        i32.const 0
        local.set 1

        ;; X86_64_PASSIVE: add  [[REG1:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], 1
        ;; AARCH64_PASSIVE: add  [[REG1:w[0-9]+]], w8, #1
        ;; TRICORE: add  [[REG1:d[0-9]+]], d9, #1
        i32.const 1
        local.get 1
        i32.add
        ;; X86_64_PASSIVE: add  edi, 2
        ;; AARCH64_PASSIVE: add  w8, [[REG2:w[0-9]+]], #2
        ;; TRICORE: addi  d9, [[REG2:d[0-9]+]], #2
        i32.const 2
        local.get 0
        i32.add

        ;; X86_64_PASSIVE: mov ebp, [[REG1]]
        ;; AARCH64_PASSIVE: mov w19, [[REG1]]
        ;; TRICORE: mov d8, [[REG1]]
        call $callee-II
        
    )

    ;; CHECK-LABEL: Function[4] Body
    (func $targetHintAsCallerScratchReg32 (result i32)

        ;; X86_64_PASSIVE: mov ebp, 5
        ;; AARCH64_PASSIVE: mov w19, #5
        ;; TRICORE: mov d8, #5
        i32.const 5
        
        ;; X86_64_PASSIVE: mov  edi, dword ptr [rbx]
        ;; AARCH64_PASSIVE: ldr w8, 
        ;; TRICORE: ld.w  d9, [a2]
        i32.const 0
        i32.load
        ;; X86_64_PASSIVE: mov  [[REG1:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], dword ptr [rbx + 4]
        ;; AARCH64_PASSIVE: ldr [[REG:w[0-9]+]],
        ;; TRICORE: ld.w  [[REG1:d[0-9]+]], [a2]#4
        i32.const 4
        i32.load
        ;; X86_64_PASSIVE: edi, [[REG1]]
        ;; AARCH64_PASSIVE: add w8, w8, [[REG1]]
        ;; TRICORE: d9, [[REG1]]
        i32.add
        call $callee-II
    )

    ;; CHECK-LABEL: Function[5] Body
    (func $targetHintAsCallerScratchReg64Native (result i64)
        ;; X86_64_PASSIVE: mov edi, 7
        ;; AARCH64_PASSIVE: mov x0, #7
        ;; TRICORE: mov e4, #7
        i64.const 7
        ;; X86_64_PASSIVE: mov  rsi, qword ptr [rbx + 0x64]
        ;; AARCH64_PASSIVE: ldr x1,
        ;; TRICORE: ld.d  e6, [a2]#0x64
        i32.const 100
        i64.load
        call $api-II
    )
    ;; CHECK-LABEL: Function[6] Body
    (func $targetHintAsCallerLocal64Native (result i64)
        (local i32 i32 i32 i32)
        i64.const 7
        ;; X86_64_PASSIVE: mov  rsi, qword ptr [rbx + 0x64]
        ;; AARCH64_PASSIVE: ldr x1,
        ;; TRICORE: ld.d  e[[#REG_INDEX:]], [a2]#0x64
        i32.const 100
        ;; TRICORE: mov  d6, d[[#REG_INDEX]]
        ;; TRICORE: mov  d7, d[[#REG_INDEX+1]]
        i64.load
        call $api-II
    )

    (memory 1)
)