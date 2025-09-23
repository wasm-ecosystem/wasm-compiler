;; 8 Stacktrace Entries are kept for this file and can be accessed via spectest.getStacktraceCount and spectest.getStacktraceEntry
(module
  (import "spectest" "requestInterruption" (func $requestInterruption))
  (import "spectest" "getStacktraceCount" (func $getStacktraceCount (result i32)))
  (export "getStacktraceCount" (func $getStacktraceCount))
  (import "spectest" "getStacktraceEntry" (func $getStacktraceEntry (param i32) (result i32)))
  (export "getStacktraceEntry" (func $getStacktraceEntry))

  (func $long (export "long")
    call $1
  )
  (func $mid (export "mid")
    call $7
  )
  (func $short (export "short")
    call $trap
  )
  (func $1 call $2)
  (func $2 call $3)
  (func $3 call $4)
  (func $4 call $5)
  (func $5 call $6)
  (func $6 call $7)
  (func $7 call $8)
  (func $8 call $trap)
  (func $trap (export "trap")
    i32.const 0
    i32.const 0
    i32.div_u
    drop
  )

  (func $exhaust (export "exhaust")
    call $exhaust
  )

  (func $importedTrap (export "importedTrap")
    call $requestInterruption
  )

  (type $abortType (func))
  (func $indirectCallTrap (export "indirectCallTrap")
    i32.const 0
    call_indirect (type $abortType)
  )

  (func $indirectImportedCallTrap (export "indirectImportedCallTrap")
    i32.const 1
    call_indirect (type $abortType)
  )

  (table 10 funcref)
  (elem (i32.const 0) $trap $requestInterruption)
)

;; Stacktrace is truncated to 8 elements
(assert_trap (invoke "long") "integer divide by zero")
(assert_return (invoke "getStacktraceCount") (i32.const 8))
(assert_return (invoke "getStacktraceEntry" (i32.const 0)) (i32.const 14))
(assert_return (invoke "getStacktraceEntry" (i32.const 1)) (i32.const 13))
(assert_return (invoke "getStacktraceEntry" (i32.const 2)) (i32.const 12))
(assert_return (invoke "getStacktraceEntry" (i32.const 3)) (i32.const 11))
(assert_return (invoke "getStacktraceEntry" (i32.const 4)) (i32.const 10))
(assert_return (invoke "getStacktraceEntry" (i32.const 5)) (i32.const 9))
(assert_return (invoke "getStacktraceEntry" (i32.const 6)) (i32.const 8))
(assert_return (invoke "getStacktraceEntry" (i32.const 7)) (i32.const 7))

(assert_trap (invoke "mid") "integer divide by zero")
(assert_return (invoke "getStacktraceCount") (i32.const 4))
(assert_return (invoke "getStacktraceEntry" (i32.const 0)) (i32.const 14))
(assert_return (invoke "getStacktraceEntry" (i32.const 1)) (i32.const 13))
(assert_return (invoke "getStacktraceEntry" (i32.const 2)) (i32.const 12))
(assert_return (invoke "getStacktraceEntry" (i32.const 3)) (i32.const 4))

(assert_trap (invoke "short") "integer divide by zero")
(assert_return (invoke "getStacktraceCount") (i32.const 2))
(assert_return (invoke "getStacktraceEntry" (i32.const 0)) (i32.const 14))
(assert_return (invoke "getStacktraceEntry" (i32.const 1)) (i32.const 5))

(assert_trap (invoke "trap") "integer divide by zero")
(assert_return (invoke "getStacktraceCount") (i32.const 1))
(assert_return (invoke "getStacktraceEntry" (i32.const 0)) (i32.const 14))

;; Even for a stack overflow we get 8 because number of stacktrace record counts is set to 8 for these tests
(assert_exhaustion (invoke "exhaust") "call stack exhausted")
(assert_return (invoke "getStacktraceCount") (i32.const 8))
(assert_return (invoke "getStacktraceEntry" (i32.const 0)) (i32.const 15))
(assert_return (invoke "getStacktraceEntry" (i32.const 1)) (i32.const 15))
(assert_return (invoke "getStacktraceEntry" (i32.const 2)) (i32.const 15))
(assert_return (invoke "getStacktraceEntry" (i32.const 3)) (i32.const 15))
(assert_return (invoke "getStacktraceEntry" (i32.const 4)) (i32.const 15))
(assert_return (invoke "getStacktraceEntry" (i32.const 5)) (i32.const 15))
(assert_return (invoke "getStacktraceEntry" (i32.const 6)) (i32.const 15))
(assert_return (invoke "getStacktraceEntry" (i32.const 7)) (i32.const 15))
(assert_return (invoke "getStacktraceEntry" (i32.const 8)) (i32.const 0xFFFFFFFF))

;; Test that it also works for imports
(assert_trap (invoke "importedTrap") "runtime interrupt request")
(assert_return (invoke "getStacktraceCount") (i32.const 2))
(assert_return (invoke "getStacktraceEntry" (i32.const 0)) (i32.const 0))
(assert_return (invoke "getStacktraceEntry" (i32.const 1)) (i32.const 16))

;; Test that it works for indirect calls
(assert_trap (invoke "indirectCallTrap") "integer divide by zero")
(assert_return (invoke "getStacktraceCount") (i32.const 2))
(assert_return (invoke "getStacktraceEntry" (i32.const 0)) (i32.const 14))
(assert_return (invoke "getStacktraceEntry" (i32.const 1)) (i32.const 17))

;; Test that it works for imported indirect calls
(assert_trap (invoke "indirectImportedCallTrap") "runtime interrupt request")
(assert_return (invoke "getStacktraceCount") (i32.const 2))
(assert_return (invoke "getStacktraceEntry" (i32.const 0)) (i32.const 0))
(assert_return (invoke "getStacktraceEntry" (i32.const 1)) (i32.const 18))
