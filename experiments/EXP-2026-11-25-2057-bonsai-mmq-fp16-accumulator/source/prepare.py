"""Generate tiled FP16 partial accumulators with FP32 final accumulators."""

from pathlib import Path
import sys


SUPPORT = """
static uint2 bonsai_mma_half_acc(uint2 acc, uint4 a, uint2 b) {
    uint lo=acc.x,hi=acc.y;
    __asm__("mma.sync.aligned.m16n8k16.row.col.f16.f16.f16.f16 "
            "{%0,%1}, {%2,%3,%4,%5}, {%6,%7}, {%0,%1};"
            : "+r"(lo),"+r"(hi)
            : "r"(a.x),"r"(a.y),"r"(a.z),"r"(a.w),"r"(b.x),"r"(b.y));
    return (uint2)(lo,hi);
}

static float bonsai_unpack_half(ushort x) {
    float out;
    __asm__("cvt.f32.f16 %0,%1;" : "=f"(out) : "h"(x));
    return out;
}

static float4 bonsai_widen_partial(uint2 acc) {
    return (float4)(bonsai_unpack_half((ushort)acc.x),bonsai_unpack_half((ushort)(acc.x>>16)),
                    bonsai_unpack_half((ushort)acc.y),bonsai_unpack_half((ushort)(acc.y>>16)));
}
"""


def variant(control, step_count):
    name = f"bonsai_pq2_cached_fp16_acc_k{step_count*16}"
    code = control.replace("bonsai_pq2_cached_64x128_k64", name)
    start = code.index("        #pragma unroll\n        for (uint k=0;k<32;k+=8)")
    finish = code.index("        sync_threads();", start)
    # Each partial segment starts from zero and every segment has a fixed length.
    loop = f"""        #pragma unroll
        for (uint segment=0;segment<32;segment+={step_count*8}) {{
            uint2 partial[8];
            #pragma unroll
            for (uint i=0;i<8;++i) partial[i]=(uint2)(0);
            #pragma unroll
            for (uint sub=0;sub<{step_count};++sub) {{
                const uint k=segment+8*sub;
                const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
                #pragma unroll
                for (uint i=0;i<8;++i) {{
                    const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                    partial[i]=bonsai_mma_half_acc(partial[i],a,b);
                }}
            }}
            #pragma unroll
            for (uint i=0;i<8;++i) acc[i]+=bonsai_widen_partial(partial[i]);
        }}
"""
    return code[:start] + loop + code[finish:]


if __name__ == "__main__":
    base, out = map(Path, sys.argv[1:])
    source = base.read_text()
    start = source.index("__kernel void bonsai_pq2_cached_64x128_k64(")
    end = source.index("\n}\n", start) + 3
    variants = [variant(source[start:end], count) for count in (2, 4)]
    out.write_text(source + "\n" + SUPPORT + "\n".join(variants))
