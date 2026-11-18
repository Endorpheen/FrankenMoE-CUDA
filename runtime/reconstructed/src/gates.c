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

// The QWEN35 tokenizer path (EXP954-956): vocabulary questions, BPE tokenisation and
// piece decoding are answered by the Goose engine. On by default; GGML_GOOSE_TOKENIZER=0
// restores the native bodies exactly, which is what the rollback arm of EXP957 checks.
int goose_tok_enabled(void) { return goose_on("GGML_GOOSE_TOKENIZER"); }

// The QWEN35 tokenizer path (EXP954-957) has a tested switch; the sampler filters
// get their own so either can roll back alone. On by default; GGML_GOOSE_SAMPLER=0
// restores the native filters exactly (EXP970 checks that arm).
int goose_sampler_enabled(void) { return goose_on("GGML_GOOSE_SAMPLER"); }

// Positive witness for the sampler arm: a pilot test cannot tell "Goose decided"
// from "the native body ran" by looking at output alone, because the two agree -
// that is the point of the gate. This counter is incremented by the hook, so the
// test can assert which path produced the answer (the same mechanism EXP954 used
// for the tokenizer).
static int64_t sampler_calls = 0;
void goose_sampler_note_call(void) { sampler_calls++; }
int64_t goose_sampler_calls(void) { return sampler_calls; }

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
