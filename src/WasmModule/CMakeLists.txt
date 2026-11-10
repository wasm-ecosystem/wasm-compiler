set(LibName vb_libWasmModule)

aux_source_directory(${CMAKE_CURRENT_LIST_DIR} SRC_FILES)

add_library(${LibName} ${SRC_FILES})
target_link_libraries(${LibName} PUBLIC
    vb_libruntime
    vb_libcompiler
)

if(BACKEND_INTERNAL STREQUAL "tricore")
    target_link_libraries(${LibName} PUBLIC lib_softfloat)
endif()

if(CXX_TARGET STREQUAL "tricore")
else()
    target_link_libraries(${LibName} PUBLIC vb_libutils)
endif()

target_include_directories(${LibName} PUBLIC "$<BUILD_INTERFACE:${WASM_COMPILER_INCLUDE_ROOT}>")

if(ENABLE_EXTENSIONS_INTERNAL)
    file(GLOB EXTENSION_SRC_FILES "${WASM_COMPILER_INCLUDE_ROOT}/extensions/*.cpp")
    add_library(vb_extension_objects OBJECT ${EXTENSION_SRC_FILES})
    set_target_properties(vb_extension_objects PROPERTIES CXX_STANDARD 20)
    if(BUILD_SHARED_LIBS)
        set_target_properties(vb_extension_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
    target_sources(${LibName} PRIVATE $<TARGET_OBJECTS:vb_extension_objects>)
    EnableClangTidyMSVS(vb_extension_objects)
endif()

EnableClangTidyMSVS(${LibName})
