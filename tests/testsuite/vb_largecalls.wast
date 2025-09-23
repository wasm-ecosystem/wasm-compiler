(module
  (type $I (func (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)))
  (type $F (func (param f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32) (result f32)))
  (type $LastI (func (param f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32) (result i32)))
  (type $LastF (func (param i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 i32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32 f32) (result f32)))
  (type $MixedI (func (param i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 ) (result i32)))
  (type $MixedF (func (param i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 i32 f32 ) (result f32)))

  (type $MixedFD_F (func (param f64 f32 f64 f32 f32 f64 f32 f32 f32 f64 f32 f32 f32 f32 f64 f64 f32 f64 f32 f32 f64 f32 f32 f32 f64 f32 f32 f32 f32 f64) (result f32)))
  (type $MixedFD_D (func (param f64 f32 f64 f32 f32 f64 f32 f32 f32 f64 f32 f32 f32 f32 f64 f64 f32 f64 f32 f32 f64 f32 f32 f32 f64 f32 f32 f32 f32 f64) (result f64)))
  (type $MixedDF_F (func (param f32 f64 f32 f64 f64 f32 f64 f64 f64 f32 f64 f64 f64 f64 f32 f32 f64 f32 f64 f64 f32 f64 f64 f64 f32 f64 f64 f64 f64 f32) (result f32)))
  (type $MixedDF_D (func (param f32 f64 f32 f64 f64 f32 f64 f64 f64 f32 f64 f64 f64 f64 f32 f32 f64 f32 f64 f64 f32 f64 f64 f64 f32 f64 f64 f64 f64 f32) (result f64)))
  (type $MixedIL_I (func (param i64 i32 i64 i32 i32 i64 i32 i32 i32 i64 i32 i32 i32 i32 i64 i64 i32 i64 i32 i32 i64 i32 i32 i32 i64 i32 i32 i32 i32 i64) (result i32)))
  (type $MixedIL_L (func (param i64 i32 i64 i32 i32 i64 i32 i32 i32 i64 i32 i32 i32 i32 i64 i64 i32 i64 i32 i32 i64 i32 i32 i32 i64 i32 i32 i32 i32 i64) (result i64)))
  (type $MixedLI_I (func (param i32 i64 i32 i64 i64 i32 i64 i64 i64 i32 i64 i64 i64 i64 i32 i32 i64 i32 i64 i64 i32 i64 i64 i64 i32 i64 i64 i64 i64 i32) (result i32)))
  (type $MixedLI_L (func (param i32 i64 i32 i64 i64 i32 i64 i64 i64 i32 i64 i64 i64 i64 i32 i32 i64 i32 i64 i64 i32 i64 i64 i64 i32 i64 i64 i64 i64 i32) (result i64)))

  (import "spectest" "sumI" (func $sumI_import (type $I)))
  (import "spectest" "sumF" (func $sumF_import (type $F)))
  (import "spectest" "sumLastI" (func $sumLastI_import (type $LastI)))
  (import "spectest" "sumLastF" (func $sumLastF_import (type $LastF)))
  (import "spectest" "sumMixedI" (func $sumMixedI_import (type $MixedI)))
  (import "spectest" "sumMixedF" (func $sumMixedF_import (type $MixedF)))

  (import "spectest" "sumMixedFD_F" (func $sumMixedFD_F_import (type $MixedFD_F)))
  (import "spectest" "sumMixedFD_D" (func $sumMixedFD_D_import (type $MixedFD_D)))
  (import "spectest" "sumMixedDF_F" (func $sumMixedDF_F_import (type $MixedDF_F)))
  (import "spectest" "sumMixedDF_D" (func $sumMixedDF_D_import (type $MixedDF_D)))
  (import "spectest" "sumMixedIL_I" (func $sumMixedIL_I_import (type $MixedIL_I)))
  (import "spectest" "sumMixedIL_L" (func $sumMixedIL_L_import (type $MixedIL_L)))
  (import "spectest" "sumMixedLI_I" (func $sumMixedLI_I_import (type $MixedLI_I)))
  (import "spectest" "sumMixedLI_L" (func $sumMixedLI_L_import (type $MixedLI_L)))

  (func (export "sumI_import_wrapper")  (result i32)
    i32.const 1
    i32.const 2
    i32.const 3
    i32.const 4
    i32.const 5
    i32.const 6
    i32.const 7
    i32.const 8
    i32.const 9
    i32.const 10
    i32.const 11
    i32.const 12
    i32.const 13
    i32.const 14
    i32.const 15
    i32.const 16
    i32.const 17
    i32.const 18
    i32.const 19
    i32.const 20
    i32.const 21
    i32.const 22
    i32.const 23
    i32.const 24
    i32.const 25
    i32.const 26
    i32.const 27
    i32.const 28
    i32.const 29
    i32.const 30
    i32.const 31
    i32.const 32
    i32.const 33
    i32.const 34
    i32.const 35
    i32.const 36
    i32.const 37
    i32.const 38
    i32.const 39
    i32.const 40
    i32.const 41
    i32.const 42
    i32.const 43
    i32.const 44
    i32.const 45
    i32.const 46
    i32.const 47
    i32.const 48
    i32.const 49
    i32.const 50
    call $sumI_import
  )

  (func (export "sumF_import_wrapper")  (result f32)
    f32.const 1.0
    f32.const 2.0
    f32.const 3.0
    f32.const 4.0
    f32.const 5.0
    f32.const 6.0
    f32.const 7.0
    f32.const 8.0
    f32.const 9.0
    f32.const 10.0
    f32.const 11.0
    f32.const 12.0
    f32.const 13.0
    f32.const 14.0
    f32.const 15.0
    f32.const 16.0
    f32.const 17.0
    f32.const 18.0
    f32.const 19.0
    f32.const 20.0
    f32.const 21.0
    f32.const 22.0
    f32.const 23.0
    f32.const 24.0
    f32.const 25.0
    f32.const 26.0
    f32.const 27.0
    f32.const 28.0
    f32.const 29.0
    f32.const 30.0
    f32.const 31.0
    f32.const 32.0
    f32.const 33.0
    f32.const 34.0
    f32.const 35.0
    f32.const 36.0
    f32.const 37.0
    f32.const 38.0
    f32.const 39.0
    f32.const 40.0
    f32.const 41.0
    f32.const 42.0
    f32.const 43.0
    f32.const 44.0
    f32.const 45.0
    f32.const 46.0
    f32.const 47.0
    f32.const 48.0
    f32.const 49.0
    f32.const 50.0
    call $sumF_import
  )

  (func (export "sumLastI_import_wrapper")  (result i32)
    f32.const 1.0
    f32.const 2.0
    f32.const 3.0
    f32.const 4.0
    f32.const 5.0
    f32.const 6.0
    f32.const 7.0
    f32.const 8.0
    f32.const 9.0
    f32.const 10.0
    f32.const 11.0
    f32.const 12.0
    f32.const 13.0
    f32.const 14.0
    f32.const 15.0
    f32.const 16.0
    f32.const 17.0
    f32.const 18.0
    f32.const 19.0
    f32.const 20.0
    f32.const 21.0
    f32.const 22.0
    f32.const 23.0
    f32.const 24.0
    f32.const 25.0
    f32.const 26.0
    f32.const 27.0
    f32.const 28.0
    f32.const 29.0
    f32.const 30.0
    f32.const 31.0
    f32.const 32.0
    f32.const 33.0
    f32.const 34.0
    f32.const 35.0
    f32.const 36.0
    f32.const 37.0
    f32.const 38.0
    f32.const 39.0
    f32.const 40.0
    f32.const 41.0
    f32.const 42.0
    f32.const 43.0
    f32.const 44.0
    f32.const 45.0
    f32.const 46.0
    f32.const 47.0
    f32.const 48.0
    f32.const 49.0
    f32.const 50.0
    i32.const 1
    i32.const 2
    i32.const 3
    i32.const 4
    i32.const 5
    i32.const 6
    i32.const 7
    i32.const 8
    i32.const 9
    i32.const 10
    i32.const 11
    i32.const 12
    i32.const 13
    i32.const 14
    i32.const 15
    i32.const 16
    i32.const 17
    i32.const 18
    i32.const 19
    i32.const 20
    i32.const 21
    i32.const 22
    i32.const 23
    i32.const 24
    i32.const 25
    i32.const 26
    i32.const 27
    i32.const 28
    i32.const 29
    i32.const 30
    i32.const 31
    i32.const 32
    i32.const 33
    i32.const 34
    i32.const 35
    i32.const 36
    i32.const 37
    i32.const 38
    i32.const 39
    i32.const 40
    i32.const 41
    i32.const 42
    i32.const 43
    i32.const 44
    i32.const 45
    i32.const 46
    i32.const 47
    i32.const 48
    i32.const 49
    i32.const 50
    call $sumLastI_import
  )

  (func (export "sumLastF_import_wrapper")  (result f32)
    i32.const 1
    i32.const 2
    i32.const 3
    i32.const 4
    i32.const 5
    i32.const 6
    i32.const 7
    i32.const 8
    i32.const 9
    i32.const 10
    i32.const 11
    i32.const 12
    i32.const 13
    i32.const 14
    i32.const 15
    i32.const 16
    i32.const 17
    i32.const 18
    i32.const 19
    i32.const 20
    i32.const 21
    i32.const 22
    i32.const 23
    i32.const 24
    i32.const 25
    i32.const 26
    i32.const 27
    i32.const 28
    i32.const 29
    i32.const 30
    i32.const 31
    i32.const 32
    i32.const 33
    i32.const 34
    i32.const 35
    i32.const 36
    i32.const 37
    i32.const 38
    i32.const 39
    i32.const 40
    i32.const 41
    i32.const 42
    i32.const 43
    i32.const 44
    i32.const 45
    i32.const 46
    i32.const 47
    i32.const 48
    i32.const 49
    i32.const 50
    f32.const 1.0
    f32.const 2.0
    f32.const 3.0
    f32.const 4.0
    f32.const 5.0
    f32.const 6.0
    f32.const 7.0
    f32.const 8.0
    f32.const 9.0
    f32.const 10.0
    f32.const 11.0
    f32.const 12.0
    f32.const 13.0
    f32.const 14.0
    f32.const 15.0
    f32.const 16.0
    f32.const 17.0
    f32.const 18.0
    f32.const 19.0
    f32.const 20.0
    f32.const 21.0
    f32.const 22.0
    f32.const 23.0
    f32.const 24.0
    f32.const 25.0
    f32.const 26.0
    f32.const 27.0
    f32.const 28.0
    f32.const 29.0
    f32.const 30.0
    f32.const 31.0
    f32.const 32.0
    f32.const 33.0
    f32.const 34.0
    f32.const 35.0
    f32.const 36.0
    f32.const 37.0
    f32.const 38.0
    f32.const 39.0
    f32.const 40.0
    f32.const 41.0
    f32.const 42.0
    f32.const 43.0
    f32.const 44.0
    f32.const 45.0
    f32.const 46.0
    f32.const 47.0
    f32.const 48.0
    f32.const 49.0
    f32.const 50.0
    call $sumLastF_import
  )

  (func (export "sumMixedI_import_wrapper")  (result i32)
    i32.const 1
    f32.const 1.0
    i32.const 2
    f32.const 2.0
    i32.const 3
    f32.const 3.0
    i32.const 4
    f32.const 4.0
    i32.const 5
    f32.const 5.0
    i32.const 6
    f32.const 6.0
    i32.const 7
    f32.const 7.0
    i32.const 8
    f32.const 8.0
    i32.const 9
    f32.const 9.0
    i32.const 10
    f32.const 10.0
    i32.const 11
    f32.const 11.0
    i32.const 12
    f32.const 12.0
    i32.const 13
    f32.const 13.0
    i32.const 14
    f32.const 14.0
    i32.const 15
    f32.const 15.0
    i32.const 16
    f32.const 16.0
    i32.const 17
    f32.const 17.0
    i32.const 18
    f32.const 18.0
    i32.const 19
    f32.const 19.0
    i32.const 20
    f32.const 20.0
    i32.const 21
    f32.const 21.0
    i32.const 22
    f32.const 22.0
    i32.const 23
    f32.const 23.0
    i32.const 24
    f32.const 24.0
    i32.const 25
    f32.const 25.0
    i32.const 26
    f32.const 26.0
    i32.const 27
    f32.const 27.0
    i32.const 28
    f32.const 28.0
    i32.const 29
    f32.const 29.0
    i32.const 30
    f32.const 30.0
    i32.const 31
    f32.const 31.0
    i32.const 32
    f32.const 32.0
    i32.const 33
    f32.const 33.0
    i32.const 34
    f32.const 34.0
    i32.const 35
    f32.const 35.0
    i32.const 36
    f32.const 36.0
    i32.const 37
    f32.const 37.0
    i32.const 38
    f32.const 38.0
    i32.const 39
    f32.const 39.0
    i32.const 40
    f32.const 40.0
    i32.const 41
    f32.const 41.0
    i32.const 42
    f32.const 42.0
    i32.const 43
    f32.const 43.0
    i32.const 44
    f32.const 44.0
    i32.const 45
    f32.const 45.0
    i32.const 46
    f32.const 46.0
    i32.const 47
    f32.const 47.0
    i32.const 48
    f32.const 48.0
    i32.const 49
    f32.const 49.0
    i32.const 50
    f32.const 50.0
    call $sumMixedI_import
  )

  (func (export "sumMixedF_import_wrapper")  (result f32)
    i32.const 1
    f32.const 1.0
    i32.const 2
    f32.const 2.0
    i32.const 3
    f32.const 3.0
    i32.const 4
    f32.const 4.0
    i32.const 5
    f32.const 5.0
    i32.const 6
    f32.const 6.0
    i32.const 7
    f32.const 7.0
    i32.const 8
    f32.const 8.0
    i32.const 9
    f32.const 9.0
    i32.const 10
    f32.const 10.0
    i32.const 11
    f32.const 11.0
    i32.const 12
    f32.const 12.0
    i32.const 13
    f32.const 13.0
    i32.const 14
    f32.const 14.0
    i32.const 15
    f32.const 15.0
    i32.const 16
    f32.const 16.0
    i32.const 17
    f32.const 17.0
    i32.const 18
    f32.const 18.0
    i32.const 19
    f32.const 19.0
    i32.const 20
    f32.const 20.0
    i32.const 21
    f32.const 21.0
    i32.const 22
    f32.const 22.0
    i32.const 23
    f32.const 23.0
    i32.const 24
    f32.const 24.0
    i32.const 25
    f32.const 25.0
    i32.const 26
    f32.const 26.0
    i32.const 27
    f32.const 27.0
    i32.const 28
    f32.const 28.0
    i32.const 29
    f32.const 29.0
    i32.const 30
    f32.const 30.0
    i32.const 31
    f32.const 31.0
    i32.const 32
    f32.const 32.0
    i32.const 33
    f32.const 33.0
    i32.const 34
    f32.const 34.0
    i32.const 35
    f32.const 35.0
    i32.const 36
    f32.const 36.0
    i32.const 37
    f32.const 37.0
    i32.const 38
    f32.const 38.0
    i32.const 39
    f32.const 39.0
    i32.const 40
    f32.const 40.0
    i32.const 41
    f32.const 41.0
    i32.const 42
    f32.const 42.0
    i32.const 43
    f32.const 43.0
    i32.const 44
    f32.const 44.0
    i32.const 45
    f32.const 45.0
    i32.const 46
    f32.const 46.0
    i32.const 47
    f32.const 47.0
    i32.const 48
    f32.const 48.0
    i32.const 49
    f32.const 49.0
    i32.const 50
    f32.const 50.0
    call $sumMixedF_import
  )

  (func (export "sumI_import_wrapper_indirect")  (result i32)
    i32.const 1
    i32.const 2
    i32.const 3
    i32.const 4
    i32.const 5
    i32.const 6
    i32.const 7
    i32.const 8
    i32.const 9
    i32.const 10
    i32.const 11
    i32.const 12
    i32.const 13
    i32.const 14
    i32.const 15
    i32.const 16
    i32.const 17
    i32.const 18
    i32.const 19
    i32.const 20
    i32.const 21
    i32.const 22
    i32.const 23
    i32.const 24
    i32.const 25
    i32.const 26
    i32.const 27
    i32.const 28
    i32.const 29
    i32.const 30
    i32.const 31
    i32.const 32
    i32.const 33
    i32.const 34
    i32.const 35
    i32.const 36
    i32.const 37
    i32.const 38
    i32.const 39
    i32.const 40
    i32.const 41
    i32.const 42
    i32.const 43
    i32.const 44
    i32.const 45
    i32.const 46
    i32.const 47
    i32.const 48
    i32.const 49
    i32.const 50
	  i32.const 0
    call_indirect (type $I)
  )

  (func (export "sumF_import_wrapper_indirect")  (result f32)
    f32.const 1.0
    f32.const 2.0
    f32.const 3.0
    f32.const 4.0
    f32.const 5.0
    f32.const 6.0
    f32.const 7.0
    f32.const 8.0
    f32.const 9.0
    f32.const 10.0
    f32.const 11.0
    f32.const 12.0
    f32.const 13.0
    f32.const 14.0
    f32.const 15.0
    f32.const 16.0
    f32.const 17.0
    f32.const 18.0
    f32.const 19.0
    f32.const 20.0
    f32.const 21.0
    f32.const 22.0
    f32.const 23.0
    f32.const 24.0
    f32.const 25.0
    f32.const 26.0
    f32.const 27.0
    f32.const 28.0
    f32.const 29.0
    f32.const 30.0
    f32.const 31.0
    f32.const 32.0
    f32.const 33.0
    f32.const 34.0
    f32.const 35.0
    f32.const 36.0
    f32.const 37.0
    f32.const 38.0
    f32.const 39.0
    f32.const 40.0
    f32.const 41.0
    f32.const 42.0
    f32.const 43.0
    f32.const 44.0
    f32.const 45.0
    f32.const 46.0
    f32.const 47.0
    f32.const 48.0
    f32.const 49.0
    f32.const 50.0
	  i32.const 1
    call_indirect (type $F)
  )

  (func (export "sumLastI_import_wrapper_indirect")  (result i32)
    f32.const 1.0
    f32.const 2.0
    f32.const 3.0
    f32.const 4.0
    f32.const 5.0
    f32.const 6.0
    f32.const 7.0
    f32.const 8.0
    f32.const 9.0
    f32.const 10.0
    f32.const 11.0
    f32.const 12.0
    f32.const 13.0
    f32.const 14.0
    f32.const 15.0
    f32.const 16.0
    f32.const 17.0
    f32.const 18.0
    f32.const 19.0
    f32.const 20.0
    f32.const 21.0
    f32.const 22.0
    f32.const 23.0
    f32.const 24.0
    f32.const 25.0
    f32.const 26.0
    f32.const 27.0
    f32.const 28.0
    f32.const 29.0
    f32.const 30.0
    f32.const 31.0
    f32.const 32.0
    f32.const 33.0
    f32.const 34.0
    f32.const 35.0
    f32.const 36.0
    f32.const 37.0
    f32.const 38.0
    f32.const 39.0
    f32.const 40.0
    f32.const 41.0
    f32.const 42.0
    f32.const 43.0
    f32.const 44.0
    f32.const 45.0
    f32.const 46.0
    f32.const 47.0
    f32.const 48.0
    f32.const 49.0
    f32.const 50.0
    i32.const 1
    i32.const 2
    i32.const 3
    i32.const 4
    i32.const 5
    i32.const 6
    i32.const 7
    i32.const 8
    i32.const 9
    i32.const 10
    i32.const 11
    i32.const 12
    i32.const 13
    i32.const 14
    i32.const 15
    i32.const 16
    i32.const 17
    i32.const 18
    i32.const 19
    i32.const 20
    i32.const 21
    i32.const 22
    i32.const 23
    i32.const 24
    i32.const 25
    i32.const 26
    i32.const 27
    i32.const 28
    i32.const 29
    i32.const 30
    i32.const 31
    i32.const 32
    i32.const 33
    i32.const 34
    i32.const 35
    i32.const 36
    i32.const 37
    i32.const 38
    i32.const 39
    i32.const 40
    i32.const 41
    i32.const 42
    i32.const 43
    i32.const 44
    i32.const 45
    i32.const 46
    i32.const 47
    i32.const 48
    i32.const 49
    i32.const 50
  	i32.const 2
    call_indirect (type $LastI)
  )

  (func (export "sumLastF_import_wrapper_indirect")  (result f32)
    i32.const 1
    i32.const 2
    i32.const 3
    i32.const 4
    i32.const 5
    i32.const 6
    i32.const 7
    i32.const 8
    i32.const 9
    i32.const 10
    i32.const 11
    i32.const 12
    i32.const 13
    i32.const 14
    i32.const 15
    i32.const 16
    i32.const 17
    i32.const 18
    i32.const 19
    i32.const 20
    i32.const 21
    i32.const 22
    i32.const 23
    i32.const 24
    i32.const 25
    i32.const 26
    i32.const 27
    i32.const 28
    i32.const 29
    i32.const 30
    i32.const 31
    i32.const 32
    i32.const 33
    i32.const 34
    i32.const 35
    i32.const 36
    i32.const 37
    i32.const 38
    i32.const 39
    i32.const 40
    i32.const 41
    i32.const 42
    i32.const 43
    i32.const 44
    i32.const 45
    i32.const 46
    i32.const 47
    i32.const 48
    i32.const 49
    i32.const 50
    f32.const 1.0
    f32.const 2.0
    f32.const 3.0
    f32.const 4.0
    f32.const 5.0
    f32.const 6.0
    f32.const 7.0
    f32.const 8.0
    f32.const 9.0
    f32.const 10.0
    f32.const 11.0
    f32.const 12.0
    f32.const 13.0
    f32.const 14.0
    f32.const 15.0
    f32.const 16.0
    f32.const 17.0
    f32.const 18.0
    f32.const 19.0
    f32.const 20.0
    f32.const 21.0
    f32.const 22.0
    f32.const 23.0
    f32.const 24.0
    f32.const 25.0
    f32.const 26.0
    f32.const 27.0
    f32.const 28.0
    f32.const 29.0
    f32.const 30.0
    f32.const 31.0
    f32.const 32.0
    f32.const 33.0
    f32.const 34.0
    f32.const 35.0
    f32.const 36.0
    f32.const 37.0
    f32.const 38.0
    f32.const 39.0
    f32.const 40.0
    f32.const 41.0
    f32.const 42.0
    f32.const 43.0
    f32.const 44.0
    f32.const 45.0
    f32.const 46.0
    f32.const 47.0
    f32.const 48.0
    f32.const 49.0
    f32.const 50.0
  	i32.const 3
    call_indirect (type $LastF)
  )

  (func (export "sumMixedI_import_wrapper_indirect")  (result i32)
    i32.const 1
    f32.const 1.0
    i32.const 2
    f32.const 2.0
    i32.const 3
    f32.const 3.0
    i32.const 4
    f32.const 4.0
    i32.const 5
    f32.const 5.0
    i32.const 6
    f32.const 6.0
    i32.const 7
    f32.const 7.0
    i32.const 8
    f32.const 8.0
    i32.const 9
    f32.const 9.0
    i32.const 10
    f32.const 10.0
    i32.const 11
    f32.const 11.0
    i32.const 12
    f32.const 12.0
    i32.const 13
    f32.const 13.0
    i32.const 14
    f32.const 14.0
    i32.const 15
    f32.const 15.0
    i32.const 16
    f32.const 16.0
    i32.const 17
    f32.const 17.0
    i32.const 18
    f32.const 18.0
    i32.const 19
    f32.const 19.0
    i32.const 20
    f32.const 20.0
    i32.const 21
    f32.const 21.0
    i32.const 22
    f32.const 22.0
    i32.const 23
    f32.const 23.0
    i32.const 24
    f32.const 24.0
    i32.const 25
    f32.const 25.0
    i32.const 26
    f32.const 26.0
    i32.const 27
    f32.const 27.0
    i32.const 28
    f32.const 28.0
    i32.const 29
    f32.const 29.0
    i32.const 30
    f32.const 30.0
    i32.const 31
    f32.const 31.0
    i32.const 32
    f32.const 32.0
    i32.const 33
    f32.const 33.0
    i32.const 34
    f32.const 34.0
    i32.const 35
    f32.const 35.0
    i32.const 36
    f32.const 36.0
    i32.const 37
    f32.const 37.0
    i32.const 38
    f32.const 38.0
    i32.const 39
    f32.const 39.0
    i32.const 40
    f32.const 40.0
    i32.const 41
    f32.const 41.0
    i32.const 42
    f32.const 42.0
    i32.const 43
    f32.const 43.0
    i32.const 44
    f32.const 44.0
    i32.const 45
    f32.const 45.0
    i32.const 46
    f32.const 46.0
    i32.const 47
    f32.const 47.0
    i32.const 48
    f32.const 48.0
    i32.const 49
    f32.const 49.0
    i32.const 50
    f32.const 50.0
  	i32.const 4
    call_indirect (type $MixedI)
  )

  (func (export "sumMixedF_import_wrapper_indirect")  (result f32)
    i32.const 1
    f32.const 1.0
    i32.const 2
    f32.const 2.0
    i32.const 3
    f32.const 3.0
    i32.const 4
    f32.const 4.0
    i32.const 5
    f32.const 5.0
    i32.const 6
    f32.const 6.0
    i32.const 7
    f32.const 7.0
    i32.const 8
    f32.const 8.0
    i32.const 9
    f32.const 9.0
    i32.const 10
    f32.const 10.0
    i32.const 11
    f32.const 11.0
    i32.const 12
    f32.const 12.0
    i32.const 13
    f32.const 13.0
    i32.const 14
    f32.const 14.0
    i32.const 15
    f32.const 15.0
    i32.const 16
    f32.const 16.0
    i32.const 17
    f32.const 17.0
    i32.const 18
    f32.const 18.0
    i32.const 19
    f32.const 19.0
    i32.const 20
    f32.const 20.0
    i32.const 21
    f32.const 21.0
    i32.const 22
    f32.const 22.0
    i32.const 23
    f32.const 23.0
    i32.const 24
    f32.const 24.0
    i32.const 25
    f32.const 25.0
    i32.const 26
    f32.const 26.0
    i32.const 27
    f32.const 27.0
    i32.const 28
    f32.const 28.0
    i32.const 29
    f32.const 29.0
    i32.const 30
    f32.const 30.0
    i32.const 31
    f32.const 31.0
    i32.const 32
    f32.const 32.0
    i32.const 33
    f32.const 33.0
    i32.const 34
    f32.const 34.0
    i32.const 35
    f32.const 35.0
    i32.const 36
    f32.const 36.0
    i32.const 37
    f32.const 37.0
    i32.const 38
    f32.const 38.0
    i32.const 39
    f32.const 39.0
    i32.const 40
    f32.const 40.0
    i32.const 41
    f32.const 41.0
    i32.const 42
    f32.const 42.0
    i32.const 43
    f32.const 43.0
    i32.const 44
    f32.const 44.0
    i32.const 45
    f32.const 45.0
    i32.const 46
    f32.const 46.0
    i32.const 47
    f32.const 47.0
    i32.const 48
    f32.const 48.0
    i32.const 49
    f32.const 49.0
    i32.const 50
    f32.const 50.0
	i32.const 5
    call_indirect (type $MixedF)
  )

  (func $sumI (export "sumI") (type $I)
    local.get 0
    local.get 1
    local.get 2
    local.get 3
    local.get 4
    local.get 5
    local.get 6
    local.get 7
    local.get 8
    local.get 9
    local.get 10
    local.get 11
    local.get 12
    local.get 13
    local.get 14
    local.get 15
    local.get 16
    local.get 17
    local.get 18
    local.get 19
    local.get 20
    local.get 21
    local.get 22
    local.get 23
    local.get 24
    local.get 25
    local.get 26
    local.get 27
    local.get 28
    local.get 29
    local.get 30
    local.get 31
    local.get 32
    local.get 33
    local.get 34
    local.get 35
    local.get 36
    local.get 37
    local.get 38
    local.get 39
    local.get 40
    local.get 41
    local.get 42
    local.get 43
    local.get 44
    local.get 45
    local.get 46
    local.get 47
    local.get 48
    local.get 49
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
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
  )

  (func $sumF (export "sumF") (type $F)
    local.get 0
    local.get 1
    local.get 2
    local.get 3
    local.get 4
    local.get 5
    local.get 6
    local.get 7
    local.get 8
    local.get 9
    local.get 10
    local.get 11
    local.get 12
    local.get 13
    local.get 14
    local.get 15
    local.get 16
    local.get 17
    local.get 18
    local.get 19
    local.get 20
    local.get 21
    local.get 22
    local.get 23
    local.get 24
    local.get 25
    local.get 26
    local.get 27
    local.get 28
    local.get 29
    local.get 30
    local.get 31
    local.get 32
    local.get 33
    local.get 34
    local.get 35
    local.get 36
    local.get 37
    local.get 38
    local.get 39
    local.get 40
    local.get 41
    local.get 42
    local.get 43
    local.get 44
    local.get 45
    local.get 46
    local.get 47
    local.get 48
    local.get 49
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
  )

  (func $sumLastI (export "sumLastI") (type $LastI)
    local.get 50
    local.get 51
    local.get 52
    local.get 53
    local.get 54
    local.get 55
    local.get 56
    local.get 57
    local.get 58
    local.get 59
    local.get 60
    local.get 61
    local.get 62
    local.get 63
    local.get 64
    local.get 65
    local.get 66
    local.get 67
    local.get 68
    local.get 69
    local.get 70
    local.get 71
    local.get 72
    local.get 73
    local.get 74
    local.get 75
    local.get 76
    local.get 77
    local.get 78
    local.get 79
    local.get 80
    local.get 81
    local.get 82
    local.get 83
    local.get 84
    local.get 85
    local.get 86
    local.get 87
    local.get 88
    local.get 89
    local.get 90
    local.get 91
    local.get 92
    local.get 93
    local.get 94
    local.get 95
    local.get 96
    local.get 97
    local.get 98
    local.get 99
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
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
  )

  (func $sumLastF (export "sumLastF") (type $LastF)
    local.get 50
    local.get 51
    local.get 52
    local.get 53
    local.get 54
    local.get 55
    local.get 56
    local.get 57
    local.get 58
    local.get 59
    local.get 60
    local.get 61
    local.get 62
    local.get 63
    local.get 64
    local.get 65
    local.get 66
    local.get 67
    local.get 68
    local.get 69
    local.get 70
    local.get 71
    local.get 72
    local.get 73
    local.get 74
    local.get 75
    local.get 76
    local.get 77
    local.get 78
    local.get 79
    local.get 80
    local.get 81
    local.get 82
    local.get 83
    local.get 84
    local.get 85
    local.get 86
    local.get 87
    local.get 88
    local.get 89
    local.get 90
    local.get 91
    local.get 92
    local.get 93
    local.get 94
    local.get 95
    local.get 96
    local.get 97
    local.get 98
    local.get 99
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
  )

  (func $sumMixedI (export "sumMixedI") (type $MixedI)
    local.get 0
    local.get 2
    local.get 4
    local.get 6
    local.get 8
    local.get 10
    local.get 12
    local.get 14
    local.get 16
    local.get 18
    local.get 20
    local.get 22
    local.get 24
    local.get 26
    local.get 28
    local.get 30
    local.get 32
    local.get 34
    local.get 36
    local.get 38
    local.get 40
    local.get 42
    local.get 44
    local.get 46
    local.get 48
    local.get 50
    local.get 52
    local.get 54
    local.get 56
    local.get 58
    local.get 60
    local.get 62
    local.get 64
    local.get 66
    local.get 68
    local.get 70
    local.get 72
    local.get 74
    local.get 76
    local.get 78
    local.get 80
    local.get 82
    local.get 84
    local.get 86
    local.get 88
    local.get 90
    local.get 92
    local.get 94
    local.get 96
    local.get 98
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
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
    i32.add
  )

  (func $sumMixedF (export "sumMixedF") (type $MixedF)
    local.get 1
    local.get 3
    local.get 5
    local.get 7
    local.get 9
    local.get 11
    local.get 13
    local.get 15
    local.get 17
    local.get 19
    local.get 21
    local.get 23
    local.get 25
    local.get 27
    local.get 29
    local.get 31
    local.get 33
    local.get 35
    local.get 37
    local.get 39
    local.get 41
    local.get 43
    local.get 45
    local.get 47
    local.get 49
    local.get 51
    local.get 53
    local.get 55
    local.get 57
    local.get 59
    local.get 61
    local.get 63
    local.get 65
    local.get 67
    local.get 69
    local.get 71
    local.get 73
    local.get 75
    local.get 77
    local.get 79
    local.get 81
    local.get 83
    local.get 85
    local.get 87
    local.get 89
    local.get 91
    local.get 93
    local.get 95
    local.get 97
    local.get 99
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
    f32.add
  )

  (func (export "sumI_wrapper")  (result i32)
    i32.const 1
    i32.const 2
    i32.const 3
    i32.const 4
    i32.const 5
    i32.const 6
    i32.const 7
    i32.const 8
    i32.const 9
    i32.const 10
    i32.const 11
    i32.const 12
    i32.const 13
    i32.const 14
    i32.const 15
    i32.const 16
    i32.const 17
    i32.const 18
    i32.const 19
    i32.const 20
    i32.const 21
    i32.const 22
    i32.const 23
    i32.const 24
    i32.const 25
    i32.const 26
    i32.const 27
    i32.const 28
    i32.const 29
    i32.const 30
    i32.const 31
    i32.const 32
    i32.const 33
    i32.const 34
    i32.const 35
    i32.const 36
    i32.const 37
    i32.const 38
    i32.const 39
    i32.const 40
    i32.const 41
    i32.const 42
    i32.const 43
    i32.const 44
    i32.const 45
    i32.const 46
    i32.const 47
    i32.const 48
    i32.const 49
    i32.const 50
    call $sumI
  )

  (func (export "sumF_wrapper")  (result f32)
    f32.const 1.0
    f32.const 2.0
    f32.const 3.0
    f32.const 4.0
    f32.const 5.0
    f32.const 6.0
    f32.const 7.0
    f32.const 8.0
    f32.const 9.0
    f32.const 10.0
    f32.const 11.0
    f32.const 12.0
    f32.const 13.0
    f32.const 14.0
    f32.const 15.0
    f32.const 16.0
    f32.const 17.0
    f32.const 18.0
    f32.const 19.0
    f32.const 20.0
    f32.const 21.0
    f32.const 22.0
    f32.const 23.0
    f32.const 24.0
    f32.const 25.0
    f32.const 26.0
    f32.const 27.0
    f32.const 28.0
    f32.const 29.0
    f32.const 30.0
    f32.const 31.0
    f32.const 32.0
    f32.const 33.0
    f32.const 34.0
    f32.const 35.0
    f32.const 36.0
    f32.const 37.0
    f32.const 38.0
    f32.const 39.0
    f32.const 40.0
    f32.const 41.0
    f32.const 42.0
    f32.const 43.0
    f32.const 44.0
    f32.const 45.0
    f32.const 46.0
    f32.const 47.0
    f32.const 48.0
    f32.const 49.0
    f32.const 50.0
    call $sumF
  )

  (func (export "sumLastI_wrapper")  (result i32)
    f32.const 1.0
    f32.const 2.0
    f32.const 3.0
    f32.const 4.0
    f32.const 5.0
    f32.const 6.0
    f32.const 7.0
    f32.const 8.0
    f32.const 9.0
    f32.const 10.0
    f32.const 11.0
    f32.const 12.0
    f32.const 13.0
    f32.const 14.0
    f32.const 15.0
    f32.const 16.0
    f32.const 17.0
    f32.const 18.0
    f32.const 19.0
    f32.const 20.0
    f32.const 21.0
    f32.const 22.0
    f32.const 23.0
    f32.const 24.0
    f32.const 25.0
    f32.const 26.0
    f32.const 27.0
    f32.const 28.0
    f32.const 29.0
    f32.const 30.0
    f32.const 31.0
    f32.const 32.0
    f32.const 33.0
    f32.const 34.0
    f32.const 35.0
    f32.const 36.0
    f32.const 37.0
    f32.const 38.0
    f32.const 39.0
    f32.const 40.0
    f32.const 41.0
    f32.const 42.0
    f32.const 43.0
    f32.const 44.0
    f32.const 45.0
    f32.const 46.0
    f32.const 47.0
    f32.const 48.0
    f32.const 49.0
    f32.const 50.0
    i32.const 1
    i32.const 2
    i32.const 3
    i32.const 4
    i32.const 5
    i32.const 6
    i32.const 7
    i32.const 8
    i32.const 9
    i32.const 10
    i32.const 11
    i32.const 12
    i32.const 13
    i32.const 14
    i32.const 15
    i32.const 16
    i32.const 17
    i32.const 18
    i32.const 19
    i32.const 20
    i32.const 21
    i32.const 22
    i32.const 23
    i32.const 24
    i32.const 25
    i32.const 26
    i32.const 27
    i32.const 28
    i32.const 29
    i32.const 30
    i32.const 31
    i32.const 32
    i32.const 33
    i32.const 34
    i32.const 35
    i32.const 36
    i32.const 37
    i32.const 38
    i32.const 39
    i32.const 40
    i32.const 41
    i32.const 42
    i32.const 43
    i32.const 44
    i32.const 45
    i32.const 46
    i32.const 47
    i32.const 48
    i32.const 49
    i32.const 50
    call $sumLastI
  )

  (func (export "sumLastF_wrapper")  (result f32)
    i32.const 1
    i32.const 2
    i32.const 3
    i32.const 4
    i32.const 5
    i32.const 6
    i32.const 7
    i32.const 8
    i32.const 9
    i32.const 10
    i32.const 11
    i32.const 12
    i32.const 13
    i32.const 14
    i32.const 15
    i32.const 16
    i32.const 17
    i32.const 18
    i32.const 19
    i32.const 20
    i32.const 21
    i32.const 22
    i32.const 23
    i32.const 24
    i32.const 25
    i32.const 26
    i32.const 27
    i32.const 28
    i32.const 29
    i32.const 30
    i32.const 31
    i32.const 32
    i32.const 33
    i32.const 34
    i32.const 35
    i32.const 36
    i32.const 37
    i32.const 38
    i32.const 39
    i32.const 40
    i32.const 41
    i32.const 42
    i32.const 43
    i32.const 44
    i32.const 45
    i32.const 46
    i32.const 47
    i32.const 48
    i32.const 49
    i32.const 50
    f32.const 1.0
    f32.const 2.0
    f32.const 3.0
    f32.const 4.0
    f32.const 5.0
    f32.const 6.0
    f32.const 7.0
    f32.const 8.0
    f32.const 9.0
    f32.const 10.0
    f32.const 11.0
    f32.const 12.0
    f32.const 13.0
    f32.const 14.0
    f32.const 15.0
    f32.const 16.0
    f32.const 17.0
    f32.const 18.0
    f32.const 19.0
    f32.const 20.0
    f32.const 21.0
    f32.const 22.0
    f32.const 23.0
    f32.const 24.0
    f32.const 25.0
    f32.const 26.0
    f32.const 27.0
    f32.const 28.0
    f32.const 29.0
    f32.const 30.0
    f32.const 31.0
    f32.const 32.0
    f32.const 33.0
    f32.const 34.0
    f32.const 35.0
    f32.const 36.0
    f32.const 37.0
    f32.const 38.0
    f32.const 39.0
    f32.const 40.0
    f32.const 41.0
    f32.const 42.0
    f32.const 43.0
    f32.const 44.0
    f32.const 45.0
    f32.const 46.0
    f32.const 47.0
    f32.const 48.0
    f32.const 49.0
    f32.const 50.0
    call $sumLastF
  )

  (func (export "sumMixedI_wrapper")  (result i32)
    i32.const 1
    f32.const 1.0
    i32.const 2
    f32.const 2.0
    i32.const 3
    f32.const 3.0
    i32.const 4
    f32.const 4.0
    i32.const 5
    f32.const 5.0
    i32.const 6
    f32.const 6.0
    i32.const 7
    f32.const 7.0
    i32.const 8
    f32.const 8.0
    i32.const 9
    f32.const 9.0
    i32.const 10
    f32.const 10.0
    i32.const 11
    f32.const 11.0
    i32.const 12
    f32.const 12.0
    i32.const 13
    f32.const 13.0
    i32.const 14
    f32.const 14.0
    i32.const 15
    f32.const 15.0
    i32.const 16
    f32.const 16.0
    i32.const 17
    f32.const 17.0
    i32.const 18
    f32.const 18.0
    i32.const 19
    f32.const 19.0
    i32.const 20
    f32.const 20.0
    i32.const 21
    f32.const 21.0
    i32.const 22
    f32.const 22.0
    i32.const 23
    f32.const 23.0
    i32.const 24
    f32.const 24.0
    i32.const 25
    f32.const 25.0
    i32.const 26
    f32.const 26.0
    i32.const 27
    f32.const 27.0
    i32.const 28
    f32.const 28.0
    i32.const 29
    f32.const 29.0
    i32.const 30
    f32.const 30.0
    i32.const 31
    f32.const 31.0
    i32.const 32
    f32.const 32.0
    i32.const 33
    f32.const 33.0
    i32.const 34
    f32.const 34.0
    i32.const 35
    f32.const 35.0
    i32.const 36
    f32.const 36.0
    i32.const 37
    f32.const 37.0
    i32.const 38
    f32.const 38.0
    i32.const 39
    f32.const 39.0
    i32.const 40
    f32.const 40.0
    i32.const 41
    f32.const 41.0
    i32.const 42
    f32.const 42.0
    i32.const 43
    f32.const 43.0
    i32.const 44
    f32.const 44.0
    i32.const 45
    f32.const 45.0
    i32.const 46
    f32.const 46.0
    i32.const 47
    f32.const 47.0
    i32.const 48
    f32.const 48.0
    i32.const 49
    f32.const 49.0
    i32.const 50
    f32.const 50.0
    call $sumMixedI
  )

  (func (export "sumMixedF_wrapper")  (result f32)
    i32.const 1
    f32.const 1.0
    i32.const 2
    f32.const 2.0
    i32.const 3
    f32.const 3.0
    i32.const 4
    f32.const 4.0
    i32.const 5
    f32.const 5.0
    i32.const 6
    f32.const 6.0
    i32.const 7
    f32.const 7.0
    i32.const 8
    f32.const 8.0
    i32.const 9
    f32.const 9.0
    i32.const 10
    f32.const 10.0
    i32.const 11
    f32.const 11.0
    i32.const 12
    f32.const 12.0
    i32.const 13
    f32.const 13.0
    i32.const 14
    f32.const 14.0
    i32.const 15
    f32.const 15.0
    i32.const 16
    f32.const 16.0
    i32.const 17
    f32.const 17.0
    i32.const 18
    f32.const 18.0
    i32.const 19
    f32.const 19.0
    i32.const 20
    f32.const 20.0
    i32.const 21
    f32.const 21.0
    i32.const 22
    f32.const 22.0
    i32.const 23
    f32.const 23.0
    i32.const 24
    f32.const 24.0
    i32.const 25
    f32.const 25.0
    i32.const 26
    f32.const 26.0
    i32.const 27
    f32.const 27.0
    i32.const 28
    f32.const 28.0
    i32.const 29
    f32.const 29.0
    i32.const 30
    f32.const 30.0
    i32.const 31
    f32.const 31.0
    i32.const 32
    f32.const 32.0
    i32.const 33
    f32.const 33.0
    i32.const 34
    f32.const 34.0
    i32.const 35
    f32.const 35.0
    i32.const 36
    f32.const 36.0
    i32.const 37
    f32.const 37.0
    i32.const 38
    f32.const 38.0
    i32.const 39
    f32.const 39.0
    i32.const 40
    f32.const 40.0
    i32.const 41
    f32.const 41.0
    i32.const 42
    f32.const 42.0
    i32.const 43
    f32.const 43.0
    i32.const 44
    f32.const 44.0
    i32.const 45
    f32.const 45.0
    i32.const 46
    f32.const 46.0
    i32.const 47
    f32.const 47.0
    i32.const 48
    f32.const 48.0
    i32.const 49
    f32.const 49.0
    i32.const 50
    f32.const 50.0
    call $sumMixedF
  )

  (func (export "sumI_wrapper_indirect")  (result i32)
    i32.const 1
    i32.const 2
    i32.const 3
    i32.const 4
    i32.const 5
    i32.const 6
    i32.const 7
    i32.const 8
    i32.const 9
    i32.const 10
    i32.const 11
    i32.const 12
    i32.const 13
    i32.const 14
    i32.const 15
    i32.const 16
    i32.const 17
    i32.const 18
    i32.const 19
    i32.const 20
    i32.const 21
    i32.const 22
    i32.const 23
    i32.const 24
    i32.const 25
    i32.const 26
    i32.const 27
    i32.const 28
    i32.const 29
    i32.const 30
    i32.const 31
    i32.const 32
    i32.const 33
    i32.const 34
    i32.const 35
    i32.const 36
    i32.const 37
    i32.const 38
    i32.const 39
    i32.const 40
    i32.const 41
    i32.const 42
    i32.const 43
    i32.const 44
    i32.const 45
    i32.const 46
    i32.const 47
    i32.const 48
    i32.const 49
    i32.const 50
  	i32.const 10
    call_indirect (type $I)
  )

  (func (export "sumF_wrapper_indirect")  (result f32)
    f32.const 1.0
    f32.const 2.0
    f32.const 3.0
    f32.const 4.0
    f32.const 5.0
    f32.const 6.0
    f32.const 7.0
    f32.const 8.0
    f32.const 9.0
    f32.const 10.0
    f32.const 11.0
    f32.const 12.0
    f32.const 13.0
    f32.const 14.0
    f32.const 15.0
    f32.const 16.0
    f32.const 17.0
    f32.const 18.0
    f32.const 19.0
    f32.const 20.0
    f32.const 21.0
    f32.const 22.0
    f32.const 23.0
    f32.const 24.0
    f32.const 25.0
    f32.const 26.0
    f32.const 27.0
    f32.const 28.0
    f32.const 29.0
    f32.const 30.0
    f32.const 31.0
    f32.const 32.0
    f32.const 33.0
    f32.const 34.0
    f32.const 35.0
    f32.const 36.0
    f32.const 37.0
    f32.const 38.0
    f32.const 39.0
    f32.const 40.0
    f32.const 41.0
    f32.const 42.0
    f32.const 43.0
    f32.const 44.0
    f32.const 45.0
    f32.const 46.0
    f32.const 47.0
    f32.const 48.0
    f32.const 49.0
    f32.const 50.0
	  i32.const 11
    call_indirect (type $F)
  )

  (func (export "sumLastI_wrapper_indirect")  (result i32)
    f32.const 1.0
    f32.const 2.0
    f32.const 3.0
    f32.const 4.0
    f32.const 5.0
    f32.const 6.0
    f32.const 7.0
    f32.const 8.0
    f32.const 9.0
    f32.const 10.0
    f32.const 11.0
    f32.const 12.0
    f32.const 13.0
    f32.const 14.0
    f32.const 15.0
    f32.const 16.0
    f32.const 17.0
    f32.const 18.0
    f32.const 19.0
    f32.const 20.0
    f32.const 21.0
    f32.const 22.0
    f32.const 23.0
    f32.const 24.0
    f32.const 25.0
    f32.const 26.0
    f32.const 27.0
    f32.const 28.0
    f32.const 29.0
    f32.const 30.0
    f32.const 31.0
    f32.const 32.0
    f32.const 33.0
    f32.const 34.0
    f32.const 35.0
    f32.const 36.0
    f32.const 37.0
    f32.const 38.0
    f32.const 39.0
    f32.const 40.0
    f32.const 41.0
    f32.const 42.0
    f32.const 43.0
    f32.const 44.0
    f32.const 45.0
    f32.const 46.0
    f32.const 47.0
    f32.const 48.0
    f32.const 49.0
    f32.const 50.0
    i32.const 1
    i32.const 2
    i32.const 3
    i32.const 4
    i32.const 5
    i32.const 6
    i32.const 7
    i32.const 8
    i32.const 9
    i32.const 10
    i32.const 11
    i32.const 12
    i32.const 13
    i32.const 14
    i32.const 15
    i32.const 16
    i32.const 17
    i32.const 18
    i32.const 19
    i32.const 20
    i32.const 21
    i32.const 22
    i32.const 23
    i32.const 24
    i32.const 25
    i32.const 26
    i32.const 27
    i32.const 28
    i32.const 29
    i32.const 30
    i32.const 31
    i32.const 32
    i32.const 33
    i32.const 34
    i32.const 35
    i32.const 36
    i32.const 37
    i32.const 38
    i32.const 39
    i32.const 40
    i32.const 41
    i32.const 42
    i32.const 43
    i32.const 44
    i32.const 45
    i32.const 46
    i32.const 47
    i32.const 48
    i32.const 49
    i32.const 50
	  i32.const 12
    call_indirect (type $LastI)
  )

  (func (export "sumLastF_wrapper_indirect")  (result f32)
    i32.const 1
    i32.const 2
    i32.const 3
    i32.const 4
    i32.const 5
    i32.const 6
    i32.const 7
    i32.const 8
    i32.const 9
    i32.const 10
    i32.const 11
    i32.const 12
    i32.const 13
    i32.const 14
    i32.const 15
    i32.const 16
    i32.const 17
    i32.const 18
    i32.const 19
    i32.const 20
    i32.const 21
    i32.const 22
    i32.const 23
    i32.const 24
    i32.const 25
    i32.const 26
    i32.const 27
    i32.const 28
    i32.const 29
    i32.const 30
    i32.const 31
    i32.const 32
    i32.const 33
    i32.const 34
    i32.const 35
    i32.const 36
    i32.const 37
    i32.const 38
    i32.const 39
    i32.const 40
    i32.const 41
    i32.const 42
    i32.const 43
    i32.const 44
    i32.const 45
    i32.const 46
    i32.const 47
    i32.const 48
    i32.const 49
    i32.const 50
    f32.const 1.0
    f32.const 2.0
    f32.const 3.0
    f32.const 4.0
    f32.const 5.0
    f32.const 6.0
    f32.const 7.0
    f32.const 8.0
    f32.const 9.0
    f32.const 10.0
    f32.const 11.0
    f32.const 12.0
    f32.const 13.0
    f32.const 14.0
    f32.const 15.0
    f32.const 16.0
    f32.const 17.0
    f32.const 18.0
    f32.const 19.0
    f32.const 20.0
    f32.const 21.0
    f32.const 22.0
    f32.const 23.0
    f32.const 24.0
    f32.const 25.0
    f32.const 26.0
    f32.const 27.0
    f32.const 28.0
    f32.const 29.0
    f32.const 30.0
    f32.const 31.0
    f32.const 32.0
    f32.const 33.0
    f32.const 34.0
    f32.const 35.0
    f32.const 36.0
    f32.const 37.0
    f32.const 38.0
    f32.const 39.0
    f32.const 40.0
    f32.const 41.0
    f32.const 42.0
    f32.const 43.0
    f32.const 44.0
    f32.const 45.0
    f32.const 46.0
    f32.const 47.0
    f32.const 48.0
    f32.const 49.0
    f32.const 50.0
	  i32.const 13
    call_indirect (type $LastF)
  )

  (func (export "sumMixedI_wrapper_indirect")  (result i32)
    i32.const 1
    f32.const 1.0
    i32.const 2
    f32.const 2.0
    i32.const 3
    f32.const 3.0
    i32.const 4
    f32.const 4.0
    i32.const 5
    f32.const 5.0
    i32.const 6
    f32.const 6.0
    i32.const 7
    f32.const 7.0
    i32.const 8
    f32.const 8.0
    i32.const 9
    f32.const 9.0
    i32.const 10
    f32.const 10.0
    i32.const 11
    f32.const 11.0
    i32.const 12
    f32.const 12.0
    i32.const 13
    f32.const 13.0
    i32.const 14
    f32.const 14.0
    i32.const 15
    f32.const 15.0
    i32.const 16
    f32.const 16.0
    i32.const 17
    f32.const 17.0
    i32.const 18
    f32.const 18.0
    i32.const 19
    f32.const 19.0
    i32.const 20
    f32.const 20.0
    i32.const 21
    f32.const 21.0
    i32.const 22
    f32.const 22.0
    i32.const 23
    f32.const 23.0
    i32.const 24
    f32.const 24.0
    i32.const 25
    f32.const 25.0
    i32.const 26
    f32.const 26.0
    i32.const 27
    f32.const 27.0
    i32.const 28
    f32.const 28.0
    i32.const 29
    f32.const 29.0
    i32.const 30
    f32.const 30.0
    i32.const 31
    f32.const 31.0
    i32.const 32
    f32.const 32.0
    i32.const 33
    f32.const 33.0
    i32.const 34
    f32.const 34.0
    i32.const 35
    f32.const 35.0
    i32.const 36
    f32.const 36.0
    i32.const 37
    f32.const 37.0
    i32.const 38
    f32.const 38.0
    i32.const 39
    f32.const 39.0
    i32.const 40
    f32.const 40.0
    i32.const 41
    f32.const 41.0
    i32.const 42
    f32.const 42.0
    i32.const 43
    f32.const 43.0
    i32.const 44
    f32.const 44.0
    i32.const 45
    f32.const 45.0
    i32.const 46
    f32.const 46.0
    i32.const 47
    f32.const 47.0
    i32.const 48
    f32.const 48.0
    i32.const 49
    f32.const 49.0
    i32.const 50
    f32.const 50.0
  	i32.const 14
    call_indirect (type $MixedI)
  )

  (func (export "sumMixedF_wrapper_indirect")  (result f32)
    i32.const 1
    f32.const 1.0
    i32.const 2
    f32.const 2.0
    i32.const 3
    f32.const 3.0
    i32.const 4
    f32.const 4.0
    i32.const 5
    f32.const 5.0
    i32.const 6
    f32.const 6.0
    i32.const 7
    f32.const 7.0
    i32.const 8
    f32.const 8.0
    i32.const 9
    f32.const 9.0
    i32.const 10
    f32.const 10.0
    i32.const 11
    f32.const 11.0
    i32.const 12
    f32.const 12.0
    i32.const 13
    f32.const 13.0
    i32.const 14
    f32.const 14.0
    i32.const 15
    f32.const 15.0
    i32.const 16
    f32.const 16.0
    i32.const 17
    f32.const 17.0
    i32.const 18
    f32.const 18.0
    i32.const 19
    f32.const 19.0
    i32.const 20
    f32.const 20.0
    i32.const 21
    f32.const 21.0
    i32.const 22
    f32.const 22.0
    i32.const 23
    f32.const 23.0
    i32.const 24
    f32.const 24.0
    i32.const 25
    f32.const 25.0
    i32.const 26
    f32.const 26.0
    i32.const 27
    f32.const 27.0
    i32.const 28
    f32.const 28.0
    i32.const 29
    f32.const 29.0
    i32.const 30
    f32.const 30.0
    i32.const 31
    f32.const 31.0
    i32.const 32
    f32.const 32.0
    i32.const 33
    f32.const 33.0
    i32.const 34
    f32.const 34.0
    i32.const 35
    f32.const 35.0
    i32.const 36
    f32.const 36.0
    i32.const 37
    f32.const 37.0
    i32.const 38
    f32.const 38.0
    i32.const 39
    f32.const 39.0
    i32.const 40
    f32.const 40.0
    i32.const 41
    f32.const 41.0
    i32.const 42
    f32.const 42.0
    i32.const 43
    f32.const 43.0
    i32.const 44
    f32.const 44.0
    i32.const 45
    f32.const 45.0
    i32.const 46
    f32.const 46.0
    i32.const 47
    f32.const 47.0
    i32.const 48
    f32.const 48.0
    i32.const 49
    f32.const 49.0
    i32.const 50
    f32.const 50.0
	  i32.const 15
    call_indirect (type $MixedF)
  )

  (func (export "sumMixedFD_F_import_wrapper")  (result f32)
    f64.const 1.0
    f32.const 2.0
    f64.const 3.0
    f32.const 4.0
    f32.const 5.0
    f64.const 6.0
    f32.const 7.0
    f32.const 8.0
    f32.const 9.0
    f64.const 10.0
    f32.const 11.0
    f32.const 12.0
    f32.const 13.0
    f32.const 14.0
    f64.const 15.0
    f64.const 16.0
    f32.const 17.0
    f64.const 18.0
    f32.const 19.0
    f32.const 20.0
    f64.const 21.0
    f32.const 22.0
    f32.const 23.0
    f32.const 24.0
    f64.const 25.0
    f32.const 26.0
    f32.const 27.0
    f32.const 28.0
    f32.const 29.0
    f64.const 30.0
    call $sumMixedFD_F_import
  )

  (func (export "sumMixedFD_D_import_wrapper")  (result f64)
    f64.const 1.0
    f32.const 2.0
    f64.const 3.0
    f32.const 4.0
    f32.const 5.0
    f64.const 6.0
    f32.const 7.0
    f32.const 8.0
    f32.const 9.0
    f64.const 10.0
    f32.const 11.0
    f32.const 12.0
    f32.const 13.0
    f32.const 14.0
    f64.const 15.0
    f64.const 16.0
    f32.const 17.0
    f64.const 18.0
    f32.const 19.0
    f32.const 20.0
    f64.const 21.0
    f32.const 22.0
    f32.const 23.0
    f32.const 24.0
    f64.const 25.0
    f32.const 26.0
    f32.const 27.0
    f32.const 28.0
    f32.const 29.0
    f64.const 30.0
    call $sumMixedFD_D_import
  )

  (func (export "sumMixedDF_F_import_wrapper") (result f32)
    f32.const 1.0
    f64.const 2.0
    f32.const 3.0
    f64.const 4.0
    f64.const 5.0
    f32.const 6.0
    f64.const 7.0
    f64.const 8.0
    f64.const 9.0
    f32.const 10.0
    f64.const 11.0
    f64.const 12.0
    f64.const 13.0
    f64.const 14.0
    f32.const 15.0
    f32.const 16.0
    f64.const 17.0
    f32.const 18.0
    f64.const 19.0
    f64.const 20.0
    f32.const 21.0
    f64.const 22.0
    f64.const 23.0
    f64.const 24.0
    f32.const 25.0
    f64.const 26.0
    f64.const 27.0
    f64.const 28.0
    f64.const 29.0
    f32.const 30.0
    call $sumMixedDF_F_import
  )

  (func (export "sumMixedDF_D_import_wrapper") (result f64)
    f32.const 1.0
    f64.const 2.0
    f32.const 3.0
    f64.const 4.0
    f64.const 5.0
    f32.const 6.0
    f64.const 7.0
    f64.const 8.0
    f64.const 9.0
    f32.const 10.0
    f64.const 11.0
    f64.const 12.0
    f64.const 13.0
    f64.const 14.0
    f32.const 15.0
    f32.const 16.0
    f64.const 17.0
    f32.const 18.0
    f64.const 19.0
    f64.const 20.0
    f32.const 21.0
    f64.const 22.0
    f64.const 23.0
    f64.const 24.0
    f32.const 25.0
    f64.const 26.0
    f64.const 27.0
    f64.const 28.0
    f64.const 29.0
    f32.const 30.0
    call $sumMixedDF_D_import
  )

  (func (export "sumMixedIL_I_import_wrapper") (result i32)
    i64.const 1
    i32.const 2
    i64.const 3
    i32.const 4
    i32.const 5
    i64.const 6
    i32.const 7
    i32.const 8
    i32.const 9
    i64.const 10
    i32.const 11
    i32.const 12
    i32.const 13
    i32.const 14
    i64.const 15
    i64.const 16
    i32.const 17
    i64.const 18
    i32.const 19
    i32.const 20
    i64.const 21
    i32.const 22
    i32.const 23
    i32.const 24
    i64.const 25
    i32.const 26
    i32.const 27
    i32.const 28
    i32.const 29
    i64.const 30
    call $sumMixedIL_I_import
  )

  (func (export "sumMixedIL_L_import_wrapper") (result i64)
    i64.const 0xF000000000000001
    i32.const 2
    i64.const 3
    i32.const 4
    i32.const 5
    i64.const 6
    i32.const 7
    i32.const 8
    i32.const 9
    i64.const 10
    i32.const 11
    i32.const 12
    i32.const 13
    i32.const 14
    i64.const 15
    i64.const 16
    i32.const 17
    i64.const 18
    i32.const 19
    i32.const 20
    i64.const 21
    i32.const 22
    i32.const 23
    i32.const 24
    i64.const 25
    i32.const 26
    i32.const 27
    i32.const 28
    i32.const 29
    i64.const 30
    call $sumMixedIL_L_import
  )

  (func (export "sumMixedLI_I_import_wrapper") (result i32)
    i32.const 1
    i64.const 2
    i32.const 3
    i64.const 4
    i64.const 5
    i32.const 6
    i64.const 7
    i64.const 8
    i64.const 9
    i32.const 10
    i64.const 11
    i64.const 12
    i64.const 13
    i64.const 14
    i32.const 15
    i32.const 16
    i64.const 17
    i32.const 18
    i64.const 19
    i64.const 20
    i32.const 21
    i64.const 22
    i64.const 23
    i64.const 24
    i32.const 25
    i64.const 26
    i64.const 27
    i64.const 28
    i64.const 29
    i32.const 30
    call $sumMixedLI_I_import
  )

  (func (export "sumMixedLI_L_import_wrapper") (result i64)
    i32.const 1
    i64.const 2
    i32.const 3
    i64.const 4
    i64.const 5
    i32.const 6
    i64.const 7
    i64.const 8
    i64.const 9
    i32.const 10
    i64.const 11
    i64.const 12
    i64.const 13
    i64.const 14
    i32.const 15
    i32.const 16
    i64.const 17
    i32.const 18
    i64.const 19
    i64.const 20
    i32.const 21
    i64.const 22
    i64.const 23
    i64.const 24
    i32.const 25
    i64.const 26
    i64.const 27
    i64.const 28
    i64.const 29
    i32.const 30
    call $sumMixedLI_L_import
  )

  (table 30 funcref)
  (elem (i32.const 0) $sumI_import $sumF_import $sumLastI_import $sumLastF_import $sumMixedI_import $sumMixedF_import)
  (elem (i32.const 10) $sumI $sumF $sumLastI $sumLastF $sumMixedI $sumMixedF)
  (elem (i32.const 20) $sumMixedFD_F_import $sumMixedFD_D_import $sumMixedDF_F_import $sumMixedDF_D_import $sumMixedIL_I_import $sumMixedIL_L_import $sumMixedLI_I_import $sumMixedLI_L_import)
)

(assert_return (invoke "sumI_import_wrapper") (i32.const 1275))
(assert_return (invoke "sumF_import_wrapper") (f32.const 1275.0))
(assert_return (invoke "sumLastI_import_wrapper") (i32.const 1275))
(assert_return (invoke "sumLastF_import_wrapper") (f32.const 1275.0))
(assert_return (invoke "sumMixedI_import_wrapper") (i32.const 1275))
(assert_return (invoke "sumMixedF_import_wrapper") (f32.const 1275.0))

(assert_return (invoke "sumI_import_wrapper_indirect") (i32.const 1275))
(assert_return (invoke "sumF_import_wrapper_indirect") (f32.const 1275.0))
(assert_return (invoke "sumLastI_import_wrapper_indirect") (i32.const 1275))
(assert_return (invoke "sumLastF_import_wrapper_indirect") (f32.const 1275.0))
(assert_return (invoke "sumMixedI_import_wrapper_indirect") (i32.const 1275))
(assert_return (invoke "sumMixedF_import_wrapper_indirect") (f32.const 1275.0))

(assert_return (invoke "sumI_wrapper") (i32.const 1275))
(assert_return (invoke "sumF_wrapper") (f32.const 1275.0))
(assert_return (invoke "sumLastI_wrapper") (i32.const 1275))
(assert_return (invoke "sumLastF_wrapper") (f32.const 1275.0))
(assert_return (invoke "sumMixedI_wrapper") (i32.const 1275))
(assert_return (invoke "sumMixedF_wrapper") (f32.const 1275.0))

(assert_return (invoke "sumI_wrapper_indirect") (i32.const 1275))
(assert_return (invoke "sumF_wrapper_indirect") (f32.const 1275.0))
(assert_return (invoke "sumLastI_wrapper_indirect") (i32.const 1275))
(assert_return (invoke "sumLastF_wrapper_indirect") (f32.const 1275.0))
(assert_return (invoke "sumMixedI_wrapper_indirect") (i32.const 1275))
(assert_return (invoke "sumMixedF_wrapper_indirect") (f32.const 1275.0))

(assert_return (invoke "sumI" (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4) (i32.const 5) (i32.const 6) (i32.const 7) (i32.const 8) (i32.const 9) (i32.const 10) (i32.const 11) (i32.const 12) (i32.const 13) (i32.const 14) (i32.const 15) (i32.const 16) (i32.const 17) (i32.const 18) (i32.const 19) (i32.const 20) (i32.const 21) (i32.const 22) (i32.const 23) (i32.const 24) (i32.const 25) (i32.const 26) (i32.const 27) (i32.const 28) (i32.const 29) (i32.const 30) (i32.const 31) (i32.const 32) (i32.const 33) (i32.const 34) (i32.const 35) (i32.const 36) (i32.const 37) (i32.const 38) (i32.const 39) (i32.const 40) (i32.const 41) (i32.const 42) (i32.const 43) (i32.const 44) (i32.const 45) (i32.const 46) (i32.const 47) (i32.const 48) (i32.const 49) (i32.const 50)) (i32.const 1275))
(assert_return (invoke "sumF" (f32.const 1.0) (f32.const 2.0) (f32.const 3.0) (f32.const 4.0) (f32.const 5.0) (f32.const 6.0) (f32.const 7.0) (f32.const 8.0) (f32.const 9.0) (f32.const 10.0) (f32.const 11.0) (f32.const 12.0) (f32.const 13.0) (f32.const 14.0) (f32.const 15.0) (f32.const 16.0) (f32.const 17.0) (f32.const 18.0) (f32.const 19.0) (f32.const 20.0) (f32.const 21.0) (f32.const 22.0) (f32.const 23.0) (f32.const 24.0) (f32.const 25.0) (f32.const 26.0) (f32.const 27.0) (f32.const 28.0) (f32.const 29.0) (f32.const 30.0) (f32.const 31.0) (f32.const 32.0) (f32.const 33.0) (f32.const 34.0) (f32.const 35.0) (f32.const 36.0) (f32.const 37.0) (f32.const 38.0) (f32.const 39.0) (f32.const 40.0) (f32.const 41.0) (f32.const 42.0) (f32.const 43.0) (f32.const 44.0) (f32.const 45.0) (f32.const 46.0) (f32.const 47.0) (f32.const 48.0) (f32.const 49.0) (f32.const 50.0)) (f32.const 1275.0))
(assert_return (invoke "sumLastI" (f32.const 1.0) (f32.const 2.0) (f32.const 3.0) (f32.const 4.0) (f32.const 5.0) (f32.const 6.0) (f32.const 7.0) (f32.const 8.0) (f32.const 9.0) (f32.const 10.0) (f32.const 11.0) (f32.const 12.0) (f32.const 13.0) (f32.const 14.0) (f32.const 15.0) (f32.const 16.0) (f32.const 17.0) (f32.const 18.0) (f32.const 19.0) (f32.const 20.0) (f32.const 21.0) (f32.const 22.0) (f32.const 23.0) (f32.const 24.0) (f32.const 25.0) (f32.const 26.0) (f32.const 27.0) (f32.const 28.0) (f32.const 29.0) (f32.const 30.0) (f32.const 31.0) (f32.const 32.0) (f32.const 33.0) (f32.const 34.0) (f32.const 35.0) (f32.const 36.0) (f32.const 37.0) (f32.const 38.0) (f32.const 39.0) (f32.const 40.0) (f32.const 41.0) (f32.const 42.0) (f32.const 43.0) (f32.const 44.0) (f32.const 45.0) (f32.const 46.0) (f32.const 47.0) (f32.const 48.0) (f32.const 49.0) (f32.const 50.0) (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4) (i32.const 5) (i32.const 6) (i32.const 7) (i32.const 8) (i32.const 9) (i32.const 10) (i32.const 11) (i32.const 12) (i32.const 13) (i32.const 14) (i32.const 15) (i32.const 16) (i32.const 17) (i32.const 18) (i32.const 19) (i32.const 20) (i32.const 21) (i32.const 22) (i32.const 23) (i32.const 24) (i32.const 25) (i32.const 26) (i32.const 27) (i32.const 28) (i32.const 29) (i32.const 30) (i32.const 31) (i32.const 32) (i32.const 33) (i32.const 34) (i32.const 35) (i32.const 36) (i32.const 37) (i32.const 38) (i32.const 39) (i32.const 40) (i32.const 41) (i32.const 42) (i32.const 43) (i32.const 44) (i32.const 45) (i32.const 46) (i32.const 47) (i32.const 48) (i32.const 49) (i32.const 50)) (i32.const 1275))
(assert_return (invoke "sumLastF" (i32.const 1) (i32.const 2) (i32.const 3) (i32.const 4) (i32.const 5) (i32.const 6) (i32.const 7) (i32.const 8) (i32.const 9) (i32.const 10) (i32.const 11) (i32.const 12) (i32.const 13) (i32.const 14) (i32.const 15) (i32.const 16) (i32.const 17) (i32.const 18) (i32.const 19) (i32.const 20) (i32.const 21) (i32.const 22) (i32.const 23) (i32.const 24) (i32.const 25) (i32.const 26) (i32.const 27) (i32.const 28) (i32.const 29) (i32.const 30) (i32.const 31) (i32.const 32) (i32.const 33) (i32.const 34) (i32.const 35) (i32.const 36) (i32.const 37) (i32.const 38) (i32.const 39) (i32.const 40) (i32.const 41) (i32.const 42) (i32.const 43) (i32.const 44) (i32.const 45) (i32.const 46) (i32.const 47) (i32.const 48) (i32.const 49) (i32.const 50) (f32.const 1.0) (f32.const 2.0) (f32.const 3.0) (f32.const 4.0) (f32.const 5.0) (f32.const 6.0) (f32.const 7.0) (f32.const 8.0) (f32.const 9.0) (f32.const 10.0) (f32.const 11.0) (f32.const 12.0) (f32.const 13.0) (f32.const 14.0) (f32.const 15.0) (f32.const 16.0) (f32.const 17.0) (f32.const 18.0) (f32.const 19.0) (f32.const 20.0) (f32.const 21.0) (f32.const 22.0) (f32.const 23.0) (f32.const 24.0) (f32.const 25.0) (f32.const 26.0) (f32.const 27.0) (f32.const 28.0) (f32.const 29.0) (f32.const 30.0) (f32.const 31.0) (f32.const 32.0) (f32.const 33.0) (f32.const 34.0) (f32.const 35.0) (f32.const 36.0) (f32.const 37.0) (f32.const 38.0) (f32.const 39.0) (f32.const 40.0) (f32.const 41.0) (f32.const 42.0) (f32.const 43.0) (f32.const 44.0) (f32.const 45.0) (f32.const 46.0) (f32.const 47.0) (f32.const 48.0) (f32.const 49.0) (f32.const 50.0)) (f32.const 1275.0))
(assert_return (invoke "sumMixedI" (i32.const 1) (f32.const 1.0) (i32.const 2) (f32.const 2.0) (i32.const 3) (f32.const 3.0) (i32.const 4) (f32.const 4.0) (i32.const 5) (f32.const 5.0) (i32.const 6) (f32.const 6.0) (i32.const 7) (f32.const 7.0) (i32.const 8) (f32.const 8.0) (i32.const 9) (f32.const 9.0) (i32.const 10) (f32.const 10.0) (i32.const 11) (f32.const 11.0) (i32.const 12) (f32.const 12.0) (i32.const 13) (f32.const 13.0) (i32.const 14) (f32.const 14.0) (i32.const 15) (f32.const 15.0) (i32.const 16) (f32.const 16.0) (i32.const 17) (f32.const 17.0) (i32.const 18) (f32.const 18.0) (i32.const 19) (f32.const 19.0) (i32.const 20) (f32.const 20.0) (i32.const 21) (f32.const 21.0) (i32.const 22) (f32.const 22.0) (i32.const 23) (f32.const 23.0) (i32.const 24) (f32.const 24.0) (i32.const 25) (f32.const 25.0) (i32.const 26) (f32.const 26.0) (i32.const 27) (f32.const 27.0) (i32.const 28) (f32.const 28.0) (i32.const 29) (f32.const 29.0) (i32.const 30) (f32.const 30.0) (i32.const 31) (f32.const 31.0) (i32.const 32) (f32.const 32.0) (i32.const 33) (f32.const 33.0) (i32.const 34) (f32.const 34.0) (i32.const 35) (f32.const 35.0) (i32.const 36) (f32.const 36.0) (i32.const 37) (f32.const 37.0) (i32.const 38) (f32.const 38.0) (i32.const 39) (f32.const 39.0) (i32.const 40) (f32.const 40.0) (i32.const 41) (f32.const 41.0) (i32.const 42) (f32.const 42.0) (i32.const 43) (f32.const 43.0) (i32.const 44) (f32.const 44.0) (i32.const 45) (f32.const 45.0) (i32.const 46) (f32.const 46.0) (i32.const 47) (f32.const 47.0) (i32.const 48) (f32.const 48.0) (i32.const 49) (f32.const 49.0) (i32.const 50) (f32.const 50.0)) (i32.const 1275))
(assert_return (invoke "sumMixedF" (i32.const 1) (f32.const 1.0) (i32.const 2) (f32.const 2.0) (i32.const 3) (f32.const 3.0) (i32.const 4) (f32.const 4.0) (i32.const 5) (f32.const 5.0) (i32.const 6) (f32.const 6.0) (i32.const 7) (f32.const 7.0) (i32.const 8) (f32.const 8.0) (i32.const 9) (f32.const 9.0) (i32.const 10) (f32.const 10.0) (i32.const 11) (f32.const 11.0) (i32.const 12) (f32.const 12.0) (i32.const 13) (f32.const 13.0) (i32.const 14) (f32.const 14.0) (i32.const 15) (f32.const 15.0) (i32.const 16) (f32.const 16.0) (i32.const 17) (f32.const 17.0) (i32.const 18) (f32.const 18.0) (i32.const 19) (f32.const 19.0) (i32.const 20) (f32.const 20.0) (i32.const 21) (f32.const 21.0) (i32.const 22) (f32.const 22.0) (i32.const 23) (f32.const 23.0) (i32.const 24) (f32.const 24.0) (i32.const 25) (f32.const 25.0) (i32.const 26) (f32.const 26.0) (i32.const 27) (f32.const 27.0) (i32.const 28) (f32.const 28.0) (i32.const 29) (f32.const 29.0) (i32.const 30) (f32.const 30.0) (i32.const 31) (f32.const 31.0) (i32.const 32) (f32.const 32.0) (i32.const 33) (f32.const 33.0) (i32.const 34) (f32.const 34.0) (i32.const 35) (f32.const 35.0) (i32.const 36) (f32.const 36.0) (i32.const 37) (f32.const 37.0) (i32.const 38) (f32.const 38.0) (i32.const 39) (f32.const 39.0) (i32.const 40) (f32.const 40.0) (i32.const 41) (f32.const 41.0) (i32.const 42) (f32.const 42.0) (i32.const 43) (f32.const 43.0) (i32.const 44) (f32.const 44.0) (i32.const 45) (f32.const 45.0) (i32.const 46) (f32.const 46.0) (i32.const 47) (f32.const 47.0) (i32.const 48) (f32.const 48.0) (i32.const 49) (f32.const 49.0) (i32.const 50) (f32.const 50.0)) (f32.const 1275.0))

(assert_return (invoke "sumMixedFD_F_import_wrapper") (f32.const 320.0))
(assert_return (invoke "sumMixedFD_D_import_wrapper") (f64.const 145.0))
(assert_return (invoke "sumMixedDF_F_import_wrapper") (f32.const 145.0))
(assert_return (invoke "sumMixedDF_D_import_wrapper") (f64.const 320.0))
(assert_return (invoke "sumMixedIL_I_import_wrapper") (i32.const 320))
(assert_return (invoke "sumMixedIL_L_import_wrapper") (i64.const 0xf000000000000091))
(assert_return (invoke "sumMixedLI_I_import_wrapper") (i32.const 145))
(assert_return (invoke "sumMixedLI_L_import_wrapper") (i64.const 320))
