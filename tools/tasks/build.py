# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""Firmware compilation, dependency setup, formatting, and ELF/PSRAM size gates."""

from __future__ import annotations

import argparse
import hashlib
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

from tools.tasks import (
    ARDUINO_CLI_CHECKSUMS,
    ARDUINO_CLI_VERSION,
    BUILD_PROFILES,
    BUILD_ROOT,
    FIRMWARE_DIR,
    FIRST_PARTY_WARN_RE,
    PARTITIONS_CSV,
    REPO_ROOT,
    REQUIREMENTS_TXT,
    SKETCH_YAML,
    run_checked,
)


def _parse_platform_entry(item: str) -> dict[str, str]:
    clean = item.split(":", 1)[1].strip() if item.startswith("platform:") else item
    match = re.match(r"^([^\s(]+)\s*\(([^)]+)\)$", clean)
    if match:
        return {"id": match.group(1), "version": match.group(2), "url": ""}
    return {"id": clean, "version": "", "url": ""}


def parse_sketch_yaml(path: Path = SKETCH_YAML) -> dict[str, object]:
    """Parse board profile, platform pins, and library pins from firmware/BmoGameboy/sketch.yaml."""
    if not path.is_file():
        raise FileNotFoundError(f"sketch.yaml not found: {path}")

    default_profile = ""
    fqbn = ""
    platforms: list[dict[str, str]] = []
    libraries: list[tuple[str, str]] = []
    section = ""
    current_platform: dict[str, str] | None = None

    for raw_line in path.read_text(encoding="utf-8").splitlines():
        stripped = raw_line.strip()
        if not stripped or stripped.startswith("#"):
            continue
        indent = len(raw_line) - len(raw_line.lstrip(" "))
        if stripped.startswith("default_profile:"):
            default_profile = stripped.split(":", 1)[1].strip()
        elif stripped.startswith("fqbn:") and indent >= 4:
            fqbn = stripped.split(":", 1)[1].strip()
        elif stripped in {"platforms:", "libraries:"}:
            section = stripped[:-1]
            current_platform = None
        elif section == "platforms" and stripped.startswith("- "):
            current_platform = _parse_platform_entry(stripped[2:].strip())
            platforms.append(current_platform)
        elif section == "platforms" and stripped.startswith("platform_index_url:") and current_platform is not None:
            current_platform["url"] = stripped.split(":", 1)[1].strip()
        elif section == "libraries" and stripped.startswith("- "):
            match = re.match(r"^(.+?)\s*\(([^)]+)\)$", stripped[2:].strip())
            if match:
                libraries.append((match.group(1).strip(), match.group(2).strip()))

    return {"default_profile": default_profile or "esp32s3", "fqbn": fqbn, "platforms": platforms, "libraries": libraries}


def find_arduino_cli() -> str | None:
    """Locate arduino-cli binary in PATH, repo root, or .tools/arduino-cli/."""
    for candidate in ("arduino-cli", "arduino-cli.exe"):
        if found := shutil.which(candidate):
            return found
    for path in (
        REPO_ROOT / "arduino-cli.exe",
        REPO_ROOT / "arduino-cli",
        REPO_ROOT / ".tools" / "arduino-cli" / "arduino-cli.exe",
        REPO_ROOT / ".tools" / "arduino-cli" / "arduino-cli",
    ):
        if path.is_file():
            return str(path)
    return None


def find_clang_format() -> str | None:
    """Locate clang-format binary in PATH or .tools/wasi-sdk-*."""
    if found := shutil.which("clang-format"):
        return found
    for candidate in sorted((REPO_ROOT / ".tools").glob("wasi-sdk-*/bin/clang-format*")):
        if candidate.is_file() and candidate.suffix.lower() in {"", ".exe"}:
            return str(candidate)
    return None


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
        raise RuntimeError(f"SHA-256 verification failed for {archive_name}: expected {expected_sha}, got {actual_sha}")

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


def first_party_cpp_files() -> list[Path]:
    """Return sorted first-party C/C++ sources/headers (never third_party)."""
    dirs = (
        "firmware/BmoGameboy/src/app",
        "firmware/BmoGameboy/src/hal",
        "firmware/BmoGameboy/src/storage",
        "firmware/BmoGameboy/src/ui",
        "firmware/BmoGameboy/src/cores",
        "firmware/BmoGameboy/src/tests",
        "tests/host",
    )
    files: list[Path] = []
    for rel in dirs:
        directory = REPO_ROOT / rel
        if directory.is_dir():
            files.extend(p for p in sorted(directory.iterdir()) if p.is_file() and p.suffix.lower() in {".c", ".cpp", ".h", ".hpp"})
    return files


def _is_module_available(module: str) -> bool:
    try:
        __import__(module)
        return True
    except ImportError:
        return False


def _ensure_python_deps() -> None:
    deps = (("PIL", "Pillow"), ("numpy", "numpy"), ("trimesh", "trimesh"), ("manifold3d", "manifold3d"))
    missing = [pkg for module, pkg in deps if not _is_module_available(module)]
    if missing:
        print(f"  Missing packages: {', '.join(missing)}")
        subprocess.run([sys.executable, "-m", "pip", "install", "-r", str(REQUIREMENTS_TXT)], check=True)
    else:
        print("  Python runtime dependencies already installed.")


def cmd_setup(args: argparse.Namespace) -> int:
    """Install git hooks, verify python deps, and sync pinned Arduino core/libs from sketch.yaml."""
    sketch = parse_sketch_yaml()
    if not args.no_hooks:
        print("[1/3] Configuring repository Git hooks (.githooks)...")
        subprocess.run([sys.executable, str(REPO_ROOT / "scripts" / "install_git_hooks.py")], cwd=str(REPO_ROOT), check=True)
    if not args.no_pip:
        print("[2/3] Checking Python package requirements...")
        _ensure_python_deps()

    print("[3/3] Syncing pinned Arduino ESP32 core & libraries from sketch.yaml...")
    cli = find_arduino_cli() or (install_arduino_cli_verified() if args.install_arduino_cli else None)
    if not cli:
        print("  arduino-cli not found. Pass --install-arduino-cli to download & SHA-256 verify v" + ARDUINO_CLI_VERSION)
        return 1

    platforms: list[dict[str, str]] = sketch["platforms"]  # type: ignore[assignment]
    urls = [p["url"] for p in platforms if p.get("url")]
    if urls:
        subprocess.run([cli, "core", "update-index", "--additional-urls", ",".join(urls)], check=True)
    for plat in platforms:
        target = f"{plat['id']}@{plat['version']}" if plat.get("version") else plat["id"]
        cmd = [cli, "core", "install", target]
        if plat.get("url"):
            cmd.extend(["--additional-urls", plat["url"]])
        subprocess.run(cmd, check=True)
    libraries: list[tuple[str, str]] = sketch["libraries"]  # type: ignore[assignment]
    for name, version in libraries:
        subprocess.run([cli, "lib", "install", f"{name}@{version}"], check=True)
    print("Setup complete.")
    return 0


def _compile_single_profile(
    cli: str,
    sketch_cfg: dict[str, object],
    profile_key: str,
    build_path: Path,
    warnings_mode: str,
    clean: bool,
    export_binaries: bool,
    compile_commands_only: bool,
) -> tuple[int, list[str], str]:
    profile_name = str(sketch_cfg["default_profile"])
    flags = list(BUILD_PROFILES[profile_key])
    if warnings_mode in {"strict", "all"}:
        flags.append("-Wshadow")

    cmd = [cli, "compile", "--profile", profile_name, "--build-path", str(build_path)]
    if compile_commands_only:
        cmd.append("--only-compilation-database")
    elif warnings_mode in {"strict", "all"}:
        cmd.extend(["--warnings", "all"])
    if clean and not compile_commands_only:
        cmd.append("--clean")
    if export_binaries and not compile_commands_only:
        cmd.append("--export-binaries")
    if flags:
        cmd.extend(["--build-property", f"compiler.c.extra_flags={' '.join(flags)}"])
        cmd.extend(["--build-property", f"compiler.cpp.extra_flags={' '.join(flags)}"])
    cmd.append(str(FIRMWARE_DIR))

    print(f"==> [{profile_key}] {' '.join(cmd)}")
    proc = subprocess.run(cmd, cwd=str(REPO_ROOT), text=True, capture_output=True)
    combined = (proc.stdout or "") + (proc.stderr or "")
    fp_warnings = [line.strip() for line in combined.splitlines() if FIRST_PARTY_WARN_RE.search(line)]
    return proc.returncode, fp_warnings, combined


def _generate_compdb(cli: str, sketch_cfg: dict[str, object], warnings: str) -> int:
    compdb_dir = BUILD_ROOT / "compdb"
    compdb_dir.mkdir(parents=True, exist_ok=True)
    rc, _, out = _compile_single_profile(cli, sketch_cfg, "default", compdb_dir, warnings, False, False, True)
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


def cmd_build(args: argparse.Namespace) -> int:
    """Compile firmware profiles (with optional compile_commands.json generation and first-party warning audit)."""
    cli = find_arduino_cli()
    if not cli:
        print("arduino-cli not found. Run `python tasks.py setup --install-arduino-cli` first.")
        return 1
    sketch_cfg = parse_sketch_yaml()
    if args.compile_commands:
        return _generate_compdb(cli, sketch_cfg, args.warnings)

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
            cli, sketch_cfg, profile_key, build_dir, args.warnings, args.clean, args.export_binaries, False
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
    targets = (
        [str(Path(f).resolve()) for f in args.files if "third_party" not in Path(f).parts and Path(f).is_file()]
        if args.files
        else [str(p) for p in first_party_cpp_files()]
    )
    if not targets:
        print("No matching first-party C/C++ files found.")
        return 0
    cmd = [cf, "--style=file", *(["--dry-run", "--Werror"] if args.check else ["-i"]), *targets]
    res = run_checked(cmd)
    if res.returncode == 0:
        print(f"clang-format {'check ' if args.check else ''}clean on {len(targets)} first-party files.")
    return res.returncode


def _validate_size_budgets(elf_path: Path | None, flash_pct: float, dram_pct: float) -> int:
    if not elf_path:
        print("FAIL: Cannot verify budgets without compiled ELF.")
        return 1
    built_partitions = elf_path.parent / "partitions.csv"
    if not built_partitions.is_file() or built_partitions.read_bytes() != PARTITIONS_CSV.read_bytes():
        print("FAIL: Built partitions.csv is missing or differs from firmware/BmoGameboy/partitions.csv.")
        return 1
    if flash_pct > 85.0 or dram_pct > 85.0:
        print(f"FAIL: Flash {flash_pct:.1f}% or DRAM {dram_pct:.1f}% exceeds 85.0% budget gate.")
        return 1
    print(f"Budget gates passed: Flash {flash_pct:.1f}% <= 85%, DRAM {dram_pct:.1f}% <= 85%.")
    return 0


def cmd_size(args: argparse.Namespace) -> int:
    """Report Flash, static DRAM, IRAM, and Octal PSRAM budgets and optionally write Markdown summary."""
    sys.path.insert(0, str(REPO_ROOT))
    from scripts import validate_repo
    from tools.guardian.core.elf_analyzer import ElfAnalyzer
    from tools.guardian.core.psram_budget import PsramBudgetAnalyzer

    analyzer = ElfAnalyzer(REPO_ROOT)
    elf_path = Path(args.elf) if args.elf else analyzer.find_latest_elf()
    try:
        elf_res = analyzer.analyze(elf_path)
    except (FileNotFoundError, RuntimeError) as exc:
        print(f"ELF size inspection unavailable: {exc}")
        return 1 if (args.check_budgets or args.elf) else 0

    app0_size = validate_repo._parse_partitions_csv(PARTITIONS_CSV).get("app0", {}).get("size", 8 * 1024 * 1024)
    bin_path = elf_res.elf_path.with_suffix(".bin") if elf_res.elf_path and elf_res.elf_path.with_suffix(".bin").is_file() else None
    flash_bytes = bin_path.stat().st_size if bin_path else elf_res.flash_used_bytes
    flash_pct = (flash_bytes / app0_size * 100.0) if app0_size else 0.0
    dram_total = elf_res.dram_total_bytes or 327680
    dram_pct = (elf_res.dram_used_bytes / dram_total * 100.0) if dram_total else 0.0
    psram = PsramBudgetAnalyzer(REPO_ROOT).to_dict()

    rel_elf = ""
    if elf_res.elf_path:
        try:
            rel_elf = elf_res.elf_path.resolve().relative_to(REPO_ROOT.resolve()).as_posix()
        except ValueError:
            rel_elf = elf_res.elf_path.name

    report = "\n".join([
        "### ESP32-S3 Memory Footprint Report (`TARGET_COMPILED`)",
        "",
        f"- **ELF:** `{rel_elf}`" if rel_elf else "- **ELF:** not found (run `python tasks.py build`)",
        f"- **App Image vs `app0` ({app0_size:,} B):** `{flash_bytes:,}` B (**{flash_pct:.1f}%**, `{app0_size - flash_bytes:,}` B headroom)",
        f"- **Static DRAM vs {dram_total:,} B:** `{elf_res.dram_used_bytes:,}` B (**{dram_pct:.1f}%**)",
        f"- **IRAM Used:** `{elf_res.iram_used_bytes:,}` B",
        f"- **PSRAM Catalog Pool + Persistent Buffers (Source-Derived):** `RomFile={psram['rom_file_sizeof']} B`, idle floor `{psram['persistent_total_floor']:,}` B (`{psram['free_after_boot_floor']:,}` B free of 8 MiB)",
    ])
    print(report)
    if summary_path := (args.markdown_summary or os.environ.get("GITHUB_STEP_SUMMARY", "")):
        with open(summary_path, "a", encoding="utf-8") as fh:
            fh.write(report + "\n")
    return _validate_size_budgets(elf_res.elf_path, flash_pct, dram_pct) if args.check_budgets else 0
