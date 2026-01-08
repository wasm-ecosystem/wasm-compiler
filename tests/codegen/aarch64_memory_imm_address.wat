(module
  (memory 1 100)
  
  ;; CHECK-LABEL: Function[0] Body
  (func $i32-load (result i32)
    i32.const 100
    ;; AARCH64: ldr  w0, [x29, #0x64]
    i32.load
  )

  ;; CHECK-LABEL: Function[1] Body
  (func $i64-load (result i64)
    i32.const 8000
    ;; AARCH64_PASSIVE: ldr  x0, [x29, #0x1f40]
    i64.load
  )

  ;; CHECK-LABEL: Function[2] Body
  (func $f32-load (result f32)
    i32.const 100
    ;; AARCH64: ldr  s0, [x29, #0x64]
    f32.load
  )

  ;; CHECK-LABEL: Function[3] Body
  (func $f64-load (result f64)
    i32.const 8000
    ;; AARCH64_PASSIVE: ldr  d0, [x29, #0x1f40]
    f64.load
  )

  ;; CHECK-LABEL: Function[4] Body
  (func $i32-load8-s (result i32)
    i32.const 100
    ;; AARCH64: ldrsb  w0, [x29, #0x64]
    i32.load8_s
  )

  ;; CHECK-LABEL: Function[5] Body
  (func $i32-load8-u (result i32)
    i32.const 100
    ;; AARCH64: ldrb  w0, [x29, #0x64]
    i32.load8_u
  )

  ;; CHECK-LABEL: Function[6] Body
  (func $i32-load16-s (result i32)
    i32.const 100
    ;; AARCH64: ldrsh  w0, [x29, #0x64]
    i32.load16_s
  )

  ;; CHECK-LABEL: Function[7] Body
  (func $i32-load16-u (result i32)
    i32.const 100
    ;; AARCH64: ldrh  w0, [x29, #0x64]
    i32.load16_u
  )

  ;; CHECK-LABEL: Function[8] Body
  (func $i64-load8-s (result i64)
    i32.const 100
    ;; AARCH64: ldrsb  x0, [x29, #0x64]
    i64.load8_s
  )

  ;; CHECK-LABEL: Function[9] Body
  (func $i64-load8-u (result i64)
    i32.const 100
    ;; AARCH64: ldrb  w0, [x29, #0x64]
    i64.load8_u
  )

  ;; CHECK-LABEL: Function[10] Body
  (func $i64-load16-s (result i64)
    i32.const 100
    ;; AARCH64: ldrsh  x0, [x29, #0x64]
    i64.load16_s
  )

  ;; CHECK-LABEL: Function[11] Body
  (func $i64-load16-u (result i64)
    i32.const 100
    ;; AARCH64: ldrh  w0, [x29, #0x64]
    i64.load16_u
  )

  ;; CHECK-LABEL: Function[12] Body
  (func $i64-load32-s (result i64)
    i32.const 100
    ;; AARCH64: ldrsw  x0, [x29, #0x64]
    i64.load32_s
  )

  ;; CHECK-LABEL: Function[13] Body
  (func $i64-load32-u (result i64)
    i32.const 100
    ;; AARCH64: ldr  w0, [x29, #0x64]
    i64.load32_u
  )

  ;; CHECK-LABEL: Function[14] Body
  (func $i32-store
    i32.const 200
    i32.const 42
    ;; AARCH64: str  [[REG:w[0-9]+]], [x29, #0xc8]
    i32.store
  )

  ;; CHECK-LABEL: Function[15] Body
  (func $i64-store
    i32.const 8000
    i64.const 42
    ;; AARCH64_PASSIVE: str  [[REG:x[0-9]+]], [x29, #0x1f40]
    i64.store
  )

  ;; CHECK-LABEL: Function[16] Body
  (func $f32-store
    i32.const 200
    f32.const 42.0
    ;; AARCH64: str  [[REG:[sdv][0-9]+]], [x29, #0xc8]
    f32.store
  )

  ;; CHECK-LABEL: Function[17] Body
  (func $f64-store
    i32.const 8000
    f64.const 42.0
    ;; AARCH64_PASSIVE: str  [[REG:[sdv][0-9]+]], [x29, #0x1f40]
    f64.store
  )

  ;; CHECK-LABEL: Function[18] Body
  (func $i32-store8
    i32.const 300
    i32.const 42
    ;; AARCH64: strb  [[REG:w[0-9]+]], [x29, #0x12c]
    i32.store8
  )

  ;; CHECK-LABEL: Function[19] Body
  (func $i32-store16
    i32.const 400
    i32.const 42
    ;; AARCH64: strh  [[REG:w[0-9]+]], [x29, #0x190]
    i32.store16
  )

  ;; CHECK-LABEL: Function[20] Body
  (func $i64-store8
    i32.const 500
    i64.const 42
    ;; AARCH64: strb  [[REG:w[0-9]+]], [x29, #0x1f4]
    i64.store8
  )

  ;; CHECK-LABEL: Function[21] Body
  (func $i64-store16
    i32.const 600
    i64.const 42
    ;; AARCH64: strh  [[REG:w[0-9]+]], [x29, #0x258]
    i64.store16
  )

  ;; CHECK-LABEL: Function[22] Body
  (func $i64-store32
    i32.const 700
    i64.const 42
    ;; AARCH64: str  [[REG:w[0-9]+]], [x29, #0x2bc]
    i64.store32
  )

  ;; CHECK-LABEL: Function[23] Body
  (func $i64-load-imm12 (result i64)
    i32.const 4088
    ;; AARCH64: ldr  x0, [x29, #0xff8]
    i64.load
  )

  ;; CHECK-LABEL: Function[24] Body
  (func $f64-load-imm12 (result f64)
    i32.const 4088
    ;; AARCH64: ldr  d0, [x29, #0xff8]
    f64.load
  )

  ;; CHECK-LABEL: Function[25] Body
  (func $i64-store-imm12
    i32.const 4088
    i64.const 42
    ;; AARCH64: str  [[REG:x[0-9]+]], [x29, #0xff8]
    i64.store
  )

  ;; CHECK-LABEL: Function[26] Body
  (func $f64-store-imm12
    i32.const 4088
    f64.const 42.0
    ;; AARCH64: str  [[REG:[sdv][0-9]+]], [x29, #0xff8]
    f64.store
  )
)