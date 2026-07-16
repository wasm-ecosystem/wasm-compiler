## Reference

## See `wasm-compiler/.github/workflows/main.yml` to get the latest approach of setup
~~The following introduction may be outdated~~

## Test the Project

### Install Dependencies

- python3
- wabt@1.0.41: https://github.com/WebAssembly/wabt/releases/download/1.0.41/, add build_target_dir to PATH
- binaryen@122: https://github.com/WebAssembly/binaryen/releases/download/version_122, add build_target_dir to PATH

### Build Spectest

- cmake
  - `-DENABLE_SPECTEST=on`
- bazel
  - `bazel build //tests:vb_spectest_json --platforms={platform_configuration}`

### Run Spectest

- in developer host machine, both three way is available

  ```bash
  ./build/bin/vb_spectest tests/testsuite/
  ```

  ```bash
  python3 tests/spectest.py # generate testcases
  ./build/bin/vb_spectest_json tests/testcases.json
  ```

- on an x86_64 host targeting AArch64 Linux via qemu

  ```bash
  cmake -S . -B build_linux_arm -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 -DBACKEND=aarch64 -DENABLE_SPECTEST=1 -DTEST_VARIANTS=1 -DCMAKE_CROSSCOMPILING_EMULATOR=qemu-aarch64
  cmake --build build_linux_arm --target vb_spectest -j4
  ctest --test-dir build_linux_arm -R spectest --output-on-failure
  ```

- in embedded device, need to generate json or binary in host machine and run test

  ```bash
  python3 tests/spectest.py # generate testcases in host
  ```

  copy `build/bin/vb_spectest_json` and `tests/testcases.json` to device

  ```bash
  ./vb_spectest_json tests/testcases.json # run test in embedded device
  ```

#### Run spectest with tricore qemu

```shell
$TRICORE_QEMU_PATH/qemu-system-tricore -semihosting -display none -M tricore_tsim162 -kernel build/bin/vb_spectest_binary_standalone_<0/1>
```

#### Run spectest with Lauterbach debugger on Tricore MCU

Run cmm/spectest_flash.cmm in t32mtc.exe

Often used commands
| | Command |
| ----------- | ----------- |
|Set break point|`Break.Set \SingleCaseTest\530` or `Break.Set \"c:\t32\myProject\myFile.c"\42`|

### Run Codegen test

```bash
sudo apt install python3-venv
mkdir -p venv && python3 -m venv ./venv
source ./venv/bin/activate
python3 binding/binding_all.py
python3 scripts/codegen_test.py --no-color
```

### Run Disassembler Explorer Server

After binding in venv(python3 binding/binding_all.py)
```bash
python3 scripts/explorer_server.py
```