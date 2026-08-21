(module
  (global $constant-global i32 (i32.const 1))
  (global $linkdata-global (mut i32) (i32.const 2))

  (func $callee (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    local.get 0
  )

  ;; CHECK-LABEL: Function[1] Body
  (func $caller1 (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    ;; AARCH64: ldr [[AARCH64_CACHED:w[0-9]+]], [sp, #0x[[#%x,AARCH64_STACK_OFFSET:]]]
    ;; AARCH64-NOT: ldr {{w[0-9]+}}, [sp, #0x[[#%x,AARCH64_STACK_OFFSET]]]
    ;; AARCH64: mov w1, [[AARCH64_CACHED]]
    ;; AARCH64: mov w2, [[AARCH64_CACHED]]
    ;; AARCH64: mov w3, [[AARCH64_CACHED]]
    ;; AARCH64: mov w4, [[AARCH64_CACHED]]
    ;; AARCH64: mov w5, [[AARCH64_CACHED]]
    ;; AARCH64: mov w6, [[AARCH64_CACHED]]
    ;; X86_64: mov edi, dword ptr [rsp + 0x[[#%x,X86_STACK_OFFSET:]]]
    ;; X86_64-NOT: mov {{(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))}}, dword ptr [rsp + 0x[[#%x,X86_STACK_OFFSET]]]
    ;; X86_64: mov [[X86_CACHED1:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], edi
    ;; X86_64: mov [[X86_CACHED2:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], edi
    ;; TRICORE: ld.w d9, [sp]{{#0x[0-9a-f]+}}
    ;; TRICORE-NOT: ld.w {{d[0-9]+}}, [sp]{{#0x[0-9a-f]+}}
    ;; TRICORE: mov d6, d9
    ;; TRICORE: mov d7, d9
    ;; TRICORE: mov d10, d9
    ;; TRICORE: mov d11, d9
    ;; TRICORE: mov d12, d9
    ;; AARCH64: bl
    ;; X86_64: call
    ;; TRICORE: fcall
    local.get 7
    local.get 8
    local.get 8
    local.get 8
    local.get 8
    local.get 8
    local.get 8
    local.get 8
    local.get 8
    local.get 8
    call $callee
  )

  ;; CHECK-LABEL: Function[2] Body
  (func $caller2 (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)
    ;; A later stack target overwrites the source slot, the second should use the original value.
    ;; p0 and p1 both read caller stack parameter 8.
    ;; AARCH64: ldr [[AARCH64_REUSED:w[0-9]+]], [sp, #0x[[#%x,AARCH64_SOURCE_OFFSET:]]]
    ;; AARCH64-NOT: ldr {{w[0-9]+}}, [sp, #0x[[#%x,AARCH64_SOURCE_OFFSET]]]
    ;; AARCH64: str {{[a-z][0-9]+}}, [sp, #0x[[#%x,AARCH64_SOURCE_OFFSET]]]
    ;; AARCH64: mov w8, [[AARCH64_REUSED]]
    ;; X86_64: mov ebp, dword ptr [rsp + 0x[[#%x,X86_SOURCE_OFFSET:]]]
    ;; X86_64: movss dword ptr [rsp + 0x[[#%x,X86_SOURCE_OFFSET]]], xmm15
    ;; X86_64: mov edi, ebp
    ;; TRICORE: ld.w d8, [sp]#{{0x[0-9a-f]+}}
    ;; TRICORE: st.a [sp]#{{0x[0-9a-f]+}}, a12
    ;; TRICORE: mov d9, d8
    local.get 8
    local.get 8
    local.get 2
    local.get 3
    local.get 4
    local.get 5
    local.get 6
    local.get 7
    ;; p8 writes caller stack parameter 9 into caller stack parameter 8.
    local.get 9
    ;; p9 uses the register that p1 will eventually target.
    local.get 1
    return_call $callee
  )

  (func $callee-ii (param i32 i32))
  (func $callee-II (param f32 f32))

  ;; CHECK-LABEL: Function[5] Body
  (func $caller-with-reused-multi-instruction-immediate
    ;; AARCH64: mov [[AARCH64_CACHED:w[0-9]+]], #0x5678
    ;; AARCH64-NEXT: movk [[AARCH64_CACHED]], #0x1234, lsl #16
    ;; AARCH64-NEXT: mov [[AARCH64_DEST:w[0-9]+]], [[AARCH64_CACHED]]

    ;; TRICORE: mov [[TRICORE_CACHED:d[0-9]+]], #0x5678
    ;; TRICORE-NEXT: addih [[TRICORE_CACHED]], [[TRICORE_CACHED]], #0x1234
    ;; TRICORE-NEXT: mov [[TRICORE_DEST:d[0-9]+]], [[TRICORE_CACHED]]

    ;; x86 only uses multi-instruction encoding for float immediate
    ;; X86_64: mov ebp, 0x12345678
    ;; X86_64-NEXT: mov edi, 0x12345678
    i32.const 0x12345678
    i32.const 0x12345678
    call $callee-ii

    f32.const 1
    f32.const 1

    ;; X86_64: movss  xmm5, xmm4
    call $callee-II
  )

  (func $linkdata-callee (param i32 i32 i32 i32))

  ;; CHECK-LABEL: Function[7] Body
  (func $linkdata-caller
    ;; The second mutable global remains in link-data and is reused for every argument.
    ;; AARCH64: ldr [[AARCH64_LINKDATA:w[0-9]+]], [x28,
    ;; AARCH64: mov {{w[0-9]+}}, [[AARCH64_LINKDATA]]
    ;; AARCH64: mov {{w[0-9]+}}, [[AARCH64_LINKDATA]]
    ;; AARCH64: mov {{w[0-9]+}}, [[AARCH64_LINKDATA]]

    ;; X86_64: mov [[X86_LINKDATA:(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))]], dword ptr [{{.*}}]
    ;; X86_64-NOT: mov {{(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))}}, dword ptr [{{.*}}]
    ;; X86_64: mov {{(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))}}, [[X86_LINKDATA]]
    ;; X86_64: mov {{(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))}}, [[X86_LINKDATA]]
    ;; X86_64: mov {{(r[0-9]+d?|[re](ax|cx|dx|bx|bp|si|di))}}, [[X86_LINKDATA]]

    ;; TRICORE: ld.w [[TRICORE_LINKDATA:d[0-9]+]], [{{.*}}]
    ;; TRICORE-NOT: ld.w {{d[0-9]+}}, [{{.*}}]
    ;; TRICORE: mov {{d[0-9]+}}, [[TRICORE_LINKDATA]]
    ;; TRICORE: mov {{d[0-9]+}}, [[TRICORE_LINKDATA]]
    ;; TRICORE: mov {{d[0-9]+}}, [[TRICORE_LINKDATA]]
    global.get $linkdata-global
    global.get $linkdata-global
    global.get $linkdata-global
    global.get $linkdata-global
    call $linkdata-callee
  )
)
