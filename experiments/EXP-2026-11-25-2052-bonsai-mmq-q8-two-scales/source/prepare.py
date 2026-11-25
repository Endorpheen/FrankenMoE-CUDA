"""Generate signed Q8 MMQ with a scale per 64 activation values."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one anchor: {old!r}")
    return text.replace(old, new)


def kernel(control, tile_tokens):
    count = tile_tokens // 16
    code = replace(control, "bonsai_pq2_mmq_int8(", f"bonsai_pq2_mmq_q8_64_n{tile_tokens}(")
    code = replace(code, "x[64][36]", f"x[{tile_tokens}][36]")
    code = replace(code, "token_scale[64]", f"token_scale[{tile_tokens}][2]")
    code = replace(code, "token_base=64*", f"token_base={tile_tokens}*")
    code = replace(code, "warp_token=32*", f"warp_token={tile_tokens // 2}*")
    code = replace(code, "float4 result[4];", f"float4 result[{count}];")
    code = replace(code, "i<4;++i", f"i<{count};++i")
    code = replace(code, "packed<64*32", f"packed<{tile_tokens}*32")
    code = replace(
        code,
        """        if (thread<64) {
            const uint t=token_base+thread, r=row_base+thread;
            token_scale[thread]=t<tokens ? scales[(ulong)t*blocks+block] : 0.0f;""",
        f"""        for (uint i=thread;i<{tile_tokens}*2;i+=256) {{
            const uint t=token_base+i/2;
            token_scale[i/2][i&1]=t<tokens ? scales[((ulong)t*blocks+block)*2+(i&1)] : 0.0f;
        }}
        if (thread<64) {{
            const uint r=row_base+thread;""",
    )
    start = code.index("        int4 dot[4];")
    end = code.index("        sync_threads();", start)
    code = code[:start] + f"""        const float wa=weight_scale[row],wb=weight_scale[row+8];
        #pragma unroll
        for (uint half_index=0;half_index<2;++half_index) {{
            int4 dot[{count}];
            #pragma unroll
            for (uint f=0;f<{count};++f) dot[f]=(int4)(0);
            #pragma unroll
            for (uint step=0;step<2;++step) {{
                const uint column=16*half_index+8*step;
                const uint4 a=bonsai_load_a(&w[warp_row][0],36,column,lane);
                #pragma unroll
                for (uint f=0;f<{count};++f) {{
                    const uint2 b=bonsai_load_b(&x[warp_token+8*f][0],36,column,lane);
                    dot[f]=bonsai_mma_i8(dot[f],a,b);
                }}
            }}
            #pragma unroll
            for (uint f=0;f<{count};++f) {{
                const uint t=token+8*f;
                const float xa=token_scale[t][half_index],xb=token_scale[t+1][half_index];
                const float4 scale=(float4)(wa*xa,wa*xb,wb*xa,wb*xb);
                const int4 d=dot[f];
                result[f]+=(float4)((float)d.x,(float)d.y,(float)d.z,(float)d.w)*scale;
            }}
        }}
""" + code[end:]
    return replace(code, "for (uint f=0;f<4;++f) bonsai_store_mma",
                   f"for (uint f=0;f<{count};++f) bonsai_store_mma")


if __name__ == "__main__":
    base, out = map(Path, sys.argv[1:])
    source = base.read_text()
    start = source.index("__kernel void bonsai_pq2_mmq_int8(")
    end = source.index("\n}\n", start) + 3
    variants = [kernel(source[start:end], tokens) for tokens in (64, 128)]
    out.write_text(source + "\n" + Path(__file__).with_name("pack.cl").read_text()
                   + "\n".join(variants))
