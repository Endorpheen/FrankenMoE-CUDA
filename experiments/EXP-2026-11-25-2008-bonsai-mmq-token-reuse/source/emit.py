"""Instantiate K64 tiles that reuse each staged weight across more tokens."""
from pathlib import Path
import sys

src = Path(sys.argv[1]).read_text()
prefix = "__kernel void bonsai_pq2_mmq_64x128_k64("
start = src.index(prefix) + len(prefix)
end = src.index("\n}\n", start)
body = src[start:end]

for m, n, w in ((64, 192, 8), (64, 256, 8), (32, 256, 8), (64, 256, 16)):
    column_warps = w // (m // 16)
    fragments = n // (column_warps * 8)
    assert n == fragments * column_warps * 8
    tile = body
    for old, new in (
        ("uint w[64][36]", f"uint w[{m}][36]"),
        ("uint x[128][36]", f"uint x[{n}][36]"),
        ("row_base=64*", f"row_base={m}*"),
        ("token_base=128*", f"token_base={n}*"),
        ("warp_row=16*(warp/2),warp_token=64*(warp%2)",
         f"warp_row=16*(warp/{column_warps}),warp_token={fragments*8}*(warp%{column_warps})"),
        ("acc[8]", f"acc[{fragments}]"),
        ("i<8;++i", f"i<{fragments};++i"),
        ("packed<128*32;packed+=1024", f"packed<{n}*32;packed+={128*w}"),
        ("index<64*16;index+=256", f"index<{m}*16;index+={32*w}"),
    ):
        assert old in tile, old
        tile = tile.replace(old, new)
    print(f"__kernel void bonsai_mmq_reuse_{m}x{n}_w{w}({tile}\n}}")
