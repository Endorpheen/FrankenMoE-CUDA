// worker-2/kv-bridge.h
//
// Typed opaque storage bridge for kv-lifecycle.goose and kv-seq-control.goose.
//
// It stores cache metadata, slot_info, contexts and ordered collections as
// opaque handles. It DOES NOT implement llama_kv_cache control logic and does
// NOT call prepare/find_slot/seq_* on the native class. Native control stays
// in Goose. Backend/tensor effects (sync, stream copy, K-shift graph, zeroing)
// are registered callbacks supplied by the parent.
//
// UNVERIFIED_PROPOSAL. Not built, not integrated.

#pragma once

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// Configuration is per cache instance. Values that are compile-time constants
// (LLAMA_MAX_SEQ, LLAMA_TOKEN_NULL) are taken from llama.h/llama-cparams.h;
// the rest are supplied by the parent after create. No process-global mutable
// configuration is used.
void fg_kv_configure(int64_t kv,
        int64_t n_seq_max,
        int64_t n_pad,
        int64_t n_swa,
        int64_t swa_type,
        int64_t arch_step35,
        int64_t n_pos_per_embd,
        int64_t n_layers,
        int64_t ple_n_heads,
        int64_t ple_image_token_id,
        int64_t ple_eos_token_id,
        int64_t rope_type_none,
        int64_t type_k,
        int64_t type_v);

// Per-layer flags used by update(): whether the layer has a V stream and
// whether it participates in RoPE (for K-shift).
void fg_kv_set_layer(int64_t kv, int64_t il, int64_t has_v, int64_t has_rope);

// Optional parent callbacks. Defaults abort when called, so the ordinary
// Apex path (n_swa == 0, no shift graph, no cross-stream copy) never needs
// them. Register real implementations before enabling those branches.
typedef int64_t (*fg_kv_swa_masked_fn)(int64_t n_swa, int64_t swa_type, int64_t p0, int64_t p1);
typedef void    (*fg_kv_sync_fn)(int64_t lctx);
typedef void    (*fg_kv_tensor_copy_fn)(int64_t kv, int64_t src, int64_t dst, int64_t il, int64_t is_v);
typedef int64_t (*fg_kv_shift_fn)(int64_t kv, int64_t lctx); // 0 success, non-zero failure
typedef void    (*fg_kv_zero_cells_fn)(int64_t kv, int64_t strm, int64_t sinfo, int64_t contiguous);
typedef void    (*fg_kv_clear_data_fn)(int64_t kv);
typedef int64_t (*fg_kv_other_pos_fn)(int64_t kv, int64_t seq, int64_t is_max);

void fg_kv_bind_swa(int64_t kv, fg_kv_swa_masked_fn fn);
void fg_kv_bind_sync(int64_t kv, fg_kv_sync_fn fn);
void fg_kv_bind_tensor_copy(int64_t kv, fg_kv_tensor_copy_fn fn);
void fg_kv_bind_shift(int64_t kv, fg_kv_shift_fn fn);
void fg_kv_bind_zero_cells(int64_t kv, fg_kv_zero_cells_fn fn);
void fg_kv_bind_clear_data(int64_t kv, fg_kv_clear_data_fn fn);
void fg_kv_bind_other_pos(int64_t kv, fg_kv_other_pos_fn fn);

#ifdef __cplusplus
}
#endif
