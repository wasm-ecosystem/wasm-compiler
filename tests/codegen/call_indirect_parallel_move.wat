;; Indirect-call table index is kept in the dedicated register while the
;; parameter parallel-move resolver breaks the swapped-parameter cycle.
(module
  (type $callee (func (param i32 i32)))

  (func $target (type $callee))

  ;; CHECK-LABEL: Function[1] Body
  (func (param i32 i32)
    local.get 1
    local.get 0
    local.get 0
    ;; AARCH64:       mov  w0, w19
    ;; AARCH64-NEXT:  mov  [[TEMP:w26]], w8
    ;; AARCH64-NEXT:  mov  w8, w19
    ;; AARCH64-NEXT:  mov  w19, [[TEMP]]
    ;; AARCH64-NEXT:  cmp  w0, #1
    ;; TRICORE:       mov  d0, d8
    ;; TRICORE-NEXT:  mov  [[TEMP:d1]], d9
    ;; TRICORE-NEXT:  mov  d9, d8
    ;; TRICORE-NEXT:  mov  d8, [[TEMP]]
    ;; TRICORE-NEXT:  jlt.u  d0, #1,
    ;; X86_64:        mov  edx, ebp
    ;; X86_64-NEXT:   xchg  ebp, edi
    ;; X86_64-NEXT:   cmp  edx, 1
    call_indirect (type $callee))

  (table 1 funcref)
  (elem (i32.const 0) $target))