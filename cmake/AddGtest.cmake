macro(add_test_case target_file)

    if(NOT DEFINED CMAKE_CROSSCOMPILING_EMULATOR)
        gtest_add_tests(TARGET      
            ${target_file}
        )
    elseif("${CMAKE_CROSSCOMPILING_EMULATOR}" MATCHES "qemu-aarch64")
        add_test(NAME ${target_file} COMMAND "qemu-aarch64" -L /usr/aarch64-linux-gnu ${CMAKE_CURRENT_BINARY_DIR}/${target_file})
    else()
        message(FATAL_ERROR "unsupported CMAKE_CROSSCOMPILING_EMULATOR for testing")
    endif()

endmacro()

find_package(GTest REQUIRED)