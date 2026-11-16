#!/usr/bin/env python3
"""Mutation run for the P10 wiring: 12 mutations per wiring, two halves of six.

    python3 gates/mutate.py 1     # six mutations for each of the four wirings
    python3 gates/mutate.py 2     # the other six

Each mutation rewrites one decision in one accepted module, regenerates that
module at -O0, relinks its host over build/goose-hosts/<name> and reruns that
wiring's gate. A survivor is a hole in the wiring: the gate cannot see a wrong
decision. Builds are serial on purpose, and the pristine host is restored at the
end.

Entries marked `new` were written for this experiment because the accepted
P10-05 tables also mutate decisions the wiring does not call
(tools/check-goose-tree.py, prepare_upstreams.sh); those would survive for the
wrong reason.
"""
from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

sys.dont_write_bytecode = True
EXP = Path(__file__).resolve().parent.parent
ROOT = EXP.parent.parent
HOSTS = ROOT / "build" / "goose-hosts"
WORK = EXP / "build" / "mut"
GOOSE = "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose"
CC = "gcc-13"

MODULE = {
    "art": ROOT / "goose/misc/generated-artifact-policy.goose",
    "env": ROOT / "goose/misc/build-env.goose",
    "pkg": ROOT / "goose/misc/release-plan.goose",
    "fx":  ROOT / "goose/misc/fixture-plan.goose",
}
GATE = {
    "art": ["gates/w1_art.py"],
    "env": ["gates/w2_env.py"],
    "pkg": ["gates/w3_pkg.py", "--skip-e2e"],
    "fx":  ["gates/w4_fx.py"],
}
TAG = {"art": "FG663_ART", "env": "FG663_ENV", "pkg": "FG663_PKG", "fx": "FG663_FX"}

HALF1 = [
    # ── pkg, from EXP-2026-11-15-645 half 1 ────────────────────────────────
    ("pkg", "tag prefix accepts w",
     "    if s.len < 1 || s[0] != 118 {", "    if s.len < 1 || s[0] != 119 {"),
    ("pkg", "prerelease marker renamed",
     '    if rlp_find(s, 0, "-rc.") >= 0 {', '    if rlp_find(s, 0, "-rc") >= 0 {'),
    ("pkg", "rc suffix off by one",
     "    p = rlp_digits(s, p + 4);", "    p = rlp_digits(s, p + 3);"),
    ("pkg", "asset limit is inclusive",
     "    if size > RLP_ASSET_LIMIT {", "    if size >= RLP_ASSET_LIMIT {"),
    ("pkg", "tree limit is ten MiB short",
     "const RLP_TREE_LIMIT: i64 = 100000000;", "const RLP_TREE_LIMIT: i64 = 10000000;"),
    ("pkg", "keep-old flag ignored",
     "    if has_runtime == 0 || keep_old != 0 || tag_present == 0 || same_version != 0 {",
     "    if has_runtime == 0 || tag_present == 0 || same_version != 0 {"),

    # ── env, from EXP-2026-11-15-645 half 1 ───────────────────────────────
    ("env", "empty override is not the default",
     "    if set == 0 || val.len == 0 {\n        return 1;\n    }",
     "    if set == 0 {\n        return 1;\n    }"),
    ("env", "build directory renamed",
     'return ev_put(out, at, "build/franken-cuda");',
     'return ev_put(out, at, "build/franken-cuda-os");'),
    ("env", "parallel default is four",
     '        return ev_put(out, 0, "8");', '        return ev_put(out, 0, "4");'),
    ("env", "c compiler default moved back",
     'at = ev_put(out, at, "gcc-13");', 'at = ev_put(out, 0, "gcc-12");'),
    ("env", "multi-config default leaks",
     "    if (set == 0 || val.len == 0) && multi == 0 {",
     "    if (set == 0 || val.len == 0) && multi == 1 {"),
    ("env", "tests subdirectory bit folded into cli",
     "    if tests != 0 && llama != 0 {\n        m = m + 4;\n    }",
     "    if tests != 0 && llama != 0 {\n        m = m + 2;\n    }"),

    # ── art, from EXP-2026-11-15-645 half 1 (all three are wired) ─────────
    ("art", "generated banner needs no full stop",
     'gap_contains(head, "Do not edit.")', 'gap_contains(head, "Do not edit")'),
    ("art", "cuh suffix misspelled",
     '    if gap_ends(path, ".cuh") && gap_contains_lower(path, "generated") {',
     '    if gap_ends(path, ".cuhx") && gap_contains_lower(path, "generated") {'),
    ("art", "cuh path test is case sensitive",
     'gap_contains_lower(path, "generated")', 'gap_contains(path, "generated")'),
    ("art", "limit converts with 1024",
     "    return mb_milli * 1048576 / 1000;", "    return mb_milli * 1048576 / 1024;"),
    ("art", "size limit is exclusive",
     "    if all_flag != 0 || size >= limit {", "    if all_flag != 0 || size > limit {"),
    ("art", "gate always warns",
     "    if all_flag != 0 || size >= limit {\n        return 2;\n    }",
     "    if all_flag != 0 || size >= limit {\n        return 1;\n    }"),

    # ── fx, new ───────────────────────────────────────────────────────────
    ("fx", "qwen tensor count loses a layer entry",
     "    if arch == FX_QWEN {\n        return 3 + FX_N_LAYER * 12;\n    }",
     "    if arch == FX_QWEN {\n        return 3 + FX_N_LAYER * 11;\n    }"),
    ("fx", "gemma tensor count loses a layer entry",
     "    return 3 + 13 + (FX_N_LAYER - 1) * 20;", "    return 3 + 13 + (FX_N_LAYER - 1) * 19;"),
    ("fx", "token embedding seed off",
     'return fx_row(out, 0 - 1, "token_embd.weight", FX_VOCAB, FX_N_EMBD, 0, 1);',
     'return fx_row(out, 0 - 1, "token_embd.weight", FX_VOCAB, FX_N_EMBD, 0, 4);'),
    ("fx", "output norm seed off",
     'return fx_row(out, 0 - 1, "output_norm.weight", FX_N_EMBD, 0, 0, 2);',
     'return fx_row(out, 0 - 1, "output_norm.weight", FX_N_EMBD, 0, 0, 5);'),
    ("fx", "attn_q loses its head stack",
     'return fx_row(out, layer, "attn_q.weight", FX_N_EMBD_HEAD * FX_N_HEAD, FX_N_EMBD, 0, s + 1);',
     'return fx_row(out, layer, "attn_q.weight", FX_N_EMBD_HEAD, FX_N_EMBD, 0, s + 1);'),
    ("fx", "per-layer seed base is ten",
     "    let s: i64 = 100 + 100 * layer;", "    let s: i64 = 100 + 10 * layer;"),
]

HALF2 = [
    # ── pkg, from EXP-2026-11-15-645 half 2 ───────────────────────────────
    ("pkg", "asset selector drops the prefix test",
     "    if rlp_selected(name) == 0 {\n        return 1;\n    }",
     "    if name.len == 0 {\n        return 1;\n    }"),
    ("pkg", "sums separator is one space",
     "        at = rlp_putc(out, at, RLP_SPACE);\n        at = rlp_putc(out, at, RLP_SPACE);",
     "        at = rlp_putc(out, at, RLP_SPACE);"),
    ("pkg", "manifest key renamed",
     '    at = rlp_put(out, at, ",\\n  \\"object_bytes\\": ");',
     '    at = rlp_put(out, at, ",\\n  \\"object_bytes_x\\": ");'),
    ("pkg", "leading whitespace not required",
     "    if lead == 0 {\n        return 0;\n    }", "    if lead < 0 {\n        return 0;\n    }"),
    ("pkg", "dist header name changed",
     '    at = rlp_put(out, at, "main.h\\n");', '    at = rlp_put(out, at, "main.hpp\\n");'),
    ("pkg", "basename keeps the trailing slash",
     "    while end > 0 && s[end - 1] == RLP_SLASH {\n        end = end - 1;\n    }",
     "    while end > 0 && s[end - 1] == RLP_SLASH && end > 1 {\n        end = end - 1;\n    }"),

    # ── env, new: the configure vector and the subdirectory mask ──────────
    ("env", "cxx default moved forward",
     'at = ev_put(out, at, "g++-13");', 'at = ev_put(out, at, "g++-14");'),
    ("env", "cuda compiler default moved",
     'at = ev_put(out, at, "/usr/bin/nvcc");', 'at = ev_put(out, at, "/usr/bin/nvcc-12");'),
    ("env", "cuda architecture default moved",
     'at = ev_put(out, at, "89");', 'at = ev_put(out, at, "90");'),
    ("env", "generator renamed",
     '    at = ev_put(out, at, "\\n-G\\nNinja\\n-DCMAKE_BUILD_TYPE=Release\\n-DCMAKE_C_COMPILER=");',
     '    at = ev_put(out, at, "\\n-G\\nUnix Makefiles\\n-DCMAKE_BUILD_TYPE=Release\\n-DCMAKE_C_COMPILER=");'),
    ("env", "tools subdirectory bit doubled",
     "    if tools != 0 {\n        m = m + 8;\n    }", "    if tools != 0 {\n        m = m + 16;\n    }"),
    ("env", "default build type lower-cased",
     '        return ev_put(out, 0, "Release");', '        return ev_put(out, 0, "release");'),

    # ── art, new: the three wired exports only ────────────────────────────
    ("art", "c suffix accepts .cc",
     'if gap_ends(path, ".c") {', 'if gap_ends(path, ".cc") {'),
    ("art", "banner word changed",
     'gap_at(head, 0, "/* Generated by the Goose compiler from ")',
     'gap_at(head, 0, "/* Generated by the Goose compiler for ")'),
    ("art", "unbannered .c treated as generated",
     "            return 1;\n        }\n        return 0;\n    }",
     "            return 1;\n        }\n        return 1;\n    }"),
    ("art", "limit divides by a hundred",
     "    return mb_milli * 1048576 / 1000;", "    return mb_milli * 1048576 / 100;"),
    ("art", "all flag needs two",
     "    if all_flag != 0 || size >= limit {", "    if all_flag > 1 || size >= limit {"),
    ("art", "edit marker must open the head",
     'gap_contains(head, "Do not edit.")', 'gap_at(head, 0, "Do not edit.")'),

    # ── fx, new ───────────────────────────────────────────────────────────
    ("fx", "attn_k_norm seed collides with attn_q_norm",
     'return fx_row(out, layer, "attn_k_norm.weight", FX_N_EMBD_HEAD, 0, 0, s + 6);',
     'return fx_row(out, layer, "attn_k_norm.weight", FX_N_EMBD_HEAD, 0, 0, s + 5);'),
    ("fx", "fused gate_up loses half its width",
     'return fx_row(out, layer, "ffn_gate_up_exps.weight", FX_N_EXPERT, 2 * FX_N_FF_EXP, FX_N_EMBD, s + 18);',
     'return fx_row(out, layer, "ffn_gate_up_exps.weight", FX_N_EXPERT, FX_N_FF_EXP, FX_N_EMBD, s + 18);'),
    ("fx", "qwen expert slot taken by a norm",
     "    if arch == FX_QWEN {\n        // k 9,10,11 are the split expert tensors.\n        if k == 9 {",
     "    if arch == FX_QWEN {\n        // k 9,10,11 are the split expert tensors.\n        if k == 12 {"),
    ("fx", "banner says layer not layers",
     '        p = fx_put_s(out, p, " layers, ");', '        p = fx_put_s(out, p, " layer, ");'),
    ("fx", "split refusal message reworded",
     'let msg: const u8[:] = "--split-max-tensors is exercised via the qwen3moe fixture only";',
     'let msg: const u8[:] = "--split-max-tensors is exercised via the qwen3moe fixture";'),
    ("fx", "sharded writer drops the small first shard",
     "    if split_max_tensors != 0 {\n        return 1;\n    }",
     "    if split_max_tensors != 0 {\n        return 0;\n    }"),
]


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, check=False, **kw)


def main() -> int:
    half = sys.argv[1] if len(sys.argv) > 1 else ""
    table = {"1": HALF1, "2": HALF2}.get(half)
    if table is None:
        print("usage: mutate.py 1|2 [art,env,pkg,fx]", file=sys.stderr)
        return 2
    only = set(sys.argv[2].split(",")) if len(sys.argv) > 2 else {"art", "env", "pkg", "fx"}
    table = [row for row in table if row[0] in only]
    WORK.mkdir(parents=True, exist_ok=True)
    caught = 0
    backup = {}
    try:
        for i, (name, why, old, new) in enumerate(table):
            src_path = MODULE[name]
            src = src_path.read_text()
            if src.count(old) != 1:
                print(f"BAD MUTATION {name}: pattern count {src.count(old)}: {why}")
                return 2
            d = WORK / f"{half}-{i}-{name}"
            if d.exists():
                shutil.rmtree(d)
            d.mkdir(parents=True)
            # The header is named after the unit, so `<name>-main.c` can include
            # the same `<name>.h` it includes from the real build directory.
            mutated = d / f"{name}-unit.goose"
            mutated.write_text(src.replace(old, new))
            r = run([GOOSE, "--standalone", "-O0", "-o", str(d / f"{name}.c"),
                     "--header", str(d / f"{name}.h"), str(mutated)])
            if r.returncode != 0:
                print(f"BAD MUTATION {name}: does not compile: {why}")
                print((r.stdout + r.stderr)[-500:])
                return 2
            r = run([CC, "-O0", "-std=c11", "-w", "-c", str(d / f"{name}.c"), "-o", str(d / f"{name}.o")])
            if r.returncode != 0:
                print(f"BAD MUTATION {name}: C does not compile: {why}")
                print(r.stderr[-500:])
                return 2
            exe = d / name
            r = run([CC, "-O0", "-std=c11", f"-I{d}", str(ROOT / "tools/goose-hosts" / f"{name}-main.c"),
                     str(d / f"{name}.o"), "-lm", "-o", str(exe)])
            if r.returncode != 0:
                print(f"BAD MUTATION {name}: link failed: {why}")
                print(r.stderr[-500:])
                return 2

            target = HOSTS / name
            if name not in backup:
                backup[name] = target.read_bytes()
            shutil.copy2(exe, target)
            q = run([sys.executable, *GATE[name]], cwd=str(EXP),
                    env={**os.environ, "BMOE_GOOSE_MUTANT": "1"})
            killed = q.returncode != 0
            lines = [l for l in (q.stdout + q.stderr).splitlines() if l.startswith(TAG[name])]
            # Name the gate line that actually failed, not whichever printed last.
            failing = [l for l in lines if "failures=0" not in l]
            print(f"{'CAUGHT  ' if killed else 'SURVIVED'} {name:3s} {why:46s} "
                  f"{(failing or lines or [''])[-1]}")
            if killed:
                caught += 1
    finally:
        for name, blob in backup.items():
            (HOSTS / name).write_bytes(blob)
            (HOSTS / name).chmod(0o755)
    print(f"half {half}: {caught}/{len(table)} mutations caught")
    return 0 if caught == len(table) else 1


if __name__ == "__main__":
    raise SystemExit(main())
