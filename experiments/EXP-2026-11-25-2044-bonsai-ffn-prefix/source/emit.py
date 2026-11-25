"""Generate matching FP16 and Q8 prefix kernels from the shared exact transform."""
from pathlib import Path
import sys


base, out = map(Path, sys.argv[1:])
control = base.read_text()
text = control.replace("bonsai_ffn_prefix_fp16", "bonsai_ffn_prefix_q8")
text = text.replace("__global uint *packed, float epsilon", "__global uint *packed, __global float *scales, float epsilon")
start = text.index("    for (uint i = thread; i < 2560; i += 256) {\n        packed")
text = text[:start] + """    const uint lane = thread % 32, warp = thread / 32;
    for (uint group = warp; group < 40; group += 8) {
        const __local float *x = cache + group * 128 + 4 * lane;
        const float4 v = (float4)(x[0],x[1],x[2],x[3]) * 0.03125f;
        float largest = as_float(as_uint(v.x) & 0x7fffffffu);
        largest = flash_max(largest, as_float(as_uint(v.y) & 0x7fffffffu));
        largest = flash_max(largest, as_float(as_uint(v.z) & 0x7fffffffu));
        largest = flash_max(largest, as_float(as_uint(v.w) & 0x7fffffffu));
        for (uint bit = 16; bit; bit /= 2) largest = flash_max(largest, flash_shuffle(largest, bit));
        const float scale = largest / 127.0f;
        const size_t packet = row / 128 + group;
        packed[packet * 32 + lane] = bonsai_quantize_q8_value(v.x,scale)
            | (bonsai_quantize_q8_value(v.y,scale) << 8)
            | (bonsai_quantize_q8_value(v.z,scale) << 16)
            | (bonsai_quantize_q8_value(v.w,scale) << 24);
        if (lane == 0) scales[packet] = scale;
    }
}
"""
out.write_text(control + "\n" + text)
