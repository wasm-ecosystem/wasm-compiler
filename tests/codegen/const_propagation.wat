(module
  (global $imported (import "test" "global_i32") i32)
  (global $a i32 (i32.const 2))
  ;; CHECK-LABEL: Function[0] Body
  (func $immutable-global (param i32)  (result i32)
    global.get $a
    i32.const 2
    i32.ne
    ;;X86_64-NOT: add  [[REG:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], 5
    ;;AARCH64-NOT: add  [[REG:w[0-9]+]], [[REG:w[0-9]+]], #5
    ;;TRICORE-NOT: addi  [[REG:d[0-9]+]], [[REG:d[0-9]+]], #5
    if
      local.get 0
      i32.const 5
      i32.add
      return
    end
    ;;X86_64:  mov  eax, 0xa
    i32.const 10
  )

  ;; CHECK-LABEL: Function[1] Body
  (func $i32-eq (param i32)  (result i32)
    i32.const 1
    i32.const 1
    i32.eq
    ;;X86_64:  mov  eax, 1
    ;;AARCH64: mov  w0, #1
    ;;TRICORE: mov  d2, #1
    return
  )

  ;; CHECK-LABEL: Function[2] Body
  (func $i32-ne (result i32)
    i32.const 5
    i32.const 3
    i32.ne
    ;;X86_64:  mov  eax, 1
    ;;AARCH64: mov  w0, #1
    ;;TRICORE: mov  d2, #1
    return
  )

  ;; CHECK-LABEL: Function[3] Body
  (func $i32-lt-s (result i32)
    i32.const -5
    i32.const 3
    i32.lt_s
    ;;X86_64:  mov  eax, 1
    ;;AARCH64: mov  w0, #1
    ;;TRICORE: mov  d2, #1
    return
  )

  ;; CHECK-LABEL: Function[4] Body
  (func $i32-lt-u (result i32)
    i32.const 2
    i32.const 10
    i32.lt_u
    ;;X86_64:  mov  eax, 1
    ;;AARCH64: mov  w0, #1
    ;;TRICORE: mov  d2, #1
    return
  )

  ;; CHECK-LABEL: Function[5] Body
  (func $i32-gt-s (result i32)
    i32.const 10
    i32.const -3
    i32.gt_s
    ;;X86_64:  mov  eax, 1
    ;;AARCH64: mov  w0, #1
    ;;TRICORE: mov  d2, #1
    return
  )

  ;; CHECK-LABEL: Function[6] Body
  (func $i32-gt-u (result i32)
    i32.const 20
    i32.const 10
    i32.gt_u
    ;;X86_64:  mov  eax, 1
    ;;AARCH64: mov  w0, #1
    ;;TRICORE: mov  d2, #1
    return
  )

  ;; CHECK-LABEL: Function[7] Body
  (func $i32-le-s (result i32)
    i32.const 5
    i32.const 5
    i32.le_s
    ;;X86_64:  mov  eax, 1
    ;;AARCH64: mov  w0, #1
    ;;TRICORE: mov  d2, #1
    return
  )

  ;; CHECK-LABEL: Function[8] Body
  (func $i32-le-u (result i32)
    i32.const 3
    i32.const 10
    i32.le_u
    ;;X86_64:  mov  eax, 1
    ;;AARCH64: mov  w0, #1
    ;;TRICORE: mov  d2, #1
    return
  )

  ;; CHECK-LABEL: Function[9] Body
  (func $i32-ge-s (result i32)
    i32.const 5
    i32.const 5
    i32.ge_s
    ;;X86_64:  mov  eax, 1
    ;;AARCH64: mov  w0, #1
    ;;TRICORE: mov  d2, #1
    return
  )

  ;; CHECK-LABEL: Function[10] Body
  (func $i32-ge-u (result i32)
    i32.const 10
    i32.const 5
    i32.ge_u
    ;;X86_64:  mov  eax, 1
    ;;AARCH64: mov  w0, #1
    ;;TRICORE: mov  d2, #1
    return
  )

  ;; CHECK-LABEL: Function[11] Body
  (func $i32-eqz-true (result i32)
    i32.const 0
    i32.eqz
    ;;X86_64:  mov  eax, 1
    ;;AARCH64: mov  w0, #1
    ;;TRICORE: mov  d2, #1
    return
  )

  ;; CHECK-LABEL: Function[12] Body
  (func $i32-eqz-false (result i32)
    i32.const 42
    i32.eqz
    ;;X86_64:  mov  eax, 0
    ;;AARCH64: mov  w0, #0
    ;;TRICORE: mov  d2, #0
    return
  )

  ;; CHECK-LABEL: Function[13] Body
  (func $i64-eq (result i32)
    i64.const 100
    i64.const 100
    i64.eq
    ;;X86_64:  mov  eax, 1
    ;;AARCH64: mov  w0, #1
    ;;TRICORE: mov  d2, #1
    return
  )

  ;; CHECK-LABEL: Function[14] Body
  (func $i64-ne (result i32)
    i64.const 100
    i64.const 200
    i64.ne
    ;;X86_64:  mov  eax, 1
    ;;AARCH64: mov  w0, #1
    ;;TRICORE: mov  d2, #1
    return
  )

  ;; CHECK-LABEL: Function[15] Body
  (func $i64-lt-s (result i32)
    i64.const -100
    i64.const 50
    i64.lt_s
    ;;X86_64:  mov  eax, 1
    ;;AARCH64: mov  w0, #1
    ;;TRICORE: mov  d2, #1
    return
  )

  ;; CHECK-LABEL: Function[16] Body
  (func $i64-lt-u (result i32)
    i64.const 10
    i64.const 100
    i64.lt_u
    ;;X86_64:  mov  eax, 1
    ;;AARCH64: mov  w0, #1
    ;;TRICORE: mov  d2, #1
    return
  )

  ;; CHECK-LABEL: Function[17] Body
  (func $i64-gt-s (result i32)
    i64.const 100
    i64.const -50
    i64.gt_s
    ;;X86_64:  mov  eax, 1
    ;;AARCH64: mov  w0, #1
    ;;TRICORE: mov  d2, #1
    return
  )

  ;; CHECK-LABEL: Function[18] Body
  (func $i64-gt-u (result i32)
    i64.const 200
    i64.const 100
    i64.gt_u
    ;;X86_64:  mov  eax, 1
    ;;AARCH64: mov  w0, #1
    ;;TRICORE: mov  d2, #1
    return
  )

  ;; CHECK-LABEL: Function[19] Body
  (func $i64-le-s (result i32)
    i64.const 50
    i64.const 50
    i64.le_s
    ;;X86_64:  mov  eax, 1
    ;;AARCH64: mov  w0, #1
    ;;TRICORE: mov  d2, #1
    return
  )

  ;; CHECK-LABEL: Function[20] Body
  (func $i64-le-u (result i32)
    i64.const 50
    i64.const 100
    i64.le_u
    ;;X86_64:  mov  eax, 1
    ;;AARCH64: mov  w0, #1
    ;;TRICORE: mov  d2, #1
    return
  )

  ;; CHECK-LABEL: Function[21] Body
  (func $i64-ge-s (result i32)
    i64.const 100
    i64.const 100
    i64.ge_s
    ;;X86_64:  mov  eax, 1
    ;;AARCH64: mov  w0, #1
    ;;TRICORE: mov  d2, #1
    return
  )

  ;; CHECK-LABEL: Function[22] Body
  (func $i64-ge-u (result i32)
    i64.const 100
    i64.const 50
    i64.ge_u
    ;;X86_64:  mov  eax, 1
    ;;AARCH64: mov  w0, #1
    ;;TRICORE: mov  d2, #1
    return
  )

  ;; CHECK-LABEL: Function[23] Body
  (func $i64-eqz-true (result i32)
    i64.const 0
    i64.eqz
    ;;X86_64:  mov  eax, 1
    ;;AARCH64: mov  w0, #1
    ;;TRICORE: mov  d2, #1
    return
  )

  ;; CHECK-LABEL: Function[24] Body
  (func $i64-eqz-false (result i32)
    i64.const 999
    i64.eqz
    ;;X86_64:  mov  eax, 0
    ;;AARCH64: mov  w0, #0
    ;;TRICORE: mov  d2, #0
    return
  )

  ;; Test cases that evaluate to 0
  ;; CHECK-LABEL: Function[25] Body
  (func $i32-eq-false (result i32)
    i32.const 1
    i32.const 2
    i32.eq
    ;;X86_64:  mov  eax, 0
    ;;AARCH64: mov  w0, #0
    ;;TRICORE: mov  d2, #0
    return
  )

  ;; CHECK-LABEL: Function[26] Body
  (func $i64-eq-false (result i32)
    i64.const 100
    i64.const 200
    i64.eq
    ;;X86_64:  mov  eax, 0
    ;;AARCH64: mov  w0, #0
    ;;TRICORE: mov  d2, #0
    return
  )

  ;; CHECK-LABEL: Function[27] Body
  (func $import-global-true (result i32)
    global.get $imported
    i32.const 666
    i32.eq
    if (result i32)
      ;;X86_64:  mov  eax, 1
      ;;AARCH64: mov  w0, #1
      ;;TRICORE: mov  d2, #1
      i32.const 1
    else
      i32.const 2
    end
  )

  ;; CHECK-LABEL: Function[28] Body
  (func $import-global-false (result i32)
    global.get $imported
    i32.const 1
    i32.eq
    if 
      ;;X86_64-NOT: call 0x[[#%x,FUNC_ADDR:]]
      ;;AARCH64-NOT: bl 0x[[#%x,FUNC_ADDR:]]
      call $i64-eq-false
      drop
    end
    i32.const 5
  )
)
