macro(add_install_target)


file(GLOB_RECURSE headerFiles src/*.hpp)


set(LibraryDir wasm-compiler)

set(installTargets vb_libcompiler vb_libruntime)

if(TARGET vb_libutils)
    list(APPEND installTargets vb_libutils)
endif()

if(TARGET lib_softfloat)
    list(APPEND installTargets lib_softfloat)
endif()

list(APPEND installTargets vb_lib_core_common)

set(installLibs)

INSTALL(TARGETS ${installTargets}
        EXPORT ${PROJECT_NAME}
        LIBRARY DESTINATION ${LibraryDir}
)

foreach(FILE ${headerFiles})
   # remove the prefix path from the filename
   file(RELATIVE_PATH FILE_REL_PATH ${CMAKE_CURRENT_LIST_DIR} ${FILE})

   # create the destination directory structure
   get_filename_component(DST_DIR ${FILE_REL_PATH} DIRECTORY)
   set(DST "${LibraryDir}/${DST_DIR}")

   # install the file
   install(FILES ${FILE}
           DESTINATION ${DST}
           )
endforeach(FILE)

configure_file(./cmake/installConfig.cmake ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake @ONLY)
install(EXPORT ${PROJECT_NAME} DESTINATION ${LibraryDir}/cmake/${PROJECT_NAME})
install(FILES ${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake DESTINATION ./)

    
endmacro(add_install_target)
