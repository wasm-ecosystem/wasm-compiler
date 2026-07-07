load("@rules_cc//cc:defs.bzl", "cc_library")
load("//src:config.bzl", "compile_defines", "compile_features", "compile_options")

config_setting(
    name = "enable_extensions",
    define_values = {
        "ENABLE_EXTENSIONS": "1",
    },
)

extension_defines = select({
    ":enable_extensions": ["ENABLE_EXTENSIONS=1"],
    "//conditions:default": [],
})

extension_deps = select({
    ":enable_extensions": ["//extensions:extensions"],
    "//conditions:default": [],
})

filegroup(
    name = "headers",
    srcs = glob([
        "**/*.hpp",
    ]),
    visibility = ["//visibility:public"],
)

# cross platform library for runtime
cc_library(
    name = "utils",
    srcs = glob(
        [
            "utils/*.cpp",
        ],
    ),
    hdrs = [":headers"],
    copts = compile_options,
    defines = compile_defines,
    features = compile_features,
    include_prefix = "wasm-compiler/src",
    visibility = ["//visibility:private"],
)

cc_library(
    name = "core",
    srcs = glob(
        [
            "**/*.cpp",
        ],
        exclude = [
            "core/compiler/analytics/*.cpp",
            "utils/*.cpp",
        ],
    ),
    hdrs = [":headers"],
    copts = compile_options,
    defines = compile_defines,
    features = compile_features,
    # see https://stackoverflow.com/questions/63468630/bazel-how-to-add-prefix-to-dependency
    include_prefix = "wasm-compiler/src",
    visibility = ["//visibility:private"],
    deps = [
        "//thirdparty:berkeley-softfloat-3-headers",
    ],
)

# core lib for unix / windows integration
cc_library(
    name = "wasm_module",
    srcs = [],
    hdrs = [":headers"],
    copts = compile_options,
    defines = compile_defines + extension_defines,
    features = compile_features,
    include_prefix = "wasm-compiler/src",
    visibility = ["//visibility:public"],
    deps = extension_deps + [
        ":core",
        ":utils",
    ],
)

# core lib for embedded devices without extension
cc_library(
    name = "wasm_module_embedded",
    srcs = [],
    hdrs = [":headers"],
    copts = compile_options,
    defines = compile_defines,
    features = compile_features,
    include_prefix = "wasm-compiler/src",
    visibility = ["//visibility:public"],
    deps = [
        ":core",
    ],
)
