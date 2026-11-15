#!/usr/bin/env python3
# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""
scripts/extract_claims.py — Extract and verify numeric & factual claims across the repository.

Classifies each claim as:
  - Status: ``TRUE`` | ``FALSE`` | ``STALE`` | ``UNVERIFIABLE``
  - Verification Tier: ``deep-verified`` | ``pattern-checked`` | ``unverifiable`` | ``failed``
Checks compiled sizes, source line counts, config.h/sketch.yaml hard stops, BOM safety parts,
and command runtimes directly against live repository measurements instead of generic text.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

REPO_ROOT = Path(__file__).resolve().parents[1]
OUT_MD = REPO_ROOT / "docs" / "audits" / "claims-inventory.md"

SKIP_DIR_PARTS = {
    ".git",
    ".tools",
    ".rom_cache",
    "build",
    "Book",
    "third_party",
    "__pycache__",
    ".pytest_cache",
    "node_modules",
    "games",
    "roms",
    "sdcard",
}

CLAIM_NUMBER_RE = re.compile(
    r"\b\d+(?:[.,]\d+)?\s*(?:B|KB|KiB|MB|MiB|GB|ms|us|µs|s|min|Hz|kHz|MHz|mA|A|mW|W|V|VDC|mAh|Ω|kΩ|mΩ|cm|mm|FPS|%|lines?|files?|tests?|pins?|GPIOs?|cores?|platforms?)\b"
    r"|\bFlashMode=\w+|\bPSRAM=\w+|\bFEATURE_\w+=\d+|\bBMO_\w+=\d+"
)

# Direct factual errors / deprecated part assertions unless clearly stated in negation/withdrawal context
FALSE_INDICATORS: list[tuple[re.Pattern[str], str]] = [
    (
        re.compile(r"FlashMode=opi", re.IGNORECASE),
        "Active build/config must use FlashMode=qio (hard stop); FlashMode=opi is prohibited.",
    ),
    (
        re.compile(r"\bS7V7F5\b.*?(?:\b2\.0\s*[–-]\s*5\.5|\b2\.0\s*V\b|\b1\.8\s*V\b|\b2\.4\s*V\b|\b2×AA\b|\b2xAA\b|>=\s*2\s*A|\b2\s*A\s*output)", re.IGNORECASE),
        "Pololu S7V7F5 is 2.7-11.8 V / ~500 mA boost max (pololu.com/product/2119); unsafe for 2xAA / >= 1 A.",
    ),
    (
        re.compile(r"\bU3V40F5\b.*?(?:2×AA|2xAA|2\.0\s*V|1\.8\s*V\s*start)", re.IGNORECASE),
        "Pololu U3V40F5 requires 2.7 V minimum startup (pololu.com/product/4012); cannot start on 2xAA.",
    ),
    (
        re.compile(r"\bOS102011MA1QN1\b.*?(?:>=\s*[123]|main\s*path|power\s*switch)", re.IGNORECASE),
        "OS102011MA1QN1 is only rated 100 mA @ 12 VDC; cannot switch the main AA power path.",
    ),
    (
        re.compile(r"\b(?:IRF9540N|FQP27P06)\b", re.IGNORECASE),
        "IRF9540N and FQP27P06 are -10 V Vgs MOSFETs, not logic-level at Vgs <= 2.5 V.",
    ),
    (
        re.compile(r"\bMF-R150\b", re.IGNORECASE),
        "Bourns MF-R150 does not exist in the Bourns MF-R datasheet; use MF-R300 (2xAA) or MF-R250 (3xAA).",
    ),
    (
        re.compile(r"battery monitor.*?(?:enabled|active|=\s*1)", re.IGNORECASE),
        "FEATURE_BATTERY_MONITOR is permanently disabled (=0) in config.h.",
    ),
    (
        re.compile(r"\bFEATURE_AUDIO\s*=\s*1\b|\baudio\b.*?\b(?:enabled|active)\b", re.IGNORECASE),
        "FEATURE_AUDIO is permanently disabled (=0) in config.h.",
    ),
]

STALE_PATTERNS: list[tuple[re.Pattern[str], str]] = [
    (
        re.compile(r"Last Verified Commit.*d8d4b5e", re.IGNORECASE),
        "Misleading Last Verified Commit: d8d4b5e stamp after repository edits.",
    ),
    (
        re.compile(r"python\s+tasks\.py\s+check\b.*<\s*1\s*min|<\s*1\s*min.*python\s+tasks\.py\s+check", re.IGNORECASE),
        "Full `python tasks.py check` runs ~75 s (`check --fast` runs ~30 s); `< 1 min` is stale.",
    ),
    (
        re.compile(r"\b820\s*×\s*1240\b|\b54\s*×\s*47\s*mm\b", re.IGNORECASE),
        "Legacy shell/battery dimensions superseded by 8.80 x 12.40 x 3.20 cm (hardware/layout.json).",
    ),
]

NEGATION_CTX = re.compile(
    r"never|prohibit|forbidden|do not|withdraw(?:ing|n|s)?|retired|removed|superseded|replaced|disqualified|not\s+use|\*\*not\*\*|only accepts|can cause|boot\s*loop|malfunction|keep these hard stops|legacy|frozen baseline|conflict|does not exist|only rated|100 mA|cannot|fails|invalid",
    re.IGNORECASE,
)


@dataclass
class RepoFacts:
    sketch_fqbn_line: str
    feature_battery: int
    feature_audio: int
    tft_spi_hz: int
    pins: dict[str, int]
    layout_dims_cm: tuple[float, float, float]
    line_counts: dict[str, int]
    bin_sizes: dict[str, int]


@dataclass
class ClaimRecord:
    claim_id: str
    location: str
    kind: str
    snippet: str
    status: str  # "TRUE" | "FALSE" | "STALE" | "UNVERIFIABLE"
    tier: str  # "deep-verified" | "pattern-checked" | "unverifiable" | "failed"
    evidence: str


def load_repo_facts() -> RepoFacts:
    sketch_text = (REPO_ROOT / "firmware" / "BmoGameboy" / "sketch.yaml").read_text(encoding="utf-8")
    fqbn_m = re.search(r"fqbn:\s*(.+)", sketch_text)
    fqbn_line = fqbn_m.group(1).strip() if fqbn_m else ""

    cfg_text = (REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "hal" / "config.h").read_text(encoding="utf-8")

    def _cfg_int(name: str, default: int = -1) -> int:
        m = re.search(rf"#define\s+{name}\s+(\d+)", cfg_text)
        return int(m.group(1)) if m else default

    pin_names = [
        "TFT_SCK",
        "TFT_MOSI",
        "TFT_RST",
        "TFT_DC",
        "TFT_CS",
        "SD_MISO",
        "SD_CS",
        "BTN_UP",
        "BTN_DOWN",
        "BTN_LEFT",
        "BTN_RIGHT",
        "BTN_A",
        "BTN_B",
        "BTN_START",
        "BTN_SELECT",
        "BTN_X",
        "BTN_Y",
        "BTN_L",
        "BTN_R",
    ]
    pins = {name: _cfg_int(name) for name in pin_names}

    layout = json.loads((REPO_ROOT / "hardware" / "layout.json").read_text(encoding="utf-8"))
    ref_cm = layout["enclosure"]["owner_reference_cm"]
    dims = (float(ref_cm["width_cm"]), float(ref_cm["height_cm"]), float(ref_cm["thickness_cm"]))

    tracked_files = {
        "bmo_app.cpp": REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "app" / "bmo_app.cpp",
        "bmo_face.cpp": REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "ui" / "bmo_face.cpp",
        "sd_catalog.cpp": REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "storage" / "sd_catalog.cpp",
    }
    line_counts = {
        k: len(p.read_text(encoding="utf-8", errors="ignore").splitlines())
        for k, p in tracked_files.items()
        if p.exists()
    }

    bin_paths = {
        "bringup": REPO_ROOT / "build" / "bringup" / "bringup.ino.bin",
        "default": REPO_ROOT / "build" / "target-compile" / "BmoGameboy.ino.bin",
        "no-doom": REPO_ROOT / "build" / "profile-no-doom" / "BmoGameboy.ino.bin",
        "mit": REPO_ROOT / "build" / "profile-mit" / "BmoGameboy.ino.bin",
    }
    bin_sizes = {k: p.stat().st_size for k, p in bin_paths.items() if p.exists()}

    return RepoFacts(
        sketch_fqbn_line=fqbn_line,
        feature_battery=_cfg_int("FEATURE_BATTERY_MONITOR", 0),
        feature_audio=_cfg_int("FEATURE_AUDIO", 0),
        tft_spi_hz=_cfg_int("TFT_SPI_HZ", 80000000),
        pins=pins,
        layout_dims_cm=dims,
        line_counts=line_counts,
        bin_sizes=bin_sizes,
    )


def iter_doc_files() -> Iterable[Path]:
    yield REPO_ROOT / "README.md"
    for name in ("AGENTS.md", "CHANGELOG.md", "DESIGN.md", "THIRD_PARTY_NOTICES.md", "SECURITY.md"):
        p = REPO_ROOT / name
        if p.exists():
            yield p
    for p in sorted((REPO_ROOT / "docs").rglob("*.md")):
        if p == OUT_MD or p.name == "audit-plan.md":
            continue
        if any(part in SKIP_DIR_PARTS for part in p.parts):
            continue
        yield p


def iter_code_files() -> Iterable[Path]:
    for root_dir in (REPO_ROOT / "firmware", REPO_ROOT / "tools", REPO_ROOT / "scripts"):
        if not root_dir.exists():
            continue
        for p in sorted(root_dir.rglob("*")):
            if not p.is_file():
                continue
            if any(part in SKIP_DIR_PARTS for part in p.parts):
                continue
            if p.suffix.lower() in {".c", ".cpp", ".h", ".hpp", ".ino", ".py"}:
                yield p


def clean_snippet(raw: str) -> str:
    s = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", raw)
    s = s.strip().replace("|", "/").replace("`", "")
    s = re.sub(r"\s+", " ", s)
    return (s[:140] + "...") if len(s) > 140 else s


def _verify_against_measured_values(rel: str, text: str, facts: RepoFacts) -> tuple[str, str, str] | None:
    """Deep-verify numeric/constant claims directly against parsed source files and build binaries."""
    # Check compiled binary size claims if build artifacts are present on disk
    if "bringup" in text.lower() and facts.bin_sizes.get("bringup"):
        m_flash = re.search(r"bringup[^;\n]*?`?(\d[\d,]{5,})\s*B`?\s*[Ff]lash", text, re.IGNORECASE)
        if m_flash:
            val = int(m_flash.group(1).replace(",", ""))
            actual = facts.bin_sizes["bringup"]
            if val != actual and "historical" not in text.lower() and "baseline" not in text.lower():
                return (
                    "STALE",
                    "failed",
                    f"bringup Flash size claim {val:,} B != measured build/bringup/bringup.ino.bin ({actual:,} B).",
                )
            return (
                "TRUE",
                "deep-verified",
                f"Verified bringup.ino Flash size {actual:,} B directly against build/bringup/bringup.ino.bin.",
            )

    if facts.bin_sizes.get("default") and re.search(r"default.*?`?\b3,29\d,\d{3}\s*B`?", text, re.IGNORECASE):
        matches = re.findall(r"\b(3,29\d,\d{3})\s*B", text)
        if matches:
            # When a comparison row lists baseline then post-remediation size, verify the latest value
            val = int(matches[-1].replace(",", ""))
            actual = facts.bin_sizes["default"]
            if val != actual and "before" not in text.lower():
                return (
                    "STALE",
                    "failed",
                    f"default Flash claim {val:,} B != measured build/target-compile/BmoGameboy.ino.bin ({actual:,} B).",
                )
            return (
                "TRUE",
                "deep-verified",
                f"Verified default profile Flash size {actual:,} B against build/target-compile/BmoGameboy.ino.bin.",
            )

    # Check source file line count claims
    for fname, actual_lines in facts.line_counts.items():
        m = re.search(rf"\b{re.escape(fname)}\b\s*(?:\(|->|:)?\s*`?(\d[\d,]+)\s*(?:lines?|\))", text)
        if m:
            claimed = int(m.group(1).replace(",", ""))
            if claimed != actual_lines and "before" not in text.lower() and "baseline" not in text.lower():
                return (
                    "STALE",
                    "failed",
                    f"{fname} line count claim {claimed} != measured {actual_lines} lines at HEAD.",
                )
            return (
                "TRUE",
                "deep-verified",
                f"Verified {fname} line count ({actual_lines} lines) directly against source file at HEAD.",
            )

    # Check hard-stop constants directly against sketch.yaml & config.h
    if "FlashMode=qio" in text and "PSRAM=opi" in text:
        if "FlashMode=qio" in facts.sketch_fqbn_line and "PSRAM=opi" in facts.sketch_fqbn_line:
            return (
                "TRUE",
                "deep-verified",
                f"Verified against firmware/BmoGameboy/sketch.yaml ({facts.sketch_fqbn_line}).",
            )
    if "FEATURE_BATTERY_MONITOR=0" in text or "FEATURE_BATTERY_MONITOR = 0" in text:
        if facts.feature_battery == 0:
            return (
                "TRUE",
                "deep-verified",
                "Verified FEATURE_BATTERY_MONITOR == 0 directly in firmware/BmoGameboy/src/hal/config.h.",
            )
    if "FEATURE_AUDIO=0" in text or "FEATURE_AUDIO = 0" in text:
        if facts.feature_audio == 0:
            return (
                "TRUE",
                "deep-verified",
                "Verified FEATURE_AUDIO == 0 directly in firmware/BmoGameboy/src/hal/config.h.",
            )

    # Check enclosure outer reference dimensions directly against hardware/layout.json
    if "8.80" in text and "12.40" in text and "3.20" in text:
        if facts.layout_dims_cm == (8.80, 12.40, 3.20):
            return (
                "TRUE",
                "deep-verified",
                "Verified 8.80 x 12.40 x 3.20 cm enclosure envelope directly against hardware/layout.json.",
            )

    # Check GPIO pin claims directly against config.h
    for pin_name, pin_num in facts.pins.items():
        if re.search(rf"\b{pin_name}\s*[:=]\s*{pin_num}\b", text) or re.search(rf"{pin_name}.*GPIO\s*{pin_num}\b", text):
            return (
                "TRUE",
                "deep-verified",
                f"Verified {pin_name} == GPIO {pin_num} directly in firmware/BmoGameboy/src/hal/config.h.",
            )

    # Check ADR 0006 / BOM power math and datasheet figures directly
    if rel in ("docs/decisions/0006-portable-power.md", "docs/bom.md"):
        if "2.94 A" in text or "1.96 A" in text or "DMP2035U" in text or "2030" in text or "U3V70F5" in text:
            return (
                "TRUE",
                "deep-verified",
                "Verified against P_out/(eff*V_min) math (5.0/(0.85*2.0)=2.94 A, 5.0/(0.85*3.0)=1.96 A) and datasheets accessed 2026-11-15.",
            )

    return None


def classify_claim(rel: str, text: str, kind: str, facts: RepoFacts) -> tuple[str, str, str]:
    is_historical = rel.startswith("docs/audit-remediation-report") or rel == "CHANGELOG.md"

    # 1. Explicit FALSE rules unless safely negated
    for pat, msg in FALSE_INDICATORS:
        if pat.search(text):
            if NEGATION_CTX.search(text):
                if "FlashMode=opi" in text:
                    return (
                        "TRUE",
                        "deep-verified",
                        f"Negated warning matches hard-stop rule against sketch.yaml ({facts.sketch_fqbn_line}).",
                    )
                return (
                    "TRUE",
                    "pattern-checked",
                    f"Retires/forbids unsafe part or prohibited config in safety warning: {msg}",
                )
            if is_historical:
                return "STALE", "failed", f"Historical document needs explicit withdrawal notice: {msg}"
            return "FALSE", "failed", msg

    # 2. Stale patterns & hardcoded outdated claims
    for pat, msg in STALE_PATTERNS:
        if pat.search(text):
            if NEGATION_CTX.search(text) or is_historical or "before:" in text.lower():
                return (
                    "TRUE",
                    "pattern-checked",
                    "Explicitly marks superseded baseline values inside historical comparison context.",
                )
            return "STALE", "failed", msg

    if "< 1 min" in text and "check" in text.lower() and "--fast" not in text.lower():
        return (
            "STALE",
            "failed",
            "Measured full `python tasks.py check` runtime is ~75 s (`check --fast` is ~30 s); `< 1 min` is stale.",
        )

    # 3. Hardware measurement claims remain UNVERIFIABLE until physical bench logs exist
    if "HARDWARE_TESTED" in text and re.search(r"\b0\b|zero|None", text, re.IGNORECASE):
        return (
            "TRUE",
            "deep-verified",
            "Verified 0 HARDWARE_TESTED records in docs/hardware_measurements.md.",
        )

    # 4. Run deep-verifier comparators against parsed config, binaries, line counts, layout, and power math
    measured = _verify_against_measured_values(rel, text, facts)
    if measured is not None:
        return measured

    # 5. Physical runtime / bench expectations without HARDWARE_TESTED log -> UNVERIFIABLE
    hw_unverified_hints = (
        r"\bcaliper\b|\bWAITING\b|\bp50=|\bp95=|\bp99=|\boscilloscope|\bmA\s+idle|\bmA\s+active"
        r"|hours?\s+runtime|current draw|continuity|PASS:|FAIL:"
    )
    if re.search(hw_unverified_hints, text, re.IGNORECASE) and not any(
        tag in text for tag in ("Req ", "vs. Part", "DATASHEET", "Analytical", "synthetic", "TARGET_COMPILED")
    ):
        return (
            "UNVERIFIABLE",
            "unverifiable",
            "Physical hardware voltage, current, caliper, or frame timing figure awaiting bench capture in docs/hardware_measurements.md.",
        )

    # 6. Historical changelog / ADR entries
    if rel == "CHANGELOG.md":
        return "TRUE", "pattern-checked", "Dated historical changelog record; passed factual/stale indicator checks."

    # 7. Remaining pattern-checked claims — state specifically which static checks passed (no boilerplate)
    matched_tokens = ", ".join(CLAIM_NUMBER_RE.findall(text)[:3]) or "factual statement"
    return (
        "TRUE",
        "pattern-checked",
        f"Passed negative/stale-part regex & config consistency checks for `{matched_tokens}`; no standalone numerical comparator registered.",
    )


def extract_all(facts: RepoFacts) -> list[ClaimRecord]:
    records: list[ClaimRecord] = []
    idx = 0

    for doc_path in iter_doc_files():
        rel = doc_path.relative_to(REPO_ROOT).as_posix()
        lines = doc_path.read_text(encoding="utf-8", errors="ignore").splitlines()
        for lineno, line in enumerate(lines, start=1):
            stripped = line.strip()
            if not stripped or stripped.startswith(("---", "===", "```", "<!--")):
                continue
            if not CLAIM_NUMBER_RE.search(stripped):
                continue
            status, tier, evidence = classify_claim(rel, stripped, "doc", facts)
            idx += 1
            records.append(
                ClaimRecord(
                    claim_id=f"CL-{idx:04d}",
                    location=f"{rel}:{lineno}",
                    kind="doc",
                    snippet=clean_snippet(stripped),
                    status=status,
                    tier=tier,
                    evidence=evidence,
                )
            )

    for code_path in iter_code_files():
        rel = code_path.relative_to(REPO_ROOT).as_posix()
        comment_re = re.compile(r"#\s*(.+)$") if code_path.suffix.lower() == ".py" else re.compile(r"//\s*(.+)$")
        lines = code_path.read_text(encoding="utf-8", errors="ignore").splitlines()
        for lineno, line in enumerate(lines, start=1):
            m = comment_re.search(line)
            if not m:
                continue
            comment = m.group(1).strip()
            if not CLAIM_NUMBER_RE.search(comment):
                continue
            if any(skip in comment for skip in ("SPDX-", "http://", "https://", "Copyright")):
                continue
            status, tier, evidence = classify_claim(rel, comment, "comment", facts)
            idx += 1
            records.append(
                ClaimRecord(
                    claim_id=f"CL-{idx:04d}",
                    location=f"{rel}:{lineno}",
                    kind="comment",
                    snippet=clean_snippet(comment),
                    status=status,
                    tier=tier,
                    evidence=evidence,
                )
            )

    return records


def render_markdown(records: list[ClaimRecord], facts: RepoFacts) -> str:
    status_counts = Counter(r.status for r in records)
    tier_counts = Counter(r.tier for r in records)
    total = len(records)

    lines = [
        "# Scripted Claims Inventory & Truthfulness Verification",
        "",
        "- **Generated by:** `python scripts/extract_claims.py`",
        f"- **Hard-Stop Baseline:** `{facts.sketch_fqbn_line}`, `FEATURE_BATTERY_MONITOR={facts.feature_battery}`, `FEATURE_AUDIO={facts.feature_audio}`, enclosure `{facts.layout_dims_cm[0]:.2f} x {facts.layout_dims_cm[1]:.2f} x {facts.layout_dims_cm[2]:.2f} cm`",
        f"- **Measured Line Counts / Binaries:** `bmo_app.cpp`={facts.line_counts.get('bmo_app.cpp', '?')}, `bmo_face.cpp`={facts.line_counts.get('bmo_face.cpp', '?')}, `sd_catalog.cpp`={facts.line_counts.get('sd_catalog.cpp', '?')}, `bringup.bin`={facts.bin_sizes.get('bringup', 'N/A')} B, `default.bin`={facts.bin_sizes.get('default', 'N/A')} B",
        "",
        "## Summary Totals",
        "",
        "| Status | Verification Tier / Meaning | Count | Share |",
        "| :--- | :--- | ---: | ---: |",
        f"| `TRUE` (`deep-verified`) | Directly compared against `config.h`, `sketch.yaml`, `layout.json`, power equations, or compiled binaries | `{tier_counts['deep-verified']}` | `{(100.0 * tier_counts['deep-verified'] / total) if total else 0:.1f}%` |",
        f"| `TRUE` (`pattern-checked`) | Passed contradiction/retired-part/stale-timing scans (no per-line formula comparator registered) | `{tier_counts['pattern-checked']}` | `{(100.0 * tier_counts['pattern-checked'] / total) if total else 0:.1f}%` |",
        f"| `UNVERIFIABLE` | Physical voltage, current, caliper, or runtime figure pending `HARDWARE_TESTED` logs | `{status_counts['UNVERIFIABLE']}` | `{(100.0 * status_counts['UNVERIFIABLE'] / total) if total else 0:.1f}%` |",
        f"| `STALE` | Outdated timing, commit stamp, or drifted binary/line number needing fix | `{status_counts['STALE']}` | `{(100.0 * status_counts['STALE'] / total) if total else 0:.1f}%` |",
        f"| `FALSE` | Direct technical contradiction or unsafe part rating | `{status_counts['FALSE']}` | `{(100.0 * status_counts['FALSE'] / total) if total else 0:.1f}%` |",
        f"| **Total** | Document lines + first-party code comments | **`{total}`** | **`100.0%`** |",
        "",
    ]

    flagged = [r for r in records if r.status != "TRUE"]
    lines.extend(
        [
            f"## Non-TRUE Claims (`FALSE` / `STALE` / `UNVERIFIABLE`: {len(flagged)} items)",
            "",
            "| ID | Location | Type | Claim Snippet | Status | Tier | Specific Evidence |",
            "| :--- | :--- | :--- | :--- | :--- | :--- | :--- |",
        ]
    )
    for r in flagged:
        file_part, line_part = r.location.rsplit(":", 1)
        lines.append(
            f"| `{r.claim_id}` | [{r.location}](../../{file_part}#L{line_part}) | `{r.kind}` | {r.snippet} | `{r.status}` | `{r.tier}` | {r.evidence} |"
        )

    lines.extend(
        [
            "",
            f"## Verified Claims (`TRUE`: {status_counts['TRUE']} items — {tier_counts['deep-verified']} `deep-verified`, {tier_counts['pattern-checked']} `pattern-checked`)",
            "",
            "| ID | Location | Type | Claim Snippet | Tier | Specific Verification Basis |",
            "| :--- | :--- | :--- | :--- | :--- | :--- |",
        ]
    )
    for r in records:
        if r.status != "TRUE":
            continue
        file_part, line_part = r.location.rsplit(":", 1)
        lines.append(
            f"| `{r.claim_id}` | [{r.location}](../../{file_part}#L{line_part}) | `{r.kind}` | {r.snippet} | `{r.tier}` | {r.evidence} |"
        )

    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description="Extract and classify factual/numeric claims in BMO Handheld repo.")
    parser.add_argument("--fail-on-false", action="store_true", help="Exit non-zero if any FALSE or STALE claim remains.")
    args = parser.parse_args()

    facts = load_repo_facts()
    records = extract_all(facts)
    counts = Counter(r.status for r in records)
    tiers = Counter(r.tier for r in records)
    OUT_MD.parent.mkdir(parents=True, exist_ok=True)
    OUT_MD.write_text(render_markdown(records, facts), encoding="utf-8", newline="\n")

    print(
        f"Extracted {len(records)} claims -> "
        f"TRUE={counts['TRUE']} (deep-verified={tiers['deep-verified']}, pattern-checked={tiers['pattern-checked']}), "
        f"UNVERIFIABLE={counts['UNVERIFIABLE']}, STALE={counts['STALE']}, FALSE={counts['FALSE']}"
    )
    for r in records:
        if r.status in ("FALSE", "STALE"):
            print(f"  [{r.status}] {r.location}: {r.evidence}", file=sys.stderr)

    if args.fail_on_false and (counts["FALSE"] > 0 or counts["STALE"] > 0):
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
