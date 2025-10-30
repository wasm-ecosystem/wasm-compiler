## Compile the Project

### external dependencies

- googletest
- wabt
- binaryen
- capstone: provide user-friendly disassembly output

  ```shell
  git clone https://github.com/capstone-engine/capstone.git
  cd capstone
  git checkout 6.0.0-Alpha4
  mkdir build
  cd build
  cmake  -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON ..
  cmake --build . --parallel
  cmake --install . --prefix=$(pwd)/install
  echo "export CMAKE_PREFIX_PATH=$(pwd)/install:$CMAKE_PREFIX_PATH" >> ~/.bashrc
  ```

### Using CMAKE

#### in Linux or Unix

```shell
mkdir build
cd build
cmake ..
cmake --build . --parallel
```

##### Install

Install with cmake

```shell
cmake --install . --prefix /path_to_install
export wasm_vb_DIR=/path_to_install
```

Use the installed package by cmake

```cmake
find_package(wasm_vb REQUIRED)
target_include_directories(libName PUBLIC ${VB_WASM_INCLUDE_DIRS})
target_link_libraries(libName ${VB_WASM_LIBRARIES})
```

#### cross compile for linux arm64

```shell
mkdir build_linux_arm
cd build
cmake -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 ..
cmake --build . --parallel
```

#### in Windows

```shell
cd wasm_compiler_dir
mkdir build_win
cd build_win
cmake -DCMAKE_BUILD_TYPE=DEBUG -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Debug
```

#### in windows for Arm64

```shell
cd wasm_compiler_dir
mkdir build_win_arm
cd build_win_arm
cmake -DCMAKE_BUILD_TYPE=DEBUG -G "Visual Studio 17 2022" -A ARM64 ..
cmake --build . --config Debug
```

#### On windows with mingw

Download Mingw from
https://github.com/niXman/mingw-builds-binaries/releases/download/12.1.0-rt_v10-rev3/x86_64-12.1.0-release-posix-seh-rt_v10-rev3.7z

```shell
cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE:STRING=Debug -DCMAKE_CXX_COMPILER=g++ -DCMAKE_C_COMPILER=gcc -DENABLE_SPECTEST=1 ..
mingw32-make -j 8
```

#### cross-compile for QNX

##### setup environment variables for qcc

QNX is an enterprise software, please refer to your company's guild line to setup qcc compiler

##### x86_64 qcc

```shell
mkdir build_qnx
cd build_qnx
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/QNXx86_64Toolchain.cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=qcc -DCMAKE_C_COMPILER=qcc -DENABLE_SPECTEST=1 -DDISABLE_SPECTEST_WAST=1 -DENABLE_FUZZ=1  ..
cmake --build . --parallel
```

##### aarch64 qcc

```shell
mkdir build_qnx_arm
cd build_qnx_arm
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/QNXArm64Toolchain.cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=qcc -DCMAKE_C_COMPILER=qcc -DENABLE_SPECTEST=1 -DDISABLE_SPECTEST_WAST=1 -DENABLE_FUZZ=1  ..
cmake --build . --parallel
```

#### cross-compile for TRICORE on x86_64 linux host With Tricore GCC

```shell
python3 tests/spectest.py
mkdir build_tricore
cd build_tricore
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=$TRICORE_GCC_PATH/tricore-elf-gcc -DCMAKE_CXX_COMPILER=$TRICORE_GCC_PATH/tricore-elf-g++ -DENABLE_CLANG_TIDY=1 -DENABLE_SPECTEST=1 -DDISABLE_SPECTEST_WAST=1 -DDISABLE_SPECTEST_JSON=1 -DENABLE_STANDALONE_TEST=1 ..
cmake --build . --parallel
```

#### cross-compile for TRICORE on x86_64 windows host With Tasking

Note user must have python 3 installed

```shell
mkdir build_tasking
cd build_tasking
cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE:FILEPATH=../cmake/TaskingTricore.cmake -DENABLE_SPECTEST=1 -DDISABLE_SPECTEST_WAST=1 -DDISABLE_SPECTEST_JSON=1 -DENABLE_STANDALONE_TEST=1 ..
cmake --build . --parallel
```

### Using BAZEL

#### x86_64 linux

```shell
bazel build //tests:vb_spectest_json --platforms=//bazel/platforms:x86_64_linux
```

#### Arm64 linux

```shell
bazel build //tests:vb_spectest_json --platforms=//bazel/platforms:aarch64_linux
```



##### x86_64 qnx

```shell
bazel build //tests:vb_spectest_json --platforms=//bazel/platforms:x86_64_qnx
```

##### Arm64 qnx

```shell
bazel build //tests:vb_spectest_json --platforms=//bazel/platforms:aarch64_qnx
```

#### Build for tricore

##### With tasking (only windows)

Add following lines to %USERPROFILE%\.bazelrc

```
build --action_env=TSK_LICENSE_KEY_SW260800=ff44-e6e6-b2b2-44b3
build --action_env=TSK_LICENSE_SERVER=wlic01s1.muc:1890
```

```shell
bazel build //tests:vb_spectest_binary_standalone_<0/1> --config=Tasking_warning_as_error --platforms=//bazel/platforms:tricore_tasking
```

##### With Tricore GCC

add following lines into ~/.bazelrc

```shell
build --action_env=TRICORE_GCC_PATH=TRICORE_GCC_INSTALL_PATH/tricore_940_linux/bin
```

```shell
bazel build //tests:vb_spectest_binary_standalone_<0/1> --platforms=//bazel/platforms:tricore_gcc
```

### Using Docker

Build image

```bash
docker build -t wasm-compiler ./docker
```

Using proxy build image

```bash
docker build -t wasm-compiler ./docker \
--build-arg http_proxy=http://user:pass@host \
--build-arg https_proxy=https://user:pass@host
```

Using aliyun apt source build image

```bash
docker build -t wasm-compiler ./docker --build-arg aliyun_apt_source=1
```

Build cmake project

```bash
docker run -it -v $(pwd):/home/wasm-compiler wasm-compiler /bin/bash
mkdir build && cd build
cmake ..
make
```

Run unittest & coverage

```bash
docker run -it -v $(pwd):/home/wasm-compiler wasm-compiler /bin/bash
mkdir build && cd build
cmake .. -DENABLE_SPECTEST=1 -DENABLE_UNITTEST=1 -DENABLE_COVERAGE=1
make
ctest --output-on-failure
make coverage
```

## Build Doc

```shell
doxygen Doxyfile
```

## Build Python binding

There are two different binding, the most common way is create a python binding for native target only.

The other way is binding for all target. It is helpful for tooling dev and researching.

### Binding Native Target Only

```bash
python3 -m venv venv # create venv
export CMAKE_ARGS=... # additional cmake args
pip3 install ./binding
```

Then python binding is available.

```python3
import vb_warp
print(vb_warp.get_configuration())
compiler = vb_warp.Compiler()
compiler.register_api(module_name, func_name, signature)
module = compiler.compile(open(module, "rb").read())
runtime = vb_warp.Runtime()
runtime.load(module)
runtime.start()
```

more detail usage see [warp_dump](scripts/warp_dump.py) and [wasm_executor](scripts/wasm_executor.py).

### Binding All Target

```bash
python3 -m venv venv # create venv
python3 binding/binding_all.py
```

It will provide 6 different bindings which can be imported independently.

- aarch64_vb_warp
- aarch64_active_vb_warp
- x86_64_vb_warp
- x86_64_active_vb_warp
- tricore_vb_warp

```python3
vb_targets = [
    "aarch64_vb_warp",
    "aarch64_active_vb_warp",
    "x86_64_vb_warp",
    "x86_64_active_vb_warp",
    "tricore_vb_warp",
]
import importlib
for name in vb_targets:
    vb_warp = importlib.import_module(name=name)
    pass
```

The internal APIs are the same as native binding.
