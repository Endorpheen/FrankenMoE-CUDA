#!/usr/bin/env python3
"""Semantic mutation gate for source/gguf-writer.goose (P7-02 S2 writer).

Every mutation rewrites the port, regenerates the C, rebuilds the fixture and
reruns the native-vs-port comparison. A mutation is caught when the fixture
reports any difference (non-zero exit code).

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
SRC = os.path.join(SUB, "source", "gguf-writer.goose")
GOOSE = os.environ.get("GOOSE", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")
REPO = os.environ.get("FG_REPO", "/home/end0/AI/FrankenMoE-CUDA")
L = os.environ.get("FG_LLAMA", os.path.join(REPO, "upstream", "llama.cpp"))
BIN = os.environ.get("FG_BIN", "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin")
MUT = os.path.join(BASE, "mut")

MUTATIONS = [
    ("M01", "key length", "the key length prefix gains one byte",
     [("        p = w_str(out, p, pool, key_off[i], key_len[i]);",
       "        p = w_str(out, p, pool, key_off[i], key_len[i] + (1 as i64));")]),
    ("M02", "kv order", "KV pairs are written from last to first",
     [("""    var i: i64 = 0;
    while i < st[S_N] {
        p = w_str(out, p, pool, key_off[i], key_len[i]);""",
       """    var i: i64 = st[S_N] - (1 as i64);
    while i >= 0 as i64 {
        p = w_str(out, p, pool, key_off[i], key_len[i]);"""),
      ("        i = i + (1 as i64);\n    }\n\n    let a = alignment;",
       "        i = i - (1 as i64);\n    }\n\n    let a = alignment;")]),
    ("M03", "array type", "the array element type is not written",
     [("            p = w_u32(out, p, t);\n            p = w_u64(out, p, v1[i]);",
       "            p = w_u64(out, p, v1[i]);")]),
    ("M04", "version", "the header version is off by one",
     [("    p = w_u32(out, p, version);",
       "    p = w_u32(out, p, version + (1 as i64));")]),
    ("M05", "header order", "n_kv and n_tensors are swapped",
     [("    p = w_u64(out, p, 0 as i64);\n    p = w_u64(out, p, st[S_N]);",
       "    p = w_u64(out, p, st[S_N]);\n    p = w_u64(out, p, 0 as i64);")]),
    ("M06", "padding", "the pad target is hard-coded to 16",
     [("    let a = alignment;", "    let a = 16 as i64;")]),
    ("M07", "string array", "each string array element keeps its trailing NUL",
     [("                    p = w_str(out, p, pool, q, e - q);",
       "                    p = w_str(out, p, pool, q, e - q + (1 as i64));")]),
    ("M08", "scalar width", "8-byte scalars are written as 4 bytes",
     [("    return 8 as i64;\n}", "    return 4 as i64;\n}")]),
    ("M09", "magic", "the magic letter U becomes V",
     [("const W_MAGIC_2: i64 = 85;", "const W_MAGIC_2: i64 = 86;")]),
    ("M10", "array count", "the element count is read from the byte length",
     [("            p = w_u64(out, p, v1[i]);",
       "            p = w_u64(out, p, v2[i]);")]),
    ("M11", "string scalar", "a scalar string is written without a length prefix",
     [("                p = w_str(out, p, pool, v0[i], v1[i]);",
       "                p = w_bytes(out, p, pool, v0[i], v1[i]);")]),
    ("M12", "pad bytes", "padding is filled with 0xff",
     [("        out[p] = 0 as u8;", "        out[p] = 255 as u8;")]),
]


def run(cmd, **kw):
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, **kw)


def build_and_run(d, src_text):
    src = os.path.join(d, "gguf-writer.goose")
    open(src, "w", encoding="utf-8").write(src_text)
    q = run([GOOSE, "--standalone", "-o", os.path.join(d, "gguf_writer.c"),
             "--header", os.path.join(d, "gguf_writer.h"), src])
    if q.returncode:
        return "build-failed", q.stdout[-300:]
    q = run(["gcc-13", "-O1", "-std=c11", "-c", os.path.join(d, "gguf_writer.c"),
             "-o", os.path.join(d, "gguf_writer.o")])
    if q.returncode:
        return "object-failed", q.stdout[-300:]
    q = run(["g++-13", "-O1", "-std=c++17", "-I", d, "-I", os.path.join(L, "ggml", "include"),
             os.path.join(BASE, "fixture.cpp"), os.path.join(d, "gguf_writer.o"),
             "-L", BIN, "-lggml-base", "-Wl,-rpath," + BIN,
             "-o", os.path.join(d, "fixture")])
    if q.returncode:
        return "host-failed", q.stdout[-300:]
    q = run([os.path.join(d, "fixture"), os.path.join(d, "align_tmp.bin")])
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
