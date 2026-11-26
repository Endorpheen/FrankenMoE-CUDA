"""Small archive safety and receipt tests without GPUs."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zipfile


spec = importlib.util.spec_from_file_location("gate", Path(__file__).with_name("release-gate.py"))
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)


class Archives(unittest.TestCase):
    def test_inventory_and_hashes(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            data = root / "data.bin"
            data.write_bytes(b"checkpoint")
            manifest = root / "CONTENTS.sha256"
            manifest.write_text(f"{gate.sha(data)}  ./data.bin\n")
            self.assertEqual(len(gate.check_contents(root)), 1)
            data.write_bytes(b"different")
            with self.assertRaisesRegex(ValueError, "mismatch"):
                gate.check_contents(root)
            (root / "unlisted.bin").touch()
            with self.assertRaisesRegex(ValueError, "every packaged file"):
                gate.check_contents(root)

    def test_refuses_path_traversal(self):
        with tempfile.TemporaryDirectory() as work:
            root = Path(work)
            archive = root / "unsafe.zip"
            with zipfile.ZipFile(archive, "w") as out:
                out.writestr("package/../../escape", "invalid")
            with self.assertRaisesRegex(ValueError, "unsafe archive path"):
                gate.extract(archive, root / "unpacked")
            self.assertFalse((root / "escape").exists())

    def test_residency_is_rounded_to_pages(self):
        with patch.object(gate, "run", return_value="8192 8191"):
            gate.resident_weights(Path("does-not-exist.gguf"))

    def test_refuses_duplicate_checksums(self):
        entry = "0" * 64 + "  data.bin\n"
        with self.assertRaisesRegex(ValueError, "duplicate checksum"):
            gate.sums(entry * 2)

    def test_vulkan_distribution_excludes_cuda_entries(self):
        matrix = {"rows": [
            {"os": "linux-x64", "backend": "cuda", "entry": "bin/apex-server", "files": ["cuda/module.cubin"]},
            {"os": "linux-x64", "backend": "vulkan", "entry": "vulkan/frankenmoe-vulkan", "files": ["vulkan/README.md"]},
            {"os": "windows-x64", "backend": "vulkan", "entry": "vulkan/frankenmoe-vulkan.exe", "files": []},
        ]}
        files = gate.distribution_files(matrix, "linux-x64")
        self.assertEqual({str(path) for path in files},
                         {"README.md", "LICENSE", "vulkan/frankenmoe-vulkan", "vulkan/README.md"})


if __name__ == "__main__":
    unittest.main()
