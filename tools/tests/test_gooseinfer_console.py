import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


class OptionalLogTest(unittest.TestCase):
    def test_optional_log_errors_leave_child_running(self):
        console = Path(__file__).resolve().parents[1] / "gooseinfer-console.py"
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

    def test_rotation_keeps_one_bounded_backup(self):
        console = Path(__file__).resolve().parents[1] / "gooseinfer-console.py"
        with tempfile.TemporaryDirectory() as tmp:
            log = Path(tmp) / "server.log"
            wrapper = (
                "import runpy, sys; "
                "module = runpy.run_path(sys.argv.pop(1)); "
                "module['main'].__globals__['LOG_MAX_BYTES'] = 32; "
                "sys.exit(module['main']())"
            )
            lines = "for i in range(10): print(f'entry-{i:02d}', flush=True)"
            proc = subprocess.run(
                [sys.executable, "-c", wrapper, str(console), "--save", str(log), "--",
                 sys.executable, "-c", lines],
                capture_output=True, text=True, timeout=10,
            )
            self.assertEqual(proc.returncode, 0, proc.stderr)
            self.assertEqual(log.read_bytes(), b"entry-09\n")
            self.assertEqual(
                log.with_suffix(".log.1").read_bytes(),
                b"entry-06\nentry-07\nentry-08\n",
            )
            self.assertEqual(len(list(Path(tmp).iterdir())), 2)


if __name__ == "__main__":
    unittest.main()
