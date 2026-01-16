(module
  ;; Test case 1: const4sx - value fits in signed 4-bit (-8 to 7)
  (func (export "imm64_const4sx_pos") (result i64)
    i64.const 7  ;; max positive 4-bit signed
  )
  (func (export "imm64_const4sx_neg") (result i64)
    i64.const -8  ;; min negative 4-bit signed
  )
  (func (export "imm64_const4sx_one") (result i64)
    i64.const 1
  )

  ;; Test case 2: const16sx - value fits in signed 16-bit (-32768 to 32767)
  (func (export "imm64_const16sx_pos") (result i64)
    i64.const 32767  ;; max positive 16-bit signed
  )
  (func (export "imm64_const16sx_neg") (result i64)
    i64.const -32768  ;; min negative 16-bit signed
  )
  (func (export "imm64_const16sx_mid") (result i64)
    i64.const 1000
  )

  ;; Test case 3: IMASK with higher32 continuous 1s (e.g., 0xFFFFFFFF_0000000F)
  ;; higher32=0xFFFFFFFF (all 1s), lower bits must align
  (func (export "imm64_imask_higher_continuous") (result i64)
    i64.const 0xFFFFFFFF0000000F  ;; higher32 all 1s, lower aligns at pos 0
  )
  (func (export "imm64_imask_higher_partial") (result i64)
    i64.const 0xFFFF000000000000  ;; higher32=0xFFFF0000, pos=16, width=16
  )

  ;; Test case 4: IMASK with only lower32 (higher32 == 0), continuous 1s width <= 4
  (func (export "imm64_imask_lower_only") (result i64)
    i64.const 0x0000000F  ;; 4 bits at pos 0
  )
  (func (export "imm64_imask_lower_shifted") (result i64)
    i64.const 0x000000F0  ;; 4 bits at pos 4
  )
  (func (export "imm64_imask_lower_single") (result i64)
    i64.const 0x00000100  ;; 1 bit at pos 8
  )

  ;; Test case 5: Fallback - requires two MOVimm calls
  (func (export "imm64_fallback_large") (result i64)
    i64.const 0x123456789ABCDEF0  ;; arbitrary large value
  )
  (func (export "imm64_fallback_discontinuous") (result i64)
    i64.const 0x0000000000000505  ;; discontinuous bits in lower32
  )
)

(assert_return (invoke "imm64_const4sx_pos") (i64.const 7))
(assert_return (invoke "imm64_const4sx_neg") (i64.const -8))
(assert_return (invoke "imm64_const4sx_one") (i64.const 1))

(assert_return (invoke "imm64_const16sx_pos") (i64.const 32767))
(assert_return (invoke "imm64_const16sx_neg") (i64.const -32768))
(assert_return (invoke "imm64_const16sx_mid") (i64.const 1000))

(assert_return (invoke "imm64_imask_higher_continuous") (i64.const 0xFFFFFFFF0000000F))
(assert_return (invoke "imm64_imask_higher_partial") (i64.const 0xFFFF000000000000))

(assert_return (invoke "imm64_imask_lower_only") (i64.const 0x0000000F))
(assert_return (invoke "imm64_imask_lower_shifted") (i64.const 0x000000F0))
(assert_return (invoke "imm64_imask_lower_single") (i64.const 0x00000100))

(assert_return (invoke "imm64_fallback_large") (i64.const 0x123456789ABCDEF0))
(assert_return (invoke "imm64_fallback_discontinuous") (i64.const 0x0000000000000505))
