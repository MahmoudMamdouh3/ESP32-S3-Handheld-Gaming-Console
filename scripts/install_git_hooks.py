"""
scripts/install_git_hooks.py — Automated Git Pre-Commit Hook Installer
Installs a pre-commit hook in .git/hooks/ to ensure that AGENT_KNOWLEDGE_GRAPH.json,
AGENT_DECISION_TREE.json, AGENT_MANIFEST.json, and CI validation run automatically on every commit.
"""

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
GIT_HOOKS_DIR = REPO_ROOT / ".git" / "hooks"


def install_hooks():
    if not GIT_HOOKS_DIR.exists():
        print(f"Git hooks directory not found at: {GIT_HOOKS_DIR}")
        return 1

    pre_commit_path = GIT_HOOKS_DIR / "pre-commit"

    # Shell script for bash / Windows git bash
    hook_content = """#!/bin/sh
# BMO Handheld Console — Pre-Commit AI Knowledge Base & Validation Hook
echo "[Git Hook] Synchronizing AI Knowledge Graph & Decision Tree..."
python -m tools.guardian index

echo "[Git Hook] Running AI Guardian CI Validation Gate..."
python scripts/validate_repo.py
if [ $? -ne 0 ]; then
    echo "[Git Hook] Pre-commit validation FAILED. Commit aborted."
    exit 1
fi
echo "[Git Hook] Pre-commit validation PASSED."
exit 0
"""

    pre_commit_path.write_text(hook_content, encoding="utf-8")
    print(f"[OK] Git pre-commit hook successfully installed at: {pre_commit_path}")
    return 0


if __name__ == "__main__":
    sys.exit(install_hooks())
