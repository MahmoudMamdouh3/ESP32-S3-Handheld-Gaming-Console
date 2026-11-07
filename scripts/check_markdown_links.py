"""Run Guardian's local Markdown link and file-URI checks."""

from pathlib import Path
import sys

REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.guardian.core.doc_verifier import DocTruthVerifier


def main() -> int:
    issues = DocTruthVerifier(REPO_ROOT).verify_markdown_links()
    if issues:
        for issue in issues:
            print(f"FAIL {issue.file_path}:{issue.line_number}: {issue.message}")
        print(f"Markdown link check failed: {len(issues)} issue(s).")
        return 1
    print("Markdown link check passed: 0 issues.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
