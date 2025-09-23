cmake_minimum_required(VERSION 3.15)
project(wasm_vb)

# integration options
option(STATIC_LIB "Build static libraries instead of shared" OFF)

set(BACKEND "" CACHE STRING "VB compiler backend to build")

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
	message(STATUS "Building ${PROJECT_NAME} as a standalone project")
	option(VB_ENABLE_DEV_FEATURE "Enable all dev features" ON)
else()
	message(STATUS "Building ${PROJECT_NAME} as a third party project")
	option(VB_ENABLE_DEV_FEATURE "Enable all dev features" OFF)
endif()

# quality options
option(ENABLE_CLANG_TIDY "Enable clang-tidy static analysis" OFF)
option(ENABLE_WERROR "Treat warnings as errors in clang-tidy" OFF)
option(ENABLE_SANITIZE "Enable address, undefined, and pointer-compare sanitizers" OFF)
option(ENABLE_COVERAGE "Enable code coverage flags" OFF)

# dev features
option(ENABLE_BINDING "Enable Python binding and related features" OFF)
option(ENABLE_UNITTEST "Enable unit tests" OFF)
option(ENABLE_EXTENSIONS "Enable analytics features" OFF)
option(ENABLE_SPECTEST "Enable spectest and related tests" OFF)
option(TEST_VARIANTS "Enable test variants for spectest" OFF)
option(ENABLE_FUZZ "Enable fuzzing targets" OFF)
option(ENABLE_DEMO "Enable demo targets" OFF)
option(ENABLE_DIS "Enable disassembler targets" OFF)
option(ENABLE_BENCH "Enable benchmark targets" OFF)
option(ENABLE_ADVANCED_APIS "Enable advanced APIs in WasmModule" OFF)

if(ENABLE_BINDING OR VB_ENABLE_DEV_FEATURE)
	set(ENABLE_BINDING_INTERNAL ON)
else()
	set(ENABLE_BINDING_INTERNAL OFF)
endif()

if(ENABLE_UNITTEST OR VB_ENABLE_DEV_FEATURE)
	set(ENABLE_UNITTEST_INTERNAL ON)
else()
	set(ENABLE_UNITTEST_INTERNAL OFF)
endif()

if(ENABLE_EXTENSIONS OR VB_ENABLE_DEV_FEATURE)
	set(ENABLE_EXTENSIONS_INTERNAL ON)
else()
	set(ENABLE_EXTENSIONS_INTERNAL OFF)
endif()

if(ENABLE_SPECTEST OR VB_ENABLE_DEV_FEATURE)
	set(ENABLE_SPECTEST_INTERNAL ON)
else()
	set(ENABLE_SPECTEST_INTERNAL OFF)
endif()

if(TEST_VARIANTS OR VB_ENABLE_DEV_FEATURE)
	set(TEST_VARIANTS_INTERNAL ON)
else()
	set(TEST_VARIANTS_INTERNAL OFF)
endif()

if(ENABLE_FUZZ OR VB_ENABLE_DEV_FEATURE)
	set(ENABLE_FUZZ_INTERNAL ON)
else()
	set(ENABLE_FUZZ_INTERNAL OFF)
endif()

if(ENABLE_DEMO OR VB_ENABLE_DEV_FEATURE)
	set(ENABLE_DEMO_INTERNAL ON)
else()
	set(ENABLE_DEMO_INTERNAL OFF)
endif()

if(ENABLE_DIS OR VB_ENABLE_DEV_FEATURE)
	set(ENABLE_DIS_INTERNAL ON)
else()
	set(ENABLE_DIS_INTERNAL OFF)
endif()

if(ENABLE_ADVANCED_APIS OR VB_ENABLE_DEV_FEATURE OR ENABLE_FUZZ_INTERNAL OR ENABLE_SPECTEST_INTERNAL)
	set(ENABLE_ADVANCED_APIS_INTERNAL ON)
else()
	set(ENABLE_ADVANCED_APIS_INTERNAL OFF)
endif()

if(ENABLE_BENCH)
	set(ENABLE_BENCH_INTERNAL ON) # bench is incompatible with most of feature, disable it by default.
else()
	set(ENABLE_BENCH_INTERNAL OFF)
endif()

# enable deps based on features
if(ENABLE_BINDING_INTERNAL)
	set(ENABLE_EXTENSIONS_INTERNAL ON)
	set(ENABLE_DIS_INTERNAL ON)
endif()

if(ENABLE_UNITTEST_INTERNAL)
	set(ENABLE_DIS_INTERNAL ON)
endif()

message("ENABLE_BINDING: ${ENABLE_BINDING_INTERNAL}")
message("ENABLE_UNITTEST: ${ENABLE_UNITTEST_INTERNAL}")
message("ENABLE_EXTENSIONS: ${ENABLE_EXTENSIONS_INTERNAL}")
message("ENABLE_SPECTEST: ${ENABLE_SPECTEST_INTERNAL}")
message("TEST_VARIANTS: ${TEST_VARIANTS_INTERNAL}")
message("ENABLE_FUZZ: ${ENABLE_FUZZ_INTERNAL}")
message("ENABLE_DEMO: ${ENABLE_DEMO_INTERNAL}")
message("ENABLE_DIS: ${ENABLE_DIS_INTERNAL}")
message("ENABLE_BENCH: ${ENABLE_BENCH_INTERNAL}")
message("ENABLE_ADVANCED_APIS: ${ENABLE_ADVANCED_APIS_INTERNAL}")

if(STATIC_LIB AND ENABLE_BINDING_INTERNAL)
	message(FATAL_ERROR "Static libraries cannot be built with Python binding enabled. Please disable ENABLE_BINDING or STATIC_LIB.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/AutoSelectBackendTarget.cmake)
include(cmake/EnableStaticCheck.cmake)
include(cmake/AddCompilerOptions.cmake)
include(cmake/AddLinkDependencies.cmake)
include(cmake/AddInstallTarget.cmake)

set(WASM_COMPILER_INCLUDE_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${WASM_COMPILER_INCLUDE_ROOT})

add_warning_flag()

if(MSVC)
	if(STATIC_LIB)
		add_compile_options(
			$<$<CONFIG:>:/MT> # ---------|
			$<$<CONFIG:Debug>:/MTd> # ---|-- Statically link the runtime libraries
			$<$<CONFIG:Release>:/MT> # --|
		)
	else()
		add_compile_options(
			$<$<CONFIG:>:/MD> # ---------|
			$<$<CONFIG:Debug>:/MDd> # ---|-- Dynamically link the runtime libraries
			$<$<CONFIG:Release>:/MD> # --|
		)
	endif()

	add_msvc_parallel_opt()

	add_definitions(-D_CRT_SECURE_NO_WARNINGS)

	set(CMAKE_SUPPRESS_REGENERATION ON)

else()
	if(ENABLE_CLANG_TIDY)
		set(CMAKE_CXX_CLANG_TIDY clang-tidy --config-file ${CMAKE_CURRENT_LIST_DIR}/.clang-tidy "--header-filter=${CMAKE_CURRENT_LIST_DIR}/(src|tests)/.*")

		if(ENABLE_WERROR)
			set(CMAKE_CXX_CLANG_TIDY ${CMAKE_CXX_CLANG_TIDY} -warnings-as-errors=*)
		endif()
	endif()

	if(ENABLE_SANITIZE)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined,pointer-compare")
	endif()

	if(STATIC_LIB)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")
	endif()

	set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		# using regular Clang or AppleClang
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-warning-option")
	endif()
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

string(TOUPPER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_BUILD_TYPE STREQUAL "DEBUG" AND NOT ENABLE_CLANG_TIDY)
	include(CheckIPOSupported)
	check_ipo_supported(RESULT supported OUTPUT error)

	if(supported)
		message(STATUS "IPO / LTO enabled")
		set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
	else()
		message(STATUS "IPO / LTO not supported: <${error}>")
	endif()
endif()

set(BACKEND_INTERNAL ${BACKEND})
AutoSelectBackendTarget()

if(BACKEND_INTERNAL STREQUAL "x86_64")
	add_definitions(-DJIT_TARGET_X86_64)
elseif(BACKEND_INTERNAL STREQUAL "aarch64")
	add_definitions(-DJIT_TARGET_AARCH64)
elseif(BACKEND_INTERNAL STREQUAL "tricore")
	add_definitions(-DJIT_TARGET_TRICORE)
else()
	message(FATAL_ERROR "BACKEND not recognized. CMake will exit.")
endif()

if((DEFINED CXX_TARGET) AND(CXX_TARGET STREQUAL "tricore"))
	if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		set(linkerScript "${CMAKE_CURRENT_SOURCE_DIR}/linker/GNUTricore.ld")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=tc39xx")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-T ${linkerScript}")
		add_definitions(-D_GLIBCXX_USE_C99_STDLIB=1 -D_GLIBCXX_USE_C99_STDIO=1)
		list(APPEND Linker_Files ${linkerScript})
	elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Tasking")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${Tasking_Linker_Flags}")
	endif()
endif()

find_package(Threads QUIET) # Needed because of CMake intricacy not finding Threads on some systems when --coverage is enabled otherwise

if(ENABLE_COVERAGE)
	if(UNIX AND(NOT CMAKE_CXX_COMPILER_ID STREQUAL "QCC"))
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --coverage")
	endif()
endif()

if(ENABLE_BINDING_INTERNAL)
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

if(ENABLE_EXTENSIONS_INTERNAL)
	add_definitions(-DENABLE_EXTENSIONS=1)
else()
	add_definitions(-DENABLE_EXTENSIONS=0)
endif()

if(ENABLE_ADVANCED_APIS_INTERNAL)
	add_definitions(-DENABLE_ADVANCED_APIS=1)
else()
	add_definitions(-DENABLE_ADVANCED_APIS=0)
endif()

#
# subdirectory
#
add_subdirectory(src)

if(ENABLE_EXTENSIONS_INTERNAL)
	add_subdirectory(extensions)
endif()

if(ENABLE_SPECTEST_INTERNAL)
	if(TEST_VARIANTS_INTERNAL)
		add_definitions(-DTEST_VARIANTS=1)
	endif()

	enable_testing()
	add_subdirectory(tests)
endif()

if(ENABLE_UNITTEST_INTERNAL)
	include(cmake/AddGtest.cmake)
	add_subdirectory(tests/unittests)
endif()

if(ENABLE_FUZZ_INTERNAL)
	add_subdirectory(fuzz)
endif()

if(ENABLE_DEMO_INTERNAL)
	add_subdirectory(demo)
endif()

if(ENABLE_BENCH_INTERNAL)
	add_subdirectory(bench)
endif()

if(ENABLE_DIS_INTERNAL)
	add_subdirectory(disassembler)
endif()

if(ENABLE_BINDING_INTERNAL)
	add_subdirectory(thirdparty/pybind11)
	add_subdirectory(binding)
endif()
