/* lb_* doors on values known by hand: the module compiled into the unit, but a door is available when it
 * answers a case that can be checked without a benchmark run. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct { const void * d; int64_t n; } csl_i64;
typedef struct { const void * d; int64_t n; } csl_u8;
typedef struct { void * d; int64_t n; } sl_u8;
typedef int64_t (*avg_fn)(csl_i64);
typedef int64_t (*esc_fn)(csl_u8, sl_u8);

int main(int argc, char ** argv) {
    void * h = dlopen(argc > 1 ? argv[1] : "libllama.so.0", RTLD_LAZY | RTLD_NOW);
    if (!h) { printf("dlopen failed\n"); return 2; }
    avg_fn avg = (avg_fn) dlsym(h, "lb_avg_u64");
    esc_fn esc = (esc_fn) dlsym(h, "lb_csv_escape");
    if (!avg || !esc) { printf("door missing\n"); return 2; }

    int bad = 0;
    const int64_t v1[2] = { 2, 4 }, v2[3] = { 1, 2, 3 }, v3[1] = { 7 };
    csl_i64 s1 = { v1, 2 }, s2 = { v2, 3 }, s3 = { v3, 1 };
    const int64_t a1 = avg(s1), a2 = avg(s2), a3 = avg(s3);
    printf("avg {2,4}=%lld {1,2,3}=%lld {7}=%lld\n", (long long) a1, (long long) a2, (long long) a3);
    if (a1 != 3 || a2 != 2 || a3 != 7) { ++bad; }

    char out[64];
    const char * plain = "abc";
    memset(out, 0, sizeof(out));
    sl_u8 so = { out, (int64_t) sizeof(out) };
    csl_u8 in_plain = { plain, 3 };
    int64_t n = esc(in_plain, so);
    printf("escape(abc) n=%lld -> [%s]\n", (long long) n, out);
    if (strcmp(out, "\"abc\"") != 0) { ++bad; }

    const char * quote = "a\"b";
    memset(out, 0, sizeof(out));
    csl_u8 in_quote = { quote, 3 };
    n = esc(in_quote, so);
    printf("escape(a\"b) n=%lld -> [%s]\n", (long long) n, out);
    if (strcmp(out, "\"a\"\"b\"") != 0) { ++bad; }

    printf("bench doors: cases=5 failures=%d\n", bad);
    return bad ? 1 : 0;
}
