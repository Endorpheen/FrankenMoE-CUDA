#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static _Atomic unsigned long long g_calls;
__attribute__((destructor)) static void fg207p_report(void) {
    if (!atomic_load(&g_calls)) return;
    fprintf(stderr, "FG207P_KV_SEQPOS calls=%llu\n", atomic_load(&g_calls));
}
static void g_fail(int64_t c){fprintf(stderr,"FG207P validation code %lld\n",(long long)c);abort();}
static int64_t g_ld_i64(uint64_t d,int64_t b){return *(const int64_t*)(uintptr_t)(d+(uint64_t)b);}
#define fg207p_ld_i64 g_ld_i64
#define fg207p_fail   g_fail
#define gg_init_globals fg207p_init_globals
#define main fg207p_generated_main
#include "seqpos-generated.c"
#undef main
int64_t fg207p_has_bit(uint64_t seqw, int64_t i, int64_t s, int64_t w) {
    atomic_fetch_add(&g_calls, 1);
    return gg_function_1(seqw, i, s, w, 0);
}
int64_t fg207p_seq_pos_min(uint64_t pos, uint64_t seqw, int64_t n, int64_t s, int64_t w) {
    atomic_fetch_add(&g_calls, 1);
    return gg_function_5(pos, seqw, n, s, w, 0);
}
int64_t fg207p_seq_pos_max(uint64_t pos, uint64_t seqw, int64_t n, int64_t s, int64_t w) {
    atomic_fetch_add(&g_calls, 1);
    return gg_function_6(pos, seqw, n, s, w, 0);
}
int fg207p_enabled(void){ const char* f=getenv("GGML_GOOSE_KV_SEQPOS"); return !(f&&strcmp(f,"0")==0); }
