project(vb_afl_harness_persistent)
find_package(Threads)

add_executable(${PROJECT_NAME} persistentharness.cpp)

target_link_libraries(${PROJECT_NAME}
	vb_libWasmModule
	Threads::Threads
)

if(CMAKE_CXX_CLANG_TIDY)
	set_target_properties(
			${PROJECT_NAME}
			PROPERTIES
			CXX_CLANG_TIDY "clang-tidy;-checks=-cert-err58-cpp"
	)
endif()

EnableClangTidyMSVS(${PROJECT_NAME})
