set(LibName vb_libWasmModule)

aux_source_directory(${CMAKE_CURRENT_LIST_DIR} SRC_FILES)

add_library(${LibName} ${SRC_FILES})
target_link_libraries(${LibName} PUBLIC
    vb_libcompiler
    vb_libruntime
)

if(BACKEND_INTERNAL STREQUAL "tricore")
    target_link_libraries(${LibName} PUBLIC lib_softfloat)
endif()

if(CXX_TARGET STREQUAL "tricore")
else()
    target_link_libraries(${LibName} PUBLIC vb_libutils)
endif()

target_include_directories(${LibName} PUBLIC "$<BUILD_INTERFACE:${WASM_COMPILER_INCLUDE_ROOT}>")

EnableClangTidyMSVS(${LibName})

if(ENABLE_EXTENSIONS_INTERNAL)
    target_link_libraries(${LibName} PUBLIC vb_extensions)
endif()
