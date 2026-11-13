"""Verify the mutation-testing sanity harness catches isolated bugs in C++ and Python modules."""

import shutil
import unittest
from pathlib import Path

try:
    from host_test_support import find_wasi_cxx
except ImportError:  # pragma: no cover - when imported as tests.test_mutation_sanity
    from tests.host_test_support import find_wasi_cxx
from scripts import run_mutation_sanity

REPO_ROOT = Path(__file__).resolve().parents[1]


class MutationSanityTests(unittest.TestCase):
    def test_10_mutations_are_killed_by_test_suite(self):
        has_wasi = bool(find_wasi_cxx(REPO_ROOT) and shutil.which("node"))
        rc = run_mutation_sanity.main([] if has_wasi else ["--python-only"])
        self.assertEqual(rc, 0)


if __name__ == "__main__":
    unittest.main()
