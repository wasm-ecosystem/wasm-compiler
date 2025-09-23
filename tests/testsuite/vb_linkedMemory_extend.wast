(module
  (import "builtin" "getU8FromLinkedMemory" (func $getU8FromLinkedMemory (param i32) (result i32)))
  (global $offset (mut i32) (i32.const 9))
  (func (export "test_read_linked_memory_with_global_index") (result i32)
    global.get $offset
    i32.const 12
    i32.ge_s
    if
    i32.const 0
    global.set $offset
    end
    global.get $offset
    call $getU8FromLinkedMemory
  )
)

(assert_return (invoke "test_read_linked_memory_with_global_index") (i32.const 9))