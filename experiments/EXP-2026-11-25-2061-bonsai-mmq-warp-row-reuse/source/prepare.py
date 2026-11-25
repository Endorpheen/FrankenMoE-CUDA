"""Generate an exact cached K64 tile that shares B fragments across row panels."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one anchor: {old!r}")
    return text.replace(old, new)


base, out = map(Path, sys.argv[1:])
source = base.read_text()
name = "bonsai_pq2_cached_64x128_k64"
start = source.index(f"__kernel void {name}(")
end = source.index("\n}\n", start) + 3
candidate = source[start:end].replace(name, "bonsai_pq2_cached_64x128_row_reuse")
candidate = replace(candidate, "warp_row=16*(warp/2),warp_token=64*(warp%2)",
                    "warp_row=32*(warp/4),warp_token=32*(warp%4)")
candidate = replace(candidate, """            const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
            #pragma unroll
            for (uint i=0;i<8;++i) {
                const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                acc[i]=bonsai_mma16x8(acc[i],a.x,a.y,a.z,a.w,b.x,b.y);
            }""", """            const uint4 a0=bonsai_load_a(&w[warp_row][0],36,k,lane);
            const uint4 a1=bonsai_load_a(&w[warp_row+16][0],36,k,lane);
            #pragma unroll
            for (uint i=0;i<4;++i) {
                const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                acc[i]=bonsai_mma16x8(acc[i],a0.x,a0.y,a0.z,a0.w,b.x,b.y);
                acc[i+4]=bonsai_mma16x8(acc[i+4],a1.x,a1.y,a1.z,a1.w,b.x,b.y);
            }""")
candidate = replace(candidate,
    "    for (uint i=0;i<8;++i) bonsai_store_mma(output,acc[i],rows,tokens,row,token+8*i);",
    """    for (uint i=0;i<4;++i) {
        bonsai_store_mma(output,acc[i],rows,tokens,row,token+8*i);
        bonsai_store_mma(output,acc[i+4],rows,tokens,row+16,token+8*i);
    }""")
out.write_text(source + "\n" + candidate)
