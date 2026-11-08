#!/usr/bin/env python3
# local_build.py - isolated build/link/run for common-memory-io.
import os, subprocess, sys, glob

W = "/work/common-memory-io"
B = os.path.join(W, "build")
CC = "/usr/bin/gcc-13"
CXX = "/usr/bin/g++-13"
INC = [
    "-I" + W, "-I" + os.path.join(W, "patched"),
    "-I/inputs/native/src", "-I/inputs/native/include",
    "-I/inputs/native/ggml/include", "-I/inputs/native/ggml/src",
]
CXXFLAGS = [CXX, "-std=c++17", "-O0", "-fPIC", "-c"]

def run(argv, what):
    print("+", " ".join(argv), flush=True)
    p = subprocess.run(argv, capture_output=True, text=True)
    if p.stdout: print(p.stdout, flush=True)
    if p.returncode != 0:
        print("FAILED", what, "rc=", p.returncode, file=sys.stderr, flush=True)
        print(p.stderr, file=sys.stderr, flush=True)
        sys.exit(p.returncode)
    return p

def wrapper():
    src = os.path.join(B, "common-memory.c")
    if "gg_function_1(" not in open(src, encoding="utf-8").read():
        raise SystemExit("entry gg_function_1 missing in " + src)
    out = os.path.join(B, "common_wrap.c")
    with open(out, "w", encoding="utf-8") as f:
        f.write("#include <stdint.h>\n")
        f.write("extern int64_t fg153_cg(int64_t,int64_t,int64_t);\n")
        f.write("extern void fg153_cp(int64_t,int64_t,int64_t,int64_t);\n")
        f.write("extern int64_t fg153_cs(int64_t,int64_t,int64_t,int64_t);\n")
        f.write("extern void fg153_cw(int64_t);\n")
        f.write("#define main fg153_common_unused_main\n")
        f.write('#include "%s"\n' % src)
        f.write("#undef main\n")
        f.write("int64_t fg153_goose_common_entry(int64_t a,int64_t b,int64_t c,int64_t d,int64_t e,int64_t f) {\n")
        f.write("  gg_stacks[1].top = 0;\n")
        f.write("  return gg_function_1(a,b,c,d,e,f,1);\n}\n")
    return out

def main():
    os.makedirs(B, exist_ok=True)
    objs = []
    w = wrapper()
    o = os.path.join(B, "common_wrap.o")
    run([CC, "-std=c11", "-D_GNU_SOURCE", "-O0", "-fPIC",
         "-Werror=implicit-function-declaration", "-c", w, "-o", o] + INC, "compile common-memory.c")
    objs.append(o)

    for src, out, extra in [
        ("patched/llama-memory.cpp", "memory.o", []),
        ("patched/llama-io.cpp", "io.o", []),
        ("services.cpp", "services.o", []),
        ("fg153-common-hooks.cpp", "hooks.o", []),
        ("fixture.cpp", "fixture.o", []),
    ]:
        o = os.path.join(B, out)
        run(CXXFLAGS + [os.path.join(W, src), "-o", o] + INC + extra, "compile " + src)
        objs.append(o)

    exe = os.path.join(B, "fixture")
    libs = sorted(glob.glob("/runtime/*.so"))
    run([CXX, "-std=c++17", "-O0", "-o", exe] + objs +
        ["-Wl,-rpath,/runtime", "-lm", "-ldl", "-lpthread"] + libs, "link fixture")

    env = dict(os.environ); env["LD_LIBRARY_PATH"] = "/runtime"
    print("+ run", exe, flush=True)
    p = subprocess.run([exe], cwd=W, env=env, capture_output=True, text=True)
    sys.stdout.write(p.stdout); sys.stderr.write(p.stderr)
    if p.returncode != 0:
        print("FIXTURE FAILED rc=", p.returncode, file=sys.stderr, flush=True)
        sys.exit(p.returncode)
    print("FIXTURE PASS", flush=True)

if __name__ == "__main__":
    main()
