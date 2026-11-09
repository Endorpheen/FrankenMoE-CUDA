// Bridge for the Goose tokenizer module. Nothing here is algorithmic.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

static _Atomic unsigned long long g_utf8, g_bigram, g_entry;

__attribute__((destructor)) static void fg202t_report(void) {
    if (!atomic_load(&g_utf8) && !atomic_load(&g_entry)) return;
    fprintf(stderr, "FG202T_TOKENIZER utf8_len=%llu bigram=%llu native=%llu\n",
            atomic_load(&g_utf8), atomic_load(&g_bigram), atomic_load(&g_entry));
}

static void g_fail(int64_t c) {
    fprintf(stderr, "FG202T validation code %lld\n", (long long)c);
    abort();
}
static void g_st_i64(uint64_t base, int64_t byte, int64_t v) {
    *(int64_t *)(uintptr_t)(base + (uint64_t)byte) = v;
}

#define fg202t_st_i64 g_st_i64
#define fg202t_fail   g_fail
#define main fg202t_generated_main
#include "vocab-generated.c"
#undef main

int64_t fg202t_bpe_add_allowed(int64_t l, int64_t r, int64_t rank) {
    atomic_fetch_add(&g_bigram, 1);
    return gg_function_8(l, r, rank, 0);
}
int64_t fg202t_bpe_merge_valid(int64_t ln, int64_t rn, int64_t concat_matches) {
    return gg_function_9(ln, rn, concat_matches, 0);
}
int64_t fg202t_bpe_ignore_merges(int64_t ignore, int64_t has_token) {
    return gg_function_10(ignore, has_token, 0);
}
int64_t fg202t_utf8_len(int64_t src) { atomic_fetch_add(&g_utf8, 1); return gg_function_1(src, 0); }
int64_t fg202t_bigram_valid(int64_t ln, int64_t rn, int64_t sz) {
    atomic_fetch_add(&g_bigram, 1);
    return gg_function_4(ln, rn, sz, 0);
}
int64_t fg202t_try_add_allowed(int64_t l, int64_t r, int64_t tok, int64_t null_tok, int64_t n) {
    return gg_function_5(l, r, tok, null_tok, n, 0);
}
int64_t fg202t_resegment_kind(int64_t is_tok, int64_t has_merge) { return gg_function_6(is_tok, has_merge, 0); }
int64_t fg202t_clamp_symbol_len(int64_t len, int64_t remaining) { return gg_function_7(len, remaining, 0); }

int fg202t_enabled(void) {
    const char * f = getenv("GGML_GOOSE_TOKENIZER");
    return !(f && strcmp(f, "0") == 0);
}
