import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


class LauncherCompatibilityTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        repo = Path(__file__).resolve().parents[2]
        shutil.copy2(repo / "tools/multimodel-release/launch-linux.sh", self.root / "gooseinfer")
        (self.root / "frankenmoe").symlink_to("gooseinfer")
        (self.root / "bin").mkdir()
        (self.root / "profiles").mkdir()
        (self.root / "profiles/apex.argv").write_text("--ctx-size\n36864\n")
        (self.root / "profiles/apex.env").write_text("")
        binary = self.root / "bin/apex-server"
        binary.write_text('#!/bin/sh\nprintf "%s\\n" "$GOOSEINFER_WEB_ROOT" "$FRANKENMOE_WEB_ROOT" "$@"\n')
        binary.chmod(0o755)
        self.model = self.root / "Apex-35B-V1.0-Q4_K_M.gguf"
        self.model.touch()

    def launch(self, name, settings):
        env = {key: value for key, value in os.environ.items()
               if not key.startswith(("GOOSEINFER_", "FRANKENMOE_"))}
        env.update(settings)
        complete = subprocess.run(
            ["bash", str(self.root / name), "apex"],
            env=env, text=True, capture_output=True, check=True, timeout=10,
        )
        return complete.stdout.splitlines()

    def test_new_names_have_priority_and_reach_old_binary(self):
        output = self.launch("gooseinfer", {
            "GOOSEINFER_MODELS_DIR": str(self.root), "FRANKENMOE_MODELS_DIR": "/invalid",
            "GOOSEINFER_WEB_ROOT": "/new/web", "FRANKENMOE_WEB_ROOT": "/old/web",
        })
        self.assertEqual(output, ["/new/web", "/new/web", "--ctx-size", "36864",
                                  str(self.model), "--serve", "9101"])

    def test_legacy_entry_point_and_variables_still_work(self):
        output = self.launch("frankenmoe", {
            "FRANKENMOE_MODELS_DIR": str(self.root), "FRANKENMOE_WEB_ROOT": "/old/web",
        })
        self.assertEqual(output, ["/old/web", "/old/web", "--ctx-size", "36864",
                                  str(self.model), "--serve", "9101"])


if __name__ == "__main__":
    unittest.main()
