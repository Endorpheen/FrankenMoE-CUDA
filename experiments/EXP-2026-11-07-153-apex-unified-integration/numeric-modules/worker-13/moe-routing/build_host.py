#!/usr/bin/env python3
# build_host.py - isolated host build of the Goose MoE routing wrappers and the
# native-vs-Goose differential fixture.
import os, subprocess, sys
HERE = "/work/moe-routing"
B = os.path.join(HERE, "build")
CC = "/usr/bin/gcc-13"
INC = ["-I/work", "-I" + HERE]
SORT = r'''
#include <stdint.h>
#include <stddef.h>
#include "fg153-moe-abi.h"
extern int64_t fg153_h_tid;
extern int64_t fg153_h_bidx[3];
extern int64_t fg153_h_bdim;
extern int32_t *fg153_h_sh;
extern int64_t fg153_h_shcap;
int64_t fg153_moe_cfg_i64(int64_t c, int64_t k);
float   fg153_moe_load(int64_t p, int64_t i);
void    fg153_moe_store_i64(int64_t p, int64_t i, int64_t v);
int64_t fg153_moe_shload(int64_t i);
void    fg153_moe_shstore(int64_t i, int64_t v);
int64_t fg153_moe_tid(void);
int64_t fg153_moe_bdim(void);
int64_t fg153_moe_bidx(int64_t a);
int64_t fg153_moe_reject(int64_t c, int64_t code);
int64_t fg153_moe_cfg_i64(int64_t c, int64_t k) { return ((const int64_t *)(uintptr_t)c)[k]; }
float fg153_moe_load(int64_t p, int64_t i) { return ((const float *)(uintptr_t)p)[i]; }
void fg153_moe_store_i64(int64_t p, int64_t i, int64_t v) { ((int *)(uintptr_t)p)[i] = (int)v; }
int64_t fg153_moe_shload(int64_t i) { return (int64_t)fg153_h_sh[i]; }
void fg153_moe_shstore(int64_t i, int64_t v) { fg153_h_sh[i] = (int32_t)v; }
int64_t fg153_moe_tid(void) { return fg153_h_tid; }
int64_t fg153_moe_bdim(void) { return fg153_h_bdim; }
int64_t fg153_moe_bidx(int64_t a) { return fg153_h_bidx[a]; }
int64_t fg153_moe_reject(int64_t c, int64_t code) { ((fg153_moe_sort_cfg *)(uintptr_t)c)->v[FG153_MOE_ERROR] = code; return code; }
#define main fg153_moe_sort_emitted_main
#include "/work/moe-routing/build/argsort.c"
#undef main
int64_t fg153_goose_moe_argsort_host(int64_t cfg) { return gg_function_1(cfg, 1); }
'''
RED = r'''
#include <stdint.h>
#include <stddef.h>
#include "fg153-moe-abi.h"
extern int64_t fg153_h_tid;
extern int64_t fg153_h_bidx[3];
extern int64_t fg153_h_bdim;
int64_t fg153_mr_cfg_i64(int64_t c, int64_t k);
float   fg153_mr_load(int64_t p, int64_t i);
void    fg153_mr_store(int64_t p, int64_t i, float v);
int64_t fg153_mr_tid(void);
int64_t fg153_mr_bdim(void);
int64_t fg153_mr_bidx(int64_t a);
int64_t fg153_mr_reject(int64_t c, int64_t code);
int64_t fg153_mr_cfg_i64(int64_t c, int64_t k) { return ((const int64_t *)(uintptr_t)c)[k]; }
float fg153_mr_load(int64_t p, int64_t i) { return ((const float *)(uintptr_t)p)[i]; }
void fg153_mr_store(int64_t p, int64_t i, float v) { ((float *)(uintptr_t)p)[i] = v; }
int64_t fg153_mr_tid(void) { return fg153_h_tid; }
int64_t fg153_mr_bdim(void) { return fg153_h_bdim; }
int64_t fg153_mr_bidx(int64_t a) { return fg153_h_bidx[a]; }
int64_t fg153_mr_reject(int64_t c, int64_t code) { ((fg153_moe_reduce_cfg *)(uintptr_t)c)->v[FG153_MR_ERROR] = code; return code; }
#define main fg153_moe_red_emitted_main
#include "/work/moe-routing/build/reduce.c"
#undef main
int64_t fg153_goose_moe_reduction_host(int64_t cfg) { return gg_function_1(cfg, 1); }
'''
def run(argv, what):
    print("+", " ".join(argv), flush=True)
    p = subprocess.run(argv, capture_output=True, text=True)
    if p.stdout: print(p.stdout, flush=True)
    if p.returncode != 0:
        print(p.stderr, file=sys.stderr, flush=True)
        raise SystemExit("FAILED " + what)
def main():
    os.makedirs(B, exist_ok=True)
    for name, text in (("sort_wrap.c", SORT), ("red_wrap.c", RED)):
        with open(os.path.join(B, name), "w", encoding="utf-8") as f:
            f.write(text)
    objs = []
    for name in ("sort_wrap", "red_wrap", "fixture"):
        src = os.path.join(B, name + ".c") if name != "fixture" else os.path.join(HERE, "fixture.c")
        out = os.path.join(B, name + ".o")
        run([CC, "-std=c11", "-D_GNU_SOURCE", "-O0", "-Werror=implicit-function-declaration",
             "-c", src, "-o", out] + INC, "compile " + name)
        objs.append(out)
    exe = os.path.join(B, "host_fixture")
    run([CC, "-std=c11", "-O0", "-o", exe] + objs + ["-lm"], "link")
    p = subprocess.run([exe], cwd=HERE, capture_output=True, text=True)
    sys.stdout.write(p.stdout)
    sys.stderr.write(p.stderr)
    if p.returncode != 0:
        raise SystemExit("FIXTURE FAILED rc=%d" % p.returncode)
if __name__ == "__main__":
    main()
