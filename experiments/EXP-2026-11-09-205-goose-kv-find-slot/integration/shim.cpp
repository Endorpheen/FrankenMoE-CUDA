// C++ shim: the can_use predicate reads the real llama_kv_cells; the walk itself is Goose's.
#include "llama-kv-cells.h"
#include "llama-hparams.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
int64_t fg205g_run(int64_t size, int64_t head0, int64_t n_tokens, int64_t cont,
                   int64_t * out, int64_t (*pred)(int64_t));
}

static const llama_kv_cells * g_cells = nullptr;
static int g_n_swa = 0, g_swa_type = 0;

// native body, llama-kv-cache.cpp:1064-1082
static int64_t can_use_impl(int64_t idx) {
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

extern "C" int64_t fg205g_find_slots(const llama_kv_cells * cells, int n_swa, int swa_type,
                                     int64_t head0, int64_t n_tokens, int64_t cont, int64_t * out) {
    g_cells = cells; g_n_swa = n_swa; g_swa_type = swa_type;
    return fg205g_run((int64_t) cells->size(), head0, n_tokens, cont, out, can_use_impl);
}

extern "C" int fg205g_enabled(void) {
    const char * f = getenv("GGML_GOOSE_KV_FIND_SLOT");
    return !(f && strcmp(f, "0") == 0);
}
