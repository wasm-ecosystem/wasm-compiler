
file(GLOB SRC_FILES *.cpp)

set(LibName vb_libruntime)

add_library(${LibName} ${SRC_FILES})

target_link_libraries(${LibName} vb_lib_core_common)

EnableClangTidyMSVS(${LibName})

target_include_directories(${LibName} PUBLIC "$<BUILD_INTERFACE:${WASM_COMPILER_INCLUDE_ROOT}>")
