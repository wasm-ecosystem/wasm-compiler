(module
  (type $check1 (func (param i32)))
  (type $check2 (func (param f32)))
  (import "spectest" "print_i32" (func $import1 (type $check1)))
  (import "spectest" "print_f32" (func $import2 (type $check2)))
  
  (func (export "call_indirect_import_print_i32") (call_indirect (type $check1) (i32.const 0) (i32.const 0)) )
  (func (export "call_indirect_import_print_f32") (call_indirect (type $check2) (f32.const 0.0) (i32.const 1)) )
  
  (table 10 funcref)
  (elem (i32.const 0) $import1 $import2)
)

(assert_return (invoke "call_indirect_import_print_i32"))
(assert_return (invoke "call_indirect_import_print_f32"))

