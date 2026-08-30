#!/usr/bin/env python3
"""
bundle_rules.py - Consolidates all 34 topic-specific rule files from .agents/rules/
into a single, beautifully formatted, self-contained rules document for human reviewers.
"""

from pathlib import Path
import re

REPO_ROOT = Path(__file__).resolve().parents[1]
RULES_DIR = REPO_ROOT / '.agents' / 'rules'


def generate_consolidated_rules(output_path: Path) -> Path:
    rule_files = sorted([f for f in RULES_DIR.glob('*.md') if f.name != 'README.md'])

    header = """# BMO Handheld Gaming Console — Consolidated Ruleset (v4)
**Target Platform:** ESP32-S3-N16R8 (Dual-Core LX7 @ 240MHz, 16MB OPI Flash, 8MB Octal PSRAM)  
**Display:** ST7789VW 240×320 SPI TFT (Landscape 320×240)  
**Document Status:** Complete Consolidated Ground Truth (Rules 00 through 34)  
**Purpose:** Single-file compilation for external architectural and code review.

---

## Table of Contents
"""

    toc_entries = []
    sections = []

    for file in rule_files:
        content = file.read_text(encoding='utf-8').strip()
        # Find first header
        first_line = content.split('\n')[0]
        title_match = re.search(r'^#+\s*(.+)', first_line)
        title = title_match.group(1) if title_match else file.stem

        anchor = re.sub(r'[^a-zA-Z0-9_-]+', '', title.lower().replace(' ', '-'))
        toc_entries.append(f"- [{title}](#{anchor})")

        sections.append(f"\n\n---\n\n<!-- Section: {file.name} -->\n\n{content}")

    full_document = header + '\n'.join(toc_entries) + '\n' + ''.join(sections) + '\n'
    
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(full_document, encoding='utf-8')
    return output_path


if __name__ == '__main__':
    import sys
    target = Path(sys.argv[1]) if len(sys.argv) > 1 else (REPO_ROOT / 'docs' / 'consolidated-rules.md')
    out = generate_consolidated_rules(target)
    print(f"Consolidated rules successfully written to: {out} ({out.stat().st_size} bytes)")
