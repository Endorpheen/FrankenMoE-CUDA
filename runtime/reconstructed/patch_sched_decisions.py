#!/usr/bin/env python3
"""Scheduler decisions of llama-context in Goose (EXP405)."""
from pathlib import Path
C = Path(__file__).resolve().parent / 'src/llama-context.cpp'
s = C.read_text()
DECL = ('// Goose scheduler decisions (EXP405). GGML_GOOSE_SCHED_DECISIONS=0 restores the native tests.\n'
        'extern "C" int fg405s_enabled(void);\n'
        'extern "C" int64_t fg405s_decide(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t * out2);\n\n')
anchor = '// Goose fused-op resolution (EXP391).'
assert anchor in s and 'fg405s_enabled' not in s
s = s.replace(anchor, DECL + anchor, 1)

old = '''    if (!sched_need_reserve) {
        return;
    }'''
new = '''    if (fg405s_enabled() ? fg405s_decide(0, sched_need_reserve ? 1 : 0, 0, 0, 0, 0, nullptr) == 0
                         : !sched_need_reserve) {
        return;
    }'''
assert old in s
s = s.replace(old, new, 1)

old = '    const uint32_t n_tokens = std::min(cparams.n_ctx, cparams.n_ubatch);'
new = ('    const uint32_t n_tokens = fg405s_enabled()\n'
       '        ? (uint32_t) fg405s_decide(1, (int64_t) cparams.n_ctx, (int64_t) cparams.n_ubatch, 0, 0, 0, nullptr)\n'
       '        : std::min(cparams.n_ctx, cparams.n_ubatch);')
assert old in s
s = s.replace(old, new, 1)

old = '''    if (n_queued_tokens == 1) {'''
new = '''    const int64_t g_stats = fg405s_enabled()
        ? fg405s_decide(2, (int64_t) n_queued_tokens, 0, 0, 0, 0, nullptr) : (n_queued_tokens == 1 ? 1 : (n_queued_tokens > 1 ? 2 : 0));
    if (fg405s_enabled() ? g_stats == 1 : n_queued_tokens == 1) {'''
assert old in s
s = s.replace(old, new, 1)
C.write_text(s)
print('scheduler decisions applied')
