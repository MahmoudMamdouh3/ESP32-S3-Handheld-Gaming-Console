"""Verify that host C++17 and Python test suites catch 10 isolated single-line mutations."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
FIRMWARE_SRC = REPO_ROOT / "firmware" / "BmoGameboy" / "src"


@dataclass(frozen=True)
class CppMutation:
    name: str
    rel_header: str
    old_snippet: str
    new_snippet: str
    test_cpp: str


@dataclass(frozen=True)
class PyMutation:
    name: str
    rel_module: str
    old_snippet: str
    new_snippet: str
    unittest_target: str


CPP_MUTATIONS: tuple[CppMutation, ...] = (
    CppMutation(
        name="bmp_header_offset_guard",
        rel_header="storage/bmp_header.h",
        old_snippet="if (dataOffset < 54U || fileSize < dataOffset) return false;",
        new_snippet="if (fileSize < dataOffset) return false;",
        test_cpp="tests/host/fuzz_storage_test.cpp",
    ),
    CppMutation(
        name="sdf_coverage_polarity",
        rel_header="ui/bmo_face_sdf.h",
        old_snippet="return smoothstepf(aaBand, 0.0f, dist);",
        new_snippet="return smoothstepf(0.0f, aaBand, dist);",
        test_cpp="tests/host/bmo_face_sdf_test.cpp",
    ),
    CppMutation(
        name="rom_file_filter_min_size",
        rel_header="storage/rom_file_filter.h",
        old_snippet="if (fileSize < 64) return false;",
        new_snippet="if (fileSize < 16) return false;",
        test_cpp="tests/host/rom_file_filter_test.cpp",
    ),
    CppMutation(
        name="input_frame_down_bit",
        rel_header="hal/input_frame.h",
        old_snippet="return (idx >= 0 && idx < BUTTON_COUNT) && ((pressedMask & (1U << idx)) != 0);",
        new_snippet="return (idx >= 0 && idx < BUTTON_COUNT) && ((pressedMask & (1U << idx)) == 0);",
        test_cpp="tests/host/input_frame_test.cpp",
    ),
    CppMutation(
        name="harmonic_spring_nonfinite_dt",
        rel_header="ui/harmonic_spring.h",
        old_snippet="if (!std::isfinite(dt) || dt <= 0.0f) return false;",
        new_snippet="if (!std::isfinite(dt) || dt <= 0.0f) return true;",
        test_cpp="tests/host/harmonic_spring_test.cpp",
    ),
    CppMutation(
        name="box_art_strip_extension",
        rel_header="storage/box_art_lookup.h",
        old_snippet="if (dot) {\n    *dot = '\\0';\n  }",
        new_snippet="if (dot) {\n    *dot = '.';\n  }",
        test_cpp="tests/host/box_art_lookup_test.cpp",
    ),
    CppMutation(
        name="console_catalog_fav_index",
        rel_header="ui/console_catalog.h",
        old_snippet="ROM_FAVORITES, -1,",
        new_snippet="ROM_FAVORITES, 0,",
        test_cpp="tests/host/console_catalog_test.cpp",
    ),
)

PY_MUTATIONS: tuple[PyMutation, ...] = (
    PyMutation(
        name="rom_key_strip_leading_slash",
        rel_module="scripts/rom_key.py",
        old_snippet='normalized = relative_path.replace("\\\\", "/").lstrip("/")',
        new_snippet='normalized = relative_path.replace("\\\\", "/")',
        unittest_target="tests/test_rom_key.py",
    ),
    PyMutation(
        name="color_token_mint_expected",
        rel_module="scripts/verify_color_tokens.py",
        old_snippet='"BMO_SCREEN_MINT": ("--screen-mint", "#CEF5E4"),',
        new_snippet='"BMO_SCREEN_MINT": ("--screen-mint", "#CEF5E5"),',
        unittest_target="scripts.verify_color_tokens",
    ),
    PyMutation(
        name="deploy_missing_nes_extension",
        rel_module="scripts/deploy_to_sd.py",
        old_snippet='".gb", ".gbc", ".nes", ".wad",',
        new_snippet='".gb", ".gbc", ".wad",',
        unittest_target="tests/test_sd_deploy.py",
    ),
)


def _find_wasi_clangxx() -> Path | None:
    env_sdk = os.environ.get("WASI_SDK_PATH")
    candidates: list[Path] = []
    if env_sdk:
        candidates.extend((Path(env_sdk) / "bin" / "clang++", Path(env_sdk) / "bin" / "clang++.exe"))
    for plat in ("windows", "linux", "macos"):
        sdk_bin = REPO_ROOT / ".tools" / f"wasi-sdk-34.0-x86_64-{plat}" / "bin"
        candidates.extend((sdk_bin / "clang++", sdk_bin / "clang++.exe"))
    for item in candidates:
        if item.is_file():
            return item
    return None


def _run_cpp_mutation(mut: CppMutation, clangxx: Path, node: str, workdir: Path) -> tuple[bool, str]:
    orig = (FIRMWARE_SRC / mut.rel_header).read_text(encoding="utf-8")
    normalized = orig.replace("\r\n", "\n")
    if mut.old_snippet not in normalized:
        return False, f"snippet not found in {mut.rel_header}"
    mutated = normalized.replace(mut.old_snippet, mut.new_snippet, 1)

    shadow_fw = workdir / "shadow_fw"
    if shadow_fw.exists():
        shutil.rmtree(shadow_fw)
    shadow_src = shadow_fw / "src"
    shutil.copytree(FIRMWARE_SRC, shadow_src, ignore=shutil.ignore_patterns("third_party"))
    (shadow_src / mut.rel_header).write_text(mutated, encoding="utf-8")

    wasm_out = workdir / f"{mut.name}.wasm"
    compile_cmd = [
        str(clangxx),
        "--target=wasm32-wasip1",
        "-std=c++17",
        "-O1",
        "-UNDEBUG",
        "-DBMO_HOST_TEST=1",
        "-I",
        str(shadow_fw),
        "-I",
        str(shadow_src),
        "-I",
        str(shadow_src / "app"),
        "-I",
        str(shadow_src / "hal"),
        "-I",
        str(shadow_src / "storage"),
        "-I",
        str(shadow_src / "ui"),
        "-I",
        str(shadow_src / "cores"),
        "-I",
        str(REPO_ROOT / "tests" / "host" / "fakes"),
        "-I",
        str(REPO_ROOT / "tests" / "host"),
        str(REPO_ROOT / mut.test_cpp),
        "-o",
        str(wasm_out),
    ]
    comp = subprocess.run(compile_cmd, capture_output=True, text=True, check=False)
    if comp.returncode != 0:
        return False, f"mutant failed to compile (not runtime-caught): {comp.stderr[:240]}"

    js_runner = (
        "const fs=require('fs'), {WASI}=require('wasi');"
        "(async()=>{const w=new WASI({version:'preview1',returnOnExit:true});"
        "const m=await WebAssembly.compile(fs.readFileSync(process.argv[1]));"
        "const i=await WebAssembly.instantiate(m,w.getImportObject());"
        "const rc=w.start(i);"
        "process.exit(typeof rc==='number'?rc:0);})().catch(e=>{console.error(e);process.exit(1);});"
    )
    run = subprocess.run([node, "-e", js_runner, str(wasm_out)], capture_output=True, text=True, check=False)
    if run.returncode == 0:
        return False, f"mutant SURVIVED {mut.test_cpp}!"
    return True, f"killed at runtime ({mut.test_cpp})"


def _run_py_mutation(mut: PyMutation, workdir: Path) -> tuple[bool, str]:
    clone = workdir / "py_clone"
    if clone.exists():
        shutil.rmtree(clone)
    for folder in ("scripts", "tests", "firmware", "simulator", "tools"):
        shutil.copytree(REPO_ROOT / folder, clone / folder, ignore=shutil.ignore_patterns("__pycache__", "third_party"))
    (clone / "DESIGN.md").write_text((REPO_ROOT / "DESIGN.md").read_text(encoding="utf-8"), encoding="utf-8")

    mod_path = clone / mut.rel_module
    orig = mod_path.read_text(encoding="utf-8").replace("\r\n", "\n")
    if mut.old_snippet not in orig:
        return False, f"snippet not found in {mut.rel_module}"
    mod_path.write_text(orig.replace(mut.old_snippet, mut.new_snippet, 1), encoding="utf-8")

    env = dict(os.environ)
    env["PYTHONPATH"] = f"{clone}{os.pathsep}{clone / 'tests'}"
    py_cmd = (
        [sys.executable, "-m", mut.unittest_target]
        if mut.unittest_target.endswith(".verify_color_tokens")
        else [sys.executable, "-m", "unittest", mut.unittest_target]
    )
    run = subprocess.run(
        py_cmd,
        cwd=clone,
        env=env,
        capture_output=True,
        text=True,
        check=False,
    )
    if run.returncode == 0:
        return False, f"mutant SURVIVED {mut.unittest_target}!"
    return True, f"killed by {mut.unittest_target}"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Run 10-mutation test suite sanity verification.")
    parser.add_argument("--python-only", action="store_true", help="Run only Python mutations cleanly.")
    args = parser.parse_args(argv)

    clangxx = None if args.python_only else _find_wasi_clangxx()
    node = None if args.python_only else shutil.which("node")
    results: list[tuple[str, bool, str]] = []

    with tempfile.TemporaryDirectory() as tmp:
        workdir = Path(tmp)
        if clangxx and node:
            for mut in CPP_MUTATIONS:
                ok, msg = _run_cpp_mutation(mut, clangxx, node, workdir)
                results.append((mut.name, ok, msg))
        elif not args.python_only:
            print("WARNING: WASI clang++/node unavailable; skipping C++ mutations.")

        for mut in PY_MUTATIONS:
            ok, msg = _run_py_mutation(mut, workdir)
            results.append((mut.name, ok, msg))

    killed = sum(1 for _, ok, _ in results if ok)
    for name, ok, msg in results:
        print(f"  [{'KILLED' if ok else 'FAILED'}] {name}: {msg}")
    print(f"Mutation summary: {killed}/{len(results)} killed.")
    return 0 if killed == len(results) and killed > 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
