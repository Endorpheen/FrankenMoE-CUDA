"""Reuse the PQ2 tile loader with separate arithmetic per 32-value group."""
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text()
needle = "__kernel void bonsai_pq2_mmq_int8("
start = source.index(needle) + len(needle)
end = source.index("\n}\n", start)
original = source[start:end]

for tokens in (64, 128):
    fragments = tokens // 16
    src = original
    for a, b in (
        ("x[64][36]", f"x[{tokens}][36]"),
        ("token_scale[64]", f"token_scale[{tokens}][4]"),
        ("token_base=64*", f"token_base={tokens}*"),
        ("warp_token=32*", f"warp_token={tokens // 2}*"),
        ("result[4]", f"result[{fragments}]"),
        ("i<4;++i", f"i<{fragments};++i"),
        ("packed<64*32", f"packed<{tokens}*32"),
        ("""        if (thread<64) {
            const uint t=token_base+thread, r=row_base+thread;
            token_scale[thread]=t<tokens ? scales[(ulong)t*blocks+block] : 0.0f;""",
         f"""        for (uint i=thread; i<{tokens}*4; i+=256) {{
            const uint t=token_base+i/4;
            token_scale[i/4][i%4]=t<tokens ? scales[((ulong)t*blocks+block)*4+i%4] : 0.0f;
        }}
        if (thread<64) {{
            const uint r=row_base+thread;"""),
    ):
        assert a in src, a
        src = src.replace(a, b)
    first = src.index("        int4 dot[4];")
    last = src.index("        sync_threads();", first)
    src = src[:first] + f"""        const float wa=weight_scale[row],wb=weight_scale[row+8];
        #pragma unroll
        for (uint k=0;k<32;k+=8) {{
            const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
            #pragma unroll
            for (uint f=0;f<{fragments};++f) {{
                const uint t=token+8*f;
                const uint2 b=bonsai_load_b(&x[warp_token+8*f][0],36,k,lane);
                const int4 dot=bonsai_mma_i8((int4)(0),a,b);
                const float xa=token_scale[t][k/8],xb=token_scale[t+1][k/8];
                const float4 scale=(float4)(wa*xa,wa*xb,wb*xa,wb*xb);
                result[f]+=(float4)((float)dot.x,(float)dot.y,(float)dot.z,(float)dot.w)*scale;
            }}
        }}
""" + src[last:]
    src = src.replace("f<4;++f", f"f<{fragments};++f")
    print(f"__kernel void bonsai_pq2_mmq_d4_{tokens}({src}\n}}")
