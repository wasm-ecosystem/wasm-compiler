(module
  ;; CHECK-LABEL: Function[0] Body
  (func $get-uninitialized-local (result i32)
    (local i32)
    local.get 0
    ;; AARCH64:      mov  w0, #0
  )
  ;; CHECK-LABEL: Function[1] Body
  (func $get-param (param i32) (result i32)
    local.get 0
    ;; AARCH64:      mov  w0, [[REG:w[0-9]+]]
  )
  
  ;; CHECK-LABEL: Function[2] Body
  (func $get-before-cfg (result i32)
    (local i32) (local i32)
    local.get 0
    block
    ;; AARCH64:      mov  [[REG:w[0-9]+]], #0
    end
    ;; AARCH64:      mov  w0, #0
  )
  ;; CHECK-LABEL: Function[3] Body
  (func $get-after-cfg (result i32)
    (local i32)
    block
    ;; AARCH64:      mov  [[REG:w[0-9]+]], #0
    end
    local.get 0
    ;; AARCH64:      mov  w0, [[REG]]
  )
  ;; CHECK-LABEL: Function[4] Body
  (func $get-in-cfg (result i32)
    (local i32)
    block (result i32)
      local.get 0
      ;; AARCH64:      mov  w0, #0
      ;; AARCH64:      mov  [[REG:w[0-9]+]], #0
    end
  )

  ;; CHECK-LABEL: Function[5] Body
  (func $set
    (local i32)
    ;; AARCH64-NOT:  mov  [[REG:w[0-9]+]], #0
    i32.const 0x222
    local.set 0
    ;; AARCH64:      mov  [[REG:w[0-9]+]], #0x222
  )

  ;; CHECK-LABEL: Function[6] Body
  (func $set-in-block
    (local i32)
    ;; AARCH64-NOT:  mov  [[REG:w[0-9]+]], #0
    block
      i32.const 0x222
      local.set 0
    end
    ;; AARCH64:      mov  [[REG:w[0-9]+]], #0x222
  )
  ;; CHECK-LABEL: Function[7] Body
  (func $set-after-block
    (local i32)
    block
    ;; AARCH64:      mov  [[REG:w[0-9]+]], #0
    end
    i32.const 0x222
    local.set 0
    ;; AARCH64:      mov  [[REG]], #0x222
  )

  ;; CHECK-LABEL: Function[8] Body
  (func $set-in-loop
    (local i32)
    ;; AARCH64:  mov      [[REG:w[0-9]+]], #0
    loop
      i32.const 0x222
      local.set 0
    end
    ;; AARCH64:      mov  [[REG]], #0x222
  )
  ;; CHECK-LABEL: Function[9] Body
  (func $set-after-loop
    (local i32)
    loop
    ;; AARCH64:      mov  [[REG:w[0-9]+]], #0
    end
    i32.const 0x222
    local.set 0
    ;; AARCH64:      mov  [[REG]], #0x222
  )

  ;; CHECK-LABEL: Function[10] Body
  (func $get-after-set (result i32)
    (local i32)
    i32.const 0x222
    ;; AARCH64:      mov  [[REG:w[0-9]+]], #0x222
    local.set 0
    local.get 0
    ;; AARCH64:      mov  w0, [[REG]]
    return
  )
)
