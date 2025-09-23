macro(add_warning_flag)
    if(MSVC)
        add_definitions(-D_CRT_SECURE_NO_WARNINGS)

        add_compile_options(/W4 /EHsc)

        if(ENABLE_WERROR)
            add_compile_options(/WX)
        endif()
    elseif((CMAKE_CXX_COMPILER_ID STREQUAL "GNU") OR(CMAKE_CXX_COMPILER_ID STREQUAL "QCC") OR(CMAKE_CXX_COMPILER_ID STREQUAL "Clang") OR(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"))
        add_compile_options(-Wall -Wextra -Wpedantic  -Wformat=2
            -Wformat-security -Werror=format-security -Wcast-align -Wcast-qual -Wconversion
            -Wdouble-promotion -Wfloat-equal -Wmissing-include-dirs 
            -Wredundant-decls -Wshadow -Wsign-conversion -Wswitch -Wuninitialized
            -Wunused-parameter -Walloca -Wunused-result -Wunused-local-typedefs
            -Wwrite-strings -Wpointer-arith -Wfloat-conversion -Wnull-dereference -Wdiv-by-zero
            -Wswitch-default -Wno-switch-bool -Wunknown-pragmas 
        )

        set(cxx_warning_flags -Wnon-virtual-dtor -Wold-style-cast)

        add_compile_options(
            "$<$<COMPILE_LANGUAGE:CXX>:${cxx_warning_flags}>"
        )

        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            if((NOT(CMAKE_CXX_COMPILER_VERSION VERSION_LESS "11")) AND(NOT CMAKE_CROSSCOMPILING))
                add_compile_options(-Wformat-signedness -Walloc-zero -Wduplicated-branches -Wduplicated-cond -Wimplicit-fallthrough=5 -Wlogical-op
                    -Wenum-conversion -Wno-switch-unreachable -Wliteral-suffix -Wno-builtin-macro-redefined -Wno-unknown-warning-option -Wdeprecated-copy
                )
            endif()
        elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
            add_compile_options(-Wenum-conversion)
        endif()

        if(ENABLE_WERROR)
            add_compile_options(-Werror)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Tasking")
        if(ENABLE_WERROR)
            add_compile_options(
            --warnings-as-errors
            --pass-c=--warnings-as-errors=505,524,545,587
            --pass-c++=--no-warnings=68
            --pass-c++=--no-warnings=161
            --pass-c++=--no-warnings=1383
            --pass-c++=--no-warnings=1385
            --pass-c=--no-warnings=501
            --pass-c=--no-warnings=507
            --pass-c=--no-warnings=553 
            --pass-c=--no-warnings=560
            --pass-c=--no-warnings=562)
        endif()
    endif()
endmacro()

macro(add_msvc_parallel_opt)
    if(MSVC)
        option(ENABLE_MSVC_PARALLEL "Enable MSVC parallel compilation" ON)

        if(ENABLE_MSVC_PARALLEL)
            add_compile_options(/MP)
        endif()
    endif()
endmacro()
