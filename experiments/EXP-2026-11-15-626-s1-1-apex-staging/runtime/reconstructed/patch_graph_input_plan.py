#!/usr/bin/env python3
"""Graph input planning of build_inp_embd in Goose (EXP408)."""
from pathlib import Path
G = Path(__file__).resolve().parent / 'src/llama-graph.cpp'
s = G.read_text()
DECL = ('// Goose graph input planning (EXP408). GGML_GOOSE_GRAPH_INPUT_PLAN=0 restores the native expressions.\n'
        'extern "C" int fg408s_enabled(void);\n'
        'extern "C" int64_t fg408s_decide(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t * out2);\n\n')
anchor = 'ggml_tensor * llm_graph_context::build_inp_embd('
assert anchor in s and 'fg408s_enabled' not in s
s = s.replace(anchor, DECL + anchor, 1)

old = '    const bool has_mixed = llm_arch_supports_mixed_batch(arch) && cparams.ctx_type == LLAMA_CONTEXT_TYPE_DEFAULT;'
new = ('    const bool has_mixed = fg408s_enabled()\n'
       '        ? fg408s_decide(0, llm_arch_supports_mixed_batch(arch) ? 1 : 0,\n'
       '                        cparams.ctx_type == LLAMA_CONTEXT_TYPE_DEFAULT ? 1 : 0, 0, 0, 0, nullptr) != 0\n'
       '        : (llm_arch_supports_mixed_batch(arch) && cparams.ctx_type == LLAMA_CONTEXT_TYPE_DEFAULT);')
assert old in s
s = s.replace(old, new, 1)

old = '    const int64_t n_tok_rows = has_mixed ? llm_graph_n_tok_rows(ubatch) : 0;'
new = ('    const int64_t n_tok_rows = fg408s_enabled()\n'
       '        ? fg408s_decide(1, has_mixed ? 1 : 0, has_mixed ? llm_graph_n_tok_rows(ubatch) : 0, 0, 0, 0, nullptr)\n'
       '        : (has_mixed ? llm_graph_n_tok_rows(ubatch) : 0);')
assert old in s
s = s.replace(old, new, 1)
G.write_text(s)
print('graph input plan applied')
