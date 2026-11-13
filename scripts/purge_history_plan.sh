#!/usr/bin/env bash
# Phase 1B safe history-purge runner using git-filter-repo in an isolated mirror.
# DO NOT run casually: rewriting public git history requires force-push coordination.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PATHS_FILE="$SCRIPT_DIR/purge_history_paths.txt"
BACKUP_DIR="${1:-$(dirname "$REPO_ROOT")/bmo-pre-purge-mirror.git}"
WORK_MIRROR="${2:-$(dirname "$REPO_ROOT")/bmo-cleaned-mirror.git}"
TARGET_BRANCH="${TARGET_BRANCH:-audit/tech-lead}"
PYTHON_BIN="${PYTHON_BIN:-python}"
if ! command -v "$PYTHON_BIN" >/dev/null 2>&1; then
  PYTHON_BIN="python3"
fi

if ! git filter-repo --version >/dev/null 2>&1; then
  echo "ERROR: git-filter-repo is not installed or not on PATH." >&2
  exit 1
fi

if [[ ! -f "$PATHS_FILE" ]]; then
  echo "ERROR: missing paths file: $PATHS_FILE" >&2
  exit 1
fi

cd "$REPO_ROOT"

if [[ -n "$(git status --porcelain --untracked-files=no)" ]]; then
  echo "ERROR: commit or stash tracked working-tree changes before running history purge." >&2
  exit 1
fi

if [[ -e "$BACKUP_DIR" ]]; then
  echo "ERROR: backup mirror already exists at '$BACKUP_DIR'. Move or pick a new path." >&2
  exit 1
fi

if [[ -e "$WORK_MIRROR" ]]; then
  echo "ERROR: output mirror already exists at '$WORK_MIRROR'. Move or pick a new path." >&2
  exit 1
fi

EXPECTED_TREE="$(git rev-parse "${TARGET_BRANCH}^{tree}")"
echo "==> Repo root:      $REPO_ROOT"
echo "==> Target branch:  $TARGET_BRANCH"
echo "==> Expected tree:  $EXPECTED_TREE"
echo "==> Baseline objects:"
git count-objects -vH

echo ""
echo "==> Step 1: Creating untouched pre-purge backup mirror at: $BACKUP_DIR"
git clone --mirror "$REPO_ROOT" "$BACKUP_DIR"

echo ""
echo "==> Step 2: Cloning backup mirror to isolated work mirror ($WORK_MIRROR) and running --analyze"
git clone --mirror "$BACKUP_DIR" "$WORK_MIRROR"
cd "$WORK_MIRROR"
git filter-repo --force --analyze
echo "Analyze summary written under: $WORK_MIRROR/filter-repo/analysis/"

echo ""
echo "==> Step 3: Pruning internal checkpoint refs and backup tag in work mirror"
while IFS= read -r ref; do
  [[ -n "$ref" ]] && git update-ref -d "$ref"
done < <(git for-each-ref --format='%(refname)' refs/agents/ refs/codex/ 2>/dev/null || true)

if git show-ref --verify --quiet refs/tags/pre-audit-2026-11; then
  git update-ref -d refs/tags/pre-audit-2026-11
fi

echo ""
echo "==> Step 4: Executing git filter-repo --invert-paths --paths-from-file (all branches & remaining tags)"
git filter-repo --force --invert-paths --paths-from-file "$PATHS_FILE"

echo ""
echo "==> Step 5: Compressing and verifying rewritten mirror"
git reflog expire --expire=now --all
git gc --prune=now --aggressive
git count-objects -vH

ACTUAL_TREE="$(git rev-parse "refs/heads/${TARGET_BRANCH}^{tree}")"
if [[ "$ACTUAL_TREE" != "$EXPECTED_TREE" ]]; then
  echo "ERROR: tree mismatch after purge! expected=$EXPECTED_TREE actual=$ACTUAL_TREE" >&2
  exit 1
fi
echo "Tree parity verified: $TARGET_BRANCH matches $EXPECTED_TREE byte-for-byte."

"$PYTHON_BIN" - <<'PY'
import subprocess, sys

p1 = subprocess.Popen(["git", "rev-list", "--objects", "--all"], stdout=subprocess.PIPE, text=True, encoding="utf-8", errors="replace")
p2 = subprocess.Popen(
    ["git", "cat-file", "--batch-check=%(objecttype)\t%(objectsize)\t%(objectname)\t%(rest)"],
    stdin=p1.stdout, stdout=subprocess.PIPE, text=True, encoding="utf-8", errors="replace"
)
p1.stdout.close()
out, _ = p2.communicate()

oversized = []
for line in out.splitlines():
    parts = line.split("\t", 3)
    if len(parts) >= 3 and parts[0] == "blob":
        sz = int(parts[1])
        if sz >= 1_048_576:
            oversized.append((sz, parts[2], parts[3] if len(parts) == 4 else ""))

if oversized:
    print("ERROR: oversized blobs (>= 1 MiB) remain:", file=sys.stderr)
    for row in oversized:
        print(f"  {row[0]} B  {row[1]}  {row[2]}", file=sys.stderr)
    sys.exit(1)
print("Verified: 0 blobs >= 1 MiB remain in rewritten history.")
PY

echo ""
echo "==> REWRITTEN MIRROR READY AND VERIFIED."
echo "    Pre-purge backup mirror (untouched): $BACKUP_DIR"
echo "    Cleaned rewritten mirror:            $WORK_MIRROR"
echo "    Live working tree untouched:         $REPO_ROOT"
echo ""
echo "    Next manual steps ONLY after final human review:"
echo "      1. Inspect log/branches/size inside $WORK_MIRROR"
echo "      2. Point cleaned mirror at the real remote and push intentionally:"
echo "         ORIGIN_URL=\"\$(git -C \"$REPO_ROOT\" remote get-url origin)\""
echo "         git -C \"$WORK_MIRROR\" remote add origin \"\$ORIGIN_URL\""
echo "         git -C \"$WORK_MIRROR\" push --force --all origin"
echo "         git -C \"$WORK_MIRROR\" push --force --tags origin"
echo "      3. Re-clone fresh working copy (preserving local gitignored games/, roms/, Book/, sdcard/),"
echo "         then run: git config core.hooksPath .githooks"
