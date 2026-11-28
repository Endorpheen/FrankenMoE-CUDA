"""Embed validated little-endian SPIR-V words in the Vulkan binary."""

from pathlib import Path
import struct
import sys


def main():
    directory = Path(sys.argv[1])
    shaders = []
    entries = []
    for name in ("rmsnorm", "q4_0", "swiglu", "rope", "kv_store", "attention",
                 "q8_0", "gather_q4_0", "bias", "add", "pq2_0", "gather_pq2_0", "fwht",
                 "causal_conv", "qk_norm", "gdn", "gdn_gates", "gated_rms",
                 "head_slice", "sigmoid_gate", "kv_store_q4", "attention_q4",
                 "q4_k", "q6_k", "gather_q4_k", "iq4_nl", "iq4_xs", "gather_q8_0",
                 "f32_project", "moe_route", "moe_weighted", "apex_qk_norm", "gdn_apply", "sigmoid_gate_scalar",
                 "kv_fwht", "inkling_route", "inkling_situ", "inkling_attention"):
        code = (directory / f"{name}.spv").read_bytes()
        words = [word for word, in struct.iter_unpack("<I", code)]
        if len(words) < 5 or words[0] != 0x07230203:
            raise ValueError(f"{name}: invalid SPIR-V header")
        rows = [", ".join(f"0x{value:08x}u" for value in words[i:i + 8])
                for i in range(0, len(words), 8)]
        entries.append(f"    [FG_VK_{name.upper()}] = {{fg_{name}, sizeof(fg_{name})}},")
        shaders.append(f"static const uint32_t fg_{name}[] = {{\n    " + ",\n    ".join(rows) + "\n};\n")
    source = ['#include "graph.h"', *shaders,
              'static const struct { const uint32_t *data; uint64_t size; } table[FG_VK_OP_COUNT] = {',
              *entries, '};',
              'void fg_vk_shader_info(uint32_t op, uint64_t *output) {',
              '    output[0] = (uintptr_t)table[op].data;',
              '    output[1] = table[op].size;', '}']
    (directory / "shader-words.c").write_text("\n".join(source) + "\n")


if __name__ == "__main__":
    main()
