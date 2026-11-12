import contextlib
import io
import json
import unittest
from pathlib import Path

from scripts.generate_ai_knowledge_base import build_knowledge_base


REPO_ROOT = Path(__file__).resolve().parents[1]
GENERATED_PATHS = (
    REPO_ROOT / "AGENT_KNOWLEDGE_GRAPH.json",
    REPO_ROOT / "AGENT_DECISION_TREE.json",
    REPO_ROOT / "AGENT_MANIFEST.json",
    REPO_ROOT / ".agents" / "rules" / "CONTEXT_INDEX.json",
)


class GeneratedArtifactDeterminismTests(unittest.TestCase):
    def test_knowledge_artifacts_are_repeatable_without_live_build_measurements(self):
        with contextlib.redirect_stdout(io.StringIO()):
            build_knowledge_base()
            first = tuple(path.read_bytes() for path in GENERATED_PATHS)
            build_knowledge_base()
            second = tuple(path.read_bytes() for path in GENERATED_PATHS)

        self.assertEqual(first, second)
        graph = json.loads(GENERATED_PATHS[0].read_text(encoding="utf-8"))
        manifest = json.loads(GENERATED_PATHS[2].read_text(encoding="utf-8"))
        self.assertNotIn("generated_at", graph)
        self.assertNotIn("microbenchmark_baselines", graph)
        self.assertIn("bench-host", graph["benchmark_policy"])
        self.assertIsNone(graph["target_mcu"]["flash_used_bytes"])
        self.assertIsNone(graph["target_mcu"]["dram_used_bytes"])
        self.assertEqual(
            graph["target_mcu"]["memory_budget_report_command"],
            "python -m tools.guardian budget --elf <target-elf>",
        )
        self.assertIsNone(manifest["flash_budget_used_bytes"])
        self.assertIsNone(manifest["dram_budget_used_bytes"])
        self.assertIsNone(manifest["iram_budget_used_bytes"])
        self.assertEqual(
            manifest["memory_budget_report_command"],
            "python -m tools.guardian budget --elf <target-elf>",
        )
        self.assertFalse(
            any(path.startswith(".tools/") for path in graph["files_index"]),
            "Downloaded toolchains must not affect generated repository indexes",
        )
        self.assertNotIn("firmware/BmoGameboy/src/assets/rom_data.h", graph["files_index"])
        self.assertNotIn("firmware/BmoGameboy/src/assets/roms/virtual_bmo.h", graph["files_index"])


if __name__ == "__main__":
    unittest.main()
