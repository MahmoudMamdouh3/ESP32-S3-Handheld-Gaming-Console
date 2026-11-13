"""Configure git to use the tracked .githooks directory."""

import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]


def install_hooks() -> int:
    hooks_dir = REPO_ROOT / ".githooks"
    if not hooks_dir.is_dir():
        print(f"Tracked hooks directory not found: {hooks_dir}")
        return 1

    subprocess.run(
        ["git", "-C", str(REPO_ROOT), "config", "core.hooksPath", ".githooks"],
        check=True,
    )
    print(f"[OK] Configured core.hooksPath = .githooks")
    return 0


if __name__ == "__main__":
    raise SystemExit(install_hooks())
