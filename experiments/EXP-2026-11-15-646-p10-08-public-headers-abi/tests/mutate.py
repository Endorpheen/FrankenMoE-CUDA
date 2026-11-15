#!/usr/bin/env python3
"""P10-08 mutation run: 12 mutations, two halves of six.

    python3 tests/mutate.py 1     # abi 2 + hdr 2 + nm 2
    python3 tests/mutate.py 2     # abi 2 + hdr 2 + nm 2

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
    "abi": B / "source" / "ggml-type-table.goose",
    "hdr": B / "source" / "public-enum-values.goose",
    "nm": B / "source" / "public-name-tables.goose",
}
FIX = {
    "abi": B / "tests" / "fixture_abi.cpp",
    "hdr": B / "tests" / "fixture_hdr.cpp",
    "nm": B / "tests" / "fixture_nm.cpp",
}
SUM = {"abi": "FG645_ABI", "hdr": "FG645_HDR", "nm": "FG645_NM"}

# Half 1: block geometry, one name, and two text decisions.
M1 = [
    ("abi", "q2_K keeps a 256-element block",
     "    256, // 10 q2_K", "    128, // 10 q2_K"),
    ("abi", "q4_K name is spelled q4_k",
     '    "q4_K",', '    "q4_k",'),

    ("hdr", "the session file version moves to 11",
     "    10, // LLAMA_SESSION_VERSION", "    11, // LLAMA_SESSION_VERSION"),
    ("hdr", "LLAMA_TOKEN_ATTR_BYTE loses its bit",
     "    32, // LLAMA_TOKEN_ATTR_BYTE", "    16, // LLAMA_TOKEN_ATTR_BYTE"),

    ("nm", "the guessed prefix loses its trailing space",
     'const NM_FTYPE_PREFIX: const u8[:] = "(guessed) ";',
     'const NM_FTYPE_PREFIX: const u8[:] = "(guessed)";'),
    ("nm", "an unknown load-mode string is reported as a null string",
     "    return NM_PARSE_UNKNOWN;", "    return NM_NONE;"),
]

# Half 2: the removed slots, the block-size guard, two more numeric and text
# decisions.
M2 = [
    ("abi", "the second DEPRECATED slot is reworded",
     '    "DEPRECATED",\n    "DEPRECATED",', '    "DEPRECATED",\n    "deprecated",'),
    ("abi", "the whole-block guard is dropped",
     "    if blck <= 0 || ne < 0 || ne % blck != 0 {",
     "    if blck <= 0 || ne < 0 {"),

    ("hdr", "the buffer-override limit shrinks",
     "    4096, // llama_max_tensor_buft_overrides",
     "    1024, // llama_max_tensor_buft_overrides"),
    ("hdr", "the tensor rank grows to five",
     "    4, // GGML_MAX_DIMS", "    5, // GGML_MAX_DIMS"),

    ("nm", "the repeat-penalty key is pluralised",
     '    "general.sampling.penalty_repeat",',
     '    "general.sampling.penalty_repeats",'),
    ("nm", "the guessed prefix is applied when the bit is clear",
     "    if (ftype & NM_FTYPE_GUESSED) != 0 {",
     "    if (ftype & NM_FTYPE_GUESSED) == 0 {"),
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
        tag = f"h{half}-{i}"
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
        hdr = {"abi": "abi.h", "hdr": "hdr.h", "nm": "nm.h"}[sl]
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
        link = f"timeout 300 {CXX} -O2 -std=c++17 {includes} {FIX[sl]} p.o -o pfix"
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
