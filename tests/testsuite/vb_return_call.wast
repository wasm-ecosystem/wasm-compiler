;; Test return_call with many parameters (10 params: 8 in regs + 2 on stack for AArch64)
;; This exercises stack-passed parameter handling in tail calls.

(module
  ;; 10-param identity: returns sum of all params for verification
  (func $sum10 (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    (i32.add (local.get 0)
    (i32.add (local.get 1)
    (i32.add (local.get 2)
    (i32.add (local.get 3)
    (i32.add (local.get 4)
    (i32.add (local.get 5)
    (i32.add (local.get 6)
    (i32.add (local.get 7)
    (i32.add (local.get 8) (local.get 9))))))))))
  )

  ;; Path A: caller has same 10 params → tail jump, stack param width equal
  (func (export "same_params") (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    local.get 0 local.get 1 local.get 2 local.get 3
    local.get 4 local.get 5 local.get 6 local.get 7
    local.get 8 local.get 9
    return_call $sum10
  )

  ;; Path B: caller has fewer params (5, all in regs) → callee needs stack params → bl+ret
  (func (export "fewer_params") (param i32 i32 i32 i32 i32) (result i32)
    local.get 0 local.get 1 local.get 2 local.get 3
    local.get 4
    i32.const 100
    i32.const 200
    i32.const 300
    i32.const 400
    i32.const 500
    return_call $sum10
  )

  ;; Path A: caller has more params (12) → callee needs fewer stack slots → tail jump
  (func (export "more_params") (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    local.get 0 local.get 1 local.get 2 local.get 3
    local.get 4 local.get 5 local.get 6 local.get 7
    local.get 8 local.get 9
    return_call $sum10
  )

  ;; Path A: 10-param caller → 10-param callee with different param values (shuffled)
  (func (export "shuffled_params") (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    local.get 9 local.get 8 local.get 7 local.get 6
    local.get 5 local.get 4 local.get 3 local.get 2
    local.get 1 local.get 0
    return_call $sum10
  )

  ;; Deep tail-recursive countdown with 10 params to verify no stack overflow
  ;; Decrements param0 until 0, then returns sum of all other params
  (func $countdown10 (export "countdown10")
    (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    (if (result i32) (i32.eqz (local.get 0))
      (then
        (i32.add (local.get 1)
        (i32.add (local.get 2)
        (i32.add (local.get 3)
        (i32.add (local.get 4)
        (i32.add (local.get 5)
        (i32.add (local.get 6)
        (i32.add (local.get 7)
        (i32.add (local.get 8) (local.get 9))))))))))
      (else
        (return_call $countdown10
          (i32.sub (local.get 0) (i32.const 1))
          (local.get 1) (local.get 2) (local.get 3) (local.get 4)
          (local.get 5) (local.get 6) (local.get 7) (local.get 8)
          (local.get 9)
        )
      )
    )
  )

  ;; Chained tail call: A → B → sum10, each with 10 params
  (func $chain_b (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    ;; Add 1 to first param and forward
    (i32.add (local.get 0) (i32.const 1))
    local.get 1 local.get 2 local.get 3
    local.get 4 local.get 5 local.get 6 local.get 7
    local.get 8 local.get 9
    return_call $sum10
  )

  (func (export "chain") (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    ;; Add 1 to first param and forward to chain_b
    (i32.add (local.get 0) (i32.const 1))
    local.get 1 local.get 2 local.get 3
    local.get 4 local.get 5 local.get 6 local.get 7
    local.get 8 local.get 9
    return_call $chain_b
  )

  ;; Return first stack-passed param (index 8) to verify stack params are correctly passed
  (func $get_param8 (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    local.get 8)

  (func (export "stack_param_passthrough") (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    local.get 0 local.get 1 local.get 2 local.get 3
    local.get 4 local.get 5 local.get 6 local.get 7
    local.get 8 local.get 9
    return_call $get_param8
  )

  ;; Path B (fewer params): verify stack params are correctly placed for callee
  (func (export "fewer_to_stack_param") (param i32 i32) (result i32)
    local.get 0 local.get 1
    i32.const 20 i32.const 30 i32.const 40
    i32.const 50 i32.const 60 i32.const 70
    i32.const 80 i32.const 90
    return_call $get_param8
  )
)

;; Path A: same 10 params, sum = 1+2+3+4+5+6+7+8+9+10 = 55
(assert_return (invoke "same_params"
  (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4) (i32.const 5)
  (i32.const 6) (i32.const 7) (i32.const 8) (i32.const 9) (i32.const 10))
  (i32.const 55))

;; Path B: fewer params, sum = 1+2+3+4+5+100+200+300+400+500 = 1515
(assert_return (invoke "fewer_params"
  (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4) (i32.const 5))
  (i32.const 1515))

;; Path A: more params (surplus), sum = 10+20+30+40+50+60+70+80+90+100 = 550
(assert_return (invoke "more_params"
  (i32.const 10) (i32.const 20) (i32.const 30) (i32.const 40) (i32.const 50)
  (i32.const 60) (i32.const 70) (i32.const 80) (i32.const 90) (i32.const 100)
  (i32.const 110) (i32.const 120))
  (i32.const 550))

;; Path A: shuffled params, sum is the same = 55
(assert_return (invoke "shuffled_params"
  (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4) (i32.const 5)
  (i32.const 6) (i32.const 7) (i32.const 8) (i32.const 9) (i32.const 10))
  (i32.const 55))

;; Deep tail recursion with 10 params: countdown from 100000, sum of params 1..9 = 45
;; reOpen it after all platform support tail-call

;; (assert_return (invoke "countdown10"
;;   (i32.const 100000)
;;   (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4)
;;   (i32.const 5) (i32.const 6) (i32.const 7) (i32.const 8) (i32.const 9))
;;   (i32.const 45))

;; Chained tail call: chain adds 2 to first param (1→3), sum = 3+2+3+4+5+6+7+8+9+10 = 57
(assert_return (invoke "chain"
  (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4) (i32.const 5)
  (i32.const 6) (i32.const 7) (i32.const 8) (i32.const 9) (i32.const 10))
  (i32.const 57))

;; Stack param passthrough: param8 = 88
(assert_return (invoke "stack_param_passthrough"
  (i32.const 10) (i32.const 20) (i32.const 30) (i32.const 40) (i32.const 50)
  (i32.const 60) (i32.const 70) (i32.const 80) (i32.const 88) (i32.const 99))
  (i32.const 88))

;; Path B fewer params: param8 = 80
(assert_return (invoke "fewer_to_stack_param"
  (i32.const 10) (i32.const 20))
  (i32.const 80))
