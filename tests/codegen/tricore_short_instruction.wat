;; CHECK-LABEL: GenericTrapHandler Body

;; Load trapCodePtr into a register and store the trapCode there
;; TRICORE: ld.a  a12, [sp]
;; TRICORE-NEXT: st.w  [a12], d0

(module
  (memory 1)
  ;; CHECK-LABEL: Function[0] Body
  (func $eq-tricore-16-bit-instruction (param i32) (result i32) (local i32 i32 i32 i32 i32 i32 i32 i32 i32)
    local.get 9
    i32.const 0
    ;; TRICORE: eq  d15, [[LOCAL_REG:d[0-9]+]], #0
    i32.eq
    local.set 9
    local.get 9
    local.get 0
    ;; TRICORE: eq  d15, [[LOCAL_REG:d[0-9]+]], [[LOCAL_REG:d[0-9]+]]
    i32.eq
    local.set 9
    local.get 9
  )
  ;; CHECK-LABEL: Function[1] Body
  (func $lt-tricore-16-bit-instruction (param i32) (result i32) (local i32 i32 i32 i32 i32 i32 i32 i32 i32)
    local.get 9
    i32.const 0
    ;; TRICORE: lt  d15, [[LOCAL_REG:d[0-9]+]], #{{[0-9]+}}
    i32.lt_s
    local.set 9
    local.get 9
    local.get 0
     ;; TRICORE: lt  d15, [[LOCAL_REG:d[0-9]+]], [[LOCAL_REG:d[0-9]+]]
    i32.lt_s
    local.set 9
    local.get 9
  )
  ;; CHECK-LABEL: Function[2] Body
  (func $or-tricore-16-bit-instruction (result i32) (local i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    i32.const 0xFF
    local.set 9
    local.get 9
    i32.const 0
    ;; TRICORE: or  d15, d15, #{{[0-9]+}}
    i32.or
    local.set 9
    local.get 9
  )
  ;; CHECK-LABEL: Function[3] Body
  (func $and-tricore-16-bit-instruction (result i32) (local i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    i32.const 0xFF
    local.set 9
    local.get 9
    i32.const 0
    ;; TRICORE: and  d15, d15, #{{[0-9]+}}
    i32.and
    local.set 9
    local.get 9
  )

  ;; CHECK-LABEL: Function[4] Body
  (func $check-load-byte-unsigned-16-bit-instruction
    (local i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    ;; TRICORE: 14 28  ld.bu  d8, [a2]
    i32.const 0
    i32.load8_u
    local.set 0
    
    i32.const 10
    ;; TRICORE: 0c 2a  ld.bu  d15, [a2]#0xa
    i32.load8_u
    local.set 9
  )

  ;; CHECK-LABEL: Function[5] Body
  (func $check-load-halfword-16-bit-instruction
    (local i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    ;; TRICORE: 94 28  ld.h  d8, [a2]
    i32.const 0
    i32.load16_s
    local.set 0
    
    i32.const 8
    ;; TRICORE: 8c 24  ld.h  d15, [a2]#8
    i32.load16_s
    local.set 9
    
  )

  ;; CHECK-LABEL: Function[6] Body
  (func $check-load-word-16-bit-instruction
    ;; TRICORE: 54 28  ld.w  d8, [a2]
    (local i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    i32.const 0
    i32.load 
    local.set 0
    i32.const 8
    ;; TRICORE: 4c 22  ld.w  d15, [a2]#8
    i32.load
    local.set 9
    
  )

  ;; CHECK-LABEL: Function[7] Body
  (func $check-store-word-16-bit-instruction  (result i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    i32.const 0
    i32.load 
    i32.const 0
    i32.load 
    i32.const 0
    i32.load 
    i32.const 0
    i32.load 
    i32.const 0
    i32.load
    i32.const 0
    i32.load
    i32.const 0
    i32.load
    i32.const 0
    i32.load
    i32.const 0
    i32.load
    i32.const 12
    i32.const 0
    ;; TRICORE: 6c 23  st.w  [a2]#0xc, d15
    i32.store
    i32.const 0
    i32.load
    i32.const 0
    i32.load
    i32.const 0
    i32.load
    i32.const 0
    i32.const 0
    ;; TRICORE: 74 22  st.w  [a2], d2
    i32.store
  )

  ;; CHECK-LABEL: Function[8] Body
  (func $check-store-byte-16-bit-instruction  (result i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    i32.const 0
    i32.load 
    i32.const 0
    i32.load 
    i32.const 0
    i32.load 
    i32.const 0
    i32.load 
    i32.const 0
    i32.load
    i32.const 0
    i32.load
    i32.const 0
    i32.load
    i32.const 0
    i32.load
    i32.const 0
    i32.load
    i32.const 8
    i32.const 0
    ;; TRICORE: 2c 28  st.b  [a2]#8, d15
    i32.store8
    i32.const 0
    i32.load
    i32.const 0
    i32.load
    i32.const 0
    i32.load
    i32.const 0
    i32.const 0
    ;; TRICORE: 34 22  st.b  [a2], d2
    i32.store8
  )

  ;; CHECK-LABEL: Function[9] Body
  (func $check-store-halfword-16-bit-instruction  (result i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32)
    i32.const 0
    i32.load 
    i32.const 0
    i32.load 
    i32.const 0
    i32.load 
    i32.const 0
    i32.load 
    i32.const 0
    i32.load
    i32.const 0
    i32.load
    i32.const 0
    i32.load
    i32.const 0
    i32.load
    i32.const 0
    i32.load
    i32.const 8
    i32.const 0
    ;; TRICORE: ac 24  st.h  [a2]#8, d15
    i32.store16
    i32.const 0
    i32.load
    i32.const 0
    i32.load
    i32.const 0
    i32.load
    i32.const 0
    i32.const 0
    ;; TRICORE: b4 22  st.h  [a2], d2
    i32.store16
  )
)


