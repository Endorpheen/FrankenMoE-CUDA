// Differential gate for stop-sequence matching (P8-04).
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define fg276t_fail   gate_fail
#define fg276t_ld_i64 gate_ld
#define fg276t_ld_u8  gate_ld8
#define fg276t_st_i64 gate_st
#include "stop_strings-functions.c"

void gate_fail(int64_t c) { fprintf(stderr, "FG276T validation code %lld\n", (long long) c); abort(); }
int64_t gate_ld(uint64_t b, int64_t i) { int64_t v; memcpy(&v, (void *) (uintptr_t) (b + (uint64_t) i), 8); return v; }
int64_t gate_ld8(uint64_t b, int64_t i) { return *(const uint8_t *) (uintptr_t) (b + (uint64_t) i); }
void gate_st(uint64_t b, int64_t i, int64_t v) { memcpy((void *) (uintptr_t) (b + (uint64_t) i), &v, 8); }

static int failures, checks;

static int64_t ref_full(const char * text, int64_t len, const char * words, const int64_t * offs, const int64_t * lens,
                        int64_t n, int64_t lts, int64_t * word_out) {
    int64_t best = -1, bw = -1;
    for (int64_t w = 0; w < n; ++w) {
        const int64_t wl = lens[w];
        const int64_t tmp = wl + lts;
        const int64_t from = len > tmp ? len - tmp : 0;
        int64_t found = -1;
        for (int64_t p = from; p + wl <= len && found < 0; ++p) {
            if (memcmp(text + p, words + offs[w], (size_t) wl) == 0) found = p;
        }
        if (found >= 0 && (best < 0 || found < best)) { best = found; bw = w; }
    }
    if (bw >= 0) *word_out = bw;
    return best;
}

static size_t string_ends_with(const char * s, int64_t slen, const char * suf, int64_t suflen) {
    if ((int64_t) suflen > slen) return 0;
    return memcmp(s + slen - suflen, suf, (size_t) suflen) == 0;
}

static int64_t ref_partial(const char * str, int64_t len, const char * stop, int64_t slen) {
    if (len == 0 || slen == 0) return -1;
    const int64_t max_len = len < slen ? len : slen;
    const char last = str[len - 1];
    for (int64_t k = max_len; k > 0; --k) {
        if (stop[k-1] == last && string_ends_with(str, len, stop, k)) return len - k;
    }
    return -1;
}

static void expect(const char * nm, int64_t got, int64_t want, int64_t gw, int64_t ww) {
    checks++;
    if (got != want || gw != ww) { failures++; printf("%-28s got %lld/%lld want %lld/%lld\n", nm, (long long) got, (long long) gw, (long long) want, (long long) ww); }
    else printf("%-28s OK\n", nm);
}

static void case_full(const char * nm, const char * text, const char * words, const int64_t * offs, const int64_t * lens,
                      int64_t n, int64_t lts) {
    const int64_t len = (int64_t) strlen(text);
    int64_t gw = -1, ww = -1;
    const int64_t g = fg276t_find_full((uint64_t)(uintptr_t) text, len, (uint64_t)(uintptr_t) words,
                                       (uint64_t)(uintptr_t) offs, (uint64_t)(uintptr_t) lens, n, lts,
                                       (uint64_t)(uintptr_t) &gw);
    const int64_t r = ref_full(text, len, words, offs, lens, n, lts, &ww);
    if (gw < 0) gw = -1;
    expect(nm, g, r, gw, ww);
}

int main(void) {
    {
        const char * words = "ENDSTOP###";
        const int64_t offs[2] = { 0, 3 }, lens[2] = { 3, 3 };
        case_full("full: first wins", "hello END world", words, offs, lens, 2, 0);
        case_full("full: later in tail", "aa STOP bb", words, offs, lens, 2, 0);
        case_full("full: none", "nothing here", words, offs, lens, 2, 0);
        case_full("full: tail window cuts", "xxxxxxxxEND", words, offs, lens, 2, 5);
        case_full("full: overlap picks earliest", "ENDSTOP", words, offs, lens, 2, 0);
    }
    {
        // both words match inside the tail window; the earlier position belongs to the earlier word
        const char * words = "baaa";
        const int64_t offs[2] = { 0, 1 }, lens[2] = { 1, 3 };
        case_full("full: earliest within window", "baaa", words, offs, lens, 2, 4);
    }
    {
        const char * text = "abcX";
        const char * word = "XYZ";
        const int64_t g = fg276t_find_partial((uint64_t)(uintptr_t) text, 4, (uint64_t)(uintptr_t) word, 3);
        expect("partial: suffix X", g, ref_partial(text, 4, word, 3), 0, 0);
        const char * t2 = "abcdef";
        expect("partial: full suffix", fg276t_find_partial((uint64_t)(uintptr_t) t2, 6, (uint64_t)(uintptr_t) "cdef", 4),
               ref_partial(t2, 6, "cdef", 4), 0, 0);
        const char * t3 = "zzz";
        expect("partial: none", fg276t_find_partial((uint64_t)(uintptr_t) t3, 3, (uint64_t)(uintptr_t) "ab", 2),
               ref_partial(t3, 3, "ab", 2), 0, 0);
    }
    printf("checks: %d, failures: %d\n", checks, failures);
    return failures ? 1 : 0;
}
