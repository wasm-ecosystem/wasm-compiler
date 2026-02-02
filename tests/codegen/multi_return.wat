(module
   ;; CHECK-LABEL: Function[0] Body
   (func $return-ii (param i32) (result i32 i32)
        ;; X86_64: add eax, 5
        ;; AARCH64: add  w0, [[REG:w[0-9]+]], #5
        ;;TRICORE: addi  d2, [[REG:d[0-9]+]], #5
        local.get 0
        i32.const 5
        i32.add
        ;; X86_64: add ecx, 7
        ;; AARCH64: add  w26, [[REG]], #7
        ;;TRICORE: addi  d3, [[REG]], #7
        local.get 0
        i32.const 7
        i32.add
   )

)