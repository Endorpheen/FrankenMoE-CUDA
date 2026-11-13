#!/usr/bin/env bash
# Rebuild llama-server with the Goose request-path, sampler and graph decisions linked in.
# Everything under /tmp; the working tree is not modified.
set -euo pipefail
FLINT=/home/end0/AI/LlamaServer/llama.cpp-flint-cuda
WORK=/tmp/flint-goose
BUILD=/tmp/build-server
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"

rsync -a --exclude build-cuda --exclude build-server --exclude .git "$FLINT/" "$WORK/"

# 1. regenerate the Goose modules with the pinned compiler
for d in EXP-2026-11-12-250-goose-server-nremaining EXP-2026-11-12-251-goose-server-params \
         EXP-2026-11-12-252-goose-server-discard EXP-2026-11-12-253-goose-graph-gating \
         EXP-2026-11-12-254-goose-sampler-choice; do
    ( cd "$ROOT/experiments/$d" && python3 emit_sources.py >/dev/null )
done

# 2. place the sources where the build expects them
cp "$ROOT/experiments/EXP-2026-11-12-250-goose-server-nremaining/source/bridge.c"            "$WORK/tools/server/fg250s-bridge.c"
cp "$ROOT/experiments/EXP-2026-11-12-250-goose-server-nremaining/source/host-qualified.c"    "$WORK/tools/server/fg250s-host-qualified.c"
cp "$ROOT/experiments/EXP-2026-11-12-251-goose-server-params/source/bridge.cpp"              "$WORK/tools/server/fg251s-bridge.cpp"
cp "$ROOT/experiments/EXP-2026-11-12-251-goose-server-params/source/host-qualified.c"        "$WORK/tools/server/fg251s-host-qualified.c"
cp "$ROOT/experiments/EXP-2026-11-12-252-goose-server-discard/source/bridge.c"               "$WORK/tools/server/fg252s-bridge.c"
cp "$ROOT/experiments/EXP-2026-11-12-252-goose-server-discard/source/host-qualified.c"       "$WORK/tools/server/fg252s-host-qualified.c"
cp "$ROOT/experiments/EXP-2026-11-12-253-goose-graph-gating/source/bridge.c"                 "$WORK/src/fg253s-bridge.c"
cp "$ROOT/experiments/EXP-2026-11-12-253-goose-graph-gating/source/host-qualified.c"         "$WORK/src/fg253s-host-qualified.c"
cp "$ROOT/experiments/EXP-2026-11-12-254-goose-sampler-choice/source/bridge.c"               "$WORK/common/fg254s-bridge.c"
cp "$ROOT/experiments/EXP-2026-11-12-254-goose-sampler-choice/source/host-qualified.c"       "$WORK/common/fg254s-host-qualified.c"

# 2b. point each bridge at its own host unit name
python3 - "$WORK" <<'PYB'
import sys
from pathlib import Path
w = Path(sys.argv[1])
for br, hu in [('tools/server/fg250s-bridge.c','fg250s-host-qualified.c'),
               ('tools/server/fg252s-bridge.c','fg252s-host-qualified.c'),
               ('src/fg253s-bridge.c','fg253s-host-qualified.c'),
               ('common/fg254s-bridge.c','fg254s-host-qualified.c')]:
    p = w / br; t = p.read_text()
    if '#include "host-qualified.c"' in t:
        p.write_text(t.replace('#include "host-qualified.c"', f'#include "{hu}"'))
PYB

# 3. restore the integration edits (they live in the tree, applied idempotently here)
python3 - "$WORK" <<'PY'
import sys
from pathlib import Path
w = Path(sys.argv[1])

def patch(path, old, new, anchor_comment):
    p = w / path; s = p.read_text()
    if anchor_comment in s:
        return
    assert old in s, f'pattern missing in {path}'
    p.write_text(s.replace(old, new, 1))

patch('tools/server/server-context.cpp',
      '    int32_t n_remaining() const {\n        return n_predict_max == -1 ? -1 : n_predict_max - (int32_t) stats.n_gen;\n    }',
      '    int32_t n_remaining() const {\n        return (int32_t) fg250s_remaining(n_predict_max, (int64_t) stats.n_gen);\n    }',
      'fg250s_remaining')

patch('tools/server/server-context.cpp',
      '    bool has_budget() const {\n        return n_predict_max == -1 || n_remaining() > 0;\n    }',
      '    bool has_budget() const {\n        return fg250s_has_budget(n_predict_max, (int64_t) stats.n_gen) != 0;\n    }',
      'fg250s_has_budget(n_predict_max')

# 251: per-request n_predict bound
patch('tools/server/server-context.cpp',
      '        slot.n_predict_max = task.params.n_predict != -1 ? task.params.n_predict : params_base.n_predict;',
      '        {\n            int64_t fg251_out[2] = {0};\n            fg251s_apply(task.params.n_predict != -1 ? task.params.n_predict : params_base.n_predict,\n                         task.params.sampling.top_k, 0, fg251_out);\n            slot.n_predict_max = (int32_t) fg251_out[0];\n        }',
      'fg251s_apply(task.params')

# 252: context-shift amount
patch('tools/server/server-context.cpp',
      '                int       n_discard = slot.task->params.n_discard ? slot.task->params.n_discard : (n_left / 2);',
      '                int       n_discard = (int) fg252s_discard(n_keep, n_left, slot.task->params.n_discard);',
      'fg252s_discard(n_keep')

# 254: greedy vs distribution
patch('common/sampling.cpp',
      '            const bool greedy = params.n_probs == 0 && !params.samplers.empty() &&\n                ((params.samplers.back() == COMMON_SAMPLER_TYPE_TEMPERATURE && params.temp == 0.0f && params.dynatemp_range == 0.0f) ||\n                 (params.samplers.back() == COMMON_SAMPLER_TYPE_TOP_K && params.top_k == 1));',
      '            const int64_t fg254_last = params.samplers.empty() ? -1 :\n                (params.samplers.back() == COMMON_SAMPLER_TYPE_TEMPERATURE ? 0 :\n                 (params.samplers.back() == COMMON_SAMPLER_TYPE_TOP_K ? 1 : 2));\n            const bool greedy = fg254s_greedy(params.n_probs, (int64_t) params.samplers.size(),\n                                              fg254_last, params.top_k) != 0;',
      'fg254s_greedy(params.n_probs')
sp = w / 'common/sampling.cpp'; st = sp.read_text()
if 'extern "C" int64_t fg254s_greedy' not in st:
    st = st.replace('#include "sampling.h"', '#include "sampling.h"\nextern "C" int64_t fg254s_greedy(int64_t, int64_t, int64_t, int64_t);', 1)
    sp.write_text(st)


# 253: three graph decisions in llama-graph.cpp
g = w / 'src/llama-graph.cpp'
t = g.read_text()
if 'fg253s_probs' not in t:
    old_gating = "    ggml_tensor * probs = nullptr;\n    switch (gating_op) {"
    new_gating = "    ggml_tensor * probs = nullptr;\n    fg253s_set_ctx(ctx0);\n    if (true) { probs = (ggml_tensor *)(uintptr_t) fg253s_probs((int64_t)(uintptr_t) logits, (int64_t) gating_op); }\n    else switch (gating_op) {"
    assert old_gating in t, 'gating block missing'
    t = t.replace(old_gating, new_gating, 1)
    t = t.replace('#include "llama-graph.h"', '#include "llama-graph.h"\nextern "C" void fg253s_set_ctx(void *);\nextern "C" int64_t fg253s_probs(int64_t, int64_t);\nextern "C" int64_t fg253s_norm(int64_t, int64_t, int64_t);\nextern "C" void fg253s_topk(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t *);', 1)
if 'fg253s_norm' not in t:
    old_norm = "    if (norm_w) {\n        weights = ggml_reshape_2d(ctx0, weights, n_expert_used, n_tokens);\n\n        ggml_tensor * weights_sum = ggml_sum_rows(ctx0, weights); // [1, n_tokens]\n        cb(weights_sum, \"ffn_moe_weights_sum\", il);\n\n        // Avoid division by zero, clamp to smallest number representable by F16\n        weights_sum = ggml_clamp(ctx0, weights_sum, 6.103515625e-5, INFINITY);\n        cb(weights_sum, \"ffn_moe_weights_sum_clamped\", il);\n\n        weights = ggml_div(ctx0, weights, weights_sum); // [n_expert_used, n_tokens]\n        cb(weights, \"ffn_moe_weights_norm\", il);"
    new_norm = "    if (norm_w) {\n        fg253s_set_ctx(ctx0);\n        weights = (ggml_tensor *)(uintptr_t) fg253s_norm((int64_t)(uintptr_t) weights, n_expert_used, n_tokens);\n        cb(weights, \"ffn_moe_weights_norm\", il);"
    assert old_norm in t, 'norm block missing'
    t = t.replace(old_norm, new_norm, 1)
if 'fg253s_topk' not in t:
    old_sel = "        selected_experts = ggml_argsort_top_k(ctx0, selection_probs, n_expert_used); // [n_expert_used, n_tokens]\n        cb(selected_experts->src[0], \"ffn_moe_argsort\", il);"
    new_sel = "        fg253s_set_ctx(ctx0);\n        int64_t fg253_sel[1] = {0};\n        fg253s_topk((int64_t)(uintptr_t) selection_probs, (int64_t)(uintptr_t) probs,\n                    n_expert_used, n_expert, n_tokens, fg253_sel);\n        selected_experts = (ggml_tensor *)(uintptr_t) fg253_sel[0];\n        cb(selected_experts->src[0], \"ffn_moe_argsort\", il);"
    assert old_sel in t, 'selection block missing'
    t = t.replace(old_sel, new_sel, 1)
g.write_text(t)

sc = w / 'tools/server/server-context.cpp'; sct = sc.read_text()
if 'extern "C" int64_t fg250s_remaining' not in sct:
    sct = sct.replace('#include "server-context.h"',
        '#include "server-context.h"\nextern "C" int64_t fg250s_remaining(int64_t, int64_t);\nextern "C" int64_t fg250s_has_budget(int64_t, int64_t);\nextern "C" void fg251s_apply(int64_t, int64_t, int64_t, int64_t *);\nextern "C" int64_t fg252s_discard(int64_t, int64_t, int64_t);', 1)
    sc.write_text(sct)
for path, items, anchor in [
    ('tools/server/CMakeLists.txt', ['fg250s-bridge.c', 'fg251s-bridge.cpp', 'fg251s-host-qualified.c', 'fg252s-bridge.c'], 'server-context.cpp'),
    ('src/CMakeLists.txt', ['fg253s-bridge.c'], 'llama-graph.cpp'),
    ('common/CMakeLists.txt', ['fg254s-bridge.c'], 'sampling.cpp'),
]:
    cp_ = w / path; ct = cp_.read_text()
    missing = [i for i in items if i not in ct]
    if missing:
        ct = ct.replace(anchor, anchor + '\n    ' + '\n    '.join(missing), 1)
        cp_.write_text(ct)
hq = w / 'tools/server/fg251s-host-qualified.c'; ht = hq.read_text()
if 'fg251ds_run' not in ht:
    ht = ht.replace('int main(int argc, char **argv)', 'int fg251ds_generated_main(int argc, char **argv)')
    hq.write_text(ht.rstrip() + '\n\nuint8_t fg251ds_run(int64_t p){return fg251ds_gg_function_1(p,1);}\n')
print('server patches applied idempotently')
PY

cmake -S "$WORK" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DGGML_CUDA=ON \
      -DCMAKE_CUDA_COMPILER=/usr/bin/nvcc -DLLAMA_BUILD_TESTS=OFF \
      -DLLAMA_BUILD_EXAMPLES=OFF -DLLAMA_BUILD_SERVER=ON >/dev/null
ninja -C "$BUILD" -j 16 llama-server
echo "built: $BUILD/bin/llama-server"
