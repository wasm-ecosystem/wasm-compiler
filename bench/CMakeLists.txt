project(vb_bench)
set(CMAKE_CXX_STANDARD 14)
file(GLOB SRC_FILES *.cpp)

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_link_libraries(${PROJECT_NAME}
	vb_libWasmModule
)

EnableClangTidyMSVS(${PROJECT_NAME})
