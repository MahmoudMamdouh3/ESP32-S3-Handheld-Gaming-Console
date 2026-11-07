"""Generate a compact symbol index from firmware public headers."""

from __future__ import annotations

from pathlib import Path


MAX_PAGE_CHARS = 10_800
SYMBOL_ROOT = Path("docs/generated/symbols")


class SymbolsGenerator:
    def __init__(self, repo_root: Path):
        self.repo_root = repo_root

    def _header_records(self, header: Path) -> list[str]:
        rel = header.relative_to(self.repo_root).as_posix()
        signatures: list[str] = []
        try:
            lines = header.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError:
            return []
        for raw in lines:
            line = raw.strip()
            if not line or line.startswith(("//", "/*", "*", "#include", "#pragma", "#if", "#else", "#endif")):
                continue
            if line.startswith("#define ") and line.split(maxsplit=2)[1].startswith(("FEATURE_", "TFT_", "SD_", "BTN_")):
                signatures.append(f"- `{line}`")
            elif line.startswith(("namespace ", "class ", "struct ", "enum ")):
                signatures.append(f"- `{line.rstrip('{').strip()}`")
            elif line.endswith(";") and "(" in line and not line.startswith(("typedef ", "if ", "while ")):
                signatures.append(f"- `{line}`")
        if not signatures:
            return []
        return [f"## `{rel}`", "", *signatures, ""]

    def _pages(self) -> dict[str, list[Path]]:
        firmware = self.repo_root / "firmware" / "BmoGameboy" / "src"
        return {
            "core": sorted((firmware / "core").glob("*.h")),
            "emulators": sorted((firmware / "emulators").glob("*.h")),
            "engine": sorted((firmware / "engine").glob("*"))
            and sorted(path for path in (firmware / "engine").rglob("*.h") if path.is_file()),
        }

    @staticmethod
    def _pack_pages(records: list[list[str]]) -> list[str]:
        bounded_records: list[list[str]] = []
        for record in records:
            if len("\n".join(record)) <= MAX_PAGE_CHARS:
                bounded_records.append(record)
                continue
            header = record[:2]
            current = header[:]
            for declaration in record[2:]:
                candidate = "\n".join([*current, declaration])
                if len(candidate) > MAX_PAGE_CHARS:
                    bounded_records.append(current + [""])
                    current = [header[0] + " (continued)", "", declaration]
                else:
                    current.append(declaration)
            if len(current) > len(header):
                bounded_records.append(current)

        pages: list[str] = []
        current = ""
        for record in bounded_records:
            block = "\n".join(record) + "\n"
            if current and len(current) + len(block) > MAX_PAGE_CHARS:
                pages.append(current.rstrip() + "\n")
                current = ""
            if len(block) > MAX_PAGE_CHARS:
                raise ValueError("A single symbol declaration exceeds the page size limit")
            current += block
        if current:
            pages.append(current.rstrip() + "\n")
        return pages

    def generate(self) -> list[Path]:
        target_dir = self.repo_root / SYMBOL_ROOT
        target_dir.mkdir(parents=True, exist_ok=True)
        for stale in target_dir.glob("*.md"):
            stale.unlink()

        pages_written: list[Path] = []
        page_links: list[str] = []
        for family, headers in self._pages().items():
            records = [record for header in headers if (record := self._header_records(header))]
            pages = self._pack_pages(records)
            for index, content in enumerate(pages, start=1):
                filename = f"{family}-{index:02d}.md"
                path = target_dir / filename
                path.write_text(
                    f"# Firmware symbols: {family} ({index}/{len(pages)})\n\n"
                    "Generated from the declarations in the listed headers. Edit the header, then run "
                    "`python -m tools.guardian symbols`.\n\n"
                    + content,
                    encoding="utf-8",
                )
                if len(path.read_text(encoding="utf-8")) > 12_000:
                    raise ValueError(f"Generated symbol page exceeds 12,000 characters: {path}")
                pages_written.append(path)
                page_links.append(f"- [{family} declarations, part {index}](symbols/{filename})")

        index = self.repo_root / "docs/generated/symbols.md"
        index.parent.mkdir(parents=True, exist_ok=True)
        index.write_text(
            "# Firmware symbol index\n\n"
            "Header declarations are the source of truth. This generated index groups the current public "
            "firmware declarations by subsystem.\n\n"
            + "\n".join(page_links)
            + "\n\nRegenerate with `python -m tools.guardian symbols`.\n",
            encoding="utf-8",
        )
        pages_written.append(index)
        return pages_written
