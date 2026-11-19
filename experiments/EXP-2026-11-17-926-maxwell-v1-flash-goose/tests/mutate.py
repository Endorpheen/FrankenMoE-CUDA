#!/usr/bin/env python3
"""Mutation gate for the Maxwell decision module.

Each row breaks one decision and the gate must fail. A mutation that survives
means the gate does not see that decision.
"""
import os
import subprocess
import sys
import tempfile

E = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(E)))
SRC = os.path.join(E, "source", "runtime.goose")
FIX = os.path.join(E, "tests", "fixture.cpp")
GOOSE = os.environ.get(
    "FG_GOOSE", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose"
)
OUT = os.path.join(ROOT, "runtime", os.path.basename(E), "mut")

MUTATIONS = [
    ("router keeps raw score weights",
     "w[k] = mx_sig(idx[k], logits) / sum;", "w[k] = mx_sig(idx[k], logits);"),
    ("router ignores expert bias",
     "let v: f32 = s + bias[i];", "let v: f32 = s;"),
    ("sinkhorn skips the first column norm",
     "    // col norm\n    var c2: i64 = 0;", "    // col norm\n    var c2: i64 = n;"),
    ("sinkhorn runs one iteration too few",
     "    var it: i64 = 1;\n    while it < iters {", "    var it: i64 = 2;\n    while it < iters {"),
    ("route accepts a block that ends past the local window",
     "if bstart >= prefix_end && bend <= local_start {", "if bstart >= prefix_end && bend < local_start {"),
    ("route lets the prefix blocks be selected as remote",
     "if bstart >= prefix_end && bend <= local_start {", "if bstart > prefix_end && bend <= local_start {"),
    ("route_final shifts the local window by one block",
     "let blk: i64 = cur_block + (1 - local_blocks + j);", "let blk: i64 = cur_block + (0 - local_blocks + j);"),
    ("route_final lets the prefix spill past the current block",
     "if i <= cur_block {", "if i < cur_block {"),
    ("mtp roll writes zero instead of the pad id",
     "    out[n - 1] = pad_id;", "    out[n - 1] = 0;"),
    ("hc post gate loses the factor of two",
     "post[i] = mx_sigmoid(hq) * 2.0;", "post[i] = mx_sigmoid(hq);"),
    ("hc pre gate drops the eps floor",
     "pre[i] = mx_sigmoid(hp) + eps;", "pre[i] = mx_sigmoid(hp);"),
    ("hc head ignores its scale",
     "out[i] = mx_sigmoid(mixes[i] * scale + base) + eps;", "out[i] = mx_sigmoid(mixes[i] + base) + eps;"),
    ("geometry accepts an unaligned prefix window",
     "if prefix_win % blk != 0 {\n        return -6;", "if false {\n        return -6;"),
    ("selected count does not clamp to the block count",
     "if r > num_blocks {\n        r = num_blocks;\n    }", "if false {\n        r = num_blocks;\n    }"),
]


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def main():
    src = open(SRC).read()
    os.makedirs(OUT, exist_ok=True)
    survived = []
    for name, old, new in MUTATIONS:
        if src.count(old) != 1:
            print(f"BAD TABLE {name}: {src.count(old)} matches")
            return 2
        mutated = src.replace(old, new)
        with tempfile.NamedTemporaryFile("w", suffix=".goose", delete=False) as f:
            f.write(mutated)
            mpath = f.name
        gen = os.path.join(OUT, "runtime-gen.c")
        hdr = os.path.join(OUT, "runtime-gen.h")
        r = run([GOOSE, "--standalone", "-O0", "-o", gen, "--header", hdr, mpath])
        if r.returncode != 0:
            print(f"BAD GEN {name}: {r.stderr.strip()[:200]}")
            survived.append(name)
            continue
        obj = os.path.join(OUT, "m.o")
        r = run(["gcc-13", "-O0", "-std=c11", "-c", gen, "-o", obj])
        if r.returncode != 0:
            print(f"BAD CC {name}: {r.stderr.strip()[:200]}")
            survived.append(name)
            continue
        bin_path = os.path.join(OUT, "gate-m")
        r = run(["g++-13", "-O0", "-std=c++17", "-I", os.path.dirname(gen),
                 FIX, obj, "-lm", "-o", bin_path])
        if r.returncode != 0:
            print(f"BAD LINK {name}: {r.stderr.strip()[:200]}")
            survived.append(name)
            continue
        r = run([bin_path])
        killed = r.returncode != 0 and "failures=0" not in r.stdout
        print(f"{'killed  ' if killed else 'SURVIVED'} {name}")
        if not killed:
            survived.append(name)
        os.unlink(mpath)
    print(f"mutations={len(MUTATIONS)} survived={len(survived)}")
    return 0 if not survived else 1


if __name__ == "__main__":
    sys.exit(main())
