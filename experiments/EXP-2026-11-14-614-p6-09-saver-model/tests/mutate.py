#!/usr/bin/env python3
"""Semantic mutation gate for source/model-saver.goose (P6-09/P7-02 S5).

Every mutation rewrites the port, regenerates the C at -O0, rebuilds the
fixture and reruns the native-vs-port comparison. A mutation is caught when the
fixture reports any difference (non-zero exit code). Compile errors count as
"build-failed", not "caught": the mutation must be semantically wrong, not
syntactically broken.

Usage:
    python3 tests/mutate.py            # all 12
    python3 tests/mutate.py --half 1   # 6 mutations
    python3 tests/mutate.py --half 2   # 6 mutations
"""

import os
import subprocess
import sys

BASE = os.path.dirname(os.path.abspath(__file__))
SUB = os.path.dirname(BASE)
SRC = os.path.join(SUB, "source", "model-saver.goose")
GOOSE = os.environ.get("GOOSE", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")
L = os.environ.get("FG_LLAMA", "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda")
BIN = os.path.join(L, "build-cuda", "bin")
MUT = os.path.join(BASE, "mut")

MUTATIONS = [
    # half 1: value types and the vocabulary path
    ("M01", "per-layer count", "the per-layer reduction ignores n_layer_nextn",
     [("    let nl: i64 = ix[I_NLAYER_ALL] - ix[I_NLAYER_NEXTN];",
       "    let nl: i64 = ix[I_NLAYER_ALL] - 0 as i64;")]),
    ("M02", "u32 type", "the u32 setter writes an i32 value",
     [("""    return sav_add_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       ks(k_off, k_len, k_pool, slot), value, out);""",
       """    return sav_add_i32(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       ks(k_off, k_len, k_pool, slot), value, out);""")]),
    ("M03", "bool type", "the bool setter writes a u32 value",
     [("""    return sav_add_bool(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                        ks(k_off, k_len, k_pool, slot), value, out);""",
       """    return sav_add_u32(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                        ks(k_off, k_len, k_pool, slot), value, out);""")]),
    ("M04", "attr mapping", "LLAMA_TOKEN_ATTR_UNUSED maps to TYPE_NORMAL",
     [("    if attr == 2 as i64 { return 5 as i64; }   // ATTR_UNUSED       -> TYPE_UNUSED",
       "    if attr == 2 as i64 { return 1 as i64; }   // ATTR_UNUSED       -> TYPE_UNUSED")]),
    ("M05", "prec mapping", "only GGML_PREC_F32 becomes 0 in the prec bool array",
     [("        if prec[i] == 30 as i64 {  // GGML_PREC_Q8",
       "        if prec[i] == 10 as i64 {  // GGML_PREC_Q8")]),
    ("M06", "empty string", "an empty std::string is still written",
     [("""    if val.len == 0 as i64 {
        out[0] = 0 as i64;
        return 0 as i64;
    }
    return sav_add_str(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       ks(k_off, k_len, k_pool, slot), val, out);""",
       """    return sav_add_str(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                       ks(k_off, k_len, k_pool, slot), val, out);""")]),
    # half 2: decisions and tensor enumeration
    ("M07", "rope scale", "the rope scaling factor is inverted",
     [("    if scale == one {", "    if scale != one {")]),
    ("M08", "dsv4 branch", "the compress-ratio branch ignores hc_mult",
     [("    if ix[I_ARCH] == ix[I_ARCH_DSV4] || hv[",
       "    if ix[I_ARCH] == ix[I_ARCH_DSV4] && hv[")]),
    ("M09", "ple layers", "the PLE layer list takes the layers without the flag",
     [("        if is_ple[il] != 0 as i64 {", "        if is_ple[il] == 0 as i64 {")]),
    ("M10", "charmap guard", "a 1-byte charmap is accepted instead of refused",
     [("    if charmap_len > 0 as i64 {", "    if charmap_len > 1 as i64 {")]),
    ("M11", "tok/output alias", "tok_embd is saved only when the names are equal",
     [("            !t_same_name(out_t, tok_t, tname_off, tname_len, tname_pool) {",
       "            t_same_name(out_t, tok_t, tname_off, tname_len, tname_pool) {")]),
    ("M12", "layer dedup", "a tensor aliased across layers is added again",
     [("                if !seen {", "                if true {")]),
]


def run(cmd, **kw):
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, **kw)


def build_and_run(d, src_text):
    src = os.path.join(d, "model-saver.goose")
    open(src, "w", encoding="utf-8").write(src_text)
    q = run([GOOSE, "--standalone", "-o", os.path.join(d, "model_saver.c"),
             "--header", os.path.join(d, "model_saver.h"), src])
    if q.returncode:
        return "build-failed", q.stdout[-300:]
    q = run(["gcc-13", "-O0", "-std=c11", "-c", os.path.join(d, "model_saver.c"),
             "-o", os.path.join(d, "model_saver.o")])
    if q.returncode:
        return "object-failed", q.stdout[-300:]
    q = run(["g++-13", "-O1", "-std=c++17", "-I", d, "-I", BASE,
             "-I", os.path.join(L, "include"), "-I", os.path.join(L, "src"),
             "-I", os.path.join(L, "ggml", "include"),
             os.path.join(BASE, "fixture.cpp"), os.path.join(d, "model_saver.o"),
             "-L", BIN, "-lllama", "-lggml", "-lggml-base", "-Wl,-rpath," + BIN,
             "-o", os.path.join(d, "fixture")])
    if q.returncode:
        return "host-failed", q.stdout[-300:]
    q = run([os.path.join(d, "fixture"), d])
    open(os.path.join(d, "gate.txt"), "w", encoding="utf-8").write(q.stdout)
    if q.returncode == 0:
        tail = q.stdout.strip().splitlines()[-1] if q.stdout.strip() else ""
        return "MISSED", tail
    first = next((l for l in q.stdout.splitlines() if l.startswith("FAIL")), "crashed")
    return "caught", first


def main():
    half = 0
    if len(sys.argv) >= 3 and sys.argv[1] == "--half":
        half = int(sys.argv[2])
    os.makedirs(MUT, exist_ok=True)
    base = open(SRC, encoding="utf-8").read()

    half_size = (len(MUTATIONS) + 1) // 2
    batch = MUTATIONS if half == 0 else MUTATIONS[(half - 1) * half_size:half * half_size]

    results = []
    for mid, cat, why, parts in batch:
        text = base
        bad = None
        for old, new in parts:
            n = text.count(old)
            if n != 1:
                bad = f"matches {n}"
                break
            text = text.replace(old, new)
        if bad:
            results.append((mid, cat, "not-applied", why, bad))
            print(f"{mid}: not-applied [{cat}] {bad}")
            continue

        d = os.path.join(MUT, mid)
        os.makedirs(d, exist_ok=True)
        status, detail = build_and_run(d, text)
        results.append((mid, cat, status, why, detail))
        print(f"{mid}: {status} [{cat}] {detail[:110]}")

    caught = sum(1 for r in results if r[2] == "caught")
    print(f"MUTATIONS {caught}/{len(batch)} caught")
    return 0 if caught == len(batch) else 1


if __name__ == "__main__":
    sys.exit(main())
