/* The HF cache naming doors on names whose validity can be read off the rules: "org/name" is a repository,
 * "org" alone is not, and a 40-hex commit is a commit while 39 is not. */
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct { const void * d; int64_t n; } csl_u8;
typedef struct { void * d; int64_t n; } sl_u8;
typedef int64_t (*valid_fn)(csl_u8);
typedef int64_t (*folder_fn)(csl_u8, sl_u8);

int main(int argc, char ** argv) {
    void * h = dlopen(argc > 1 ? argv[1] : "libllama.so.0", RTLD_LAZY | RTLD_NOW);
    if (!h) { printf("dlopen failed: %s\n", dlerror()); return 2; }
    valid_fn rv = (valid_fn) dlsym(h, "hf_repo_valid");
    valid_fn cv = (valid_fn) dlsym(h, "hf_commit_valid");
    folder_fn rf = (folder_fn) dlsym(h, "hf_repo_folder");
    if (!rv || !cv || !rf) { printf("door missing\n"); return 2; }

    int bad = 0;
    struct { const char * s; int want; } repos[] = {
        { "org/name", 1 }, { "org", 0 }, { "", 0 }, { "a/b/c", 0 },
    };
    for (unsigned i = 0; i < sizeof(repos)/sizeof(repos[0]); ++i) {
        csl_u8 in = { repos[i].s, (int64_t) strlen(repos[i].s) };
        const int64_t got = rv(in);
        printf("  repo_valid(\"%s\") = %lld %s\n", repos[i].s, (long long) got,
               (got == 1) == (repos[i].want == 1) ? "" : " <- unexpected");
        if ((got == 1) != (repos[i].want == 1)) { ++bad; }
    }

    struct { const char * s; int want; } commits[] = {
        { "0123456789abcdef0123456789abcdef01234567", 1 },   /* 40 hex */
        { "0123456789abcdef0123456789abcdef0123456", 0 },    /* 39 */
    };
    for (unsigned i = 0; i < sizeof(commits)/sizeof(commits[0]); ++i) {
        csl_u8 in = { commits[i].s, (int64_t) strlen(commits[i].s) };
        const int64_t got = cv(in);
        printf("  commit_valid(len %zu) = %lld %s\n", strlen(commits[i].s), (long long) got,
               (got == 1) == (commits[i].want == 1) ? "" : " <- unexpected");
        if ((got == 1) != (commits[i].want == 1)) { ++bad; }
    }

    char out[128];
    memset(out, 0, sizeof(out));
    sl_u8 so = { out, (int64_t) sizeof(out) };
    csl_u8 rin = { "org/name", 8 };
    const int64_t n = rf(rin, so);
    printf("  repo_folder(\"org/name\") = %lld -> [%s]\n", (long long) n, out);
    if (n <= 0 || out[0] == 0) { ++bad; }

    printf("hf naming doors: failures=%d\n", bad);
    return bad ? 1 : 0;
}
