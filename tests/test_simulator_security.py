import unittest

from tools.bmo_simulator import run_simulator


class SimulatorSecurityTests(unittest.TestCase):
    def setUp(self):
        with run_simulator._rate_lock:
            run_simulator._request_times.clear()

    def test_only_canonical_loopback_host_and_origin_are_accepted(self):
        self.assertTrue(run_simulator.valid_host_header("127.0.0.1:8080"))
        self.assertFalse(run_simulator.valid_host_header("localhost:8080"))
        self.assertFalse(run_simulator.valid_host_header("attacker.example:8080"))
        self.assertTrue(run_simulator.valid_origin_header("http://127.0.0.1:8080"))
        self.assertFalse(run_simulator.valid_origin_header("http://attacker.example:8080"))

    def test_static_paths_cannot_escape_simulator_directory(self):
        self.assertIsNone(run_simulator.resolve_static_path("/%2e%2e/README.md"))
        self.assertIsNone(run_simulator.resolve_static_path("/../../AGENTS.md"))
        self.assertEqual(
            run_simulator.resolve_static_path("/index.html"),
            run_simulator.DIRECTORY / "index.html",
        )

    def test_host_test_request_limit_is_three_per_minute(self):
        self.assertTrue(run_simulator.allow_host_core_request("127.0.0.1", now=100.0))
        self.assertTrue(run_simulator.allow_host_core_request("127.0.0.1", now=101.0))
        self.assertTrue(run_simulator.allow_host_core_request("127.0.0.1", now=102.0))
        self.assertFalse(run_simulator.allow_host_core_request("127.0.0.1", now=103.0))
        self.assertTrue(run_simulator.allow_host_core_request("127.0.0.1", now=160.0))


if __name__ == "__main__":
    unittest.main()
