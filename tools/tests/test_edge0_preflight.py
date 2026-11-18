#!/usr/bin/env python3
"""Reject unfinished and same-size corrupt pack files before any model load."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("edge0_pack", ROOT / "tools/check-edge0-pack.py")
pack = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pack)


class Edge0PreflightTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / "scales").mkdir()
        for layer in range(48):
            (self.root / "scales" / f"l{layer}.f32").write_bytes(bytes(5120))
        self.manifest = {"files": [{"path": "experts.bin", "bytes": 3, "algorithm": "sha256",
                                    "digest": hashlib.sha256(b"abc").hexdigest()}]}

    def problems(self, hash_files=True, selected=None):
        problems = []
        pack.check_files(self.root, hash_files, problems, self.manifest, selected)
        return problems

    def test_full_sized_part_is_not_reported_ready(self):
        (self.root / "experts.bin.part").write_bytes(b"abc")
        self.assertTrue(any("unfinalized" in s for s in self.problems(False)))

    def test_same_size_corruption_is_rejected(self):
        (self.root / "experts.bin").write_bytes(b"abd")
        self.assertTrue(any("hash mismatch" in s for s in self.problems()))

    def test_correct_sha_and_git_blob_hashes_pass(self):
        (self.root / "experts.bin").write_bytes(b"abc")
        self.assertEqual(self.problems(), [])
        row = self.manifest["files"][0]
        row.update(algorithm="git-blob-sha1", digest=hashlib.sha1(b"blob 3\0abc").hexdigest())
        self.assertEqual(self.problems(), [])

    def test_selective_recheck_does_not_accept_unchecked_hashes(self):
        (self.root / "experts.bin").write_bytes(b"abc")
        (self.root / "other.bin").write_bytes(b"wrong")
        self.manifest["files"].append({"path": "other.bin", "bytes": 5, "algorithm": "sha256", "digest": "0" * 64})
        self.assertEqual(self.problems(selected={"experts.bin"}), [])
        self.assertTrue(any("other.bin" in s for s in self.problems()))

    def test_duplicate_layers_are_rejected(self):
        rows = [f"{layer} 11 20 {layer * 512 * pack.EXPERT_RECORD} {pack.EXPERT_RECORD}" for layer in range(48)]
        rows[-1] = rows[0]
        (self.root / "native_experts.txt").write_text("\n".join(rows) + "\n")
        problems = []
        pack.check_native_experts(self.root, problems)
        self.assertTrue(any("duplicate" in s for s in problems))


if __name__ == "__main__":
    unittest.main()
