"""Generate the Q8 main-product and packed Q4 residual matrix path."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one anchor: {old!r}")
    return text.replace(old, new)


base, out = map(Path, sys.argv[1:])
source = base.read_text()
start = source.index("__kernel void bonsai_pq2_mmq_int8(")
end = source.index("\n}\n", start) + 3
tile = source[start:end]
tile = replace(tile, "bonsai_pq2_mmq_int8(", "bonsai_pq2_mmq_q8_q4_residual(")
tile = replace(tile, "__global const uint *input,", "__global const uint *input, __global const uint *correction,")
tile = replace(tile, "    __local float weight_scale", """    __local __attribute__((aligned(16))) uint wlow[64][20],xlow[64][20];
    __local float weight_scale""")
tile = replace(tile, "        if (thread<64) {", """        for (uint packed=4*thread;packed<64*16;packed+=1024) {
            const uint t=token_base+packed/16;
            const uint size=t<tokens ? 16 : 0;
            const ulong from=size ? ((ulong)t*blocks+block)*16+packed%16 : 0;
            bonsai_copy16(&xlow[packed/16][packed%16],correction+from,size);
        }
        if (thread<64) {""")
tile = replace(tile, "            w[i/32][i%32]=bonsai_expand_pq2_byte(code);", """            const uint expanded=bonsai_expand_pq2_byte(code);
            w[i/32][i%32]=expanded;
            if (!(i&1)) {
                const uint next=r<rows ? weights[((ulong)r*blocks+block)*34+3+i%32] : 0x55u;
                wlow[i/32][(i%32)/2]=bonsai_narrow_four(expanded) |
                    (bonsai_narrow_four(bonsai_expand_pq2_byte(next))<<16);
            }""")
tile = replace(tile, "        const float wa=weight_scale[row],wb=weight_scale[row+8];", """        int4 low[4];
        #pragma unroll
        for (uint f=0;f<4;++f) low[f]=(int4)(0);
        #pragma unroll
        for (uint k=0;k<16;k+=8) {
            const uint4 a=bonsai_load_a(&wlow[warp_row][0],20,k,lane);
            #pragma unroll
            for (uint f=0;f<4;++f) {
                const uint2 b=bonsai_load_b(&xlow[warp_token+8*f][0],20,k,lane);
                low[f]=bonsai_mma_i4(low[f],a,b);
            }
        }
        const float wa=weight_scale[row],wb=weight_scale[row+8];""")
tile = replace(tile, "            result[f]+=(float4)((float)d.x,(float)d.y,(float)d.z,(float)d.w)*scale;", """            const int4 l=low[f];
            const float4 hi=(float4)((float)d.x,(float)d.y,(float)d.z,(float)d.w);
            const float4 lo=(float4)((float)l.x,(float)l.y,(float)l.z,(float)l.w);
            result[f]+=(hi+lo*(1.0f/14.0f))*scale;""")
experiments = Path(__file__).resolve().parents[2]
residual = experiments / "EXP-2026-11-25-1996-bonsai-pq2-q8-residual/source/residual.cl"
out.write_text(source + "\n" + residual.read_text() + "\n"
               + Path(__file__).with_name("residual.cl").read_text() + "\n" + tile)
