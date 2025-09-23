### Run coverity

#### for x86_64 backend

```shell
mkdir build_coverity
cd build_coverity
cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release -DBACKEND=x86_64 ..
cov-build --dir=./coverity_out cmake --build . --parallel
cov-analyze --coding-standard-config=../autosarcpp14-wasm.config --strip-path=$(pwd)/../src --dir=./coverity_out --disable-default
cov-format-errors --dir ./coverity_out --emacs-style --exclude-files='/usr/*'
```

#### For aarch64 backend

```shell
mkdir build_coverity_arm
cd build_coverity_arm
cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release -DBACKEND=aarch64 ..
cov-build --dir=./coverity_out cmake --build . --parallel
cov-analyze --coding-standard-config=../autosarcpp14-wasm.config --strip-path=$(pwd)/../src --dir=./coverity_out --disable-default
cov-format-errors --dir ./coverity_out --emacs-style --exclude-files='/usr/*'
```

#### For tricore backend

```shell
mkdir build_coverity_tricore
cd build_coverity_tricore
cmake -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release -DBACKEND=tricore ..
cov-build --dir=./coverity_out cmake --build . --parallel
cov-analyze --coding-standard-config=../autosarcpp14-wasm.config --strip-path=$(pwd)/../src --dir=./coverity_out --disable-default

## Do not need to check thirdparty
cov-format-errors --dir ./coverity_out --emacs-style --exclude-files='(wasm-compiler/thirdparty/.*|/usr/.*)'
```
