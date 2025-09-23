This folder shows how to integrate and use WARP compiler

## Build demo

build with cmake arg `-DENABLE_DEMO=1`

## Run demo on Unix and Windows:

```shell
./build/bin/vb_demo wasm_examples/log.wasm
```

## Run demo for embedded tricore

```shell
$TRICORE_QEMU_PATH/qemu-system-tricore -semihosting -display none -M tricore_tsim162 -kernel build_tricore_gcc/bin/vb_demo
```
