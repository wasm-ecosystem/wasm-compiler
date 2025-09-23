(module
  (func $callee/0)
  ;; CHECK-LABEL: Function[1] Body
  (func $with-end/1
    (param $arg0 i32)
    block $loop
    ;; enter loop
    ;; AARCH64:  add  sp, sp, #0x[[#%x,STACK_SIZE:]]
      call $callee/0
    ;; before function call, locals should be stored in stack
    ;; AARCH64:  str  w19,
    ;; AARCH64:  bl
    ;; after function call, locals should be recover to reg
    ;; AARCH64:  ldr w19,
    end
    ;; exit loop
    ;; AARCH64:  sub  sp, sp, #0x[[#%x,STACK_SIZE]]
  )
  ;; CHECK-LABEL: Function[2] Body
  (func $with-br/2
    (local $arg0 i32)
    ;; before loop, locals will be STACK_REG
    ;; AARCH64:  str  w19,
    loop $loop
    ;; enter loop
    ;; AARCH64:  add  sp, sp, #0x[[#%x,STACK_SIZE:]]
      call $callee/0
    ;; AARCH64:  bl
      br $loop
    ;; after function call, locals should be recover to reg
    ;; AARCH64:  ldr w19,
    end
    ;; exit loop
    ;; AARCH64:  sub  sp, sp, #0x[[#%x,STACK_SIZE]]
  )
  ;; CHECK-LABEL: Function[3] Body
  (func $with-br-if/3
    (local $arg0 i32)
    (local $arg1 i32)
    ;; before loop, locals will be STACK_REG
    ;; AARCH64:  str  w19,
    loop $loop
    ;; enter loop
    ;; AARCH64:  add  sp, sp, #0x[[#%x,STACK_SIZE:]]
      call $callee/0
    ;; AARCH64:  bl
      local.get $arg1
      br_if $loop
    ;; after function call, locals should be recover to reg
    ;; AARCH64:  ldr w19,
    end
    ;; exit loop
    ;; AARCH64:  sub  sp, sp, #0x[[#%x,STACK_SIZE]]
  )
  ;; CHECK-LABEL: Function[4] Body
  (func $with-reover-from-unreachable/4 (param $arg0 i32) (param $arg1 i32) (result i32)
    local.get $arg1
    ;; before if, locals will be STACK_REG
    ;; AARCH64:      str  w19,
    if
      call $callee/0
    ;; AARCH64:      bl
      unreachable
    end
    local.get $arg0
    ;; Because if-block is unreachable, considering the code size, compiler does not generate load instruction.
    ;; After block, local should be in reg, compiler also does not generate load instruction.
    ;; AARCH64-NOT:  ldr  w19,
  )
)
