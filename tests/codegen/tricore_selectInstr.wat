(module
  (memory 1)
  ;; CHECK-LABEL: Function[0] Body
  (func (result i32)
    i32.const 0x1
    i32.const 0x1
    i32.mul
    ;; TRICORE: mov  d2, #1
    ;; TRICORE-NEXT: mul  d2, d2
  )

  ;; CHECK-LABEL: Function[1] Body
  (func (result i32)
    i32.const 0x10
    i32.const 0x10
    i32.mul
    ;; TRICORE: mov.u d2, #0x10
    ;; TRICORE-NEXT: mul  d2, d2
  )

  ;; CHECK-LABEL: Function[2] Body
  (func (result i32)
    i32.const 0xff
    i32.const 0x1
    i32.mul
    ;; TRICORE: mov d2, #1
    ;; TRICORE-NEXT: mul  d2, d2, #0xff
  )

  ;; CHECK-LABEL: Function[3] Body
  (func (param i32) (result i32)
    local.get 0
    i32.const 0x1
    i32.mul
    ;; TRICORE: mul  d2, d8, #1
  )

  ;; CHECK-LABEL: Function[4] Body
  (func (param i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    local.get 7
    local.get 0
    i32.mul
    ;; TRICORE: ld.w  d2, [sp]#0xc8
    ;; TRICORE-NEXT: mul  d2, d8
  )

  ;; CHECK-LABEL: Function[5] Body
  (func (param i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    local.get 7
    i32.const 0x1
    i32.mul
    ;; TRICORE: ld.w  d2, [sp]#0xc8
    ;; TRICORE-NEXT: mul  d2, d2, #1
  )

  ;; CHECK-LABEL: Function[6] Body
  (func (param i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    local.get 7
    i32.const 0x100
    i32.mul
    ;; TRICORE: ld.w  d2, [sp]#0xc8
    ;; TRICORE-NEXT: mov.u  d13, #0x100
    ;; TRICORE-NEXT: mul  d2, d13
  )

  ;; CHECK-LABEL: Function[7] Body
  (func (param i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    local.get 0
    local.get 1
    i32.mul
    ;; TRICORE: mul d2, d8, d9
  )

  ;; CHECK-LABEL: Function[8] Body
  (func (param i32 i32 i32 i32 i32 i32 i32 i32)
    local.get 0
    local.get 1
    i32.mul
    ;; TRICORE: mul d13, d8, d9
    i32.load
    drop
  )

  ;; CHECK-LABEL: Function[9] Body
  (func (param i32 i32 i32 i32 i32 i32 i32 i32)
    i32.const 0xff
    local.get 7
    i32.mul
    ;; TRICORE: ld.w  d15, [sp]#0xc8
    ;; TRICORE-NEXT: mul d15, d15, #0xff
    i32.load
    drop
  )

  ;; CHECK-LABEL: Function[10] Body
  (func (param i32 i32 i32 i32 i32 i32 i32 i32)
    i32.const 0x100
    local.get 7
    i32.mul
    ;; TRICORE: ld.w  d15, [sp]#0xc8
    ;; TRICORE-NEXT: mov.u d13, #0x100
    ;; TRICORE-NEXT: mul d13, d15
    i32.load
    drop
  )

  ;; CHECK-LABEL: Function[11] Body
  (func (param i32 i32 i32 i32 i32 i32 i32 i32)
    local.get 7
    local.get 7
    i32.mul
    ;; TRICORE: ld.w  d15, [sp]#0xc8
    ;; TRICORE-NEXT: mul d15, d15
    i32.load
    drop
  )

  ;; CHECK-LABEL: Function[12] Body
  (func (param i32 i32 i32 i32 i32 i32 i32 i32)
    i32.const -1
    local.get 7
    i32.xor
    ;; TRICORE: ld.w  d15, [sp]#0xc8
    ;; TRICORE-NEXT: mov d13, #-1
    ;; TRICORE-NEXT: xor d13, d15
    i32.load
    drop
  )

  ;; CHECK-LABEL: Function[13] Body
  (func (param i32 i32 i32 i32 i32 i32 i32 i32)
    i32.const -1
    local.get 7
    i32.xor
    ;; TRICORE: ld.w  d15, [sp]#0xc8
    ;; TRICORE-NEXT: mov d13, #-1
    ;; TRICORE-NEXT: xor d13, d15
    i32.load
    drop
  )

  ;; CHECK-LABEL: Function[14] Body
  (func (param i32 i32 i32 i32 i32 i32 i32 i32)
    i32.const -1
    local.get 7
    i32.xor
    ;; TRICORE: ld.w  d15, [sp]#0xc8
    ;; TRICORE-NEXT: mov d13, #-1
    ;; TRICORE-NEXT: xor d13, d15
    i32.load
    drop
  )

  ;; CHECK-LABEL: Function[15] Body
  (func (param i32 i32 i32 i32 i32 i32 i32 i32)
    i32.const 0xff
    local.get 7
    i32.and
    ;; TRICORE: ld.w  d15, [sp]#0xc8
    ;; TRICORE-NEXT: and d15, #0xff
    i32.load
    drop
  )

  ;; CHECK-LABEL: Function[16] Body
  (func (param i32 i32 i32 i32 i32 i32 i32 i32)
    i32.const 0x200
    local.get 7
    i32.and
    ;; TRICORE: ld.w  d15, [sp]#0xc8
    ;; TRICORE-NEXT: mov.u d13, #0x200
    ;; TRICORE-NEXT: and d13, d15
    i32.load
    drop
  )

  ;; CHECK-LABEL: Function[17] Body
  (func $I64Sub (param i64) (result i64)
     
     local.get 0
     i64.const 200
    ;; TRICORE: addx  d2, [[REG:d[0-9]+]], #-[[IMM:(0x)?[0-9a-f]+]]
    ;; TRICORE-NEXT: addc  d3, [[REG:d[0-9]+]], #-[[IMM:(0x)?[0-9a-f]+]]
     i64.sub
    ;; TRICORE-NOT: mov 
    ;; TRICORE: fret
     return
  )

  ;; CHECK-LABEL: Function[18] Body
  (func $I64Mul (param i64) (result i64)
        local.get 0
        i64.const 1
        ;; TRICORE: mul.u  e2, [[REG:d[0-9]+]], [[REG:d[0-9]+]]
        i64.mul
        i64.const 2
        ;; TRICORE: mul.u  e6, [[REG:d[0-9]+]], [[REG:d[0-9]+]]
        i64.mul
        i64.const 3
        ;; TRICORE: mul.u  e2, [[REG:d[0-9]+]], [[REG:d[0-9]+]]
        i64.mul
        ;; TRICORE-NOT: mov 
        ;; TRICORE: fret
        return
  )

  ;; CHECK-LABEL: Function[19] Body
  (func  (result i64)
        i64.const 0
        ;; TRICORE: {{[0-9a-f][0-9a-f] [0-9a-f][0-9a-f]}} mov  e2, #0
        return
  )

  ;; CHECK-LABEL: Function[20] Body
  (func  (result i64)
        i64.const 0x1000
        ;; TRICORE: {{[0-9a-f][0-9a-f] [0-9a-f][0-9a-f] [0-9a-f][0-9a-f] [0-9a-f][0-9a-f]}} mov  e2, #0x1000
        return
  )

  ;; CHECK-LABEL: Function[21] Body
  (func  (result i64)
        i64.const 0x7ff8000000000000
        ;; TRICORE: imask e2, #0, #0x13, #0xc
        return
  )

  ;; CHECK-LABEL: Function[22] Body
  (func  (result i64)
        i64.const 0x0001000000010000
        ;; TRICORE: imask  e2, #1, #0x10, #1
        return
  )

  ;; CHECK-LABEL: Function[23] Body
  (func  (result i64)
        i64.const 0xF00000
        ;; TRICORE: imask e2, #0xf, #0x14, #0
        return
  )
)