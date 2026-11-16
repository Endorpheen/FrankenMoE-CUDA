#!/usr/bin/env python3
"""W3-R gate: scripts/make-release.sh, end to end.

Both modes of the real script, in the working checkout, for the cases that do
not need the network: the tag acceptance rule, the notes requirement, the asset
size limit and the runtime-asset class. Only the dry-run path is exercised, so
no tag is created and nothing is pushed.

The checkout must be a git repository with `origin/main` present (the script
requires a clean tree and HEAD == origin/main); `gates/run.sh` skips this gate
when it is not.

Usage: w3_release.py
"""
from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from host import ROOT, Tally  # noqa: E402

SCRIPT = ROOT / "scripts" / "make-release.sh"


def is_repo() -> bool:
    p = subprocess.run(["git", "rev-parse", "--git-dir"], cwd=str(ROOT),
                       capture_output=True, text=True, check=False)
    q = subprocess.run(["git", "rev-parse", "--verify", "origin/main"], cwd=str(ROOT),
                       capture_output=True, text=True, check=False)
    return p.returncode == 0 and q.returncode == 0


def run(mode: str, *args: str) -> tuple[int, str]:
    env = dict(os.environ)
    env["BMOE_GOOSE_PKG"] = mode
    p = subprocess.run(["bash", str(SCRIPT), *args], cwd=str(ROOT), env=env,
                       capture_output=True, text=True, check=False)
    return p.returncode, p.stdout + p.stderr


def main() -> int:
    if not is_repo():
        print("FG663_RELEASE skipped: the checkout is not a git repository with origin/main")
        return 0
    if subprocess.run(["git", "status", "--porcelain"], cwd=str(ROOT),
                      capture_output=True, text=True).stdout.strip():
        print("FG663_RELEASE skipped: the working tree is not clean")
        return 0

    t = Tally("FG663_RELEASE")
    with tempfile.TemporaryDirectory() as td:
        notes = Path(td) / "notes.md"
        notes.write_text("# notes\n- gate case\n")
        small = Path(td) / "goose-runtime-linux-x64.tar.gz"
        small.write_bytes(b"x" * 1024)
        plain = Path(td) / "bmoe-cli-linux-x64.tar.gz"
        plain.write_bytes(b"y" * 1024)
        huge = Path(td) / "goose-runtime-linux-arm64.tar.gz"
        with open(huge, "wb") as f:
            f.truncate(3 * 1024 ** 3)   # sparse: 3 GiB by stat, no bytes on disk

        scenarios = [
            ("dry_run", ("--version", "v0.1.0", "--notes-file", str(notes))),
            ("dry_run_rc", ("--version", "v0.1.0-rc.1", "--notes-file", str(notes))),
            ("with_runtime_asset", ("--version", "v0.1.0", "--notes-file", str(notes),
                                    "--asset", str(small))),
            ("with_plain_asset", ("--version", "v0.1.0", "--notes-file", str(notes),
                                  "--asset", str(plain))),
            ("keep_old_runtime", ("--version", "v0.1.0", "--notes-file", str(notes),
                                  "--asset", str(small), "--keep-old-runtime")),
            ("bad_version_missing_minor", ("--version", "v1.0", "--notes-file", str(notes))),
            ("bad_version_rc_without_dot", ("--version", "v0.1.0-rc", "--notes-file", str(notes))),
            ("bad_version_no_prefix", ("--version", "0.1.0", "--notes-file", str(notes))),
            ("bad_version_empty_patch", ("--version", "v0.1.", "--notes-file", str(notes))),
            ("missing_notes", ("--version", "v0.1.0", "--notes-file", str(Path(td) / "nope.md"))),
            ("missing_asset", ("--version", "v0.1.0", "--notes-file", str(notes),
                               "--asset", str(Path(td) / "absent.bin"))),
            ("asset_over_limit", ("--version", "v0.1.0", "--notes-file", str(notes),
                                  "--asset", str(huge))),
        ]
        for name, args in scenarios:
            off = run("0", *args)
            on = run("1", *args)
            t.rows += 1
            t.eq(t.rows, f"{name} exit", off[0], on[0])
            t.eq(t.rows, f"{name} output", off[1], on[1])
    return t.done()


if __name__ == "__main__":
    raise SystemExit(main())
