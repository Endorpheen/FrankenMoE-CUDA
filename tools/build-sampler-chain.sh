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
# The detokenizer port joins the unit so the shipped library can reach it: 337 lines of ported logic that
# were built only in the separate main unit and called by nothing (EXP-2026-11-19-1314).
# Its dependency closure, found by the compiler: unicode codepoint helpers and the tokenizer
# module that owns BT_UNKNOWN and the bt_* helpers the detokenizer calls.
cat "$root/goose/misc/unicode-tables.goose" >> "$rootfile"
cat "$root/goose/misc/unicode-text.goose" >> "$rootfile"
cat "$root/goose/misc/pretokenizer-qwen35.goose" >> "$rootfile"
cat "$root/goose/model/bpe-tokenize.goose" >> "$rootfile"
cat "$root/goose/model/bpe-detokenize.goose" >> "$rootfile"
# The GGUF reader joins the unit for the same reason the detokenizer did: it was built in the separate
# main unit and reached by nothing (EXP-2026-11-19-1324).
cat "$root/goose/misc/gguf-headers.goose" >> "$rootfile"
cat "$root/goose/model/gguf-tensors.goose" >> "$rootfile"
cat "$root/goose/model/gguf-reader.goose" >> "$rootfile"
cat "$root/goose/model/tokenizer-decisions.goose" >> "$rootfile"
cat "$root/goose/model/tokenizer-doors.goose" >> "$rootfile"
cat "$root/goose/cli/fit-params-line.goose" >> "$rootfile"
cat "$root/goose/cli/llama-bench-report.goose" >> "$rootfile"
cat "$root/goose/model/arch-registry.goose" >> "$rootfile"
cat "$root/goose/misc/spec-decode-decisions.goose" >> "$rootfile"
cat "$root/goose/cli/config-validate.goose" >> "$rootfile"
cat "$root/goose/metrics/metrics-traces.goose" >> "$rootfile"
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
    let rja = fg_sp_reject_accept(0.4, 0.5, 0.5);
    if rja < -1 {
        return;
    }
    let syn = fg_sp_synth_accept(0.5, 0.25, 0, 0);
    if syn < -1 {
        return;
    }
    let inp8: u8[8] = [104, 105, 32, 116, 104, 101, 114, 101];
    let outp8: u8[64] = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    let mtin: u8[16] = [97,44,34,10,98,0,0,0,0,0,0,0,0,0,0,0];
    let mtout: u8[64] = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0];
    let mtr = mt_field_encode(mtin, mtout);
    if mtr < 0 {
        return;
    }
    let cvi: i64[32] = [128, 4, 2048, 0, 0, 0, 0, 0, 1, 0, 0, 40, 0, 4, 3, 12, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    let cvf: f64[5] = [0.0, 0.9, 0.0, 0.0, 0.0];
    let cvr = cfg_validate(cvi, cvf);
    if cvr < 0 {
        return;
    }
    let sdctx: i32[4] = [1, 2, 3, 1];
    let sdout: i32[8] = [0, 0, 0, 0, 0, 0, 0, 0];
    let sdr = sdp_ngram_draft(sdctx, 2, 4, 1, 4, sdout);
    if sdr < 0 {
        return;
    }
    let arc = ar_count();
    if arc < 0 {
        return;
    }
    let bpb = fg_tk_bpe_before(3, 1, 5, 0);
    if bpb < -1 {
        return;
    }
    let nrm = fg_tk_norm(inp8, outp8, 1, 1, 0, 0);
    if nrm < -1 {
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

SHIM = """#include <stdio.h>
#include "sampler_chain.h"
#include "sampler_chain_ids.h"

// One initialisation for the whole library; the exported doors panic without it.
__attribute__((constructor)) static void fg_init(void) { goose_init(0, 0); }

// The metrics module calls out for "%.6g" formatting, and nothing provided it - the symbol was
// undefined and the library would not load once the module joined this unit. This is that provider:
// the same conversion the native route sink performs with fprintf.
int64_t mt_fmt_g6(sl_u8 out, int64_t at, double x) {
    char tmp[64];
    const int n = snprintf(tmp, (int) sizeof(tmp), "%.6g", x);
    if (n < 0) {
        return 0;
    }
    int64_t w = 0;
    for (int i = 0; i < n && at + w < out.len; ++i) {
        out.data[at + w] = (uint8_t) tmp[i];
        w++;
    }
    return w;
}

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
