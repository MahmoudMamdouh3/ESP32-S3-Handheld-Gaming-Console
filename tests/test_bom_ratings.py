#!/usr/bin/env python3
# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""Validate every BOM component has an MPN/manufacturer datasheet URL and meets worst-case electrical ratings."""

from __future__ import annotations

import re
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
BOM_PATH = REPO_ROOT / "docs" / "bom.md"

BANNED_UNSAFE_PARTS = (
    "S7V7F5",
    "IRF9540N",
    "FQP27P06",
    "TPS61023",
    "EVQ-P1D05M",
    "2.80 V rising",
    "2.40 V falling",
)

BANNED_ACTIVE_SPECS = (
    "OS102011MA1QN1",
    "MF-R150",
    "1N5819",
)

# Every BOM row's datasheet URL must identify its MPN or OEM/vendor datasheet path (never an unrelated part/retail page).
EXPECTED_URL_TOKENS: dict[str, tuple[str, ...]] = {
    "U1": ("espressif.com", "devkitc-1"),
    "DISP1": ("st7789",),
    "SD1": ("adafruit.com", "4682"),
    "SW_DPAD": ("omron", "b3f"),
    "SW_AB": ("omron", "b3f"),
    "SW_SYS": ("omron", "b3f"),
    "SW_LR": ("omron", "b3f"),
    "R_CS1, R_CS2": ("yageo.com", "mfr"),
    "R_SCK, R_MOSI": ("yageo.com", "mfr"),
    "C_DEC1, C_DEC2": ("murata.com", "rder7"),
    "PCB1": ("busboard.com", "pad3u"),
    "BAT1": ("eneloop", "bk-3mccf"),
    "BH1": ("2462",),
    "F1": ("bourns.com", "mfr"),
    "SW_PWR": ("littelfuse.com", "ck-slides"),
    "Q1": ("diodes.com", "dmp2035u"),
    "U2A": ("adafruit.com", "2030"),
    "U2B": ("ti.com", "tps61022"),
    "U2C": ("pololu.com", "4012"),
    "U2D": ("pololu.com", "2891"),
    "D1": ("st.com", "1n582x"),
    "R_BAT1, R_BAT2, C_BAT": ("yageo.com", "mfr"),
}


def parse_bom_tables(markdown: str) -> list[dict[str, str]]:
    """Parse all BOM Markdown tables into normalized row dicts."""
    rows: list[dict[str, str]] = []
    headers: list[str] | None = None
    for raw_line in markdown.splitlines():
        line = raw_line.strip()
        if not line.startswith("|"):
            headers = None
            continue
        cells = [cell.strip() for cell in line.strip("|").split("|")]
        if headers is None:
            headers = cells
            continue
        if all(set(cell) <= set(":- ") for cell in cells):
            continue
        if len(cells) != len(headers):
            raise AssertionError(f"Malformed BOM table row: {line}")
        row = dict(zip(headers, cells, strict=True))
        row["Ref"] = row["Ref"].replace("*", "").strip()
        rows.append(row)
    return rows


def _extract_first_float(pattern: str, text: str, label: str) -> float:
    match = re.search(pattern, text)
    if not match:
        raise AssertionError(f"Missing {label} matching {pattern!r} in: {text!r}")
    return float(match.group(1))


def _get_url_column(row: dict[str, str]) -> str:
    for key, val in row.items():
        if key.startswith("Datasheet URL"):
            return val
    return ""


def _validate_core_bom_row(ref: str, url_lower: str, req_text: str, part_text: str) -> None:
    if ref == "U1":
        req_ma = _extract_first_float(r">=\s*(\d+(?:\.\d+)?)\s*mA", req_text, "U1 req mA")
        part_ma = _extract_first_float(r"(\d+(?:\.\d+)?)\s*(?:–\d+(?:\.\d+)?)?\s*mA", part_text, "U1 part mA")
        if part_ma < req_ma:
            raise AssertionError(f"{ref}: LDO rating {part_ma} mA < req {req_ma} mA")
    elif ref == "DISP1":
        if "adafruit.com/product/4311" in url_lower:
            raise AssertionError(f"{ref}: must cite Sitronix ST7789 datasheet, not Adafruit retail page")
        req_v = _extract_first_float(r"(\d+(?:\.\d+)?)\s*V", req_text, "DISP1 req V")
        max_io_v = _extract_first_float(r"\d+(?:\.\d+)?–(\d+(?:\.\d+)?)\s*V\s*IO", part_text, "DISP1 part IO V")
        if max_io_v < req_v:
            raise AssertionError(f"{ref}: IO voltage {max_io_v} V < req {req_v} V")
    elif ref == "SD1":
        miso_limit = _extract_first_float(r"MISO\s*<=\s*(\d+(?:\.\d+)?)\s*V", req_text, "SD1 MISO max V")
        if miso_limit > 3.35:
            raise AssertionError(f"{ref}: MISO limit {miso_limit} V exceeds 3.35 V ESP32 GPIO ceiling")
        if "3.3 V-only" not in part_text:
            raise AssertionError(f"{ref}: SD1 part must be documented as 3.3 V-only")
    elif ref == "PCB1":
        if "2462" in url_lower:
            raise AssertionError(f"{ref}: perfboard URL must not point to battery holder datasheet 2462")
        if "0.254 cm" not in req_text or "FR-4" not in part_text:
            raise AssertionError(f"{ref}: missing 0.254 cm grid or FR-4 rating")
    else:
        raise AssertionError(f"Unhandled BOM Ref {ref!r} — add explicit rating validation rule")


def _validate_passive_or_switch_row(ref: str, req_text: str, part_text: str) -> None:
    if ref in ("SW_DPAD", "SW_AB", "SW_SYS", "SW_LR"):
        req_ma = _extract_first_float(r"<=\s*(\d+(?:\.\d+)?)\s*mA", req_text, f"{ref} req mA")
        part_ma = _extract_first_float(r"(\d+(?:\.\d+)?)\s*mA", part_text, f"{ref} part mA")
        part_v = _extract_first_float(r"(\d+(?:\.\d+)?)\s*VDC", part_text, f"{ref} part VDC")
        if part_ma < req_ma or part_v < 3.3:
            raise AssertionError(f"{ref}: tactile switch rating {part_ma} mA @ {part_v} VDC insufficient")
    elif ref in ("R_CS1, R_CS2", "R_SCK, R_MOSI"):
        req_mw = _extract_first_float(r"(\d+(?:\.\d+)?)\s*mW", req_text, f"{ref} req mW")
        part_mw = _extract_first_float(r"(\d+(?:\.\d+)?)\s*mW", part_text, f"{ref} part mW")
        part_v = _extract_first_float(r"(\d+(?:\.\d+)?)\s*V\s*max", part_text, f"{ref} part V")
        if part_mw < req_mw or part_v < 3.3:
            raise AssertionError(f"{ref}: resistor rating {part_mw} mW / {part_v} V < req {req_mw} mW / 3.3 V")
    elif ref == "C_DEC1, C_DEC2":
        req_v = _extract_first_float(r"(\d+(?:\.\d+)?)\s*V\s*DC", req_text, "C_DEC req V")
        part_v = _extract_first_float(r"(\d+(?:\.\d+)?)(?:–\d+(?:\.\d+)?)?\s*V\s*DC", part_text, "C_DEC part V")
        if part_v < req_v * 2:
            raise AssertionError(f"{ref}: capacitor voltage {part_v} V needs >= 2x {req_v} V margin")
    elif ref == "R_BAT1, R_BAT2, C_BAT":
        req_mw = _extract_first_float(r"<=\s*(\d+(?:\.\d+)?)\s*mW", req_text, "R_BAT req mW")
        part_mw = _extract_first_float(r"(\d+(?:\.\d+)?)\s*mW", part_text, "R_BAT part mW")
        adc_v = _extract_first_float(r"ADC output\s*\d+(?:\.\d+)?–(\d+(?:\.\d+)?)\s*V", part_text, "ADC max V")
        if part_mw < req_mw or adc_v >= 3.3:
            raise AssertionError(f"{ref}: divider margin invalid ({part_mw} mW vs {req_mw} mW, ADC {adc_v} V)")


def _validate_power_path_row(ref: str, req_text: str, part_text: str) -> None:
    if ref == "BAT1":
        req_a = _extract_first_float(r">=\s*(\d+(?:\.\d+)?)\s*A\s*\(2xAA\)", req_text, "BAT1 2xAA req A")
        part_a = _extract_first_float(r">=\s*(\d+(?:\.\d+)?)\s*A\s*NiMH", part_text, "BAT1 discharge A")
        if part_a < req_a:
            raise AssertionError(f"{ref}: cell discharge {part_a} A < req {req_a} A")
    elif ref == "BH1":
        req_a = _extract_first_float(r">=\s*(\d+(?:\.\d+)?)\s*A", req_text, "BH1 req A")
        part_a = _extract_first_float(r">=\s*(\d+(?:\.\d+)?)\s*A", part_text, "BH1 part A")
        if part_a < req_a:
            raise AssertionError(f"{ref}: holder current {part_a} A < req {req_a} A")
    elif ref == "F1":
        req_2x = _extract_first_float(r"I_hold\s*>=\s*(\d+(?:\.\d+)?)\s*A\s*\(2xAA\)", req_text, "F1 2xAA req A")
        req_3x = _extract_first_float(r">=\s*(\d+(?:\.\d+)?)\s*A\s*\(3xAA\)", req_text, "F1 3xAA req A")
        hold_2x = _extract_first_float(r"(\d+(?:\.\d+)?)\s*A\s*hold\s*/\s*\d+(?:\.\d+)?\s*A\s*trip.*MF-R300", part_text, "MF-R300 hold A")
        hold_3x = _extract_first_float(r"(\d+(?:\.\d+)?)\s*A\s*/\s*\d+(?:\.\d+)?\s*A\s*\(MF-R250\)", part_text, "MF-R250 hold A")
        if hold_2x < req_2x or hold_3x < req_3x:
            raise AssertionError(f"{ref}: fuse hold ({hold_2x} A / {hold_3x} A) < req ({req_2x} A / {req_3x} A)")
    elif ref == "SW_PWR":
        req_a = _extract_first_float(r">=\s*(\d+(?:\.\d+)?)\s*A\s*\(2xAA\)", req_text, "SW_PWR req A")
        ratings = re.findall(r"(\d+(?:\.\d+)?)\s*A\s*@\s*(\d+(?:\.\d+)?)\s*VDC", part_text)
        if not ratings:
            raise AssertionError(f"{ref}: no DC switch current/voltage ratings found in {part_text!r}")
        for amps_s, volts_s in ratings:
            amps, volts = float(amps_s), float(volts_s)
            if amps < req_a or amps < 2.0 or volts < 5.0:
                raise AssertionError(f"{ref}: switch rating {amps} A @ {volts} VDC below req {req_a} A (>= 2 A @ >= 5 V)")


def _validate_regulator_and_diode_row(ref: str, req_text: str, part_text: str) -> None:
    if ref == "Q1":
        req_a = _extract_first_float(r">=\s*(\d+(?:\.\d+)?)\s*A\s*\(2xAA\)", req_text, "Q1 req A")
        part_a = _extract_first_float(r"-(\d+(?:\.\d+)?)\s*A", part_text, "Q1 Id max A")
        vgs_abs = _extract_first_float(r"@\s*-?(\d+(?:\.\d+)?)\s*V", part_text, "Q1 Rds(on) Vgs")
        if part_a < req_a:
            raise AssertionError(f"{ref}: MOSFET Id {part_a} A < req {req_a} A")
        if vgs_abs > 2.5:
            raise AssertionError(f"{ref}: MOSFET Rds(on) must be specified at |Vgs| <= 2.5 V, got {vgs_abs} V")
    elif ref in ("U2A", "U2B"):
        req_start = _extract_first_float(r"start\s*<=\s*(\d+(?:\.\d+)?)\s*V", req_text, f"{ref} req start V")
        req_out = _extract_first_float(r"I_out\s*>=\s*(\d+(?:\.\d+)?)\s*A", req_text, f"{ref} req I_out A")
        part_start = _extract_first_float(r"(\d+(?:\.\d+)?)\s*V\s*start", part_text, f"{ref} part start V")
        part_out = _extract_first_float(r">=\s*(\d+(?:\.\d+)?)\s*A\s*@\s*2\.0\s*V", part_text, f"{ref} part I_out @ 2.0 V")
        if part_start > req_start:
            raise AssertionError(f"{ref}: startup {part_start} V > 2xAA cutoff start limit {req_start} V")
        if part_out < req_out:
            raise AssertionError(f"{ref}: output {part_out} A < req {req_out} A")
    elif ref in ("U2C", "U2D"):
        req_start = _extract_first_float(r"start\s*<\s*(\d+(?:\.\d+)?)\s*V", req_text, f"{ref} req start V")
        req_out = _extract_first_float(r"I_out\s*>=\s*(\d+(?:\.\d+)?)\s*A", req_text, f"{ref} req I_out A")
        part_start = _extract_first_float(r"(\d+(?:\.\d+)?)\s*V\s*(?:start|min input)", part_text, f"{ref} part min V")
        part_out = _extract_first_float(r"(\d+(?:\.\d+)?)\s*A\+?\s*@\s*3\.0\s*V", part_text, f"{ref} part I_out @ 3.0 V")
        if part_start >= req_start:
            raise AssertionError(f"{ref}: min input {part_start} V >= 3xAA cutoff {req_start} V")
        if part_out < req_out:
            raise AssertionError(f"{ref}: output {part_out} A < req {req_out} A")
    elif ref == "D1":
        req_a = _extract_first_float(r">=\s*(\d+(?:\.\d+)?)\s*A", req_text, "D1 req A")
        d1_a = _extract_first_float(r"1N5822\s*(\d+(?:\.\d+)?)\s*A", part_text, "1N5822 A")
        if d1_a < req_a:
            raise AssertionError(f"{ref}: diode rating {d1_a} A < req {req_a} A")


def validate_bom_row(row: dict[str, str]) -> None:
    """Assert MPN/manufacturer datasheet URL and rating >= worst-case requirement for a single BOM row."""
    ref = row["Ref"]
    spec_col = row.get("Vetted Part / Spec") or row.get("Vetted Part / Options") or ""
    margin_col = row.get("Worst-Case Requirement vs. Part Rating", "")
    url_col = _get_url_column(row)

    if not url_col.startswith("https://"):
        raise AssertionError(f"{ref}: invalid or missing datasheet URL: {url_col!r}")
    expected_tokens = EXPECTED_URL_TOKENS.get(ref)
    if expected_tokens is None:
        raise AssertionError(f"{ref}: missing EXPECTED_URL_TOKENS entry in tests/test_bom_ratings.py")
    url_lower = url_col.lower()
    for token in expected_tokens:
        if token not in url_lower:
            raise AssertionError(f"{ref}: datasheet URL {url_col!r} must include MPN/manufacturer token {token!r}")

    if " vs. Part " not in margin_col:
        raise AssertionError(f"{ref}: must state 'Req ... vs. Part ...' in margin column, got: {margin_col!r}")

    for banned in (*BANNED_UNSAFE_PARTS, *BANNED_ACTIVE_SPECS):
        if banned in spec_col or banned in margin_col:
            raise AssertionError(f"{ref}: banned/retired part {banned!r} found in active BOM spec/rating")

    req_text, part_text = margin_col.split(" vs. Part ", 1)
    if ref in ("SW_DPAD", "SW_AB", "SW_SYS", "SW_LR", "R_CS1, R_CS2", "R_SCK, R_MOSI", "C_DEC1, C_DEC2", "R_BAT1, R_BAT2, C_BAT"):
        _validate_passive_or_switch_row(ref, req_text, part_text)
    elif ref in ("BAT1", "BH1", "F1", "SW_PWR"):
        _validate_power_path_row(ref, req_text, part_text)
    elif ref in ("Q1", "U2A", "U2B", "U2C", "U2D", "D1"):
        _validate_regulator_and_diode_row(ref, req_text, part_text)
    else:
        _validate_core_bom_row(ref, url_lower, req_text, part_text)


class BomRatingsTests(unittest.TestCase):
    def test_all_bom_rows_have_datasheets_and_valid_margins(self) -> None:
        markdown = BOM_PATH.read_text(encoding="utf-8")
        for forbidden in BANNED_UNSAFE_PARTS:
            self.assertNotIn(forbidden, markdown, msg=f"Forbidden string {forbidden!r} must not appear in docs/bom.md")

        rows = parse_bom_tables(markdown)
        self.assertEqual(len(rows), len(EXPECTED_URL_TOKENS))
        for row in rows:
            validate_bom_row(row)

    def test_negative_cases_catch_underrated_parts(self) -> None:
        bad_switch = {
            "Ref": "SW_PWR",
            "Vetted Part / Options": "OS102011MA1QN1",
            "Worst-Case Requirement vs. Part Rating": "Req >= 2.94 A (2xAA) vs. Part 0.1 A @ 12 VDC",
            "Datasheet URL (Accessed `2026-11-15..16`)": "https://www.littelfuse.com/ck-slides-os.pdf",
        }
        bad_boost = {
            "Ref": "U2A",
            "Vetted Part / Options": "Wrong Boost",
            "Worst-Case Requirement vs. Part Rating": "Req start <= 1.80 V, I_out >= 1.00 A vs. Part 2.70 V start, >= 0.50 A @ 2.0 V",
            "Datasheet URL (Accessed `2026-11-15..16`)": "https://www.adafruit.com/product/2030",
        }
        bad_pfet = {
            "Ref": "Q1",
            "Vetted Part / Options": "High-Vgs PFET",
            "Worst-Case Requirement vs. Part Rating": "Req >= 2.94 A (2xAA) vs. Part -20 V, -4.0 A, <= 100 mOhm @ -10.0 V",
            "Datasheet URL (Accessed `2026-11-15..16`)": "https://www.diodes.com/assets/Datasheets/DMP2035U.pdf",
        }
        for bad in (bad_switch, bad_boost, bad_pfet):
            with self.assertRaises(AssertionError):
                validate_bom_row(bad)

    def test_negative_cases_catch_mismatched_urls(self) -> None:
        wrong_disp_url = {
            "Ref": "DISP1",
            "Vetted Part / Spec": "ST7789VW",
            "Worst-Case Requirement vs. Part Rating": "Req 3.3 V logic/VCC vs. Part 2.4–3.3 V IO (3.3 V module)",
            "Datasheet URL (Accessed `2026-11-15..16`)": "https://www.adafruit.com/product/4311",
        }
        wrong_pcb_url = {
            "Ref": "PCB1",
            "Vetted Part / Spec": "Perfboard",
            "Worst-Case Requirement vs. Part Rating": "Req 0.254 cm grid vs. Part 1.6 mm FR-4 PTH",
            "Datasheet URL (Accessed `2026-11-15..16`)": "https://www.mouser.com/datasheet/2/215/2462-739343.pdf",
        }
        for bad in (wrong_disp_url, wrong_pcb_url):
            with self.assertRaises(AssertionError):
                validate_bom_row(bad)


if __name__ == "__main__":
    unittest.main()
