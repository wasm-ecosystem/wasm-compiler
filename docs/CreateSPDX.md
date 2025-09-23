# Generate SPDX files for used open source submodules

Install dependence
```shell
pip install spdx-tools==0.7.1 GitPython
```

Generate:
```shell
python scripts/spdx/wasm_compiler_spdx.py -o ./build
```
Note: open source components for testing is not included