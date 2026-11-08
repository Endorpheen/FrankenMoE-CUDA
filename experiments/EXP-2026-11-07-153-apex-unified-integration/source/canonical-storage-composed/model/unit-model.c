// worker-4/canonical-model: strong C entries for the generated model module.
#include "canonical-model.h"
#include "canonical-storage-required.h"

#define main fgc_model_generated_main
#include "gen/canonical-model.c"
#undef main

int64_t fgc_model_new(void) { return gg_function_1(0); }
int64_t fgc_model_free(int64_t m) { return gg_function_66(m, 0); }
int64_t fgc_model_kind(int64_t m) { return gg_function_10(m, 0); }
int64_t fgc_model_set_int(int64_t m, int64_t k, int64_t v) { return gg_function_13(m, k, v, 0); }
int64_t fgc_model_get_int(int64_t m, int64_t k) { return gg_function_15(m, k, 0); }
int64_t fgc_model_set_float(int64_t m, int64_t k, float v) { return gg_function_18(m, k, v, 0); }
float fgc_model_get_float(int64_t m, int64_t k) { return gg_function_21(m, k, 0); }
int64_t fgc_model_set_handle(int64_t m, int64_t k, int64_t v) { return gg_function_24(m, k, v, 0); }
int64_t fgc_model_hparams_get(int64_t m, int64_t k) { return gg_function_28(m, k, 0); }
int64_t fgc_model_hparams_set(int64_t m, int64_t k, int64_t v) { return gg_function_26(m, k, v, 0); }
int64_t fgc_model_n_layers(int64_t m) { return gg_function_30(m, 0); }
int64_t fgc_model_layer_new(int64_t m, int64_t l) { return gg_function_33(m, l, 0); }
int64_t fgc_model_layer_get(int64_t m, int64_t l) { return gg_function_39(m, l, 0); }
int64_t fgc_model_layer_set_int(int64_t m, int64_t l, int64_t k, int64_t v) { return gg_function_41(m, l, k, v, 0); }
int64_t fgc_model_layer_get_int(int64_t m, int64_t l, int64_t k) { return gg_function_43(m, l, k, 0); }
int64_t fgc_model_bind_set(int64_t m, int64_t l, int64_t k, int64_t t) { return gg_function_45(m, l, k, t, 0); }
int64_t fgc_model_bind_get(int64_t m, int64_t l, int64_t k) { return gg_function_48(m, l, k, 0); }
int64_t fgc_model_bind_erase(int64_t m, int64_t l, int64_t k) { return gg_function_50(m, l, k, 0); }
int64_t fgc_model_bind_from_loader(int64_t m, int64_t ld, int64_t l, int64_t k) { return gg_function_53(m, ld, l, k, 0); }
int64_t fgc_model_vocab_set(int64_t m, int64_t v) { return gg_function_56(m, v, 0); }
int64_t fgc_model_vocab_get(int64_t m) { return gg_function_58(m, 0); }
int64_t fgc_model_vocab_n_tokens(int64_t m) { return gg_function_60(m, 0); }
int64_t fgc_model_validate(int64_t m) { return gg_function_63(m, 0); }
