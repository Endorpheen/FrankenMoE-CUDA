#!/usr/bin/env bash
# Builds the shipped sampler chain: compiles the module with a build root that
# keeps the entry point reachable, then emits the id header and the C shim.
# The language has no export, so the library reaches the chain through the shim.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
mod="$root/goose/sampling/sampler-chain-full.goose"
out="${1:-$root/build/sampler-chain}"
compiler="${GOOSE_COMPILER:-/home/end0/AI/Fried-Goose/.cache/goose/simd7}"
mkdir -p "$out"
rootfile="$out/sampler-chain-root.goose"
cp "$mod" "$rootfile"
cat >> "$rootfile" <<'EOF'

fn main() {
    let lv: f32[8] = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0];
    let p: f32[8] = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0];
    let id: i64[8] = [0, 1, 2, 3, 4, 5, 6, 7];
    let keep = fg_sampler_chain(0, lv, p, id, 8, 0.5, 0, 0.0, 0.0);
    if keep < 0 {
        return;
    }
}
EOF
GOOSE_HOME="$root/experiments/EXP-2026-11-18-1031-p6-10-typical" \
TMPDIR="$root/experiments/EXP-2026-11-18-1008-p1-12-gemv/tmp" \
    "$compiler" -o "$out/sampler_chain.c" "$rootfile"
python3 - "$out" <<'PY'
import re, sys
out = sys.argv[1]
src = open(out + '/sampler_chain.c').read()
m = re.search(r'static int64_t (gg_function_\d+)\(int64_t p0, gg_slice p1, gg_slice p2, gg_slice p3,'
              r' int64_t p4, float p5, int64_t p6, float p7, float p8, int64_t base\);', src)
if not m:
    sys.exit('entry point prototype not found in the generated C')
open(out + '/sampler_chain_ids.h', 'w').write('#define FG_SAMPLER_CHAIN %s\n' % m.group(1))
open(out + '/sampler_chain_shim.c', 'w').write(
    '#include "sampler_chain.c"\n#include "sampler_chain_ids.h"\n\n'
    '__attribute__((constructor)) static void fg_init(void) { gg_init_globals(0, 0); }\n\n'
    'int64_t fg_sampler_chain_call(int64_t op, const float * lv, float * p, int64_t * id,\n'
    '                              int64_t n, float f, int64_t k, float f2, float f3) {\n'
    '    gg_slice s0 = { (void *) lv, n }, s1 = { (void *) p, n }, s2 = { (void *) id, n };\n'
    '    return FG_SAMPLER_CHAIN(op, s0, s1, s2, n, f, k, f2, f3, 0);\n'
    '}\n')
print('entry point:', m.group(1))
PY
echo "built into $out"
