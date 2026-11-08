#!/usr/bin/env python3
# build_host.py - isolated host build of the Goose-emitted C wrappers and the
# native-vs-Goose differential fixture.
import os, subprocess, sys
W = "/work"; B = os.path.join(W, "build"); CC = "/usr/bin/gcc-13"
INC = ["-I" + W, "-I" + B]
GDN = r'''
#include <stdint.h>
#include <stddef.h>
#include "fg153-gdn-abi.h"
#include "fg153-host-state.h"
int64_t fg153_gdn_cfg_i64(int64_t c, int64_t k);
float   fg153_gdn_cfg_f32(int64_t c, int64_t k);
float   fg153_gdn_load(int64_t p, int64_t i);
void    fg153_gdn_store(int64_t p, int64_t i, float v);
int64_t fg153_gdn_tid(void);
int64_t fg153_gdn_bidx(int64_t a);
int64_t fg153_gdn_reject(int64_t c, int64_t code);
int64_t fg153_gdn_cfg_i64(int64_t c, int64_t k) { return ((const int64_t *)(uintptr_t)c)[k]; }
float fg153_gdn_cfg_f32(int64_t c, int64_t k) { return ((const fg153_gdn_cfg *)(uintptr_t)c)->f[k]; }
float fg153_gdn_load(int64_t p, int64_t i) { return ((const float *)(uintptr_t)p)[i]; }
void fg153_gdn_store(int64_t p, int64_t i, float v) { ((float *)(uintptr_t)p)[i] = v; }
int64_t fg153_gdn_tid(void) { return fg153_h_tid; }
int64_t fg153_gdn_bidx(int64_t a) { return fg153_h_bidx[a]; }
int64_t fg153_gdn_reject(int64_t c, int64_t code) { ((fg153_gdn_cfg *)(uintptr_t)c)->v[FG153_GDN_ERROR] = code; return code; }
#define main fg153_gdn_emitted_main
#include "/work/build/gdn.c"
#undef main
int64_t fg153_goose_gdn_host(int64_t cfg) { return gg_function_1(cfg, 1); }
'''
CONV = r'''
#include <stdint.h>
#include <stddef.h>
#include "fg153-gdn-abi.h"
#include "fg153-host-state.h"
int64_t fg153_conv_cfg_i64(int64_t c, int64_t k);
float   fg153_conv_load(int64_t p, int64_t i);
void    fg153_conv_store(int64_t p, int64_t i, float v);
float   fg153_conv_shload(int64_t i);
void    fg153_conv_shstore(int64_t i, float v);
void    fg153_conv_sync(void);
int64_t fg153_conv_tid(void);
int64_t fg153_conv_bidx(int64_t a);
int64_t fg153_conv_reject(int64_t c, int64_t code);
int64_t fg153_conv_cfg_i64(int64_t c, int64_t k) { return ((const int64_t *)(uintptr_t)c)[k]; }
float fg153_conv_load(int64_t p, int64_t i) { return ((const float *)(uintptr_t)p)[i]; }
void fg153_conv_store(int64_t p, int64_t i, float v) { ((float *)(uintptr_t)p)[i] = v; }
float fg153_conv_shload(int64_t i) { return fg153_h_smem[i]; }
void fg153_conv_shstore(int64_t i, float v) { fg153_h_smem[i] = v; }
void fg153_conv_sync(void) { }
int64_t fg153_conv_tid(void) { return fg153_h_tid; }
int64_t fg153_conv_bidx(int64_t a) { return fg153_h_bidx[a]; }
int64_t fg153_conv_reject(int64_t c, int64_t code) { ((fg153_conv_cfg *)(uintptr_t)c)->v[FG153_CONV_ERROR] = code; return code; }
#define main fg153_conv_emitted_main
#include "/work/build/conv.c"
#undef main
int64_t fg153_goose_conv_host(int64_t cfg) { return gg_function_1(cfg, 1); }
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
    for name, text in (("gdn_wrap.c", GDN), ("conv_wrap.c", CONV)):
        with open(os.path.join(B, name), "w", encoding="utf-8") as f:
            f.write(text)
    objs = []
    for name in ("gdn_wrap", "conv_wrap", "fixture"):
        src = os.path.join(B if name != "fixture" else W, name + ".c")
        out = os.path.join(B, name + ".o")
        run([CC, "-std=c11", "-D_GNU_SOURCE", "-O0", "-Werror=implicit-function-declaration",
             "-c", src, "-o", out] + INC, "compile " + name)
        objs.append(out)
    exe = os.path.join(B, "host_fixture")
    run([CC, "-std=c11", "-O0", "-o", exe] + objs + ["-lm"], "link")
    p = subprocess.run([exe], cwd=W, capture_output=True, text=True)
    sys.stdout.write(p.stdout)
    sys.stderr.write(p.stderr)
    if p.returncode != 0:
        raise SystemExit("FIXTURE FAILED rc=%d" % p.returncode)
if __name__ == "__main__":
    main()
