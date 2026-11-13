import contextlib
import io
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools.telemetry_plotter import (
    main,
    parse_frame_percentile_windows,
    parse_profile_csv,
    parse_telemetry_stream,
    render_ascii_plot,
)


class TelemetryPlotterTests(unittest.TestCase):
    def test_parses_per_second_record(self):
        samples = parse_telemetry_stream(
            "[INFO] FPS: 59 | Emu: 2 target=16667us | Frame: avg=1200us min=900us max=1800us | Dropped: 1 (total 4)"
        )
        self.assertEqual(len(samples), 1)
        self.assertEqual((samples[0].fps, samples[0].emulator_index, samples[0].target_frame_us), (59, 2, 16667))
        self.assertEqual((samples[0].avg_us, samples[0].min_us, samples[0].max_us), (1200, 900, 1800))

    def test_parses_frame_percentile_record(self):
        windows = parse_frame_percentile_windows(
            "[INFO] Frame window: emu=4 n=600 target=28571us p50=1100us p95=2400us p99=3100us (core execution)"
        )
        self.assertEqual(len(windows), 1)
        self.assertEqual((windows[0].emulator_index, windows[0].frame_count), (4, 600))
        self.assertEqual((windows[0].p50_us, windows[0].p95_us, windows[0].p99_us), (1100, 2400, 3100))

    def test_parses_profile_csv_rows_and_falls_back_cleanly(self):
        csv_log = "\n".join([
            "[PROFILE_CSV_HDR] emu,n,target_us,fps,min_us,avg_us,max_us,p50_us,p95_us,p99_us,dropped,spi_avg_us",
            "[PROFILE_CSV] 1,600,16742,60,1401,1967,2400,2100,2370,2394,0,3120",
        ])
        rows = parse_profile_csv(csv_log)
        self.assertEqual(len(rows), 1)
        self.assertEqual((rows[0].emulator_index, rows[0].frame_count, rows[0].spi_avg_us), (1, 600, 3120))
        samples = parse_telemetry_stream(csv_log)
        self.assertEqual(len(samples), 1)
        self.assertEqual((samples[0].fps, samples[0].min_us, samples[0].avg_us, samples[0].max_us), (60, 1401, 1967, 2400))
        windows = parse_frame_percentile_windows(csv_log)
        self.assertEqual(len(windows), 1)
        self.assertEqual((windows[0].p50_us, windows[0].p95_us, windows[0].p99_us), (2100, 2370, 2394))

    def test_synthetic_default_is_stamped_in_title_filename_and_stdout(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            old_cwd = Path.cwd()
            os.chdir(temp_dir)
            try:
                output = io.StringIO()
                with patch.object(sys, "argv", ["telemetry_plotter.py"]), contextlib.redirect_stdout(output):
                    self.assertEqual(main(), 0)
                text = output.getvalue()
                self.assertIn("Data source: SYNTHETIC", text)
                self.assertIn("[SYNTHETIC]", text)
                self.assertIn("bmo_telemetry_SYNTHETIC.txt", text)
                report = Path("bmo_telemetry_SYNTHETIC.txt")
                self.assertTrue(report.is_file())
                self.assertIn("[SYNTHETIC]", report.read_text(encoding="utf-8"))
            finally:
                os.chdir(old_cwd)


if __name__ == "__main__":
    unittest.main()
