set(LibName vb_libcompiler)

file(GLOB SRC_FILES
    ${CMAKE_CURRENT_LIST_DIR}/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/frontend/*.cpp
    ${CMAKE_CURRENT_LIST_DIR}/common/*.cpp
)

file(GLOB_RECURSE backendFiles
    ${CMAKE_CURRENT_LIST_DIR}/backend/*.cpp
)

add_library(${LibName} ${SRC_FILES} ${backendFiles})
target_link_libraries(${LibName} PUBLIC
    vb_lib_core_common
)

if((BACKEND_INTERNAL STREQUAL "x86_64") OR(BACKEND_INTERNAL STREQUAL "aarch64") OR(BACKEND_INTERNAL STREQUAL "tricore"))
    if(BACKEND_INTERNAL STREQUAL "tricore")
        add_subdirectory(../../../thirdparty/softfloat_integration ${CMAKE_CURRENT_BINARY_DIR}/softfloat_integration)
        target_link_libraries(${LibName} PUBLIC lib_softfloat)
    endif()

    string(ASCII 27 ESCAPE)
    message("Building compiler with ${ESCAPE}[32m${BACKEND_INTERNAL}${ESCAPE}[0m backend")

else()
    message(FATAL_ERROR "Backend not recognized. CMake will exit.")
endif()

target_include_directories(${LibName} PUBLIC "$<BUILD_INTERFACE:${WASM_COMPILER_INCLUDE_ROOT}>")

EnableClangTidyMSVS(${LibName})
