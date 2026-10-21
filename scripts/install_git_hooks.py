"""
scripts/install_git_hooks.py — Automated Git Pre-Commit & Commit-Msg Hook Installer
Installs:
  1. .git/hooks/pre-commit: Runs scripts/validate_repo.py (which rebuilds knowledge base + runs all 8 CI phases)
  2. .git/hooks/commit-msg: Enforces structured two-part commit messages (-m "subject" -m "- bullet points")
"""

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
GIT_HOOKS_DIR = REPO_ROOT / ".git" / "hooks"


def install_hooks() -> int:
    if not GIT_HOOKS_DIR.exists():
        print(f"Git hooks directory not found at: {GIT_HOOKS_DIR}")
        return 1

    pre_commit_path = GIT_HOOKS_DIR / "pre-commit"
    pre_commit_content = """#!/bin/sh
# BMO Handheld Console — Pre-Commit AI Validation & Knowledge Sync Hook
echo "[Git Hook] Running AI Guardian 8-Phase CI Validation & Knowledge Sync..."
python scripts/validate_repo.py
if [ $? -ne 0 ]; then
    echo "[Git Hook] Pre-commit validation FAILED. Commit aborted."
    exit 1
fi
git add AGENT_KNOWLEDGE_GRAPH.json AGENT_DECISION_TREE.json AGENT_MANIFEST.json .agents/rules/CONTEXT_INDEX.json
echo "[Git Hook] Pre-commit validation PASSED."
exit 0
"""
    pre_commit_path.write_text(pre_commit_content, encoding="utf-8")

    commit_msg_path = GIT_HOOKS_DIR / "commit-msg"
    commit_msg_content = r"""#!/bin/sh
# BMO Handheld Console — Multi-Line Bullet Commit Message Gate (Rule 05)
MSG_FILE="$1"
python -c '
import sys, pathlib
lines = [l.rstrip() for l in pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="ignore").splitlines() if not l.strip().startswith("#")]
non_empty = [l for l in lines if l.strip()]
has_bullets = any(l.strip().startswith(("-", "*")) for l in non_empty[1:])
if len(non_empty) < 2 or not has_bullets:
    print("[Git Hook] ERROR: Commit message must include a summary line AND a bullet-point body (Rule 05).")
    print("  Example: git commit -m \"feat(scope): short summary\" -m \"- bullet 1\\n- bullet 2\\n- Verified: ...\"")
    sys.exit(1)
' "$MSG_FILE"
exit $?
"""
    commit_msg_path.write_text(commit_msg_content, encoding="utf-8")

    print(f"[OK] Installed pre-commit hook at: {pre_commit_path}")
    print(f"[OK] Installed commit-msg hook at: {commit_msg_path}")
    return 0


if __name__ == "__main__":
    sys.exit(install_hooks())
