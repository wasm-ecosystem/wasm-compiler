load("@rules_cc//cc:defs.bzl", "cc_toolchain")
load("tasking_config.bzl", "tricore_tasking_toolchain_config")
load("toolchain_config.bzl", "aarch64_linux_toolchain_config", "aarch64_qnx_toolchain_config", "tricore_gcc_toolchain_config", "x86_64_linux_toolchain_config", "x86_64_qnx_toolchain_config")

x86_64_linux_toolchain_config(name = "x86_64_linux_toolchain_config")

aarch64_linux_toolchain_config(name = "aarch64_linux_toolchain_config")

x86_64_qnx_toolchain_config(name = "x86_64_qnx_toolchain_config")

aarch64_qnx_toolchain_config(name = "aarch64_qnx_toolchain_config")

tricore_gcc_toolchain_config(name = "tricore_gcc_toolchain_config")

tricore_tasking_toolchain_config(name = "tricore_tasking_toolchain_config")

filegroup(
    name = "toolchain_files",
)

filegroup(
    name = "wrappers_x86_64_files",
    srcs = glob(["wrappers_x86_64/*"]),
)

filegroup(
    name = "x86_64_qnx_toolchain_files",
    srcs = [
        ":wrappers_x86_64_files",
    ],
)

filegroup(
    name = "wrappers_aarch64_files",
    srcs = glob(["wrappers_aarch64/*"]),
)

filegroup(
    name = "aarch64_qnx_toolchain_files",
    srcs = [
        ":wrappers_aarch64_files",
    ],
)

filegroup(
    name = "wrappers_tricore_gcc_files",
    srcs = glob(["wrappers_tricore_gcc/*"]),
)

filegroup(
    name = "tricore_gcc_toolchain_files",
    srcs = [
        ":wrappers_tricore_gcc_files",
    ],
)

cc_toolchain(
    name = "x86_64_cc_toolchain_definition",
    all_files = ":toolchain_files",
    compiler_files = ":toolchain_files",
    dwp_files = ":toolchain_files",
    linker_files = ":toolchain_files",
    objcopy_files = ":toolchain_files",
    strip_files = ":toolchain_files",
    toolchain_config = ":x86_64_linux_toolchain_config",
)

toolchain(
    name = "x86_64_linux_toolchain",
    target_compatible_with = [
        "//bazel/platforms/cpu:x86_64",
        "@platforms//os:linux",
    ],
    toolchain = ":x86_64_cc_toolchain_definition",
    toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
)

cc_toolchain(
    name = "aarch64_cc_toolchain_definition",
    all_files = ":toolchain_files",
    compiler_files = ":toolchain_files",
    dwp_files = ":toolchain_files",
    linker_files = ":toolchain_files",
    objcopy_files = ":toolchain_files",
    strip_files = ":toolchain_files",
    toolchain_config = ":aarch64_linux_toolchain_config",
)

toolchain(
    name = "aarch64_linux_toolchain",
    target_compatible_with = [
        "//bazel/platforms/cpu:aarch64",
        "@platforms//os:linux",
    ],
    toolchain = ":aarch64_cc_toolchain_definition",
    toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
)

cc_toolchain(
    name = "x86_64_qnx_toolchain_definition",
    all_files = ":x86_64_qnx_toolchain_files",
    ar_files = ":x86_64_qnx_toolchain_files",
    compiler_files = ":x86_64_qnx_toolchain_files",
    dwp_files = ":x86_64_qnx_toolchain_files",
    linker_files = ":x86_64_qnx_toolchain_files",
    objcopy_files = ":x86_64_qnx_toolchain_files",
    strip_files = ":x86_64_qnx_toolchain_files",
    toolchain_config = ":x86_64_qnx_toolchain_config",
)

toolchain(
    name = "x86_64_qnx_toolchain",
    target_compatible_with = [
        "//bazel/platforms/cpu:x86_64",
        "@platforms//os:qnx",
    ],
    toolchain = ":x86_64_qnx_toolchain_definition",
    toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
)

cc_toolchain(
    name = "aarch64_qnx_toolchain_definition",
    all_files = ":aarch64_qnx_toolchain_files",
    ar_files = ":aarch64_qnx_toolchain_files",
    compiler_files = ":aarch64_qnx_toolchain_files",
    dwp_files = ":aarch64_qnx_toolchain_files",
    linker_files = ":aarch64_qnx_toolchain_files",
    objcopy_files = ":aarch64_qnx_toolchain_files",
    strip_files = ":aarch64_qnx_toolchain_files",
    toolchain_config = ":aarch64_qnx_toolchain_config",
)

toolchain(
    name = "aarch64_qnx_toolchain",
    target_compatible_with = [
        "//bazel/platforms/cpu:aarch64",
        "@platforms//os:qnx",
    ],
    toolchain = ":aarch64_qnx_toolchain_definition",
    toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
)

cc_toolchain(
    name = "tricore_gcc_toolchain_definition",
    all_files = ":tricore_gcc_toolchain_files",
    ar_files = ":tricore_gcc_toolchain_files",
    compiler_files = ":tricore_gcc_toolchain_files",
    dwp_files = ":tricore_gcc_toolchain_files",
    linker_files = ":tricore_gcc_toolchain_files",
    objcopy_files = ":tricore_gcc_toolchain_files",
    strip_files = ":tricore_gcc_toolchain_files",
    toolchain_config = ":tricore_gcc_toolchain_config",
)

toolchain(
    name = "tricore_gcc_toolchain",
    target_compatible_with = [
        "//bazel/platforms/cpu:tricore",
        "@platforms//os:none",
        "//bazel/platforms/compiler:gcc",
    ],
    toolchain = ":tricore_gcc_toolchain_definition",
    toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
)

cc_toolchain(
    name = "tricore_tasking_toolchain_definition",
    all_files = ":toolchain_files",
    compiler_files = ":toolchain_files",
    dwp_files = ":toolchain_files",
    linker_files = ":toolchain_files",
    objcopy_files = ":toolchain_files",
    strip_files = ":toolchain_files",
    toolchain_config = ":tricore_tasking_toolchain_config",
)

toolchain(
    name = "tricore_tasking_toolchain",
    target_compatible_with = [
        "//bazel/platforms/cpu:tricore",
        "@platforms//os:none",
        "//bazel/platforms/compiler:tasking",
    ],
    toolchain = ":tricore_tasking_toolchain_definition",
    toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
)
