#!/usr/bin/env python3
"""mmap alignment decision of the loader in Goose (EXP403)."""
from pathlib import Path
L = Path(__file__).resolve().parent / 'src/llama-model-loader.cpp'
s = L.read_text()
DECL = ('// Goose loader alignment decision (EXP403). GGML_GOOSE_LOADER_ALIGN=0 restores the native test.\n'
        'extern "C" int fg403s_enabled(void);\n'
        'extern "C" int64_t fg403s_decide(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t * out2);\n\n')
anchor = 'extern "C" int fg201l_shape_check('
assert anchor in s and 'fg403s_enabled' not in s
s = s.replace(anchor, DECL + anchor, 1)
old = '        if (use_mmap && gguf_get_data_offset(metadata) % tensor_align != 0) {'
new = ('        const bool g_unaligned = fg403s_enabled()\n'
       '            ? fg403s_decide(0, use_mmap ? 1 : 0, (int64_t) gguf_get_data_offset(metadata),\n'
       '                            (int64_t) tensor_align, 0, 0, nullptr) == 0\n'
       '            : (use_mmap && gguf_get_data_offset(metadata) % tensor_align != 0);\n'
       '        if (g_unaligned) {')
assert old in s
s = s.replace(old, new, 1)
L.write_text(s)
print('loader alignment applied')
