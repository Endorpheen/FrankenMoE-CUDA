#!/usr/bin/env python3
"""P10-08 mutation run: 12 mutations per slice, two halves of six per slice.

    python3 tests/mutate.py 1     # 6 bkh + 6 ggh + 6 son
    python3 tests/mutate.py 2     # 6 bkh + 6 ggh + 6 son

Each mutation rewrites one decision of one Goose module, regenerates that module,
compiles the generated C at -O0, links the slice fixture and reruns its gate. A
surviving mutation is a hole in the gate; every mutation must be caught.

Builds are serial on purpose: the gate builds are manual, one at a time.
"""
from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

B = Path(__file__).resolve().parent.parent
OUT = B / "build"
WORK = OUT / "mut"
GOOSE = "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose"
NATIVE = Path("/home/end0/AI/FrankenMoE-CUDA/upstream/llama.cpp")
CC = "gcc-13"
CXX = "g++-13"

SRC = {
    "bkh": B / "source" / "backend-headers.goose",
    "ggh": B / "source" / "gguf-headers.goose",
    "son": B / "source" / "soname-exports.goose",
}
FIX = {
    "bkh": B / "tests" / "fixture_bkh.cpp",
    "ggh": B / "tests" / "fixture_ggh.cpp",
    "son": B / "tests" / "fixture_son.cpp",
}
SUM = {"bkh": "FG651_BKH", "ggh": "FG651_GGH", "son": "FG651_SON"}

# ── half 1 ──────────────────────────────────────────────────────────────────
M1 = [
    ("bkh", "the meta device type moves to 5",
     "    0, 1, 2,\n    0, 1, 2, 3, 4,",
     "    0, 1, 2,\n    0, 1, 2, 3, 5,"),
    ("bkh", "the meta device ceiling halves",
     "    16,\n    0, 1, 2, 3, 10, 11, 98, 99,",
     "    8,\n    0, 1, 2, 3, 10, 11, 98, 99,"),
    ("bkh", "the mirrored axis loses a letter",
     '    "MIRRORED",', '    "MIRROR",'),
    ("bkh", "the split-state size shrinks by one segment",
     "    2128,", "    2120,"),
    ("bkh", "the first callback gains a parameter",
     "const BKH_CB_ARITY: i64[11] = [2, 1, 2, 2, 2, 1, 3, 1, 3, 2, 4];",
     "const BKH_CB_ARITY: i64[11] = [3, 1, 2, 2, 2, 1, 3, 1, 3, 2, 4];"),
    ("bkh", "an unknown axis returns an empty name instead of -1",
     "        i = i + 1;\n    }\n    return BKH_NONE;",
     "        i = i + 1;\n    }\n    return 0;"),

    ("ggh", "the last magic byte becomes G",
     "    71, 71, 85, 70,", "    71, 71, 85, 71,"),
    ("ggh", "the format version moves to 4",
     "    3,\n    32,\n    17,", "    4,\n    32,\n    17,"),
    ("ggh", "the type count loses a member",
     "    13,\n    16,", "    12,\n    16,"),
    ("ggh", "the float type name gains a letter",
     '    "f32",', '    "fp32",'),
    ("ggh", "the float element size doubles",
     "[1, 1, 2, 2, 4, 4, 4, 1, 0, 0, 8, 8, 8]",
     "[1, 1, 2, 2, 4, 4, 8, 1, 0, 0, 8, 8, 8]"),
    ("ggh", "the default alignment halves",
     "    32,\n    17,", "    16,\n    17,"),

    ("son", "the first ggml version goes back one minor",
     '    "0.22.0",\n    "0.22.0",\n    "0.22.0",\n    "0.22.0"',
     '    "0.21.0",\n    "0.22.0",\n    "0.22.0",\n    "0.22.0"'),
    ("son", "libggml-base claims soversion 1",
     "const SON_LIB_SOV: i64[7] = [0, 0, 0, 0, 0, 0, 0];",
     "const SON_LIB_SOV: i64[7] = [0, 0, 0, 0, 1, 0, 0];"),
    ("son", "the SONAME is built with a longer prefix",
     '    var at: i64 = son_put(out, 0, "lib");\n'
     "    at = son_put(out, at, SON_LIB_STEM[id]);\n"
     '    at = son_put(out, at, ".so.");',
     '    var at: i64 = son_put(out, 0, "libs");\n'
     "    at = son_put(out, at, SON_LIB_STEM[id]);\n"
     '    at = son_put(out, at, ".so.");'),
    ("son", "libllama stops requiring libggml",
     '    "libggml.so.0",\n    "libggml-base.so.0",\n    "libllama.so.0",',
     '    "libggml-cpu.so.0",\n    "libggml-base.so.0",\n    "libllama.so.0",'),
    ("son", "the forbidden dependency becomes the one libllama does link",
     'const SON_LIB_FORBID: const u8[:][7] = [\n    "libggml-cuda.so.0",',
     'const SON_LIB_FORBID: const u8[:][7] = [\n    "libggml.so.0",'),
    ("son", "ggml-backend.h is exported from one library only",
     "const SON_HDR_LIBS: i64[5] = [\n    1,\n    16,\n    24,",
     "const SON_HDR_LIBS: i64[5] = [\n    1,\n    16,\n    8,"),
]

# ── half 2 ──────────────────────────────────────────────────────────────────
M2 = [
    ("bkh", "the first callback returns void",
     "const BKH_CB_RET: i64[11] = [5, 0, 1, 5, 0, 5, 0, 5, 1, 6, 1];",
     "const BKH_CB_RET: i64[11] = [0, 0, 1, 5, 0, 5, 0, 5, 1, 6, 1];"),
    ("bkh", "the eval callback takes a pointer as its first parameter",
     "    2, 5, 5, 5\n];", "    5, 5, 5, 5\n];"),
    ("bkh", "the device caps move to offset 40",
     "    0, 8, 16, 24, 32, 40, 48,", "    0, 8, 16, 24, 32, 40, 40,"),
    ("bkh", "the compute usage value moves to 3",
     "    0, 1, 2,\n    0, 1, 2, 3, 4,", "    0, 1, 3,\n    0, 1, 2, 3, 4,"),
    ("bkh", "the none axis becomes 97",
     "    0, 1, 2, 3, 10, 11, 98, 99,", "    0, 1, 2, 3, 10, 11, 97, 99,"),
    ("bkh", "the callback parameter stride loses a slot",
     "BKH_CB_PAR[id * 4 + j]", "BKH_CB_PAR[id * 3 + j]"),

    ("ggh", "a zero alignment is accepted",
     "    if a <= 0 {\n        return 0;\n    }\n    if (a & (a - 1)) != 0 {",
     "    if a < 0 {\n        return 0;\n    }\n    if (a & (a - 1)) != 0 {"),
    ("ggh", "the data offset is rounded down",
     "    return (meta_end + alignment - 1) & (0 - alignment);",
     "    return (meta_end + alignment - 1) & (alignment - 1);"),
    ("ggh", "a partial last chunk asks for one chunk too few",
     "        calls = calls + 1;", "        calls = calls;"),
    ("ggh", "the last chunk of a read ignores the remaining bytes",
     "    if left > limit {\n        return limit;\n    }\n    return left;",
     "    return limit;"),
    ("ggh", "any non-negative read continues the stream",
     "    if returned == requested {\n        return 1;\n    }",
     "    if returned >= 0 {\n        return 1;\n    }"),
    ("ggh", "the alignment key loses its tail",
     '    return ggh_copy(out, "general.alignment");',
     '    return ggh_copy(out, "general.align");'),

    ("son", "the ledger loses an export",
     "const SON_LEDGER_EXPORTS: i64 = 307;", "const SON_LEDGER_EXPORTS: i64 = 306;"),
    ("son", "the allowance is spelled differently",
     'const SON_ALLOW_NAME: const u8[:][1] = [\n    "ggml_backend_op_alloc_size_may_expand"\n];',
     'const SON_ALLOW_NAME: const u8[:][1] = [\n    "ggml_backend_op_alloc_size_expand"\n];'),
    ("son", "the legacy export set loses one member",
     "const SON_LEGACY_N: i64 = 6;", "const SON_LEGACY_N: i64 = 5;"),
    ("son", "the soversion swallows the whole version",
     "    while i < sov_end && at < out.len {",
     "    while i < name.len && at < out.len {"),
    ("son", "the identity check changes its length",
     "    if name.len != 12 {", "    if name.len != 13 {"),
    ("son", "an LLAMA_API declaration is classified as GGML_API",
     "    var base: i64 = 0;\n    if id == 2 {\n        base = 2;\n    }",
     "    var base: i64 = 0;\n    if id == 2 {\n        base = 1;\n    }"),
]

MUT = {1: M1, 2: M2}


def run(cmd: str, cwd: Path | None = None) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=900, cwd=cwd)


def main() -> int:
    half = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    if half not in MUT:
        print("usage: mutate.py 1|2")
        return 2
    WORK.mkdir(parents=True, exist_ok=True)
    tot = {s: {"caught": 0, "survived": 0, "broken": 0} for s in SRC}
    for i, (sl, name, old, new) in enumerate(MUT[half], 1):
        tag = f"h{half}-{i:02d}"
        d = WORK / tag
        shutil.rmtree(d, ignore_errors=True)
        d.mkdir(parents=True)
        base = SRC[sl].read_text()
        n = base.count(old)
        if n != 1:
            print(f"MUT {tag} SETUP-ERROR occurrences={n} :: {sl} :: {name}")
            tot[sl]["broken"] += 1
            continue
        (d / "p.goose").write_text(base.replace(old, new, 1))
        hdr = {"bkh": "bkh.h", "ggh": "ggh.h", "son": "son.h"}[sl]
        g = run(f"timeout 300 {GOOSE} --standalone -o p.c --header {hdr} p.goose", cwd=d)
        if g.returncode != 0:
            print(f"MUT {tag} BUILD-FAIL (goose) :: {name}\n{g.stderr.strip()[:200]}")
            tot[sl]["broken"] += 1
            continue
        c = run(f"timeout 300 {CC} -O0 -std=c11 -w -c p.c -o p.o", cwd=d)
        if c.returncode != 0:
            print(f"MUT {tag} BUILD-FAIL (gcc) :: {name}")
            tot[sl]["broken"] += 1
            continue
        includes = (
            f'-I{d} -I{OUT}/native -I{OUT} -I{NATIVE}/include '
            f"-I{NATIVE}/ggml/include -I{NATIVE}/ggml/src"
        )
        link = (
            f"timeout 300 {CXX} -O2 -std=c++17 {includes} {FIX[sl]} p.o "
            f"{OUT}/native/native_abi.o -ldl -o pfix"
        )
        link_res = run(link, cwd=d)
        if link_res.returncode != 0:
            print(f"MUT {tag} BUILD-FAIL (fixture) :: {name}\n{link_res.stderr.strip()[:300]}")
            tot[sl]["broken"] += 1
        else:
            r = run("timeout 300 ./pfix", cwd=d)
            line = "no summary (stopped before it)"
            for ln in r.stdout.splitlines():
                if ln.startswith(SUM[sl]):
                    line = ln
            if r.returncode != 0:
                tot[sl]["caught"] += 1
                print(f"MUT {tag} CAUGHT :: {sl} :: {name} :: {line}")
            else:
                tot[sl]["survived"] += 1
                print(f"MUT {tag} SURVIVED :: {sl} :: {name} :: {line}")
        shutil.rmtree(d, ignore_errors=True)
    shutil.rmtree(WORK, ignore_errors=True)
    bad = 0
    for sl in SRC:
        t = tot[sl]
        print(
            f'HALF {half} slice={sl} total={t["caught"] + t["survived"] + t["broken"]} '
            f'caught={t["caught"]} survived={t["survived"]} broken={t["broken"]}'
        )
        bad += t["survived"] + t["broken"]
    print(
        f'HALF {half} overall total={len(MUT[half])} caught={sum(t["caught"] for t in tot.values())} '
        f'survived={sum(t["survived"] for t in tot.values())} '
        f'broken={sum(t["broken"] for t in tot.values())}'
    )
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
