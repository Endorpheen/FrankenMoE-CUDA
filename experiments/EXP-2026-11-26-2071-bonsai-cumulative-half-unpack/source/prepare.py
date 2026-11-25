"""Combine previously verified half unpack methods with cumulative MMQ."""

from pathlib import Path
import sys


base, out = map(Path, sys.argv[1:])
source = base.read_text()
experiments = Path(__file__).resolve().parents[2]
half2 = (experiments / "EXP-2026-11-25-2022-bonsai-pq2-half2/source/unpack.cl").read_text()
start = half2.index("__kernel void bonsai_pq2_validate_unpack")
half2 = half2[:start]
variants = []
for width in (5120, 6144, 17408):
    entry = f"bonsai_pq2_half_full_{width}"
    start = source.index(f"__kernel void {entry}(")
    end = source.index("\n}\n", start) + 3
    code = source[start:end]
    call = "bonsai_ffn_unpack_cached((ushort)entry,codes)"
    assert code.count(call) == 1
    for name, routine in (("lut", "bonsai_pq2_quad"), ("half2", "bonsai_pq2_half2")):
        variants.append(code.replace(entry, f"bonsai_pq2_half_unpack_{name}_{width}")
                        .replace(call, f"{routine}((ushort)entry,codes)"))
out.write_text(source + "\n" + half2 + "\n".join(variants))
