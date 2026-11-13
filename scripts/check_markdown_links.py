"""Verify local Markdown links and reject file:// URIs."""

import re
from pathlib import Path
from urllib.parse import unquote, urlsplit

REPO_ROOT = Path(__file__).resolve().parents[1]
MARKDOWN_LINK_REGEX = re.compile(r"\[[^\]]*\]\(([^)\s]+)(?:\s+[^)]*)?\)")
IGNORED_DIRS = {
    ".git", ".tools", "build", "node_modules", "__pycache__", "venv",
    "games", "roms", "sdcard", "Book",
}


def find_markdown_link_issues(repo_root: Path) -> list[tuple[str, int, str, str]]:
    issues: list[tuple[str, int, str, str]] = []
    root_resolved = repo_root.resolve()
    for path in sorted(repo_root.rglob("*.md")):
        relative = path.relative_to(repo_root)
        if any(part in IGNORED_DIRS for part in relative.parts):
            continue
        rel_str = relative.as_posix()
        in_fence_char = ""
        in_fence_len = 0
        for line_number, line in enumerate(
            path.read_text(encoding="utf-8", errors="ignore").splitlines(), 1
        ):
            stripped = line.lstrip(" ")
            if len(line) - len(stripped) <= 3:
                if not in_fence_char:
                    m_open = re.match(r"^(`{3,}|~{3,})(.*)$", stripped)
                    if m_open:
                        fence = m_open.group(1)
                        info = m_open.group(2)
                        if not (fence[0] == "`" and "`" in info):
                            in_fence_char = fence[0]
                            in_fence_len = len(fence)
                            continue
                elif re.match(rf"^{re.escape(in_fence_char)}{{{in_fence_len},}}[ \t]*$", stripped):
                    in_fence_char = ""
                    in_fence_len = 0
                    continue
            if in_fence_char:
                continue
            for match in MARKDOWN_LINK_REGEX.finditer(line):
                href = match.group(1).strip().strip("<>")
                if href.lower().startswith("file:"):
                    issues.append(
                        (rel_str, line_number, "FILE_URI_LINK",
                         "Uses a file:// URI; replace with a repository-relative link")
                    )
                    continue
                parsed = urlsplit(href)
                if parsed.scheme or href.startswith("//") or not parsed.path:
                    continue
                target_text = unquote(parsed.path).replace("\\", "/")
                target = (path.parent / target_text).resolve()
                try:
                    target.relative_to(root_resolved)
                except ValueError:
                    issues.append(
                        (rel_str, line_number, "UNRESOLVED_LINK",
                         f"Relative link `{href}` resolves outside the repository")
                    )
                    continue
                if not target.exists():
                    issues.append(
                        (rel_str, line_number, "UNRESOLVED_LINK",
                         f"Relative link target does not exist: `{href}`")
                    )
    return issues


def main() -> int:
    issues = find_markdown_link_issues(REPO_ROOT)
    if issues:
        for file_path, line_number, category, message in issues:
            print(f"FAIL {file_path}:{line_number} [{category}]: {message}")
        print(f"Markdown link check failed: {len(issues)} issue(s).")
        return 1
    print("Markdown link check passed: 0 issues.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
