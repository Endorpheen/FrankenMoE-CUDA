#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct sl8  { const void * data; int64_t len; };
struct sl64 { int64_t * data; int64_t len; };
struct slf  { double * data; int64_t len; };
// Eighteen parameters, exactly as the generated wrapper declares them: text, eight value arrays, four edge arrays, the string
// buffer, three child arrays and the state. An earlier version of this probe declared fifteen and passed three fewer - the
// first one happened to survive on luck, this one crashed, which is why the signature is read from the generated C and not
// guessed.
typedef int64_t (*parse_fn)(struct sl8, struct sl64, struct sl64, struct slf, struct sl64, struct sl64, struct sl64,
                            struct sl64, struct sl64, struct sl64, struct sl64, struct sl64, struct sl64, struct sl8,
                            struct sl64, struct sl64, struct sl64, struct sl64);

static parse_fn g_parse;
static int64_t jk[1024], jv[1024], js[1024], jl[1024], jf[1024], jn[1024], jh[1024];
static int64_t ev[512], eko[512], ekl[512], en[512], jkey[512], jkl[512], jchild[512], jst[16];
static double jd[1024];
static char sbuf[4096];

static int64_t parse(const char * text, int64_t * root_kind) {
    memset(jk, 0, sizeof jk); memset(jv, 0, sizeof jv); memset(js, 0, sizeof js); memset(jl, 0, sizeof jl);
    memset(jf, 0, sizeof jf); memset(jn, 0, sizeof jn); memset(jh, 0, sizeof jh); memset(ev, 0, sizeof ev);
    memset(eko, 0, sizeof eko); memset(ekl, 0, sizeof ekl); memset(en, 0, sizeof en); memset(sbuf, 0, sizeof sbuf);
    memset(jkey, 0, sizeof jkey); memset(jkl, 0, sizeof jkl); memset(jchild, 0, sizeof jchild); memset(jst, 0, sizeof jst);
    const int64_t r = g_parse((struct sl8){ text, (int64_t) strlen(text) },
        (struct sl64){ jk, 1024 }, (struct sl64){ jv, 1024 }, (struct slf){ jd, 1024 }, (struct sl64){ js, 1024 },
        (struct sl64){ jl, 1024 }, (struct sl64){ jf, 1024 }, (struct sl64){ jn, 1024 }, (struct sl64){ jh, 1024 },
        (struct sl64){ ev, 1024 }, (struct sl64){ eko, 1024 }, (struct sl64){ ekl, 1024 }, (struct sl64){ en, 1024 },
        (struct sl8){ sbuf, (int64_t) sizeof sbuf - 1 },
        (struct sl64){ jkey, 512 }, (struct sl64){ jkl, 512 }, (struct sl64){ jchild, 512 }, (struct sl64){ jst, 16 });
    if (root_kind != NULL) { *root_kind = jk[0]; }
    return r;
}

int main(int argc, char ** argv) {
    void * h = dlopen(argc > 1 ? argv[1] : "libllama.so.0", RTLD_LAZY);
    if (!h) { printf("cannot open the library: %s\n", dlerror()); return 2; }
    g_parse = (parse_fn) dlsym(h, "json_parse");
    if (!g_parse) { printf("json_parse is not exported\n"); return 2; }
    int fails = 0;

    static const char * good[] = { "{\"a\":1,\"b\":[true,null]}", "[1,2,3]", "\"x\"", "42", "null" };
    long seen_kinds = 0;
    int64_t kinds[5];
    for (size_t i = 0; i < sizeof good / sizeof good[0]; ++i) {
        int64_t kind = -1;
        const int64_t r = parse(good[i], &kind);
        kinds[i] = kind;
        if (r != 0) { ++fails; printf("  VALID text refused: %s (r=%lld)\n", good[i], (long long) r); }
        for (long k = 0; k < seen_kinds; ++k) {
            if (kinds[k] == kind && i != (size_t) k) { /* a repeated kind is fine, it just is not evidence on its own */ }
        }
        seen_kinds = (long) i + 1;
    }
    printf("valid: root kinds = %lld %lld %lld %lld %lld\n", (long long) kinds[0], (long long) kinds[1],
           (long long) kinds[2], (long long) kinds[3], (long long) kinds[4]);

    static const char * bad[] = { "{", "[1,", "{\"a\":}", "tru" };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
        const int64_t r = parse(bad[i], NULL);
        if (r == 0) { ++fails; printf("  INVALID text accepted: %s\n", bad[i]); }
    }
    printf("json door: valid=%zu invalid=%zu failures=%d\n", sizeof good / sizeof good[0],
           sizeof bad / sizeof bad[0], fails);
    return fails == 0 ? 0 : 1;
}
