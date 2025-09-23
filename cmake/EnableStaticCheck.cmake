function(EnableClangTidyMSVS target)
    if((${CMAKE_GENERATOR} MATCHES "Visual Studio") AND ENABLE_CLANG_TIDY)
        set_target_properties(${target} PROPERTIES
            VS_GLOBAL_RunCodeAnalysis true
            VS_GLOBAL_EnableMicrosoftCodeAnalysis false
            VS_GLOBAL_EnableClangTidyCodeAnalysis true
        )
    endif()
	
endfunction()