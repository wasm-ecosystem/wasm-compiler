
add_subdirectory(fuzz_with_debugger)

if(NOT FUZZ_ONLY_WITH_DEBUGGER)
    add_subdirectory(corpus)
    add_subdirectory(reference)
    add_subdirectory(afl_harness)
endif()
