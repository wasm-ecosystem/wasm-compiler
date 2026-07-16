(module
  (import "env" "imp10" (func $imp10 (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)))

  ;; CHECK-LABEL: Function[0] Body
  ;; Path B: import function → always falls back to normal call + ret regardless of param count
  (func $return_call_import_10 (export "return_call_import_10") (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    local.get 0 local.get 1 local.get 2 local.get 3
    local.get 4 local.get 5 local.get 6 local.get 7
    local.get 8 local.get 9
    ;; Import calls save LR (str x30) and restore it, then ret
    ;; AARCH64:  str  x30,
    ;; AARCH64:  ldr  x30,
    ;; AARCH64:  ret
    ;; CHECK: Size of the function body
    return_call $imp10
  )
)
