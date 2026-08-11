import argparse
import importlib
import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
TESTSUITE_DIR = ROOT / "tests" / "testsuite"
SNAPSHOT_DIR = ROOT / "tests" / "snapshot_diff"
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
            "--enable-tail-call",
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


def generate_wasm_files(output_dir: Path) -> None:
    for wast_path in sorted(TESTSUITE_DIR.glob("*.wast")):
        run_wast2json(wast_path, output_dir / f"{wast_path.stem}.json")


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


def get_wasm_files(wasm_dir: Path) -> dict[str, Path]:
    return {path.name: path for path in sorted(wasm_dir.glob("*.wasm"))}


def compare_wasm_files(generated_dir: Path) -> None:
    generated = get_wasm_files(generated_dir)
    checked_in = get_wasm_files(SNAPSHOT_DIR)
    if set(generated) != set(checked_in):
        missing = sorted(set(generated) - set(checked_in))
        extra = sorted(set(checked_in) - set(generated))
        raise SnapshotError(f"wasm file set differs: missing={missing}, extra={extra}")
    for name, generated_path in generated.items():
        if generated_path.read_bytes() != checked_in[name].read_bytes():
            raise SnapshotError(f"wasm file differs from snapshot: {name}")


def update_wasm_files(generated_dir: Path) -> None:
    SNAPSHOT_DIR.mkdir(parents=True, exist_ok=True)
    for path in SNAPSHOT_DIR.glob("*.wasm"):
        path.unlink()
    for name, generated_path in get_wasm_files(generated_dir).items():
        shutil.copyfile(generated_path, SNAPSHOT_DIR / name)


def load_backend(backend: str):
    add_virtualenv_site_packages()
    module = importlib.import_module(f"{backend}_vb_warp")
    module.enable_color(False)
    return module


def create_compiler(module):
    compiler = module.Compiler()
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


def run_backend(backend: str, wasm_dir: Path, update: bool) -> None:
    module_names = get_module_wasm_names(wasm_dir)
    if update:
        update_backend_snapshots(backend, wasm_dir, module_names)
    else:
        compare_backend_snapshots(backend, wasm_dir, module_names)


def run(update: bool, update_wasm: bool) -> None:
    with tempfile.TemporaryDirectory(prefix="wasm-snapshot-") as temporary_dir:
        wasm_dir = Path(temporary_dir)
        generate_wasm_files(wasm_dir)
        if update or update_wasm:
            update_wasm_files(wasm_dir)
        else:
            compare_wasm_files(wasm_dir)

        if update_wasm:
            return

        for backend in BACKENDS:
            command = [
                sys.executable,
                str(Path(__file__).resolve()),
                "--backend",
                backend,
                "--wasm-dir",
                str(wasm_dir),
            ]
            if update:
                command.append("-u")
            subprocess.run(command, check=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    update_group = parser.add_mutually_exclusive_group()
    update_group.add_argument("-u", "--update", action="store_true")
    update_group.add_argument(
        "--update-wasm",
        action="store_true",
        help="update the checked-in wasm file set without updating backend snapshots",
    )
    parser.add_argument("--backend", choices=BACKENDS)
    parser.add_argument("--wasm-dir", type=Path)
    args = parser.parse_args()

    try:
        if args.backend is not None:
            if args.update_wasm:
                parser.error("--update-wasm cannot be used with --backend")
            if args.wasm_dir is None:
                parser.error("--wasm-dir is required with --backend")
            run_backend(args.backend, args.wasm_dir, args.update)
        else:
            if args.wasm_dir is not None:
                parser.error("--wasm-dir requires --backend")
            run(args.update, args.update_wasm)
    except (OSError, SnapshotError, subprocess.CalledProcessError) as error:
        print(f"snapshot check failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
