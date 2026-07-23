---
applyTo: "tests/**"
---

# Testing Cheat Sheet

This project has two kinds of tests for the compiler backend: **codegen tests** (static disassembly checks) and **wast tests** (runtime execution checks).
This cheat sheet assumes you are in the root of the project and have already have all dependencies installed.

## Codegen Tests (FileCheck-based)

**Location:** `tests/codegen/*.wat`

Codegen tests verify the disassembly output of compiled WebAssembly functions using [LLVM FileCheck](https://llvm.org/docs/CommandGuide/FileCheck.html) directives embedded in WAT comments.

### How to write a new codegen test

1. Create `tests/codegen/my_feature.wat` with a WAT module.
1. Add FileCheck directives in comments for relevant backends.
1. Run: `python scripts/code_gen_test.py --backend aarch64 --case tests/codegen/my_feature.wat`
1. `--backend` and `--case` are optional.

### Tip: explore disassembly for debug

To see the disassembly output before writing CHECK directives, use the Python binding. Eg:

```python
import sys
sys.path.insert(0, 'build_binding_aarch64/binding')
import aarch64_vb_warp as vb
from helper import wasm_utils

wasm = wasm_utils.wat_to_wasm(path='tests/codegen/my_feature.wat')
compiler = vb.Compiler()
print(compiler.disassemble_wasm(wasm))
```

### How to run the codegen tests

If `wasm-compiler/venv` exists, reuse it. If `src` code changed, rebuild with `python binding/binding_all.py`.
Don't always rebuild the binding, as it takes a long time.

```bash
# Codegen tests
# `--backend` and `--case` are optional
python scripts/code_gen_test.py --backend aarch64 --case tests/codegen/foo.wat # run specific case on specific backend
```

## Wast Tests (Runtime Execution)

Wast tests execute WebAssembly modules and verify results using `assert_return`, `assert_trap`, etc.

### How to write a new wast test

1. Create `tests/testsuite/vb_my_feature.wast` with module + assertions.
2. Naming convention
  - Custom tests: `vb_*.wast` (e.g., `vb_call.wast`, `vb_arm64_imm_addr.wast`)
  - Architecture-specific tests: `vb_arm64_*.wast`, `vb_tricore_*.wast`

### Cheat sheet for wast tests

```bash
# x86 spectest on x86 host
cmake --build build/                                                 # rebuild
./build/bin/vb_spectest tests/testsuite/vb_my_test.wast             # single file
cd build && ctest -R spectest                                        # all spectest

# AArch64 spectest with qemu
cmake -S . -B build_linux_arm -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 -DBACKEND=aarch64 -DENABLE_SPECTEST=1 -DTEST_VARIANTS=1
cmake --build build_linux_arm --target vb_spectest -j4              # build ARM spectest binary
qemu-aarch64 -L /usr/aarch64-linux-gnu ./build_linux_arm/bin/vb_spectest tests/testsuite # run ARM spectest directly

# TriCore spectest with qemu
python3 ./tests/spectest.py -f
cmake . -B build_tricore -DCMAKE_BUILD_TYPE=Debug -DENABLE_WERROR=1 -DCMAKE_C_COMPILER=/usr/bin/tricore-elf-gcc -DVB_ENABLE_DEV_FEATURE=OFF -DCMAKE_CXX_COMPILER=/usr/bin/tricore-elf-g++ -DENABLE_SPECTEST=1 -DENABLE_DEMO=1 -DTEST_VARIANTS=1 -DDISABLE_SPECTEST_WAST=1 -DDISABLE_SPECTEST_JSON=1 -DENABLE_STANDALONE_TEST=1
cmake --build build_tricore --parallel
for spectest_binary in build_tricore/bin/vb_spectest_binary_standalone_*; do
  python3 scripts/tricore_qemu_execute.py qemu-system-tricore -semihosting -display none -M tricore_tsim162 -kernel "$spectest_binary"
done
qemu-system-tricore -semihosting -display none -M tricore_tsim162 -kernel build_tricore/bin/vb_demo

# TriCore spectest single case (example: vb_return_call)
# 1) Generate testcase data from a directory containing only one .wast
tmp_dir=/tmp/vb_single_case
rm -rf "$tmp_dir" && mkdir -p "$tmp_dir"
cp tests/testsuite/vb_return_call.wast "$tmp_dir"/
python3 ./tests/spectest.py -f --testsuite-path "$tmp_dir"

# 2) Rebuild tricore standalone binaries (they now contain only this case)
cmake . -B build_tricore -DCMAKE_BUILD_TYPE=Debug -DENABLE_WERROR=1 -DCMAKE_C_COMPILER=/usr/bin/tricore-elf-gcc -DVB_ENABLE_DEV_FEATURE=OFF -DCMAKE_CXX_COMPILER=/usr/bin/tricore-elf-g++ -DENABLE_SPECTEST=1 -DENABLE_DEMO=1 -DTEST_VARIANTS=1 -DDISABLE_SPECTEST_WAST=1 -DDISABLE_SPECTEST_JSON=1 -DENABLE_STANDALONE_TEST=1
cmake --build build_tricore --parallel

# 3) Run standalone_0 (single-case payload will be in total_0.cpp)
python3 scripts/tricore_qemu_execute.py qemu-system-tricore -semihosting -display none -M tricore_tsim162 -kernel build_tricore/bin/vb_spectest_binary_standalone_0
```

## Unit Tests

Unit tests are built into the `Unittests` binary under `build/tests/unittests/`.

### Running unit tests

```bash
# Build unit tests
cmake --build build --target Unittests -j4

# Run all unit tests
./build/tests/unittests/Unittests

# Run a filtered subset of unit tests
./build/tests/unittests/Unittests --gtest_filter='*RegisterCopyResolver*:*ParallelMoveResolver*'
```
