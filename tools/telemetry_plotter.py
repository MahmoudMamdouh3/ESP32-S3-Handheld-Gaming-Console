"""
tools/telemetry_plotter.py — BMO Handheld serial telemetry parser and report renderer.
"""

import argparse
import json
import math
import re
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Dict, List, Optional

DEFAULT_FRAME_BUDGET_US = 16667

TELEMETRY_LINE_RE = re.compile(
    r"FPS:\s*(?P<fps>\d+)\s*"
    r"(?:\|\s*Emu:\s*(?P<emu>\d+)\s+target=(?P<target>\d+)us\s*)?"
    r"\|\s*Frame:\s*avg=(?P<avg>\d+)us\s+min=(?P<min>\d+)us\s+max=(?P<max>\d+)us"
    r"(?:\s*\|\s*Dropped:\s*(?P<dropped>\d+)\s*\(total\s*(?P<total_dropped>\d+)\))?"
)
FRAME_WINDOW_RE = re.compile(
    r"Frame window:\s*emu=(?P<emu>\d+)\s+n=(?P<count>\d+)\s+"
    r"target=(?P<target>\d+)us\s+p50=(?P<p50>\d+)us\s+"
    r"p95=(?P<p95>\d+)us\s+p99=(?P<p99>\d+)us"
)
PROFILE_CSV_RE = re.compile(
    r"\[PROFILE_CSV\]\s*"
    r"(?P<emu>-?\d+),(?P<count>\d+),(?P<target>\d+),(?P<fps>\d+),"
    r"(?P<min>\d+),(?P<avg>\d+),(?P<max>\d+),"
    r"(?P<p50>\d+),(?P<p95>\d+),(?P<p99>\d+),"
    r"(?P<dropped>\d+),(?P<spi_avg>\d+)"
)


@dataclass
class TelemetrySample:
    sample_index: int
    fps: int
    avg_us: int
    min_us: int
    max_us: int
    emulator_index: Optional[int] = None
    target_frame_us: int = DEFAULT_FRAME_BUDGET_US
    dropped: int = 0
    total_dropped: int = 0

    @property
    def jitter_us(self) -> int:
        return max(0, self.max_us - self.min_us)

    @property
    def headroom_percent(self) -> float:
        return max(0.0, (1.0 - (self.avg_us / float(self.target_frame_us))) * 100.0)


@dataclass
class TelemetrySummary:
    sample_count: int
    mean_fps: float
    min_fps: int
    max_fps: int
    mean_frame_us: float
    p50_frame_us: float
    p95_frame_us: float
    p99_frame_us: float
    mean_jitter_us: float
    max_jitter_us: int
    total_dropped_frames: int
    mean_headroom_percent: float


@dataclass
class FramePercentileWindow:
    sample_index: int
    emulator_index: int
    target_frame_us: int
    frame_count: int
    p50_us: int
    p95_us: int
    p99_us: int


@dataclass
class ProfileCsvRow:
    sample_index: int
    emulator_index: int
    frame_count: int
    target_frame_us: int
    fps: int
    min_us: int
    avg_us: int
    max_us: int
    p50_us: int
    p95_us: int
    p99_us: int
    dropped: int
    spi_avg_us: int


def parse_profile_csv(text: str) -> List[ProfileCsvRow]:
    """Extract `[PROFILE_CSV]` rows emitted by firmware built with `-DBMO_PROFILE=1`."""
    rows: List[ProfileCsvRow] = []
    for line in text.splitlines():
        match = PROFILE_CSV_RE.search(line)
        if not match:
            continue
        rows.append(
            ProfileCsvRow(
                sample_index=len(rows),
                emulator_index=int(match.group("emu")),
                frame_count=int(match.group("count")),
                target_frame_us=int(match.group("target")),
                fps=int(match.group("fps")),
                min_us=int(match.group("min")),
                avg_us=int(match.group("avg")),
                max_us=int(match.group("max")),
                p50_us=int(match.group("p50")),
                p95_us=int(match.group("p95")),
                p99_us=int(match.group("p99")),
                dropped=int(match.group("dropped")),
                spi_avg_us=int(match.group("spi_avg")),
            )
        )
    return rows


def parse_frame_percentile_windows(text: str) -> List[FramePercentileWindow]:
    """Read firmware percentile reports for unpaced emulator execution time."""
    windows: List[FramePercentileWindow] = []
    for line in text.splitlines():
        match = FRAME_WINDOW_RE.search(line)
        if not match:
            continue
        windows.append(FramePercentileWindow(
            sample_index=len(windows),
            emulator_index=int(match.group("emu")),
            target_frame_us=int(match.group("target")),
            frame_count=int(match.group("count")),
            p50_us=int(match.group("p50")),
            p95_us=int(match.group("p95")),
            p99_us=int(match.group("p99")),
        ))
    if windows:
        return windows
    return [
        FramePercentileWindow(
            sample_index=r.sample_index,
            emulator_index=r.emulator_index,
            target_frame_us=r.target_frame_us,
            frame_count=r.frame_count,
            p50_us=r.p50_us,
            p95_us=r.p95_us,
            p99_us=r.p99_us,
        )
        for r in parse_profile_csv(text)
    ]


def parse_telemetry_stream(text: str) -> List[TelemetrySample]:
    """Extracts all structured TelemetrySample entries from serial log text."""
    samples: List[TelemetrySample] = []
    for line in text.splitlines():
        match = TELEMETRY_LINE_RE.search(line)
        if not match:
            continue
        samples.append(
            TelemetrySample(
                sample_index=len(samples),
                fps=int(match.group("fps")),
                avg_us=int(match.group("avg")),
                min_us=int(match.group("min")),
                max_us=int(match.group("max")),
                emulator_index=(int(match.group("emu")) if match.group("emu") else None),
                target_frame_us=int(match.group("target") or DEFAULT_FRAME_BUDGET_US),
                dropped=int(match.group("dropped") or 0),
                total_dropped=int(match.group("total_dropped") or 0),
            )
        )
    if samples:
        return samples
    return [
        TelemetrySample(
            sample_index=r.sample_index,
            fps=r.fps,
            avg_us=r.avg_us,
            min_us=r.min_us,
            max_us=r.max_us,
            emulator_index=r.emulator_index,
            target_frame_us=r.target_frame_us,
            dropped=r.dropped,
            total_dropped=r.dropped,
        )
        for r in parse_profile_csv(text)
    ]


def _percentile(sorted_vals: List[int], pct: float) -> float:
    if not sorted_vals:
        return 0.0
    if len(sorted_vals) == 1:
        return float(sorted_vals[0])
    pos = (len(sorted_vals) - 1) * (pct / 100.0)
    lo = int(math.floor(pos))
    hi = int(math.ceil(pos))
    if lo == hi:
        return float(sorted_vals[lo])
    weight = pos - lo
    return sorted_vals[lo] * (1.0 - weight) + sorted_vals[hi] * weight


def summarize_telemetry(samples: List[TelemetrySample]) -> TelemetrySummary:
    """Computes statistical frame pacing and FPS metrics across telemetry samples."""
    if not samples:
        return TelemetrySummary(0, 0.0, 0, 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0, 0, 0.0)

    n = len(samples)
    fps_vals = [s.fps for s in samples]
    avg_vals = sorted(s.avg_us for s in samples)
    jitters = [s.jitter_us for s in samples]
    total_dropped = max((s.total_dropped for s in samples), default=0) or sum(s.dropped for s in samples)

    return TelemetrySummary(
        sample_count=n,
        mean_fps=round(sum(fps_vals) / n, 2),
        min_fps=min(fps_vals),
        max_fps=max(fps_vals),
        mean_frame_us=round(sum(avg_vals) / n, 1),
        p50_frame_us=round(_percentile(avg_vals, 50.0), 1),
        p95_frame_us=round(_percentile(avg_vals, 95.0), 1),
        p99_frame_us=round(_percentile(avg_vals, 99.0), 1),
        mean_jitter_us=round(sum(jitters) / n, 1),
        max_jitter_us=max(jitters),
        total_dropped_frames=total_dropped,
        mean_headroom_percent=round(sum(s.headroom_percent for s in samples) / n, 2),
    )


def render_ascii_plot(samples: List[TelemetrySample], width: int = 50,
                      frame_windows: Optional[List[FramePercentileWindow]] = None,
                      source_label: str = "CAPTURED") -> str:
    """Renders an ASCII bar chart of FPS and frame time latency."""
    summary = summarize_telemetry(samples)
    lines = [
        "=" * 70,
        f" BMO HANDHELD TELEMETRY & FRAME PACING REPORT [{source_label}]",
        "=" * 70,
        f" Samples: {summary.sample_count} | Mean FPS: {summary.mean_fps:.2f} (min {summary.min_fps}, max {summary.max_fps})",
        f" Frame Latency: mean={summary.mean_frame_us:.1f}us | p50={summary.p50_frame_us:.1f}us | p95={summary.p95_frame_us:.1f}us | p99={summary.p99_frame_us:.1f}us",
        f" Jitter: mean={summary.mean_jitter_us:.1f}us (max {summary.max_jitter_us}us) | Headroom: {summary.mean_headroom_percent:.1f}% | Dropped: {summary.total_dropped_frames}",
        "-" * 70,
    ]
    if frame_windows:
        latest = frame_windows[-1]
        lines.append(
            f" Emulator execution (last {latest.frame_count} frames): "
            f"p50={latest.p50_us}us p95={latest.p95_us}us p99={latest.p99_us}us"
        )
    for s in samples[:30]:
        ratio = min(1.0, s.avg_us / float(s.target_frame_us))
        filled = int(round(ratio * width))
        bar = "#" * filled + "." * (width - filled)
        lines.append(f" [{s.sample_index:02d}] {s.fps:2d} FPS |{bar}| {s.avg_us:5d}us (jitter {s.jitter_us:4d}us)")
    lines.append("=" * 70)
    return "\n".join(lines)


def generate_synthetic_telemetry(seconds: int = 12) -> str:
    """Generates representative 60 FPS serial output lines for self-test and CI validation."""
    lines: List[str] = []
    total_drop = 0
    for i in range(seconds):
        avg = 4850 + ((i * 137) % 420)
        lo = avg - 280
        hi = avg + 510 + ((i % 3) * 110)
        fps = 60 if hi < DEFAULT_FRAME_BUDGET_US else 59
        lines.append(f"[INFO] FPS: {fps} | Emu: 1 target=16742us | Frame: avg={avg}us min={lo}us max={hi}us | Dropped: 0 (total {total_drop})")
    lines.append("[INFO] Frame window: emu=1 n=600 target=16742us p50=5100us p95=6200us p99=7400us (core execution)")
    return "\n".join(lines)


def _with_synthetic_suffix(path: Path) -> Path:
    if "synthetic" in path.stem.lower():
        return path
    return path.with_name(f"{path.stem}_SYNTHETIC{path.suffix}")


def main() -> int:
    parser = argparse.ArgumentParser(description="Analyze and plot BMO Handheld serial telemetry logs.")
    parser.add_argument("--input", type=Path, default=None, help="Path to captured serial log file")
    parser.add_argument("--output", type=Path, default=None, help="ASCII report output path")
    parser.add_argument("--json-out", type=Path, default=None, help="Optional JSON summary output path")
    args = parser.parse_args()

    synthetic = not (args.input and args.input.is_file())
    text = args.input.read_text(encoding="utf-8", errors="ignore") if not synthetic else generate_synthetic_telemetry()
    samples = parse_telemetry_stream(text)
    frame_windows = parse_frame_percentile_windows(text)
    source_label = "SYNTHETIC" if synthetic else "CAPTURED"
    output_path = args.output or Path("bmo_telemetry_SYNTHETIC.txt" if synthetic else "bmo_telemetry.txt")
    if synthetic:
        output_path = _with_synthetic_suffix(output_path)
    report_text = render_ascii_plot(samples, frame_windows=frame_windows, source_label=source_label)
    output_path.write_text(report_text + "\n", encoding="utf-8")
    print(f"Data source: {source_label}")
    print(report_text)
    print(f"Output: {output_path}")
    if args.json_out:
        summary = summarize_telemetry(samples)
        report = {
            "interval_telemetry": asdict(summary),
            "frame_execution_windows": [asdict(window) for window in frame_windows],
        }
        json_path = _with_synthetic_suffix(args.json_out) if synthetic else args.json_out
        json_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(f"JSON output: {json_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
