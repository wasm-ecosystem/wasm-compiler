;; Test cases for br_table instruction optimization for tricore
;; Tests both in-range and out-of-range cases for tricore JLTU immediate encoding

(module
  ;; Test case: small br_table (numBranchTargets + 1 fits in unsigned 4-bit range)
  ;; 4 branch targets (0, 1, 2, default=3), numBranchTargets=3, branchTargetsMax=4 fits in 4 bits
  (func (export "br_table_small") (param i32) (result i32)
    (block $b3 (result i32)
      (block $b2 (result i32)
        (block $b1 (result i32)
          (block $b0 (result i32)
            i32.const 100
            local.get 0
            br_table $b0 $b1 $b2 $b3
          )
          i32.const 10
          i32.add
          return
        )
        i32.const 20
        i32.add
        return
      )
      i32.const 30
      i32.add
      return
    )
    i32.const 40
    i32.add
  )

  ;; Test case: large br_table (numBranchTargets + 1 > 15, out of unsigned 4-bit range)
  ;; 17 branch targets (0-15 + default=16), numBranchTargets=16, branchTargetsMax=17 > 15
  (func (export "br_table_large") (param i32) (result i32)
    (block $b16 (result i32)
      (block $b15 (result i32)
        (block $b14 (result i32)
          (block $b13 (result i32)
            (block $b12 (result i32)
              (block $b11 (result i32)
                (block $b10 (result i32)
                  (block $b9 (result i32)
                    (block $b8 (result i32)
                      (block $b7 (result i32)
                        (block $b6 (result i32)
                          (block $b5 (result i32)
                            (block $b4 (result i32)
                              (block $b3 (result i32)
                                (block $b2 (result i32)
                                  (block $b1 (result i32)
                                    (block $b0 (result i32)
                                      i32.const 0
                                      local.get 0
                                      ;; 17 branch targets (0-15 + default=16), numBranchTargets=16, branchTargetsMax=17 > 15
                                      br_table $b0 $b1 $b2 $b3 $b4 $b5 $b6 $b7 $b8 $b9 $b10 $b11 $b12 $b13 $b14 $b15 $b16
                                    )
                                    i32.const 0
                                    i32.add
                                    return
                                  )
                                  i32.const 1
                                  i32.add
                                  return
                                )
                                i32.const 2
                                i32.add
                                return
                              )
                              i32.const 3
                              i32.add
                              return
                            )
                            i32.const 4
                            i32.add
                            return
                          )
                          i32.const 5
                          i32.add
                          return
                        )
                        i32.const 6
                        i32.add
                        return
                      )
                      i32.const 7
                      i32.add
                      return
                    )
                    i32.const 8
                    i32.add
                    return
                  )
                  i32.const 9
                  i32.add
                  return
                )
                i32.const 10
                i32.add
                return
              )
              i32.const 11
              i32.add
              return
            )
            i32.const 12
            i32.add
            return
          )
          i32.const 13
          i32.add
          return
        )
        i32.const 14
        i32.add
        return
      )
      i32.const 15
      i32.add
      return
    )
    i32.const 16
    i32.add
  )

  ;; Test case: exactly 15 branch targets (numBranchTargets=14, branchTargetsMax=15, edge case in range)
  ;; 15 branch targets (0-13 + default=14), branchTargetsMax=15 fits in 4 bits
  (func (export "br_table_edge") (param i32) (result i32)
    (block $b14 (result i32)
      (block $b13 (result i32)
        (block $b12 (result i32)
          (block $b11 (result i32)
            (block $b10 (result i32)
              (block $b9 (result i32)
                (block $b8 (result i32)
                  (block $b7 (result i32)
                    (block $b6 (result i32)
                      (block $b5 (result i32)
                        (block $b4 (result i32)
                          (block $b3 (result i32)
                            (block $b2 (result i32)
                              (block $b1 (result i32)
                                (block $b0 (result i32)
                                  i32.const 0
                                  local.get 0
                                  ;; 15 branch targets (0-13 + default=14), numBranchTargets=14, branchTargetsMax=15 fits in 4 bits
                                  br_table $b0 $b1 $b2 $b3 $b4 $b5 $b6 $b7 $b8 $b9 $b10 $b11 $b12 $b13 $b14
                                )
                                i32.const 0
                                i32.add
                                return
                              )
                              i32.const 1
                              i32.add
                              return
                            )
                            i32.const 2
                            i32.add
                            return
                          )
                          i32.const 3
                          i32.add
                          return
                        )
                        i32.const 4
                        i32.add
                        return
                      )
                      i32.const 5
                      i32.add
                      return
                    )
                    i32.const 6
                    i32.add
                    return
                  )
                  i32.const 7
                  i32.add
                  return
                )
                i32.const 8
                i32.add
                return
              )
              i32.const 9
              i32.add
              return
            )
            i32.const 10
            i32.add
            return
          )
          i32.const 11
          i32.add
          return
        )
        i32.const 12
        i32.add
        return
      )
      i32.const 13
      i32.add
      return
    )
    i32.const 14
    i32.add
  )
)

;; Small br_table tests (in 4-bit range)
(assert_return (invoke "br_table_small" (i32.const 0)) (i32.const 110))
(assert_return (invoke "br_table_small" (i32.const 1)) (i32.const 120))
(assert_return (invoke "br_table_small" (i32.const 2)) (i32.const 130))
(assert_return (invoke "br_table_small" (i32.const 3)) (i32.const 140))  ;; default
(assert_return (invoke "br_table_small" (i32.const 100)) (i32.const 140))  ;; out of range -> default

;; Large br_table tests (branchTargetsMax=17, out of 4-bit range)
(assert_return (invoke "br_table_large" (i32.const 0)) (i32.const 0))
(assert_return (invoke "br_table_large" (i32.const 1)) (i32.const 1))
(assert_return (invoke "br_table_large" (i32.const 5)) (i32.const 5))
(assert_return (invoke "br_table_large" (i32.const 10)) (i32.const 10))
(assert_return (invoke "br_table_large" (i32.const 15)) (i32.const 15))
(assert_return (invoke "br_table_large" (i32.const 16)) (i32.const 16))  ;; default
(assert_return (invoke "br_table_large" (i32.const 100)) (i32.const 16))  ;; out of range -> default

;; Edge case br_table tests (branchTargetsMax=15, still in 4-bit range)
(assert_return (invoke "br_table_edge" (i32.const 0)) (i32.const 0))
(assert_return (invoke "br_table_edge" (i32.const 7)) (i32.const 7))
(assert_return (invoke "br_table_edge" (i32.const 13)) (i32.const 13))
(assert_return (invoke "br_table_edge" (i32.const 14)) (i32.const 14))  ;; default
(assert_return (invoke "br_table_edge" (i32.const 99)) (i32.const 14))  ;; out of range -> default
