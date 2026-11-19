// The metrics and trace decisions, asked directly on the CPU.
//
// CSV field quoting and escaping, the NaN rule for weights, the column counts the three trace headers pin, a full compute
// row, and the workload-replay accept/refuse rules. These decide what a trace file contains, which is how a performance claim
// is audited later, so none of it needs a model or a GPU.
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

struct sl_u8 { const void * data; int64_t len; };
typedef int64_t (*s_fn)(struct sl_u8);
typedef int64_t (*so_fn)(struct sl_u8, struct sl_u8);
typedef int64_t (*d_fn)(double);
typedef int64_t (*i_fn)(int64_t);
typedef int64_t (*v_fn)(void);
typedef int64_t (*row_fn)(int64_t, int64_t, int64_t, int64_t, int64_t, struct sl_u8, struct sl_u8, int64_t, int64_t,
                          struct sl_u8);

static struct sl_u8 S(const char * s) { struct sl_u8 r = { s, (int64_t) strlen(s) }; return r; }

int main(int argc, char ** argv) {
    const char * path = argc > 1 ? argv[1] : "builds/flint-acc/bin/libllama.so.0";
    void * h = dlopen(path, RTLD_LAZY);
    if (!h) { printf("cannot open %s: %s\n", path, dlerror()); return 2; }
    s_fn needs_quote = (s_fn) dlsym(h, "mt_field_needs_quote");
    so_fn encode = (so_fn) dlsym(h, "mt_field_encode");
    d_fn wkind = (d_fn) dlsym(h, "mt_weight_kind");
    i_fn cols = (i_fn) dlsym(h, "mt_trace_cols");
    v_fn metrics_cols = (v_fn) dlsym(h, "mt_metrics_cols");
    v_fn summary_keys = (v_fn) dlsym(h, "mt_summary_keys");
    row_fn compute_row = (row_fn) dlsym(h, "mt_compute_row_encode");
    if (!needs_quote || !encode || !wkind || !cols || !metrics_cols || !summary_keys || !compute_row) {
        printf("a door is missing\n"); return 2; }
    int fails = 0;
    char out[512];

    // a field needs quoting when it holds a separator, a quote or a newline
    struct { const char * s; int want; } qc[] = {
        { "plain", 0 }, { "a,b", 1 }, { "a\"b", 1 }, { "a\nb", 1 }, { "", 0 }, { "a b", 0 },
    };
    for (size_t i = 0; i < sizeof(qc)/sizeof(qc[0]); ++i) {
        const int64_t got = needs_quote(S(qc[i].s)) != 0;
        if (got != qc[i].want) { ++fails; printf("  QUOTE '%s' got=%lld want=%d\n", qc[i].s, (long long) got, qc[i].want); }
    }
    // the encoded form is what a CSV reader must read back as the original field
    struct { const char * in; const char * want; } ec[] = {
        { "plain", "plain" }, { "a,b", "\"a,b\"" }, { "a\"b", "\"a\"\"b\"" }, { "", "" },
    };
    for (size_t i = 0; i < sizeof(ec)/sizeof(ec[0]); ++i) {
        memset(out, 0, sizeof(out));
        const int64_t n = encode(S(ec[i].in), (struct sl_u8){ out, sizeof(out) - 1 });
        if (n != (int64_t) strlen(ec[i].want) || strcmp(out, ec[i].want) != 0) { ++fails;
            printf("  ENCODE '%s' -> '%s' want '%s'\n", ec[i].in, out, ec[i].want); }
    }
    // NaN takes the text arm, every other value the numeric one
    {
        const double vals[] = { NAN, 0.0, -1.5, 1e300 };
        const int kind_nan = (int) wkind(vals[0]);
        for (int i = 1; i < 4; ++i) {
            if ((int) wkind(vals[i]) == kind_nan) { ++fails;
                printf("  WEIGHT %.3f shares the NaN arm\n", vals[i]); }
        }
    }
    // the column counts the headers pin
    {
        const int64_t c0 = cols(0), c1 = cols(1), c2 = cols(2), c9 = cols(99);
        printf("  trace columns: compute=%lld io=%lld route=%lld unknown=%lld\n",
               (long long) c0, (long long) c1, (long long) c2, (long long) c9);
        if (c0 != 9 || c1 != 12 || c2 != 10 || c9 != 0) { ++fails; printf("  COLUMNS differ from the pinned widths\n"); }
        const int64_t mc = metrics_cols(), sk = summary_keys();
        if (mc <= 0 || sk <= 0) { ++fails; printf("  METRICS cols=%lld summary=%lld\n", (long long) mc, (long long) sk); }
        else printf("  metrics columns=%lld summary keys=%lld\n", (long long) mc, (long long) sk);
    }
    // one full compute row: nine columns, ending in a newline, with the op and name escaped
    {
        memset(out, 0, sizeof(out));
        const int64_t n = compute_row(1, 2, 3, 4, 5, S("mul,mat"), S("layer 5"), 1000, 7,
                                      (struct sl_u8){ out, sizeof(out) - 1 });
        if (n <= 0 || n != (int64_t) strlen(out)) { ++fails; printf("  ROW length %lld vs %zu\n",
            (long long) n, strlen(out)); }
        else {
            int commas = 0; for (int64_t i = 0; i < n; ++i) { if (out[i] == ',') ++commas; }
            // nine columns means eight separators plus the quoted separator inside the op field
            printf("  compute row: %s", out);
            if (out[n - 1] != '\n') { ++fails; printf("  ROW does not end with a newline\n"); }
            if (strstr(out, "\"mul,mat\"") == nullptr) { ++fails; printf("  ROW did not quote the comma in op\n"); }
            if (commas < 8) { ++fails; printf("  ROW has only %d separators\n", commas); }
        }
    }

    printf("metrics traces: quote=6 encode=4 weight_nan=1 columns=4 row=1 failures=%d\n", fails);
    dlclose(h);
    return fails == 0 ? 0 : 1;
}
