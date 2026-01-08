(module
  (memory 1)
  
  (data (i32.const 0) "\ff\fe\80\81\00\00\00\80\00\00\00\00\ff\ff\ff\ff")
  (data (i32.const 100) "\ff\7f\80\00\ff\ff\00\80\00\00\00\00\ff\ff\ff\7f")
  (data (i32.const 4000) "\ff\ff\ff\ff\ff\ff\ff\7f\00\00\00\00\00\00\00\80")
  (data (i32.const 5000) "\80\00\ff\00\00\ff\ff\ff\ff\ff\ff\ff\ff\ff\ff\ff")
  (data (i32.const 20000) "\ff\ff\ff\7f\00\00\00\80\ff\00\80\00\ff\ff\00\80")
  (data (i32.const 40000) "\ff\ff\ff\ff\ff\ff\ff\7f\00\00\00\00\00\00\00\80")

  (func (export "i32_load_aligned") (result i32)
    i32.const 0
    i32.load offset=100
  )
  
  (func (export "i32_load_misaligned") (result i32)
    i32.const 0
    i32.load offset=101
  )
  
  (func (export "i32_load_out_of_range") (result i32)
    i32.const 0
    i32.load offset=20000
  )

  (func (export "i64_load_aligned") (result i64)
    i32.const 0
    i64.load offset=4000
  )
  
  (func (export "i64_load_misaligned") (result i64)
    i32.const 0
    i64.load offset=4001
  )
  
  (func (export "i64_load_out_of_range") (result i64)
    i32.const 0
    i64.load offset=40000
  )

  (func (export "f32_load_aligned") (result f32)
    i32.const 0
    f32.load offset=100
  )
  
  (func (export "f32_load_misaligned") (result f32)
    i32.const 0
    f32.load offset=101
  )
  
  (func (export "f32_load_out_of_range") (result f32)
    i32.const 0
    f32.load offset=20000
  )

  (func (export "f64_load_aligned") (result f64)
    i32.const 0
    f64.load offset=4000
  )
  
  (func (export "f64_load_misaligned") (result f64)
    i32.const 0
    f64.load offset=4001
  )
  
  (func (export "f64_load_out_of_range") (result f64)
    i32.const 0
    f64.load offset=40000
  )

  (func (export "i32_load8_s_in_range") (result i32)
    i32.const 0
    i32.load8_s offset=100
  )
  
  (func (export "i32_load8_s_odd") (result i32)
    i32.const 0
    i32.load8_s offset=101
  )
  
  (func (export "i32_load8_s_out_of_range") (result i32)
    i32.const 0
    i32.load8_s offset=5000
  )

  (func (export "i32_load8_u_in_range") (result i32)
    i32.const 0
    i32.load8_u offset=100
  )
  
  (func (export "i32_load8_u_odd") (result i32)
    i32.const 0
    i32.load8_u offset=101
  )
  
  (func (export "i32_load8_u_out_of_range") (result i32)
    i32.const 0
    i32.load8_u offset=5000
  )

  (func (export "i32_load16_s_aligned") (result i32)
    i32.const 0
    i32.load16_s offset=100
  )
  
  (func (export "i32_load16_s_misaligned") (result i32)
    i32.const 0
    i32.load16_s offset=101
  )
  
  (func (export "i32_load16_s_out_of_range") (result i32)
    i32.const 0
    i32.load16_s offset=20000
  )

  (func (export "i32_load16_u_aligned") (result i32)
    i32.const 0
    i32.load16_u offset=100
  )
  
  (func (export "i32_load16_u_misaligned") (result i32)
    i32.const 0
    i32.load16_u offset=101
  )
  
  (func (export "i32_load16_u_out_of_range") (result i32)
    i32.const 0
    i32.load16_u offset=20000
  )

  (func (export "i64_load8_s_in_range") (result i64)
    i32.const 0
    i64.load8_s offset=100
  )
  
  (func (export "i64_load8_s_odd") (result i64)
    i32.const 0
    i64.load8_s offset=101
  )
  
  (func (export "i64_load8_s_out_of_range") (result i64)
    i32.const 0
    i64.load8_s offset=5000
  )

  (func (export "i64_load8_u_in_range") (result i64)
    i32.const 0
    i64.load8_u offset=100
  )
  
  (func (export "i64_load8_u_odd") (result i64)
    i32.const 0
    i64.load8_u offset=101
  )
  
  (func (export "i64_load8_u_out_of_range") (result i64)
    i32.const 0
    i64.load8_u offset=5000
  )

  (func (export "i64_load16_s_aligned") (result i64)
    i32.const 0
    i64.load16_s offset=100
  )
  
  (func (export "i64_load16_s_misaligned") (result i64)
    i32.const 0
    i64.load16_s offset=101
  )
  
  (func (export "i64_load16_s_out_of_range") (result i64)
    i32.const 0
    i64.load16_s offset=20000
  )

  (func (export "i64_load16_u_aligned") (result i64)
    i32.const 0
    i64.load16_u offset=100
  )
  
  (func (export "i64_load16_u_misaligned") (result i64)
    i32.const 0
    i64.load16_u offset=101
  )
  
  (func (export "i64_load16_u_out_of_range") (result i64)
    i32.const 0
    i64.load16_u offset=20000
  )

  (func (export "i64_load32_s_aligned") (result i64)
    i32.const 0
    i64.load32_s offset=100
  )
  
  (func (export "i64_load32_s_misaligned") (result i64)
    i32.const 0
    i64.load32_s offset=101
  )
  
  (func (export "i64_load32_s_out_of_range") (result i64)
    i32.const 0
    i64.load32_s offset=20000
  )

  (func (export "i64_load32_u_aligned") (result i64)
    i32.const 0
    i64.load32_u offset=100
  )
  
  (func (export "i64_load32_u_misaligned") (result i64)
    i32.const 0
    i64.load32_u offset=101
  )
  
  (func (export "i64_load32_u_out_of_range") (result i64)
    i32.const 0
    i64.load32_u offset=20000
  )
)

(assert_return (invoke "i32_load_aligned") (i32.const 0x00807fff))
(assert_return (invoke "i32_load_misaligned") (i32.const 0xff00807f))
(assert_return (invoke "i32_load_out_of_range") (i32.const 0x7fffffff))

(assert_return (invoke "i64_load_aligned") (i64.const 0x7fffffffffffffff))
(assert_return (invoke "i64_load_misaligned") (i64.const 0x007fffffffffffff))
(assert_return (invoke "i64_load_out_of_range") (i64.const 0x7fffffffffffffff))

(assert_return (invoke "i32_load8_s_in_range") (i32.const -1))
(assert_return (invoke "i32_load8_s_odd") (i32.const 127))
(assert_return (invoke "i32_load8_s_out_of_range") (i32.const -128))

(assert_return (invoke "i32_load8_u_in_range") (i32.const 255))
(assert_return (invoke "i32_load8_u_odd") (i32.const 127))
(assert_return (invoke "i32_load8_u_out_of_range") (i32.const 128))

(assert_return (invoke "i32_load16_s_aligned") (i32.const 32767))
(assert_return (invoke "i32_load16_s_misaligned") (i32.const -32641))
(assert_return (invoke "i32_load16_s_out_of_range") (i32.const -1))

(assert_return (invoke "i32_load16_u_aligned") (i32.const 32767))
(assert_return (invoke "i32_load16_u_misaligned") (i32.const 32895))
(assert_return (invoke "i32_load16_u_out_of_range") (i32.const 65535))

(assert_return (invoke "i64_load8_s_in_range") (i64.const -1))
(assert_return (invoke "i64_load8_s_odd") (i64.const 127))
(assert_return (invoke "i64_load8_s_out_of_range") (i64.const -128))

(assert_return (invoke "i64_load8_u_in_range") (i64.const 255))
(assert_return (invoke "i64_load8_u_odd") (i64.const 127))
(assert_return (invoke "i64_load8_u_out_of_range") (i64.const 128))

(assert_return (invoke "i64_load16_s_aligned") (i64.const 32767))
(assert_return (invoke "i64_load16_s_misaligned") (i64.const -32641))
(assert_return (invoke "i64_load16_s_out_of_range") (i64.const -1))

(assert_return (invoke "i64_load16_u_aligned") (i64.const 32767))
(assert_return (invoke "i64_load16_u_misaligned") (i64.const 32895))
(assert_return (invoke "i64_load16_u_out_of_range") (i64.const 65535))

(assert_return (invoke "i64_load32_s_aligned") (i64.const 0x807fff))
(assert_return (invoke "i64_load32_s_misaligned") (i64.const 0xffffffffff00807f))
(assert_return (invoke "i64_load32_s_out_of_range") (i64.const 0x7fffffff))

(assert_return (invoke "i64_load32_u_aligned") (i64.const 0x807fff))
(assert_return (invoke "i64_load32_u_misaligned") (i64.const 0xff00807f))
(assert_return (invoke "i64_load32_u_out_of_range") (i64.const 0x7fffffff))

(module
  (memory 1)
  
  (func (export "i32_store_aligned") (result i32)
    i32.const 0
    i32.const 0x12345678
    i32.store offset=200
    i32.const 0
    i32.load offset=200
  )
  
  (func (export "i32_store_misaligned") (result i32)
    i32.const 0
    i32.const 0xabcdef01
    i32.store offset=201
    i32.const 0
    i32.load offset=201
  )
  
  (func (export "i32_store_out_of_range") (result i32)
    i32.const 0
    i32.const 0xdeadbeef
    i32.store offset=30000
    i32.const 0
    i32.load offset=30000
  )

  (func (export "i64_store_aligned") (result i64)
    i32.const 0
    i64.const 0x123456789abcdef0
    i64.store offset=4200
    i32.const 0
    i64.load offset=4200
  )
  
  (func (export "i64_store_misaligned") (result i64)
    i32.const 0
    i64.const 0xfedcba9876543210
    i64.store offset=4201
    i32.const 0
    i64.load offset=4201
  )
  
  (func (export "i64_store_out_of_range") (result i64)
    i32.const 0
    i64.const 0x0123456789abcdef
    i64.store offset=50000
    i32.const 0
    i64.load offset=50000
  )

  (func (export "i32_store8_in_range") (result i32)
    i32.const 0
    i32.const 0xff
    i32.store8 offset=300
    i32.const 0
    i32.load8_u offset=300
  )
  
  (func (export "i32_store8_odd") (result i32)
    i32.const 0
    i32.const 0x7f
    i32.store8 offset=301
    i32.const 0
    i32.load8_u offset=301
  )
  
  (func (export "i32_store8_out_of_range") (result i32)
    i32.const 0
    i32.const 0x80
    i32.store8 offset=6000
    i32.const 0
    i32.load8_u offset=6000
  )

  (func (export "i32_store16_aligned") (result i32)
    i32.const 0
    i32.const 0xabcd
    i32.store16 offset=400
    i32.const 0
    i32.load16_u offset=400
  )
  
  (func (export "i32_store16_misaligned") (result i32)
    i32.const 0
    i32.const 0x1234
    i32.store16 offset=401
    i32.const 0
    i32.load16_u offset=401
  )
  
  (func (export "i32_store16_out_of_range") (result i32)
    i32.const 0
    i32.const 0xfeed
    i32.store16 offset=25000
    i32.const 0
    i32.load16_u offset=25000
  )

  (func (export "i64_store8_in_range") (result i64)
    i32.const 0
    i64.const 0xff
    i64.store8 offset=500
    i32.const 0
    i64.load8_u offset=500
  )
  
  (func (export "i64_store8_odd") (result i64)
    i32.const 0
    i64.const 0x42
    i64.store8 offset=501
    i32.const 0
    i64.load8_u offset=501
  )
  
  (func (export "i64_store8_out_of_range") (result i64)
    i32.const 0
    i64.const 0x99
    i64.store8 offset=7000
    i32.const 0
    i64.load8_u offset=7000
  )

  (func (export "i64_store16_aligned") (result i64)
    i32.const 0
    i64.const 0x5678
    i64.store16 offset=600
    i32.const 0
    i64.load16_u offset=600
  )
  
  (func (export "i64_store16_misaligned") (result i64)
    i32.const 0
    i64.const 0x9abc
    i64.store16 offset=601
    i32.const 0
    i64.load16_u offset=601
  )
  
  (func (export "i64_store16_out_of_range") (result i64)
    i32.const 0
    i64.const 0xcafe
    i64.store16 offset=26000
    i32.const 0
    i64.load16_u offset=26000
  )

  (func (export "i64_store32_aligned") (result i64)
    i32.const 0
    i64.const 0x87654321
    i64.store32 offset=700
    i32.const 0
    i64.load32_u offset=700
  )
  
  (func (export "i64_store32_misaligned") (result i64)
    i32.const 0
    i64.const 0xfedcba98
    i64.store32 offset=701
    i32.const 0
    i64.load32_u offset=701
  )
  
  (func (export "i64_store32_out_of_range") (result i64)
    i32.const 0
    i64.const 0xbaadf00d
    i64.store32 offset=35000
    i32.const 0
    i64.load32_u offset=35000
  )
)

(assert_return (invoke "i32_store_aligned") (i32.const 0x12345678))
(assert_return (invoke "i32_store_misaligned") (i32.const 0xabcdef01))
(assert_return (invoke "i32_store_out_of_range") (i32.const 0xdeadbeef))

(assert_return (invoke "i64_store_aligned") (i64.const 0x123456789abcdef0))
(assert_return (invoke "i64_store_misaligned") (i64.const 0xfedcba9876543210))
(assert_return (invoke "i64_store_out_of_range") (i64.const 0x0123456789abcdef))

(assert_return (invoke "i32_store8_in_range") (i32.const 0xff))
(assert_return (invoke "i32_store8_odd") (i32.const 0x7f))
(assert_return (invoke "i32_store8_out_of_range") (i32.const 0x80))

(assert_return (invoke "i32_store16_aligned") (i32.const 0xabcd))
(assert_return (invoke "i32_store16_misaligned") (i32.const 0x1234))
(assert_return (invoke "i32_store16_out_of_range") (i32.const 0xfeed))

(assert_return (invoke "i64_store8_in_range") (i64.const 0xff))
(assert_return (invoke "i64_store8_odd") (i64.const 0x42))
(assert_return (invoke "i64_store8_out_of_range") (i64.const 0x99))

(assert_return (invoke "i64_store16_aligned") (i64.const 0x5678))
(assert_return (invoke "i64_store16_misaligned") (i64.const 0x9abc))
(assert_return (invoke "i64_store16_out_of_range") (i64.const 0xcafe))

(assert_return (invoke "i64_store32_aligned") (i64.const 0x87654321))
(assert_return (invoke "i64_store32_misaligned") (i64.const 0xfedcba98))
(assert_return (invoke "i64_store32_out_of_range") (i64.const 0xbaadf00d))

(module
  (memory 1)
  
  (data (i32.const 0) "\ff\fe\80\81\00\00\00\80\00\00\00\00\ff\ff\ff\ff")
  (data (i32.const 100) "\ff\7f\80\00\ff\ff\00\80\00\00\00\00\ff\ff\ff\7f")
  (data (i32.const 4000) "\ff\ff\ff\ff\ff\ff\ff\7f\00\00\00\00\00\00\00\80")
  
  (func (export "i32_load_max_imm12") (result i32)
    i32.const 0
    i32.load offset=4092
  )
  
  (func (export "i32_load_max_imm12_unaligned") (result i32)
    i32.const 0
    i32.load offset=4093
  )
  
  (func (export "i32_load_beyond_imm12") (result i32)
    i32.const 0
    i32.load offset=4096
  )
  
  (func (export "i64_load_max_imm12") (result i64)
    i32.const 0
    i64.load offset=4088
  )
  
  (func (export "i64_load_max_imm12_unaligned") (result i64)
    i32.const 0
    i64.load offset=4089
  )
  
  (func (export "i64_load_beyond_imm12") (result i64)
    i32.const 0
    i64.load offset=4096
  )

  (func (export "f32_load_max_imm12") (result f32)
    i32.const 0
    f32.load offset=4092
  )
  
  (func (export "f32_load_max_imm12_unaligned") (result f32)
    i32.const 0
    f32.load offset=4093
  )
  
  (func (export "f32_load_beyond_imm12") (result f32)
    i32.const 0
    f32.load offset=4096
  )

  (func (export "f64_load_max_imm12") (result f64)
    i32.const 0
    f64.load offset=4088
  )
  
  (func (export "f64_load_max_imm12_unaligned") (result f64)
    i32.const 0
    f64.load offset=4089
  )
  
  (func (export "f64_load_beyond_imm12") (result f64)
    i32.const 0
    f64.load offset=4096
  )

  (func (export "i32_load8_s_max_imm12") (result i32)
    i32.const 0
    i32.load8_s offset=4095
  )
  
  (func (export "i32_load8_s_beyond_imm12") (result i32)
    i32.const 0
    i32.load8_s offset=4096
  )

  (func (export "i32_load8_u_max_imm12") (result i32)
    i32.const 0
    i32.load8_u offset=4095
  )
  
  (func (export "i32_load8_u_beyond_imm12") (result i32)
    i32.const 0
    i32.load8_u offset=4096
  )

  (func (export "i32_load16_s_max_imm12") (result i32)
    i32.const 0
    i32.load16_s offset=4094
  )
  
  (func (export "i32_load16_s_max_imm12_unaligned") (result i32)
    i32.const 0
    i32.load16_s offset=4095
  )
  
  (func (export "i32_load16_s_beyond_imm12") (result i32)
    i32.const 0
    i32.load16_s offset=4096
  )

  (func (export "i32_load16_u_max_imm12") (result i32)
    i32.const 0
    i32.load16_u offset=4094
  )
  
  (func (export "i32_load16_u_max_imm12_unaligned") (result i32)
    i32.const 0
    i32.load16_u offset=4095
  )
  
  (func (export "i32_load16_u_beyond_imm12") (result i32)
    i32.const 0
    i32.load16_u offset=4096
  )

  (func (export "i64_load8_s_max_imm12") (result i64)
    i32.const 0
    i64.load8_s offset=4095
  )
  
  (func (export "i64_load8_s_beyond_imm12") (result i64)
    i32.const 0
    i64.load8_s offset=4096
  )

  (func (export "i64_load8_u_max_imm12") (result i64)
    i32.const 0
    i64.load8_u offset=4095
  )
  
  (func (export "i64_load8_u_beyond_imm12") (result i64)
    i32.const 0
    i64.load8_u offset=4096
  )

  (func (export "i64_load16_s_max_imm12") (result i64)
    i32.const 0
    i64.load16_s offset=4094
  )
  
  (func (export "i64_load16_s_max_imm12_unaligned") (result i64)
    i32.const 0
    i64.load16_s offset=4095
  )
  
  (func (export "i64_load16_s_beyond_imm12") (result i64)
    i32.const 0
    i64.load16_s offset=4096
  )

  (func (export "i64_load16_u_max_imm12") (result i64)
    i32.const 0
    i64.load16_u offset=4094
  )
  
  (func (export "i64_load16_u_max_imm12_unaligned") (result i64)
    i32.const 0
    i64.load16_u offset=4095
  )
  
  (func (export "i64_load16_u_beyond_imm12") (result i64)
    i32.const 0
    i64.load16_u offset=4096
  )

  (func (export "i64_load32_s_max_imm12") (result i64)
    i32.const 0
    i64.load32_s offset=4092
  )
  
  (func (export "i64_load32_s_max_imm12_unaligned") (result i64)
    i32.const 0
    i64.load32_s offset=4093
  )
  
  (func (export "i64_load32_s_beyond_imm12") (result i64)
    i32.const 0
    i64.load32_s offset=4096
  )

  (func (export "i64_load32_u_max_imm12") (result i64)
    i32.const 0
    i64.load32_u offset=4092
  )
  
  (func (export "i64_load32_u_max_imm12_unaligned") (result i64)
    i32.const 0
    i64.load32_u offset=4093
  )
  
  (func (export "i64_load32_u_beyond_imm12") (result i64)
    i32.const 0
    i64.load32_u offset=4096
  )
)

(assert_return (invoke "i32_load_max_imm12") (i32.const 0))
(assert_return (invoke "i32_load_max_imm12_unaligned") (i32.const 0))
(assert_return (invoke "i32_load_beyond_imm12") (i32.const 0))

(assert_return (invoke "i64_load_max_imm12") (i64.const 0))
(assert_return (invoke "i64_load_max_imm12_unaligned") (i64.const 0))
(assert_return (invoke "i64_load_beyond_imm12") (i64.const 0))

(assert_return (invoke "f32_load_max_imm12") (f32.const 0))
(assert_return (invoke "f32_load_max_imm12_unaligned") (f32.const 0))
(assert_return (invoke "f32_load_beyond_imm12") (f32.const 0))

(assert_return (invoke "f64_load_max_imm12") (f64.const 0))
(assert_return (invoke "f64_load_max_imm12_unaligned") (f64.const 0))
(assert_return (invoke "f64_load_beyond_imm12") (f64.const 0))

(assert_return (invoke "i32_load8_s_max_imm12") (i32.const 0))
(assert_return (invoke "i32_load8_s_beyond_imm12") (i32.const 0))

(assert_return (invoke "i32_load8_u_max_imm12") (i32.const 0))
(assert_return (invoke "i32_load8_u_beyond_imm12") (i32.const 0))

(assert_return (invoke "i32_load16_s_max_imm12") (i32.const 0))
(assert_return (invoke "i32_load16_s_max_imm12_unaligned") (i32.const 0))
(assert_return (invoke "i32_load16_s_beyond_imm12") (i32.const 0))

(assert_return (invoke "i32_load16_u_max_imm12") (i32.const 0))
(assert_return (invoke "i32_load16_u_max_imm12_unaligned") (i32.const 0))
(assert_return (invoke "i32_load16_u_beyond_imm12") (i32.const 0))

(assert_return (invoke "i64_load8_s_max_imm12") (i64.const 0))
(assert_return (invoke "i64_load8_s_beyond_imm12") (i64.const 0))

(assert_return (invoke "i64_load8_u_max_imm12") (i64.const 0))
(assert_return (invoke "i64_load8_u_beyond_imm12") (i64.const 0))

(assert_return (invoke "i64_load16_s_max_imm12") (i64.const 0))
(assert_return (invoke "i64_load16_s_max_imm12_unaligned") (i64.const 0))
(assert_return (invoke "i64_load16_s_beyond_imm12") (i64.const 0))

(assert_return (invoke "i64_load16_u_max_imm12") (i64.const 0))
(assert_return (invoke "i64_load16_u_max_imm12_unaligned") (i64.const 0))
(assert_return (invoke "i64_load16_u_beyond_imm12") (i64.const 0))

(assert_return (invoke "i64_load32_s_max_imm12") (i64.const 0))
(assert_return (invoke "i64_load32_s_max_imm12_unaligned") (i64.const 0))
(assert_return (invoke "i64_load32_s_beyond_imm12") (i64.const 0))

(assert_return (invoke "i64_load32_u_max_imm12") (i64.const 0))
(assert_return (invoke "i64_load32_u_max_imm12_unaligned") (i64.const 0))
(assert_return (invoke "i64_load32_u_beyond_imm12") (i64.const 0))

(module
  (memory 1)
  
  (func (export "i32_store_max_imm12") (result i32)
    i32.const 0
    i32.const 0xabcdef01
    i32.store offset=4092
    i32.const 0
    i32.load offset=4092
  )
  
  (func (export "i32_store_max_imm12_unaligned") (result i32)
    i32.const 0
    i32.const 0x11223344
    i32.store offset=4093
    i32.const 0
    i32.load offset=4093
  )
  
  (func (export "i32_store_beyond_imm12") (result i32)
    i32.const 0
    i32.const 0xdeadbeef
    i32.store offset=4096
    i32.const 0
    i32.load offset=4096
  )

  (func (export "i64_store_max_imm12") (result i64)
    i32.const 0
    i64.const 0xfedcba9876543210
    i64.store offset=4088
    i32.const 0
    i64.load offset=4088
  )
  
  (func (export "i64_store_max_imm12_unaligned") (result i64)
    i32.const 0
    i64.const 0x1122334455667788
    i64.store offset=4089
    i32.const 0
    i64.load offset=4089
  )
  
  (func (export "i64_store_beyond_imm12") (result i64)
    i32.const 0
    i64.const 0x0123456789abcdef
    i64.store offset=4096
    i32.const 0
    i64.load offset=4096
  )

  (func (export "i32_store8_max_imm12") (result i32)
    i32.const 0
    i32.const 0x7f
    i32.store8 offset=4095
    i32.const 0
    i32.load8_u offset=4095
  )
  
  (func (export "i32_store8_beyond_imm12") (result i32)
    i32.const 0
    i32.const 0x80
    i32.store8 offset=4096
    i32.const 0
    i32.load8_u offset=4096
  )

  (func (export "i32_store16_max_imm12") (result i32)
    i32.const 0
    i32.const 0x1234
    i32.store16 offset=4094
    i32.const 0
    i32.load16_u offset=4094
  )
  
  (func (export "i32_store16_max_imm12_unaligned") (result i32)
    i32.const 0
    i32.const 0x5678
    i32.store16 offset=4095
    i32.const 0
    i32.load16_u offset=4095
  )
  
  (func (export "i32_store16_beyond_imm12") (result i32)
    i32.const 0
    i32.const 0xfeed
    i32.store16 offset=4096
    i32.const 0
    i32.load16_u offset=4096
  )

  (func (export "i64_store8_max_imm12") (result i64)
    i32.const 0
    i64.const 0x42
    i64.store8 offset=4095
    i32.const 0
    i64.load8_u offset=4095
  )
  
  (func (export "i64_store8_beyond_imm12") (result i64)
    i32.const 0
    i64.const 0x99
    i64.store8 offset=4096
    i32.const 0
    i64.load8_u offset=4096
  )

  (func (export "i64_store16_max_imm12") (result i64)
    i32.const 0
    i64.const 0x9abc
    i64.store16 offset=4094
    i32.const 0
    i64.load16_u offset=4094
  )
  
  (func (export "i64_store16_max_imm12_unaligned") (result i64)
    i32.const 0
    i64.const 0xdef0
    i64.store16 offset=4095
    i32.const 0
    i64.load16_u offset=4095
  )
  
  (func (export "i64_store16_beyond_imm12") (result i64)
    i32.const 0
    i64.const 0xcafe
    i64.store16 offset=4096
    i32.const 0
    i64.load16_u offset=4096
  )

  (func (export "i64_store32_max_imm12") (result i64)
    i32.const 0
    i64.const 0xfedcba98
    i64.store32 offset=4092
    i32.const 0
    i64.load32_u offset=4092
  )
  
  (func (export "i64_store32_max_imm12_unaligned") (result i64)
    i32.const 0
    i64.const 0x12345678
    i64.store32 offset=4093
    i32.const 0
    i64.load32_u offset=4093
  )
  
  (func (export "i64_store32_beyond_imm12") (result i64)
    i32.const 0
    i64.const 0xbaadf00d
    i64.store32 offset=4096
    i32.const 0
    i64.load32_u offset=4096
  )
)

(assert_return (invoke "i32_store_max_imm12") (i32.const 0xabcdef01))
(assert_return (invoke "i32_store_max_imm12_unaligned") (i32.const 0x11223344))
(assert_return (invoke "i32_store_beyond_imm12") (i32.const 0xdeadbeef))

(assert_return (invoke "i64_store_max_imm12") (i64.const 0xfedcba9876543210))
(assert_return (invoke "i64_store_max_imm12_unaligned") (i64.const 0x1122334455667788))
(assert_return (invoke "i64_store_beyond_imm12") (i64.const 0x0123456789abcdef))

(assert_return (invoke "i32_store8_max_imm12") (i32.const 0x7f))
(assert_return (invoke "i32_store8_beyond_imm12") (i32.const 0x80))

(assert_return (invoke "i32_store16_max_imm12") (i32.const 0x1234))
(assert_return (invoke "i32_store16_max_imm12_unaligned") (i32.const 0x5678))
(assert_return (invoke "i32_store16_beyond_imm12") (i32.const 0xfeed))

(assert_return (invoke "i64_store8_max_imm12") (i64.const 0x42))
(assert_return (invoke "i64_store8_beyond_imm12") (i64.const 0x99))

(assert_return (invoke "i64_store16_max_imm12") (i64.const 0x9abc))
(assert_return (invoke "i64_store16_max_imm12_unaligned") (i64.const 0xdef0))
(assert_return (invoke "i64_store16_beyond_imm12") (i64.const 0xcafe))

(assert_return (invoke "i64_store32_max_imm12") (i64.const 0xfedcba98))
(assert_return (invoke "i64_store32_max_imm12_unaligned") (i64.const 0x12345678))
(assert_return (invoke "i64_store32_beyond_imm12") (i64.const 0xbaadf00d))
