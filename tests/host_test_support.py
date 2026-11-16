"""Shared compiler discovery for C/C++ fixtures."""

from __future__ import annotations

import contextlib
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


def strict_verification_requested() -> bool:
    return os.environ.get("BMO_REQUIRE_TARGET_COMPILE", "").strip().lower() in {
        "1", "true", "yes", "on",
    }


def find_wasi_cxx(repo_root: Path):
    sdk_root = os.environ.get("WASI_SDK_PATH")
    candidates = []
    if sdk_root:
        sdk = Path(sdk_root)
        candidates.extend((sdk / "bin" / "clang++", sdk / "bin" / "clang++.exe"))
    for platform in ("windows", "linux", "macos"):
        sdk = repo_root / ".tools" / f"wasi-sdk-34.0-x86_64-{platform}" / "bin"
        candidates.extend((sdk / "clang++", sdk / "clang++.exe"))
    for compiler in candidates:
        if compiler.is_file():
            return compiler
    return None


def find_native_c_compiler():
    for name in ("cl", "gcc", "clang"):
        compiler = shutil.which(name)
        if compiler:
            return compiler
    return None


def require_compiler(test_case, compiler, description: str) -> None:
    if compiler:
        return
    message = f"{description} is unavailable; host fixture was not verified"
    if strict_verification_requested():
        test_case.fail(message)
    test_case.skipTest(message)


_VCVARS_CANDIDATES = (
    Path(r"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"),
    Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"),
    Path(r"C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"),
    Path(r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"),
)


def find_vcvars64():
    for candidate in _VCVARS_CANDIDATES:
        if candidate.is_file():
            return candidate
    return None


def find_native_cxx():
    """Return ("msvc", vcvars_path) or ("gnu", compiler_path), or None."""
    vcvars = find_vcvars64() if os.name == "nt" else None
    if vcvars:
        return ("msvc", vcvars)
    for name in ("g++", "clang++"):
        compiler = shutil.which(name)
        if compiler:
            return ("gnu", Path(compiler))
    return None


def build_and_run_native_cxx(test_case, sources, *, include_dirs=(), defines=(),
                             args=(), sanitize=True, timeout=300, workdir=None):
    """Compile C++17 sources natively and run the executable.

    On Windows this uses MSVC from vcvars64.bat (``/fsanitize=address`` when
    ``sanitize``). Elsewhere it uses g++/clang++ with
    ``-fsanitize=address,undefined -fno-sanitize-recover=undefined``.
    Skips (or fails under BMO_REQUIRE_TARGET_COMPILE=1) when no compiler exists.
    Returns the CompletedProcess of the test executable. The caller asserts on
    ``returncode`` and output.
    """
    import subprocess
    import tempfile

    toolchain = find_native_cxx()
    require_compiler(test_case, toolchain, "Native C++ compiler (MSVC or g++/clang++)")
    kind, tool = toolchain
    sources = [str(Path(s)) for s in sources]
    tmp_ctx = tempfile.TemporaryDirectory() if workdir is None else None
    out_dir = Path(tmp_ctx.name if tmp_ctx else workdir)
    out_dir.mkdir(parents=True, exist_ok=True)
    exe = out_dir / ("host_test.exe" if os.name == "nt" else "host_test")
    try:
        if kind == "msvc":
            flags = ["/nologo", "/std:c++17", "/EHsc", "/W3", "/Od", "/Zi", "/D_CRT_SECURE_NO_WARNINGS"]
            if sanitize:
                flags.append("/fsanitize=address")
            flags += [f'/I"{d}"' for d in include_dirs]
            flags += [f"/D{d}" for d in defines]
            src_args = " ".join(f'"{s}"' for s in sources)
            run_args = " ".join(f'"{a}"' for a in args)
            bat = out_dir / "build_and_run.bat"
            bat.write_text(
                "@echo off\r\n"
                f'call "{tool}" >nul\r\n'
                f'cl {" ".join(flags)} /Fe:"{exe}" /Fo:"{out_dir}\\\\" /Fd:"{out_dir}\\\\" {src_args} >"{out_dir}\\build.log" 2>&1\r\n'
                f'if errorlevel 1 (type "{out_dir}\\build.log" & exit /b 97)\r\n'
                f'"{exe}" {run_args}\r\n',
                encoding="utf-8",
            )
            proc = subprocess.run([str(bat)], capture_output=True, text=True, timeout=timeout,
                                  cwd=str(out_dir))
            if proc.returncode == 97:
                test_case.fail("Native compile failed:\n" + proc.stdout + proc.stderr)
            return proc
        flags = ["-std=c++17", "-O1", "-g", "-Wall"]
        if sanitize:
            flags += ["-fsanitize=address,undefined", "-fno-sanitize-recover=undefined",
                      "-fno-omit-frame-pointer"]
        flags += [f"-I{d}" for d in include_dirs]
        flags += [f"-D{d}" for d in defines]
        build = subprocess.run([str(tool), *flags, "-o", str(exe), *sources],
                               capture_output=True, text=True, timeout=timeout)
        if build.returncode != 0:
            test_case.fail("Native compile failed:\n" + build.stdout + build.stderr)
        return subprocess.run([str(exe), *[str(a) for a in args]], capture_output=True,
                              text=True, timeout=timeout, cwd=str(out_dir))
    finally:
        if tmp_ctx is not None:
            with contextlib.suppress(OSError):
                tmp_ctx.cleanup()



def find_node_binary(repo_root: Path) -> Path | None:
    """Locate Node.js for running compiled WASI binaries on host."""
    candidates = [
        repo_root / ".tools" / "node" / "node.exe",
        repo_root / ".tools" / "node" / "node",
    ]
    for cand in candidates:
        if cand.is_file():
            return cand
    found = shutil.which("node")
    return Path(found) if found else None


def find_wasi_c_compiler(repo_root: Path) -> Path | None:
    """Locate the sibling WASI C compiler alongside find_wasi_cxx()."""
    wasi_cxx = find_wasi_cxx(repo_root)
    if not wasi_cxx:
        return None
    suffix = wasi_cxx.suffix
    for name in (f"wasm32-wasip1-clang{suffix}", f"clang{suffix}"):
        cand = wasi_cxx.with_name(name)
        if cand.is_file():
            return cand
    return None


def _compile_wasi_object(
    cc_c: Path,
    cc_cxx: Path,
    src: Path,
    obj: Path,
    include_dirs: tuple[Path, ...],
    defines: tuple[str, ...],
    forced_includes: tuple[str, ...] = (),
) -> None:
    is_cpp = src.suffix in {".cpp", ".cc", ".cxx"}
    compiler = cc_cxx if is_cpp else cc_c
    cmd = [
        str(compiler),
        "-c",
        str(src),
        "-o",
        str(obj),
        "-O2",
        "-Wno-unused-parameter",
        "-Wno-sign-compare",
        "-Wno-missing-field-initializers",
        "-Wno-unknown-attributes",
    ]
    if is_cpp:
        cmd.extend(["-std=c++17", "-fno-exceptions"])
    else:
        cmd.extend(["-std=gnu11", "-Wno-incompatible-function-pointer-types"])
    cmd.append("-D_WASI_EMULATED_SIGNAL")
    cmd.extend(f"-D{def_name}" for def_name in defines)
    for inc_file in forced_includes:
        cmd.extend(["-include", inc_file])
    cmd.extend(f"-I{inc}" for inc in include_dirs)
    res = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if res.returncode != 0:
        raise AssertionError(
            f"WASI compile failed for {src.name}:\nCMD: {' '.join(cmd)}\nSTDOUT:\n{res.stdout}\nSTDERR:\n{res.stderr}"
        )


def _run_wasi_module(
    node_bin: Path,
    wasm_path: Path,
    cwd: Path,
    args: tuple[str, ...],
    timeout: int,
) -> subprocess.CompletedProcess[str]:
    runner = wasm_path.parent / "run_wasi.mjs"
    runner.write_text(
        "import fs from 'node:fs';\n"
        "import { WASI } from 'node:wasi';\n"
        "const [wasmFile, ...appArgs] = process.argv.slice(2);\n"
        "const wasi = new WASI({\n"
        "  version: 'preview1',\n"
        "  args: [wasmFile, ...appArgs],\n"
        "  preopens: { '.': '.' },\n"
        "  returnOnExit: true,\n"
        "});\n"
        "const mod = await WebAssembly.compile(fs.readFileSync(wasmFile));\n"
        "const inst = await WebAssembly.instantiate(mod, wasi.getImportObject());\n"
        "const code = wasi.start(inst);\n"
        "process.exit(typeof code === 'number' ? code : 0);\n",
        encoding="utf-8",
    )
    return subprocess.run(
        [str(node_bin), str(runner), str(wasm_path), *args],
        cwd=str(cwd),
        capture_output=True,
        text=True,
        timeout=timeout,
        check=False,
    )


def build_and_run_wasi_c(
    test_case: unittest.TestCase,
    sources: list[Path] | tuple[Path, ...],
    *,
    include_dirs: tuple[Path, ...] = (),
    defines: tuple[str, ...] = (),
    forced_includes: tuple[str, ...] = (),
    args: tuple[str, ...] = (),
    timeout: int = 300,
    workdir: Path | None = None,
) -> subprocess.CompletedProcess[str]:
    """Compile C/C++ sources to WASI with WASI-SDK and run under Node WASI."""
    repo_root = Path(__file__).resolve().parents[1]
    cc_c = find_wasi_c_compiler(repo_root)
    cc_cxx = find_wasi_cxx(repo_root)
    node_bin = find_node_binary(repo_root)
    require_compiler(test_case, cc_c and cc_cxx, "WASI-SDK C/C++ compiler")
    require_compiler(test_case, node_bin, "Node.js WASI runtime")
    assert cc_c is not None and cc_cxx is not None and node_bin is not None

    with tempfile.TemporaryDirectory() as tmp:
        tmp_dir = Path(tmp)
        objs: list[Path] = []
        has_cpp = any(Path(s).suffix in {".cpp", ".cc", ".cxx"} for s in sources)
        for idx, src in enumerate(sources):
            obj = tmp_dir / f"obj_{idx}_{Path(src).stem}.o"
            _compile_wasi_object(
                cc_c,
                cc_cxx,
                Path(src),
                obj,
                include_dirs,
                defines,
                forced_includes,
            )
            objs.append(obj)
        wasm_path = tmp_dir / "test_wasi.wasm"
        linker = cc_cxx if has_cpp else cc_c
        link_cmd = [str(linker), *[str(o) for o in objs], "-lwasi-emulated-signal", "-o", str(wasm_path)]
        link_res = subprocess.run(link_cmd, capture_output=True, text=True, check=False)
        test_case.assertEqual(
            link_res.returncode,
            0,
            f"WASI link failed:\n{link_res.stdout}\n{link_res.stderr}",
        )
        run_res = _run_wasi_module(node_bin, wasm_path, workdir or repo_root, args, timeout)
        test_case.assertEqual(
            run_res.returncode,
            0,
            f"WASI execution failed:\nSTDOUT:\n{run_res.stdout}\nSTDERR:\n{run_res.stderr}",
        )
        return run_res


def build_and_run_firmware_cxx(
    test_case: unittest.TestCase,
    cpp_body: str,
    *,
    extra_sources: tuple[Path, ...] = (),
    include_dirs: tuple[Path, ...] = (),
    defines: tuple[str, ...] = (),
    include_fakes_runtime: bool = True,
    sanitize: bool = True,
    timeout: int = 300,
) -> subprocess.CompletedProcess[str]:
    """Compile and execute an ad-hoc C++17 behavioral test against firmware headers."""
    repo_root = Path(__file__).resolve().parents[1]
    fw_src = repo_root / "firmware" / "BmoGameboy" / "src"
    incs: list[Path] = []
    if include_fakes_runtime:
        incs.append(repo_root / "tests" / "host" / "fakes" / "runtime")
    incs.extend([repo_root / "tests" / "host" / "fakes", repo_root / "tests" / "host", fw_src])
    incs.extend(fw_src / sub for sub in ("app", "cores", "hal", "storage", "ui"))
    incs.extend(include_dirs)
    with tempfile.TemporaryDirectory() as tmp:
        driver = Path(tmp) / "fw_behavior_driver.cpp"
        driver.write_text(cpp_body, encoding="utf-8")
        res = build_and_run_native_cxx(
            test_case,
            [driver, *extra_sources],
            include_dirs=tuple(incs),
            defines=("BMO_HOST_TEST=1", "LOG_LEVEL=0", *defines),
            sanitize=sanitize,
            timeout=timeout,
            workdir=repo_root,
        )
        test_case.assertEqual(
            res.returncode,
            0,
            f"Firmware C++ behavior check failed:\nSTDOUT:\n{res.stdout}\nSTDERR:\n{res.stderr}",
        )
        return res
