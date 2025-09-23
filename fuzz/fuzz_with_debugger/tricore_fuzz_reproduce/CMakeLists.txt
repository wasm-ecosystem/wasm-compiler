project(tricore_fuzz_reproduce)


aux_source_directory(${CMAKE_CURRENT_LIST_DIR} sourceFiles)

add_executable(${PROJECT_NAME} ${sourceFiles})

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
