#!/usr/bin/env python3
# local_build.py - isolated build+link+run of the recurrent fixture against the
# emitted Goose C. Diagnostics and results stay inside worker-3.
import os, subprocess, sys, glob

W = "/work"
B = os.path.join(W, "build")
CC = "/usr/bin/gcc-13"
CXX = "/usr/bin/g++-13"
INC = [
    "-I/inputs/native/src", "-I/inputs/native/include",
    "-I/inputs/native/ggml/include", "-I/inputs/native/ggml/src",
    "-I" + os.path.join(W, "patched"), "-I" + W,
]
MODS = [("cells", "fg153_goose_cells_entry"),
        ("lifecycle", "fg153_goose_lifecycle_entry"),
        ("state", "fg153_goose_state_entry")]

EXTERNS = {
    "cells": """
extern int64_t fg153_rg(int64_t,int64_t,int64_t);
extern void fg153_rw(int64_t,int64_t,int64_t,int64_t);
extern int64_t fg153_rs(int64_t,int64_t,int64_t,int64_t);
extern void fg153_rc(int64_t);
""",
    "lifecycle": """
extern int64_t fg153_lg(int64_t,int64_t,int64_t);
extern void fg153_lp(int64_t,int64_t,int64_t,int64_t);
extern int64_t fg153_ls(int64_t,int64_t,int64_t,int64_t);
extern int64_t fg153_lc(int64_t,int64_t,int64_t,int64_t);
extern void fg153_lw(int64_t);
""",
    "state": """
extern int64_t fg153_sg(int64_t,int64_t,int64_t);
extern void fg153_sp(int64_t,int64_t,int64_t,int64_t);
extern int64_t fg153_ss(int64_t,int64_t,int64_t,int64_t);
extern int64_t fg153_sc(int64_t,int64_t,int64_t,int64_t);
extern void fg153_sw(int64_t);
""",
}

def run(argv, what):
    print("+", " ".join(argv), flush=True)
    p = subprocess.run(argv, capture_output=True, text=True)
    if p.stdout:
        print(p.stdout, flush=True)
    if p.returncode != 0:
        print("FAILED", what, "rc=", p.returncode, file=sys.stderr, flush=True)
        print(p.stderr, file=sys.stderr, flush=True)
        sys.exit(p.returncode)
    return p

def wrapper(mod, sym):
    src = os.path.join(B, mod + ".c")
    text = open(src, "r", encoding="utf-8").read()
    if "gg_function_1(" not in text:
        raise SystemExit("entry gg_function_1 not found in " + src)
    out = os.path.join(B, mod + "_wrap.c")
    with open(out, "w", encoding="utf-8") as f:
        f.write("#include <stdint.h>\n")
        f.write(EXTERNS[mod])
        f.write("#define main fg153_%s_unused_main\n" % mod)
        f.write('#include "%s"\n' % src)
        f.write("#undef main\n")
        f.write("int64_t %s(int64_t a,int64_t b,int64_t c,int64_t d,int64_t e,int64_t f) {\n" % sym)
        f.write("  gg_stacks[1].top = 0;\n")
        f.write("  return gg_function_1(a,b,c,d,e,f,1);\n}\n")
    return out

def main():
    os.makedirs(B, exist_ok=True)
    objs = []
    for mod, sym in MODS:
        w = wrapper(mod, sym)
        o = os.path.join(B, mod + "_wrap.o")
        run([CC, "-std=c11", "-D_GNU_SOURCE", "-O0", "-fPIC",
             "-Werror=implicit-function-declaration", "-c", w, "-o", o] + INC, "compile " + mod)
        objs.append(o)

    seam_o = os.path.join(B, "seam.o")
    run([CXX, "-std=c++17", "-O0", "-fPIC", "-c", os.path.join(W, "patched", "seam.cpp"),
         "-o", seam_o] + INC, "compile seam")
    objs.append(seam_o)

    objs += [os.path.join(B, x) for x in
             ("recurrent.o", "services.o", "hooks.o", "fixture.o")]

    libs = sorted(glob.glob("/runtime/*.so") + glob.glob("/runtime/*.so.*"))
    print("runtime libs:", [os.path.basename(x) for x in libs], flush=True)
    exe = os.path.join(B, "fixture")
    link = [CXX, "-std=c++17", "-O0", "-o", exe] + objs + libs + \
           ["-Wl,-rpath,/runtime", "-lm", "-ldl", "-lpthread"]
    run(link, "link fixture")

    env = dict(os.environ)
    env["LD_LIBRARY_PATH"] = "/runtime"
    print("+ run", exe, flush=True)
    p = subprocess.run([exe], cwd=W, env=env, capture_output=True, text=True)
    sys.stdout.write(p.stdout)
    sys.stderr.write(p.stderr)
    if p.returncode != 0:
        print("FIXTURE FAILED rc=", p.returncode, file=sys.stderr, flush=True)
        sys.exit(p.returncode)
    print("FIXTURE PASS", flush=True)

if __name__ == "__main__":
    main()
