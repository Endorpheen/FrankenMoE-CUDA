#!/usr/bin/env python3
"""P10-08 mutation run: 12 mutations per slice, two halves of six per slice.

    python3 tests/mutate.py 1     # 6 devh + 6 cpph + 6 sched
    python3 tests/mutate.py 2     # 6 devh + 6 cpph + 6 sched

Each mutation rewrites one decision of one Goose module, regenerates that
module, compiles the generated C at -O0, links the slice fixture and reruns its
gate. A surviving mutation is a hole in the gate; every mutation must be
caught. Builds are serial on purpose: the gate builds are manual, one at a
time.
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
CARRIER = Path("/home/end0/AI/FrankenMoE-CUDA/runtime/core/include")
SO = Path("/home/end0/AI/FrankenMoE-CUDA/build/exp054-dflash/bin")
CC = "gcc-13"
CXX = "g++-13"

SRC = {
    "devh": B / "source" / "device-headers.goose",
    "cpph": B / "source" / "cpp-headers.goose",
    "sched": B / "source" / "sched-eval.goose",
}
FIX = {
    "devh": B / "tests" / "fixture_devh.cpp",
    "cpph": B / "tests" / "fixture_cpph.cpp",
    "sched": B / "tests" / "fixture_sched.cpp",
}
SUM = {"devh": "FG659_DEVH", "cpph": "FG659_CPPH", "sched": "FG659_SCHED"}
HDR = {"devh": "devh_gen.h", "cpph": "cpph_gen.h", "sched": "sched_gen.h"}

WRAPS = (
    "-Wl,--wrap=ggml_free -Wl,--wrap=gguf_free -Wl,--wrap=ggml_gallocr_free "
    "-Wl,--wrap=ggml_backend_free -Wl,--wrap=ggml_backend_buffer_free "
    "-Wl,--wrap=ggml_backend_event_free -Wl,--wrap=ggml_backend_sched_free"
)

# ── half 1 ──────────────────────────────────────────────────────────────────
M1 = [
    ("devh", "the CUDA name loses its meaning",
     '    "CUDA",\n    "ET",', '    "CUDA2",\n    "ET",'),
    ("devh", "the CUDA device ceiling halves",
     "const DEVH_CONST_NUM: i64[14] = [16, 0, 16, 0, 0, 16, 48, 0, 16, 0, 0, 6, 0, 0];",
     "const DEVH_CONST_NUM: i64[14] = [8, 0, 16, 0, 0, 16, 48, 0, 16, 0, 0, 6, 0, 0];"),
    ("devh", "the RPC major version moves forward",
     "0, 16, 0, 0, 6, 0, 0];", "0, 16, 0, 0, 7, 0, 0];"),
    ("devh", "a declaration is filed under the wrong header",
     "const DEVH_FN_HDR: i64[84] = [\n    8, 8, 8, 7,",
     "const DEVH_FN_HDR: i64[84] = [\n    8, 8, 8, 4,"),
    ("devh", "a declaration loses a parameter",
     "const DEVH_FN_ARITY: i64[84] = [\n    0, 0, 2, 1, 0, 3, 3, 0,",
     "const DEVH_FN_ARITY: i64[84] = [\n    0, 0, 2, 1, 0, 2, 3, 0,"),
    ("devh", "a pointer return becomes void",
     "const DEVH_FN_RET: i64[84] = [\n    5, 5, 0, 5,",
     "const DEVH_FN_RET: i64[84] = [\n    0, 5, 0, 5,"),

    ("cpph", "the scheduler deleter releases a buffer",
     "const CPPH_WRAP_FREE_ID: i64[7] = [0, 1, 2, 3, 4, 5, 6];",
     "const CPPH_WRAP_FREE_ID: i64[7] = [0, 1, 2, 3, 4, 5, 5];"),
    ("cpph", "the context wrapper becomes shared",
     "const CPPH_WRAP_KIND: i64[7] = [0, 0, 0, 0, 0, 0, 0];",
     "const CPPH_WRAP_KIND: i64[7] = [1, 0, 0, 0, 0, 0, 0];"),
    ("cpph", "a deleter is declared stateful",
     "const CPPH_DELETER_EMPTY: i64 = 1;", "const CPPH_DELETER_EMPTY: i64 = 0;"),
    ("cpph", "a live wrapper is released twice",
     "    if live != 0 {\n        return 1;\n    }",
     "    if live != 0 {\n        return 2;\n    }"),
    ("cpph", "release() also releases",
     "export fn cpph_release_calls() -> i64 {\n    return 0;",
     "export fn cpph_release_calls() -> i64 {\n    return 1;"),
    ("cpph", "the sampling top-k default changes",
     "    4294967295, 0, 40, 0,\n", "    4294967295, 0, 32, 0,\n"),

    ("sched", "the first ask is reported as an observation",
     "ne = sched_push(ev, ne, j0 * 2 + SCHED_ASK);",
     "ne = sched_push(ev, ne, j0 * 2 + SCHED_OBSERVE);"),
    ("sched", "the computed range loses its end",
     "nc = sched_push(comp, nc, j0 * 256 + (j1 + 1));",
     "nc = sched_push(comp, nc, j0 * 256 + j1);"),
    ("sched", "the scheduler observes the node it was told not to",
     "        if need == 1 {\n            ne = sched_push(ev, ne, j1 * 2 + SCHED_OBSERVE);",
     "        if need == 0 {\n            ne = sched_push(ev, ne, j1 * 2 + SCHED_OBSERVE);"),
    ("sched", "the observe callback can no longer cancel",
     "            if cancel_at == j1 {\n                status = SCHED_CANCELLED;",
     "            if cancel_at == 0 - 2 {\n                status = SCHED_CANCELLED;"),
    ("sched", "the event count is written one too large",
     "    ev[0] = ne - 1;", "    ev[0] = ne;"),
    ("sched", "a successful load drops its final progress call",
     "    return n_events + 1;", "    return n_events;"),
]

# ── half 2 ──────────────────────────────────────────────────────────────────
M2 = [
    ("devh", "the SYCL device ceiling grows",
     "0, 0, 16, 48, 0, 16, 0, 0, 6, 0, 0];",
     "0, 0, 16, 64, 0, 16, 0, 0, 6, 0, 0];"),
    ("devh", "the host-buffer capability answers buffer_type",
     "    if c == 4 {\n        return devh_suffix(s, \"_host_buffer_type\");",
     "    if c == 4 {\n        return devh_suffix(s, \"_buffer_type\");"),
    ("devh", "the reg capability looks for init",
     '    if c == 1 {\n        return devh_suffix(s, "_reg");',
     '    if c == 1 {\n        return devh_suffix(s, "_init");'),
    ("devh", "the SYCL ceiling table and the macro disagree",
     "const DEVH_MAXDEV: i64[14] = [16, 0 - 1, 16, 0 - 1, 0 - 1, 0 - 1, 48, 16,",
     "const DEVH_MAXDEV: i64[14] = [16, 0 - 1, 16, 0 - 1, 0 - 1, 0 - 1, 16, 16,"),
    ("devh", "a header name changes",
     '    "ggml-sycl.h",', '    "ggml-sycl.hpp",'),
    ("devh", "a numeric macro answers with a string",
     "    if i < 0 || i >= DEVH_CONST_N || DEVH_CONST_KIND[i] != 1 {\n        return DEVH_NONE;\n    }\n    return devh_copy(out, DEVH_CONST_TXT[i]);",
     "    if i < 0 || i >= DEVH_CONST_N {\n        return DEVH_NONE;\n    }\n    return devh_copy(out, DEVH_CONST_TXT[i]);"),

    ("cpph", "the cache floor shrinks",
     "    0, 0, 1536, 0, 2,", "    0, 0, 1024, 0, 2,"),
    ("cpph", "the wrapper is twice as wide",
     "const CPPH_WRAPPER_SIZE: i64 = 8;", "const CPPH_WRAPPER_SIZE: i64 = 16;"),
    ("cpph", "the header loses an include",
     "const CPPH_INCLUDE_N: i64 = 5;", "const CPPH_INCLUDE_N: i64 = 4;"),
    ("cpph", "version() invents a number without the build define",
     '    return cpph_copy(out, "unknown");', '    return cpph_copy(out, "0.0.0");'),
    ("cpph", "is_mtp accepts the ngram draft",
     "export fn cpph_spec_is_mtp(source: i64) -> i64 {\n    if source == 1 {",
     "export fn cpph_spec_is_mtp(source: i64) -> i64 {\n    if source == 2 {"),
    ("cpph", "the carrier layout loses a field slot",
     "const CPPH_LAYOUT_VAL: i64[8] = [16, 76, 20, 12, 20, 36, 4, 16];",
     "const CPPH_LAYOUT_VAL: i64[8] = [16, 76, 20, 12, 20, 32, 4, 16];"),

    ("sched", "the last node of a split is never asked",
     "    while j0 < n {", "    while j0 < n - 1 {"),
    ("sched", "the node limit doubles",
     "if n < 0 || n > SCHED_MAX_NODES || ev.len < 1 || comp.len < 1 {",
     "if n < 0 || n > 64 || ev.len < 1 || comp.len < 1 {"),
    ("sched", "an aborted progress load reports one call too few",
     "        return abort_at + 1;", "        return abort_at;"),
    ("sched", "a null progress callback is still invoked",
     "    if has_cb == 0 {\n        return 0;\n    }\n    if abort_at >= 0 {\n        return abort_at + 1;",
     "    if has_cb == 0 {\n        return 1;\n    }\n    if abort_at >= 0 {\n        return abort_at + 1;"),
    ("sched", "a null log callback routes to a listener",
     "    if set_called != 0 && null_cb == 0 {\n        return 0;\n    }",
     "    if set_called != 0 {\n        return 0;\n    }"),
    ("sched", "the error level becomes a warning",
     "const SCHED_LEVEL_VAL: i64[6] = [0, 1, 2, 3, 4, 5];",
     "const SCHED_LEVEL_VAL: i64[6] = [0, 1, 2, 3, 3, 5];"),
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
        g = run(f"timeout 300 {GOOSE} --standalone -o p.c --header {HDR[sl]} p.goose", cwd=d)
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
            f"-I{NATIVE}/ggml/include -I{NATIVE}/ggml/src -I{CARRIER}"
        )
        extra = ""
        if sl == "cpph":
            extra = f"{WRAPS} {OUT}/ver-defined.o {OUT}/ver-plain.o"
        elif sl == "sched":
            extra = f'-L{SO} -lggml-base -Wl,-rpath,{SO}'
        link = (
            f"timeout 300 {CXX} -O2 -std=c++17 {includes} {FIX[sl]} p.o {extra} "
            f"-o pfix"
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
