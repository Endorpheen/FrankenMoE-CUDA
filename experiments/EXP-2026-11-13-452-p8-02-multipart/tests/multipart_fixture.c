// Differential fixture for the Goose multipart / path-parameter helpers (EXP452).
//
// Right arm: a transcription of decode_path_component, the multipart form
// grouping in server_http_context::post and the get_params precedence.
// Left arm: the Goose module emitted by the pinned FrankenGoose compiler.
// check.py independently re-decodes the corpus with Python's urllib.
#include <stdio.h>
#include "ffi-adapter.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

int64_t  fg452_ld_i64(uint64_t p, uint64_t o) { return *(const int64_t *) ((const char *) (uintptr_t) p + o); }

static int cases = 0, checks = 0, fails = 0;

// ---- native decode_path_component -----------------------------------------
static void native_decode(const char * in, size_t n, char * out, size_t * on) {
    size_t o = 0;
    for (size_t i = 0; i < n; ++i) {
        if (in[i] == '%' && i + 2 < n) {
            int hi = -1, lo = -1;
            char a = in[i + 1], b = in[i + 2];
            if (a >= '0' && a <= '9') hi = a - '0';
            else if (a >= 'a' && a <= 'f') hi = a - 'a' + 10;
            else if (a >= 'A' && a <= 'F') hi = a - 'A' + 10;
            if (b >= '0' && b <= '9') lo = b - '0';
            else if (b >= 'a' && b <= 'f') lo = b - 'a' + 10;
            else if (b >= 'A' && b <= 'F') lo = b - 'A' + 10;
            if (hi >= 0 && lo >= 0) {
                out[o++] = (char) ((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        out[o++] = in[i];
    }
    *on = o;
}

static void check_decode(FILE * log) {
    static const char * corpus[] = {
        "", "plain", "%41", "%2f", "%2F%2f", "%zz", "%4", "%", "a%20b", "%00x",
        "%%41", "%41%", "slots%2F0", "%E2%82%AC", "%e2%82%ac", "abc%2", "%G0",
        "%0G", "%%", "a+b", "%2525", "%C3%A9", "x%2Fy%2Fz", "%2e%2e%2f",
        "%2E%2E", "100%", "%2", "%2G", "%g2", "%%41%42", "a%00b", "%ff%fe",
        "tok%3Aen", "%7B%7D", "%5B%5D", "a%2fb%2Fc", "%20%20", "  ", "%252F",
        "%C0%AF", "%F0%9F%98%80", "%:1", "%1:", "%@0", "%[1", "%`0", "%0:",
        "%/1", "%41:", "%1@", "%z1", "%1z", "%+1", "%.1",
    };
    char buf[512];
    for (size_t t = 0; t < sizeof(corpus) / sizeof(corpus[0]); ++t) {
        const char * s = corpus[t];
        size_t n = strlen(s);
        char ref[1024]; size_t rn = 0;
        native_decode(s, n, ref, &rn);
        uint8_t got[1024];
        int64_t gn = fg452_decode((uint64_t) (uintptr_t) s, (int64_t) n,
                                  (uint64_t) (uintptr_t) got, 1024);
        ++cases; ++checks;
        if (gn != (int64_t) rn || memcmp(got, ref, rn) != 0) {
            ++fails;
            printf("FAIL decode '%s' native_len=%zu goose_len=%lld\n", s, rn, (long long) gn);
        }
        // record for the Python cross-check: input hex <TAB> output hex
        int o = 0;
        for (size_t i = 0; i < n && o < (int) sizeof buf - 3; ++i)
            o += snprintf(buf + o, sizeof buf - (size_t) o, "%02x", (unsigned char) s[i]);
        buf[o++] = '\t';
        for (size_t i = 0; i < (size_t) gn && o < (int) sizeof buf - 3; ++i)
            o += snprintf(buf + o, sizeof buf - (size_t) o, "%02x", got[i]);
        buf[o] = 0;
        fprintf(log, "%s\n", buf);
    }
}

// Exhaustive "%XY" over characters just inside and outside the hex ranges:
// the decoder must reject every pair where either digit is not hex.
static void check_escape_pairs(FILE * log) {
    static const char alpha[] = "09afAFgG:@/`zZ+-2eEB?";
    char s[4] = {'%', 0, 0, 0};
    char buf[64];
    for (size_t a = 0; a < strlen(alpha); ++a)
    for (size_t b = 0; b < strlen(alpha); ++b) {
        s[1] = alpha[a];
        s[2] = alpha[b];
        char ref[8]; size_t rn = 0;
        native_decode(s, 3, ref, &rn);
        uint8_t got[8];
        int64_t gn = fg452_decode((uint64_t) (uintptr_t) s, 3, (uint64_t) (uintptr_t) got, 8);
        ++cases; ++checks;
        if (gn != (int64_t) rn || memcmp(got, ref, rn) != 0) {
            ++fails;
            printf("FAIL escape '%s' native_len=%zu goose_len=%lld\n", s, rn, (long long) gn);
        }
        int o = snprintf(buf, sizeof buf, "25%02x%02x\t", (unsigned char) s[1], (unsigned char) s[2]);
        for (size_t i = 0; i < (size_t) gn; ++i)
            o += snprintf(buf + o, sizeof buf - (size_t) o, "%02x", got[i]);
        fprintf(log, "%s\n", buf);
    }
}

// ---- native form grouping -------------------------------------------------
static int native_actions(char keys[][32], int n, int * out) {
    for (int i = 0; i < n; ++i) {
        out[i] = 0;
        for (int j = 0; j < i; ++j)
            if (strcmp(keys[i], keys[j]) == 0) { out[i] = 1; break; }
    }
    return n;
}

static void check_forms(void) {
    static const char * alphabet[] = {"a", "b", "", "ab", "a b"};
    const int A = 5;
    char keys[8][32];
    static char kbuf[64 * 32];
    int64_t koffs[8], klens[8];
    int64_t got[8];

    long total = 0;
    for (int n = 0; n <= 6; ++n) {
        long combos = 1;
        for (int i = 0; i < n; ++i) combos *= A;
        for (long code = 0; code < combos; ++code) {
            long c = code;
            size_t pos = 0;
            for (int i = 0; i < n; ++i) {
                const char * k = alphabet[c % A];
                c /= A;
                snprintf(keys[i], sizeof keys[i], "%s", k);
                koffs[i] = (int64_t) pos;
                klens[i] = (int64_t) strlen(k);
                memcpy(kbuf + pos, k, strlen(k));
                pos += strlen(k);
            }
            int ref[8] = {0};
            native_actions(keys, n, ref);
            fg452_actions(n, kbuf, (int64_t) pos, koffs, klens, got);
            ++cases; ++checks; ++total;
            for (int i = 0; i < n; ++i) {
                ++checks;
                if (got[i] != ref[i]) {
                    ++fails;
                    if (fails <= 10) printf("FAIL form n=%d i=%d key='%s' native=%d goose=%lld\n",
                        n, i, keys[i], ref[i], (long long) got[i]);
                }
            }
        }
    }
    (void) total;
}

// ---- native parameter precedence ------------------------------------------
static int native_source(int has_query, int has_path) {
    if (has_path) return 2;
    if (has_query) return 1;
    return 0;
}

int main(void) {
    goose_init(0, 0);
    FILE * log = fopen("decoded.txt", "w");
    if (!log) { perror("decoded.txt"); return 1; }

    check_decode(log);
    check_escape_pairs(log);
    fclose(log);

    check_forms();

    for (int q = 0; q < 2; ++q)
    for (int p = 0; p < 2; ++p) {
        int64_t got = fg452_source(q, p);
        int want = native_source(q, p);
        ++cases; ++checks;
        if (got != want) {
            ++fails;
            printf("FAIL source q=%d p=%d native=%d goose=%lld\n", q, p, want, (long long) got);
        }
    }

    printf("FG452_MULTIPART cases=%d values=%d failures=%d\n", cases, checks, fails);
    return fails ? 1 : 0;
}
