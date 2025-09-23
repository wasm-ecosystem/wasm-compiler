SET(CMAKE_SYSTEM_PROCESSOR "tricore")
SET(CMAKE_SYSTEM_NAME "Generic")
SET(CMAKE_CROSSCOMPILING TRUE)

SET(CMAKE_C_COMPILER_ID_RUN TRUE CACHE INTERNAL "")
SET(CMAKE_CXX_COMPILER_ID_RUN TRUE CACHE INTERNAL "")
set(CMAKE_TASKING_TOOLSET "SmartCode")
set(CMAKE_CXX_COMPILER_ARCHITECTURE_ID "TriCore")

SET(CMAKE_C_COMPILER_ID "Tasking")
SET(CMAKE_CXX_COMPILER_ID "Tasking")

set(CMAKE_C_COMPILER cctc)
set(CMAKE_CXX_COMPILER cctc)
set(CMAKE_AR "python")

set(AR_COMMAND "<CMAKE_AR> ${CMAKE_CURRENT_LIST_DIR}/../bazel/toolchains/wrappers_tasking_win_host/artc.py -r <TARGET> <OBJECTS>")

set(CMAKE_C_ARCHIVE_CREATE ${AR_COMMAND})
set(CMAKE_CXX_ARCHIVE_CREATE ${AR_COMMAND})
set(CMAKE_C_ARCHIVE_CREATE ${AR_COMMAND})
set(CMAKE_C_ARCHIVE_APPEND ${AR_COMMAND})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --force-c++ --pending-instantiations=200")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wcp --no-warnings=68 -Wcp --no-warnings=1383 -Wcp --no-warnings=1385") #workaround to suppress warnings caused by --warnings-as-errors

set(tasking_linker_folder "${CMAKE_CURRENT_LIST_DIR}/../linker/tasking")

file(GLOB Linker_Files "${tasking_linker_folder}/*.lsl" "${tasking_linker_folder}/*.ld")
set(Linker_Script "${tasking_linker_folder}/tc49x.ld")
set(Tasking_Linker_Flags "-d ${Linker_Script} --lsl-core=tc0 --pass-linker=-DNOXC800INIT")

set(CMAKE_EXECUTABLE_SUFFIX_C        .elf)
set(CMAKE_EXECUTABLE_SUFFIX_CXX      .elf)

set(CMAKE_STATIC_LIBRARY_SUFFIX_C    .a)
set(CMAKE_STATIC_LIBRARY_SUFFIX_CXX  .a)