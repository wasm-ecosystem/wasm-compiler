project(vb_debugger_fuzz)
add_executable(${PROJECT_NAME} fuzz_with_debugger.cpp)

target_link_libraries(${PROJECT_NAME}
    vb_libWasmModule
)

if(DEFINED Linker_Files)
    AddLinkDependencies(${PROJECT_NAME} Linker_Files)
endif()

target_link_libraries(${PROJECT_NAME}
    vb_libutils
)

EnableClangTidyMSVS(${PROJECT_NAME})

add_subdirectory(tricore_fuzz_reproduce)

