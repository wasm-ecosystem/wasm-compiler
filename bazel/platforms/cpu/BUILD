package(
    default_visibility = ["//visibility:public"],
)

constraint_setting(name = "cpu")

# TODO(b/136237408): Remove this generic CPU name and replace with a specific one.
alias(
    name = "aarch64",
    actual = ":arm64",
)

constraint_value(
    name = "arm64",
    constraint_setting = ":cpu",
)

constraint_value(
    name = "x86_64",
    constraint_setting = ":cpu",
)

constraint_value(
    name = "tricore",
    constraint_setting = ":cpu",
)
