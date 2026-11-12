import argparse
import importlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TESTSUITE_DIR = ROOT / "tests" / "testsuite"
SNAPSHOT_DIR = ROOT / "tests" / "snapshot_diff"
WASM_DIR = SNAPSHOT_DIR / "_wasm"
BACKENDS = ("x86_64", "aarch64", "tricore")
SNAPSHOT_SUFFIX = ".snapshot"
POSITION_RE = re.compile(r"^[0-9A-Fa-f]+[ \t]{2,}")
KNOWN_SKIPS = {
    ("tricore", "vb_builtins_trace_point.0.wasm"): "Not implemented",
}


class SnapshotError(RuntimeError):
    pass


def add_virtualenv_site_packages() -> None:
    site_packages = ROOT / "venv" / "lib"
    for candidate in sorted(site_packages.glob("python*/site-packages"), reverse=True):
        sys.path.insert(0, str(candidate))


def run_wast2json(wast_path: Path, output_path: Path) -> None:
    result = subprocess.run(
        [
            "wast2json",
            "-o",
            str(output_path),
            str(wast_path),
        ],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        details = result.stderr.strip() or result.stdout.strip()
        raise SnapshotError(f"wast2json failed for {wast_path}: {details}")


def generate_wasm_files(output_dir: Path) -> list[str]:
    for wast_path in sorted(TESTSUITE_DIR.glob("*.wast")):
        run_wast2json(wast_path, output_dir / f"{wast_path.stem}.json")
    return get_module_wasm_names(output_dir)


def get_module_wasm_names(wasm_dir: Path) -> list[str]:
    names = set()
    for json_path in sorted(wasm_dir.glob("*.json")):
        with json_path.open(encoding="utf-8") as input_file:
            testcase = json.load(input_file)
        for command in testcase["commands"]:
            if command.get("type") != "module":
                continue
            filename = command.get("filename")
            if filename and filename.endswith(".wasm"):
                names.add(filename)
    return sorted(names)


def load_backend(backend: str):
    add_virtualenv_site_packages()
    module = importlib.import_module(f"{backend}_vb_warp")
    module.enable_color(False)
    return module


def create_compiler(module):
    compiler = module.Compiler()
    compiler.enable_dwarf(True)
    compiler.register_global("spectest", "global_i32", module.WasmType.I32, "666")
    compiler.register_global("spectest", "global_i64", module.WasmType.I64, "666")
    compiler.register_global("spectest", "global_f32", module.WasmType.F32, "666.6")
    compiler.register_global("spectest", "global_f64", module.WasmType.F64, "666.6")
    return compiler


def get_snapshot_path(backend: str, wasm_name: str) -> Path:
    return SNAPSHOT_DIR / backend / f"{Path(wasm_name).stem}{SNAPSHOT_SUFFIX}"


def normalize_snapshot(output: str) -> bytes:
    lines = [POSITION_RE.sub("", line, count=1) for line in output.splitlines()]
    return ("\n".join(lines) + "\n").encode("utf-8")


def compile_snapshot(
    module, backend: str, wasm_name: str, wasm_path: Path
) -> bytes | None:
    expected_error = KNOWN_SKIPS.get((backend, wasm_name))
    try:
        output = create_compiler(module).disassemble_wasm(wasm_path.read_bytes())
    except RuntimeError as error:
        if expected_error is None:
            raise SnapshotError(f"{backend} failed for {wasm_name}: {error}") from error
        if str(error) != expected_error:
            raise SnapshotError(
                f"{backend} changed the expected error for {wasm_name}: {error}"
            ) from error
        return None
    if expected_error is not None:
        raise SnapshotError(f"{backend} unexpectedly compiled {wasm_name}")
    return normalize_snapshot(output)


def compare_backend_snapshots(
    backend: str, wasm_dir: Path, module_names: list[str]
) -> None:
    module = load_backend(backend)
    expected = set()
    for wasm_name in module_names:
        snapshot = compile_snapshot(module, backend, wasm_name, wasm_dir / wasm_name)
        snapshot_path = get_snapshot_path(backend, wasm_name)
        if snapshot is None:
            continue
        expected.add(snapshot_path.name)
        if not snapshot_path.exists():
            raise SnapshotError(
                f"missing snapshot: {snapshot_path.relative_to(SNAPSHOT_DIR)}"
            )
        if snapshot_path.read_bytes() != snapshot:
            raise SnapshotError(
                f"snapshot differs: {snapshot_path.relative_to(SNAPSHOT_DIR)}"
            )

    actual = {
        path.name for path in (SNAPSHOT_DIR / backend).glob(f"*{SNAPSHOT_SUFFIX}")
    }
    if actual != expected:
        missing = sorted(expected - actual)
        extra = sorted(actual - expected)
        raise SnapshotError(
            f"{backend} snapshot file set differs: missing={missing}, extra={extra}"
        )


def update_backend_snapshots(
    backend: str, wasm_dir: Path, module_names: list[str]
) -> None:
    module = load_backend(backend)
    backend_dir = SNAPSHOT_DIR / backend
    backend_dir.mkdir(parents=True, exist_ok=True)
    for path in backend_dir.glob(f"*{SNAPSHOT_SUFFIX}"):
        path.unlink()
    for wasm_name in module_names:
        snapshot = compile_snapshot(module, backend, wasm_name, wasm_dir / wasm_name)
        if snapshot is not None:
            get_snapshot_path(backend, wasm_name).write_bytes(snapshot)


def run_backend(
    backend: str, wasm_dir: Path, module_names: list[str], update: bool
) -> None:
    if update:
        update_backend_snapshots(backend, wasm_dir, module_names)
    else:
        compare_backend_snapshots(backend, wasm_dir, module_names)


def run(update: bool) -> None:
    shutil.rmtree(WASM_DIR, ignore_errors=True)
    WASM_DIR.mkdir(parents=True, exist_ok=True)
    module_names = generate_wasm_files(WASM_DIR)

    for backend in BACKENDS:
        run_backend(backend, WASM_DIR, module_names, update)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("-u", "--update", action="store_true")
    args = parser.parse_args()

    try:
        run(args.update)
    except (OSError, SnapshotError, subprocess.CalledProcessError) as error:
        print(f"snapshot check failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
