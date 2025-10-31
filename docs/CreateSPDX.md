# Generate SPDX files for used open source submodules

## Prerequisites

### Install Python dependencies

Install all required Python packages from requirements.txt:
```shell
pip install -r requirements.txt
```

Or if using a virtual environment:
```shell
python -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

### Download SPDX Java Tools (for v3 conversion)

Download and extract the SPDX Java tools:
```shell
sudo apt-get update
sudo apt-get install -y wget unzip
wget https://github.com/spdx/tools-java/releases/download/v2.0.2/tools-java-2.0.2.zip
unzip tools-java-2.0.2.zip -d ~/spdx-tools
```

## Generate SPDX v2.3 (tag-value format)

```shell
python scripts/spdx/generate_spdx.py -o ./build
```

The output file will be `./build/wasm-compiler.spdx`.

## Convert to SPDX v3 (JSON-LD format)

Optional: Set proxy for SPDX Java tools if needed:
```shell
export JAVA_TOOL_OPTIONS='-Dhttp.proxyHost=host -Dhttp.proxyPort=port -Dhttps.proxyHost=host -Dhttps.proxyPort=port'
```

### Convert SPDX v2.3 to v3 JSON-LD:
```shell 
java -jar ~/spdx-tools/tools-java-2.0.2-jar-with-dependencies.jar Convert ./build/wasm-compiler.spdx ./build/wasm-compiler.spdx3.jsonld.json TAG JSONLD
```

### Validate the converted SPDX v3 file:
```shell
java -jar ~/spdx-tools/tools-java-2.0.2-jar-with-dependencies.jar Verify ./build/wasm-compiler.spdx3.jsonld.json
```

Note: open source components for testing is not included