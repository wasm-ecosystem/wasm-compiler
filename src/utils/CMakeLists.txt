if(CXX_TARGET STREQUAL "tricore")
    set(SRC_FILES MemUtils.cpp)
else()
    file(GLOB SRC_FILES *.cpp)
endif()

add_library(vb_libutils ${SRC_FILES})
EnableClangTidyMSVS(vb_libutils)

target_include_directories(vb_libutils PUBLIC "$<BUILD_INTERFACE:${WASM_COMPILER_INCLUDE_ROOT}>")

if(NOT(CXX_TARGET STREQUAL "tricore"))
    find_package(Threads)
    target_link_libraries(vb_libutils Threads::Threads)
endif()
