#!/usr/bin/env python3
"""P10-06 mutation run: 12 mutations per slice, delivered in two halves.

Each mutation rewrites one decision of the port, regenerates the module with
the Goose compiler at -O0, links it against the unmodified fixture and runs the
unmodified gate. A mutation that leaves the gate green is a hole in the gate.

Usage: mutate.py <arch|tgt|inst> <1|2> [--jobs N]
Output: one line per mutation, CAUGHT or SURVIVED, plus a summary line.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
EXP = HERE.parent
GOOSE = os.environ.get("FG_GOOSE", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")
CC = os.environ.get("FG_CC", "gcc-13")
CXX = os.environ.get("FG_CXX", "g++-13")

SLICES = {
    "arch": ("cuda-arch-policy.goose", "arch_fixture.cpp"),
    "tgt": ("cuda-target-flags.goose", "tgt_fixture.cpp"),
    "inst": ("cuda-install-layout.goose", "inst_fixture.cpp"),
}

# (name, old, new). Every `old` must occur exactly once in the slice source.
MUTATIONS: dict[str, list[tuple[str, str, str]]] = {
    "arch": [
        ("default drops 50-virtual",
         '        at = arc_put(out, at, "50-virtual");\n        at = arc_putc(out, at, ARC_SEMI);\n', ""),
        ("default swaps 61 and 70",
         '        at = arc_put(out, at, "61-virtual");\n        at = arc_putc(out, at, ARC_SEMI);\n        at = arc_put(out, at, "70-virtual");',
         '        at = arc_put(out, at, "70-virtual");\n        at = arc_putc(out, at, ARC_SEMI);\n        at = arc_put(out, at, "61-virtual");'),
        ("default 86 becomes virtual",
         'at = arc_put(out, at, "86-real");', 'at = arc_put(out, at, "86-virtual");'),
        ("11.8 threshold becomes 11.9",
         "if arch_ver_ge(cuda_major, cuda_minor, 11, 8) == 1 {",
         "if arch_ver_ge(cuda_major, cuda_minor, 11, 9) == 1 {"),
        ("12.9 threshold becomes 13.0",
         'if arch_ver_ge(cuda_major, cuda_minor, 12, 9) == 1 {\n        at = arc_putc(out, at, ARC_SEMI);\n        at = arc_put(out, at, "121a-real");',
         'if arch_ver_ge(cuda_major, cuda_minor, 13, 0) == 1 {\n        at = arc_putc(out, at, ARC_SEMI);\n        at = arc_put(out, at, "121a-real");'),
        ("12.8 threshold becomes 12.7",
         "if arch_ver_ge(cuda_major, cuda_minor, 12, 8) == 1 {",
         "if arch_ver_ge(cuda_major, cuda_minor, 12, 7) == 1 {"),
        ("rewrite matches 13X instead of 12X",
         "if s[from] != 49 || s[from + 1] != 50 || arc_digit(s[from + 2]) == false {",
         "if s[from] != 49 || s[from + 1] != 51 || arc_digit(s[from + 2]) == false {"),
        ("rewrite accepts a four-digit token",
         "    if len == 3 {\n        return 1;\n    }",
         "    if len == 3 || len == 4 {\n        return 1;\n    }"),
        ("resolve validates in element mode",
         'if arc_eq(arch, "native") && arc_valid_impl(native_value, 1) == 1 {',
         'if arc_eq(arch, "native") && arc_valid_impl(native_value, 0) == 1 {'),
        ("list check rejects a bare separator",
         "        if end > i && arc_token_ok_span(s, i, end) == 0 {",
         "        if arc_token_ok_span(s, i, end) == 0 {"),
        ("kind swaps real and virtual",
         'if arc_ends(s, "-virtual") {\n        return 2;\n    }\n    if arc_ends(s, "-real") {\n        return 1;\n    }',
         'if arc_ends(s, "-virtual") {\n        return 1;\n    }\n    if arc_ends(s, "-real") {\n        return 2;\n    }'),
        ("sm returns 0 for a token without digits",
         'if s.len == 0 || arc_digit(s[0]) == false {\n        return -1;\n    }',
         'if s.len == 0 || arc_digit(s[0]) == false {\n        return 0;\n    }'),
    ],
    "tgt": [
        ("flags drop -extended-lambda",
         'at = tgt_put(out, at, "-extended-lambda");', 'at = tgt_put(out, at, "");'),
        ("-lineinfo always present",
         "if debug == 1 {\n        at = tgt_putc(out, at, TGT_SEMI);\n        at = tgt_put(out, at, \"-lineinfo\");\n    }",
         "if debug == 1 || debug == 0 {\n        at = tgt_putc(out, at, TGT_SEMI);\n        at = tgt_put(out, at, \"-lineinfo\");\n    }"),
        ("compress threshold becomes 12.9",
         "if tgt_ver_ge(major, minor, 12, 8) {", "if tgt_ver_ge(major, minor, 12, 9) {"),
        ("fatal warnings drop all-warnings",
         'at = tgt_putc(out, at, TGT_SEMI);\n        at = tgt_put(out, at, "all-warnings");',
         ''),
        ("msvc uses the gcc warning flag",
         'warn = "/Zc:preprocessor";', 'warn = "-Wno-pedantic";'),
        ("host flags ignored when all warnings on",
         "if all_warn == 1 && msvc == 0 && host_flags.len > 0 {",
         "if all_warn == 1 && msvc == 1 && host_flags.len > 0 {"),
        ("NO_FA define inverted",
         "    if fa == 0 {\n        at = tgt_putc(out, at, TGT_SEMI);\n        at = tgt_put(out, at, \"GGML_CUDA_NO_FA\");\n    }",
         "    if fa == 1 {\n        at = tgt_putc(out, at, TGT_SEMI);\n        at = tgt_put(out, at, \"GGML_CUDA_NO_FA\");\n    }"),
        ("FORCE_MMQ defines the CUBLAS macro",
         'at = tgt_put(out, at, "GGML_CUDA_FORCE_MMQ");',
         'at = tgt_put(out, at, "GGML_CUDA_FORCE_CUBLAS");'),
        ("NCCL define without the found bit",
         "if nccl == 1 && nccl_found == 1 {", "if nccl == 1 || nccl_found == 1 {"),
        ("static build drops cublasLt",
         'at = tgt_put(out, at, "CUDA::cublas_static");\n                at = tgt_putc(out, at, TGT_SEMI);\n                at = tgt_put(out, at, "CUDA::cublasLt_static");',
         'at = tgt_put(out, at, "CUDA::cublas_static");'),
        ("cuda_driver linked with NO_VMM",
         "if novmm == 0 {\n        at = tgt_putc(out, at, TGT_SEMI);\n        at = tgt_put(out, at, \"CUDA::cuda_driver\");",
         "if novmm == 0 || novmm == 1 {\n        at = tgt_putc(out, at, TGT_SEMI);\n        at = tgt_put(out, at, \"CUDA::cuda_driver\");"),
        ("windows static links the shared cublas",
         'at = tgt_put(out, at, "CUDA::cublas");\n        first = 0;',
         'at = tgt_put(out, at, "CUDA::cublas_static");\n        first = 0;'),
    ],
    "inst": [
        ("dlopen module becomes a plain library",
         "    if dl == 1 {\n        return 2;\n    }", "    if dl == 1 {\n        return 1;\n    }"),
        ("backend dir never selected",
         "    if dir_set == 1 {\n        return 2;\n    }", "    if dir_set == 1 {\n        return 1;\n    }"),
        ("versioning ignores the Apple module exception",
         "if dl == 1 && apple == 1 {\n        return 0;\n    }", "if dl == 1 && apple == 2 {\n        return 0;\n    }"),
        ("shared install drops the soname link",
         '    at = ins_putc(out, at, INS_SEMI);\n    i = 0;\n    while i < base {\n        at = ins_putc(out, at, out[i]);\n        i = i + 1;\n    }\n    at = ins_put(out, at, ".so");\n    return at;',
         '    return at;'),
        ("static install names a shared object",
         '    if kind == 0 {\n        at = ins_put(out, at, ".a");\n        return at;\n    }',
         '    if kind == 0 {\n        at = ins_put(out, at, ".so");\n        return at;\n    }'),
        ("module install gets a version suffix",
         '    if kind == 2 {\n        at = ins_put(out, at, ".so");\n        return at;\n    }',
         '    if kind == 2 {\n        at = ins_put(out, at, ".so.0");\n        return at;\n    }'),
        ("public shared define dropped from the target list",
         'at = ins_put(out, at, "GGML_BACKEND_BUILD");\n        at = ins_putc(out, at, INS_SEMI);\n        at = ins_put(out, at, "GGML_BACKEND_SHARED");',
         'at = ins_put(out, at, "GGML_BACKEND_BUILD");'),
        ("DL define emitted for a linked build",
         "    if dl == 1 {\n        at = ins_put(out, at, \"GGML_BACKEND_DL\");\n        any = 1;\n    }\n    if shared == 1 {",
         "    if dl == 1 || dl == 0 {\n        at = ins_put(out, at, \"GGML_BACKEND_DL\");\n        any = 1;\n    }\n    if shared == 1 {"),
        ("interface defines leak on a static build",
         'fn ins_public_defs_impl(shared: i64, out: u8[:]) -> i64 {\n    if shared == 0 {\n        return 0;\n    }',
         'fn ins_public_defs_impl(shared: i64, out: u8[:]) -> i64 {\n    if shared == 2 {\n        return 0;\n    }'),
        ("available list appends a duplicate",
         "    if found == 1 {\n        return at;\n    }", "    if found == 2 {\n        return at;\n    }"),
        ("available list replaces instead of appending",
         "    at = ins_putc(out, at, INS_SEMI);\n    return ins_put(out, at, backend);",
         "    return ins_put(out, 0, backend);"),
        ("version drops the patch component",
         "    at = ins_num(out, at, minor);\n    at = ins_putc(out, at, 46);\n    return ins_num(out, at, patch);",
         "    return ins_num(out, at, minor);"),
    ],
}

GATES = {"arch": "FG660_ARCH", "tgt": "FG660_TGT", "inst": "FG660_INST"}


def run_one(slice_name: str, name: str, old: str, new: str) -> tuple[str, str, str]:
    src_name, fix_name = SLICES[slice_name]
    src = EXP / "source" / src_name
    text = src.read_text()
    count = text.count(old)
    if count != 1:
        return ("BAD", name, f"pattern occurs {count} times")
    work = Path(tempfile.mkdtemp(prefix=f"fg660-mut-{slice_name}-"))
    try:
        mutated = work / src_name
        mutated.write_text(text.replace(old, new))
        gen_c = work / "gen.c"
        gen_h = work / (Path(src_name).stem + ".h")
        p = subprocess.run([GOOSE, "--standalone", "-o", str(gen_c), "--header", str(gen_h),
                            str(mutated)], capture_output=True, text=True, check=False)
        if p.returncode != 0:
            return ("REJECT", name, "Goose rejected the mutation")
        obj = work / "gen.o"
        p = subprocess.run([CC, "-O0", "-std=c11", "-w", "-c", str(gen_c), "-o", str(obj)],
                           capture_output=True, text=True, check=False)
        if p.returncode != 0:
            return ("REJECT", name, "mutation does not compile")
        fix = work / "fix.o"
        p = subprocess.run([CXX, "-O2", "-std=c++17", "-I", str(work), "-c",
                            str(HERE / fix_name), "-o", str(fix)],
                           capture_output=True, text=True, check=False)
        if p.returncode != 0:
            return ("BAD", name, "fixture build failed: " + p.stderr[-200:])
        binary = work / "gate"
        p = subprocess.run([CXX, str(fix), str(obj), "-o", str(binary)],
                           capture_output=True, text=True, check=False)
        if p.returncode != 0:
            return ("BAD", name, "link failed: " + p.stderr[-200:])
        p = subprocess.run([str(binary), str(HERE / "cases" / f"{slice_name}.tsv"),
                            str(HERE / f"oracle_{slice_name}.py"), str(work)],
                           capture_output=True, text=True, check=False)
        line = [l for l in p.stdout.splitlines() if GATES[slice_name] in l]
        summary = line[-1] if line else "(no gate line)"
        if any(l.startswith("FAIL") for l in p.stdout.splitlines()):
            return ("CAUGHT", name, summary)
        return ("SURVIVED", name, summary)
    finally:
        shutil.rmtree(work, ignore_errors=True)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("slice")
    ap.add_argument("half", type=int, choices=(1, 2))
    ap.add_argument("--jobs", type=int, default=1)
    args = ap.parse_args()
    table = MUTATIONS[args.slice]
    if len(table) != 12:
        raise SystemExit(f"{args.slice}: expected 12 mutations, found {len(table)}")
    lo = 0 if args.half == 1 else 6
    batch = table[lo:lo + 6]
    caught = survives = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(run_one, args.slice, n, o, w) for n, o, w in batch]
        for fut in futures:
            status, name, detail = fut.result()
            if status == "CAUGHT":
                caught += 1
            else:
                survives += 1
            print(f"{status:<8} {args.slice}  {name:<46} {detail}")
    print(f"{GATES[args.slice]}_MUT half={args.half} caught={caught}/{len(batch)}")
    return 0 if survives == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
