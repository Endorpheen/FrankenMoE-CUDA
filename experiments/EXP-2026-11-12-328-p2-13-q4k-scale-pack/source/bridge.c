#include <stdint.h>
static void g_fail(int64_t c) { (void)c; __builtin_trap(); }
static int64_t g_ld_u8(uint64_t a) { return *(const uint8_t *)(uintptr_t)a; }
static void g_st_u8(uint64_t a, int64_t v) { *(uint8_t *)(uintptr_t)a = (uint8_t)v; }
#define fg328q_ld_u8 g_ld_u8
#define fg328q_st_u8 g_st_u8
#define fg328q_fail  g_fail
#define main fg328q_generated_main
#include "pack-generated.c"
#undef main
int fg328q_enter(const uint8_t * lslm, uint8_t * scales) {
    fg328q_fn_1((uint64_t)(uintptr_t) lslm, (uint64_t)(uintptr_t) scales, 0);
    return 0;
}
