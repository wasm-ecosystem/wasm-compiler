# Propose

On some embedded system, binaryen is not portable to target machine. To solve this problem, binaryen is run on host machine. Then the output of wasm-opt and wasm-interp can be copied to target machine by debugger. After fuzzing execution, the result can be fetched again to host machine by debugger.

## Build fuzz

The debugger need to read and write memory according to debug symbol. So that the fuzz must be built in debug mode

### Build with tasking

```bash
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE:FILEPATH=../cmake/TaskingTricore.cmake -DENABLE_SPECTEST=1 -DDISABLE_SPECTEST_WAST=1 -DDISABLE_SPECTEST_JSON=1 -DENABLE_STANDALONE_TEST=1 -DENABLE_FUZZ=1 -DFUZZ_ONLY_WITH_DEBUGGER=1 -B build_tasking .
cmake --build build_tasking --parallel
```

### Build with gcc

```bash
mkdir build_tricore_gcc
cmake -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=$TRICORE_GCC_PATH/tricore-elf-gcc -DCMAKE_CXX_COMPILER=$TRICORE_GCC_PATH/tricore-elf-g++ -DENABLE_FUZZ=1 -DFUZZ_ONLY_WITH_DEBUGGER=1 -B build_tricore_gcc .
cmake --build build_tricore_gcc --parallel
```

## Run with GDB

### pre-condition:

1. setup environment `TRICORE_QEMU_PATH` or ensure `qemu-system-tricore` in path.
2. ensure `tricore-elf-gdb` in path.

```shell
export VB_FUZZ_LOGGING_LEVEL=Debug # can be ignored
export VB_QEMU_GDB_PORT=1121
export VB_FUZZ_TARGET_DIR=$(pwd)/build_tricore_fuzz_tmp/$VB_QEMU_GDB_PORT
rm -rf $VB_FUZZ_TARGET_DIR ; mkdir -p $VB_FUZZ_TARGET_DIR | true

nohup $TRICORE_QEMU_PATH/qemu-system-tricore -semihosting -display none -M tricore_tsim162 -kernel build_tricore_gcc/bin/vb_debugger_fuzz -gdb tcp::$VB_QEMU_GDB_PORT -S >$VB_FUZZ_TARGET_DIR/qemu_$VB_QEMU_GDB_PORT.log 2>&1 &
nohup tricore-elf-gdb ./build_tricore_gcc/bin/vb_debugger_fuzz --quiet -x ./fuzz/fuzz_with_debugger/fuzz_init.gdb -ex "start_fuzz $VB_QEMU_GDB_PORT" > $VB_FUZZ_TARGET_DIR/fuzz_$VB_QEMU_GDB_PORT.log 2>&1 &

exit # if you are use ssh, remember to exit shell firstly instead of close ssh connection directly.
```

## Run with lauterbach

### Install dependencies

```batch
cd /d %TRACE32_HOME%\demo\api\python\rcl\dist
pip install lauterbach_trace32_rcl-latest-py3-none-any.whl
```

Document is in `%TRACE32_HOME%\demo\api\python\rcl\doc\html`

### Run Fuzz

1. Enable TCP connection of lauterbach trace32 in launch config
   Add following lines to under `%TRACE32_HOME%\config.t32`

```
RCL=NETTCP
PORT=20000
```

2. Run `%TRACE32_HOME%\bin\windows64\t32mtc.exe`
3. `python fuzz\fuzz_with_debugger\lauterbach_fuzz.py`
