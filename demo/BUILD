load("@rules_cc//cc:defs.bzl", "cc_binary")

platform_related_linkers = select({
    "//bazel/platforms/cpu:x86_64": [],
    "//bazel/platforms/cpu:aarch64": [],
    "//bazel/platforms:config_tricore_gcc": ["//linker:GNUTricore.ld"],
    "//bazel/platforms:config_tricore_tasking": ["//linker/tasking:tc49x.ld"],
    "//conditions:default": [],
})

platform_related_linkopts = select({
    "//bazel/platforms/cpu:x86_64": [],
    "//bazel/platforms/cpu:aarch64": [],
    "//bazel/platforms:config_tricore_gcc": ["-Wl,-T$(location //linker:GNUTricore.ld)"],
    "//bazel/platforms:config_tricore_tasking": [
        "-d $(location //linker/tasking:tc49x.ld)",
        "--lsl-core=tc0",
        "--pass-linker=-DNOXC800INIT",
    ],
    "//conditions:default": [],
})

platform_related_deps = select({
    "//bazel/platforms/cpu:x86_64": ["//src:wasm_module"],
    "//bazel/platforms/cpu:aarch64": ["//src:wasm_module"],
    "//bazel/platforms:config_tricore_gcc": [
        "//src:wasm_module_embedded",
        "//thirdparty:berkeley-softfloat-3",
    ],
    "//bazel/platforms:config_tricore_tasking": [
        "//src:wasm_module_embedded",
        "//thirdparty:berkeley-softfloat-3",
    ],
    "//conditions:default": ["//src:wasm_module"],
})

cc_binary(
    name = "vb_demo",
    srcs = ["vb_demo.cpp"],
    linkopts = platform_related_linkopts,
    deps = platform_related_linkers + platform_related_deps,
)
