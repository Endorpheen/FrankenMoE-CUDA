#!/usr/bin/env python3
import glob, os, subprocess, sys

W = "/work/canonical-io"; B = os.path.join(W, "build")
CC = "/usr/bin/gcc-13"
INC = ["-I" + W]
CFLAGS = [CC, "-std=c11", "-D_GNU_SOURCE", "-O0", "-fPIC", "-Werror=implicit-function-declaration"]

def run(argv, what):
    print("+", " ".join(argv), flush=True)
    p = subprocess.run(argv, capture_output=True, text=True)
    if p.stdout: print(p.stdout, flush=True)
    if p.returncode:
        print("FAILED", what, p.returncode, file=sys.stderr, flush=True)
        print(p.stderr, file=sys.stderr, flush=True); sys.exit(p.returncode)

def wrapper():
    src = os.path.join(B, "canonical-io.c")
    out = os.path.join(B, "wrap.c")
    with open(out, "w", encoding="utf-8") as f:
        f.write("#include <stdint.h>\n")
        f.write("extern int64_t fgc_record_new(int64_t);\n")
        f.write("extern int64_t fgc_record_get(int64_t,int64_t);\n")
        f.write("extern int64_t fgc_record_set(int64_t,int64_t,int64_t);\n")
        f.write("extern int64_t fgc_record_free(int64_t);\n")
        f.write("extern int64_t fgc_vec_new(int64_t);\n")
        f.write("extern int64_t fgc_vec_len(int64_t);\n")
        f.write("extern int64_t fgc_vec_data(int64_t);\n")
        f.write("extern int64_t fgc_vec_resize(int64_t,int64_t);\n")
        f.write("extern int64_t fgc_vec_free(int64_t);\n")
        f.write("extern int64_t fgc_map_new(void);\n")
        f.write("extern int64_t fgc_map_free(int64_t);\n")
        f.write("extern int64_t fgc_set_new(void);\n")
        f.write("extern int64_t fgc_set_insert(int64_t,int64_t);\n")
        f.write("extern int64_t fgc_set_contains(int64_t,int64_t);\n")
        f.write("extern int64_t fgc_set_free(int64_t);\n")
        f.write("extern int64_t fgcio_raw(int64_t,int64_t,int64_t,int64_t,int64_t);\n")
        f.write("#define main fg153_canonical_unused_main\n")
        f.write('#include "%s"\n' % src)
        f.write("#undef main\n")
        f.write("int64_t fg153_goose_canonical_entry(int64_t a,int64_t b,int64_t c,int64_t d,int64_t e,int64_t f){\n")
        f.write("  gg_stacks[1].top=0; return gg_function_1(a,b,c,d,e,f,1);}\n")
    return out

def main():
    os.makedirs(B, exist_ok=True)
    w = wrapper(); objs = []
    for src, out in [(w, "wrap.o"), (os.path.join(W, "abi.c"), "abi.o"),
                     (os.path.join(W, "fixture.c"), "fixture.o")]:
        o = os.path.join(B, out)
        run(CFLAGS + ["-c", src, "-o", o] + INC, os.path.basename(src))
        objs.append(o)
    # real worker8 canonical storage + primitives
    S = "/inputs/canonical-storage-ready"
    SINC = ["-I" + S]
    for mod, out in [("record","rec.o"),("vector","vec.o"),("map","map.o"),("set","set.o")]:
        src = S + "/fgc-" + mod + "-unit.c"
        o = os.path.join(B, out)
        run([CC,"-std=c11","-D_GNU_SOURCE","-O0","-fPIC","-Dmain=fgc_"+mod+"_unit_main",
             "-c",src,"-o",o]+SINC, out)
        objs.append(o)
    o = os.path.join(B,"ssvc.o")
    run([CC,"-std=c11","-D_GNU_SOURCE","-O0","-fPIC","-c",S+"/canonical-storage-services.c","-o",o]+SINC,"ssvc")
    objs.append(o)
    eff = os.path.join(B,"effects.o")
    run(["/usr/bin/g++-13","-std=c++17","-O0","-fPIC","-c",os.path.join(W,"test-effects.cpp"),"-o",eff]+INC, "effects")
    objs.append(eff)
    exe = os.path.join(B, "fixture")
    run(["/usr/bin/g++-13","-std=c++17","-O0","-o",exe]+objs+["-lm","-ldl","-lpthread"], "link")
    env = dict(os.environ); env["LD_LIBRARY_PATH"] = "/runtime"
    p = subprocess.run([exe], cwd=W, env=env, capture_output=True, text=True)
    sys.stdout.write(p.stdout); sys.stderr.write(p.stderr)
    if p.returncode:
        print("FIXTURE FAILED", p.returncode, file=sys.stderr, flush=True); sys.exit(p.returncode)
    print("FIXTURE PASS", flush=True)

if __name__ == "__main__":
    main()
