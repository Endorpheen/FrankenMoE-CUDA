"""Generate cumulative FP16-partial tiles with B reused across two row groups."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one anchor: {old!r}")
    return text.replace(old, new)


def variant(source, width):
    name = f"bonsai_pq2_half_full_{width}"
    start = source.index(f"__kernel void {name}(")
    end = source.index("\n}\n", start) + 3
    code = source[start:end].replace(name, f"bonsai_pq2_half_row_reuse_{width}")
    code = replace(code, "warp_row=16*(warp/2),warp_token=64*(warp%2)",
                   "warp_row=32*(warp/4),warp_token=32*(warp%4)")
    code = replace(
        code,
        """                const uint4 a=bonsai_load_a(&w[warp_row][0],36,k,lane);
                #pragma unroll
                for (uint i=0;i<8;++i) {
                    const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                    partial[i]=bonsai_mma_half_acc(partial[i],a,b);
                }""",
        """                const uint4 a0=bonsai_load_a(&w[warp_row][0],36,k,lane);
                const uint4 a1=bonsai_load_a(&w[warp_row+16][0],36,k,lane);
                #pragma unroll
                for (uint i=0;i<4;++i) {
                    const uint2 b=bonsai_load_b(&x[warp_token+8*i][0],36,k,lane);
                    partial[i]=bonsai_mma_half_acc(partial[i],a0,b);
                    partial[i+4]=bonsai_mma_half_acc(partial[i+4],a1,b);
                }""",
    )
    return replace(
        code,
        """    for (uint i=0;i<8;++i) {
        const ulong offset=(ulong)(token+8*i)*rows+row;
        output[offset]=acc[i].x;
        output[offset+8]=acc[i].z;
        output[offset+rows]=acc[i].y;
        output[offset+rows+8]=acc[i].w;
    }""",
        """    for (uint i=0;i<4;++i) {
        const ulong offset=(ulong)(token+8*i)*rows+row;
        output[offset]=acc[i].x;
        output[offset+8]=acc[i].z;
        output[offset+rows]=acc[i].y;
        output[offset+rows+8]=acc[i].w;
        output[offset+16]=acc[i+4].x;
        output[offset+24]=acc[i+4].z;
        output[offset+rows+16]=acc[i+4].y;
        output[offset+rows+24]=acc[i+4].w;
    }""",
    )


if __name__ == "__main__":
    base, out = map(Path, sys.argv[1:])
    source = base.read_text()
    out.write_text(source + "\n".join(variant(source, width) for width in (5120, 6144, 17408)))
