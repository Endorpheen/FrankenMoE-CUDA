"""Generate exact warp-softmax and causal-tail specializations from EXP-2033."""
from pathlib import Path
import sys


def replace(text, old, new):
    assert text.count(old) == 1, old
    return text.replace(old, new)


base, out = map(Path, sys.argv[1:])
source = base.read_text().split("#define VERIFY_ATTN", maxsplit=1)[0]
broadcast = """static inline float bonsai_softmax_broadcast(float value, uint lane) {
    float result;
    __asm__("shfl.sync.idx.b32 %0, %1, %2, 31, -1;" : "=f"(result) : "f"(value), "r"(lane));
    return result;
}
"""
lane = replace(source, ", maximum[3][2], mass[3][2];",
               ";\n    float maximum = as_float(0xff800000u), mass = 0.0f;")
lane = replace(lane, """            maximum[t][h] = as_float(0xff800000u);
            mass[t][h] = 0.0f;
""", "")
a = lane.index("        float previous[3][2]")
b = lane.index("        #pragma unroll\n        for (uint b", a)
lane = lane[:a] + """        float assigned = 0.0f;
        #pragma unroll
        for (uint t = 0; t < 3; ++t) {
            if (row >= first_keys + t) continue;
            #pragma unroll
            for (uint h = 0; h < heads; ++h) {
                const float score = bonsai_decode_warp_sum(dot[t][h]) * scale + 0.0f;
                if (lane == t * heads + h) assigned = score;
            }
        }
        float keep = 0.0f, add = 0.0f;
        if (lane < 3 * heads && row < first_keys + lane / heads) {
            const float next = assigned > maximum ? assigned : maximum;
            keep = attention_exp(maximum - next);
            add = attention_exp(assigned - next);
            mass = mass * keep + add;
            maximum = next;
        }
        float previous[3][2], weight[3][2];
        #pragma unroll
        for (uint t = 0; t < 3; ++t) {
            #pragma unroll
            for (uint h = 0; h < heads; ++h) {
                const uint owner = t * heads + h;
                previous[t][h] = bonsai_softmax_broadcast(keep, owner);
                weight[t][h] = bonsai_softmax_broadcast(add, owner);
            }
        }
""" + lane[b:]
lane = replace(lane, """            if (lane == 0) {
                out[0] = maximum[t][h];
                out[1] = mass[t][h];
            }""", """            if (lane == t * heads + h) {
                out[0] = maximum;
                out[1] = mass;
            }""")


def split_tail(text):
    start = text.index("    for (uint row = first; row < last; ++row)")
    end = text.index("\n\n    #pragma unroll", start)
    loop = text[start:end]
    hot = loop.replace("row < last", "row < common_end")
    hot = "\n".join(line for line in hot.splitlines() if "if (row >= first_keys + t) continue;" not in line)
    hot = hot.replace("lane < 3 * heads && row < first_keys + lane / heads", "lane < 3 * heads")
    tail = loop.replace("uint row = first;", "uint row = common_end > first ? common_end : first;")
    return text[:start] + """    const uint common_end = last < first_keys ? last : first_keys;
""" + hot + "\n" + tail + text[end:]


variants = {"lane": lane, "tail": split_tail(source), "lane_tail": split_tail(lane)}
parts = [broadcast]
for name, text in variants.items():
    text = replace(text, "void verify_grouped_tiles(", f"void verify_grouped_{name}(")
    parts.append(text + f"""
__kernel void bonsai_verify_q4_{name}(__global const float *q, __global const uchar *k,
    __global const uchar *v, __global float *scratch, uint keys, uint max_tiles, float scale) {{
    verify_grouped_{name}(q, k, v, scratch, keys, max_tiles, scale, 2);
}}
""")
out.write_text("\n".join(parts))
