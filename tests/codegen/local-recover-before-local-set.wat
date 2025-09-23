;; This test case aim to ensure that local is recovered correctly before local.set/local.tee,
;; including case that i32.load immediately following local.set/local.tee.

(module
  (memory 1)
  (func $dummy)
  (func $load_set (result i32)
  ;; CHECK-LABEL: Function[1] Body
    (local i32)
    ;; before function call, locals should be stored in stack
    ;; AARCH64:  mov  [[LOCAL_REG:w[0-9]+]], #0x111
    i32.const 0x111
    local.set 0
    call $dummy
    ;; AARCH64:  bl 
    i32.const 0
    i32.load offset=0xa
    ;; AARCH64:  mov  w[[TMP_REG:[0-9]+]], #0
    ;; AARCH64:  add  x[[TMP_REG]], x[[TMP_REG]], #0xa
    local.tee 0
    ;; AARCH64:  ldr  [[LOCAL_REG]], [x29, x[[TMP_REG]]]
  )
)
