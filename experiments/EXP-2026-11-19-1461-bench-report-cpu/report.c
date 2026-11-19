// The bench report's formatting decisions, asked directly on the CPU.
//
// Averages, standard deviations, CSV and JSON escaping, the markdown width and name rules, and the size and parameter
// renderings. These turn numbers into the text a report contains, so a change here is a change in what a reader is told -
// and none of it needs a model or a GPU.
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

struct sl_u8  { const void * data; int64_t len; };
struct sl_i64 { const int64_t * data; int64_t len; };
struct sl_f64 { const double * data; int64_t len; };

typedef int64_t (*u64_avg_fn)(struct sl_i64);
typedef double  (*ts_fn)(struct sl_i64, int64_t);
typedef int64_t (*s2o_fn)(struct sl_u8, struct sl_u8);
typedef int64_t (*so_fn)(struct sl_u8, struct sl_u8);
typedef int64_t (*i1o_fn)(int64_t, struct sl_u8);
typedef int64_t (*d2o_fn)(double, double, struct sl_u8);
typedef int64_t (*i3o_fn)(int64_t, int64_t, int64_t, struct sl_u8);

static struct sl_u8 S(const char * s) { struct sl_u8 r = { s, (int64_t) strlen(s) }; return r; }

int main(int argc, char ** argv) {
    const char * path = argc > 1 ? argv[1] : "builds/flint-acc/bin/libllama.so.0";
    void * h = dlopen(path, RTLD_LAZY);
    if (!h) { printf("cannot open %s: %s\n", path, dlerror()); return 2; }
    u64_avg_fn avg = (u64_avg_fn) dlsym(h, "lb_avg_u64");
    u64_avg_fn sd  = (u64_avg_fn) dlsym(h, "lb_stdev_u64");
    ts_fn avg_ts   = (ts_fn) dlsym(h, "lb_avg_ts");
    s2o_fn csv     = (s2o_fn) dlsym(h, "lb_csv_escape");
    s2o_fn json    = (s2o_fn) dlsym(h, "lb_json_escape");
    i1o_fn sz      = (i1o_fn) dlsym(h, "lb_md_size");
    i1o_fn prm     = (i1o_fn) dlsym(h, "lb_md_params");
    i3o_fn test    = (i3o_fn) dlsym(h, "lb_md_test");
    d2o_fn ts      = (d2o_fn) dlsym(h, "lb_md_ts");
    i1o_fn outstr  = (i1o_fn) dlsym(h, "lb_out_str");
    if (!avg || !sd || !avg_ts || !csv || !json || !sz || !prm || !test || !ts || !outstr) {
        printf("a door is missing\n"); return 2; }
    int fails = 0;
    char out[256];

    // averages and standard deviations of the values a bench run produces
    {
        const int64_t v1[] = { 10, 20, 30, 40 };
        const int64_t v2[] = { 5 };
        const int64_t v3[] = { 7, 7, 7, 7 };
        struct { const int64_t * v; int64_t n; int64_t avg; int64_t sd; } cases[] = {
            { v1, 4, 25, -1 }, { v2, 1, 5, 0 }, { v3, 4, 7, 0 },
        };
        for (size_t i = 0; i < 3; ++i) {
            const int64_t a = avg((struct sl_i64){ cases[i].v, cases[i].n });
            if (a != cases[i].avg) { ++fails; printf("  AVG n=%lld got=%lld want=%lld\n",
                (long long) cases[i].n, (long long) a, (long long) cases[i].avg); }
            const int64_t d = sd((struct sl_i64){ cases[i].v, cases[i].n });
            if (d < 0) { ++fails; printf("  STDEV negative for n=%lld\n", (long long) cases[i].n); }
            if (cases[i].sd >= 0 && d != cases[i].sd) { ++fails; printf("  STDEV n=%lld got=%lld want=%lld\n",
                (long long) cases[i].n, (long long) d, (long long) cases[i].sd); }
        }
    }
    // tokens per second: the same samples with different token counts scale
    {
        const int64_t v[] = { 1000000, 2000000 };
        const double t100 = avg_ts((struct sl_i64){ v, 2 }, 100);
        const double t200 = avg_ts((struct sl_i64){ v, 2 }, 200);
        if (!(t100 > 0.0) || fabs(t200 - t100 * 2.0) > 1e-6) { ++fails;
            printf("  AVG_TS 100=%.6f 200=%.6f (expected double)\n", t100, t200); }
    }
    // CSV: every field quoted, inner quotes doubled
    {
        struct { const char * in; const char * want; } cases[] = {
            { "plain", "\"plain\"" }, { "a\"b", "\"a\"\"b\"" }, { "", "\"\"" },
        };
        for (size_t i = 0; i < 3; ++i) {
            memset(out, 0, sizeof(out));
            const int64_t n = csv(S(cases[i].in), (struct sl_u8){ out, sizeof(out) - 1 });
            if (n != (int64_t) strlen(cases[i].want) || strcmp(out, cases[i].want) != 0) { ++fails;
                printf("  CSV '%s' -> '%s' want '%s'\n", cases[i].in, out, cases[i].want); }
        }
    }
    // JSON: quotes and backslashes escaped
    {
        memset(out, 0, sizeof(out));
        const int64_t n = json(S("a\"b\\c"), (struct sl_u8){ out, sizeof(out) - 1 });
        if (n <= 0 || strcmp(out, "a\\\"b\\\\c") != 0) { ++fails;
            printf("  JSON -> '%s' want 'a\\\\\"b\\\\\\\\c'\n", out); }
    }
    // size and parameter rendering
    {
        // The module's contract, read from lb_row_size: two decimals always, MiB below one GiB and GiB above it. The
        // first version of this table expected "512 B" and "1.0 MiB" and failed on its own assumptions.
        struct { int64_t bytes; const char * want; } szc[] = {
            { 512, "0.00 MiB" }, { 1048576, "1.00 MiB" }, { 1572864, "1.50 MiB" },
            { 1610612736, "1.50 GiB" }, { 1073741824, "1.00 GiB" },
        };
        for (size_t i = 0; i < sizeof(szc)/sizeof(szc[0]); ++i) {
            memset(out, 0, sizeof(out));
            const int64_t n = sz(szc[i].bytes, (struct sl_u8){ out, sizeof(out) - 1 });
            if (n <= 0 || strcmp(out, szc[i].want) != 0) { ++fails;
                printf("  SIZE %lld -> '%s' want '%s'\n", (long long) szc[i].bytes, out, szc[i].want); }
        }
        struct { int64_t n; const char * want; } prc[] = {
            // below one billion the module reports millions with an "M", above it billions with a "B"
            { 1000000000, "1.00 B" }, { 5000000000, "5.00 B" }, { 250000000, "250.00 M" },
        };
        for (size_t i = 0; i < sizeof(prc)/sizeof(prc[0]); ++i) {
            memset(out, 0, sizeof(out));
            const int64_t r = prm(prc[i].n, (struct sl_u8){ out, sizeof(out) - 1 });
            if (r <= 0 || strcmp(out, prc[i].want) != 0) { ++fails;
                printf("  PARAMS %lld -> '%s' want '%s'\n", (long long) prc[i].n, out, prc[i].want); }
        }
    }
    // the test column: pp512 / tg128 shapes
    {
        memset(out, 0, sizeof(out));
        const int64_t n = test(512, 0, 1, (struct sl_u8){ out, sizeof(out) - 1 });
        if (n <= 0) { ++fails; printf("  TEST pp512 refused\n"); }
        else if (strncmp(out, "pp512", 5) != 0) { ++fails; printf("  TEST pp512 -> '%s'\n", out); }
        memset(out, 0, sizeof(out));
        const int64_t m = test(0, 128, 1, (struct sl_u8){ out, sizeof(out) - 1 });
        if (m <= 0 || strncmp(out, "tg128", 5) != 0) { ++fails; printf("  TEST tg128 -> '%s'\n", out); }
    }
    // the output enumeration renders, and every rendering is distinct
    {
        char seen[3][256]; int n_seen = 0;
        for (int64_t v = 0; v < 3; ++v) {
            memset(out, 0, sizeof(out));
            const int64_t n = outstr(v, (struct sl_u8){ out, sizeof(out) - 1 });
            if (n <= 0) { ++fails; printf("  OUT %lld refused\n", (long long) v); continue; }
            for (int k = 0; k < n_seen; ++k) {
                if (strcmp(seen[k], out) == 0) { ++fails; printf("  OUT %lld duplicates '%s'\n", (long long) v, out); }
            }
            snprintf(seen[n_seen++], sizeof(seen[0]), "%s", out);
        }
    }

    printf("bench report: avg_sd=3 avg_ts=1 csv=3 json=1 size=4 params=2 test=2 out=3 failures=%d\n", fails);
    dlclose(h);
    return fails == 0 ? 0 : 1;
}
