(module
  ;; CHECK-LABEL: Function[0] Body
  (func $div_zero_check.i32_div_s.normal (param i32 i32) (result i32)
    (local.set 0 (i32.const 0x10))
    (local.set 1 (i32.const 0x11))
    ;; AARCH64:         mov  [[L0:w[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:w[0-9]+]], #0x11
    ;; TRICORE:         mov.u  [[L0:d[0-9]+]], #0x10
    ;; TRICORE:         mov.u  [[L1:d[0-9]+]], #0x11

    (i32.div_s (local.get 0) (local.get 1))
    ;; AARCH64:         cmp  [[L1]], #0
    ;; AARCH64-NEXT:    b.
    ;; AARCH64:         mov  [[TMP:w[0-9]+]], #-0x80000000
    ;; AARCH64-NEXT:    cmp  [[L0]], [[TMP]]
    ;; AARCH64-NEXT:    b.
    ;; AARCH64:         mov  [[TMP:w[0-9]+]], #-1
    ;; AARCH64-NEXT:    cmp  [[L1]], [[TMP]]
    ;; AARCH64-NEXT:    b.

    ;; TRICORE:         jne  [[L1]], #0,
    ;; TRICORE:         movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE:         jne  [[L0]], [[TMP]],
    ;; TRICORE:         jne  [[L1]], #-1,
  )
  ;; CHECK-LABEL: Function[1] Body
  (func $div_zero_check.i32_div_s.const_divisor (param i32 i32) (result i32)
    (local.set 0 (i32.const 0x10))
    (local.set 1 (i32.const 0x11))
    ;; AARCH64:         mov  [[L0:w[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:w[0-9]+]], #0x11
    ;; TRICORE:         mov.u  [[L0:d[0-9]+]], #0x10
    ;; TRICORE:         mov.u  [[L1:d[0-9]+]], #0x11
    (i32.div_s (local.get 0) (i32.const 10))
    ;; AARCH64-NOT:     cmp  w8, #0
    ;; AARCH64-NOT:     mov  [[TMP:w[0-9]+]], #-0x80000000
    ;; AARCH64-NOT:     mov  [[TMP:w[0-9]+]], #-1

    ;; TRICORE-NOT:     jeq  [[L1]], #0,
    ;; TRICORE-NOT:     jne  [[L1]], #0,
    ;; TRICORE-NOT:     movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE-NOT:     jne  [[REG:d[0-9]+]], #-1,
  )
  ;; CHECK-LABEL: Function[2] Body
  (func $div_zero_check.i32_div_s.const_divisor.0 (param i32 i32) (result i32)
    (i32.div_s (local.get 0) (i32.const 0))
    ;; AARCH64:         cmp  [[REG:w[0-9]+]], #0
    ;; AARCH64-NEXT:    b.
    ;; AARCH64-NOT:     mov  [[TMP:w[0-9]+]], #-0x80000000
    ;; AARCH64-NOT:     mov  [[TMP:w[0-9]+]], #-1

    ;; TRICORE:         mov  [[L1:d[0-9]+]], #0
    ;; TRICORE:         jeq  [[L1]], #0,
    ;; TRICORE-NOT:     movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE-NOT:     jne  [[REG:d[0-9]+]], #-1,
  )
  ;; CHECK-LABEL: Function[3] Body
  (func $div_zero_check.i32_div_s.const_dividend (param i32 i32) (result i32)
    (local.set 0 (i32.const 0x10))
    (local.set 1 (i32.const 0x11))
    ;; AARCH64:         mov  [[L0:w[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:w[0-9]+]], #0x11
    ;; TRICORE:         mov.u  [[L0:d[0-9]+]], #0x10
    ;; TRICORE:         mov.u  [[L1:d[0-9]+]], #0x11

    (i32.div_s (i32.const 0) (local.get 1))
    ;; AARCH64:         cmp [[L1]], #0
    ;; AARCH64-NEXT:    b.
    ;; AARCH64-NOT:     mov  [[TMP:w[0-9]+]], #-0x80000000
    ;; AARCH64-NOT:     mov  [[TMP:w[0-9]+]], #-1

    ;; TRICORE:         jeq  [[L1]], #0,
    ;; TRICORE-NOT:     movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE-NOT:     jne  [[L1]], #-1,
  )
  ;; CHECK-LABEL: Function[4] Body
  (func $div_zero_check.i32_div_s.const_dividend.0x80000000 (param i32 i32) (result i32)
    (local.set 0 (i32.const 0x10))
    (local.set 1 (i32.const 0x11))
    ;; AARCH64:         mov  [[L0:w[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:w[0-9]+]], #0x11
    ;; TRICORE:         mov.u  [[L0:d[0-9]+]], #0x10
    ;; TRICORE:         mov.u  [[L1:d[0-9]+]], #0x11

    (i32.div_s (i32.const 0x80000000) (local.get 1))
    ;; AARCH64:         mov  [[DIVIDEND:w[0-9]+]], #-0x80000000
    ;; AARCH64:         cmp [[L1]], #0
    ;; AARCH64-NEXT:    b.
    ;; AARCH64:         mov  [[TMP:w[0-9]+]], #-0x80000000
    ;; AARCH64-NEXT:    cmp  [[DIVIDEND]], [[TMP]]
    ;; AARCH64-NEXT:    b.
    ;; AARCH64:         mov  [[TMP:w[0-9]+]], #-1
    ;; AARCH64-NEXT:    cmp  [[L1]], [[TMP]]
    ;; AARCH64-NEXT:    b.

    ;; TRICORE:         jeq  [[L1]], #0,
    ;; TRICORE:         movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE:         jne  [[DIVIDEND:d[0-9]+]], [[TMP]],
    ;; TRICORE:         jne  [[L1]], #-1,
  )

  ;; CHECK-LABEL: Function[5] Body
  (func $div_zero_check.i32_rem_s.normal (param i32 i32) (result i32)
    (local.set 0 (i32.const 0x10))
    (local.set 1 (i32.const 0x11))
    ;; AARCH64:         mov  [[L0:w[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:w[0-9]+]], #0x11
    ;; TRICORE:         mov.u  [[L0:d[0-9]+]], #0x10
    ;; TRICORE:         mov.u  [[L1:d[0-9]+]], #0x11

    (i32.rem_s (local.get 0) (local.get 1))
    ;; AARCH64:         cmp  [[L1]], #0
    ;; AARCH64-NEXT:    b.
    ;; AARCH64:         mov  [[TMP:w[0-9]+]], #-0x80000000
    ;; AARCH64-NEXT:    cmp  [[L0]], [[TMP]]
    ;; AARCH64-NEXT:    b.
    ;; AARCH64:         mov  [[TMP:w[0-9]+]], #-1
    ;; AARCH64-NEXT:    cmp  [[L1]], [[TMP]]
    ;; AARCH64-NEXT:    b.

    ;; TRICORE:         jeq  [[L1]], #0,
    ;; TRICORE:         movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE:         jne  [[L0]], [[TMP]],
    ;; TRICORE:         jne  [[L1]], #-1,
  )
  ;; CHECK-LABEL: Function[6] Body
  (func $div_zero_check.i32_rem_s.const_divisor (param i32 i32) (result i32)
    (local.set 0 (i32.const 0x10))
    (local.set 1 (i32.const 0x11))
    ;; AARCH64:         mov  [[L0:w[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:w[0-9]+]], #0x11
    ;; TRICORE:         mov.u  [[L0:d[0-9]+]], #0x10
    ;; TRICORE:         mov.u  [[L1:d[0-9]+]], #0x11
    (i32.rem_s (local.get 0) (i32.const 10))
    ;; AARCH64-NOT:     cmp  w8, #0
    ;; AARCH64-NOT:     mov  [[TMP:w[0-9]+]], #-0x80000000
    ;; AARCH64-NOT:     mov  [[TMP:w[0-9]+]], #-1

    ;; TRICORE-NOT:     jeq  [[L1]], #0,
    ;; TRICORE-NOT:     movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE-NOT:     jne  [[L1]], #-1,
  )
  ;; CHECK-LABEL: Function[7] Body
  (func $div_zero_check.i32_rem_s.const_divisor.0 (param i32 i32) (result i32)
    (i32.rem_s (local.get 0) (i32.const 0))
    ;; AARCH64:         cmp  [[REG:w[0-9]+]], #0
    ;; AARCH64-NEXT:    b.
    ;; AARCH64-NOT:     mov  [[TMP:w[0-9]+]], #-0x80000000
    ;; AARCH64-NOT:     mov  [[TMP:w[0-9]+]], #-1

    ;; TRICORE:         jeq  [[REG:d[0-9]+]], #0,
    ;; TRICORE-NOT:     movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE-NOT:     jne  [[REG:d[0-9]+]], #-1,
  )
  ;; CHECK-LABEL: Function[8] Body
  (func $div_zero_check.i32_rem_s.const_dividend (param i32 i32) (result i32)
    (local.set 0 (i32.const 0x10))
    (local.set 1 (i32.const 0x11))
    ;; AARCH64:         mov  [[L0:w[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:w[0-9]+]], #0x11
    ;; TRICORE:         mov.u  [[L0:d[0-9]+]], #0x10
    ;; TRICORE:         mov.u  [[L1:d[0-9]+]], #0x11

    (i32.rem_s (i32.const 0) (local.get 1))
    ;; AARCH64:         cmp [[L1]], #0
    ;; AARCH64-NEXT:    b.
    ;; AARCH64-NOT:     mov  [[TMP:w[0-9]+]], #-0x80000000
    ;; AARCH64-NOT:     mov  [[TMP:w[0-9]+]], #-1

    ;; TRICORE:         jeq  [[L1]], #0,
    ;; TRICORE-NOT:     movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE-NOT:     jne  [[L1]], #-1,
  )
  ;; CHECK-LABEL: Function[9] Body
  (func $div_zero_check.i32_rem_s.const_dividend.0x80000000 (param i32 i32) (result i32)
    (local.set 0 (i32.const 0x10))
    (local.set 1 (i32.const 0x11))
    ;; AARCH64:         mov  [[L0:w[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:w[0-9]+]], #0x11
    ;; TRICORE:         mov.u  [[L0:d[0-9]+]], #0x10
    ;; TRICORE:         mov.u  [[L1:d[0-9]+]], #0x11

    (i32.rem_s (i32.const 0x80000000) (local.get 1))
    ;; AARCH64:         mov  [[DIVIDEND:w[0-9]+]], #-0x80000000
    ;; AARCH64:         cmp [[L1]], #0
    ;; AARCH64-NEXT:    b.
    ;; AARCH64:         mov  [[TMP:w[0-9]+]], #-0x80000000
    ;; AARCH64-NEXT:    cmp  [[DIVIDEND]], [[TMP]]
    ;; AARCH64-NEXT:    b.
    ;; AARCH64:         mov  [[TMP:w[0-9]+]], #-1
    ;; AARCH64-NEXT:    cmp  [[L1]], [[TMP]]
    ;; AARCH64-NEXT:    b.

    ;; TRICORE:         jeq  [[L1]], #0,
    ;; TRICORE:         movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE:         jne  [[DIVIDEND:d[0-9]+]], [[TMP]],
    ;; TRICORE:         jne  [[L1]], #-1,
  )

  ;; CHECK-LABEL: Function[10] Body
  (func $div_zero_check.i64_div_s.normal (param i64 i64) (result i64)
    (local.set 0 (i64.const 0x10))
    (local.set 1 (i64.const 0x11))
    ;; AARCH64:         mov  [[L0:x[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:x[0-9]+]], #0x11

    (i64.div_s (local.get 0) (local.get 1))
    ;; AARCH64:         cmp  [[L1]], #0
    ;; AARCH64-NEXT:    b.
    ;; AARCH64:         mov  [[TMP:x[0-9]+]], #-0x8000000000000000
    ;; AARCH64-NEXT:    cmp  [[L0]], [[TMP]]
    ;; AARCH64-NEXT:    b.
    ;; AARCH64:         mov  [[TMP:x[0-9]+]], #-1
    ;; AARCH64-NEXT:    cmp  [[L1]], [[TMP]]
    ;; AARCH64-NEXT:    b.

    ;; TRICORE:         jne  [[TMP_1:d[0-9]+]], #0,
    ;; TRICORE-NEXT:    jne  [[TMP_2:d[0-9]+]], #0,

    ;; TRICORE:         jne  [[DIVIDEND_L:d[0-9]+]], #0,
    ;; TRICORE-NEXT:    movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE-NEXT:    jne  [[DIVIDEND_H:d[0-9]+]], [[TMP]],
    ;; TRICORE-NEXT:    jne  [[TMP_1]], #-1,
    ;; TRICORE-NEXT:    jne  [[TMP_2]], #-1,
  )
  ;; CHECK-LABEL: Function[11] Body
  (func $div_zero_check.i64_div_s.const_divisor (param i64 i64) (result i64)
    (i64.div_s (local.get 0) (i64.const 10))
    ;; AARCH64-NOT:     cmp  x8, #0
    ;; AARCH64-NOT:     mov  [[TMP:x[0-9]+]], #-0x8000000000000000
    ;; AARCH64-NOT:     mov  [[TMP:x[0-9]+]], #-1

    ;; TRICORE-NOT:     jne  [[DIVISOR_1:d[0-9]+]], #0,
    ;; TRICORE-NOT:     jne  [[DIVISOR_2:d[0-9]+]], #0,

    ;; TRICORE-NOT:     movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE-NOT:     jne  [[DIVISOR_1:d[0-9]+]], #-1,
    ;; TRICORE-NOT:     jne  [[DIVISOR_2:d[0-9]+]], #-1,
  )
  ;; CHECK-LABEL: Function[12] Body
  (func $div_zero_check.i64_div_s.const_divisor.0 (param i64 i64) (result i64)
    (i64.div_s (local.get 0) (i64.const 0))
    ;; AARCH64:         cmp  [[REG:x[0-9]+]], #0
    ;; AARCH64-NEXT:    b.
    ;; AARCH64-NOT:     mov  [[TMP:x[0-9]+]], #-0x8000000000000000
    ;; AARCH64-NOT:     mov  [[TMP:x[0-9]+]], #-1

    ;; TRICORE:         jne  [[DIVISOR_1:d[0-9]+]], #0,
    ;; TRICORE-NEXT:    jne  [[DIVISOR_2:d[0-9]+]], #0,

    ;; TRICORE-NOT:     movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE-NOT:     jne  [[DIVISOR_1:d[0-9]+]], #-1,
    ;; TRICORE-NOT:     jne  [[DIVISOR_2:d[0-9]+]], #-1,
  )
  ;; CHECK-LABEL: Function[13] Body
  (func $div_zero_check.i64_div_s.const_dividend (param i64 i64) (result i64)
    (local.set 0 (i64.const 0x10))
    (local.set 1 (i64.const 0x11))
    ;; AARCH64:         mov  [[L0:x[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:x[0-9]+]], #0x11

    (i64.div_s (i64.const 0) (local.get 1))
    ;; AARCH64:         cmp [[L1]], #0
    ;; AARCH64-NEXT:    b.
    ;; AARCH64-NOT:     mov  [[TMP:x[0-9]+]], #-0x8000000000000000
    ;; AARCH64-NOT:     mov  [[TMP:x[0-9]+]], #-1

    ;; TRICORE:         jne  [[DIVISOR_1:d[0-9]+]], #0,
    ;; TRICORE-NEXT:    jne  [[DIVISOR_2:d[0-9]+]], #0,

    ;; TRICORE-NOT:     movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE-NOT:     jne  [[DIVISOR_1:d[0-9]+]], #-1,
    ;; TRICORE-NOT:     jne  [[DIVISOR_2:d[0-9]+]], #-1,
  )
  ;; CHECK-LABEL: Function[14] Body
  (func $div_zero_check.i64_div_s.const_dividend.0x8000000000000000 (param i64 i64) (result i64)
    (local.set 0 (i64.const 0x10))
    (local.set 1 (i64.const 0x11))
    ;; AARCH64:         mov  [[L0:x[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:x[0-9]+]], #0x11

    (i64.div_s (i64.const 0x8000000000000000) (local.get 1))
    ;; AARCH64:         mov  [[DIVIDEND:x[0-9]+]], #-0x8000000000000000
    ;; AARCH64:         cmp [[L1]], #0
    ;; AARCH64-NEXT:    b.
    ;; AARCH64:         mov  [[TMP:x[0-9]+]], #-0x8000000000000000
    ;; AARCH64-NEXT:    cmp  [[DIVIDEND]], [[TMP]]
    ;; AARCH64-NEXT:    b.
    ;; AARCH64:         mov  [[TMP:x[0-9]+]], #-1
    ;; AARCH64-NEXT:    cmp  [[L1]], [[TMP]]
    ;; AARCH64-NEXT:    b.

    ;; TRICORE:         jne  [[TMP_1:d[0-9]+]], #0,
    ;; TRICORE-NEXT:    jne  [[TMP_2:d[0-9]+]], #0,

    ;; TRICORE:         jne  [[DIVIDEND_L:d[0-9]+]], #0,
    ;; TRICORE-NEXT:    movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE-NEXT:    jne  [[DIVIDEND_H:d[0-9]+]], [[TMP]],
    ;; TRICORE-NEXT:    jne  [[TMP_1]], #-1,
    ;; TRICORE-NEXT:    jne  [[TMP_2]], #-1,
  )

  ;; CHECK-LABEL: Function[15] Body
  (func $div_zero_check.i64_rem_s.normal (param i64 i64) (result i64)
    (local.set 0 (i64.const 0x10))
    (local.set 1 (i64.const 0x11))
    ;; AARCH64:         mov  [[L0:x[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:x[0-9]+]], #0x11

    (i64.rem_s (local.get 0) (local.get 1))
    ;; AARCH64:         cmp  [[L1]], #0
    ;; AARCH64-NEXT:    b.
    ;; AARCH64:         mov  [[TMP:x[0-9]+]], #-0x8000000000000000
    ;; AARCH64-NEXT:    cmp  [[L0]], [[TMP]]
    ;; AARCH64-NEXT:    b.
    ;; AARCH64:         mov  [[TMP:x[0-9]+]], #-1
    ;; AARCH64-NEXT:    cmp  [[L1]], [[TMP]]
    ;; AARCH64-NEXT:    b.

    ;; TRICORE:         jne  [[TMP_1:d[0-9]+]], #0,
    ;; TRICORE-NEXT:    jne  [[TMP_2:d[0-9]+]], #0,

    ;; TRICORE:         jne  [[DIVIDEND_L:d[0-9]+]], #0,
    ;; TRICORE-NEXT:    movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE-NEXT:    jne  [[DIVIDEND_H:d[0-9]+]], [[TMP]],
    ;; TRICORE-NEXT:    jne  [[TMP_1]], #-1,
    ;; TRICORE-NEXT:    jne  [[TMP_2]], #-1,
  )
  ;; CHECK-LABEL: Function[16] Body
  (func $div_zero_check.i64_rem_s.const_divisor (param i64 i64) (result i64)
    (i64.rem_s (local.get 0) (i64.const 10))
    ;; AARCH64-NOT:     cmp  x8, #0
    ;; AARCH64-NOT:     mov  [[TMP:x[0-9]+]], #-0x8000000000000000
    ;; AARCH64-NOT:     mov  [[TMP:x[0-9]+]], #-1

    ;; TRICORE-NOT:     jne  [[DIVISOR_1:d[0-9]+]], #0,
    ;; TRICORE-NOT:     jne  [[DIVISOR_2:d[0-9]+]], #0,

    ;; TRICORE-NOT:     movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE-NOT:     jne  [[DIVISOR_1:d[0-9]+]], #-1,
    ;; TRICORE-NOT:     jne  [[DIVISOR_2:d[0-9]+]], #-1,
  )
  ;; CHECK-LABEL: Function[17] Body
  (func $div_zero_check.i64_rem_s.const_divisor.0 (param i64 i64) (result i64)
    (i64.rem_s (local.get 0) (i64.const 0))
    ;; AARCH64:         cmp  [[REG:x[0-9]+]], #0
    ;; AARCH64-NEXT:    b.
    ;; AARCH64-NOT:     mov  [[TMP:x[0-9]+]], #-0x8000000000000000
    ;; AARCH64-NOT:     mov  [[TMP:x[0-9]+]], #-1

    ;; TRICORE:         jne  [[DIVISOR_1:d[0-9]+]], #0,
    ;; TRICORE-NEXT:    jne  [[DIVISOR_2:d[0-9]+]], #0,

    ;; TRICORE-NOT:     movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE-NOT:     jne  [[DIVISOR_1:d[0-9]+]], #-1,
    ;; TRICORE-NOT:     jne  [[DIVISOR_2:d[0-9]+]], #-1,
  )
  ;; CHECK-LABEL: Function[18] Body
  (func $div_zero_check.i64_rem_s.const_dividend (param i64 i64) (result i64)
    (local.set 0 (i64.const 0x10))
    (local.set 1 (i64.const 0x11))
    ;; AARCH64:         mov  [[L0:x[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:x[0-9]+]], #0x11

    (i64.rem_s (i64.const 0) (local.get 1))
    ;; AARCH64:         cmp [[L1]], #0
    ;; AARCH64-NEXT:    b.
    ;; AARCH64-NOT:     mov  [[TMP:x[0-9]+]], #-0x8000000000000000
    ;; AARCH64-NOT:     mov  [[TMP:x[0-9]+]], #-1

    ;; TRICORE:         jne  [[DIVISOR_1:d[0-9]+]], #0,
    ;; TRICORE-NEXT:    jne  [[DIVISOR_2:d[0-9]+]], #0,

    ;; TRICORE-NOT:     movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE-NOT:     jne  [[DIVISOR_1:d[0-9]+]], #-1,
    ;; TRICORE-NOT:     jne  [[DIVISOR_2:d[0-9]+]], #-1,
  )
  ;; CHECK-LABEL: Function[19] Body
  (func $div_zero_check.i64_rem_s.const_dividend.0x8000000000000000 (param i64 i64) (result i64)
    (local.set 0 (i64.const 0x10))
    (local.set 1 (i64.const 0x11))
    ;; AARCH64:         mov  [[L0:x[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:x[0-9]+]], #0x11

    (i64.rem_s (i64.const 0x8000000000000000) (local.get 1))
    ;; AARCH64:         mov  [[DIVIDEND:x[0-9]+]], #-0x8000000000000000
    ;; AARCH64:         cmp [[L1]], #0
    ;; AARCH64-NEXT:    b.
    ;; AARCH64:         mov  [[TMP:x[0-9]+]], #-0x8000000000000000
    ;; AARCH64-NEXT:    cmp  [[DIVIDEND]], [[TMP]]
    ;; AARCH64-NEXT:    b.
    ;; AARCH64:         mov  [[TMP:x[0-9]+]], #-1
    ;; AARCH64-NEXT:    cmp  [[L1]], [[TMP]]
    ;; AARCH64-NEXT:    b.

    ;; TRICORE:         jne  [[TMP_1:d[0-9]+]], #0,
    ;; TRICORE-NEXT:    jne  [[TMP_2:d[0-9]+]], #0,

    ;; TRICORE:         jne  [[DIVIDEND_L:d[0-9]+]], #0,
    ;; TRICORE-NEXT:    movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE-NEXT:    jne  [[DIVIDEND_H:d[0-9]+]], [[TMP]],
    ;; TRICORE-NEXT:    jne  [[TMP_1]], #-1,
    ;; TRICORE-NEXT:    jne  [[TMP_2]], #-1,
  )

  ;; CHECK-LABEL: Function[20] Body
  (func $div_zero_check.i32_div_u.normal (param i32 i32) (result i32)
    (local.set 0 (i32.const 0x10))
    (local.set 1 (i32.const 0x11))
    ;; AARCH64:         mov  [[L0:w[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:w[0-9]+]], #0x11

    (i32.div_u (local.get 0) (local.get 1))
    ;; AARCH64:         cmp  [[L1]], #0
    ;; AARCH64-NEXT:    b.
  )
  ;; CHECK-LABEL: Function[21] Body
  (func $div_zero_check.i32_div_u.const_divisor (param i32 i32) (result i32)
    (i32.div_u (local.get 0) (i32.const 10))
    ;; AARCH64-NOT:     cmp  w8, #0
  )
  ;; CHECK-LABEL: Function[22] Body
  (func $div_zero_check.i32_div_u.const_divisor.0 (param i32 i32) (result i32)
    (i32.div_u (local.get 0) (i32.const 0))
    ;; AARCH64:         cmp  [[REG:w[0-9]+]], #0
    ;; AARCH64-NEXT:    b.
  )
  ;; CHECK-LABEL: Function[23] Body
  (func $div_zero_check.i32_div_u.const_dividend (param i32 i32) (result i32)
    (local.set 0 (i32.const 0x10))
    (local.set 1 (i32.const 0x11))
    ;; AARCH64:         mov  [[L0:w[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:w[0-9]+]], #0x11

    (i32.div_u (i32.const 0) (local.get 1))
    ;; AARCH64:         cmp [[L1]], #0
    ;; AARCH64-NEXT:    b.
  )
  ;; CHECK-LABEL: Function[24] Body
  (func $div_zero_check.i32_div_u.const_dividend.0x80000000 (param i32 i32) (result i32)
    (local.set 0 (i32.const 0x10))
    (local.set 1 (i32.const 0x11))
    ;; AARCH64:         mov  [[L0:w[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:w[0-9]+]], #0x11

    (i32.div_u (i32.const 0x80000000) (local.get 1))
    ;; AARCH64:         mov  [[DIVIDEND:w[0-9]+]], #-0x80000000
    ;; AARCH64:         cmp [[L1]], #0
    ;; AARCH64-NEXT:    b.
  )

  ;; CHECK-LABEL: Function[25] Body
  (func $div_zero_check.i32_rem_u.normal (param i32 i32) (result i32)
    (local.set 0 (i32.const 0x10))
    (local.set 1 (i32.const 0x11))
    ;; AARCH64:         mov  [[L0:w[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:w[0-9]+]], #0x11

    (i32.rem_u (local.get 0) (local.get 1))
    ;; AARCH64:         cmp  [[L1]], #0
    ;; AARCH64-NEXT:    b.
  )
  ;; CHECK-LABEL: Function[26] Body
  (func $div_zero_check.i32_rem_u.const_divisor (param i32 i32) (result i32)
    (i32.rem_u (local.get 0) (i32.const 10))
    ;; AARCH64-NOT:     cmp  w8, #0
  )
  ;; CHECK-LABEL: Function[27] Body
  (func $div_zero_check.i32_rem_u.const_divisor.0 (param i32 i32) (result i32)
    (i32.rem_u (local.get 0) (i32.const 0))
    ;; AARCH64:         cmp  [[REG:w[0-9]+]], #0
    ;; AARCH64-NEXT:    b.
  )
  ;; CHECK-LABEL: Function[28] Body
  (func $div_zero_check.i32_rem_u.const_dividend (param i32 i32) (result i32)
    (local.set 0 (i32.const 0x10))
    (local.set 1 (i32.const 0x11))
    ;; AARCH64:         mov  [[L0:w[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:w[0-9]+]], #0x11

    (i32.rem_u (i32.const 0) (local.get 1))
    ;; AARCH64:         cmp [[L1]], #0
    ;; AARCH64-NEXT:    b.
  )
  ;; CHECK-LABEL: Function[29] Body
  (func $div_zero_check.i32_rem_u.const_dividend.0x80000000 (param i32 i32) (result i32)
    (local.set 0 (i32.const 0x10))
    (local.set 1 (i32.const 0x11))
    ;; AARCH64:         mov  [[L0:w[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:w[0-9]+]], #0x11

    (i32.rem_u (i32.const 0x80000000) (local.get 1))
    ;; AARCH64:         mov  [[DIVIDEND:w[0-9]+]], #-0x80000000
    ;; AARCH64:         cmp [[L1]], #0
    ;; AARCH64-NEXT:    b.
  )

  ;; CHECK-LABEL: Function[30] Body
  (func $div_zero_check.i64_div_u.normal (param i64 i64) (result i64)
    (local.set 0 (i64.const 0x10))
    (local.set 1 (i64.const 0x11))
    ;; AARCH64:         mov  [[L0:x[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:x[0-9]+]], #0x11

    (i64.div_u (local.get 0) (local.get 1))
    ;; AARCH64:         cmp  [[L1]], #0
    ;; AARCH64-NEXT:    b.
  )
  ;; CHECK-LABEL: Function[31] Body
  (func $div_zero_check.i64_div_u.const_divisor (param i64 i64) (result i64)
    (i64.div_u (local.get 0) (i64.const 10))
    ;; AARCH64-NOT:     cmp  x8, #0
  )
  ;; CHECK-LABEL: Function[32] Body
  (func $div_zero_check.i64_div_u.const_divisor.0 (param i64 i64) (result i64)
    (i64.div_u (local.get 0) (i64.const 0))
    ;; AARCH64:         cmp  [[REG:x[0-9]+]], #0
    ;; AARCH64-NEXT:    b.
  )
  ;; CHECK-LABEL: Function[33] Body
  (func $div_zero_check.i64_div_u.const_dividend (param i64 i64) (result i64)
    (local.set 0 (i64.const 0x10))
    (local.set 1 (i64.const 0x11))
    ;; AARCH64:         mov  [[L0:x[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:x[0-9]+]], #0x11

    (i64.div_u (i64.const 0) (local.get 1))
    ;; AARCH64:         cmp [[L1]], #0
    ;; AARCH64-NEXT:    b.
  )
  ;; CHECK-LABEL: Function[34] Body
  (func $div_zero_check.i64_div_u.const_dividend.0x8000000000000000 (param i64 i64) (result i64)
    (local.set 0 (i64.const 0x10))
    (local.set 1 (i64.const 0x11))
    ;; AARCH64:         mov  [[L0:x[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:x[0-9]+]], #0x11

    (i64.div_u (i64.const 0x8000000000000000) (local.get 1))
    ;; AARCH64:         mov  [[DIVIDEND:x[0-9]+]], #-0x8000000000000000
    ;; AARCH64:         cmp [[L1]], #0
    ;; AARCH64-NEXT:    b.
  )

  ;; CHECK-LABEL: Function[35] Body
  (func $div_zero_check.i64_rem_u.normal (param i64 i64) (result i64)
    (local.set 0 (i64.const 0x10))
    (local.set 1 (i64.const 0x11))
    ;; AARCH64:         mov  [[L0:x[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:x[0-9]+]], #0x11

    (i64.rem_u (local.get 0) (local.get 1))
    ;; AARCH64:         cmp  [[L1]], #0
    ;; AARCH64-NEXT:    b.

    ;; TRICORE:         jne  [[TMP_1:d[0-9]+]], #0,
    ;; TRICORE-NEXT:    jne  [[TMP_2:d[0-9]+]], #0,

    ;; TRICORE:         jne  [[DIVIDEND_L:d[0-9]+]], #0,
    ;; TRICORE-NEXT:    movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE-NEXT:    jne  [[DIVIDEND_H:d[0-9]+]], [[TMP]],
    ;; TRICORE-NEXT:    jne  [[TMP_1]], #-1,
    ;; TRICORE-NEXT:    jne  [[TMP_2]], #-1,
  )
  ;; CHECK-LABEL: Function[36] Body
  (func $div_zero_check.i64_rem_u.const_divisor (param i64 i64) (result i64)
    (i64.rem_u (local.get 0) (i64.const 10))
    ;; AARCH64-NOT:     cmp  x8, #0

    ;; TRICORE-NOT:     jne  [[DIVISOR_1:d[0-9]+]], #0,
    ;; TRICORE-NOT:     jne  [[DIVISOR_2:d[0-9]+]], #0,

    ;; TRICORE-NOT:     movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE-NOT:     jne  [[DIVISOR_1:d[0-9]+]], #-1,
    ;; TRICORE-NOT:     jne  [[DIVISOR_2:d[0-9]+]], #-1,
  )
  ;; CHECK-LABEL: Function[37] Body
  (func $div_zero_check.i64_rem_u.const_divisor.0 (param i64 i64) (result i64)
    (i64.rem_u (local.get 0) (i64.const 0))
    ;; AARCH64:         cmp  [[REG:x[0-9]+]], #0
    ;; AARCH64-NEXT:    b.

    ;; TRICORE:         jne  [[DIVISOR_1:d[0-9]+]], #0,
    ;; TRICORE-NEXT:    jne  [[DIVISOR_2:d[0-9]+]], #0,

    ;; TRICORE-NOT:     movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE-NOT:     jne  [[DIVISOR_1:d[0-9]+]], #-1,
    ;; TRICORE-NOT:     jne  [[DIVISOR_2:d[0-9]+]], #-1,
  )
  ;; CHECK-LABEL: Function[38] Body
  (func $div_zero_check.i64_rem_u.const_dividend (param i64 i64) (result i64)
    (local.set 0 (i64.const 0x10))
    (local.set 1 (i64.const 0x11))
    ;; AARCH64:         mov  [[L0:x[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:x[0-9]+]], #0x11

    (i64.rem_u (i64.const 0) (local.get 1))
    ;; AARCH64:         cmp [[L1]], #0
    ;; AARCH64-NEXT:    b.

    ;; TRICORE:         jne  [[DIVISOR_1:d[0-9]+]], #0,
    ;; TRICORE-NEXT:    jne  [[DIVISOR_2:d[0-9]+]], #0,

    ;; TRICORE-NOT:     movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE-NOT:     jne  [[DIVISOR_1:d[0-9]+]], #-1,
    ;; TRICORE-NOT:     jne  [[DIVISOR_2:d[0-9]+]], #-1,
  )
  ;; CHECK-LABEL: Function[39] Body
  (func $div_zero_check.i64_rem_u.const_dividend.0x8000000000000000 (param i64 i64) (result i64)
    (local.set 0 (i64.const 0x10))
    (local.set 1 (i64.const 0x11))
    ;; AARCH64:         mov  [[L0:x[0-9]+]], #0x10
    ;; AARCH64:         mov  [[L1:x[0-9]+]], #0x11

    (i64.rem_u (i64.const 0x8000000000000000) (local.get 1))
    ;; AARCH64:         mov  [[DIVIDEND:x[0-9]+]], #-0x8000000000000000
    ;; AARCH64:         cmp [[L1]], #0
    ;; AARCH64-NEXT:    b.

    ;; TRICORE:         jne  [[TMP_1:d[0-9]+]], #0,
    ;; TRICORE-NEXT:    jne  [[TMP_2:d[0-9]+]], #0,

    ;; TRICORE:         jne  [[DIVIDEND_L:d[0-9]+]], #0,
    ;; TRICORE-NEXT:    movh  [[TMP:d[0-9]+]], #0x8000
    ;; TRICORE-NEXT:    jne  [[DIVIDEND_H:d[0-9]+]], [[TMP]],
    ;; TRICORE-NEXT:    jne  [[TMP_1]], #-1,
    ;; TRICORE-NEXT:    jne  [[TMP_2]], #-1,
  )

)

