;; Test unreachable returns of different but compatible type
(module
  (type $t0 (func))
  (type $t1 (func (param f32) (result f32)))
  (type $t2 (func (param f64) (result f64)))
  (func $func (type $t0)
    unreachable
    call $f10
    i64.trunc_f32_s
    f64.const 0x1.8p+1 (;=3;)
    call $f11
    block $B0
    end
    drop
    drop)
  (func $f10 (type $t1) (param $p0 f32) (result f32)
    local.get $p0)
  (func $f11 (type $t2) (param $p0 f64) (result f64)
    local.get $p0)
)

;; Test unreachable block returns
(module
  (func $func (param $p f64) (result i32)
    unreachable
        
    block (result f64)
      local.get $p
    end
    f64.eq
    select

    ;; does nothing
    i32.const 1
    if
      call $aux
      drop
    end
  )
  (func $aux (result f32)
   f32.const 1.0
  )
  (export "func_2" (func $func))
)


(module
  (global (mut i32) (i32.const 0))
  (func (result i32)
    global.get 0
    i32.const 1
    i32.add
    global.set 0
    global.get 0

    global.get 0
    i32.const 1
    i32.add
    global.set 0
    global.get 0

    global.get 0
    i32.const 1
    i32.add
    global.set 0
    global.get 0

    global.get 0
    i32.const 1
    i32.add
    global.set 0
    global.get 0

    global.get 0
    i32.const 1
    i32.add
    global.set 0
    global.get 0

    global.get 0
    i32.const 1
    i32.add
    global.set 0
    global.get 0

    global.get 0
    i32.const 1
    i32.add
    global.set 0
    global.get 0

    global.get 0
    i32.const 1
    i32.add
    global.set 0
    global.get 0

    global.get 0
    i32.const 1
    i32.add
    global.set 0
    global.get 0

    global.get 0
    i32.const 1
    i32.add
    global.set 0
    global.get 0

    global.get 0
    i32.const 1
    i32.add
    global.set 0
    global.get 0

    global.get 0
    i32.const 1
    i32.add
    global.set 0
    global.get 0

    global.get 0
    i32.const 1
    i32.add
    global.set 0
    global.get 0

    global.get 0
    i32.const 1
    i32.add
    global.set 0
    global.get 0

    global.get 0
    i32.const 1
    i32.add
    global.set 0
    global.get 0

    global.get 0
    i32.const 1
    i32.add
    global.set 0
    global.get 0

    global.get 0
    i32.const 1
    i32.add
    global.set 0
    global.get 0

    global.get 0
    i32.const 1
    i32.add
    global.set 0
    global.get 0

    global.get 0
    i32.const 1
    i32.add
    global.set 0
    global.get 0

    global.get 0
    i32.const 1
    i32.add
    global.set 0
    global.get 0

    global.get 0
    i32.const 1
    i32.add
    global.set 0
    global.get 0

    global.get 0
    i32.const 1
    i32.add
    global.set 0
    global.get 0

    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
  )
)

(module
  (global (mut i32) (i32.const 1))
  (func (result i32)
    i32.const 1
	i32.const 2
	i32.const 1
	select
	global.set 0
	global.get 0
  )
)

(module
  (global (mut i32) (i32.const 1))
  (func (result i32)
    i32.const 1
	i32.const 2
	i32.const 1
	select
	global.set 0
	global.get 0
  )
)

(module
  (func (result i64)
    (local i64)
	local.get 0
	i64.const 16
	i64.add
	i64.const 1024
	i64.add
  )
)

(assert_invalid
  (module
    (import "env" "unavailableimport" (memory 1))
  )
  "importing memory not supported"
)

(assert_invalid
  (module
    (import "env" "unavailableimport" (table 1 funcref))
  )
  "unknown import"
)

(assert_invalid
  (module
    (import "env" "unavailableimport" (global i32))
  )
  "unknown import"
)

(module
  (import "spectest" "nop" (func))
  (start 0)
)

(module
  (memory 1)
  (func
    unreachable
    memory.size
    drop
  )
)

(module
  (import "spectest" "print_i32" (func (param i32)))
  (export "reexport" (func 0))
)

(module
  (global $i64 i64 (i64.const 1))
  (global $f64 f64 (f64.const 1))
  (export "i64" (global 0))
  (export "f64" (global 1))
)

(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\01\04\01\60\00\00"      ;; Type section
    "\03\02\01\00"            ;; Function section
    "\04\04\01\70\00\00"      ;; Table section
    "\0a\07\01"               ;; Code section

    ;; function 0
    "\05\00"
    "\41\00"                   ;; i32.const 0
    "\c0"                      ;; call_indirect (type 0)
    "\0b"                      ;; end
  )
  "Unknown instruction (i32.extend8_s)"
)

(assert_invalid
(module
  (import "env" "imp" (global i32))
  (func)
  (table 1 funcref)
  (elem (global.get 0) 0)
)
"No support in VB for global initialization of elem"
)

(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\02\0a\01"               ;; Import section

	"\03" "\65\6e\76" ;; env
	"\03" "\69\6d\70" ;; imp
	"\04" ;; import type
  )
  "unknown import type"
)

(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"

	"\05\03\01" ;; Memory section
	"\00\01" ;; max and initial size

	"\0b\07\01" ;; Data Section
	"\00" ;; memory index
	"\23\00\0b" ;; offset via global
	"\01" ;; segment size
	"\61" ;; data
  )
  "Malformed offset"
)

(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"

	"\05\03\01" ;; Memory section
	"\00\01" ;; max and initial size

	"\0b\07\01" ;; Data Section
	"\00" ;; memory index
	"\00\00\0b" ;; offset malformed
	"\01" ;; segment size
	"\61" ;; data
  )
  "Malformed offset"
)

(module binary
    "\00asm" "\01\00\00\00"
	"\01\04\01\60\00\00" ;; Section "Type"
	"\03\02\01\00" ;; Section "Function"
	"\0a\04\01\02\00\0b" ;; Section "Code"

	"\00\10\04\6e\61\6d\65" ;; Section "name"
	"\01\04\01\00\01\61"  ;; Subsection
	"\02\03\01\00\00" ;; Local name
)

(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"
    "\04\04\01\70\00\01" ;; Section "Table"
    "\09\06\01\00\23\00\0b\00" ;; Section "Elem"
  )
  "cannot use globals as elem offset"
)

(module
  (global (mut i32) (i32.const 2))
  (global (mut i32) (i32.const 1))
  (func (result i32)
	global.get 1
    global.set 0
	global.get 0
  )
)


(module
  (global (mut f32) (f32.const 2))
  (global (mut f32) (f32.const 1))
  (func (result f32)
	global.get 1
    global.set 0
	global.get 0
  )
)

(module
  (global (mut f64) (f64.const 2))
  (func (result f64)
	f64.const 1
    global.set 0
	global.get 0
  )
)

(module
  (global (mut i64) (i64.const 2))
  (func (result i64)
	i64.const 1
    global.set 0
	global.get 0
  )
)

(module
  (global (mut i64) (i64.const 2))
  (func (result i64)
	i64.const 0xFFFFFFFF
    global.set 0
	global.get 0
  )
)

(module
  (func (result f32)
	f32.const 1
    f32.const 2
    i32.const 1
    select
  )
)

(module
  (func (result f64)
	f64.const 1
    f64.const 2
    i32.const 1
    select
  )
)

(module
  (global (mut i64) (i64.const 2))
  (func
	i64.const 0xFF
    i64.const 0xFFFFFFFFFF
    i32.const 1
    select
	global.set 0

	global.get 0
	i64.const 0xFFFF
    i32.const 0
    select
	global.set 0
  )
)

(module
  (func (result i32)
	i32.const 1
    i32.const 2
    i32.const 1
    select
  )
)

(module
  (func (result i64)
	i64.const 512
    i64.const 1024
    i32.const 1
    select
  )
)

(module
  (func (result i64)
	i64.const 512
    i64.const 512
    i64.mul
  )
)

(module
  (func (result i64)
	i64.const 0xFFFFFFFFFFFFF
    i64.const 0xFFFFFFFFFFFFF
    i64.mul
  )
)

(module
  (func (result i32)
	i64.const 512
    i32.wrap_i64
  )
)

(module
  (func (result i64)
	i32.const 5
    i64.extend_i32_u
  )
)

(module
  (func (result f32)
	i32.const 5
    f32.reinterpret_i32
  )
)

(module
  (global (mut i32) (i32.const 5))
  (func (result f32)
	global.get 0
    f32.reinterpret_i32
  )
)

(module
  (global (mut i32) (i32.const 5))
  (func (result i32)
	global.get 0
    f32.reinterpret_i32
    i32.reinterpret_f32
  )
)

(module
  (func (result i32)
  	i32.const 2
  	i32.const 1
	f32.const 1.0
	f32.const 2.0
	f32.eq
	br_if 0
	drop
  )
)

(module
  (func (result i32)
  	i32.const 2
  	i32.const 1
	f32.const 1.0
	f32.const 2.0
	f32.ne
	br_if 0
	drop
  )
)

(module
  (func)
  (func (result i32)
	block (result i32)
	  call 0
	  i32.const 1
	  f32.const 1.0
	  f32.const 2.0
	  f32.ge
	  br_if 0
	end
  )
)

(module
  (func)
  (func (result i32)
	block (result i32)
	  call 0
	  i32.const 1
	  f32.const 1.0
	  f32.const 2.0
	  f32.ne
	  br_if 0
	end
  )
)

(module
  (type (;0;) (func (param i32 i32)))
  (type (;1;) (func (param i32)))
  (type (;2;) (func))
  (import "unknown" "import1" (func (;0;) (type 0)))
  (import "unknown" "import2" (func (;1;) (type 1)))
  (func (;2;) (type 2)
    i32.const 1
    i32.const 1
    call 0
    i32.const 1
    call 1)
  (export "_start" (func 2))
)

(assert_trap (invoke "_start") "unknown import")

(module
  (func (result f64)
    call $gen
    i32.const 1
    if (result f64)
      f64.const 1
    else
    	f64.const 0
    end
    return
  )

  (func $gen (result f32)
    (f32.const 0)
  )
)

;; Simplified module from fuzz that failed merge stack ranges
(module
  (type $t0 (func (param i32) (result i32)))
  (func $f0 (type $t0) (param $p0 i32) (result i32)
    block $B0 (result i32)
      i32.const 1
      i32.const 2
      i32.add
      i32.const 1
      i32.const 2
      i32.add
      i32.const 0
      local.get $p0
      select
      br_if $B0
    end))

;; Simplified module from fuzz that failed spill from stack
(module
  (func (result i32)
    (local i32)
    i32.const 15
    i32.load8_u
    i32.const 6
    i32.load8_u
    i32.const 14
    i32.load8_u
    i32.const 6
    i32.load8_u
    i32.const 13
    i32.load8_u
    i32.const 12
    i32.load8_u
    i32.const 1
    i32.load8_u
    i32.const 11
    i32.load8_u
    i32.const 10
    i32.load8_u
    i32.const 9
    i32.load8_u
    i32.const 1
    i32.load8_u
    i32.const 8
    i32.load8_u
    i32.const 7
    i32.load8_u
    i32.const 13
    i32.load8_u
    i32.const 6
    i32.load8_u
    i32.const 1
    i32.load8_u
    i32.const 4
    i32.load8_u
    i32.const 3
    i32.load8_u
    i32.const 2
    i32.load8_u
    i32.const 1
    i32.load8_u
    i32.const 0
    i32.load8_u
    local.get 0
    i32.const 5
    i32.shl
    local.get 0
    i32.add
    i32.xor
    i32.xor
    local.tee 0
    local.get 0
    i32.const 5
    i32.shl
    i32.add
    i32.xor
    local.tee 0
    local.get 0
    i32.const 5
    i32.shl
    i32.add
    i32.xor
    local.tee 0
    local.get 0
    i32.const 5
    i32.shl
    i32.add
    i32.xor
    local.tee 0
    local.get 0
    i32.const 5
    i32.shl
    i32.add
    i32.xor
    local.tee 0
    local.get 0
    i32.const 5
    i32.shl
    local.get 0
    i32.add
    i32.add
    i32.xor
    local.tee 0
    local.get 0
    i32.const 5
    i32.shl
    i32.add
    i32.xor
    local.get 0
    i32.const 5
    i32.shl
    i32.add
    i32.xor
    local.tee 0
    local.get 0
    i32.const 5
    i32.shl
    i32.add
    i32.xor
    local.tee 0
    local.get 0
    i32.const 5
    i32.shl
    i32.add
    i32.xor
    i32.xor
    local.tee 0
    local.get 0
    i32.const 5
    i32.shl
    i32.add
    i32.xor
    local.tee 0
    local.get 0
    i32.const 5
    i32.shl
    i32.add
    i32.xor
    i32.add
    i32.xor
    local.tee 0
    local.get 0
    i32.const 7
    i32.load8_u
    i32.shl
    i32.add
    i32.xor
    i32.add
    i32.xor
    i32.add
    i32.xor
  )
  (memory (;0;) 1 2)
)

;; (module
;;   (func (export "abc") 
;;         drop
;;     )
;; )

(assert_malformed
  (module binary
    "\00asm" "\01\00\00\00"     ;; WASM magic bytes and version
    "\01\04\01\60\00\00"        ;; Type section: 1 type
    "\03\02\01\00"              ;; Function section: 1 function
    "\07\07\01"                 ;; Export section: 1 export
    "\03\61\62\63"              ;; Export name: "abc"
    "\00\00"                    ;; Export kind: Function, index 0
    "\0a\05\01"                 ;; Code section: 1 function body
    "\03\00"                    ;; Function body size and local count
    "\1a"                       ;; drop (with no value on stack)
    "\0b"                       ;; end
  )
  "type mismatch"
)