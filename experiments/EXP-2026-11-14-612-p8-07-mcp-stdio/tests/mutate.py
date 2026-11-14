#!/usr/bin/env python3
"""Mutation gate for the MCP stdio transport shells (P8-07 S5). 12 edits in two halves."""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "source", "mcp-stdio.goose")
OUT = os.path.join(ROOT, "build")
GOOSE = os.environ.get("GOOSE", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")
VENDOR = os.environ.get("FG_LLAMA", "/home/end0/AI/LlamaServer/llama.cpp-flint-cuda") + "/vendor"

M = [
    # half 1
    ("an extension pass beats the exact name", """    var p = ms_dirs_pass(out, 0, dirs, command, "");
    p = ms_ext_pass(out, p, dirs, command, pathext);""",
     """    var p = ms_ext_pass(out, 0, dirs, command, pathext);
    p = ms_dirs_pass(out, p, dirs, command, "");"""),
    ("a command with a separator also searches the path", """        if ms_has_ext(command) == false {
            p = ms_cmd_ext_pass(out, p, command, pathext);
        }
        return p;""",
     """        if ms_has_ext(command) == false {
            p = ms_cmd_ext_pass(out, p, command, pathext);
        }
        p = ms_dirs_pass(out, p, dirs, command, "");
        return p;"""),
    ("env overrides are emitted in input order", """        while j > 0 && ms_less_range(overrides, ko[j], kl[j], overrides, ko[j - 1], kl[j - 1]) {""",
     """        while false {"""),
    ("a parent key is never matched against the overrides", """            let eq = ms_find_range(parent, t, e, 61);
            var pk: i64 = 0;
            if eq < e {""",
     """            let eq = ms_find_range(parent, t, e, 61);
            var pk: i64 = 0;
            if false {"""),
    ("a write interrupted by EINTR aborts", """            } else if c0 == 105 && e - t == 1 {
                steps = steps + 1;""",
     """            } else if c0 == 105 && e - t == 1 {
                steps = steps + 1;
                rc = 1;
                stop = 1;"""),
    ("a zero return from write is retried", """            } else if c0 == 48 && e - t == 1 {
                rc = 1;
                steps = steps + 1;
                stop = 1;""",
     """            } else if c0 == 48 && e - t == 1 {
                steps = steps + 1;"""),
    # half 2
    ("a pump line keeps its carriage return", """                    if ll > 0 && buf[ll - 1] == 13 {""",
     """                    if false {"""),
    ("the resync drops the rest of the buffer", """                    bn = ms_erase(buf, bn, nl + 1);
                    skipping = 0;""",
     """                    bn = ms_erase(buf, bn, nl + 1);
                    bn = 0;
                    skipping = 0;"""),
    ("the cooldown includes its deadline instant", """    } else if dead != 0 && now < ddl {""",
     """    } else if dead != 0 && now <= ddl {"""),
    ("a failed start records no cooldown", """            cool = now + 5000;""",
     """            cool = 0;"""),
    ("the warmup budget shrinks to five seconds", """                    let dl = spawn + 10000;""",
     """                    let dl = spawn + 5000;"""),
    ("a duplicate server name is warmed up twice", """            if dup != 0 {""",
     """            if false {"""),
]


def run(cmd, timeout=300):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=timeout)


def main():
    lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
    orig = open(SRC, encoding="utf-8").read()
    caught = 0
    for idx in range(lo, hi):
        label, old, new = M[idx]
        if orig.count(old) != 1:
            print(f"[skip] {idx} {label}: anchor {orig.count(old)}")
            continue
        open(os.path.join(ROOT, "source", "m.goose"), "w", encoding="utf-8").write(orig.replace(old, new, 1))
        g = run(f"{GOOSE} --standalone -o {OUT}/m.c --header {OUT}/g.h {ROOT}/source/m.goose")
        if g.returncode != 0:
            print(f"[{idx}] BUILD-FAIL {label}")
            caught += 1
            continue
        c = run(f"gcc-13 -O0 -std=c11 -w -c {OUT}/m.c -o {OUT}/m.o")
        if c.returncode != 0:
            print(f"[{idx}] BUILD-FAIL {label}")
            caught += 1
            continue
        l = run(f"g++-13 -O1 -std=c++17 -I{OUT} -I{VENDOR} {ROOT}/tests/fixture.cpp "
                f"{OUT}/m.o -o {OUT}/m-gate")
        if l.returncode != 0:
            print(f"[{idx}] BUILD-FAIL {label}")
            caught += 1
            continue
        r = run(f"{OUT}/m-gate")
        if r.returncode == 0:
            print(f"[{idx}] SURVIVED {label}")
        else:
            first = next((ln for ln in r.stdout.splitlines() if ln.startswith("FAIL")), "FAIL")
            print(f"[{idx}] caught {label}: {first}")
            caught += 1
    os.remove(os.path.join(ROOT, "source", "m.goose"))
    for f in ("m.c", "m.h", "m.o", "m-gate"):
        p = os.path.join(OUT, f)
        if os.path.exists(p):
            os.remove(p)
    run(f"{GOOSE} --standalone -o {ROOT}/build/g.c --header {ROOT}/build/g.h {SRC}")
    run(f"gcc-13 -O1 -std=c11 -w -c {ROOT}/build/g.c -o {ROOT}/build/g.o")
    run(f"g++-13 -O1 -std=c++17 -I{ROOT}/build -I{VENDOR} {ROOT}/tests/fixture.cpp "
        f"{ROOT}/build/g.o -o {ROOT}/build/gate")
    print(f"mutations {caught}/{hi - lo}")
    return 0 if caught == hi - lo else 1


if __name__ == "__main__":
    sys.exit(main())
