## Install afl++

Note: AFL is no long maintained, always use afl++ for fuzzing

```shell
git clone https://github.com/AFLplusplus/AFLplusplus.git
cd AFLplusplus
make -j $(nproc)
sudo make install
```

## Checkout the fuzz corpus

```
git clone https://github.com/Schleifner/wasm-fuzz-corpus.git
```

## Run the fuzz

Note

1. the persistent mode is much faster than basic mode, so it's recommended to use persistent mode.
   See [https://github.com/AFLplusplus/AFLplusplus/blob/stable/instrumentation/README.persistent_mode.md]
2. Recommend to use afl-clang-lto with LLVM-11+, it's faster than afl-clang-fast
   https://github.com/AFLplusplus/AFLplusplus/blob/stable/docs/fuzzing_in_depth.md

### Build the fuzz

```shell
mkdir build_fuzz
cd build_fuzz
cmake -DCMAKE_C_COMPILER=afl-clang-lto -DCMAKE_CXX_COMPILER=afl-clang-lto++ -DENABLE_FUZZ=1 -DCMAKE_CXX_FLAGS="-DLINEAR_MEMORY_BOUNDS_CHECKS=1 -DACTIVE_STACK_OVERFLOW_CHECK=1 -DNO_PASSIVE_PROTECTION_WARNING=1" ..
make -j $(nproc)
```

### Run fuzz

Prepare(optional)
Create a ram disk to run the fuzz faster and protect the ssd

```shell
mkdir ~/tmpfs
sudo mount -o size=2G -t tmpfs none ~/tmpfs
cd ~/tmpfs
git clone https://github.com/Schleifner/wasm-fuzz-corpus.git
mkdir output
```

Run

```shell
afl-fuzz -i ~/tmpfs/wasm-fuzz-corpus -o ~/tmpfs/output -- ./bin/vb_afl_harness_persistent
```

### Run fuzz with multi process

https://aflplus.plus/docs/parallel_fuzzing/

In project root, run

```shell

./fuzz/afl_harness/afl_launch.sh -i ~/tmpfs/wasm-fuzz-corpus -o ~/tmpfs/output -x build_fuzz/bin/vb_afl_harness_persistent -b ~/tmpfs/back
```

## Debug the crash found by AFL

When AFL found some crash, you will see ` total crashes : 2 (2 saved)`, then go to `~/tmpfs/output/output/default/crashes`
There will be crash file like this `id:000000,sig:11,src:000652,time:13896,execs:367068,op:havoc,rep:1`

Build the fuzz with local compiler again and reproduce the crash with

```shell
./bin/vb_afl_harness_persistent ~/tmpfs/output/output/default/crashes/crash_file_name
```
