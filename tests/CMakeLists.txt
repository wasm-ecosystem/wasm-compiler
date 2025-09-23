project(vb_spectest)

set(CMAKE_CXX_STANDARD 14)

aux_source_directory(. SRC_FILES)

# ###################
# vb_spectest_core #
# ###################
add_library(${PROJECT_NAME}_core
	${SRC_FILES}
)
target_include_directories(${PROJECT_NAME}_core SYSTEM PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/../thirdparty/json/include/
)
target_link_libraries(${PROJECT_NAME}_core
	vb_libWasmModule
)

target_link_libraries(${PROJECT_NAME}_core
	vb_libutils
)

if(CMAKE_CXX_CLANG_TIDY)
	set_target_properties(
		${PROJECT_NAME}_core
		PROPERTIES
		CXX_CLANG_TIDY "clang-tidy;-checks=-clang-analyzer-unix.Malloc,-cert-err58-cpp"
	)
endif()

EnableClangTidyMSVS(${PROJECT_NAME}_core)

# ###################
# vb_spectest_wast #
# ###################
if(NOT DISABLE_SPECTEST_WAST)
	add_executable(${PROJECT_NAME}
		execute/test_with_wast.cpp
		loader/json_loader.cpp
	)
	set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
	target_link_libraries(${PROJECT_NAME}
		${PROJECT_NAME}_core
	)

	if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9"))
		target_link_libraries(${PROJECT_NAME} stdc++fs)
	endif()

	if(CMAKE_CXX_CLANG_TIDY)
		set_target_properties(
			${PROJECT_NAME}
			PROPERTIES
			CXX_CLANG_TIDY "clang-tidy;-checks=-clang-analyzer-unix.Malloc,-cert-err58-cpp"
		)
	endif()

	EnableClangTidyMSVS(${PROJECT_NAME})
endif()

# ###################
# vb_spectest_json #
# ###################
if(NOT DISABLE_SPECTEST_JSON)
	add_executable(${PROJECT_NAME}_json
		execute/test_with_json.cpp
		loader/json_loader.cpp
	)

	target_link_libraries(${PROJECT_NAME}_json
		${PROJECT_NAME}_core
	)

	if(CMAKE_CXX_CLANG_TIDY)
		set_target_properties(
			${PROJECT_NAME}_json
			PROPERTIES
			CXX_CLANG_TIDY "clang-tidy;-checks=-clang-analyzer-unix.Malloc,-cert-err58-cpp"
		)
	endif()

	EnableClangTidyMSVS(${PROJECT_NAME}_json)
endif()

add_library(BinaryTestCaseLib
	execute/RunBinaryTestCase.cpp
	loader/stream_loader.cpp
)

if(ENABLE_STANDALONE_TEST)
	add_executable(${PROJECT_NAME}_binary_standalone_0
		execute/test_with_binary_standalone.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/testcases/bin/total_0.cpp
	)
	add_executable(${PROJECT_NAME}_binary_standalone_1
		execute/test_with_binary_standalone.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/testcases/bin/total_1.cpp
	)
	target_link_libraries(${PROJECT_NAME}_binary_standalone_0
		${PROJECT_NAME}_core
		BinaryTestCaseLib
	)
	target_link_libraries(${PROJECT_NAME}_binary_standalone_1
		${PROJECT_NAME}_core
		BinaryTestCaseLib
	)

	if(DEFINED Linker_Files)
		AddLinkDependencies(${PROJECT_NAME}_binary_standalone_0 Linker_Files)
	endif()

	if(DEFINED Linker_Files)
		AddLinkDependencies(${PROJECT_NAME}_binary_standalone_1 Linker_Files)
	endif()
endif()

if(NOT DEFINED CMAKE_CROSSCOMPILING_EMULATOR)
	add_test(
		NAME spectest
		COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/testsuite
		WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
	)
elseif("${CMAKE_CROSSCOMPILING_EMULATOR}" MATCHES "qemu-aarch64")
	add_test(
		NAME spectest
		COMMAND "qemu-aarch64" -L /usr/aarch64-linux-gnu ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/testsuite
		WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
	)
else()
	message(FATAL_ERROR "unsupported CMAKE_CROSSCOMPILING_EMULATOR for testing")
endif()

if(ENABLE_COVERAGE)
	if(UNIX AND(NOT CMAKE_CXX_COMPILER_ID STREQUAL "QCC"))
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
	endif()

	if(CMAKE_GENERATOR STREQUAL "Unix Makefiles")
		if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
			set(CovExecutable "llvm-cov gcov")
		elseif((CMAKE_CXX_COMPILER_ID STREQUAL "GNU") OR(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"))
			set(CovExecutable "gcov")
		endif()

		add_custom_target(run_ctest
			COMMAND ctest "--output-on-failure"
			WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/../
		)

		if(DEFINED CovExecutable)
			set(gcovr_options_basic --exclude-throw-branches --exclude-unreachable-branches --exclude-lines-by-pattern .*assert.* --gcov-executable ${CovExecutable} -r ${CMAKE_CURRENT_SOURCE_DIR}/../src .)
			set(gcovr_options_xml ${gcovr_options_basic} -d -s -x coverage.xml)
			add_custom_target(coverage_basic VERBATIM
				COMMAND gcovr ${gcovr_options_basic}
				WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/../
				DEPENDS run_ctest
			)
			add_custom_target(
				coverage VERBATIM
				COMMAND gcovr ${gcovr_options_xml}
				WORKING_DIRECTORY ${PROJECT_BINARY_DIR}/../
				DEPENDS coverage_basic
			)
		endif()
	endif()
endif()
