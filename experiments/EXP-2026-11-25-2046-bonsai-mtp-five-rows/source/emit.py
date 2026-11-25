"""Specialize exact five-row PQ2, convolution and grouped Q4 attention."""
from pathlib import Path
import sys


mvq, attention, conv, output = map(Path, sys.argv[1:])
q8 = mvq.read_text().replace("mvq_three", "mvq_five").replace("mvq_q8_three_", "mvq_q8_five_")
q8 = q8.replace("MVQ_VERIFY", "MVQ_VERIFY_FIVE").replace("[3]", "[5]").replace("t < 3", "t < 5")
q8 = q8.replace("{0, 0, 0}", "{0, 0, 0, 0, 0}")
conv_text = conv.read_text().replace("prefix_four", "prefix_five")
conv_text = conv_text.replace("__global float *third)", "__global float *third, __global float *fourth)")
conv_text = conv_text.replace("{first, second, third, final}", "{first, second, third, fourth, final}")
conv_text = conv_text.replace("t < 4", "t < 5")
tail = attention.read_text()
a = tail.index("static inline __attribute__((always_inline)) void verify_grouped_tail(")
b = tail.index("__kernel void bonsai_verify_q4_tail", a)
tail = tail[a:b]


def spec(name, rows, heads):
    text = tail.replace("verify_grouped_tail", name)
    text = text.replace("[3][2]", f"[{rows}][{heads}]").replace("t < 3", f"t < {rows}")
    return text.replace("first_keys + 2", f"first_keys + {rows - 1}")


parts = [q8, conv_text, spec("five_tile_one", 1, 3), spec("five_tile_pair", 2, 3),
         spec("five_tile_triple", 3, 2)]
parts.append("""
static inline uint bonsai_verify_five_group(void) {
    uint group;
    __asm__("mov.u32 %0, %%ctaid.z;" : "=r"(group));
    return group;
}

__kernel void bonsai_verify_five_221(__global const float *q, __global const uchar *k,
    __global const uchar *v, __global float *scratch, uint keys, uint max_tiles, float scale) {
    const uint group = bonsai_verify_five_group(), row = group * 2;
    q += row * 6144;
    scratch += (size_t)row * 24 * max_tiles * 258;
    if (group < 2) {
        five_tile_pair(q, k, v, scratch, keys + row, max_tiles, scale, 3);
    } else {
        five_tile_one(q, k, v, scratch, keys + row, max_tiles, scale, 3);
    }
}

__kernel void bonsai_verify_five_32(__global const float *q, __global const uchar *k,
    __global const uchar *v, __global float *scratch, uint keys, uint max_tiles, float scale) {
    if (bonsai_verify_five_group() == 0) {
        five_tile_triple(q, k, v, scratch, keys, max_tiles, scale, 2);
    } else if (work_group() < 8) {
        five_tile_pair(q + 3 * 6144, k, v, scratch + (size_t)3 * 24 * max_tiles * 258,
                       keys + 3, max_tiles, scale, 3);
    }
}
""")
output.write_text("\n".join(parts))
