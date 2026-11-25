"""Generate packed-fragment variants with a shorter live register window."""
from pathlib import Path
import sys


template = Path(sys.argv[1]).read_text()
parts = [template]
for name, tile_rows, tile_tokens, stage_words in (
    ("k64_loop", 64, 128, 32),
    ("k128_loop", 64, 128, 64),
    ("k128_n64", 64, 64, 64),
    ("k128_m32", 32, 128, 64),
):
    helper = template[
        template.index("static inline __attribute__((always_inline)) void bonsai_pq2_compressed_fragment("):
        template.index("#define PQ2_COMPRESSED")]
    warps_along_token = 8 // (tile_rows // 16)
    accumulators = tile_tokens // (8 * warps_along_token)
    for old, new in (
        ("bonsai_pq2_compressed_fragment", "bonsai_pq2_compressed_" + name),
        ("64 * work_group()", f"{tile_rows} * work_group()"),
        ("128 * flash_grid_y()", f"{tile_tokens} * flash_grid_y()"),
        ("16 * (warp / 2), warp_token = 64 * (warp % 2)",
         f"16 * (warp / {warps_along_token}), warp_token = {accumulators * 8} * (warp % {warps_along_token})"),
        ("part < 4", f"part < {tile_rows // 16}"),
        ("128 * stage_words", f"{tile_tokens} * stage_words"),
        ("acc[8]", f"acc[{accumulators}]"),
        ("i < 8", f"i < {accumulators}"),
        ("#pragma unroll\n            for (uint k", "#pragma unroll 1\n            for (uint k"),
    ):
        assert old in helper, old
        helper = helper.replace(old, new)
    parts.append(helper + f"""
__kernel void bonsai_pq2_fragment_{name}(__global const uchar *weights,
    __global const uint *input, __global float *output, uint width, uint rows, uint tokens) {{
    __local __attribute__((aligned(16))) ushort w[{tile_rows}][18];
    __local __attribute__((aligned(16))) uint x[{tile_tokens}][{stage_words + 4}];
    bonsai_pq2_compressed_{name}(weights, input, output, width, rows, tokens,
        {stage_words}, &w[0][0], &x[0][0]);
}}
""")
Path(sys.argv[2]).write_text("\n".join(parts))
