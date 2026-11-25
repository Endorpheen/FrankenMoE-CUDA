"""Generate exact four-query tiles with alternate query/head grouping."""
from pathlib import Path
import sys


base, out = map(Path, sys.argv[1:])
source = base.read_text().split("#define VERIFY_ATTN", maxsplit=1)[0]


def specialize(symbol, rows, heads, pair):
    text = source
    for old, new in (
        ("verify_grouped_tiles", symbol + "_impl"),
        ("[3][2]", f"[{rows}][{heads}]"),
        ("t < 3", f"t < {rows}"),
        ("first_keys + 2", f"first_keys + {rows - 1}"),
    ):
        assert old in text
        text = text.replace(old, new)
    if pair:
        anchor = "    const uint lane = local_id()"
        assert text.count(anchor) == 1
        text = text.replace(anchor, """    uint pair;
    __asm__("mov.u32 %0, %%ctaid.z;" : "=r"(pair));
    const uint first_query = pair * 2;
    q += first_query * 6144;
    scratch += (size_t)first_query * 24 * max_tiles * 258;
    first_keys += first_query;
""" + anchor)
    start = text.index("    for (uint row = first; row < last; ++row)")
    end = text.index("\n\n    #pragma unroll", start)
    loop = text[start:end]
    hot = loop.replace("row < last", "row < common_end")
    hot = "\n".join(line for line in hot.splitlines()
                    if "if (row >= first_keys + t) continue;" not in line)
    tail = loop.replace("uint row = first;", "uint row = common_end > first ? common_end : first;")
    text = text[:start] + "    const uint common_end = last < first_keys ? last : first_keys;\n" + hot + "\n" + tail + text[end:]
    return text + f"""
__kernel void {symbol}(__global const float *q, __global const uchar *k,
    __global const uchar *v, __global float *scratch, uint keys, uint max_tiles, float scale) {{
    {symbol}_impl(q, k, v, scratch, keys, max_tiles, scale, {heads});
}}
"""


out.write_text("\n".join(specialize("bonsai_verify_" + name, rows, heads, pair)
    for name, rows, heads, pair in (
        ("four_tail2", 4, 2, False), ("pair_tail2", 2, 2, True), ("pair_tail3", 2, 3, True),
    )))
