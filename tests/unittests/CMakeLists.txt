set(CMAKE_CXX_STANDARD 17)
project(Unittests)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} sourceFiles)

add_executable(${PROJECT_NAME} ${sourceFiles})

target_include_directories(${PROJECT_NAME} PRIVATE ${GTEST_INCLUDE_DIRS} ${WASM_COMPILER_INCLUDE_ROOT})

target_link_libraries(${PROJECT_NAME} ${GTEST_BOTH_LIBRARIES} vb_libcompiler vb_libruntime vb_libutils vb_lib_disassembler vb_libWasmModule)

add_test_case(${PROJECT_NAME})

EnableClangTidyMSVS(${PROJECT_NAME})
