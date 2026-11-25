"""Expose independent QK work while preserving ordered decode softmax."""
from pathlib import Path
import sys

s = Path(sys.argv[1]).read_text()
start = s.index("static inline __attribute__((always_inline)) void grouped_tiles(")
end = s.index("#define GROUPED", start)
gpu = s[start:end].rstrip()
begin = gpu.index("    for (uint row = first; row < last; ++row)")
stop = gpu.index("\n    #pragma unroll\n    for (uint h", begin)
old = gpu[begin:stop]
fold = old[old.index("        float previous[3]"):old.rindex("\n    }")]
assert fold.count("warp_sum(dot[h])") == 1
assert fold.count("q4_value(vr + 18 * b, lane)") == 1

for heads, block, eager_v in ((2, 2, 0), (3, 2, 0), (3, 4, 0), (2, 2, 1), (3, 2, 1)):
    tiles = f"""    for (uint row = first; row < last; row += {block}) {{
        float dots[{block}][{heads}] = {{0}};
"""
    if eager_v:
        tiles += f"        float values[{block}][8];\n"
    tiles += f"""        #pragma unroll
        for (uint b = 0; b < 8; ++b) {{
            #pragma unroll
            for (uint j = 0; j < {block}; ++j) {{
                const uint key_row = row+j < last ? row+j : last-1;
                const size_t byte = (size_t)key_row * 576 + kv * 144 + 18 * b;
                const float key = q4_value(k + byte, lane);
"""
    if eager_v:
        tiles += "                values[j][b] = q4_value(v + byte, lane);\n"
    tiles += """                #pragma unroll
                for (uint h = 0; h < heads; ++h) {
                    float next;
                    __asm__("fma.rn.f32 %0, %1, %2, %3;" : "=f"(next)
                        : "f"(query[h][b]), "f"(key), "f"(dots[j][h]));
                    dots[j][h] = next;
                }
            }
        }
"""
    tiles += f"""        #pragma unroll
        for (uint j=0; j < {block}; ++j) {{
            if (row+j >= last) break;
"""
    if not eager_v:
        tiles += "            const __global uchar *vr=v+(size_t)(row+j)*576+kv*144;\n"
    body = fold.replace("dot[h]", "dots[j][h]")
    if eager_v:
        body = body.replace("q4_value(vr + 18 * b, lane)", "values[j][b]")
    tiles += "\n".join("    " + line if line else "" for line in body.splitlines()) + "\n        }\n    }\n"
    kernel = gpu.replace(old,tiles)
    name = f"bonsai_gqa_h{heads}_k{block}_v{eager_v}"
    kernel = kernel.replace("static inline __attribute__((always_inline)) void grouped_tiles(", f"__kernel void {name}(")
    kernel = kernel.replace("float scale, uint heads) {", f"float scale) {{\n    const uint heads = {heads};")
    print(kernel)
