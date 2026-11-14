#!/usr/bin/env python3
"""Semantic mutation gate for source/model-saver.goose (P6-09 S4).

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
    # half 1: scalar overloads
    ("M01", "u32 type", "the u32 overload tags the value as i32",
     [("""    return core_s1_set_val(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                      key, K_U32, value, out);""",
       """    return core_s1_set_val(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                      key, K_I32, value, out);""")]),
    ("M02", "i32 type", "the i32 overload tags the value as u64",
     [("""    return core_s1_set_val(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                      key, K_I32, value, out);""",
       """    return core_s1_set_val(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                      key, K_U64, value, out);""")]),
    ("M03", "bool normalize", "the bool overload stores the raw value instead of 0/1",
     [("""    var bits: i64 = 0;
    if value != 0 as i64 {
        bits = 1 as i64;
    }""",
       """    var bits: i64 = value;""")]),
    ("M04", "u64 type", "the u64 overload tags the value as u32",
     [("""    return core_s1_set_val(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                      key, K_U64, value, out);""",
       """    return core_s1_set_val(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                      key, K_U32, value, out);""")]),
    ("M05", "f32 type", "the f32 overload tags the value as u64",
     [("""    return core_s1_set_val(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                      key, K_F32, value, out);""",
       """    return core_s1_set_val(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                      key, K_U64, value, out);""")]),
    ("M06", "str value", "the string overload stores the key as the value",
     [("""    return core_s1_set_str(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                      key, val, out);""",
       """    return core_s1_set_str(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                      key, key, out);""")]),
    # half 2: containers, per-layer reduction, tensors, save
    ("M07", "per-layer count", "the per-layer container ignores the model layer count",
     [("""        n_values = nl;""",
       """        n_values = n;""")]),
    ("M08", "per-layer collapse", "a per-layer container always collapses to one scalar",
     [("""            if !k_elem_eq(data, 0 as i64, i, esize) {
                same = false;
            }""",
       """            if !k_elem_eq(data, 0 as i64, i, esize) {
                same = true;
            }""")]),
    ("M09", "array type", "every numeric array is written as u64",
     [("                      key, etype, data[0..n_values * esize], n_values, out);",
       "                      key, K_U64, data[0..n_values * esize], n_values, out);")]),
    ("M10", "array count", "the array count is the byte length, not the element count",
     [("                      key, etype, data[0..n_values * esize], n_values, out);",
       "                      key, etype, data[0..n_values * esize], n_values * esize, out);")]),
    ("M11", "rope duplicate", "a duplicate rope tensor is added again instead of skipped",
     [("""        if k_rope_name(name) {
            return 1 as i64;
        }""",
       """        if k_rope_name(name) {
            return core_s3_add_tensor(tst, tpool, tne, trec, name, ndims, ne, t, data);
        }""")]),
    ("M12", "save meta", "save() writes metadata only, without the data section",
     [("""    return core_s3_write(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                    tst, tpool, tne, trec, out, version, 0 as i64);""",
       """    return core_s3_write(st, pool, key_off, key_len, types, arr, v0, v1, v2,
                    tst, tpool, tne, trec, out, version, 1 as i64);""")]),
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
    q = run(["g++-13", "-O1", "-std=c++17", "-I", d,
             "-I", os.path.join(L, "include"), "-I", os.path.join(L, "src"),
             "-I", os.path.join(L, "ggml", "include"),
             os.path.join(BASE, "fixture.cpp"), os.path.join(d, "model_saver.o"),
             "-L", BIN, "-lllama", "-lggml", "-lggml-base", "-Wl,-rpath," + BIN,
             "-o", os.path.join(d, "fixture")])
    if q.returncode:
        return "host-failed", q.stdout[-300:]
    q = run([os.path.join(d, "fixture"), os.path.join(d, "s4_tmp.gguf")])
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
