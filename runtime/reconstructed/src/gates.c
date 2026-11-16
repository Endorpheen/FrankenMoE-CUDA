#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int goose_on(const char * name) {
    const char * f = getenv(name);
    return !(f && strcmp(f, "0") == 0);
}
int fg286m_enabled(void) { return goose_on("GGML_GOOSE_MMAP_BOOK"); }
int fg262a_enabled(void) { return goose_on("GGML_GOOSE_MOE_CACHE_ADMIT"); }
int fg756s_enabled(void) { return goose_on("GGML_GOOSE_MOE_SLRU"); }

// The SLRU protected-segment ceiling, as num/den. Default 7/8: EXP-757 measured hit rate 83.7 -> 84.8% and
// upload -6.7% against the native 1/2, with identical answers at 128/256/512. Setting
// GGML_GOOSE_SLRU_PROT=1/2 restores the native policy exactly, which is what the provenance arm of the
// gate checks.
static int64_t prot_num = -1, prot_den = -1;
static void prot_parse(void) {
    const char * v = getenv("GGML_GOOSE_SLRU_PROT");
    int64_t n = 7, d = 8;
    if (v != NULL) {
        long long a = 0, b = 0;
        if (sscanf(v, "%lld/%lld", &a, &b) == 2 && a > 0 && b > 0 && a <= b) {
            n = a; d = b;
        }
    }
    prot_num = n;
    prot_den = d;
}
int64_t fg756s_prot_num(void) { if (prot_num < 0) prot_parse(); return prot_num; }
int64_t fg756s_prot_den(void) { if (prot_den < 0) prot_parse(); return prot_den; }
