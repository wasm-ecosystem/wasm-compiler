project(vb_fuzz)
file(GLOB SRC_FILES *.cpp)

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_link_libraries(${PROJECT_NAME}
	vb_libWasmModule
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "QCC")
	target_link_libraries(${PROJECT_NAME} c++fs)
endif()

if(CMAKE_CXX_CLANG_TIDY)
	set_target_properties(
			${PROJECT_NAME}
			PROPERTIES
			CXX_CLANG_TIDY "clang-tidy;-checks=-cert-err58-cpp"
	)
endif()

EnableClangTidyMSVS(${PROJECT_NAME})
