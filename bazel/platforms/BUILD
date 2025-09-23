platform(
    name = "x86_64_linux",
    constraint_values = [
        "//bazel/platforms/cpu:x86_64",
        "@platforms//os:linux",
    ],
)

platform(
    name = "aarch64_linux",
    constraint_values = [
        "//bazel/platforms/cpu:aarch64",
        "@platforms//os:linux",
    ],
)

platform(
    name = "x86_64_qnx",
    constraint_values = [
        "//bazel/platforms/cpu:x86_64",
        "@platforms//os:qnx",
    ],
)

platform(
    name = "aarch64_qnx",
    constraint_values = [
        "//bazel/platforms/cpu:aarch64",
        "@platforms//os:qnx",
    ],
)

platform(
    name = "tricore_gcc",
    constraint_values = [
        "//bazel/platforms/cpu:tricore",
        "//bazel/platforms/compiler:gcc",
        "@platforms//os:none",
    ],
)

platform(
    name = "tricore_tasking",
    constraint_values = [
        "//bazel/platforms/cpu:tricore",
        "//bazel/platforms/compiler:tasking",
        "@platforms//os:none",
    ],
)

config_setting(
    name = "config_tricore_gcc",
    constraint_values = [
        "//bazel/platforms/cpu:tricore",
        "//bazel/platforms/compiler:gcc",
    ],
)

config_setting(
    name = "config_tricore_tasking",
    constraint_values = [
        "//bazel/platforms/cpu:tricore",
        "//bazel/platforms/compiler:tasking",
    ],
)
