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
    let rc0: i64[8] = [0, 0, 0, 0, 0, 0, 0, 0];
    let dryr = fg_sampler_dry(id, rc0, id, id, id, id, lv, id, 8, 8, 1, 0, 0.0, 1.0);
    if dryr < 0 {
        return;
    }
    let scr: f32[8] = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0];
    let typ = fg_sampler_typical(lv, p, id, scr, 8, 0.9, 1);
    if typ < 0 {
        return;
    }
    let topr = fg_sampler_top_p(lv, p, id, 8, 0.9, 1, 171);
    let xtcr = fg_sampler_xtc(lv, p, id, 8, 0.5, 0.1, 1, 0.0, 172);
    let tbl: i64[16] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    let penr = fg_sampler_penalties(lv, id, p, tbl, tbl, 8, 8, 1.0, 0.0, 0.0);
    let adr = fg_sampler_adaptive_p(lv, p, id, 8, 0.5, 0.0, 1.0, 174);
    let mir = fg_sampler_mirostat(p, 8, 1.0, 3, 100.0, 175);
    let dst = fg_sampler_dist(lv, 8, id, 176);
    let dsw = fg_sampler_dist_words(lv, 8, 1, 2, 177);
    let dpk = fg_sampler_dist_pick(lv, p, 8, 1, 2, 178);
    let tx = fg_sampler_temp_ext(lv, p, 8, 0.8, 0.4, 1.0, 179);
    let fset = fg_grammar_first_set(rc0, rc0, rc0, rc0, 1, rc0, rc0, 4, 181);
    if fset < -1 {
        return;
    }
    let gr = fg_grammar_reject(rc0, rc0, rc0, rc0, rc0, 1, rc0, rc0, rc0, rc0, rc0, 2,
                               rc0, rc0, rc0, rc0, rc0, rc0, rc0, rc0, rc0, 4, 4, 2, 180);
    if tx < 0 || gr < 0 {
        return;
    }
    if dpk < 0 {
        return;
    }
    if dsw < 0 {
        return;
    }
    if dst < 0 {
        return;
    }
    if adr < 0 || mir < 0 {
        return;
    }
    if penr < 0 {
        return;
    }
    if xtcr < 0 {
        return;
    }
    if topr < 0 {
        return;
    }
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

# Every entry point is identified by its exact prototype. The names never change;
# the generated indexes do, and the root file calls each one so none is dropped.
ENTRIES = [
    ("CHAIN",     r'\(int64_t p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t p4, float p5, int64_t p6, float p7, float p8, int64_t base\);'),
    ("DRY",       r'\(gg_slice p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, gg_slice p5, gg_slice p6, gg_slice p7, int64_t p8, int64_t p9, int64_t p10, int64_t p11, float p12, float p13, int64_t base\);'),
    ("TYPICAL",   r'\(gg_slice p0, gg_slice p1, gg_slice p2, gg_slice p3, int64_t p4, float p5, int64_t p6, int64_t base\);'),
    ("TOP_P",     r'\(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, float p4, int64_t p5, int64_t p6, int64_t base\);'),
    ("XTC",       r'\(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, float p4, float p5, int64_t p6, float p7, int64_t p8, int64_t base\);'),
    ("PENALTIES", r'\(gg_slice p0, gg_slice p1, gg_slice p2, gg_slice p3, gg_slice p4, int64_t p5, int64_t p6, float p7, float p8, float p9, int64_t base\);'),
    ("ADAPTIVE",  r'\(gg_slice p0, gg_slice p1, gg_slice p2, int64_t p3, float p4, float p5, float p6, int64_t p7, int64_t base\);'),
    ("MIROSTAT",  r'\(gg_slice p0, int64_t p1, float p2, int64_t p3, float p4, int64_t p5, int64_t base\);'),
    ("DIST",      r'\(gg_slice p0, int64_t p1, gg_slice p2, int64_t p3, int64_t base\);'),
    ("DIST_WORDS",r'\(gg_slice p0, int64_t p1, int64_t p2, int64_t p3, int64_t p4, int64_t base\);'),
    ("DIST_PICK", r'\(gg_slice p0, gg_slice p1, int64_t p2, int64_t p3, int64_t p4, int64_t p5, int64_t base\);'),
    ("TEMP_EXT",  r'\(gg_slice p0, gg_slice p1, int64_t p2, float p3, float p4, float p5, int64_t p6, int64_t base\);'),
]

found = {}
for name, pat in ENTRIES:
    m = re.search(r'static int64_t (gg_function_\d+)' + pat, src)
    if not m:
        sys.exit('prototype not found in the generated C: ' + name)
    found[name] = m.group(1)

hdr = ''.join('#define FG_SAMPLER_%s %s\n' % (k, v) for k, v in found.items())
open(out + '/sampler_chain_ids.h', 'w').write(hdr)

SHIM = """#include "sampler_chain.c"
#include "sampler_chain_ids.h"

__attribute__((constructor)) static void fg_init(void) { gg_init_globals(0, 0); }

int64_t fg_sampler_chain_call(int64_t op, const float * lv, float * p, int64_t * id,
                              int64_t n, float f, int64_t k, float f2, float f3) {
    gg_slice s0 = { (void *) lv, n }, s1 = { (void *) p, n }, s2 = { (void *) id, n };
    return FG_SAMPLER_CHAIN(op, s0, s1, s2, n, f, k, f2, f3, 0);
}

int64_t fg_sampler_dry_call(const int64_t * last, int64_t * rc, const int64_t * bh,
                            const int64_t * bts, const int64_t * btl, const int64_t * btail,
                            float * lv, const int64_t * id, int64_t nl, int64_t nc,
                            int64_t allowed, int64_t nb, float mult, float base) {
    gg_slice s0 = { (void *) last, nl }, s1 = { (void *) rc, nl }, s2 = { (void *) bh, nb };
    gg_slice s3 = { (void *) bts, nb }, s4 = { (void *) btl, nb }, s5 = { (void *) btail, nb * 4 };
    gg_slice s6 = { (void *) lv, nc }, s7 = { (void *) id, nc };
    return FG_SAMPLER_DRY(s0, s1, s2, s3, s4, s5, s6, s7, nl, nc, allowed, nb, mult, base, 0);
}

int64_t fg_sampler_typical_call(float * lv, float * p, int64_t * id, float * scr,
                                int64_t n, float typical_p, int64_t min_keep) {
    gg_slice s0 = { (void *) lv, n }, s1 = { (void *) p, n };
    gg_slice s2 = { (void *) id, n }, s3 = { (void *) scr, n };
    return FG_SAMPLER_TYPICAL(s0, s1, s2, s3, n, typical_p, min_keep, 0);
}

int64_t fg_sampler_top_p_call(float * lv, float * p, int64_t * id, int64_t n,
                              float top_p, int64_t min_keep) {
    gg_slice s0 = { (void *) lv, n }, s1 = { (void *) p, n }, s2 = { (void *) id, n };
    return FG_SAMPLER_TOP_P(s0, s1, s2, n, top_p, min_keep, 171, 0);
}

int64_t fg_sampler_xtc_call(float * lv, float * p, int64_t * id, int64_t n,
                            float probability, float threshold, int64_t min_keep, float chance) {
    gg_slice s0 = { (void *) lv, n }, s1 = { (void *) p, n }, s2 = { (void *) id, n };
    return FG_SAMPLER_XTC(s0, s1, s2, n, probability, threshold, min_keep, chance, 172, 0);
}

int64_t fg_sampler_penalties_call(float * lv, int64_t * id, const float * hist, int64_t n,
                                  int64_t hn, float repeat, float freq, float present,
                                  int64_t * tbl_keys, int64_t * tbl_cnt, int64_t slots) {
    gg_slice s0 = { (void *) lv, n }, s1 = { (void *) id, n }, s2 = { (void *) hist, hn };
    gg_slice s3 = { (void *) tbl_keys, slots }, s4 = { (void *) tbl_cnt, slots };
    return FG_SAMPLER_PENALTIES(s0, s1, s2, s3, s4, n, hn, repeat, freq, present, 0);
}

int64_t fg_sampler_adaptive_p_call(float * lv, float * p, int64_t * id, int64_t n,
                                   float target, float weighted_sum, float total_weight) {
    gg_slice s0 = { (void *) lv, n }, s1 = { (void *) p, n }, s2 = { (void *) id, n };
    return FG_SAMPLER_ADAPTIVE(s0, s1, s2, n, target, weighted_sum, total_weight, 174, 0);
}

int64_t fg_sampler_mirostat_call(float * p, int64_t n, float mu, int64_t m, float n_vocab) {
    gg_slice s0 = { (void *) p, n };
    return FG_SAMPLER_MIROSTAT(s0, n, mu, m, n_vocab, 175, 0);
}

int64_t fg_sampler_dist_call(float * p, int64_t n, int64_t * state) {
    gg_slice s0 = { (void *) p, n }, s1 = { (void *) state, 625 };
    return FG_SAMPLER_DIST(s0, n, s1, 176, 0);
}

int64_t fg_sampler_dist_words_call(float * p, int64_t n, int64_t w1, int64_t w2) {
    gg_slice s0 = { (void *) p, n };
    return FG_SAMPLER_DIST_WORDS(s0, n, w1, w2, 177, 0);
}

int64_t fg_sampler_dist_pick_call(float * lv, float * p, int64_t n, int64_t w1, int64_t w2) {
    gg_slice s0 = { (void *) lv, n }, s1 = { (void *) p, n };
    return FG_SAMPLER_DIST_PICK(s0, s1, n, w1, w2, 178, 0);
}

int64_t fg_sampler_temp_ext_call(float * lv, float * p, int64_t n, float temp, float delta,
                                 float exponent) {
    gg_slice s0 = { (void *) lv, n }, s1 = { (void *) p, n };
    return FG_SAMPLER_TEMP_EXT(s0, s1, n, temp, delta, exponent, 179, 0);
}
"""
open(out + '/sampler_chain_shim.c', 'w').write(SHIM)
print('entry points:', found)
PY
python3 "$root/tools/gen-grammar-shim.py" "$out"
echo "built into $out"
