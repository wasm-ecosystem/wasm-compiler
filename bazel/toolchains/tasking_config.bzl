""" tricore tasking toolchain config """

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

link_actions = [
    ACTION_NAMES.cpp_link_static_library,
    ACTION_NAMES.cpp_link_dynamic_library,
    ACTION_NAMES.cpp_link_nodeps_dynamic_library,
]

compile_actions = cpp_compile_actions + c_compile_actions

random_seed_feature = feature(
    name = "random_seed",
    enabled = False,
)

cctc_compile_flags_feature = feature(
    name = "default_cpp_compile_flags",
    enabled = True,
    flag_sets = [
        flag_set(
            actions = cpp_compile_actions + all_link_actions,
            flag_groups = [flag_group(flags = [
                "--force-c++",
                "--pending-instantiations=200",
            ])],
        ),
    ],
)

archive_param_file_feature = feature(
    name = "archive_param_file",
    enabled = True,
)

def _impl_tasking_tricore(ctx):
    out = ctx.actions.declare_file(ctx.label.name)
    ctx.actions.write(out, "tasking tricore elf")
    features = [cctc_compile_flags_feature, random_seed_feature, archive_param_file_feature]
    tool_paths = [
        tool_path(
            name = "ar",
            path = "wrappers_tasking_win_host/artc.bat",
        ),
        tool_path(
            name = "cpp",
            path = "cctc",
        ),
        tool_path(
            name = "gcc",
            path = "wrappers_tasking_win_host/cctc.bat",
        ),
        tool_path(
            name = "gcov",
            path = "none",
        ),
        tool_path(
            name = "ld",
            path = "ldtc",
        ),
        tool_path(
            name = "nm",
            path = "none",
        ),
        tool_path(
            name = "objdump",
            path = "elfdump",
        ),
        tool_path(
            name = "strip",
            path = "none",
        ),
    ]
    return [
        cc_common.create_cc_toolchain_config_info(
            ctx = ctx,
            features = features,
            cxx_builtin_include_directories = [
            ],
            toolchain_identifier = "tasking-tricore-toolchain",
            host_system_name = "windows",
            target_system_name = "nothing",
            target_cpu = "tricore",
            target_libc = "nothing",
            cc_target_os = "nothing",
            compiler = "tasking",
            abi_version = "nothing",
            abi_libc_version = "eleventy",
            tool_paths = tool_paths,
        ),
        DefaultInfo(
            executable = out,
        ),
    ]

tricore_tasking_toolchain_config = rule(
    implementation = _impl_tasking_tricore,
    provides = [CcToolchainConfigInfo],
    executable = True,
)
