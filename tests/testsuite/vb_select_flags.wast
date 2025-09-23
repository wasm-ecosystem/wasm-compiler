;; This test case is added to ensure that CPU Flags remain unchanged when emit instructions for `select`.
(module
  (func (;0;) (result i32)
    (local i32)

    i32.const 0xf

    call 1

    i32.const 15  
    i32.load8_u   ;; L1 = 0

    i32.const 14
    i32.load8_u   ;; L2 = 0

    i32.const 13
    i32.load8_u   ;; L3 = 0

    i32.const 12
    i32.load8_u   ;; L4 = 0

    i32.const 11
    i32.load8_u   ;; L5 = 0

    i32.const 10
    i32.load8_u   ;; L6 = 0

    i32.const 9
    i32.load8_u   ;; L7 = 0

    i32.const 8
    i32.load8_u   ;; L8 = 0

    i32.const 7
    i32.load8_u   ;; L9 = 0

    i32.const 6
    i32.load8_u   ;; L10 = 0

    i32.const 5
    i32.load8_u   ;; L11 = 0

    i32.const 4
    i32.load8_u   ;; L12 = 0

    i32.const 3
    i32.load8_u   ;; L13 = 0

    i32.const 2
    i32.load8_u   ;; L14 = 0

    i32.const 1
    i32.load8_u   ;; L15 = 0

    i32.const 0
    i32.load8_u   ;; L16 = 0

    i32.const 123
    i32.const 345

    i32.const 0xABCD
    i32.load8_s

    select
    return

  )
  (func (;1;) 
  )
  (memory (;0;) 16 17)
  (global (;0;) (mut i32) (i32.const -1))
  (export "hashMemory" (func 0))
)

(assert_return (invoke "hashMemory") (i32.const 345))
