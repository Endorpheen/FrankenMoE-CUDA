"""Generate full Q8 tiles that reconstruct the exact int32 dot from INT4 digits."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one anchor: {old!r}")
    return text.replace(old, new)


base, out = map(Path, sys.argv[1:])
source = base.read_text()
entry = "bonsai_pq2_mmq_int8_full_n128"
begin = source.index(f"__kernel void {entry}(")
end = source.index("\n}\n", begin) + 3
tile = source[begin:end].replace(entry, "bonsai_pq2_mmq_q8_digits_n128")
tile = replace(tile, "__global const uint *input,",
               "__global const uint *upper, __global const uint *lower,")
tile = replace(tile, "uint w[64][36],x[128][36];",
               "uint w[64][20],hi[128][20],lo[128][20];")
begin = tile.index("        for (uint packed=4*thread;")
end = tile.index("        if (thread<128)", begin)
tile = tile[:begin] + """        for (uint packed=4*thread;packed<128*16;packed+=1024) {
            const uint t=token_base+packed/16;
            const ulong from=((ulong)t*blocks+block)*16+packed%16;
            bonsai_copy16(&hi[packed/16][packed%16],upper+from,16);
            bonsai_copy16(&lo[packed/16][packed%16],lower+from,16);
        }
""" + tile[end:]
begin = tile.index("        for (uint i=thread;i<64*32")
end = tile.index("        bonsai_copy_complete();", begin)
tile = tile[:begin] + """        for (uint i=thread;i<64*16;i+=256) {
            const uint row=row_base+i/16;
            const ulong offset=((ulong)row*blocks+block)*34+2+2*(i%16);
            const uint a=bonsai_expand_pq2_byte(weights[offset]);
            const uint b=bonsai_expand_pq2_byte(weights[offset+1]);
            w[i/16][i%16]=bonsai_low_nibbles(a)|(bonsai_low_nibbles(b)<<16);
        }
""" + tile[end:]
begin = tile.index("        int4 dot[8];")
end = tile.index("        const float wa=", begin)
tile = tile[:begin] + """        int4 dot_hi[8],dot_lo[8];
        #pragma unroll
        for (uint f=0;f<8;++f) {
            dot_hi[f]=(int4)(0);
            dot_lo[f]=(int4)(0);
        }
        #pragma unroll
        for (uint k=0;k<16;k+=8) {
            const uint4 a=bonsai_load_a(&w[warp_row][0],20,k,lane);
            #pragma unroll
            for (uint f=0;f<8;++f) {
                const uint2 b0=bonsai_load_b(&hi[warp_token+8*f][0],20,k,lane);
                const uint2 b1=bonsai_load_b(&lo[warp_token+8*f][0],20,k,lane);
                dot_hi[f]=bonsai_mma_digit_signed(dot_hi[f],a,b0);
                dot_lo[f]=bonsai_mma_digit_unsigned(dot_lo[f],a,b1);
            }
        }
""" + tile[end:]
tile = replace(tile, "const int4 d=dot[f];", "const int4 d=16*dot_hi[f]+dot_lo[f];")
out.write_text(source + "\n" + Path(__file__).with_name("digits.cl").read_text() + "\n" + tile)
