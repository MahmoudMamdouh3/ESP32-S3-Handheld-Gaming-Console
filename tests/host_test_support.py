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
