import os
import subprocess
import tempfile
import unittest
from pathlib import Path

from host_test_support import build_and_run_native_cxx


REPO_ROOT = Path(__file__).resolve().parents[1]


class SpiArbiterTests(unittest.TestCase):
    def test_mutex_failures_never_continue_as_successful_locks(self):
        source = REPO_ROOT / "tests" / "host" / "spi_arbiter_test.cpp"
        firmware_core = REPO_ROOT / "firmware" / "BmoGameboy" / "src" / "core"
        fake_headers = REPO_ROOT / "tests" / "host" / "fakes"
        arbiter_source = firmware_core / "spi_arbiter.cpp"

        with tempfile.TemporaryDirectory() as workdir:
            normal = build_and_run_native_cxx(
                self,
                [source, arbiter_source],
                include_dirs=[fake_headers, firmware_core],
                args=["normal"],
                sanitize=False,
                workdir=workdir,
            )
            self.assertEqual(normal.returncode, 0, normal.stdout + normal.stderr)

            executable = Path(workdir) / ("host_test.exe" if os.name == "nt" else "host_test")
            for mode in ("create-failure", "before-init", "take-failure"):
                with self.subTest(mode=mode):
                    failed = subprocess.run(
                        [str(executable), mode],
                        capture_output=True,
                        text=True,
                        check=False,
                    )
                    self.assertNotEqual(
                        failed.returncode,
                        0,
                        f"{mode} continued without a valid lock",
                    )


if __name__ == "__main__":
    unittest.main()
