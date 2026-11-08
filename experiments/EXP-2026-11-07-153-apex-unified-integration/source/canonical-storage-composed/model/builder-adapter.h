#pragma once
// worker-4/canonical-model: typed consumption seam for the normal Goose builder.
// The accepted Apex builder algorithms stay unchanged; only its C++ model field
// reads (fg153_am_*) are replaced by these schema accessors.
#include "canonical-model.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline int64_t fgc_builder_n_layer(int64_t model) {
    return fgc_model_hparams_get(model, FGC_H_N_LAYER);
}
static inline int64_t fgc_builder_n_embd(int64_t model) {
    return fgc_model_hparams_get(model, FGC_H_N_EMBD);
}
static inline int64_t fgc_builder_n_head(int64_t model, int64_t layer) {
    return fgc_model_layer_get_int(model, layer, FGC_L_N_HEAD);
}
static inline int64_t fgc_builder_n_head_kv(int64_t model, int64_t layer) {
    return fgc_model_layer_get_int(model, layer, FGC_L_N_HEAD_KV);
}
static inline int64_t fgc_builder_n_ff(int64_t model, int64_t layer) {
    return fgc_model_layer_get_int(model, layer, FGC_L_N_FF);
}
static inline int64_t fgc_builder_is_recr(int64_t model, int64_t layer) {
    return fgc_model_layer_get_int(model, layer, FGC_L_IS_RECR);
}
static inline int64_t fgc_builder_tensor(int64_t model, int64_t layer, int64_t kind) {
    return fgc_model_bind_get(model, layer, kind);
}
static inline int64_t fgc_builder_global_tensor(int64_t model, int64_t kind) {
    return fgc_model_bind_get(model, FGC_MODEL_GLOBAL_LAYER, kind);
}
static inline int64_t fgc_builder_vocab_n_tokens(int64_t model) {
    return fgc_model_vocab_n_tokens(model);
}

#ifdef __cplusplus
}
#endif
