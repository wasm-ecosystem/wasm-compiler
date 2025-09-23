load("//src:config.bzl", "compile_defines", "compile_features", "compile_options")

# enhancement features for wasm compiler
cc_library(
    name = "extensions",
    srcs = glob([
        "*.cpp",
    ]),
    hdrs = ["//src:headers"] + glob([
        "*.hpp",
    ]),
    copts = compile_options + select({
        "@bazel_tools//src/conditions:windows": ["/std:c++20"],
        "//conditions:default": ["--std=c++20"],
    }),
    defines = compile_defines,
    features = compile_features,
    visibility = ["//visibility:public"],
)
