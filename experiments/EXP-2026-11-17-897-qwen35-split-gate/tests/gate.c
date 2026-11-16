// Differential gate: native unicode_regex_split_custom_qwen35 vs goose/misc/qwen35-split.goose.
//
// The corpus is chosen to separate the cases the rule order distinguishes, not to be large: contractions in both
// cases, words starting with a symbol, digits beside letters, \r\n inside and at the end of whitespace runs, a
// whitespace run reaching the segment end (rule 7 must not fire), CJK, and combining marks.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

typedef struct { uint32_t * cp; size_t n; } cps;
static cps decode(const char * s) {
    cps r; r.cp = malloc(4 * (strlen(s) + 1)); r.n = 0;
    for (const unsigned char * p = (const unsigned char *) s; *p; ) {
        uint32_t c; int extra;
        if (*p < 0x80) { c = *p; extra = 0; }
        else if ((*p & 0xE0) == 0xC0) { c = *p & 0x1F; extra = 1; }
        else if ((*p & 0xF0) == 0xE0) { c = *p & 0x0F; extra = 2; }
        else { c = *p & 0x07; extra = 3; }
        ++p;
        for (int i = 0; i < extra; ++i, ++p) { c = (c << 6) | (*p & 0x3F); }
        r.cp[r.n++] = c;
    }
    return r;
}

typedef size_t (*ref_fn)(const uint32_t *, size_t, const size_t *, size_t, size_t *, size_t);
typedef int64_t (*goose_fn)(int64_t);
typedef const uint16_t * (*view_fn)(void);

int main(int argc, char ** argv) {
    const char * lib = argc > 1 ? argv[1] : "runtime/goose-stage/lib/libllama.so";
    void * h = dlopen(lib, RTLD_NOW | RTLD_LOCAL);
    if (!h) { printf("Q35S_GATE FAIL dlopen: %s\n", dlerror()); return 1; }
    ref_fn ref = (ref_fn) dlsym(h, "fg_unicode_split_qwen35_ref");
    goose_fn gs = (goose_fn) dlsym(h, "q35s_split");
    view_fn view = (view_fn) dlsym(h, "fg_unicode_flags_view_c");
    if (!ref || !gs || !view) { printf("Q35S_GATE FAIL missing symbol\n"); return 1; }

    static const char * corpus[] = {
        "hello world",
        "don't stop",
        "DON'T STOP",
        "I'll we've they're",
        "'s 't 'm 'd",
        "'re 've 'll",
        "\"xfoo bar",
        "xfoo",
        "abc123def",
        "123 456",
        "hello\nworld",
        "a\r\nb",
        "a  \n\n  b",
        "trailing   ",
        "trailing  \n",
        "   ",
        "a   b",
        "\n\n\n",
        "   lead",
        "中文分词测试",
        "a\u0301bc",
        "cafe\u0301",
        "mixed 中文 and ascii",
        "!!!hello",
        "a!b",
        "  +-  /* */  ",
        "",
        "x",
        " ",
        "\r\n",
    };
    const size_t n_cases = sizeof corpus / sizeof *corpus;
    int fails = 0, total = 0;
    size_t off[64], a[64], b[64];

    for (size_t t = 0; t < n_cases; ++t) {
        cps c = decode(corpus[t]);
        // one segment covering the whole string, the way the caller splits on whitespace-adjacent boundaries
        off[0] = c.n;
        const size_t na = ref(c.cp, c.n, off, 1, a, 64);

        int64_t pkt[8] = { 0 };
        pkt[0] = (int64_t)(uintptr_t) c.cp;
        pkt[1] = (int64_t) c.n;
        pkt[2] = (int64_t)(uintptr_t) off;
        pkt[3] = 1;
        pkt[4] = (int64_t)(uintptr_t) view();
        pkt[5] = (int64_t)(uintptr_t) b;
        pkt[6] = 64;
        static int started = 0;
    if (!started) { ((void(*)(void)) dlsym(h, "fg_qwen35_split_init"))(); started = 1; }
    const int64_t nb = gs((int64_t)(uintptr_t) pkt);

        ++total;
        const int same = (na == (size_t) nb) && memcmp(a, b, na * sizeof a[0]) == 0;
        if (!same) {
            ++fails;
            printf("MISMATCH case %zu %s\n  native n=%zu:", t, corpus[t], na);
            for (size_t i = 0; i < na && i < 16; ++i) printf(" %zu", a[i]);
            printf("\n  goose  n=%lld:", (long long) nb);
            for (size_t i = 0; i < (size_t) nb && i < 16; ++i) printf(" %zu", b[i]);
            printf("\n");
        }
        free(c.cp);
    }
    printf("Q35S_GATE cases=%d mismatches=%d %s\n", total, fails, fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
