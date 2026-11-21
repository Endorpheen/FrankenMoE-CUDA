#!/usr/bin/env python3
"""Check the architecture acceptance policy without a GPU or model files."""

import contextlib
import importlib.util
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock


SPEC = importlib.util.spec_from_file_location("gate", Path(__file__).resolve().parents[1] / "check-model-regressions.py")
GATE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GATE)


class ArchitectureRegressions(unittest.TestCase):
    def setUp(self):
        self.registry = json.loads(GATE.REGISTRY.read_bytes())
        self.case = self.registry["architectures"][0]["references"][0]
        reference = dict(zip(GATE.RATE_KEYS + GATE.MEMORY_KEYS, (1200, 29, 21479000, 7272)))
        reference.update(process_swap_peak_kib=0, system_swap_out_pages=0,
                         output_fingerprint=self.case["baseline_output"])
        self.runs = [reference.copy() for _ in GATE.ORDER]

    def test_identical_runs_pass_case(self):
        self.assertEqual(GATE.compare(self.case, self.runs)["status"], "pass")

    def test_reject_slow_legs_independently(self):
        for key in GATE.RATE_KEYS:
            with self.subTest(metric=key):
                original = self.runs[1][key]
                self.runs[1][key] = original * 0.94
                self.assertEqual(GATE.compare(self.case, self.runs)["status"], "fail")
                self.runs[1][key] = original

    def test_reject_memory_growth_and_swap(self):
        for key in (*GATE.MEMORY_KEYS, "process_swap_peak_kib", "system_swap_out_pages"):
            with self.subTest(metric=key):
                original = self.runs[1][key]
                self.runs[1][key] += 1
                self.assertEqual(GATE.compare(self.case, self.runs)["status"], "fail")
                self.runs[1][key] = original

    def test_changed_response_requires_review(self):
        self.runs[1]["output_fingerprint"] = "f" * 64
        report = GATE.compare(self.case, self.runs)
        self.assertEqual(report["status"], "fail")
        self.assertIn("quality review", report["failures"][0])
        self.case["reviewed_outputs"].append({"fingerprint": "f" * 64, "evidence": "docs/goose-migration/APEX-32K-ACCEPTANCE.md"})
        GATE.validate_registry(self.registry)
        self.assertEqual(GATE.compare(self.case, self.runs)["status"], "pass")

    def test_legacy_gap_is_reported(self):
        _, gaps = GATE.validate_registry(self.registry)
        self.assertIn("maxwell-moe", gaps)

    def test_reject_missing_apex_reference(self):
        self.registry["architectures"][0]["references"] = []
        with self.assertRaisesRegex(ValueError, "without a regression reference"):
            GATE.validate_registry(self.registry)

    def test_gate_runs_remaining_cases_after_failure(self):
        second = dict(self.case, id="second-apex-profile")
        self.registry["architectures"][0]["references"].append(second)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            registry = root / "registry.json"
            registry.write_text(json.dumps(self.registry))
            report = root / "report"
            args = ["check-model-regressions.py", "--candidate", sys.executable, "--output", str(report)]
            with mock.patch.object(GATE, "REGISTRY", registry), mock.patch.object(sys, "argv", args), \
                 mock.patch.object(GATE, "capture", side_effect=[ValueError("bad capture"), {"status": "pass", "failures": []}]) as run, \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(GATE.main(), 1)
            self.assertEqual(run.call_count, 2)
            receipt = json.loads((report / "receipt.json").read_text())
            self.assertEqual(receipt["references"][self.case["id"]]["failures"], ["bad capture"])
            self.assertEqual(receipt["references"][second["id"]]["status"], "pass")
            self.assertEqual(receipt["missing_legacy_references"], ["maxwell-moe"])


if __name__ == "__main__":
    unittest.main()
