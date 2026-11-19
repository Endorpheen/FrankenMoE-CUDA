#!/usr/bin/env bash
# Builds the shipped sampler chain: compiles the module with a build root that
# keeps the entry point reachable, then emits the id header and the C shim.
# Entry points are `export fn`, so the generated C carries real names and the library does
# not depend on generated indexes. Generated with --header, which also gives the host
# `goose_init` and omits `main`.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
mod="$root/goose/sampling/sampler-chain-full.goose"
out="${1:-$root/build/sampler-chain}"
compiler="${GOOSE_COMPILER:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}"
mkdir -p "$out"
rootfile="$out/sampler-chain-root.goose"
# The speculative doors join the same unit: one build, one shim, and a host that can call both
# the sampler chain and the speculative decisions by name.
cp "$mod" "$rootfile"
cat "$root/spec_decisions" >> "$rootfile" 2>/dev/null || true
cat "$root/goose/speculative/spec-decisions.goose" >> "$rootfile"
cat "$root/goose/speculative/spec-doors.goose" >> "$rootfile"
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
    let typ2 = fg_typical_entry(lv, p, id, scr, 8, 0.9, 1);
    if typ2 < 0 {
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
    let cmp = fg_sp_compatible(1, 1, 1, 1, 2, 100, 1, 1, 1, 1, 2, 100, 128);
    if cmp < -1 {
        return;
    }
    let drl = fg_dry_rep_limit_entry(rc0, rc0, rc0, rc0, rc0, 1, 1);
    if drl < -1 {
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
    "$compiler" --standalone --header "$out/sampler_chain.h" -o "$out/sampler_chain.c" "$rootfile"
python3 - "$out" <<'PY'
import re, sys
out = sys.argv[1]
src = open(out + '/sampler_chain.c').read()

# Every entry point is identified by its exact prototype. The names never change;
# the generated indexes do, and the root file calls each one so none is dropped.
ENTRIES = [
    ("CHAIN",      "fg_sampler_chain"),
    ("DRY",        "fg_sampler_dry"),
    ("TYPICAL",    "fg_typical_entry"),
    ("TOP_P",      "fg_sampler_top_p"),
    ("XTC",        "fg_sampler_xtc"),
    ("PENALTIES",  "fg_sampler_penalties"),
    ("ADAPTIVE",   "fg_sampler_adaptive_p"),
    ("MIROSTAT",   "fg_sampler_mirostat"),
    ("DIST",       "fg_sampler_dist"),
    ("DIST_WORDS", "fg_sampler_dist_words"),
    ("DIST_PICK",  "fg_sampler_dist_pick"),
    ("TEMP_EXT",   "fg_sampler_temp_ext"),
]

found = {}
for name, sym in ENTRIES:
    if not re.search(r'^int64_t ' + sym + r'\(', src, re.M):
        sys.exit('exported entry point not found in the generated C: ' + sym)
    found[name] = sym

hdr = ''.join('#define FG_SAMPLER_%s %s\n' % (k, v) for k, v in found.items())
open(out + '/sampler_chain_ids.h', 'w').write(hdr)

SHIM = """#include "sampler_chain.h"
#include "sampler_chain_ids.h"

// One initialisation for the whole library; the exported doors panic without it.
__attribute__((constructor)) static void fg_init(void) { goose_init(0, 0); }

int64_t fg_sampler_chain_call(int64_t op, const float * lv, float * p, int64_t * id,
                              int64_t n, float f, int64_t k, float f2, float f3) {
    sl_f32 s0 = { (float *) lv, n }, s1 = { p, n };
    sl_i64 s2 = { id, n };
    return FG_SAMPLER_CHAIN(op, s0, s1, s2, n, f, k, f2, f3);
}

int64_t fg_sampler_dry_call(const int64_t * last, int64_t * rc, const int64_t * bh,
                            const int64_t * bts, const int64_t * btl, const int64_t * btail,
                            float * lv, const int64_t * id, int64_t nl, int64_t nc,
                            int64_t allowed, int64_t nb, float mult, float base) {
    sl_i64 s0 = { (int64_t *) last, nl }, s1 = { rc, nl }, s2 = { (int64_t *) bh, nb };
    sl_i64 s3 = { (int64_t *) bts, nb }, s4 = { (int64_t *) btl, nb }, s5 = { (int64_t *) btail, nb * 4 };
    sl_f32 s6 = { lv, nc };
    sl_i64 s7 = { (int64_t *) id, nc };
    return FG_SAMPLER_DRY(s0, s1, s2, s3, s4, s5, s6, s7, nl, nc, allowed, nb, mult, base);
}

int64_t fg_sampler_typical_call(float * lv, float * p, int64_t * id, float * scr,
                                int64_t n, float typical_p, int64_t min_keep) {
    sl_f32 s0 = { lv, n }, s1 = { p, n }, s3 = { scr, n };
    sl_i64 s2 = { id, n };
    return FG_SAMPLER_TYPICAL(s0, s1, s2, s3, n, typical_p, min_keep);
}

int64_t fg_sampler_top_p_call(float * lv, float * p, int64_t * id, int64_t n,
                              float top_p, int64_t min_keep) {
    sl_f32 s0 = { lv, n }, s1 = { p, n };
    sl_i64 s2 = { id, n };
    return FG_SAMPLER_TOP_P(s0, s1, s2, n, top_p, min_keep, 171);
}

int64_t fg_sampler_xtc_call(float * lv, float * p, int64_t * id, int64_t n,
                            float probability, float threshold, int64_t min_keep, float chance) {
    sl_f32 s0 = { lv, n }, s1 = { p, n };
    sl_i64 s2 = { id, n };
    return FG_SAMPLER_XTC(s0, s1, s2, n, probability, threshold, min_keep, chance, 172);
}

int64_t fg_sampler_penalties_call(float * lv, int64_t * id, const float * hist, int64_t n,
                                  int64_t hn, float repeat, float freq, float present,
                                  int64_t * tbl_keys, int64_t * tbl_cnt, int64_t slots) {
    sl_f32 s0 = { lv, n }, s2 = { (float *) hist, hn };
    sl_i64 s1 = { id, n }, s3 = { tbl_keys, slots }, s4 = { tbl_cnt, slots };
    return FG_SAMPLER_PENALTIES(s0, s1, s2, s3, s4, n, hn, repeat, freq, present);
}

int64_t fg_sampler_adaptive_p_call(float * lv, float * p, int64_t * id, int64_t n,
                                   float target, float weighted_sum, float total_weight) {
    sl_f32 s0 = { lv, n }, s1 = { p, n };
    sl_i64 s2 = { id, n };
    return FG_SAMPLER_ADAPTIVE(s0, s1, s2, n, target, weighted_sum, total_weight, 174);
}

int64_t fg_sampler_mirostat_call(float * p, int64_t n, float mu, int64_t m, float n_vocab) {
    sl_f32 s0 = { p, n };
    return FG_SAMPLER_MIROSTAT(s0, n, mu, m, n_vocab, 175);
}

int64_t fg_sampler_dist_call(float * p, int64_t n, int64_t * state) {
    sl_f32 s0 = { p, n };
    sl_i64 s1 = { state, 625 };
    return FG_SAMPLER_DIST(s0, n, s1, 176);
}

int64_t fg_sampler_dist_words_call(float * p, int64_t n, int64_t w1, int64_t w2) {
    sl_f32 s0 = { p, n };
    return FG_SAMPLER_DIST_WORDS(s0, n, w1, w2, 177);
}

int64_t fg_sampler_dist_pick_call(float * lv, float * p, int64_t n, int64_t w1, int64_t w2) {
    sl_f32 s0 = { lv, n }, s1 = { p, n };
    return FG_SAMPLER_DIST_PICK(s0, s1, n, w1, w2, 178);
}

int64_t fg_sampler_temp_ext_call(float * lv, float * p, int64_t n, float temp, float delta,
                                 float exponent) {
    sl_f32 s0 = { lv, n }, s1 = { p, n };
    return FG_SAMPLER_TEMP_EXT(s0, s1, n, temp, delta, exponent, 179);
}
"""
open(out + '/sampler_chain_shim.c', 'w').write(SHIM)
print('entry points:', found)
PY
python3 "$root/tools/gen-grammar-shim.py" "$out"
echo "built into $out"
