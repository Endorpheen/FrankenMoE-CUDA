"""Generate K32 staging without changing EXP-2059 MMA or rounding order."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one anchor: {old!r}")
    return text.replace(old, new)


def variant(control, tokens):
    fragments = tokens // 16
    code = control.replace("bonsai_pq2_cached_fp16_acc_k32", f"bonsai_pq2_half_stage32_n{tokens}")
    code = replace(code, "uint w[64][36];", "uint w[64][20];")
    code = replace(code, "uint x[128][36];", f"uint x[{tokens}][20];")
    code = replace(code, "token_base=128*flash_grid_y()", f"token_base={tokens}*flash_grid_y()")
    code = replace(code, "warp_token=64*(warp%2)", f"warp_token={tokens // 2}*(warp%2)")
    code = replace(code, "float4 acc[8];", f"float4 acc[{fragments}];")
    code = replace(code, "for (uint i=0;i<8;++i) acc[i]=(float4)(0.0f);",
                   f"for (uint i=0;i<{fragments};++i) acc[i]=(float4)(0.0f);")

    start = code.index("    uint raw[4];")
    stop = code.index("        sync_threads();\n    }", start)
    code = code[:start] + f"""    for (uint step=0;step<4*blocks;++step) {{
        for (uint packed=thread*4;packed<{tokens}*16;packed+=1024) {{
            const uint token=token_base+packed/16,col=packed%16;
            const uint valid=token<tokens ? 16 : 0;
            const ulong source=valid ? (ulong)token*(width/2)+step*16+col : 0;
            bonsai_copy16(&x[packed/16][col],input+source,valid);
        }}
        #pragma unroll
        for (uint load=0;load<2;++load) {{
            const uint index=thread+256*load;
            const uint row=row_base+index/8,quad=index%8;
            uint2 pair=(uint2)(0);
            if (row<rows) {{
                const __global uchar *pq2=weights+((ulong)row*blocks+step/4)*34;
                const ushort scale=*(__global const ushort *)pq2;
                pair=bonsai_ffn_unpack_cached(scale,pq2[2+8*(step&3)+quad]);
            }}
            w[index/8][quad*2]=pair.x;
            w[index/8][quad*2+1]=pair.y;
        }}
        bonsai_copy_complete();
        sync_threads();
        uint2 partial[{fragments}];
        #pragma unroll
        for (uint i=0;i<{fragments};++i) partial[i]=(uint2)(0);
        #pragma unroll
        for (uint k=0;k<16;k+=8) {{
            const uint4 a=bonsai_load_a(&w[warp_row][0],20,k,lane);
            #pragma unroll
            for (uint i=0;i<{fragments};++i) {{
                const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],20,k,lane);
                partial[i]=bonsai_mma_half_acc(partial[i],a,b);
            }}
        }}
        #pragma unroll
        for (uint i=0;i<{fragments};++i) acc[i]+=bonsai_widen_partial(partial[i]);
""" + code[stop:]
    return replace(code, "for (uint i=0;i<8;++i) bonsai_store_mma",
                   f"for (uint i=0;i<{fragments};++i) bonsai_store_mma")


if __name__ == "__main__":
    base, out = map(Path, sys.argv[1:])
    source = base.read_text()
    start = source.index("__kernel void bonsai_pq2_cached_fp16_acc_k32(")
    end = source.index("\n}\n", start) + 3
    variants = [variant(source[start:end], tokens) for tokens in (64, 128)]
    out.write_text(source + "\n" + "\n".join(variants))
