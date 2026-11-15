#!/usr/bin/env python3
"""Mutation gate for P9-05.

12 textual mutations per slice, six in each half. Every mutant must pass
`goose check`, then the slice's differential gate must fail when the mutant is
built at -O0.

    tests/mutate.py --module ss|rts|nd|all --half 1|2|all
"""
import argparse
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
GOOSE = os.environ.get("GOOSE_BIN", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")

SLICES = {
    "ss": ("ss-session-lifecycle.goose", "fixture_ss.cpp"),
    "rts": ("rts-runtime-facade.goose", "fixture_rts.cpp"),
    "nd": ("nd-draft-source.goose", "fixture_nd.cpp"),
}

# (name, old, new)
SS_H1 = [
    ("A1 sentinel floor off by one",
     "return SS_MIN_SENTINELS;",
     "return SS_MIN_SENTINELS + 1;"),
    ("A2 slots off by one",
     "let slots = total - sent;",
     "let slots = total - sent + 1;"),
    ("A3 clamp past the expert count",
     "return n_expert;",
     "return n_expert + 1;"),
    ("A4 clamp adds the reserve",
     "let safe = free_mib - SS_VRAM_RESERVE_MIB;",
     "let safe = free_mib + SS_VRAM_RESERVE_MIB;"),
    ("A5 reserve boundary strictness",
     "if free_mib <= SS_VRAM_RESERVE_MIB {",
     "if free_mib < SS_VRAM_RESERVE_MIB {"),
    ("A6 mtp margin subtracted",
     "return SS_PROBE_MARGIN_BASE + SS_PROBE_MARGIN_MTP;",
     "return SS_PROBE_MARGIN_BASE - SS_PROBE_MARGIN_MTP;"),
]

SS_H2 = [
    ("B1 probe budget stricter",
     "if free_mib > margin {",
     "if free_mib > margin + 1 {"),
    ("B2 probe runs on a cpu session",
     "if plan_slots > 0 && n_gpu_layers != 0 && plan_mib > 0 {",
     "if plan_slots > 0 && n_gpu_layers == 0 && plan_mib > 0 {"),
    ("B3 resize comparison flipped",
     "if pb < plan_mib {",
     "if pb > plan_mib {"),
    ("B4 disable keeps the plan's slots",
     "                action = SS_DISABLE;\n                slots = 0;",
     "                action = SS_DISABLE;\n                slots = plan_slots;"),
    ("B5 source torn down after the sampler",
     "if stage == SS_ST_SOURCE {\n        return 0;",
     "if stage == SS_ST_SOURCE {\n        return 1;"),
    ("B6 check accepts a duplicate",
     "if r <= prev {",
     "if r < prev {"),
]

RTS_H1 = [
    ("A1 n_batch follows n_ubatch",
     "sc[RTS_N_BATCH] = rts_n_batch(cfg[RTS_N_CTX]);",
     "sc[RTS_N_BATCH] = rts_n_batch(cfg[RTS_N_UBATCH]);"),
    ("A2 mapping refuses a full slice",
     "if cfg.len < 12 || sc.len < RTS_SC_FIELDS {",
     "if cfg.len < 13 || sc.len < RTS_SC_FIELDS {"),
    ("A3 n_ubatch reads n_predict",
     "sc[RTS_N_UBATCH] = cfg[RTS_N_UBATCH];",
     "sc[RTS_N_UBATCH] = cfg[RTS_N_PREDICT];"),
    ("A4 ubatch fallback takes 0",
     "if n_ubatch > 0 {\n        return n_ubatch;",
     "if n_ubatch >= 0 {\n        return n_ubatch;"),
    ("A5 open failure does not stop the plan",
     "if open_ok == 0 || n >= out.len {",
     "if open_ok != 0 || n >= out.len {"),
    ("A6 clear_kv dropped",
     "out[0] = 1; // clear_kv = true, unconditionally",
     "out[0] = 0; // clear_kv = true, unconditionally"),
]

RTS_H2 = [
    ("B1 render_text always on",
     "out[3] = progress; // render_text = cfg.progress",
     "out[3] = 1; // render_text = cfg.progress"),
    ("B2 n_predict takes think",
     "out[1] = n_predict;",
     "out[1] = think;"),
    ("B3 cancelled reported as completed",
     "return RTS_RES_CANCELLED;",
     "return RTS_RES_OK;"),
    ("B4 failure reported as success",
     "if ok == 0 {\n        return RTS_RES_ERROR;",
     "if ok != 0 {\n        return RTS_RES_ERROR;"),
    ("B5 chatml forced on",
     "sc[RTS_CHATML] = cfg[RTS_CHATML];",
     "sc[RTS_CHATML] = 1;"),
    ("B6 trace granularity reads top-k",
     "sc[RTS_COMPUTE_TRACE_LAYERS] = cfg[RTS_COMPUTE_TRACE_LAYERS];",
     "sc[RTS_COMPUTE_TRACE_LAYERS] = cfg[RTS_N_EXPERT_USED];"),
]

ND_H1 = [
    ("A1 ngram label",
     'return "ngram";',
     'return "ngrm";'),
    ("A2 load_mtp never requested",
     "if src == ND_MTP {\n        return 1;\n    }\n    return 0;\n}",
     "if src == ND_MTP {\n        return 1;\n    }\n    return 1;\n}"),
    ("A3 admit lets an empty head through",
     "if src == ND_MTP && n_layer_nextn <= 0 {",
     "if src == ND_MTP && n_layer_nextn < 0 {"),
    ("A4 streamed span subtracted",
     "return n_layer + n_layer_nextn;",
     "return n_layer - n_layer_nextn;"),
    ("A5 rollback snapshots from the wrong source",
     "if nd_enabled(src) != 0 {\n        return draft_max;",
     "if nd_enabled(src) == 0 {\n        return draft_max;"),
    ("A6 draft width one too wide",
     "var w: i64 = draft_max + 1;",
     "var w: i64 = draft_max + 2;"),
]

ND_H2 = [
    ("B1 draft clamp inverted",
     "if w > tgt_ubatch {\n        return tgt_ubatch;",
     "if w < tgt_ubatch {\n        return tgt_ubatch;"),
    ("B2 verify width one too wide",
     "let verify = draft_max + 1;",
     "let verify = draft_max + 2;"),
    ("B3 off reports a draft budget",
     "if nd_enabled(src) == 0 {\n        return 0;",
     "if nd_enabled(src) != 0 {\n        return 0;"),
    ("B4 p_min zeroed for mtp too",
     "if src != ND_MTP {\n        return 0.0;",
     "if src != ND_MTP {\n        return 1.0;"),
    ("B5 ngram knob reported for mtp",
     "if src != ND_NGRAM {\n        return 0;",
     "if src != ND_MTP {\n        return 0;"),
    ("B6 enabled inverted",
     "if src == ND_OFF {\n        return 0;\n    }\n    return 1;",
     "if src != ND_OFF {\n        return 0;\n    }\n    return 1;"),
]


def pick(module, half):
    tables = {
        ("ss", "1"): SS_H1, ("ss", "2"): SS_H2,
        ("rts", "1"): RTS_H1, ("rts", "2"): RTS_H2,
        ("nd", "1"): ND_H1, ("nd", "2"): ND_H2,
    }
    if half == "all":
        return tables[(module, "1")] + tables[(module, "2")]
    return tables[(module, half)]


def run_one(module, name, old, new, idx):
    src_name, fixture = SLICES[module]
    src_path = os.path.join(ROOT, "source", src_name)
    src = open(src_path, encoding="utf-8").read()
    if src.count(old) != 1:
        print(f"SKIP {module}/{name}: pattern occurs {src.count(old)} times")
        return False

    work = os.path.join(HERE, "build", f"mut-{module}-{idx:02d}")
    os.makedirs(work, exist_ok=True)
    mutant = os.path.join(work, src_name)
    with open(mutant, "w", encoding="utf-8") as f:
        f.write(src.replace(old, new))

    check = subprocess.run([GOOSE, "check", mutant], capture_output=True, text=True)
    if check.returncode != 0:
        print(f"SKIP {module}/{name}: goose check failed")
        return False

    env = dict(os.environ, GOOSE_SRC=mutant, OUT_DIR=work, OPT="-O0")
    build = subprocess.run(["bash", os.path.join(HERE, "build.sh"), module],
                           env=env, capture_output=True, text=True)
    caught = build.returncode != 0
    print(f"{'CATCH' if caught else 'MISS '} {module}/{name}")
    if not caught:
        for line in (build.stdout + build.stderr).splitlines()[-8:]:
            print("   ", line)
    return caught


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--module", choices=["ss", "rts", "nd", "all"], default="all")
    ap.add_argument("--half", choices=["1", "2", "all"], default="all")
    a = ap.parse_args()

    modules = list(SLICES) if a.module == "all" else [a.module]
    total = caught = 0
    for module in modules:
        muts = pick(module, a.half)
        got = 0
        for i, (name, old, new) in enumerate(muts, 1):
            if run_one(module, name, old, new, i):
                got += 1
        print(f"{module}: mutations {got}/{len(muts)} caught")
        total += len(muts)
        caught += got
    print(f"mutations {caught}/{total} caught")
    return 0 if caught == total and total > 0 else 1


if __name__ == "__main__":
    sys.exit(main())
