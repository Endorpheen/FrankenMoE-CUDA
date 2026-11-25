"""Append the measured E4M3 entries to the cumulative Bonsai PTX."""

from pathlib import Path
import sys


base, donor, out = map(Path, sys.argv[1:])
ptx = base.read_text()
fp8 = donor.read_text()
assert ptx.count(".version 8.3\n.target sm_80\n") == 1
assert ".version 8.4\n.target sm_89\n" in fp8
assert ".extern .shared .align 16 .b8 bonsai_flash_shared[];" in ptx
ptx = ptx.replace(".version 8.3\n.target sm_80\n", ".version 8.4\n.target sm_89\n")
entries = ("bonsai_flash_stage_k_fp8", "bonsai_flash_stage_v_only", "bonsai_flash_fp8_qk")
for name in entries:
    anchor = f".visible .entry {name}("
    assert anchor not in ptx and fp8.count(anchor) == 1
    first = fp8.index(anchor)
    last = fp8.index("\n}", first) + 2
    ptx += "\n\n" + fp8[first:last] + "\n"
out.write_text(ptx)
