"""Restrict the two-scale gate/up path to layers 32 through 47."""

from pathlib import Path
import subprocess
import sys


base, out = map(Path, sys.argv[1:])
experiments = Path(__file__).resolve().parents[2]
source = experiments / "EXP-2026-11-25-2052-bonsai-mmq-q8-two-scales/source/quality-host.py"
subprocess.run([sys.executable, source, base, out], check=True)

host = out.read_text()
anchor = "            if (m->q8_64 && (ffn_gate || ffn_up) && 2*scale_bytes <= m->partial_capacity) {"
assert host.count(anchor) == 1
patched = """            if (m->q8_64 && (ffn_gate || ffn_up) && 2*scale_bytes <= m->partial_capacity &&
                    m->ffn_layer > 32 && m->ffn_layer <= 48) {"""
out.write_text(host.replace(anchor, patched))
