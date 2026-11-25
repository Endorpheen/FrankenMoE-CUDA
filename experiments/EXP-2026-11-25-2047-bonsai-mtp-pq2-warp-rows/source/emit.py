"""Generate exact PQ2 warp shapes and a shift-based weight unpack alternative."""
from pathlib import Path
import sys


base, output = map(Path, sys.argv[1:])
template = base.read_text().split("#define MVQ_VERIFY", maxsplit=1)[0]
old = """                const uint even = mvq_permute(map, map, code & 0x3333);
                const uint odd = mvq_permute(map, map, (code >> 2) & 0x3333);
                const uint w0 = mvq_permute(even, odd, 0x5140);
                const uint w1 = mvq_permute(even, odd, 0x7362);"""
assert template.count(old) == 1

parts = ["""static inline uint bonsai_pq2_spread_byte(uint x) {
    x = (x | (x << 12)) & 0x000f000fu;
    x = (x | (x << 6)) & 0x03030303u;
    return ((x | 0x80808080u) - 0x01010101u) ^ 0x80808080u;
}

__kernel void bonsai_pq2_spread_check(__global uint *result) {
    const uint code = work_group() * 256 + local_id();
    result[code] = bonsai_pq2_spread_byte(code);
}
"""]
for width in (3, 4, 5):
    for unpack in ("permutation", "spread"):
        label = f"{width}_{unpack}"
        text = template.replace("mvq_three", "mvq_tile_" + label)
        text = text.replace("values[2][3]", f"values[4][{width}]")
        text = text.replace("[3]", f"[{width}]").replace("t < 3", f"t < {width}")
        text = text.replace("{0, 0, 0}", "{" + ", ".join(["0"] * width) + "}")
        if unpack == "spread":
            text = text.replace("            const uint map = 0x020100ff;\n", "")
            text = text.replace(old, """                const uint w0 = bonsai_pq2_spread_byte(code & 255u);
                const uint w1 = bonsai_pq2_spread_byte(code >> 8);""")
        parts.append(text)
        for group in (1, 2, 4):
            parts.append(f"""
__kernel void bonsai_mvq_{label}_{group}(__global const uchar *weights,
    __global const uchar *input, __global float *output, uint width, uint rows) {{
    mvq_tile_{label}(weights, input, output, width, rows, {group});
}}
""")
output.write_text("\n".join(parts))
