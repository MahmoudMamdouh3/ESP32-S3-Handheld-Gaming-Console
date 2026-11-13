#!/usr/bin/env python3
"""Cross-platform task runner for BMO Handheld firmware, tests, lint, formatting, and CI."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import urllib.request
import zipfile
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple

REPO_ROOT = Path(__file__).resolve().parent
FIRMWARE_DIR = REPO_ROOT / "firmware" / "BmoGameboy"
SKETCH_YAML = FIRMWARE_DIR / "sketch.yaml"
PARTITIONS_CSV = FIRMWARE_DIR / "partitions.csv"
REQUIREMENTS_TXT = REPO_ROOT / "requirements.txt"
BUILD_ROOT = REPO_ROOT / "build"

ARDUINO_CLI_VERSION = "1.6.0"
ARDUINO_CLI_CHECKSUMS: Dict[Tuple[str, str], Tuple[str, str]] = {
    ("Windows", "AMD64"): (
        f"https://github.com/arduino/arduino-cli/releases/download/v{ARDUINO_CLI_VERSION}/"
        f"arduino-cli_{ARDUINO_CLI_VERSION}_Windows_64bit.zip",
        "2769b9b282ce597db9679d8ec14d726d57004ca4205ef4d7a795bf36b6cdc9d8",
    ),
    ("Linux", "x86_64"): (
        f"https://github.com/arduino/arduino-cli/releases/download/v{ARDUINO_CLI_VERSION}/"
        f"arduino-cli_{ARDUINO_CLI_VERSION}_Linux_64bit.tar.gz",
        "321ee249f44c78053234fec0f36101092bc6021bf24939e15e0cc2449c5ed250",
    ),
    ("Darwin", "x86_64"): (
        f"https://github.com/arduino/arduino-cli/releases/download/v{ARDUINO_CLI_VERSION}/"
        f"arduino-cli_{ARDUINO_CLI_VERSION}_macOS_64bit.tar.gz",
        "af6a30cf5f57cc076b45fe55c6fd389440156f70706e7f8d3302a11a9fe32913",
    ),
    ("Darwin", "arm64"): (
        f"https://github.com/arduino/arduino-cli/releases/download/v{ARDUINO_CLI_VERSION}/"
        f"arduino-cli_{ARDUINO_CLI_VERSION}_macOS_ARM64.tar.gz",
        "dc70f037fcbc8e7add892282bed5bab5eac317ff00bf0dd2f21e4a8e1d338a1c",
    ),
}

BUILD_PROFILES: Dict[str, List[str]] = {
    "default": [],
    "no-doom": ["-DBMO_ENABLE_DOOM=0"],
    "mit": ["-DBMO_EXPERIMENTAL_CORES=0", "-DBMO_ENABLE_DOOM=0"],
    "audio-compile-only": ["-DFEATURE_AUDIO=1", "-DBMO_HARDWARE_CONFIRMED_AUDIO=1"],
    "battery-compile-only": ["-DFEATURE_BATTERY_MONITOR=1", "-DBMO_HARDWARE_CONFIRMED_BATTERY=1"],
    "debug-log-compile-only": ["-DLOG_LEVEL=LOG_LEVEL_DEBUG"],
    "profiler-compile-only": ["-DBMO_PROFILE=1"],
}

FIRST_PARTY_WARN_RE = re.compile(
    r"firmware[/\\]BmoGameboy[/\\](BmoGameboy\.ino|src[/\\](app|hal|storage|ui|cores|tests)[/\\]).*?:\s*warning:",
    re.IGNORECASE,
)


def parse_sketch_yaml(path: Path = SKETCH_YAML) -> Dict[str, object]:
    """Parse board profile, platform pins, and library pins from firmware/BmoGameboy/sketch.yaml."""
    if not path.is_file():
        raise FileNotFoundError(f"sketch.yaml not found: {path}")

    default_profile = ""
    fqbn = ""
    platforms: List[Dict[str, str]] = []
    libraries: List[Tuple[str, str]] = []

    section = ""
    current_platform: Optional[Dict[str, str]] = None
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        stripped = raw_line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        indent = len(raw_line) - len(raw_line.lstrip(" "))
        if stripped.startswith("default_profile:"):
            default_profile = stripped.split(":", 1)[1].strip()
        elif stripped.startswith("fqbn:") and indent >= 4:
            fqbn = stripped.split(":", 1)[1].strip()
        elif stripped == "platforms:":
            section = "platforms"
            current_platform = None
        elif stripped == "libraries:":
            section = "libraries"
            current_platform = None
        elif section == "platforms" and stripped.startswith("- "):
            item = stripped[2:].strip()
            if item.startswith("platform:"):
                item = item.split(":", 1)[1].strip()
            m = re.match(r"^([^\s(]+)\s*\(([^)]+)\)$", item)
            if m:
                current_platform = {"id": m.group(1), "version": m.group(2), "url": ""}
                platforms.append(current_platform)
            else:
                current_platform = {"id": item, "version": "", "url": ""}
                platforms.append(current_platform)
        elif section == "platforms" and stripped.startswith("platform_index_url:") and current_platform is not None:
            current_platform["url"] = stripped.split(":", 1)[1].strip()
        elif section == "libraries" and stripped.startswith("- "):
            item = stripped[2:].strip()
            m = re.match(r"^(.+?)\s*\(([^)]+)\)$", item)
            if m:
                libraries.append((m.group(1).strip(), m.group(2).strip()))

    return {
        "default_profile": default_profile or "esp32s3",
        "fqbn": fqbn,
        "platforms": platforms,
        "libraries": libraries,
    }


def find_arduino_cli() -> Optional[str]:
    """Locate arduino-cli binary in PATH, repo root, or .tools/arduino-cli/."""
    for candidate in ("arduino-cli", "arduino-cli.exe"):
        found = shutil.which(candidate)
        if found:
            return found
    for candidate in (
        REPO_ROOT / "arduino-cli.exe",
        REPO_ROOT / "arduino-cli",
        REPO_ROOT / ".tools" / "arduino-cli" / "arduino-cli.exe",
        REPO_ROOT / ".tools" / "arduino-cli" / "arduino-cli",
    ):
        if candidate.is_file():
            return str(candidate)
    return None


def find_clang_format() -> Optional[str]:
    """Locate clang-format binary in PATH or .tools/wasi-sdk-*."""
    found = shutil.which("clang-format")
    if found:
        return found
    for candidate in sorted((REPO_ROOT / ".tools").glob("wasi-sdk-*/bin/clang-format*")):
        if candidate.is_file() and candidate.suffix.lower() in {"", ".exe"}:
            return str(candidate)
    return None


def _run_checked(
    cmd: List[str],
    *,
    cwd: Path = REPO_ROOT,
    capture: bool = False,
    env: Optional[Dict[str, str]] = None,
) -> subprocess.CompletedProcess:
    return subprocess.run(
        cmd,
        cwd=str(cwd),
        text=True,
        capture_output=capture,
        env=env,
    )


def _sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest().lower()


def install_arduino_cli_verified() -> str:
    """Download and SHA-256 verify pinned arduino-cli into .tools/arduino-cli."""
    sys_name = platform.system()
    machine = platform.machine()
    arch_key = (sys_name, "AMD64" if machine.lower() in {"amd64", "x86_64"} else machine)
    if arch_key not in ARDUINO_CLI_CHECKSUMS:
        raise RuntimeError(f"Unsupported platform for automatic arduino-cli install: {arch_key}")

    url, expected_sha = ARDUINO_CLI_CHECKSUMS[arch_key]
    dest_dir = REPO_ROOT / ".tools" / "arduino-cli"
    dest_dir.mkdir(parents=True, exist_ok=True)
    archive_name = url.rsplit("/", 1)[-1]
    archive_path = dest_dir / archive_name

    print(f"Downloading {url} ...")
    urllib.request.urlretrieve(url, archive_path)
    actual_sha = _sha256_file(archive_path)
    if actual_sha != expected_sha:
        archive_path.unlink(missing_ok=True)
        raise RuntimeError(
            f"SHA-256 verification failed for {archive_name}: expected {expected_sha}, got {actual_sha}"
        )
    print(f"Verified SHA-256: {actual_sha}")

    if archive_name.endswith(".zip"):
        with zipfile.ZipFile(archive_path) as zf:
            zf.extractall(dest_dir)
    else:
        with tarfile.open(archive_path, "r:gz") as tf:
            tf.extractall(dest_dir)
    archive_path.unlink(missing_ok=True)

    exe_path = dest_dir / ("arduino-cli.exe" if sys_name == "Windows" else "arduino-cli")
    if not exe_path.is_file():
        raise FileNotFoundError(f"Expected binary after extraction not found: {exe_path}")
    if sys_name != "Windows":
        exe_path.chmod(0o755)
    return str(exe_path)


def check_include_case_sensitivity(repo_root: Path = REPO_ROOT) -> List[str]:
    """Verify every quoted #include path matches exact filename casing on disk (catches Linux breaks on Windows)."""
    include_re = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)
    issues: List[str] = []
    for top in ("firmware/BmoGameboy", "tests", "simulator"):
        base = repo_root / top
        if not base.is_dir():
            continue
        for source in sorted(base.rglob("*")):
            if source.suffix.lower() not in {".c", ".cpp", ".h", ".hpp", ".ino"}:
                continue
            text = source.read_text(encoding="utf-8", errors="ignore")
            for match in include_re.finditer(text):
                inc = match.group(1).replace("\\", "/")
                cur = source.parent
                for part in inc.split("/"):
                    if part in {"", "."}:
                        continue
                    if part == "..":
                        cur = cur.parent
                        continue
                    if not cur.is_dir():
                        break
                    entries = os.listdir(cur)
                    if part in entries:
                        cur = cur / part
                    else:
                        lower_map = {entry.lower(): entry for entry in entries}
                        actual = lower_map.get(part.lower())
                        if actual:
                            rel = source.relative_to(repo_root).as_posix()
                            issues.append(f'{rel}: #include "{inc}" mismatches exact disk case "{actual}"')
                        break
    return issues


def first_party_cpp_files() -> List[Path]:
    """Return sorted first-party C/C++ sources/headers (never third_party)."""
    files: List[Path] = []
    for rel in (
        "firmware/BmoGameboy/src/app",
        "firmware/BmoGameboy/src/hal",
        "firmware/BmoGameboy/src/storage",
        "firmware/BmoGameboy/src/ui",
        "firmware/BmoGameboy/src/cores",
        "firmware/BmoGameboy/src/tests",
        "tests/host",
    ):
        directory = REPO_ROOT / rel
        if directory.is_dir():
            for path in sorted(directory.iterdir()):
                if path.is_file() and path.suffix.lower() in {".c", ".cpp", ".h", ".hpp"}:
                    files.append(path)
    return files


def cmd_setup(args: argparse.Namespace) -> int:
    """Install git hooks, verify python deps, and sync pinned Arduino core/libs from sketch.yaml."""
    sketch = parse_sketch_yaml()
    if not args.no_hooks:
        print("[1/3] Configuring repository Git hooks (.githooks)...")
        subprocess.run([sys.executable, str(REPO_ROOT / "scripts" / "install_git_hooks.py")], cwd=str(REPO_ROOT), check=True)

    if not args.no_pip:
        print("[2/3] Checking Python package requirements...")
        missing = []
        for module, pkg in (("PIL", "Pillow"), ("numpy", "numpy"), ("trimesh", "trimesh"), ("manifold3d", "manifold3d")):
            try:
                __import__(module)
            except ImportError:
                missing.append(pkg)
        if missing:
            print(f"  Missing packages: {', '.join(missing)}")
            print("  Installing from requirements.txt ...")
            subprocess.run([sys.executable, "-m", "pip", "install", "-r", str(REQUIREMENTS_TXT)], check=True)
        else:
            print("  Python runtime dependencies already installed.")

    print("[3/3] Syncing pinned Arduino ESP32 core & libraries from sketch.yaml...")
    cli = find_arduino_cli()
    if not cli:
        if args.install_arduino_cli:
            cli = install_arduino_cli_verified()
        else:
            print("  arduino-cli not found. Pass --install-arduino-cli to download & SHA-256 verify v" + ARDUINO_CLI_VERSION)
            return 1
    print(f"  Using arduino-cli: {cli}")

    platforms: List[Dict[str, str]] = sketch["platforms"]  # type: ignore[assignment]
    urls = [p["url"] for p in platforms if p.get("url")]
    if urls:
        subprocess.run([cli, "core", "update-index", "--additional-urls", ",".join(urls)], check=True)
    for plat in platforms:
        target = f"{plat['id']}@{plat['version']}" if plat.get("version") else plat["id"]
        cmd = [cli, "core", "install", target]
        if plat.get("url"):
            cmd.extend(["--additional-urls", plat["url"]])
        subprocess.run(cmd, check=True)

    libraries: List[Tuple[str, str]] = sketch["libraries"]  # type: ignore[assignment]
    for name, version in libraries:
        subprocess.run([cli, "lib", "install", f"{name}@{version}"], check=True)
    print("Setup complete.")
    return 0


def cmd_check(args: argparse.Namespace) -> int:
    """Run fast local verification (< 1 min, no firmware compile)."""
    sys.path.insert(0, str(REPO_ROOT))
    sys.path.insert(0, str(REPO_ROOT / "scripts"))
    from scripts import check_markdown_links, validate_repo, verify_color_tokens

    failures: List[str] = []

    print("[1/6] Python syntax, firmware hard stops, and structural checks...")
    for label, fn in (
        ("Python syntax", validate_repo.check_python_files),
        ("Safety guardrails", validate_repo.check_firmware_safety_guardrails),
        ("Static engine source gate", validate_repo.check_flash_budget_static),
        ("Structural registration", validate_repo.check_structural_soundness),
    ):
        issues = fn()
        if issues:
            failures.extend(f"{label}: {item}" for item in issues)
        else:
            print(f"  PASS: {label}")

    print("[2/6] Quoted #include filename case-sensitivity check...")
    case_issues = check_include_case_sensitivity()
    if case_issues:
        failures.extend(case_issues)
    else:
        print("  PASS: 0 case mismatches across firmware/, tests/, simulator/")

    print("[3/6] Markdown link integrity check...")
    link_issues = check_markdown_links.find_markdown_link_issues(REPO_ROOT)
    if link_issues:
        for file_path, line_num, cat, msg in link_issues:
            failures.append(f"{file_path}:{line_num} [{cat}] {msg}")
    else:
        print("  PASS: 0 markdown link issues")

    print("[4/6] UI color token parity check...")
    if verify_color_tokens.main() != 0:
        failures.append("verify_color_tokens failed")

    print("[5/6] Guardian static audit...")
    guardian_cmd = [sys.executable, "-m", "tools.guardian", "audit"]
    if args.cppcheck:
        guardian_cmd.append("--cppcheck")
    if _run_checked(guardian_cmd).returncode != 0:
        failures.append("tools.guardian audit failed")

    ruff_bin = shutil.which("ruff")
    if ruff_bin:
        print("[5b] Running ruff check...")
        if _run_checked([ruff_bin, "check", "."]).returncode != 0:
            failures.append("ruff check failed")
    elif args.strict_lint:
        failures.append("ruff not installed (--strict-lint)")
    else:
        print("  INFO: ruff not installed; skipping Python lint check.")

    if not args.no_tests:
        print("[6/6] Running fast host unit tests (guardian + non-WASM core logic)...")
        if _run_checked([sys.executable, "-m", "unittest", "discover", "-s", "tools/guardian/tests"]).returncode != 0:
            failures.append("tools/guardian/tests failed")
        if args.with_all_tests:
            if _run_checked([sys.executable, "-m", "unittest", "discover", "-s", "tests"]).returncode != 0:
                failures.append("tests/ suite failed")
        else:
            test_env = os.environ.copy()
            existing_pp = test_env.get("PYTHONPATH", "")
            test_env["PYTHONPATH"] = (
                f"{REPO_ROOT}{os.pathsep}{REPO_ROOT / 'tests'}"
                + (f"{os.pathsep}{existing_pp}" if existing_pp else "")
            )
            fast_modules = sorted(
                f"tests.{p.stem}"
                for p in (REPO_ROOT / "tests").glob("test_*.py")
                if p.name not in {"test_native_cores_execution.py", "test_enclosure_generator.py"}
            )
            if _run_checked([sys.executable, "-m", "unittest", *fast_modules], env=test_env).returncode != 0:
                failures.append("fast host tests failed")
        ctest_err = _run_host_ctest()
        if ctest_err:
            failures.append(ctest_err)

    if failures:
        print("\nCHECK FAILED:")
        for item in failures:
            print(f"  - {item}")
        return 1
    print("\nCHECK PASSED.")
    return 0


def _run_host_ctest() -> Optional[str]:
    """Configure, build, and execute host C++17 CTest suite when CMake/CTest are present."""
    cmake_bin = shutil.which("cmake")
    ctest_bin = shutil.which("ctest")
    if not cmake_bin or not ctest_bin:
        print("  INFO: cmake/ctest not found; skipping native C++17 CTest suite.")
        return None
    if not (REPO_ROOT / "CMakeLists.txt").is_file():
        return "CMakeLists.txt missing for host CTest suite"

    build_dir = REPO_ROOT / "build" / "host-ctest"
    cmds = [
        [cmake_bin, "-S", str(REPO_ROOT), "-B", str(build_dir)],
        [cmake_bin, "--build", str(build_dir), "--config", "Release"],
        [ctest_bin, "--test-dir", str(build_dir), "-C", "Release", "--output-on-failure"],
    ]
    for cmd in cmds:
        if _run_checked(cmd).returncode != 0:
            return f"host CTest step failed: {' '.join(cmd)}"
    return None


def cmd_test(args: argparse.Namespace) -> int:
    """Run full host unit test suites and validate_repo."""
    ctest_err = _run_host_ctest()
    if ctest_err:
        print(f"FAIL: {ctest_err}")
        return 1
    cmds = [
        [sys.executable, "-m", "unittest", "discover", "-s", "tests", "-v"],
        [sys.executable, "-m", "unittest", "discover", "-s", "tools/guardian/tests", "-v"],
        [sys.executable, "scripts/validate_repo.py", *(["--require-target-compile"] if args.strict else [])],
    ]
    for cmd in cmds:
        print(f"==> Running: {' '.join(cmd)}")
        if _run_checked(cmd).returncode != 0:
            return 1
    return 0


def _compile_single_profile(
    cli: str,
    sketch_cfg: Dict[str, object],
    profile_key: str,
    build_path: Path,
    warnings_mode: str,
    clean: bool,
    export_binaries: bool,
    compile_commands_only: bool,
) -> Tuple[int, List[str], str]:
    profile_name = str(sketch_cfg["default_profile"])
    extra_defs = BUILD_PROFILES[profile_key]
    c_flags = list(extra_defs)
    cpp_flags = list(extra_defs)
    if warnings_mode in {"strict", "all"}:
        c_flags.append("-Wshadow")
        cpp_flags.append("-Wshadow")

    cmd = [cli, "compile", "--profile", profile_name, "--build-path", str(build_path)]
    if compile_commands_only:
        cmd.append("--only-compilation-database")
    elif warnings_mode in {"strict", "all"}:
        cmd.extend(["--warnings", "all"])
    if clean and not compile_commands_only:
        cmd.append("--clean")
    if export_binaries and not compile_commands_only:
        cmd.append("--export-binaries")
    if c_flags:
        cmd.extend(["--build-property", f"compiler.c.extra_flags={' '.join(c_flags)}"])
    if cpp_flags:
        cmd.extend(["--build-property", f"compiler.cpp.extra_flags={' '.join(cpp_flags)}"])
    cmd.append(str(FIRMWARE_DIR))

    print(f"==> [{profile_key}] {' '.join(cmd)}")
    proc = subprocess.run(cmd, cwd=str(REPO_ROOT), text=True, capture_output=True)
    combined = (proc.stdout or "") + (proc.stderr or "")
    first_party_warnings: List[str] = []
    for line in combined.splitlines():
        if FIRST_PARTY_WARN_RE.search(line):
            first_party_warnings.append(line.strip())
    return proc.returncode, first_party_warnings, combined


def cmd_build(args: argparse.Namespace) -> int:
    """Compile firmware profiles (with optional compile_commands.json generation and first-party warning audit)."""
    cli = find_arduino_cli()
    if not cli:
        print("arduino-cli not found. Run `python tasks.py setup --install-arduino-cli` first.")
        return 1
    sketch_cfg = parse_sketch_yaml()

    if args.compile_commands:
        compdb_dir = BUILD_ROOT / "compdb"
        compdb_dir.mkdir(parents=True, exist_ok=True)
        rc, _, out = _compile_single_profile(
            cli, sketch_cfg, "default", compdb_dir, args.warnings, False, False, True
        )
        if out.strip():
            print(out.strip())
        if rc != 0:
            return rc
        src_db = compdb_dir / "compile_commands.json"
        if src_db.is_file():
            BUILD_ROOT.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src_db, BUILD_ROOT / "compile_commands.json")
            print(f"Generated {BUILD_ROOT / 'compile_commands.json'}")
        return 0

    targets = list(BUILD_PROFILES.keys()) if args.profile == "all" else [args.profile]
    for profile_key in targets:
        if args.build_dir and len(targets) == 1:
            build_dir = Path(args.build_dir)
        elif profile_key == "default":
            build_dir = BUILD_ROOT / "target-compile"
        else:
            build_dir = BUILD_ROOT / f"profile-{profile_key}"
        build_dir.mkdir(parents=True, exist_ok=True)

        rc, fp_warnings, out = _compile_single_profile(
            cli,
            sketch_cfg,
            profile_key,
            build_dir,
            args.warnings,
            args.clean,
            args.export_binaries,
            False,
        )
        for line in out.splitlines():
            if "Sketch uses" in line or "Global variables use" in line or "error:" in line.lower():
                print(line)
        print(f"  [{profile_key}] First-party warnings (-Wall -Wextra -Wshadow): {len(fp_warnings)}")
        for warn in fp_warnings:
            print(f"    {warn}")
        if rc != 0:
            print(out)
            return rc
        if args.fail_on_warnings and fp_warnings:
            print(f"Failing because --fail-on-warnings is set and {len(fp_warnings)} first-party warning(s) remain.")
            return 1
    return 0


def cmd_format(args: argparse.Namespace) -> int:
    """Format first-party firmware and host C/C++ code using clang-format (never touches third_party)."""
    cf = find_clang_format()
    if not cf:
        print("clang-format not found in PATH or .tools/. Install LLVM/clang-format cleanly.")
        return 1
    if args.files:
        targets = [
            str(Path(f).resolve())
            for f in args.files
            if "third_party" not in Path(f).parts and Path(f).is_file()
        ]
    else:
        targets = [str(p) for p in first_party_cpp_files()]
    if not targets:
        print("No matching first-party C/C++ files found.")
        return 0
    cmd = [cf, "--style=file"]
    if args.check:
        cmd.extend(["--dry-run", "--Werror"])
    else:
        cmd.append("-i")
    cmd.extend(targets)
    res = _run_checked(cmd)
    if res.returncode == 0:
        print(f"clang-format {'check ' if args.check else ''}clean on {len(targets)} first-party files.")
    return res.returncode


def cmd_size(args: argparse.Namespace) -> int:
    """Report Flash, static DRAM, IRAM, and Octal PSRAM budgets and optionally write Markdown summary."""
    sys.path.insert(0, str(REPO_ROOT))
    from scripts import validate_repo
    from tools.guardian.core.elf_analyzer import ElfAnalyzer
    from tools.guardian.core.psram_budget import PsramBudgetAnalyzer

    analyzer = ElfAnalyzer(REPO_ROOT)
    elf_path = Path(args.elf) if args.elf else analyzer.find_latest_elf()
    elf_res = analyzer.analyze(elf_path)
    parts = validate_repo._parse_partitions_csv(PARTITIONS_CSV)
    app0_size = parts.get("app0", {}).get("size", 8 * 1024 * 1024)

    bin_path: Optional[Path] = None
    if elf_res.elf_path:
        candidate = elf_res.elf_path.with_suffix(".bin")
        if candidate.is_file():
            bin_path = candidate

    flash_bytes = bin_path.stat().st_size if bin_path else elf_res.flash_used_bytes
    flash_pct = (flash_bytes / app0_size * 100.0) if app0_size else 0.0
    dram_total = elf_res.dram_total_bytes or 327680
    dram_pct = (elf_res.dram_used_bytes / dram_total * 100.0) if dram_total else 0.0
    psram = PsramBudgetAnalyzer(REPO_ROOT).to_dict()

    lines = [
        "### ESP32-S3 Memory Footprint Report (`TARGET_COMPILED`)",
        "",
        f"- **ELF:** `{elf_res.elf_path}`" if elf_res.elf_path else "- **ELF:** not found (run `python tasks.py build`)",
        f"- **App Image vs `app0` ({app0_size:,} B):** `{flash_bytes:,}` B (**{flash_pct:.1f}%**, `{app0_size - flash_bytes:,}` B headroom)",
        f"- **Static DRAM vs {dram_total:,} B:** `{elf_res.dram_used_bytes:,}` B (**{dram_pct:.1f}%**)",
        f"- **IRAM Used:** `{elf_res.iram_used_bytes:,}` B",
        f"- **PSRAM Catalog Pool + Persistent Buffers (Source-Derived):** `RomFile={psram['rom_file_sizeof']} B`, idle floor `{psram['persistent_total_floor']:,}` B (`{psram['free_after_boot_floor']:,}` B free of 8 MiB)",
    ]
    report = "\n".join(lines)
    print(report)

    summary_path = args.markdown_summary or os.environ.get("GITHUB_STEP_SUMMARY", "")
    if summary_path:
        with open(summary_path, "a", encoding="utf-8") as fh:
            fh.write(report + "\n")

    if args.check_budgets:
        if not elf_res.elf_path:
            print("FAIL: Cannot verify budgets without compiled ELF.")
            return 1
        built_partitions = elf_res.elf_path.parent / "partitions.csv"
        if not built_partitions.is_file() or built_partitions.read_bytes() != PARTITIONS_CSV.read_bytes():
            print("FAIL: Built partitions.csv is missing or differs from firmware/BmoGameboy/partitions.csv.")
            return 1
        if flash_pct > 85.0:
            print(f"FAIL: Flash app image {flash_pct:.1f}% exceeds 85.0% budget gate.")
            return 1
        if dram_pct > 85.0:
            print(f"FAIL: Static DRAM {dram_pct:.1f}% exceeds 85.0% budget gate.")
            return 1
        print(f"Budget gates passed: Flash {flash_pct:.1f}% <= 85%, DRAM {dram_pct:.1f}% <= 85%.")
    return 0


def _resolve_serial_port(cli: str, explicit_port: Optional[str]) -> Optional[str]:
    if explicit_port:
        return explicit_port
    proc = subprocess.run([cli, "board", "list", "--format", "json"], capture_output=True, text=True)
    if proc.returncode != 0:
        return None
    try:
        data = json.loads(proc.stdout)
    except json.JSONDecodeError:
        return None
    ports = [
        item.get("port", {}).get("address")
        for item in (data.get("detected_ports") or [])
        if item.get("port", {}).get("address")
    ]
    if len(ports) == 1:
        return ports[0]
    if not ports:
        print("No serial ports detected. Connect the ESP32-S3 via USB-C and pass --port COMx.")
    else:
        print(f"Multiple serial ports detected: {', '.join(ports)}. Pass --port explicitly.")
    return None


def cmd_flash(args: argparse.Namespace) -> int:
    """Safety-check config.h and upload compiled firmware to the attached ESP32-S3."""
    sys.path.insert(0, str(REPO_ROOT))
    from scripts import validate_repo

    issues = validate_repo.check_firmware_safety_guardrails()
    if issues:
        print("Refusing to flash due to firmware safety guardrail failure:")
        for item in issues:
            print(f"  - {item}")
        return 1
    cli = find_arduino_cli()
    if not cli:
        print("arduino-cli not found.")
        return 1
    port = _resolve_serial_port(cli, args.port)
    if not port:
        return 1
    sketch_cfg = parse_sketch_yaml()
    build_dir = BUILD_ROOT / "target-compile"
    cmd = [cli, "upload", "--profile", str(sketch_cfg["default_profile"]), "--port", port]
    if build_dir.is_dir():
        cmd.extend(["--input-dir", str(build_dir)])
    cmd.append(str(FIRMWARE_DIR))
    print(f"Flashing {port} ...")
    return _run_checked(cmd).returncode


def cmd_monitor(args: argparse.Namespace) -> int:
    """Open serial monitor at 115200 baud cleanly."""
    cli = find_arduino_cli()
    if not cli:
        print("arduino-cli not found.")
        return 1
    port = _resolve_serial_port(cli, args.port)
    if not port:
        return 1
    return _run_checked([cli, "monitor", "--port", port, "--config", f"baudrate={args.baud}"]).returncode


def cmd_sim(args: argparse.Namespace) -> int:
    """Launch the local loopback simulator (building WASM cores automatically if needed)."""
    cmd = [sys.executable, str(REPO_ROOT / "simulator" / "run_simulator.py")]
    if args.no_open:
        cmd.append("--no-open")
    return _run_checked(cmd).returncode


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        prog="tasks.py",
        description="Unified BMO Handheld developer task runner.",
    )
    sub = parser.add_subparsers(dest="command", required=True)

    p_setup = sub.add_parser("setup", help="Install hooks, verify Python deps, and sync pinned Arduino core/libs.")
    p_setup.add_argument("--install-arduino-cli", action="store_true", help="Download SHA-256 verified arduino-cli into .tools/.")
    p_setup.add_argument("--no-hooks", action="store_true", help="Skip configuring git core.hooksPath.")
    p_setup.add_argument("--no-pip", action="store_true", help="Skip checking Python package imports.")
    p_setup.set_defaults(func=cmd_setup)

    p_check = sub.add_parser("check", help="Fast verification (< 1 min, no firmware compile).")
    p_check.add_argument("--cppcheck", action="store_true", help="Include cppcheck static analysis.")
    p_check.add_argument("--no-tests", action="store_true", help="Skip Python unit tests.")
    p_check.add_argument("--with-all-tests", action="store_true", help="Include slower WASI execution and CAD mesh tests.")
    p_check.add_argument("--strict-lint", action="store_true", help="Fail if ruff is unavailable.")
    p_check.set_defaults(func=cmd_check)

    p_test = sub.add_parser("test", help="Run full unittest suite and repository validator.")
    p_test.add_argument("--strict", action="store_true", help="Require target compile inside validate_repo.py.")
    p_test.set_defaults(func=cmd_test)

    p_build = sub.add_parser("build", help="Compile firmware profile(s) with sketch.yaml and strict warning reporting.")
    p_build.add_argument("--profile", choices=[*BUILD_PROFILES.keys(), "all"], default="default")
    p_build.add_argument("--warnings", choices=["strict", "all", "none"], default="strict")
    p_build.add_argument("--clean", action="store_true")
    p_build.add_argument("--export-binaries", action="store_true")
    p_build.add_argument("--compile-commands", action="store_true", help="Generate build/compile_commands.json only.")
    p_build.add_argument("--build-dir", default="")
    p_build.add_argument("--fail-on-warnings", action="store_true")
    p_build.set_defaults(func=cmd_build)

    p_fmt = sub.add_parser("format", help="Check or apply clang-format on first-party C/C++ sources only.")
    p_fmt.add_argument("--check", action="store_true", help="Report formatting diffs without modifying files.")
    p_fmt.add_argument("files", nargs="*", help="Optional specific first-party files to format/check.")
    p_fmt.set_defaults(func=cmd_format)

    p_size = sub.add_parser("size", help="Report compiled Flash/DRAM/IRAM and PSRAM budget.")
    p_size.add_argument("--elf", default="", help="Path to compiled .elf (auto-discovered if omitted).")
    p_size.add_argument("--markdown-summary", default="", help="Append Markdown report to file.")
    p_size.add_argument("--check-budgets", action="store_true", help="Fail if Flash > 85% app0 or DRAM > 85%.")
    p_size.set_defaults(func=cmd_size)

    p_flash = sub.add_parser("flash", help="Safety-check and flash firmware to connected ESP32-S3.")
    p_flash.add_argument("--port", default="", help="Serial port (e.g., COM3 or /dev/ttyACM0).")
    p_flash.set_defaults(func=cmd_flash)

    p_mon = sub.add_parser("monitor", help="Open serial monitor at 115200 baud.")
    p_mon.add_argument("--port", default="", help="Serial port (e.g., COM3 or /dev/ttyACM0).")
    p_mon.add_argument("--baud", type=int, default=115200)
    p_mon.set_defaults(func=cmd_monitor)

    p_sim = sub.add_parser("sim", help="Launch local browser simulator.")
    p_sim.add_argument("--no-open", action="store_true")
    p_sim.set_defaults(func=cmd_sim)

    parsed = parser.parse_args(argv)
    return int(parsed.func(parsed))


if __name__ == "__main__":
    raise SystemExit(main())
