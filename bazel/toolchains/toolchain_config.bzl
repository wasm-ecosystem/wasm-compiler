""" cpp toolchain config """

load("@bazel_tools//tools/build_defs/cc:action_names.bzl", "ACTION_NAMES")
load(
    "@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
    "feature",
    "flag_group",
    "flag_set",
    "tool_path",
)

all_link_actions = [
    ACTION_NAMES.cpp_link_executable,
    ACTION_NAMES.cpp_link_dynamic_library,
    ACTION_NAMES.cpp_link_nodeps_dynamic_library,
]

cpp_compile_actions = [
    ACTION_NAMES.cpp_compile,
    ACTION_NAMES.cpp_header_parsing,
    ACTION_NAMES.cpp_module_compile,
    ACTION_NAMES.cpp_module_codegen,
]

c_compile_actions = [ACTION_NAMES.c_compile]

compile_actions = cpp_compile_actions + c_compile_actions

gnu_compile_flags_feature = feature(
    name = "default_compile_flags",
    enabled = True,
    flag_sets = [
        flag_set(
            actions = compile_actions,
            flag_groups = [flag_group(flags = [
                "-pedantic",
            ])],
        ),
    ],
)

gnu_cpp_compile_flags_feature = feature(
    name = "default_cpp_compile_flags",
    enabled = True,
    flag_sets = [
        flag_set(
            actions = cpp_compile_actions,
            flag_groups = [flag_group(flags = [
                "-Wall",
                "-std=gnu++14",
                "-Wnon-virtual-dtor",
                "-Wold-style-cast",
                "-Wsign-conversion",
                "-Wconversion",
                "-Wduplicated-branches",
                "-Wcast-qual",
                "-Wimplicit-fallthrough=5",
                "-Wformat=2",
                "-Wformat-security",
                "-Werror=format-security",
                "-Wformat-signedness",
                "-Wcast-align",
                "-Wdouble-promotion",
                "-Wfloat-equal",
                "-Wredundant-decls",
                "-Wshadow",
                "-Wswitch",
                "-Wuninitialized",
                "-Wunused-parameter",
                "-Walloc-zero",
                "-Walloca",
                "-Wduplicated-cond",
                "-Wlogical-op",
            ])],
        ),
    ],
)

tricore_cpp_compile_flags_feature = feature(
    name = "tricore_cpp_compile_flags",
    enabled = True,
    flag_sets = [
        flag_set(
            actions = compile_actions + all_link_actions,
            flag_groups = [flag_group(flags = [
                "-MMD",  # avoid tracking dependencies on system headers
                "-mcpu=tc39xx",
            ])],
        ),
    ],
)

gnu_c_compile_flags_feature = feature(
    name = "default_c_compile_flags",
    enabled = True,
    flag_sets = [
        flag_set(
            actions = c_compile_actions,
            flag_groups = [flag_group(flags = [
                "-std=gnu11",
            ])],
        ),
    ],
)

qcc_compile_flags_feature = feature(
    name = "default_compile_flags",
    enabled = True,
    flag_sets = [
        flag_set(
            actions = compile_actions,
            flag_groups = [flag_group(flags = [
                "-D_QNX_SOURCE",
            ])],
        ),
    ],
)

features_link_stdcpp = feature(
    name = "default_linker_flags",
    enabled = True,
    flag_sets = [
        flag_set(
            actions = all_link_actions,
            flag_groups = ([
                flag_group(
                    flags = [
                        "-lstdc++",
                        "-lpthread",
                    ],
                ),
            ]),
        ),
    ],
)

features_link_stdcpp_tricore_gcc = feature(
    name = "default_linker_flags",
    enabled = True,
    flag_sets = [
        flag_set(
            actions = all_link_actions,
            flag_groups = ([
                flag_group(
                    flags = [
                        "-lstdc++",
                    ],
                ),
            ]),
        ),
    ],
)

def _impl_x86_64_linux(ctx):
    out = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.write(out, "linux x86_64 executable")
    features = [gnu_compile_flags_feature, gnu_c_compile_flags_feature, gnu_cpp_compile_flags_feature, features_link_stdcpp]
    tool_paths = [
        tool_path(
            name = "ar",
            path = "/usr/bin/ar",
        ),
        tool_path(
            name = "cpp",
            path = "/usr/bin/cpp",
        ),
        tool_path(
            name = "gcc",
            path = "/usr/bin/gcc",
        ),
        tool_path(
            name = "gcov",
            path = "/usr/bin/gcov",
        ),
        tool_path(
            name = "ld",
            path = "/usr/bin/ld",
        ),
        tool_path(
            name = "nm",
            path = "/usr/bin/nm",
        ),
        tool_path(
            name = "objdump",
            path = "/usr/bin/objdump",
        ),
        tool_path(
            name = "strip",
            path = "/usr/bin/strip",
        ),
    ]
    return [
        cc_common.create_cc_toolchain_config_info(
            ctx = ctx,
            features = features,
            cxx_builtin_include_directories = [
                "/usr/include/",
                "/usr/lib/gcc/x86_64-linux-gnu/9/include",
                "/usr/lib/gcc/x86_64-linux-gnu/10/include",
                "/usr/lib/gcc/x86_64-linux-gnu/11/include",
                "/usr/lib/gcc/x86_64-linux-gnu/13/include",
            ],
            toolchain_identifier = "linux-x86_64-toolchain",
            host_system_name = "linux",
            target_system_name = "nothing",
            target_cpu = "x86_64",
            target_libc = "nothing",
            cc_target_os = "linux",
            compiler = "gcc",
            abi_version = "nothing",
            abi_libc_version = "eleventy",
            tool_paths = tool_paths,
        ),
        DefaultInfo(
            executable = out,
        ),
    ]

def _impl_aarch64_linux(ctx):
    out = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.write(out, "linux aarch64 executable")
    features = [gnu_compile_flags_feature, gnu_c_compile_flags_feature, gnu_cpp_compile_flags_feature, features_link_stdcpp]
    tool_paths = [
        tool_path(
            name = "ar",
            path = "/usr/bin/aarch64-linux-gnu-ar",
        ),
        tool_path(
            name = "cpp",
            path = "/usr/bin/aarch64-linux-gnu-cpp",
        ),
        tool_path(
            name = "gcc",
            path = "/usr/bin/aarch64-linux-gnu-gcc",
        ),
        tool_path(
            name = "gcov",
            path = "/usr/bin/aarch64-linux-gnu-gcov",
        ),
        tool_path(
            name = "ld",
            path = "/usr/bin/aarch64-linux-gnu-ld",
        ),
        tool_path(
            name = "nm",
            path = "/usr/bin/aarch64-linux-gnu-nm",
        ),
        tool_path(
            name = "objdump",
            path = "/usr/bin/aarch64-linux-gnu-objdump",
        ),
        tool_path(
            name = "strip",
            path = "/usr/bin/aarch64-linux-gnu-strip",
        ),
    ]
    return [
        cc_common.create_cc_toolchain_config_info(
            ctx = ctx,
            features = features,
            cxx_builtin_include_directories = [
                "/usr/aarch64-linux-gnu/include",
                "/usr/lib/gcc-cross/aarch64-linux-gnu/9/include",
                "/usr/lib/gcc-cross/aarch64-linux-gnu/10/include",
                "/usr/lib/gcc-cross/aarch64-linux-gnu/11/include",
                "/usr/lib/gcc-cross/aarch64-linux-gnu/13/include",
            ],
            toolchain_identifier = "linux-aarch64-toolchain",
            host_system_name = "linux",
            target_system_name = "nothing",
            target_cpu = "aarch64",
            target_libc = "nothing",
            cc_target_os = "linux",
            compiler = "gcc",
            abi_version = "nothing",
            abi_libc_version = "eleventy",
            tool_paths = tool_paths,
        ),
        DefaultInfo(
            executable = out,
        ),
    ]

def _impl_aarch64_qnx(ctx):
    out = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.write(out, "qnx aarch64 executable")

    dependency_file_feature = feature(
        name = "dependency_file",
        enabled = False,
    )

    aarch64_qcc_compile_flags_feature = feature(
        name = "aarch64_qcc_compile_flags_feature",
        enabled = True,
        flag_sets = [
            flag_set(
                actions = compile_actions + all_link_actions,
                flag_groups = [
                    flag_group(flags = [
                        "-V8.3.0,gcc_ntoaarch64le",
                        "-march=armv8.1-a",
                    ]),
                ],
            ),
        ],
    )

    features_qcc_stdcpp = feature(
        name = "default_linker_flags",
        enabled = True,
        flag_sets = [
            flag_set(
                actions = all_link_actions,
                flag_groups = ([
                    flag_group(
                        flags = [
                            "-lang-c++",
                        ],
                    ),
                ]),
            ),
        ],
    )

    features = [gnu_compile_flags_feature, gnu_c_compile_flags_feature, gnu_cpp_compile_flags_feature, qcc_compile_flags_feature, aarch64_qcc_compile_flags_feature, features_qcc_stdcpp, dependency_file_feature]

    tool_paths = [
        tool_path(
            name = "ar",
            path = "wrappers_aarch64/ar",
        ),
        tool_path(
            name = "cpp",
            path = "wrappers_aarch64/cpp",
        ),
        tool_path(
            name = "gcc",
            path = "wrappers_aarch64/qcc",
        ),
        tool_path(
            name = "gcov",
            path = "wrappers_aarch64/gcov",
        ),
        tool_path(
            name = "ld",
            path = "wrappers_aarch64/ld",
        ),
        tool_path(
            name = "nm",
            path = "wrappers_aarch64/nm",
        ),
        tool_path(
            name = "objdump",
            path = "wrappers_aarch64/objdump",
        ),
        tool_path(
            name = "strip",
            path = "wrappers_aarch64/strip",
        ),
    ]
    return [
        cc_common.create_cc_toolchain_config_info(
            ctx = ctx,
            features = features,
            cxx_builtin_include_directories = [],
            toolchain_identifier = "qnx-aarch64-toolchain",
            host_system_name = "linux",
            target_system_name = "nothing",
            target_cpu = "aarch64",
            target_libc = "nothing",
            cc_target_os = "qnx",
            compiler = "qcc",
            abi_version = "gcc-8.3",
            abi_libc_version = "eleventy",
            tool_paths = tool_paths,
        ),
        DefaultInfo(
            executable = out,
        ),
    ]

def _impl_x86_64_qnx(ctx):
    out = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.write(out, "qnx x86_64 executable")

    dependency_file_feature = feature(
        name = "dependency_file",
        enabled = False,
    )

    features_qcc_stdcpp = feature(
        name = "default_linker_flags",
        enabled = True,
        flag_sets = [
            flag_set(
                actions = all_link_actions,
                flag_groups = ([
                    flag_group(
                        flags = [
                            "-lang-c++",
                        ],
                    ),
                ]),
            ),
        ],
    )

    features = [gnu_compile_flags_feature, gnu_c_compile_flags_feature, gnu_cpp_compile_flags_feature, qcc_compile_flags_feature, features_qcc_stdcpp, dependency_file_feature]

    tool_paths = [
        tool_path(
            name = "ar",
            path = "wrappers_x86_64/ar",
        ),
        tool_path(
            name = "cpp",
            path = "wrappers_x86_64/cpp",
        ),
        tool_path(
            name = "gcc",
            path = "wrappers_x86_64/qcc",
        ),
        tool_path(
            name = "gcov",
            path = "wrappers_x86_64/gcov",
        ),
        tool_path(
            name = "ld",
            path = "wrappers_x86_64/ld",
        ),
        tool_path(
            name = "nm",
            path = "wrappers_x86_64/nm",
        ),
        tool_path(
            name = "objdump",
            path = "wrappers_x86_64/objdump",
        ),
        tool_path(
            name = "strip",
            path = "wrappers_x86_64/strip",
        ),
    ]
    return [
        cc_common.create_cc_toolchain_config_info(
            ctx = ctx,
            features = features,
            cxx_builtin_include_directories = [],
            toolchain_identifier = "qnx-x86_64-toolchain",
            host_system_name = "linux",
            target_system_name = "nothing",
            target_cpu = "x86_64",
            target_libc = "nothing",
            cc_target_os = "qnx",
            compiler = "qcc",
            abi_version = "gcc-8.3",
            abi_libc_version = "eleventy",
            tool_paths = tool_paths,
        ),
        DefaultInfo(
            executable = out,
        ),
    ]

def _impl_tricore_gcc(ctx):
    out = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.write(out, "gcc tricore executable")
    features = [gnu_compile_flags_feature, gnu_c_compile_flags_feature, gnu_cpp_compile_flags_feature, features_link_stdcpp_tricore_gcc, tricore_cpp_compile_flags_feature]
    tool_paths = [
        tool_path(
            name = "ar",
            path = "wrappers_tricore_gcc/ar",
        ),
        tool_path(
            name = "cpp",
            path = "wrappers_tricore_gcc/cpp",
        ),
        tool_path(
            name = "gcc",
            path = "wrappers_tricore_gcc/gcc",
        ),
        tool_path(
            name = "gcov",
            path = "wrappers_tricore_gcc/gcov",
        ),
        tool_path(
            name = "ld",
            path = "wrappers_tricore_gcc/ld",
        ),
        tool_path(
            name = "nm",
            path = "wrappers_tricore_gcc/nm",
        ),
        tool_path(
            name = "objdump",
            path = "wrappers_tricore_gcc/objdump",
        ),
        tool_path(
            name = "strip",
            path = "wrappers_tricore_gcc/strip",
        ),
    ]
    return [
        cc_common.create_cc_toolchain_config_info(
            ctx = ctx,
            features = features,
            cxx_builtin_include_directories = [],
            toolchain_identifier = "tricore-gcc-toolchain",
            host_system_name = "linux",
            target_system_name = "nothing",
            target_cpu = "tricore",
            target_libc = "nothing",
            cc_target_os = "none",
            compiler = "gcc",
            abi_version = "nothing",
            abi_libc_version = "eleventy",
            tool_paths = tool_paths,
        ),
        DefaultInfo(
            executable = out,
        ),
    ]

x86_64_linux_toolchain_config = rule(
    implementation = _impl_x86_64_linux,
    provides = [CcToolchainConfigInfo],
    executable = True,
)

aarch64_linux_toolchain_config = rule(
    implementation = _impl_aarch64_linux,
    provides = [CcToolchainConfigInfo],
    executable = True,
)

x86_64_qnx_toolchain_config = rule(
    implementation = _impl_x86_64_qnx,
    provides = [CcToolchainConfigInfo],
    executable = True,
)

aarch64_qnx_toolchain_config = rule(
    implementation = _impl_aarch64_qnx,
    provides = [CcToolchainConfigInfo],
    executable = True,
)

tricore_gcc_toolchain_config = rule(
    implementation = _impl_tricore_gcc,
    provides = [CcToolchainConfigInfo],
    executable = True,
)
