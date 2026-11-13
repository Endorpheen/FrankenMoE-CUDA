// Integration bridge for EXP205: the can_use predicate reads the real llama_kv_cells.
//
// Native body, llama-kv-cache.cpp:1064-1082:
//   bool can_use = cells.is_empty(idx);
//   if (!can_use && cells.seq_count(idx) == 1) {
//       const llama_pos pos_cell = cells.pos_get(idx);
//       if (!can_use) {
//           const llama_seq_id seq_id_cell = cells.seq_get(idx);
//           if (llama_hparams::is_masked_swa(n_swa, swa_type, pos_cell,
//                                            cells.seq_pos_max(seq_id_cell) + 1)) can_use = true;
//       }
//   }
#include "llama-kv-cells.h"
#include "llama-hparams.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <atomic>
#include <vector>

static std::atomic<unsigned long long> g_calls{0}, g_tested{0};
static const llama_kv_cells * g_cells;
static int g_n_swa = 0, g_swa_type = 0;

extern "C" int64_t fg205g_can_use_impl(int64_t idx) {
    g_tested.fetch_add(1);
    if (!g_cells) return 0;
    const uint32_t i = (uint32_t) idx;
    bool can = g_cells->is_empty(i);
    if (!can && g_cells->seq_count(i) == 1) {
        const llama_pos pos_cell = g_cells->pos_get(i);
        const llama_seq_id seq_id_cell = g_cells->seq_get(i);
        if (llama_hparams::is_masked_swa(g_n_swa, (llama_swa_type) g_swa_type, pos_cell,
                                         g_cells->seq_pos_max(seq_id_cell) + 1)) {
            can = true;
        }
    }
    return can ? 1 : 0;
}

static void g_fail(int64_t c) { fprintf(stderr, "FG205G validation code %lld\n", (long long)c); abort(); }
static void g_st_i64(uint64_t d, int64_t b, int64_t v) { *(int64_t *)(uintptr_t)(d + (uint64_t)b) = v; }

extern "C" {
#define fg205g_fail    g_fail
#define fg205g_st_i64  g_st_i64
#define fg205g_can_use fg205g_can_use_impl
#define gg_init_globals fg205g_init_globals
#define main fg205g_generated_main
#include "../source/find_slot2-generated.c"
#undef main
}

__attribute__((destructor)) static void fg205g_report(void) {
    if (!g_calls.load()) return;
    fprintf(stderr, "FG205G_KV_FIND_SLOTS calls=%llu tested=%llu\n",
            g_calls.load(), g_tested.load());
}

extern "C" int64_t fg205g_find_slots(const llama_kv_cells * cells, int n_swa, int swa_type,
                                     int64_t head0, int64_t n_tokens, int64_t cont, int64_t * out) {
    g_cells = cells; g_n_swa = n_swa; g_swa_type = swa_type;
    g_calls.fetch_add(1);
    return gg_function_4((int64_t) cells->size(), head0, n_tokens, cont,
                         (uint64_t)(uintptr_t) out, 0);
}

extern "C" int fg205g_enabled(void) {
    const char * f = getenv("GGML_GOOSE_KV_FIND_SLOT");
    return !(f && strcmp(f, "0") == 0);
}
