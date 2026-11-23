import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


class OptionalLogTest(unittest.TestCase):
    def test_optional_log_errors_leave_child_running(self):
        console = Path(__file__).resolve().parents[1] / "frankenmoe-console.py"
        with tempfile.TemporaryDirectory() as tmp:
            for target in ("/dev/full", str(Path(tmp) / "missing" / "server.log")):
                with self.subTest(target=target):
                    proc = subprocess.run(
                        [sys.executable, str(console), "--save", target, "--",
                         sys.executable, "-c", "print('server alive', flush=True); "
                         "import time; time.sleep(.4); print('test child completed')"],
                        capture_output=True, text=True, timeout=10,
                    )
                    self.assertEqual(proc.returncode, 0, proc.stderr)
                    self.assertIn("Could not save the server log:", proc.stderr)
                    self.assertNotIn("Traceback", proc.stderr)


if __name__ == "__main__":
    unittest.main()
