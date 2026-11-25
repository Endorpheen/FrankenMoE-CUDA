"""Generate partitioned K64 MMQ and an ordered FP32 combine."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one anchor: {old!r}")
    return text.replace(old, new)


base, out = map(Path, sys.argv[1:])
source = base.read_text()
start = source.index("__kernel void bonsai_pq2_cached_64x128_k64(")
end = source.index("\n}\n", start) + 3
part = source[start:end]
part = replace(part, "bonsai_pq2_cached_64x128_k64", "bonsai_pq2_cached_split_k")
part = replace(part, "uint width,uint rows,uint tokens)", "uint width,uint rows,uint tokens,uint parts)")
part = replace(part, "    const uint blocks=width/128;", """    const uint blocks=width/128;
    uint split;
    __asm__("mov.u32 %0, %%ctaid.z;" : "=r"(split));
    const uint first_block=(blocks*split)/parts, end_block=(blocks*(split+1))/parts;
    output+=(ulong)split*rows*tokens;""")
part = replace(part, "for (uint step=0;step<2*blocks;++step)",
               "for (uint step=2*first_block;step<2*end_block;++step)")
merge = """
__kernel void bonsai_pq2_sum_split_k(__global const float *partial, __global float *output,
                                     uint values,uint parts) {
    const uint i=work_group()*256+local_id();
    if (i>=values) return;
    float sum=partial[i];
    for (uint part=1;part<parts;++part) sum+=partial[(ulong)part*values+i];
    output[i]=sum;
}
"""
out.write_text(source + "\n" + part + merge)
