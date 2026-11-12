// Bridge for the MoE cache copy decisions (EXP-263): primitives, the default-on switch, and the four entry points.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void g_fail(int64_t c) { fprintf(stderr, "FG263C validation code %lld\n", (long long) c); abort(); }
static int64_t g_ld32(uint64_t b, int64_t i) { int32_t v; memcpy(&v, (void *) (uintptr_t) (b + (uint64_t) i), 4); return v; }
static void g_st64(uint64_t b, int64_t i, int64_t v) { memcpy((void *) (uintptr_t) (b + (uint64_t) i), &v, 8); }

#define fg263c_fail   g_fail
#define fg263c_ld_i32 g_ld32
#define fg263c_st_i64 g_st64
#define main fg263c_generated_main
#include "copy_decisions-functions.c"
#undef main

// Default on, per rule 10: the switch exists only to fall back to the native loops.
int fg263c_enabled(void) {
    const char * f = getenv("GGML_GOOSE_MOE_CACHE_COPY");
    return !(f && strcmp(f, "0") == 0);
}
