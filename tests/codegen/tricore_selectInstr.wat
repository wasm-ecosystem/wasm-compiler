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
)