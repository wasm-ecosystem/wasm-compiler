# Run bench

### 1. Build v8

#### Install depot_tools

```shell
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
echo "export PATH=$(pwd)/depot_tools:\$PATH" >> ~/.profile
source ~/.profile
```

#### build v8

x86_64 local build

```shell
mkdir v8
cd v8
fetch v8
gclient sync
cd v8
./build/install-build-deps.sh #linux only
tools/dev/v8gen.py x64.release -- 'v8_use_external_startup_data=false' && autoninja -C out/x64.release d8
echo "export PATH=$(pwd)/out/x64.release:\$PATH" >> ~/.profile
source ~/.profile
```

cross compile for arm64 on x86_64 host

```shell
python3 build/linux/sysroot_scripts/install-sysroot.py --arch=arm64

tools/dev/v8gen.py arm64.release -- 'target_cpu="arm64"' 'v8_use_external_startup_data=false' 'icu_use_data_file=false' && autoninja -C out.gn/arm64.release d8
```

### 2. Build SpiderMonkey

create a build config file `$HOME/mozconfigs/optimized` according to this doc
https://firefox-source-docs.mozilla.org/js/build.html#optimized-builds

```shell
git clone https://github.com/mozilla-firefox/firefox.git
cd firefox
export MOZCONFIG=$HOME/mozconfigs/optimized
./mach build
# Note: need to adapt the path if not no x86_64 linux
echo "export PATH=$(pwd)/obj-opt-x86_64-pc-linux-gnu/dist/bin:\$PATH" >> ~/.profile
source ~/.profile
```

### 3. Build vb_bench

```shell
cmake -B build_bench -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DVB_ENABLE_DEV_FEATURE=OFF -DENABLE_BENCH=1 -DCMAKE_CXX_FLAGS="-DINTERRUPTION_REQUEST=0 -DEAGER_ALLOCATION=1"
cmake --build build_bench --parallel
```

### 4. Build Benchmark sources

Build benchmark according to the Readme:
https://github.com/Schleifner/open-wasm-compiler-benchmark.git

### 5. Run bench

```
python scripts/benchmark/run_bench.py -i ../open-wasm-compiler-benchmark -x ./build_bench/bin/vb_bench
```
