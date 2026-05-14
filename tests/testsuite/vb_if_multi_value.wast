(module
  ;; CHECK-LABEL: Function[0] Body
  (func $if-multi-return (param i32) (result i32)
    local.get 0
    i32.const 5
    local.set 0
    i32.const 10
    i32.eq
    if (result i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
      i32.const 1
      i32.const 2
      i32.const 3
      i32.const 4
      i32.const 5
      i32.const 6
      i32.const 7
      i32.const 8
      i32.const 9
      i32.const 10
      i32.const 11
      i32.const 12
      i32.const 13
      i32.const 14
      i32.const 15
      i32.const 16
      i32.const 17
      i32.const 18
      i32.const 19
      i32.const 20
    else
      i32.const 3
      i32.const 4
      i32.const 5
      i32.const 6
      i32.const 7
      i32.const 8
      i32.const 9
      i32.const 10
      i32.const 11
      i32.const 12
      i32.const 13
      i32.const 14
      i32.const 15
      i32.const 16
      i32.const 17
      i32.const 18
      i32.const 19
      i32.const 20
      i32.const 21
      i32.const 22
    end

    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add

    return)

  (export "if-multi-return" (func $if-multi-return))
)

(assert_return (invoke "if-multi-return" (i32.const 10)) (i32.const 210))
(assert_return (invoke "if-multi-return" (i32.const 1)) (i32.const 250))