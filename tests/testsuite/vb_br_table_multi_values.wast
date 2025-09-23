;; Additional cases for the `br_table` instruction in multi-value spectest

(module
  (func (export "multiple-value") (param i32) (result i32 i32)
    (local i32)
    (local.set 1 (block (result i32 i32)
      (local.set 1 (block (result i32 i32)
        (local.set 1 (block (result i32 i32)
          (local.set 1 (block (result i32 i32)
            (local.set 1 (block (result i32 i32)
              (br_table 3 2 1 0 4 (i32.const 100) (i32.const 200) (local.get 0))
              (return (i32.add (local.get 1) (i32.const 99)))
            ))
            (return (i32.add (local.get 1) (i32.const 10)))
          ))
          (return (i32.add (local.get 1) (i32.const 11)))
        ))
        (return (i32.add (local.get 1) (i32.const 12)))
      ))
      (return (i32.add (local.get 1) (i32.const 13)))
    ))
    (i32.add (local.get 1) (i32.const 14))
  )

  (func (export "multi-loop-block-br_table") (param i32) (result i32 i32)
    i32.const 100
    i32.const 200
    loop (param i32 i32) (result i32 i32)
      i32.const 1
      i32.add
      block (param i32 i32) (result i32 i32)
        local.get 0
        local.get 0
        i32.const 1
        i32.add
        local.set 0
        br_table 1 0 0
      end
    end
  )
)

(assert_return (invoke "multiple-value" (i32.const 0)) (i32.const 100) (i32.const 213))
(assert_return (invoke "multiple-value" (i32.const 1)) (i32.const 100) (i32.const 212))
(assert_return (invoke "multiple-value" (i32.const 2)) (i32.const 100) (i32.const 211))
(assert_return (invoke "multiple-value" (i32.const 3)) (i32.const 100) (i32.const 210))
(assert_return (invoke "multiple-value" (i32.const 4)) (i32.const 100) (i32.const 214))
(assert_return (invoke "multiple-value" (i32.const 5)) (i32.const 100) (i32.const 214))
(assert_return (invoke "multiple-value" (i32.const 6)) (i32.const 100) (i32.const 214))
(assert_return (invoke "multiple-value" (i32.const 10)) (i32.const 100) (i32.const 214))
(assert_return (invoke "multiple-value" (i32.const -1)) (i32.const 100) (i32.const 214))
(assert_return (invoke "multiple-value" (i32.const 0xffffffff)) (i32.const 100) (i32.const 214))

(assert_return (invoke "multi-loop-block-br_table" (i32.const 0))  (i32.const 100) (i32.const 202))
(assert_return (invoke "multi-loop-block-br_table" (i32.const 1))  (i32.const 100) (i32.const 201))
(assert_return (invoke "multi-loop-block-br_table" (i32.const 2))  (i32.const 100) (i32.const 201))
(assert_return (invoke "multi-loop-block-br_table" (i32.const 3))  (i32.const 100) (i32.const 201))

(module
  ;; unreachable br_table should drop valentBlock correctly
  (func $unreachable-br-table (result f64 f32)
    unreachable
    block (result f64 f32)
      f64.const 0
      f32.const 1
      i32.const 0xff
      br_table 0
    end
  )
)

