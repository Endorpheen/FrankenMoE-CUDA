#!/usr/bin/env bash
# Parse and dump through the two composed units, compared with the canonical text.
#
# The parser and the dumper cannot share a unit (duplicate constants), so this is the first check that exercises both together:
# parse a text, dump the tree compactly, and require exactly the canonical bytes. Order of object keys matters, which is what
# common_json promises, so the expectation is the input itself for these cases.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN=${APEX_SERVER_DIR:-$ROOT/builds/flint-acc/bin}
OUT=$ROOT/results/json-roundtrip; mkdir -p "$OUT"
cat > "$OUT/probe.c" <<'C'
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
struct sl8 { const void * data; int64_t len; };
struct sl64 { int64_t * data; int64_t len; };
struct slc64 { const int64_t * data; int64_t len; };
struct slf { double * data; int64_t len; };
struct slcf { const double * data; int64_t len; };
typedef int64_t (*parse_fn)(struct sl8, struct sl64, struct sl64, struct slf, struct sl64, struct sl64, struct sl64,
                            struct sl64, struct sl64, struct sl64, struct sl64, struct sl64, struct sl64, struct sl8,
                            struct sl64, struct sl64, struct sl64, struct sl64);
typedef int64_t (*dump_fn)(int64_t, struct slc64, struct slc64, struct slcf, struct slc64, struct slc64, struct slc64,
                           struct slc64, struct slc64, struct slc64, struct sl64, struct sl8, struct sl8, int64_t, int64_t);
static parse_fn g_parse;
static dump_fn  g_dump;
static int64_t jk[512], jv[512], js[512], jl[512], jf[512], jn[512], jh[512];
static int64_t ev[512], eko[512], ekl[512], en[512], jkey[512], jkl[512], jchild[512], jst[16];
static double jd[512];
static char sbuf[2048];
static char out[2048];

static const char * roundtrip(const char * text, int64_t * rc) {
    memset(jk,0,sizeof jk); memset(jv,0,sizeof jv); memset(js,0,sizeof js); memset(jl,0,sizeof jl);
    memset(jf,0,sizeof jf); memset(jn,0,sizeof jn); memset(jh,0,sizeof jh); memset(ev,0,sizeof ev);
    memset(eko,0,sizeof eko); memset(ekl,0,sizeof ekl); memset(en,0,sizeof en); memset(sbuf,0,sizeof sbuf);
    memset(jkey,0,sizeof jkey); memset(jkl,0,sizeof jkl); memset(jchild,0,sizeof jchild); memset(jst,0,sizeof jst);
    memset(out, 0, sizeof out);
    const int64_t r = g_parse((struct sl8){ text, (int64_t) strlen(text) },
        (struct sl64){ jk, 512 }, (struct sl64){ jv, 512 }, (struct slf){ jd, 512 }, (struct sl64){ js, 512 },
        (struct sl64){ jl, 512 }, (struct sl64){ jf, 512 }, (struct sl64){ jn, 512 }, (struct sl64){ jh, 512 },
        (struct sl64){ ev, 512 }, (struct sl64){ eko, 512 }, (struct sl64){ ekl, 512 }, (struct sl64){ en, 512 },
        (struct sl8){ sbuf, sizeof sbuf - 1 },
        (struct sl64){ jkey, 512 }, (struct sl64){ jkl, 512 }, (struct sl64){ jchild, 512 }, (struct sl64){ jst, 16 });
    if (r != 0) { *rc = r; return "<parse failed>"; }
    const int64_t n = g_dump(0, (struct slc64){ jk, 512 }, (struct slc64){ jv, 512 }, (struct slcf){ jd, 512 },
        (struct slc64){ js, 512 }, (struct slc64){ jl, 512 }, (struct slc64){ jf, 512 }, (struct slc64){ jn, 512 },
        (struct slc64){ jkey, 512 }, (struct slc64){ jkl, 512 }, (struct sl64){ jchild, 512 },
        (struct sl8){ sbuf, sizeof sbuf - 1 }, (struct sl8){ out, sizeof out - 1 }, -1, 0);
    *rc = n;
    return out;
}

int main(int argc, char ** argv) {
    void * h = dlopen(argc > 1 ? argv[1] : "libllama.so.0", RTLD_LAZY);
    if (!h) { printf("cannot open the library: %s\n", dlerror()); return 2; }
    g_parse = (parse_fn) dlsym(h, "json_parse");
    g_dump  = (dump_fn)  dlsym(h, "json_dump");
    void (*init)(int, char **) = (void (*)(int, char **)) dlsym(h, "gs_json_dump_init");
    if (!g_parse || !g_dump) { printf("a door is missing\n"); return 2; }
    if (init) { init(0, 0); }

    // Key order is preserved, which is what common_json promises its callers. Numbers are normalised the way nlohmann
    // normalises them: -1.5e3 becomes -1500.0, so the expectation for that case is the normalised text, not the input. The
    // first version of this table compared against the input and failed on its own assumption.
    struct { const char * text; const char * want; } cases[] = {
        { "{\"a\":1,\"b\":[true,null]}", "{\"a\":1,\"b\":[true,null]}" },
        { "{\"z\":1,\"a\":2}",           "{\"z\":1,\"a\":2}" },
        { "[1,2,3]",                        "[1,2,3]" },
        { "{\"s\":\"x y\",\"n\":-1.5e3}",   "{\"s\":\"x y\",\"n\":-1500.0}" },
        { "null",                           "null" },
    };
    int fails = 0;
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        int64_t rc = 0;
        const char * got = roundtrip(cases[i].text, &rc);
        if (rc <= 0 || strcmp(got, cases[i].want) != 0) {
            ++fails; printf("  ROUNDTRIP %s -> %s (want %s, rc=%lld)\n", cases[i].text, got, cases[i].want,
                            (long long) rc);
        }
    }
    printf("json roundtrip: cases=%zu failures=%d\n", sizeof cases / sizeof cases[0], fails);
    return fails == 0 ? 0 : 1;
}
C
gcc -O1 -o "$OUT/probe" "$OUT/probe.c" -ldl || exit 1
LD_LIBRARY_PATH="$BIN" "$OUT/probe" "$BIN/libllama.so.0"; rc=$?
[ "$rc" -eq 0 ] || { echo "FAIL: parse and dump do not round-trip"; exit 1; }
echo "PASS: both units round-trip the canonical text, key order included"
