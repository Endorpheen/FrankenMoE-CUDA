#!/usr/bin/env python3
"""Semantic mutation gate for source/gguf-split.goose (P7-02 S4).

Every mutation rewrites the port, regenerates the C, rebuilds the fixture and
reruns the native-vs-port comparison. A mutation is caught when the fixture
reports any difference (non-zero exit code). Compile errors count as
"build-failed", not "caught": the mutation must be semantically wrong, not
syntactically broken.

Usage:
    python3 tests/mutate.py            # all 12
    python3 tests/mutate.py --half 1   # 6 mutations
    python3 tests/mutate.py --half 2   # 6 mutations
    python3 tests/mutate.py --jobs 4   # build several mutants at once
"""

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor

BASE = os.path.dirname(os.path.abspath(__file__))
SUB = os.path.dirname(BASE)
SRC = os.path.join(SUB, "source", "gguf-split.goose")
GOOSE = os.environ.get("GOOSE", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")
REPO = os.environ.get("FG_REPO", "/home/end0/AI/FrankenMoE-CUDA")
L = os.environ.get("FG_LLAMA", os.path.join(REPO, "upstream", "llama.cpp"))
BIN = os.environ.get("FG_BIN", "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin")
MUT = os.path.join(BASE, "mut")

MUTATIONS = [
    # half 1: shard metadata, layout, size boundary
    ("M01", "split.no", "the shard number is written one higher",
     [("    q = w_u32(out, q, GSP_KV_UINT16);\n    q = w_bits(out, q, i_split, 2 as i64);",
       "    q = w_u32(out, q, GSP_KV_UINT16);\n    q = w_bits(out, q, i_split + 1 as i64, 2 as i64);")]),
    ("M02", "split.count", "every shard claims to be the only one",
     [("    q = w_u32(out, q, GSP_KV_UINT16);\n    q = w_bits(out, q, n_split, 2 as i64);",
       "    q = w_u32(out, q, GSP_KV_UINT16);\n    q = w_bits(out, q, 1 as i64, 2 as i64);")]),
    ("M03", "tensors.count", "split.tensors.count counts one tensor too many",
     [("    q = w_bits(out, q, n_tensors, 4 as i64);",
       "    q = w_bits(out, q, n_tensors + 1 as i64, 4 as i64);")]),
    ("M04", "first shard", "the input metadata is replayed in the wrong shards",
     [("""        if s == 0 as i64 {
            var k: i64 = 0;
            while k < st[S_N] {
                let key: const u8[:] = pool[ko[k]..ko[k] + kl[k]];
                if !g_is_split_key(key) {
                    let need: i64 = g_kv_need(kl[k], ar[k], ty[k], v1[k], v2[k]);""",
       """        if s != 0 as i64 {
            var k: i64 = 0;
            while k < st[S_N] {
                let key: const u8[:] = pool[ko[k]..ko[k] + kl[k]];
                if !g_is_split_key(key) {
                    let need: i64 = g_kv_need(kl[k], ar[k], ty[k], v1[k], v2[k]);""")]),
    ("M05", "offset origin", "descriptor offsets start at the shard's file position",
     [("        var cur: i64 = 0;\n        var i: i64 = t0[s];",
       "        var cur: i64 = q;\n        var i: i64 = t0[s];")]),
    ("M06", "size boundary", "a tensor that exactly fills the size limit starts a new shard",
     [("        if next_size > n_bytes_split {",
       "        if next_size >= n_bytes_split {")]),
    # half 2: tensor limit, data padding, merge
    ("M07", "tensor limit", "the tensor split limit is one higher",
     [("           i_tensor % n_split_tensors == 0 as i64 {",
       "           i_tensor % (n_split_tensors + 1 as i64) == 0 as i64 {")]),
    ("M08", "data padding", "shard tensor data is not padded to the alignment",
     [("""            q = w_bytes(out, q, inp, src, nb);
            q = w_pad(out, q, GSP_ALIGN);
            i += 1 as i64;
        }
        sh_off[s] = start;""",
       """            q = w_bytes(out, q, inp, src, nb);
            i += 1 as i64;
        }
        sh_off[s] = start;""")]),
    ("M09", "descriptor offset", "the descriptor uses the input file offset",
     [("            q = w_u64(out, q, cur);",
       "            q = w_u64(out, q, trec[i * TR_STRIDE + TR_OFFSET]);")]),
    ("M10", "merge split.count", "the merged file keeps split.count from the shard",
     [('    if g_set_zero(st, pool, ko, kl, ty, ar, v0, v1, v2, "split.count") != 0 as i64 {',
       "    if (1 as i64) == 2 as i64 {")]),
    ("M11", "merge offsets", "the merged descriptors use 64-byte offsets",
     [("    tst[TS_ALIGN] = GSP_ALIGN;\n    if t_recompute(tst, tne, trec, 0 as i64) != 0 as i64 {",
       "    tst[TS_ALIGN] = 64 as i64;\n    if t_recompute(tst, tne, trec, 0 as i64) != 0 as i64 {")]),
    ("M12", "merge prefix", "only the .gguf suffix is checked on a merge input name",
     [("    let slen: i64 = q + 5 as i64;",
       "    let slen: i64 = 5 as i64;")]),
]


def run(cmd, **kw):
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, **kw)


def build_and_run(mid, src_text, tool):
    d = os.path.join(MUT, mid)
    if os.path.isdir(d):
        shutil.rmtree(d)
    os.makedirs(d)
    src = os.path.join(d, "gguf-split.goose")
    with open(src, "w", encoding="utf-8") as f:
        f.write(src_text)
    q = run([GOOSE, "--standalone", "-o", os.path.join(d, "gguf_split.c"),
             "--header", os.path.join(d, "gguf_split.h"), src])
    if q.returncode:
        return "build-failed", q.stdout[-300:]
    q = run(["gcc-13", "-O0", "-std=c11", "-c", os.path.join(d, "gguf_split.c"),
             "-o", os.path.join(d, "gguf_split.o")])
    if q.returncode:
        return "object-failed", q.stdout[-300:]
    q = run(["g++-13", "-O1", "-std=c++17", "-I", d, "-I", os.path.join(L, "include"),
             "-I", os.path.join(L, "ggml", "include"), os.path.join(BASE, "fixture.cpp"),
             os.path.join(d, "gguf_split.o"), "-L", BIN, "-lllama", "-lggml-base",
             "-Wl,-rpath," + BIN, "-o", os.path.join(d, "fixture")])
    if q.returncode:
        return "host-failed", q.stdout[-300:]
    work = os.path.join(d, "work")
    q = run([os.path.join(d, "fixture"), work, tool])
    with open(os.path.join(d, "gate.txt"), "w", encoding="utf-8") as f:
        f.write(q.stdout)
    if q.returncode == 0:
        tail = q.stdout.strip().splitlines()[-1] if q.stdout.strip() else ""
        return "MISSED", tail
    if "goose runtime error" in q.stdout and "FG_G4" not in q.stdout:
        return "crashed", next((l for l in q.stdout.splitlines() if "goose runtime error" in l), "")
    first = next((l for l in q.stdout.splitlines() if l.startswith("FAIL")), "crashed")
    return "caught", first


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--half", type=int, default=0)
    ap.add_argument("--jobs", type=int, default=1)
    args = ap.parse_args()

    os.makedirs(MUT, exist_ok=True)
    with open(SRC, encoding="utf-8") as f:
        base = f.read()

    half_size = (len(MUTATIONS) + 1) // 2
    batch = MUTATIONS if args.half == 0 else MUTATIONS[(args.half - 1) * half_size:args.half * half_size]

    tool = run(["bash", os.path.join(BASE, "build-native.sh")])
    if tool.returncode:
        print("native oracle build failed:", tool.stdout[-300:])
        return 1
    tool_path = tool.stdout.strip().splitlines()[-1]

    prepared = []
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
            print(f"{mid}: not-applied [{cat}] {bad}")
            prepared.append((mid, cat, "not-applied", why, bad))
            continue
        prepared.append((mid, cat, None, why, text))

    results = []

    def work(item):
        mid, cat, status, why, payload = item
        if status:
            return (mid, cat, status, why, payload)
        st, detail = build_and_run(mid, payload, tool_path)
        return (mid, cat, st, why, detail)

    if args.jobs > 1:
        with ThreadPoolExecutor(max_workers=args.jobs) as ex:
            results = list(ex.map(work, prepared))
    else:
        results = [work(it) for it in prepared]

    for mid, cat, status, why, detail in results:
        print(f"{mid}: {status} [{cat}] {why[:60]} | {detail[:100]}")

    caught = sum(1 for r in results if r[2] == "caught")
    print(f"MUTATIONS {caught}/{len(batch)} caught")
    return 0 if caught == len(batch) else 1


if __name__ == "__main__":
    sys.exit(main())
