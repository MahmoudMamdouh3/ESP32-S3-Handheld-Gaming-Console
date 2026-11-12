"""Shared compiler discovery for C/C++ fixtures."""

import os
import shutil
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
            try:
                tmp_ctx.cleanup()
            except OSError:
                pass
