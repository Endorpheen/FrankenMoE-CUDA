"""Limit Bonsai FP16 partial accumulation to the FFN down projection."""

from pathlib import Path
import sys


base, out = map(Path, sys.argv[1:])
host = base.read_text()
anchor = "            const int partial_half = k64 && m->mmq_cached && m->mmq_half_acc;"
assert host.count(anchor) == 1
out.write_text(host.replace(anchor, """            const int partial_half = k64 && m->mmq_cached && m->mmq_half_acc &&
                in_width == 17408 && out_width == 5120 && p[2] == (CUdeviceptr)m->b[18];"""))
