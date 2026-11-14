#!/usr/bin/env python3
"""Semantic mutation gate for source/gguf-tensors.goose (P7-02 S3).

Every mutation rewrites the port, regenerates the C, rebuilds the fixture and
reruns the native-vs-port comparison. A mutation is caught when the fixture
reports any difference (non-zero exit code). Compile errors count as
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
SRC = os.path.join(SUB, "source", "gguf-tensors.goose")
GOOSE = os.environ.get("GOOSE", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")
REPO = os.environ.get("FG_REPO", "/home/end0/AI/FrankenMoE-CUDA")
L = os.environ.get("FG_LLAMA", os.path.join(REPO, "upstream", "llama.cpp"))
BIN = os.environ.get("FG_BIN", "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin")
MUT = os.path.join(BASE, "mut")

MUTATIONS = [
    # half 1
    ("M01", "offset pad", "cumulative offsets skip the alignment padding",
     [("trec[p * TR_STRIDE + TR_OFFSET] + t_pad(nb, tst[TS_ALIGN]);",
       "trec[p * TR_STRIDE + TR_OFFSET] + nb;")]),
    ("M02", "n_dims", "the descriptor always claims 4 dimensions",
     [("        p = w_u32(out, p, trec[i * TR_STRIDE + TR_DIMS]);",
       "        p = w_u32(out, p, 4 as i64);")]),
    ("M03", "ne count", "the descriptor writes all 4 ne values",
     [("        while d < trec[i * TR_STRIDE + TR_DIMS] {",
       "        while d < 4 as i64 {")]),
    ("M04", "name prefix", "the tensor name loses its u64 length prefix",
     [("""        p = w_str(out, p, tpool, trec[i * TR_STRIDE + TR_NAME_OFF],
                  trec[i * TR_STRIDE + TR_NAME_LEN]);""",
       """        p = w_bytes(out, p, tpool, trec[i * TR_STRIDE + TR_NAME_OFF],
                    trec[i * TR_STRIDE + TR_NAME_LEN]);""")]),
    ("M05", "data pad", "tensor data is not padded to the alignment",
     [("""        p = w_bytes(out, p, tpool, trec[i * TR_STRIDE + TR_DATA_OFF], nb);
        p = w_pad(out, p, a);
        i += 1 as i64;""",
       """        p = w_bytes(out, p, tpool, trec[i * TR_STRIDE + TR_DATA_OFF], nb);
        i += 1 as i64;""")]),
    ("M06", "type width", "the tensor type is written as 8 bytes",
     [("        p = w_u32(out, p, trec[i * TR_STRIDE + TR_TYPE]);",
       "        p = w_u64(out, p, trec[i * TR_STRIDE + TR_TYPE]);")]),
    # half 2
    ("M07", "block size", "the 32-element block group reports 16",
     [("        return 32 as i64;\n    }\n    if t == 40 as i64",
       "        return 16 as i64;\n    }\n    if t == 40 as i64")]),
    ("M08", "type size", "f16 reports a type size of 4 bytes",
     [("    if t == 1 as i64 { return 2 as i64; }    // f16",
       "    if t == 1 as i64 { return 4 as i64; }    // f16")]),
    ("M09", "data offset", "the data copy starts one byte late",
     [("        p = w_bytes(out, p, tpool, trec[i * TR_STRIDE + TR_DATA_OFF], nb);",
       "        p = w_bytes(out, p, tpool, trec[i * TR_STRIDE + TR_DATA_OFF] + (1 as i64), nb);")]),
    ("M10", "data order", "the data blocks are written last to first",
     [("""    i = 0;
    while i < tst[TS_N] {
        let ty: i64 = trec[i * TR_STRIDE + TR_TYPE];""",
       """    i = tst[TS_N] - (1 as i64);
    while i >= 0 as i64 {
        let ty: i64 = trec[i * TR_STRIDE + TR_TYPE];"""),
      ("        p = w_pad(out, p, a);\n        i += 1 as i64;\n    }\n    return p;",
       "        p = w_pad(out, p, a);\n        i -= 1 as i64;\n    }\n    return p;")]),
    ("M11", "descriptor order", "the descriptors are written last to first",
     [("""    var i: i64 = 0;
    while i < tst[TS_N] {
        p = w_str(out, p, tpool,""",
       """    var i: i64 = tst[TS_N] - (1 as i64);
    while i >= 0 as i64 {
        p = w_str(out, p, tpool,"""),
      ("        i += 1 as i64;\n    }\n\n    let a: i64 = tst[TS_ALIGN];",
       "        i -= 1 as i64;\n    }\n\n    let a: i64 = tst[TS_ALIGN];")]),
    ("M12", "meta pad", "the descriptor block is not padded before the data section",
     [("""    let a: i64 = tst[TS_ALIGN];
    p = w_pad(out, p, a);

    if only_meta != 0 as i64 {""",
       """    let a: i64 = tst[TS_ALIGN];
    p = p + (0 as i64);

    if only_meta != 0 as i64 {""")]),
]


def run(cmd, **kw):
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, **kw)


def build_and_run(d, src_text):
    src = os.path.join(d, "gguf-tensors.goose")
    open(src, "w", encoding="utf-8").write(src_text)
    q = run([GOOSE, "--standalone", "-o", os.path.join(d, "gguf_tensors.c"),
             "--header", os.path.join(d, "gguf_tensors.h"), src])
    if q.returncode:
        return "build-failed", q.stdout[-300:]
    q = run(["gcc-13", "-O0", "-std=c11", "-c", os.path.join(d, "gguf_tensors.c"),
             "-o", os.path.join(d, "gguf_tensors.o")])
    if q.returncode:
        return "object-failed", q.stdout[-300:]
    q = run(["g++-13", "-O1", "-std=c++17", "-I", d, "-I", os.path.join(L, "ggml", "include"),
             os.path.join(BASE, "fixture.cpp"), os.path.join(d, "gguf_tensors.o"),
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
