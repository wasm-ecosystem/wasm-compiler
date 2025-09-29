(module
  (func $dummy)

  ;; CHECK-LABEL: Function[1] Body
  (func $helper (param i32) (result i32)
    ;; AARCH64:  str  w19,
    ;; X86_64: mov dword ptr [rsp{{.*}}, ebp
    ;; TRICORE: st.w  [sp]{{.*}}, d8
    call $dummy
    ;; AARCH64:  ldr  w19,
    ;; X86_64: mov  ebp, dword ptr [rsp{{.*}}
    ;; TRICORE: ld.w  d8,
    local.get 0)

  ;; CHECK-LABEL: Function[2] Body
  (func $simple (param i32) (result i32)
    i32.const 10
    local.set 0

    ;; AARCH64:  bl
    ;; X86_64: call
    ;; TRICORE: fcall
    call $dummy

    local.get 0
    ;; AARCH64-NOT:  str  w19, [sp, #8]
    ;; X86_64-NOT: mov dword ptr [rsp{{.*}}, ebp
    ;; TRICORE-NOT: st.w  [sp]{{.*}}, d8
    ;; AARCH64:  bl
    ;; X86_64: call
    ;; TRICORE: fcall
    call $helper)

  ;; CHECK-LABEL: Function[3] Body
  (func $test_if (param i32) (result i32)
    i32.const 10
    ;; AARCH64:  str  w19,
    ;; X86_64: mov dword ptr [rsp{{.*}}, ebp
    ;; TRICORE: st.w  [sp]{{.*}}, d8
    (if (result i32)
      (then
        ;; AARCH64-NOT:  str  w19,
        ;; X86_64-NOT: mov dword ptr [rsp{{.*}}, ebp
        ;; TRICORE-NOT: st.w  [sp]{{.*}}, d8
        (call $dummy)
        (local.get 0)
        ;; AARCH64-NOT:  str  w19,
        ;; X86_64-NOT: mov dword ptr [rsp{{.*}}, ebp
        ;; TRICORE-NOT: st.w  [sp]{{.*}}, d8
        (call $dummy)
        ;; AARCH64-NOT:  str  w19,
        ;; X86_64-NOT: mov dword ptr [rsp{{.*}}, ebp
        ;; TRICORE-NOT: st.w  [sp]{{.*}}, d8
        (call $dummy))
      (else
        (call $dummy)
        (i32.const 1))))

  ;; CHECK-LABEL: Function[4] Body
  (func $test_br_if (param i32) (result i32)
    (block (result i32)
      ;; AARCH64:  str  w19,
      ;; X86_64: mov dword ptr [rsp{{.*}}, ebp
      ;; TRICORE: st.w  [sp]{{.*}}, d8
      (br_if 0
        (i32.const 2)
        (if (result i32)
          (local.get 0)
          (then
            (local.get 0)
            ;; AARCH64-NOT:  str  w19,
            ;; X86_64-NOT: mov dword ptr [rsp{{.*}}, ebp
            ;; TRICORE-NOT: st.w  [sp]{{.*}}, d8
            (call $dummy)
            ;; AARCH64-NOT:  str  w19,
            ;; X86_64-NOT: mov dword ptr [rsp{{.*}}, ebp
            ;; TRICORE-NOT: st.w  [sp]{{.*}}, d8
            (call $dummy))
          (else
            (call $dummy)
            (i32.const 0))))
      (return
        (local.get 0))))

  ;; CHECK-LABEL: Function[5] Body
  (func $test_loop (param i32) (result i32)
    ;; AARCH64:  str  w19,
    ;; X86_64: mov dword ptr [rsp{{.*}}, ebp
    ;; TRICORE: st.w  [sp]{{.*}}, d8
    (loop (result i32)
      local.get 0
      ;; AARCH64-NOT:  str  w19,
      ;; X86_64-NOT: mov dword ptr [rsp{{.*}}, ebp
      ;; TRICORE-NOT: st.w  [sp]{{.*}}, d8
      (if (result i32)
        (then
          (call $dummy)
          (local.get 0))
        (else
          ;; AARCH64-NOT:  str  w19,
          ;; X86_64-NOT: mov dword ptr [rsp{{.*}}, ebp
          ;; TRICORE-NOT: st.w  [sp]{{.*}}, d8
          (call $dummy)
          (i32.const 0)
          (call $dummy)))
      (call $dummy)
      (call $dummy))))
