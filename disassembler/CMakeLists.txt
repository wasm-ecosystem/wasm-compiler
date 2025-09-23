set(CMAKE_CXX_STANDARD 20)

find_library(CAPSTONE NAMES capstone)

if(CAPSTONE)
  find_program(CSTOOL "cstool")

  # When using conan to install the dependencies but CMake to do the build, the
  # capstone libraries may not be findable by the loader. Set LD_LIBRARY path to
  # fix this.CAPSTONE
  get_filename_component(CAPSTONE_LIB_DIR "${CAPSTONE}" DIRECTORY)
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E env
    "LD_LIBRARY_PATH=$ENV{LD_LIBRARY_PATH}:${CAPSTONE_LIB_DIR}" #
    "${CSTOOL}" "-v" OUTPUT_VARIABLE CSTOOL_VERSION)
  string(REGEX MATCH "v([0-9]+\.[0-9]+\.[0-9]+)" CSTOOL_VERSION
    "${CSTOOL_VERSION}")
  set(CSTOOL_VERSION "${CMAKE_MATCH_1}")
  set(vb_disassembler_found_capstone "1")
else()
  message(WARNING "capstone not found")
endif()

if(NOT CAPSTONE_INCLUDE_DIR)
  get_filename_component(CAPSTONE_LIB_DIR ${CAPSTONE} DIRECTORY)
  get_filename_component(CAPSTONE_INCLUDE_DIR ${CAPSTONE_LIB_DIR}/../include
    ABSOLUTE)

  if(NOT EXISTS ${CAPSTONE_INCLUDE_DIR}/capstone)
    message(WARNING "CAPSTONE_INCLUDE_DIR not found")
    unset(CAPSTONE_INCLUDE_DIR)
  endif()

  unset(CAPSTONE_LIB_DIR)
endif()

if(NOT CAPSTONE OR "${CSTOOL_VERSION}" VERSION_LESS "5.0.1")
  message(
    STATUS
    " No Capstone installation found.\n"
    " - If Capstone is not installed, install it from source.\n"
    "   You can get the latest version of Capstone at:\n"
    "       http://www.capstone-engine.org/\n"
    " - If Capstone is installed, make sure the installation location is in your PATH,\n"
    "   and it is at least version 5.0.1.\n")
  set(vb_disassembler_found_capstone "0")
  set(CAPSTONE "")
  set(CAPSTONE_INCLUDE_DIR "")
endif()

file(GLOB SRC_FILES
  ${CMAKE_CURRENT_LIST_DIR}/*.cpp
)

add_library(vb_lib_disassembler
  ${SRC_FILES}
)
target_link_libraries(vb_lib_disassembler PRIVATE
  vb_lib_core_common
  vb_libcompiler
  ${CAPSTONE}
)
target_include_directories(vb_lib_disassembler PRIVATE
  ${CAPSTONE_INCLUDE_DIR}
)
target_compile_definitions(vb_lib_disassembler PRIVATE
  "-DVB_DISASSEMBLER_FOUND_CAPSTONE=${vb_disassembler_found_capstone}"
)
