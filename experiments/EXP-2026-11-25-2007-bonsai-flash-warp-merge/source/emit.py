"""Generate ordered merge variants with independent loads before use."""
from pathlib import Path
import sys

src = Path(sys.argv[1]).read_text()
start = src.index("__kernel void bonsai_flash_compact_ordered_merge(")
end = src.index("static uint4 bonsai_load_a(", start)
function = src[start:end]
loop = """        for (uint i = 0; i < active; ++i) {
            const __global float *tile = part+(ulong)i*stride;
            const float2 input = *(__global const float2 *)(tile+2+2*thread);
            const float old = keep[i], weight = add[i];
            values = values*old + input*weight;
            if (thread == 0) sum = sum*old + tile[1]*weight;
        }"""
if function.count(loop) != 1:
    raise SystemExit("ordered merge loop changed")
for n in (2, 4, 8):
    full = f"""        const uint bulk = active & ~{n-1}u;
        for (uint i = 0; i < bulk; i += {n}) {{
            float2 input[{n}];
            #pragma unroll
            for (uint j = 0; j < {n}; ++j) {{
                const __global float *tile = part + (ulong)(i+j)*stride;
                input[j] = *(__global const float2 *)(tile+2+2*thread);
            }}
            #pragma unroll
            for (uint j = 0; j < {n}; ++j) {{
                const uint index = i+j;
                const float old = keep[index], weight = add[index];
                values = values*old + input[j]*weight;
                if (thread == 0) sum = sum*old + part[(ulong)index*stride+1]*weight;
            }}
        }}
"""
    full += loop.replace("uint i = 0;", "uint i = bulk;")
    print(function.replace("bonsai_flash_compact_ordered_merge", f"bonsai_flash_merge_prefetch{n}").replace(loop, full))
