#!/usr/bin/env python3
import os, subprocess, sys, glob
W="/work/canonical-context-io-adapter"; B=os.path.join(W,"build")
P="/inputs/parent-canonical-io-current"
INC=["-I"+W,"-I"+P+"/canonical-io","-I/inputs/native/ggml/include","-I/inputs/native/include","-I/inputs/native/src"]
def run(a,w):
    print("+"," ".join(a),flush=True); p=subprocess.run(a,capture_output=True,text=True)
    if p.stdout: print(p.stdout,flush=True)
    if p.returncode: print("FAILED",w,p.returncode,file=sys.stderr); print(p.stderr,file=sys.stderr); sys.exit(p.returncode)
def wrap():
    src=os.path.join(B,"fgcio-context.c"); out=os.path.join(B,"wrap.c")
    with open(out,"w") as f:
        f.write("#include <stdint.h>\n")
        for s in ["fgc_record_new(int64_t)","fgc_record_get(int64_t,int64_t)","fgc_record_set(int64_t,int64_t,int64_t)",
                  "fgc_i64_record_free(int64_t)","fgc_vec_new(int64_t)","fgc_vec_len(int64_t)","fgc_i64_vec_data(int64_t)",
                  "fgc_vec_resize(int64_t,int64_t)","fgc_i64_vec_free(int64_t)","fgc_map_new(void)","fgc_map_put(int64_t,int64_t,int64_t)",
                  "fgc_i64_map_get(int64_t,int64_t,int64_t)","fgc_map_erase(int64_t,int64_t)","fgc_i64_map_free(int64_t)",
                  "fgcio_create(int64_t,int64_t,int64_t,int64_t)","fgcio_dispose(int64_t)","fgcio_raw(int64_t,int64_t,int64_t,int64_t,int64_t)"]:
            f.write("extern int64_t %s;\n"%s)
        f.write("#define main fgcio_ctx_unused_main\n#include \"%s\"\n#undef main\n"%src)
        f.write("int64_t fgcio_context_entry(int64_t a,int64_t b,int64_t c,int64_t d,int64_t e,int64_t f){\n")
        f.write("  gg_stacks[1].top=0; return gg_function_1(a,b,c,d,e,f,1);}\n")
    return out
def main():
    os.makedirs(B,exist_ok=True); objs=[]
    w=wrap(); o=os.path.join(B,"wrap.o"); run(["/usr/bin/gcc-13","-std=c11","-D_GNU_SOURCE","-O0","-fPIC","-Werror=implicit-function-declaration","-c",w,"-o",o]+INC,"wrap"); objs.append(o)
    cw=os.path.join(B,"wrap_canon.c")
    with open(cw,"w") as f:
        f.write("#include <stdint.h>\n")
        for s in ["fgc_record_new(int64_t)","fgc_record_get(int64_t,int64_t)","fgc_record_set(int64_t,int64_t,int64_t)",
                  "fgc_i64_record_free(int64_t)","fgc_vec_new(int64_t)","fgc_vec_len(int64_t)","fgc_i64_vec_data(int64_t)",
                  "fgc_vec_resize(int64_t,int64_t)","fgc_i64_vec_free(int64_t)","fgc_map_new(void)","fgc_map_put(int64_t,int64_t,int64_t)",
                  "fgc_i64_map_get(int64_t,int64_t,int64_t)","fgc_map_erase(int64_t,int64_t)","fgc_i64_map_free(int64_t)",
                  "fgcio_raw(int64_t,int64_t,int64_t,int64_t,int64_t)",
                  "fgc_set_new(void)","fgc_set_insert(int64_t,int64_t)","fgc_set_contains(int64_t,int64_t)","fgc_i64_set_free(int64_t)"]:
            f.write("extern int64_t %s;\n"%s)
        f.write('#define main fg153_canon_unused_main\n#include "%s/canonical-io/generated.c"\n#undef main\n'%P)
        f.write("int64_t fg153_goose_canonical_entry(int64_t a,int64_t b,int64_t c,int64_t d,int64_t e,int64_t f){\n")
        f.write("  gg_stacks[1].top=0; return gg_function_1(a,b,c,d,e,f,1);}\n")
    co=os.path.join(B,"wrap_canon.o"); run(["/usr/bin/gcc-13","-std=c11","-D_GNU_SOURCE","-O0","-fPIC","-Werror=implicit-function-declaration","-c",cw,"-o",co]+INC,"wrap_canon"); objs.append(co)
    o=os.path.join(B,"abi_context.o"); run(["/usr/bin/gcc-13","-std=c11","-D_GNU_SOURCE","-O0","-fPIC","-c",os.path.join(W,"abi_context.c"),"-o",o]+INC,"abi_context"); objs.append(o)
    for src,out in [("fgcio-context-adapter.cpp","adapter.o"),("fixture.cpp","fixture.o")]:
        o=os.path.join(B,out); run(["/usr/bin/g++-13","-std=c++17","-O0","-fPIC","-c",os.path.join(W,src),"-o",o]+INC,out); objs.append(o)
    for name in ["io-abi.o","io-real-effects.o","io-set-abi.o"]:
        objs.append(os.path.join(P,"lib",name))
    objs.append(os.path.join(P,"lib","libfgc-apex-storage.a"))
    exe=os.path.join(B,"fixture"); libs=sorted(glob.glob("/runtime/*.so"))
    run(["/usr/bin/g++-13","-std=c++17","-O0","-o",exe]+objs+["-Wl,-rpath,/runtime","-lm","-ldl","-lpthread"]+libs,"link")
    env=dict(os.environ); env["LD_LIBRARY_PATH"]="/runtime"
    p=subprocess.run([exe],cwd=W,env=env,capture_output=True,text=True)
    sys.stdout.write(p.stdout); sys.stderr.write(p.stderr)
    if p.returncode: print("FIXTURE FAILED",p.returncode,file=sys.stderr); sys.exit(p.returncode)
    print("FIXTURE PASS",flush=True)
if __name__=="__main__": main()
