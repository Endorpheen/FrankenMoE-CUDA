#!/usr/bin/env python3
"""W4 gate: the fixture plan in scripts/make-tiny-moe.py.

The two arms are the real generator in both modes, over 12 parameter
combinations: the emitted gguf files must be byte-identical, file for file, and
the printed banner and the refusal messages must match. Weights are the same
random draws when the plan is the same, so a plan difference shows up as a
different file and not as a different size only.

Usage: w4_fx.py
"""
from __future__ import annotations

import hashlib
import os
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from host import ROOT, Tally  # noqa: E402

SCRIPT = ROOT / "scripts" / "make-tiny-moe.py"

CASES = [
    ("qwen3moe", 0),
    ("qwen3moe", 20),   # the fixture runtime/tests/CMakeLists.txt uses
    ("qwen3moe", 10),
    ("qwen3moe", 7),
    ("qwen3moe", 51),   # exactly the tensor count: metadata-only shard + one data shard
    ("qwen3moe", 100),  # more than the tensor count
    ("qwen3moe", 1),    # one tensor per shard, 52 files
    ("gemma4", 0),
    ("gemma4", 20),     # refused: split is qwen3moe-only
    ("gemma4", 1),      # refused
    ("gemma4", 5),      # refused
    ("qwen3moe", 13),
]


def run(script: Path, arch: str, split: int, out: Path, mode: str) -> tuple[int, str, str]:
    env = dict(os.environ)
    env["BMOE_GOOSE_FX"] = mode
    cmd = [sys.executable, str(script), "--arch", arch, "--out", str(out)]
    if split:
        cmd += ["--split-max-tensors", str(split)]
    p = subprocess.run(cmd, env=env, capture_output=True, text=True, check=False)
    # The banner prints the --out path, which differs per arm by design.
    return p.returncode, p.stdout, p.stderr


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def relative(path: Path, root: Path) -> str:
    """`out.gguf` or `out-00001-of-00004.gguf`, without the arm's directory."""
    return path.name


def main() -> int:
    t = Tally("FG663_FX")
    with tempfile.TemporaryDirectory() as td:
        base = Path(td)
        for arch, split in CASES:
            name = f"{arch}-split{split}"
            arms = {}
            for mode in ("0", "1"):
                arm = base / f"{name}-{mode}"
                arm.mkdir()
                out = arm / "tiny.gguf"
                rc, stdout, stderr = run(SCRIPT, arch, split, out, mode)
                # The banner names the --out path, which differs per arm by design.
                arms[mode] = (rc, stdout.replace(str(arm), "<arm>"), stderr, arm)
            off, on = arms["0"], arms["1"]
            t.rows += 1
            t.eq(t.rows, f"{name} exit", off[0], on[0])
            t.eq(t.rows, f"{name} stdout", off[1], on[1])
            t.eq(t.rows, f"{name} stderr", scrub(off[2]), scrub(on[2]))
            files_off = sorted(p.name for p in off[3].iterdir() if p.is_file())
            files_on = sorted(p.name for p in on[3].iterdir() if p.is_file())
            t.eq(t.rows, f"{name} files", files_off, files_on)
            for rel in files_off:
                if rel in files_on:
                    t.eq(t.rows, f"{name} {rel}", digest(off[3] / rel), digest(on[3] / rel))
    return t.done()


def scrub(text: str) -> str:
    """Drop the missing-host notice, which is the one legitimate difference."""
    return "\n".join(l for l in text.splitlines() if "goose-hosts/fx" not in l)


if __name__ == "__main__":
    raise SystemExit(main())
