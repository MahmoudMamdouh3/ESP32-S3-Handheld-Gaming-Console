import contextlib
import io
import os
import unittest
from unittest.mock import patch

from scripts import validate_repo


class ValidateRepoStrictModeTests(unittest.TestCase):
    def _run_without_arduino_cli(self, argv=(), env=None):
        output = io.StringIO()
        patched_env = {"BMO_REQUIRE_TARGET_COMPILE": ""}
        if env:
            patched_env.update(env)
        phases = [("Phase 0: Binary Compilation Gate", validate_repo.check_arduino_compile)]
        with patch.object(validate_repo, "PHASES", phases), \
             patch.object(validate_repo, "_find_arduino_cli", return_value=None), \
             patch.dict(os.environ, patched_env, clear=False), \
             contextlib.redirect_stdout(output):
            validate_repo.TARGET_COMPILE_STATUS = "NOT_RUN"
            validate_repo.main(list(argv))
        return output.getvalue()

    def test_default_mode_reports_unverified_skip_in_summary(self):
        output = self._run_without_arduino_cli()
        self.assertIn("TARGET COMPILE: SKIPPED - NOT VERIFIED", output)
        self.assertIn("VALIDATION PASSED", output)

    def test_flag_makes_missing_target_compile_fail(self):
        with self.assertRaises(SystemExit) as raised:
            self._run_without_arduino_cli(("--require-target-compile",))
        self.assertEqual(raised.exception.code, 1)

    def test_environment_variable_makes_missing_target_compile_fail(self):
        with self.assertRaises(SystemExit) as raised:
            self._run_without_arduino_cli(env={"BMO_REQUIRE_TARGET_COMPILE": "1"})
        self.assertEqual(raised.exception.code, 1)

    def test_phase_two_rejects_debug_and_unit_test_definitions(self):
        definitions = validate_repo._debug_only_definitions(
            "#define ENABLE_UNIT_TESTS 1\n-DBMO_DEBUG_BUILD=1\n#define LOG_LEVEL_DEBUG 4\n"
        )
        self.assertEqual(definitions, ["ENABLE_UNIT_TESTS", "BMO_DEBUG_BUILD"])

    def test_phase_two_guard_is_clean_for_current_release_headers_and_profile(self):
        self.assertEqual(validate_repo.check_release_debug_macro_guardrails(), [])

    def test_reported_fqbn_matches_pinned_sketch_profile(self):
        profile = (validate_repo.FIRMWARE_DIR / "sketch.yaml").read_text(encoding="utf-8")
        self.assertIn(f"fqbn: {validate_repo.ARDUINO_FQBN}", profile)


if __name__ == "__main__":
    unittest.main()
