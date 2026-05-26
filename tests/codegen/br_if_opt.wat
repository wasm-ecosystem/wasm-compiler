(module
  (global $imported (import "test" "global_i32") i32)
  ;; CHECK-LABEL: Function[0] Body
  (func (param i32)
    block
      ;; X86_64: jmp
      ;; AARCH64: b
      ;; TRICORE: j
      i32.const 1
      br_if 0
      ;; This part should be optimized out because the br_if will always jump
      ;; X86_64-NOT: jne
      ;; X86_64-NOT: mov  ebp, 0x64
      ;; AARCH64-NOT: b.ne
      ;; AARCH64-NOT: mov  w19, #0x64
      ;; TRICORE-NOT: jnz.a
      ;; TRICORE-NOT: mov.u  d8, #0x64
      i32.const 100
      local.set 0
    end
    
    ;; X86_64: ret
    return)

  ;; CHECK-LABEL: Function[1] Body
  ;; Immutable import global is also regarded as a constant
  (func (param i32)
    block
      ;; X86_64: jmp
      ;; AARCH64: b
      ;; TRICORE: j
      global.get $imported
      i32.const 666
      i32.eq
      br_if 0
      ;; X86_64-NOT: jne
      ;; X86_64-NOT: mov  ebp, 0x64
      ;; AARCH64-NOT: b.ne
      ;; AARCH64-NOT: mov  w19, #0x64
      ;; TRICORE-NOT: jnz.a
      ;; TRICORE-NOT: mov.u  d8, #0x64
      i32.const 100
      local.set 0
    end
    
    ;; X86_64: ret
    return)

   ;; CHECK-LABEL: Function[2] Body
  (func (param i32)
    block

      i32.const 0
      br_if 0
      ;; The condition jump should be optimized out because the condition is always false
      ;; X86_64-NOT: jne
      ;; X86_64: mov  ebp, 0x64
      ;; AARCH64-NOT: b.ne
      ;; AARCH64: mov  w19, #0x64
      ;; TRICORE-NOT: jnz.a
      ;; TRICORE: mov.u  d8, #0x64
      i32.const 100
      local.set 0
    end
    
    ;; X86_64: ret
    return)
)