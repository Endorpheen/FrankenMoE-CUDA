#!/usr/bin/env python3
"""Transfer policy of the context in Goose (EXP406)."""
from pathlib import Path
C = Path(__file__).resolve().parent / 'src/llama-context.cpp'
s = C.read_text()
DECL = ('// Goose transfer policy (EXP406). GGML_GOOSE_TRANSFER_POLICY=0 restores the native calls.\n'
        'extern "C" int fg406s_enabled(void);\n'
        'extern "C" int64_t fg406s_decide(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t * out2);\n\n')
anchor = '// Goose scheduler decisions (EXP405).'
assert anchor in s and 'fg406s_enabled' not in s
s = s.replace(anchor, DECL + anchor, 1)

old = '        ggml_backend_tensor_get_async(backend_res, t_logits, logits.data, 0, n_tokens*n_vocab*sizeof(float));'
new = ('        if (fg406s_enabled() ? fg406s_decide(0, (int64_t) (n_tokens*n_vocab*sizeof(float)), 0, 0, 0, 0, nullptr) != 0 : true) {\n'
       '            ggml_backend_tensor_get_async(backend_res, t_logits, logits.data, 0, n_tokens*n_vocab*sizeof(float));\n'
       '        } else {\n'
       '            ggml_backend_tensor_get(t_logits, logits.data, 0, n_tokens*n_vocab*sizeof(float));\n'
       '        }')
assert old in s
s = s.replace(old, new, 1)

old = '                    ggml_backend_tensor_get_async(backend_embd, t_embd, embd.data, 0, n_tokens*n_embd_out*sizeof(float));'
new = ('                    if (fg406s_enabled() ? fg406s_decide(0, (int64_t) (n_tokens*n_embd_out*sizeof(float)), 0, 0, 0, 0, nullptr) != 0 : true) {\n'
       '                        ggml_backend_tensor_get_async(backend_embd, t_embd, embd.data, 0, n_tokens*n_embd_out*sizeof(float));\n'
       '                    } else {\n'
       '                        ggml_backend_tensor_get(t_embd, embd.data, 0, n_tokens*n_embd_out*sizeof(float));\n'
       '                    }')
assert old in s
s = s.replace(old, new, 1)
C.write_text(s)
print('transfer policy applied')
