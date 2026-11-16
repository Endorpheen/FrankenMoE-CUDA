#!/usr/bin/env python3
"""Regression checks for read-only runtime accounting and link provenance."""
import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
import os

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("closure", ROOT / "tools/audit-runtime-closure.py")
closure = importlib.util.module_from_spec(spec)
spec.loader.exec_module(closure)


class RuntimeClosureTests(unittest.TestCase):
    def test_relative_commands_use_recorded_working_directory(self):
        with tempfile.TemporaryDirectory() as td:
            directory = Path(td)
            record = directory / "commands.jsonl"
            record.write_text(json.dumps(["cc", "-c", "src/bridge.c", "-o", "bridge.o"]) + "\n")
            index, _ = closure.compile_index([record], {str(record): directory})
            self.assertEqual(index[directory / "bridge.o"][0], directory / "src/bridge.c")

    def test_ninja_includes_implicit_archives_but_not_order_only_edges(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "build.ninja"
            path.write_text("build bin/server: CXX_EXECUTABLE_LINKER__server_Release main.o | common.a libggml.so || order-only.o\n"
                            "build common.a: CXX_STATIC_LIBRARY_LINKER__common_Release context.o queue.o\n")
            links = closure.ninja_links(path)
            self.assertEqual(links["bin/server"], ["main.o", "common.a"])
            self.assertEqual(links["common.a"], ["context.o", "queue.o"])

    def test_compile_database_and_generator_provenance(self):
        with tempfile.TemporaryDirectory() as td:
            directory = Path(td)
            database = directory / "compile_commands.json"
            database.write_text(json.dumps([{"directory": td, "command": "cc -c generated.c -o generated.o"}]))
            generators = directory / "generators.json"
            generators.write_text(json.dumps([["/pinned/goose", "-o", "generated.c", "--header", "generated.h", "policy.goose"]]))
            index, source = closure.compile_index([database, generators], {str(generators): directory})
            self.assertEqual(source[index[directory / "generated.o"][0]], directory / "policy.goose")

    def test_default_status_never_executes_model_probe(self):
        with tempfile.TemporaryDirectory() as td:
            directory = Path(td)
            marker = directory / "executed"
            probe = directory / "probe"
            probe.write_text('#!/bin/sh\ntouch "$FG_TEST_MARKER"\n'
                             'printf "FG_TENSOR_DATA mismatches=0 tensors=1\\nFG_STANDALONE_V3 agree=yes\\n"\n')
            probe.chmod(0o700)
            model = directory / "fixture.gguf"
            model.write_bytes(b"not a model")
            env = {**os.environ, "FG_STANDALONE_BIN": str(probe), "FG_APEX_GGUF": str(model), "FG_TEST_MARKER": str(marker)}
            command = ["bash", str(ROOT / "tools/migration-status.sh")]
            result = subprocess.run(command, env=env, text=True, capture_output=True, check=True)
            self.assertFalse(marker.exists(), result.stdout + result.stderr)
            self.assertIn("not probed", result.stdout)
            subprocess.run(command + ["--probe-load"], env=env, capture_output=True, check=True)
            self.assertTrue(marker.exists())


if __name__ == "__main__":
    unittest.main()
