#pragma once
// worker-4/canonical-model: exact canonical-storage ABI used by the MODEL schema.
//
// Signatures below are the actual worker8 canonical-storage.h ABI: free
// functions are void, vec_data returns void *, map_get takes int64_t *.
// Generated Goose code cannot use void/pointer signatures directly, so the
// distinctly named bridge symbols at the end are the typed i64 forms. They are
// thin bindings, not a second storage implementation.
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t fgc_handle;

// ---- worker8 canonical storage (actual ABI) -------------------------------
fgc_handle fgc_record_new(int64_t kind);
int64_t    fgc_record_get(fgc_handle handle, int64_t key);
int64_t    fgc_record_set(fgc_handle handle, int64_t key, int64_t value);
int64_t    fgc_record_kind(fgc_handle handle);
int64_t    fgc_record_count(fgc_handle handle);
int64_t    fgc_record_clear(fgc_handle handle);
int64_t fgc_i64_record_free(fgc_handle handle);

fgc_handle fgc_vec_new(int64_t item_bytes);
int64_t    fgc_vec_len(fgc_handle handle);
int64_t fgc_i64_vec_data(fgc_handle handle);
int64_t    fgc_vec_item_bytes(fgc_handle handle);
int64_t    fgc_vec_reserve(fgc_handle handle, int64_t count);
int64_t    fgc_vec_resize(fgc_handle handle, int64_t count);
int64_t    fgc_vec_clear(fgc_handle handle);
int64_t fgc_i64_vec_free(fgc_handle handle);

fgc_handle fgc_map_new(void);
int64_t    fgc_map_put(fgc_handle handle, int64_t key, int64_t value);
int64_t    fgc_i64_map_get(fgc_handle handle, int64_t key, int64_t out_value);
int64_t    fgc_map_erase(fgc_handle handle, int64_t key);
int64_t    fgc_map_size(fgc_handle handle);
int64_t    fgc_map_clear(fgc_handle handle);
int64_t fgc_i64_map_free(fgc_handle handle);

// ---- bridge symbols for generated Goose code (distinct i64 forms) ---------
int64_t fgc_record_free_i64(fgc_handle handle);
int64_t fgc_vec_free_i64(fgc_handle handle);
int64_t fgc_map_free_i64(fgc_handle handle);
int64_t fgc_vec_data_i64(fgc_handle handle);
int64_t fgc_vec_get_i64(fgc_handle handle, int64_t index);
int64_t fgc_vec_set_i64(fgc_handle handle, int64_t index, int64_t value);
int64_t fgc_map_get_i64(fgc_handle handle, int64_t key); // value or 0
int64_t fgc_map_has(fgc_handle handle, int64_t key);     // 1/0
int64_t fgc_bits_f32(float value);
float   fgc_from_bits_i64(int64_t bits);

// Cross-schema imports (worker16 / worker15); strong external symbols.
int64_t fgc_vocab_n_tokens(int64_t vocab);
int64_t fgc_loader_get_tensor(int64_t loader, int64_t layer, int64_t kind);
int64_t fgc_loader_handover(int64_t loader, int64_t model);

#ifdef __cplusplus
}
#endif
