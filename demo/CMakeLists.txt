project(vb_demo)
set(CMAKE_CXX_STANDARD 14)

add_executable(vb_demo vb_demo.cpp)

target_link_libraries(vb_demo
	vb_libWasmModule
)
EnableClangTidyMSVS(vb_demo)

