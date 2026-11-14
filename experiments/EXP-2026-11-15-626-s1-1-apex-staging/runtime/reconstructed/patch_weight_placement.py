#!/usr/bin/env python3
"""Weight placement decisions of the loader in Goose (EXP404)."""
from pathlib import Path
L = Path(__file__).resolve().parent / 'src/llama-model-loader.cpp'
s = L.read_text()
DECL = ('// Goose weight placement (EXP404). GGML_GOOSE_WEIGHT_PLACEMENT=0 restores the native walk.\n'
        'extern "C" int fg404s_enabled(void);\n'
        'extern "C" int64_t fg404s_decide(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t * out2);\n\n')
anchor = 'extern "C" int fg201l_shape_check('
assert anchor in s and 'fg404s_enabled' not in s
s = s.replace(anchor, DECL + anchor, 1)
old = '''    GGML_ASSERT(!buft_list->empty());
    for (const auto & cur : *buft_list) {
        ggml_backend_dev_t cur_dev = cur.first;
        ggml_backend_buffer_type_t cur_buft = cur.second;
        if (weight_buft_supported(hparams, tensor, op, cur_buft, cur_dev)) {
            return cur_buft;
        }
    }

    return nullptr;'''
new = '''    GGML_ASSERT(!buft_list->empty());

    if (fg404s_enabled() ? fg404s_decide(0, op == GGML_OP_NONE ? 1 : 0, 0, 0, 0, 0, nullptr) != 0
                         : op == GGML_OP_NONE) {
        // no operation to run, so the first buffer is taken without probing
        return buft_list->front().second;
    }

    int64_t g_idx = 0;
    for (const auto & cur : *buft_list) {
        ggml_backend_dev_t cur_dev = cur.first;
        ggml_backend_buffer_type_t cur_buft = cur.second;
        const bool sup = weight_buft_supported(hparams, tensor, op, cur_buft, cur_dev);
        const int64_t chosen = fg404s_enabled()
            ? fg404s_decide(1, sup ? 1 : 0, g_idx, 0, 0, 0, nullptr) : (sup ? g_idx : -1);
        if (chosen >= 0) {
            return cur_buft;
        }
        ++g_idx;
    }

    return nullptr;'''
assert old in s
s = s.replace(old, new, 1)
L.write_text(s)
print('weight placement applied')
