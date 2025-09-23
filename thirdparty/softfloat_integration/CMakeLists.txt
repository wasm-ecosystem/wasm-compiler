project(lib_softfloat)
set(CMAKE_CXX_CLANG_TIDY "")
set(CMAKE_C_FLAGS "-w")
set(CMAKE_C_STANDARD 99)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Tasking")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --pass-c=--no-warnings=501 --pass-c=--no-warnings=507 --pass-c=--no-warnings=553 --pass-c=--no-warnings=560 --pass-c=--no-warnings=562")
endif()

SET(SOFTFLOAT_DIR ${CMAKE_CURRENT_LIST_DIR}/../berkeley-softfloat-3)
SET(SPECIALIZE_TYPE 8086)

add_library(lib_softfloat STATIC
    ${SOFTFLOAT_DIR}/source/s_compare96M.c
    ${SOFTFLOAT_DIR}/source/s_compare128M.c
    ${SOFTFLOAT_DIR}/source/s_shortShiftLeft64To96M.c
    ${SOFTFLOAT_DIR}/source/s_shortShiftLeftM.c
    ${SOFTFLOAT_DIR}/source/s_shiftLeftM.c
    ${SOFTFLOAT_DIR}/source/s_shortShiftRightM.c
    ${SOFTFLOAT_DIR}/source/s_shortShiftRightJam64.c
    ${SOFTFLOAT_DIR}/source/s_shortShiftRightJamM.c
    ${SOFTFLOAT_DIR}/source/s_shiftRightJam32.c
    ${SOFTFLOAT_DIR}/source/s_shiftRightJam64.c
    ${SOFTFLOAT_DIR}/source/s_shiftRightJamM.c
    ${SOFTFLOAT_DIR}/source/s_shiftRightM.c
    ${SOFTFLOAT_DIR}/source/s_countLeadingZeros8.c
    ${SOFTFLOAT_DIR}/source/s_countLeadingZeros16.c
    ${SOFTFLOAT_DIR}/source/s_countLeadingZeros32.c
    ${SOFTFLOAT_DIR}/source/s_countLeadingZeros64.c
    ${SOFTFLOAT_DIR}/source/s_addM.c
    ${SOFTFLOAT_DIR}/source/s_addCarryM.c
    ${SOFTFLOAT_DIR}/source/s_addComplCarryM.c
    ${SOFTFLOAT_DIR}/source/s_negXM.c
    ${SOFTFLOAT_DIR}/source/s_sub1XM.c
    ${SOFTFLOAT_DIR}/source/s_subM.c
    ${SOFTFLOAT_DIR}/source/s_mul64To128M.c
    ${SOFTFLOAT_DIR}/source/s_mul128MTo256M.c
    ${SOFTFLOAT_DIR}/source/s_approxRecip_1Ks.c
    ${SOFTFLOAT_DIR}/source/s_approxRecip32_1.c
    ${SOFTFLOAT_DIR}/source/s_approxRecipSqrt_1Ks.c
    ${SOFTFLOAT_DIR}/source/s_approxRecipSqrt32_1.c
    ${SOFTFLOAT_DIR}/source/s_remStepMBy32.c

    ${SOFTFLOAT_DIR}/source/${SPECIALIZE_TYPE}/softfloat_raiseFlags.c
    ${SOFTFLOAT_DIR}/source/${SPECIALIZE_TYPE}/s_f32UIToCommonNaN.c
    ${SOFTFLOAT_DIR}/source/${SPECIALIZE_TYPE}/s_commonNaNToF32UI.c
    ${SOFTFLOAT_DIR}/source/${SPECIALIZE_TYPE}/s_propagateNaNF32UI.c
    ${SOFTFLOAT_DIR}/source/${SPECIALIZE_TYPE}/s_f64UIToCommonNaN.c
    ${SOFTFLOAT_DIR}/source/${SPECIALIZE_TYPE}/s_commonNaNToF64UI.c
    ${SOFTFLOAT_DIR}/source/${SPECIALIZE_TYPE}/s_propagateNaNF64UI.c

    ${SOFTFLOAT_DIR}/source/s_roundToUI32.c
    ${SOFTFLOAT_DIR}/source/s_roundMToUI64.c
    ${SOFTFLOAT_DIR}/source/s_roundToI32.c
    ${SOFTFLOAT_DIR}/source/s_roundMToI64.c
    ${SOFTFLOAT_DIR}/source/s_normSubnormalF32Sig.c
    ${SOFTFLOAT_DIR}/source/s_roundPackToF32.c
    ${SOFTFLOAT_DIR}/source/s_normRoundPackToF32.c
    ${SOFTFLOAT_DIR}/source/s_addMagsF32.c
    ${SOFTFLOAT_DIR}/source/s_subMagsF32.c
    ${SOFTFLOAT_DIR}/source/s_mulAddF32.c
    ${SOFTFLOAT_DIR}/source/s_normSubnormalF64Sig.c
    ${SOFTFLOAT_DIR}/source/s_roundPackToF64.c
    ${SOFTFLOAT_DIR}/source/s_normRoundPackToF64.c
    ${SOFTFLOAT_DIR}/source/s_addMagsF64.c
    ${SOFTFLOAT_DIR}/source/s_subMagsF64.c
    ${SOFTFLOAT_DIR}/source/s_mulAddF64.c
    ${SOFTFLOAT_DIR}/source/s_normExtF80SigM.c
    ${SOFTFLOAT_DIR}/source/softfloat_state.c
    ${SOFTFLOAT_DIR}/source/ui32_to_f32.c
    ${SOFTFLOAT_DIR}/source/ui32_to_f64.c
    ${SOFTFLOAT_DIR}/source/ui64_to_f32.c
    ${SOFTFLOAT_DIR}/source/ui64_to_f64.c
    ${SOFTFLOAT_DIR}/source/i32_to_f32.c
    ${SOFTFLOAT_DIR}/source/i32_to_f64.c
    ${SOFTFLOAT_DIR}/source/i64_to_f32.c
    ${SOFTFLOAT_DIR}/source/i64_to_f64.c
    ${SOFTFLOAT_DIR}/source/f32_to_ui32.c
    ${SOFTFLOAT_DIR}/source/f32_to_ui64.c
    ${SOFTFLOAT_DIR}/source/f32_to_i32.c
    ${SOFTFLOAT_DIR}/source/f32_to_i64.c
    ${SOFTFLOAT_DIR}/source/f32_to_ui32_r_minMag.c
    ${SOFTFLOAT_DIR}/source/f32_to_ui64_r_minMag.c
    ${SOFTFLOAT_DIR}/source/f32_to_i32_r_minMag.c
    ${SOFTFLOAT_DIR}/source/f32_to_i64_r_minMag.c
    ${SOFTFLOAT_DIR}/source/f32_to_f64.c
    ${SOFTFLOAT_DIR}/source/f32_roundToInt.c
    ${SOFTFLOAT_DIR}/source/f32_add.c
    ${SOFTFLOAT_DIR}/source/f32_sub.c
    ${SOFTFLOAT_DIR}/source/f32_mul.c
    ${SOFTFLOAT_DIR}/source/f32_mulAdd.c
    ${SOFTFLOAT_DIR}/source/f32_div.c
    ${SOFTFLOAT_DIR}/source/f32_rem.c
    ${SOFTFLOAT_DIR}/source/f32_sqrt.c
    ${SOFTFLOAT_DIR}/source/f32_eq.c
    ${SOFTFLOAT_DIR}/source/f32_le.c
    ${SOFTFLOAT_DIR}/source/f32_lt.c
    ${SOFTFLOAT_DIR}/source/f32_eq_signaling.c
    ${SOFTFLOAT_DIR}/source/f32_le_quiet.c
    ${SOFTFLOAT_DIR}/source/f32_lt_quiet.c
    ${SOFTFLOAT_DIR}/source/f32_isSignalingNaN.c
    ${SOFTFLOAT_DIR}/source/f64_to_ui32.c
    ${SOFTFLOAT_DIR}/source/f64_to_ui64.c
    ${SOFTFLOAT_DIR}/source/f64_to_i32.c
    ${SOFTFLOAT_DIR}/source/f64_to_i64.c
    ${SOFTFLOAT_DIR}/source/f64_to_ui32_r_minMag.c
    ${SOFTFLOAT_DIR}/source/f64_to_ui64_r_minMag.c
    ${SOFTFLOAT_DIR}/source/f64_to_i32_r_minMag.c
    ${SOFTFLOAT_DIR}/source/f64_to_i64_r_minMag.c
    ${SOFTFLOAT_DIR}/source/f64_to_f32.c
    ${SOFTFLOAT_DIR}/source/f64_roundToInt.c
    ${SOFTFLOAT_DIR}/source/f64_add.c
    ${SOFTFLOAT_DIR}/source/f64_sub.c
    ${SOFTFLOAT_DIR}/source/f64_mul.c
    ${SOFTFLOAT_DIR}/source/f64_mulAdd.c
    ${SOFTFLOAT_DIR}/source/f64_div.c
    ${SOFTFLOAT_DIR}/source/f64_rem.c
    ${SOFTFLOAT_DIR}/source/f64_sqrt.c
    ${SOFTFLOAT_DIR}/source/f64_eq.c
    ${SOFTFLOAT_DIR}/source/f64_le.c
    ${SOFTFLOAT_DIR}/source/f64_lt.c
    ${SOFTFLOAT_DIR}/source/f64_eq_signaling.c
    ${SOFTFLOAT_DIR}/source/f64_le_quiet.c
    ${SOFTFLOAT_DIR}/source/f64_lt_quiet.c
    ${SOFTFLOAT_DIR}/source/f64_isSignalingNaN.c
)

target_include_directories(lib_softfloat SYSTEM PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>")
target_include_directories(lib_softfloat SYSTEM PUBLIC "$<BUILD_INTERFACE:${SOFTFLOAT_DIR}/source/${SPECIALIZE_TYPE}>" )
target_include_directories(lib_softfloat SYSTEM PUBLIC "$<BUILD_INTERFACE:${SOFTFLOAT_DIR}/source/include>" )
