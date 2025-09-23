This folder is a helper for fast minimal reproduce fuzz bug of tricore.

Usage

1. paste the wat content into reproduce.wat
2. Modify the exported function name and signature with need to run in main.cpp
3. run python ./reproduce.py

The reproduce.py will do following steps

1. convert the wat to wasm
2. convert the wasm to c++ array and link it into elf
3. run the elf with tricore qemu
4. run the wasm with wasm-interp

Note:
don't commit the reproduce.wat and reproduce.cpp in git
