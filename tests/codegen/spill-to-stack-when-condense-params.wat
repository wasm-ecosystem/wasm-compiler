(module
    ;; CHECK-LABEL: Function[0] Body
    (func $goo (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
        local.get 8
    )
    ;; CHECK-LABEL: Function[1] Body
    (func $foo (result i32)
        (local $l0 i32) (local $l1 i32) (local $l2 i32) (local $l3 i32) (local $l4 i32) (local $l5 i32) (local $l6 i32) (local $l7 i32) (local $l8 i32) (local $l9 i32)
        (local i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
        i32.const 0
        local.set $l0
        i32.const 1
        local.set $l1
        i32.const 2
        local.set 2
        i32.const 3
        local.set $l3
        i32.const 4
        local.set $l4
        i32.const 5
        local.set $l5
        i32.const 6
        local.set $l6
        i32.const 7
        local.set $l7
        i32.const 8
        local.set $l8
        i32.const 9
        local.set $l9
        i32.const 0
        local.set 10
        i32.const 0
        local.set 11
        i32.const 0
        local.set 12
        i32.const 0
        local.set 13
        i32.const 0
        local.set 14
        i32.const 0
        local.set 15
        i32.const 0
        local.set 16
        i32.const 0
        local.set 17
        i32.const 0
        local.set 18
        i32.const 0
        local.set 19

        block (result i32)
        i32.const 100
        i32.load
        ;; X86_64_PASSIVE:  lea rsp, [rsp + 0x[[OFFSET_ADJUST:[0-9a-f]+]]]
        ;; AARCH64_PASSIVE: add sp, sp, #0x[[OFFSET_ADJUST:[0-9a-f]+]]
        ;; TRICORE: lea sp, [sp]#0x[[OFFSET_ADJUST:[0-9a-f]+]]
        ;;--------params
        local.get 0
        local.get 1
        local.get 2
        local.get 3
        local.get 4
        local.get 5
        local.get 6
        local.get 7
        

        

        local.get 1
        local.get 1
        i32.add

        local.get 1
        local.get 1
        i32.add

        
        local.get 1
        local.get 1
        i32.add

        
        local.get 1
        local.get 1
        i32.add

       

        local.get 1
        local.get 1
        i32.add

        

        local.get 1
        local.get 1
        i32.add

        local.get 1
        local.get 1
        i32.add

        i32.add

        i32.add

        i32.add

        i32.add

        i32.add
        i32.add
        ;; ---------- space used for temp results on stack ------------
        ;; X86_64_PASSIVE:  lea rsp, [rsp - 0x[[NUM:[0-9a-f]+]]]
        ;; AARCH64_PASSIVE: sub sp, sp, #0x[[NUM:[0-9a-f]+]]
        ;; TRICORE: sub.a  sp, #0x[[NUM:[0-9a-f]+]]

        i32.const 1
        local.get 2
        ;; X86_64_PASSIVE: add [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], 1
        ;; AARCH64_PASSIVE: add  [[REG:w[0-9]+]], [[REG2:w[0-9]+]], #1
        ;; TRICORE: addi  [[REG:d[0-9]+]], [[REG2:d[0-9]+]], #1
        i32.add

        i32.const 1
        ;;------------------scratch reg should be reused when condense this vb-------------------
        ;; X86_64_PASSIVE: add [[REG]], 1
        ;; AARCH64_PASSIVE: add  [[REG]], [[REG2]], #1
        ;; TRICORE: addi  [[REG]], [[REG2]], #1
        local.get 2
        i32.add
        ;;------------------ mov scratch reg to callee param on stack -------------------
        ;; X86_64_PASSIVE: mov  dword ptr [rsp + 0x[[NUM]]], [[REG]]
        ;; AARCH64_PASSIVE: str  [[REG]], [sp, #0x[[NUM]]]
        ;; TRICORE: st.w  [sp]#0x[[NUM]], [[REG]]
        
        ;;------------------ adjust stack back to param pos and call -------------------
        ;; X86_64_PASSIVE:  lea rsp, [rsp + 0x[[NUM]]]
        ;; X86_64_PASSIVE:  call
        ;; AARCH64_PASSIVE: add sp, sp, #0x[[NUM]]
        ;; AARCH64_PASSIVE: bl
        ;; TRICORE: lea sp, [sp]#0x[[NUM]]
        ;; TRICORE: fcall
        call $goo

        i32.add

        end

        
    )
    (memory 1)
 

    
)