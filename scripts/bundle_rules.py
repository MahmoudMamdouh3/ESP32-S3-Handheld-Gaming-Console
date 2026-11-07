#!/usr/bin/env python3
"""Build a compact, ordered index and bounded pages for the agent rules."""

from __future__ import annotations

from pathlib import Path
import posixpath
import re
from urllib.parse import unquote, urlsplit, urlunsplit

REPO_ROOT = Path(__file__).resolve().parents[1]
RULES_DIR = REPO_ROOT / ".agents" / "rules"
OUTPUT_DIR = REPO_ROOT / "docs" / "consolidated-rules"
INDEX_PATH = REPO_ROOT / "docs" / "consolidated-rules.md"
MAX_PAGE_CHARS = 10_800
LOCAL_LINK_RE = re.compile(r"\]\(([^)\s]+)")


def _rewrite_rule_links(text: str, source_path: Path) -> str:
    """Keep source links valid when rule text is copied under docs/."""
    output_dir_from_root = "docs/consolidated-rules"

    def rewrite(match: re.Match[str]) -> str:
        href = match.group(1)
        parsed = urlsplit(href)
        if parsed.scheme or href.startswith("//") or not parsed.path:
            return match.group(0)
        source_target = (source_path.parent / unquote(parsed.path)).resolve()
        try:
            target_from_root = source_target.relative_to(REPO_ROOT.resolve()).as_posix()
        except ValueError:
            return match.group(0)
        output_href = posixpath.relpath(target_from_root, output_dir_from_root)
        output_href = urlunsplit(("", "", output_href, parsed.query, parsed.fragment))
        return "](" + output_href

    return LOCAL_LINK_RE.sub(rewrite, text)


def _rule_version() -> str:
    meta = (RULES_DIR / "11_rules_meta.md").read_text(encoding="utf-8")
    match = re.search(r"RULESET_VERSION:\s*(\d+)", meta)
    if not match:
        raise ValueError("11_rules_meta.md must define RULESET_VERSION")
    return match.group(1)


def _paragraph_chunks(text: str, limit: int) -> list[str]:
    chunks: list[str] = []
    current = ""
    for paragraph in re.split(r"(?<=\n)\s*\n", text):
        if current and len(current) + len(paragraph) + 1 > limit:
            chunks.append(current.rstrip() + "\n")
            current = ""
        if len(paragraph) > limit:
            lines = paragraph.splitlines(keepends=True)
            for line in lines:
                if current and len(current) + len(line) > limit:
                    chunks.append(current.rstrip() + "\n")
                    current = ""
                current += line
        else:
            current += paragraph + "\n\n"
    if current:
        chunks.append(current.rstrip() + "\n")
    return chunks


def _split_rule(text: str) -> list[str]:
    body_limit = MAX_PAGE_CHARS - 220
    if len(text) <= body_limit:
        return [text.rstrip() + "\n"]
    sections = re.split(r"(?=^#{2,3} )", text, flags=re.MULTILINE)
    chunks: list[str] = []
    current = ""
    for section in sections:
        if len(section) > body_limit:
            if current:
                chunks.append(current.rstrip() + "\n")
                current = ""
            chunks.extend(_paragraph_chunks(section, body_limit))
        elif current and len(current) + len(section) > body_limit:
            chunks.append(current.rstrip() + "\n")
            current = section
        else:
            current += section
    if current:
        chunks.append(current.rstrip() + "\n")
    return chunks


def generate_consolidated_rules(output_path: Path = INDEX_PATH) -> Path:
    rule_files = sorted(p for p in RULES_DIR.glob("*.md") if p.name != "README.md")
    version = _rule_version()
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    for stale in OUTPUT_DIR.glob("rule-*.md"):
        stale.unlink()

    rows: list[str] = []
    for number, rule_file in enumerate(rule_files):
        source = rule_file.read_text(encoding="utf-8").strip()
        title_match = re.search(r"(?m)^#+\s*(.+)$", source)
        title = title_match.group(1).strip() if title_match else rule_file.stem
        pages = _split_rule(source)
        page_links: list[str] = []
        for part, content in enumerate(pages, start=1):
            filename = f"rule-{rule_file.stem}-{part:02d}.md"
            target = OUTPUT_DIR / filename
            page_heading = f"# Section {rule_file.stem[:2]} — {title}"
            if len(pages) > 1:
                page_heading += f" ({part}/{len(pages)})"
            source_link = f"../../.agents/rules/{rule_file.name}"
            content = _rewrite_rule_links(content, rule_file)
            content = "\n".join(line.rstrip() for line in content.splitlines()).rstrip()
            target.write_text(f"{page_heading}\n\nSource: [`{rule_file.name}`]({source_link}).\n\n{content}", encoding="utf-8")
            if len(target.read_text(encoding="utf-8")) >= 12_000:
                raise ValueError(f"Rule bundle page exceeds 12,000 characters: {target}")
            page_links.append(f"[part {part}]({OUTPUT_DIR.name}/{filename})" if len(pages) > 1 else f"[{rule_file.name}]({OUTPUT_DIR.name}/{filename})")
        rows.append(f"- **Section {rule_file.stem[:2]} — {title}**: " + ", ".join(page_links))

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output = (
        f"# BMO Handheld Gaming Console — Consolidated Ruleset (v{version})\n\n"
        f"**Rule files:** {len(rule_files)}. Sections follow rule filenames in numeric order.\n\n"
        "This index links to bounded pages generated from `.agents/rules/`. Edit the source rule and rerun `python scripts/bundle_rules.py`.\n\n"
        "## Table of Contents\n\n" + "\n".join(rows) + "\n"
    )
    if len(output) >= 12_000:
        raise ValueError("Consolidated rules index exceeds 12,000 characters")
    output_path.write_text(output, encoding="utf-8")
    return output_path


if __name__ == "__main__":
    import sys

    target = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else INDEX_PATH
    out = generate_consolidated_rules(target)
    print(f"Consolidated rules index written to: {out} ({len(out.read_text(encoding='utf-8'))} characters)")
