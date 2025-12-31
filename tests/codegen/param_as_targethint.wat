(module
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
    (func $targetHintAsCallerScratchReg (result i32)

        ;; X86_64_PASSIVE: mov ebp, 5
        i32.const 5
        
        ;; X86_64_PASSIVE: mov  edi, dword ptr [rbx]
        i32.const 0
        i32.load
        ;; X86_64_PASSIVE: mov  [[REG1:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], dword ptr [rbx + 4]
        i32.const 4
        i32.load
         ;; X86_64_PASSIVE: edi, [[REG1]]
        i32.add
        call $callee-II
    )

    (memory 1)
)