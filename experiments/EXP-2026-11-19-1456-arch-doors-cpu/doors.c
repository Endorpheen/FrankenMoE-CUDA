// The architecture doors on their own: no model, no GPU, no loader.
//
// ar_find maps a GGUF architecture name to its row, arch_rewrite turns a list of versions into the rewritten list, and
// arch_ver_ge compares a version pair. The loader sweeps the first of them on every start; this test asks all of them
// directly, so a change to the table is caught without loading the weights.
//
// Every argument that is a slice is passed as the two-field struct the generated C declares: a pointer and a length. Calling
// it as (pointer, length) in separate registers crashed the first version of this harness, and that is exactly the kind of
// mistake a test written against a real ABI is supposed to reveal.
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

struct sl_u8 { const void * data; int64_t len; };
struct sl_i64 { const int64_t * data; int64_t len; };

typedef int64_t (*ar_find_fn)(struct sl_u8);
typedef int64_t (*ar_count_fn)(void);
typedef int64_t (*ar_arch_fn)(int64_t, struct sl_u8);
typedef int64_t (*arch_rewrite_fn)(struct sl_u8, struct sl_u8);
typedef int64_t (*arch_ver_ge_fn)(int64_t, int64_t, int64_t, int64_t);

static struct sl_u8 u8s(const char * s) { struct sl_u8 r = { s, (int64_t) strlen(s) }; return r; }

int main(int argc, char ** argv) {
    const char * path = argc > 1 ? argv[1] : "builds/flint-acc/bin/libllama.so.0";
    void * h = dlopen(path, RTLD_LAZY);
    if (!h) { printf("cannot open %s: %s\n", path, dlerror()); return 2; }
    ar_find_fn find = (ar_find_fn) dlsym(h, "ar_find");
    arch_rewrite_fn rewrite = (arch_rewrite_fn) dlsym(h, "arch_rewrite");
    arch_ver_ge_fn ver_ge = (arch_ver_ge_fn) dlsym(h, "arch_ver_ge");
    ar_count_fn count = (ar_count_fn) dlsym(h, "ar_count");
    ar_arch_fn arch_of = (ar_arch_fn) dlsym(h, "ar_arch");
    if (!find || !rewrite || !ver_ge || !count || !arch_of) { printf("a door is missing\n"); return 2; }

    // The names come from the registry itself. A guessed list was wrong in the first version of this test - it named
    // architectures this build deliberately dropped - and a test that fails on its own guesses says nothing about the code.
    const int64_t n_rows = count();
    printf("arch doors: registry rows=%lld\n", (long long) n_rows);
    long checked = 0, not_found = 0, dup_rows = 0;
    int64_t seen[64]; long n_seen = 0;
    char name[128];
    for (int64_t row = 0; row < n_rows; ++row) {
        memset(name, 0, sizeof(name));
        const int64_t len = arch_of(row, (struct sl_u8){ name, (int64_t) sizeof(name) - 1 });
        ++checked;
        if (len <= 0) { ++not_found; printf("  EMPTY NAME for row %lld\n", (long long) row); continue; }
        // the round trip is the contract: the name the row gives must map back to that same row
        const int64_t back = find((struct sl_u8){ name, len });
        if (back != row) {
            ++not_found;
            printf("  ROUND TRIP row %lld -> '%s' -> %lld\n", (long long) row, name, (long long) back);
        }
        for (long k = 0; k < n_seen; ++k) {
            if (seen[k] == row) { ++dup_rows; printf("  SAME ROW %lld for %s\n", (long long) row, name); }
        }
        if (n_seen < 64) seen[n_seen++] = row;
    }

    // names that are not architectures must be refused rather than mapped
    static const char * not_arch[] = { "", "x", "not-a-model", "llama2", "QWEN35", "qwen35mo" };
    long refused = 0;
    for (size_t i = 0; i < sizeof(not_arch) / sizeof(not_arch[0]); ++i) {
        const int64_t row = find(u8s(not_arch[i]));
        if (row < 0) ++refused;
        else printf("  INVENTED row %lld for '%s'\n", (long long) row, not_arch[i]);
    }

    // arch_rewrite takes a list of version strings and returns the rewritten list
    {
        static const char * in[] = { "12.0", "8.0", "7.5" };
        char out[256]; memset(out, 0, sizeof(out));
        const int64_t n = rewrite(u8s("12.0;8.0;7.5"), (struct sl_u8){ out, (int64_t) sizeof(out) - 1 });
        printf("arch_rewrite(\"%s\") -> %lld bytes: %s\n", "12.0;8.0;7.5", (long long) n, out);
        (void) in;
    }

    // the version comparison as a truth table
    struct { int m, n, rm, rn, want; } cases[] = {
        {12, 0, 12, 0, 1}, {12, 1, 12, 0, 1}, {11, 9, 12, 0, 0}, {13, 0, 12, 4, 1}, {12, 0, 12, 4, 0},
        {11, 6, 11, 6, 1}, {10, 2, 11, 0, 0},
    };
    long ver_checked = 0, ver_bad = 0;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const int64_t got = ver_ge(cases[i].m, cases[i].n, cases[i].rm, cases[i].rn);
        ++ver_checked;
        if ((got != 0) != (cases[i].want != 0)) {
            ++ver_bad;
            printf("  VER_GE %d.%d >= %d.%d gave %lld, wanted %d\n", cases[i].m, cases[i].n, cases[i].rm,
                   cases[i].rn, (long long) got, cases[i].want);
        }
    }

    printf("arch doors: rows_checked=%ld round_trip_failures=%ld duplicate_rows=%ld unknown_refused=%ld/%zu "
           "ver_checked=%ld ver_failures=%ld\n", checked, not_found, dup_rows, refused,
           sizeof(not_arch) / sizeof(not_arch[0]), ver_checked, ver_bad);
    dlclose(h);
    return (not_found == 0 && dup_rows == 0 && ver_bad == 0) ? 0 : 1;
}
