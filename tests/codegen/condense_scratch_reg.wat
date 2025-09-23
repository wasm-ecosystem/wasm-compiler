(module
   ;; CHECK-LABEL: Function[0] Body
  (func $foo (param i32 i32) (result i32)
    (local i32)
    i32.const 4
    ;; X86_64_NO_ACTIVE_STACK_OVERFLOW_CHECK: mov [[REG1:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], dword ptr [rbx + [[NUM:[0-9]+]]]
    ;; AARCH64_NO_ACTIVE_STACK_OVERFLOW_CHECK: ldr [[REG1:w[0-9]+]], [x29, [[REG:x[0-9]+]]]
    i32.load
    i32.const 0
    ;; X86_64_NO_ACTIVE_STACK_OVERFLOW_CHECK-NEXT: mov [[REG2:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], dword ptr [rbx]
    ;; AARCH64_NO_ACTIVE_STACK_OVERFLOW_CHECK: ldr [[REG2:w[0-9]+]], [x29, [[REG:x[0-9]+]]]
    i32.load

    i32.const 4
    ;; X86_64_NO_ACTIVE_STACK_OVERFLOW_CHECK-NEXT: mov [[REG3:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], dword ptr [rbx + [[NUM:[0-9a-f]+]]]
    ;; AARCH64_NO_ACTIVE_STACK_OVERFLOW_CHECK: ldr [[REG3:w[0-9]+]], [x29, [[REG:x[0-9]+]]]
    i32.load
    local.set 2

    ;; X86_64_NO_ACTIVE_STACK_OVERFLOW_CHECK-NEXT: sub [[REG1]], [[REG2]]
    ;; AARCH64_NO_ACTIVE_STACK_OVERFLOW_CHECK: sub [[REG:w[0-9]+]], [[REG1]], [[REG2]]
    i32.sub

    local.get 0
    local.get 1
    i32.mul

    i32.add
    return
  )

  (memory 1 100)
  (data (i32.const 0) "\01\00\00\00\02\00\00\00")
)