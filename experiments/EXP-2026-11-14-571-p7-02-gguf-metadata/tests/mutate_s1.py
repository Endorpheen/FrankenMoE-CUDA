#!/usr/bin/env python3
"""Semantic mutation gate for s1_metadata.goose."""

import os
import shutil
import subprocess
import sys

BASE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(os.path.dirname(BASE), "source", "s1_metadata.goose")
GOOSE = "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose"
MUT = os.path.join(BASE, "mut_s1")

MUTATIONS = [
    ("M01", "set replaces", "set_val stops removing the old key",
     "    let _drop: i64 = drop(st, pool, key_off, key_len, types, arr, v0, v1, v2, key);\n    let off: i64 = append_key(st, pool, key_off, key_len, key);\n    types[st[S_N]] = t;\n    arr[st[S_N]] = 0 as i64;\n    v0[st[S_N]] = bits;",
     "    let _drop: i64 = -(1 as i64);\n    let off: i64 = append_key(st, pool, key_off, key_len, key);\n    types[st[S_N]] = t;\n    arr[st[S_N]] = 0 as i64;\n    v0[st[S_N]] = bits;"),
    ("M02", "remove shift", "shift_left stops one entry early",
     "    while i + (1 as i64) < st[S_N] {",
     "    while i + (2 as i64) < st[S_N] {"),
    ("M03", "remove missing", "drop returns 0 for an absent key",
     "    if at < 0 as i64 {\n        return -(1 as i64);\n    }\n    return shift_left",
     "    if at < 0 as i64 {\n        return 0 as i64;\n    }\n    return shift_left"),
    ("M04", "reserved type", "reserved key accepts only strings instead of u32",
     "    if t != T_UINT32 {\n        return false;\n    }",
     "    if t != T_STRING {\n        return false;\n    }"),
    ("M05", "key length", "append_key records one byte less",
     "    key_len[st[S_N]] = key.len;",
     "    key_len[st[S_N]] = key.len - (1 as i64);"),
    ("M06", "scalar type", "set_val stores type 0 for every scalar",
     "    types[st[S_N]] = t;\n    arr[st[S_N]] = 0 as i64;\n    v0[st[S_N]] = bits;",
     "    types[st[S_N]] = 0 as i64;\n    arr[st[S_N]] = 0 as i64;\n    v0[st[S_N]] = bits;"),
    ("M07", "string length", "set_str records a zero value length",
     "    v0[st[S_N]] = st[S_POOL];\n    v1[st[S_N]] = val.len;\n    v2[st[S_N]] = val.len;",
     "    v0[st[S_N]] = st[S_POOL];\n    v1[st[S_N]] = val.len;\n    v2[st[S_N]] = 0 as i64;"),
    ("M08", "array count", "set_arr records a zero element count",
     "    types[st[S_N]] = atype;\n    arr[st[S_N]] = 1 as i64;\n    v0[st[S_N]] = st[S_POOL];\n    v1[st[S_N]] = n;\n    v2[st[S_N]] = data.len;",
     "    types[st[S_N]] = atype;\n    arr[st[S_N]] = 1 as i64;\n    v0[st[S_N]] = st[S_POOL];\n    v1[st[S_N]] = 0 as i64;\n    v2[st[S_N]] = data.len;"),
    ("M09", "set_kv count", "set_kv does not advance the destination count",
     "        dst_st[S_N] = di + (1 as i64);",
     "        dst_st[S_N] = di;"),
    ("M10", "set_kv replace", "set_kv stops removing destination duplicates",
     "        let _drop: i64 = drop(dst_st, dst_pool, dst_key_off, dst_key_len, dst_types,\n                              dst_arr, dst_v0, dst_v1, dst_v2, key);",
     "        let _drop: i64 = -(1 as i64);"),
    ("M11", "set_kv bytes", "set_kv copies zero bytes for strings and arrays",
     "            let m: i64 = src_v2[k];",
     "            let m: i64 = 0 as i64;"),
    ("M12", "string array flag", "string array is stored with the numeric array flag",
     "    arr[st[S_N]] = 2 as i64;",
     "    arr[st[S_N]] = 1 as i64;"),
]


def run(cmd, **kw):
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, **kw)


def main():
    half = 0
    if len(sys.argv) >= 3 and sys.argv[1] == "--half":
        half = int(sys.argv[2])
    os.makedirs(MUT, exist_ok=True)
    base = open(SRC, encoding="utf-8").read()
    half_size = (len(MUTATIONS) + 1) // 2
    batch = MUTATIONS if half == 0 else MUTATIONS[(half - 1) * half_size:half * half_size]
    results = []
    for mid, cat, why, old, new in batch:
        n = base.count(old)
        if n != 1:
            results.append((mid, cat, "not-applied", why, f"matches {n}"))
            continue
        d = os.path.join(MUT, mid)
        os.makedirs(d, exist_ok=True)
        src = os.path.join(d, "s1_metadata.goose")
        open(src, "w", encoding="utf-8").write(base.replace(old, new))
        q = run([GOOSE, "-o", os.path.join(d, "s1_metadata.c"), "--standalone",
                 "--header", os.path.join(d, "s1_metadata.h"), src])
        if q.returncode:
            results.append((mid, cat, "build-failed", why, q.stdout[-300:]))
            continue
        q = run(["gcc", "-O1", "-c", os.path.join(d, "s1_metadata.c"),
                 "-o", os.path.join(d, "s1_metadata.o")])
        if q.returncode:
            results.append((mid, cat, "object-failed", why, q.stdout[-300:]))
            continue
        q = run(["g++", "-O1", "-I", d, "-I", BASE, os.path.join(BASE, "host_s1.cpp"),
                 os.path.join(d, "s1_metadata.o"), "-o", os.path.join(d, "host_s1"), "-lm"])
        if q.returncode:
            results.append((mid, cat, "host-failed", why, q.stdout[-300:]))
            continue
        out = run([os.path.join(d, "host_s1")]).stdout
        open(os.path.join(d, "goose_s1.tsv"), "w", encoding="utf-8").write(out)
        diff = subprocess.run(["diff", os.path.join(os.path.dirname(BASE), "build", "oracle.tsv"),
                               os.path.join(d, "goose_s1.tsv")],
                              stdout=subprocess.PIPE, text=True).stdout
        caught = bool(diff.strip())
        first = next((l for l in diff.splitlines() if l.startswith("<") or l.startswith(">")), "")
        results.append((mid, cat, "caught" if caught else "MISSED", why, first))
        print(f"{mid}: {'caught' if caught else 'MISSED'} [{cat}] {first[:100]}")
    caught = sum(1 for r in results if r[2] == "caught")
    print(f"MUTATIONS {caught}/{len(batch)} caught")
    if len(results) != len(batch) or caught != len(batch):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
