import tempfile
import unittest
from pathlib import Path

try:
    from tests.host_test_support import build_and_run_native_cxx
except ImportError:
    from host_test_support import build_and_run_native_cxx


class HostTestSupportTests(unittest.TestCase):
    def test_native_cxx_helper_runs_and_catches_asan_oob(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            src = Path(tmpdir) / "asan_probe.cpp"
            src.write_text(
                "#include <cstdlib>\n"
                "#include <cstring>\n"
                "#if defined(__GNUC__) || defined(__clang__)\n"
                "__attribute__((no_sanitize(\"undefined\")))\n"
                "#endif\n"
                "static void write_index(volatile char* data, size_t index) {\n"
                "  data[index] = 1;\n"
                "}\n"
                "int main(int argc, char** argv) {\n"
                "  char* buf = static_cast<char*>(std::malloc(8));\n"
                "  if (!buf) return 2;\n"
                "  std::memset(buf, 0, 8);\n"
                "  if (argc > 1) {\n"
                "    write_index(buf, static_cast<size_t>(std::strtoul(argv[1], nullptr, 10)));\n"
                "  }\n"
                "  std::free(buf);\n"
                "  return 0;\n"
                "}\n",
                encoding="utf-8",
            )
            ok = build_and_run_native_cxx(self, [src], sanitize=True)
            self.assertEqual(ok.returncode, 0, msg=ok.stdout + ok.stderr)

            bad = build_and_run_native_cxx(self, [src], args=["8"], sanitize=True)
            self.assertNotEqual(bad.returncode, 0)
            self.assertIn("AddressSanitizer", bad.stdout + bad.stderr)


if __name__ == "__main__":
    unittest.main()
