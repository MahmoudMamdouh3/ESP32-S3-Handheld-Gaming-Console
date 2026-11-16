# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""Fast static checks, include case checks, guardian audit, lint, and unit test gates."""

from __future__ import annotations

import argparse
import os
import re
import shutil
import sys
from pathlib import Path

from tools.tasks import REPO_ROOT, run_checked
from tools.tasks.tests import run_host_ctest

_FAST_SKIP_TESTS = {
    "test_native_cores_execution.py",
    "test_enclosure_generator.py",
    "test_all_tiers_validation.py",
    "test_tier1_validation.py",
    "test_tier2_validation.py",
    "test_bmo_face_spring.py",
    "test_bmo_mascot_engine.py",
    "test_genesis_emu.py",
    "test_pce_emu.py",
    "test_wswan_emu.py",
    "test_new_cores_and_qol.py",
    "test_mutation_sanity.py",
    "test_validate_repo_strict_mode.py",
}


def _find_case_mismatch(source: Path, inc: str, repo_root: Path) -> str | None:
    cur = source.parent
    for part in inc.split("/"):
        if part in {"", "."}:
            continue
        if part == "..":
            cur = cur.parent
            continue
        if not cur.is_dir():
            return None
        entries = os.listdir(cur)
        if part in entries:
            cur = cur / part
            continue
        actual = {entry.lower(): entry for entry in entries}.get(part.lower())
        if actual:
            return f'{source.relative_to(repo_root).as_posix()}: #include "{inc}" mismatches exact disk case "{actual}"'
        return None
    return None


def check_include_case_sensitivity(repo_root: Path = REPO_ROOT) -> list[str]:
    """Verify every quoted #include path matches exact filename casing on disk (catches Linux breaks on Windows)."""
    include_re = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)
    issues: list[str] = []
    for top in ("firmware/BmoGameboy", "tests", "simulator"):
        base = repo_root / top
        if not base.is_dir():
            continue
        for source in sorted(base.rglob("*")):
            if source.suffix.lower() not in {".c", ".cpp", ".h", ".hpp", ".ino"}:
                continue
            text = source.read_text(encoding="utf-8", errors="ignore")
            issues.extend(
                mismatch
                for match in include_re.finditer(text)
                if (mismatch := _find_case_mismatch(source, match.group(1).replace("\\", "/"), repo_root))
            )
    return issues


def _run_structural_and_doc_checks(failures: list[str]) -> None:
    from scripts import check_markdown_links, validate_repo, verify_color_tokens

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
    failures.extend(case_issues)
    if not case_issues:
        print("  PASS: 0 case mismatches across firmware/, tests/, simulator/")

    print("[3/6] Markdown link integrity check...")
    link_issues = check_markdown_links.find_markdown_link_issues(REPO_ROOT)
    for file_path, line_num, cat, msg in link_issues:
        failures.append(f"{file_path}:{line_num} [{cat}] {msg}")
    if not link_issues:
        print("  PASS: 0 markdown link issues")

    print("[4/6] UI color token parity check...")
    if verify_color_tokens.main() != 0:
        failures.append("verify_color_tokens failed")


def _run_static_analyzers(args: argparse.Namespace, failures: list[str]) -> None:
    print("[5/6] Guardian static audit...")
    guardian_cmd = [sys.executable, "-m", "tools.guardian", "audit", *(["--cppcheck"] if args.cppcheck else [])]
    if run_checked(guardian_cmd).returncode != 0:
        failures.append("tools.guardian audit failed")

    ruff_bin = shutil.which("ruff")
    if ruff_bin:
        print("[5b] Running ruff check...")
        if run_checked([ruff_bin, "check", "."]).returncode != 0:
            failures.append("ruff check failed")
    elif args.strict_lint:
        failures.append("ruff not installed (--strict-lint)")
    else:
        print("  INFO: ruff not installed; skipping Python lint check.")

    mypy_bin = shutil.which("mypy")
    if mypy_bin:
        print("[5b2] Running mypy strict type check...")
        if run_checked([mypy_bin, "scripts", "hardware", "tools", "simulator", "benchmarks", "tasks.py"]).returncode != 0:
            failures.append("mypy failed")
    elif args.strict_lint:
        failures.append("mypy not installed (--strict-lint)")
    else:
        print("  INFO: mypy not installed; skipping Python type check.")

    tsc_bin = REPO_ROOT / ".tools" / "node" / "node_modules" / ".bin" / ("tsc.cmd" if os.name == "nt" else "tsc")
    if tsc_bin.is_file():
        print("[5b3] Running TypeScript JS type check...")
        if run_checked([str(tsc_bin), "--noEmit", "-p", "jsconfig.json"]).returncode != 0:
            failures.append("tsc --noEmit -p jsconfig.json failed")

    print("[5c] AI codemap & simulator bundle sync check...")
    if run_checked([sys.executable, str(REPO_ROOT / "scripts" / "sync_ai_codemap.py"), "--check"]).returncode != 0:
        failures.append("scripts/sync_ai_codemap.py --check failed (run --write)")
    if run_checked([sys.executable, str(REPO_ROOT / "scripts" / "bundle_simulator.py"), "--check"]).returncode != 0:
        failures.append("scripts/bundle_simulator.py --check failed (run --write)")


def _run_check_unit_tests(args: argparse.Namespace, failures: list[str]) -> None:
    print("[6/6] Running fast host unit tests (guardian + non-WASM core logic)...")
    if run_checked([sys.executable, "-m", "unittest", "discover", "-s", "tools/guardian/tests"]).returncode != 0:
        failures.append("tools/guardian/tests failed")
    if args.with_all_tests:
        if run_checked([sys.executable, "-m", "unittest", "discover", "-s", "tests"]).returncode != 0:
            failures.append("tests/ suite failed")
    else:
        test_env = os.environ.copy()
        existing_pp = test_env.get("PYTHONPATH", "")
        test_env["PYTHONPATH"] = f"{REPO_ROOT}{os.pathsep}{REPO_ROOT / 'tests'}" + (f"{os.pathsep}{existing_pp}" if existing_pp else "")
        skip = _FAST_SKIP_TESTS if getattr(args, "fast", False) else {"test_native_cores_execution.py", "test_enclosure_generator.py"}
        fast_modules = sorted(f"tests.{p.stem}" for p in (REPO_ROOT / "tests").glob("test_*.py") if p.name not in skip)
        if run_checked([sys.executable, "-m", "unittest", *fast_modules], env=test_env).returncode != 0:
            failures.append("fast host tests failed")


def cmd_check(args: argparse.Namespace) -> int:
    """Run fast local verification (< 1 min, no firmware compile)."""
    sys.path.insert(0, str(REPO_ROOT))
    sys.path.insert(0, str(REPO_ROOT / "scripts"))
    failures: list[str] = []

    _run_structural_and_doc_checks(failures)
    _run_static_analyzers(args, failures)
    if not args.no_tests:
        _run_check_unit_tests(args, failures)
        if not getattr(args, "fast", False) and (
            ctest_err := run_host_ctest(use_sanitizers=getattr(args, "sanitizers", False))
        ):
            failures.append(ctest_err)
    elif getattr(args, "sanitizers", False) and (ctest_err := run_host_ctest(use_sanitizers=True)):
        failures.append(ctest_err)

    if failures:
        print("\nCHECK FAILED:")
        for item in failures:
            print(f"  - {item}")
        return 1
    print("\nCHECK PASSED.")
    return 0
