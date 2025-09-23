(module
  ;; CHECK-LABEL: Function[0] Body
  (func $br_table/0 (param i32)
    block
      block
        block
          block
            i32.const 0x2222
            br_table 0 2 1 3 3
          end
        end
      end
    end
    ;; AARCH64:      mov  [[INDEX_REG:w[0-9]+]], #0x2222
    ;; AARCH64-NEXT: mov  [[MAX_TABLE_REG:w[0-9]+]], #4
    ;; AARCH64-NEXT: cmp  [[INDEX_REG]], [[MAX_TABLE_REG]]
    ;; AARCH64-NEXT: csel [[INDEX_REG]], [[INDEX_REG]], [[MAX_TABLE_REG]], lo
  )
)
