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
print('server patches applied idempotently')
PY

cmake -S "$WORK" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DGGML_CUDA=ON \
      -DCMAKE_CUDA_COMPILER=/usr/bin/nvcc -DLLAMA_BUILD_TESTS=OFF \
      -DLLAMA_BUILD_EXAMPLES=OFF -DLLAMA_BUILD_SERVER=ON >/dev/null
ninja -C "$BUILD" -j 16 llama-server
echo "built: $BUILD/bin/llama-server"
