
macro(AddLinkDependencies TARGET_NAME Linker_Files)
    
    list(JOIN Linker_Files "\;" Linker_File_Str)
    set_target_properties(${TARGET_NAME} PROPERTIES LINK_DEPENDS ${Linker_File_Str})
endmacro()
