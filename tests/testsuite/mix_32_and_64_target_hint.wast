(module

  (func $mem-to-mem (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i64 i64) (result i64)
    local.get 9

    ;; for i32.wrap_i64, it's syntax semantic should be i64 -> i32.
    ;; here if we use wasmType::i32, and the source location is in memory,
    ;; then here emit instructions like:  movss xmm15, dword ptr [x],  movss dword ptr[x + 8], xmm15
    i32.wrap_i64
    i64.extend_i32_u
    local.tee 10   ;; targetHint = local6 with wasmType::i64
  )
  (func $reg-to-reg  (result i64)
    (local i64)
    i64.const 0xFFFFFFFFFF
    i64.const 0
    i64.add
    i32.wrap_i64
    i64.extend_i32_u
    local.tee 0
  )

  (func $reg-to-mem  (result i64)
    (local i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64)
    (local $l1 i64)
    i64.const 0xFFFFFFFFFF
    local.tee 0
    local.tee $l1
    i32.wrap_i64
    i64.extend_i32_u
    local.tee $l1
  )

  (func $const-to-mem  (result i64)
    (local i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64 i64)
    (local $l1 i64)
    i64.const 0xFFFFFFFFFF
    local.set $l1
    i64.const 0xFFFFFFFFFF
    i32.wrap_i64
    i64.extend_i32_u
    local.tee $l1
  )

  (export "reg-to-reg" (func $reg-to-reg))

  (export "mem-to-mem" (func $mem-to-mem))
  (export "reg-to-mem" (func $reg-to-mem))
  (export "const-to-mem" (func $const-to-mem))
)

(assert_return (invoke "mem-to-mem" (i32.const 0) (i32.const 0) (i32.const 0) (i32.const 0) (i32.const 0) (i32.const 0) (i32.const 0) (i32.const 0) (i32.const 0) (i64.const 0xAABBCCDDEE) (i64.const 0x1122334455667788)) (i64.const 0xBBCCDDEE))
(assert_return (invoke "mem-to-mem" (i32.const 0) (i32.const 0) (i32.const 0) (i32.const 0) (i32.const 0) (i32.const 0) (i32.const 0) (i32.const 0) (i32.const 0) (i64.const 0xAABBCCDD) (i64.const 0x1122334455667788)) (i64.const 0xAABBCCDD))
(assert_return (invoke "reg-to-reg") (i64.const 0xFFFFFFFF))
(assert_return (invoke "reg-to-mem") (i64.const 0xFFFFFFFF))
(assert_return (invoke "const-to-mem") (i64.const 0xFFFFFFFF))

(module
  (func $load (result i64)
    (local $l i32)
    
    f32.const 2
    
    f32.const 1
    
    f32.lt

    i64.load32_u
    
    return
    )
    (memory 1)
    (data (i32.const 0) "\44\33\22\11")

    (export "load" (func $load))
  )
(assert_return (invoke "load") (i64.const 0x11223344))

(module
  (func $const-to-global (result i64)
      f32.const 1
      f32.const 2
      
      f32.eq
      i64.extend_i32_u
      global.set 0
      
      global.get 0
    )

  (global (;0;) (mut i64) (i64.const -70368744177664))
  (export "const-to-global" (func $const-to-global))
)
(assert_return (invoke "const-to-global") (i64.const 0))