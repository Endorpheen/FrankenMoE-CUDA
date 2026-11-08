// worker-4/canonical-model: synthetic MODEL/HPARAMS/layer/binding corpus.
// Uses the real Goose model module and a test-only conformance implementation of
// the canonical-storage contract (not production). No model/weights/GPU.
#include "canonical-model.h"
#include "canonical-storage-required.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL line=%d %s\n", __LINE__, #x); return 1; } } while (0)

extern "C" {
int64_t fgc_test_vocab_new(int64_t n_tokens);
int64_t fgc_test_loader_new(void);
int64_t fgc_test_loader_add(int64_t loader, int64_t layer, int64_t kind, int64_t tensor);
int64_t fgc_test_record_frees(void);
int64_t fgc_test_vec_frees(void);
int64_t fgc_test_map_frees(void);
void    fgc_test_vocab_free(int64_t vocab);
void    fgc_test_loader_free(int64_t loader);
}

static int64_t set_hp(int64_t m, int64_t k, int64_t v) { return fgc_model_hparams_set(m, k, v); }
static int64_t get_hp(int64_t m, int64_t k) { return fgc_model_hparams_get(m, k); }

int main() {
    const int64_t m = fgc_model_new();
    CHECK(m != 0);
    CHECK(fgc_model_kind(m) == FGC_KIND_MODEL);

    CHECK(fgc_model_set_int(m, FGC_M_ARCH, 42) == 0);
    CHECK(fgc_model_set_int(m, FGC_M_N_TENSORS, 500) == 0);
    CHECK(fgc_model_get_int(m, FGC_M_ARCH) == 42);
    CHECK(fgc_model_get_int(m, FGC_M_N_TENSORS) == 500);

    // HPARAMS record: scalars and f32-bit fields.
    CHECK(set_hp(m, FGC_H_N_EMBD, 8) == 0);
    CHECK(set_hp(m, FGC_H_N_LAYER, 2) == 0);
    CHECK(set_hp(m, FGC_H_N_LAYER_ALL, 2) == 0);
    CHECK(set_hp(m, FGC_H_N_ROT, 8) == 0);
    CHECK(set_hp(m, FGC_H_N_HEAD, 2) == 0);
    CHECK(set_hp(m, FGC_H_N_HEAD_KV, 1) == 0);
    CHECK(set_hp(m, FGC_H_N_EMBD_HEAD_K, 8) == 0);
    CHECK(set_hp(m, FGC_H_N_EMBD_HEAD_V, 8) == 0);
    CHECK(set_hp(m, FGC_H_N_EMBD_K_GQA, 8) == 0);
    CHECK(set_hp(m, FGC_H_N_EMBD_V_GQA, 8) == 0);
    CHECK(set_hp(m, FGC_H_N_EXPERT, 8) == 0);
    CHECK(set_hp(m, FGC_H_N_EXPERT_USED, 2) == 0);
    CHECK(set_hp(m, FGC_H_ROPE_TYPE, 0) == 0);
    CHECK(set_hp(m, FGC_H_FREQ_BASE, fgc_bits_f32(10000.0f)) == 0);
    CHECK(fgc_from_bits_i64(get_hp(m, FGC_H_FREQ_BASE)) == 10000.0f);
    CHECK(get_hp(m, FGC_H_N_EMBD) == 8);
    CHECK(get_hp(m, FGC_H_N_LAYER) == 2);

    // Layers and per-layer metadata.
    for (int64_t l = 0; l < 2; ++l) {
        const int64_t lh = fgc_model_layer_new(m, l);
        CHECK(lh != 0);
        CHECK(fgc_model_layer_get(m, l) == lh);
        CHECK(fgc_model_layer_set_int(m, l, FGC_L_N_HEAD, 2 + (int64_t) l) == 0);
        CHECK(fgc_model_layer_set_int(m, l, FGC_L_N_HEAD_KV, 1) == 0);
        CHECK(fgc_model_layer_set_int(m, l, FGC_L_N_FF, 32) == 0);
        CHECK(fgc_model_layer_set_int(m, l, FGC_L_N_FF_EXP, 16) == 0);
        CHECK(fgc_model_layer_set_int(m, l, FGC_L_N_EXPERT_USED, 2) == 0);
        CHECK(fgc_model_layer_set_int(m, l, FGC_L_IS_RECR, l == 0 ? 1 : 0) == 0);
        CHECK(fgc_model_layer_get_int(m, l, FGC_L_N_HEAD) == 2 + (int64_t) l);
        CHECK(fgc_model_layer_get_int(m, l, FGC_L_IS_RECR) == (l == 0 ? 1 : 0));
    }
    CHECK(fgc_model_n_layers(m) == 2);

    // Global tensors (layer -1) and per-layer bindings.
    CHECK(fgc_model_bind_set(m, FGC_MODEL_GLOBAL_LAYER, FGC_T_TOK_EMBD, 0x1000) == 0);
    CHECK(fgc_model_bind_set(m, FGC_MODEL_GLOBAL_LAYER, FGC_T_OUTPUT_NORM, 0x1001) == 0);
    CHECK(fgc_model_bind_set(m, FGC_MODEL_GLOBAL_LAYER, FGC_T_OUTPUT, 0x1002) == 0);
    for (int64_t l = 0; l < 2; ++l) {
        const int64_t base = 0x1100 + l * 0x10;
        CHECK(fgc_model_bind_set(m, l, FGC_T_ATTN_NORM, base + 0) == 0);
        CHECK(fgc_model_bind_set(m, l, FGC_T_WQKV, base + 1) == 0);
        CHECK(fgc_model_bind_set(m, l, FGC_T_WO, base + 2) == 0);
        CHECK(fgc_model_bind_set(m, l, FGC_T_FFN_UP_EXPS, base + 3) == 0);
    }
    CHECK(fgc_model_bind_get(m, FGC_MODEL_GLOBAL_LAYER, FGC_T_TOK_EMBD) == 0x1000);
    CHECK(fgc_model_bind_get(m, 1, FGC_T_WQKV) == 0x1101 + 0x10);
    CHECK(fgc_model_get_int(m, FGC_M_N_BINDINGS) == 11);

    // Loader handover (typed cross-schema import) and VOCAB reference.
    const int64_t loader = fgc_test_loader_new();
    CHECK(loader != 0);
    CHECK(fgc_test_loader_add(loader, 0, FGC_T_WQKV_B, 0x1200) == 0);
    CHECK(fgc_test_loader_add(loader, 1, FGC_T_WV, 0x1201) == 0);
    CHECK(fgc_model_bind_from_loader(m, loader, 0, FGC_T_WQKV_B) == 0);
    CHECK(fgc_model_bind_from_loader(m, loader, 1, FGC_T_WV) == 0);
    CHECK(fgc_model_bind_get(m, 0, FGC_T_WQKV_B) == 0x1200);
    CHECK(fgc_model_bind_get(m, 1, FGC_T_WV) == 0x1201);
    CHECK(fgc_model_get_int(m, FGC_M_N_BINDINGS) == 13);

    const int64_t vocab = fgc_test_vocab_new(128);
    CHECK(vocab != 0);
    CHECK(fgc_model_vocab_set(m, vocab) == 0);
    CHECK(fgc_model_vocab_get(m) == vocab);
    CHECK(fgc_model_vocab_n_tokens(m) == 128);

    CHECK(fgc_model_validate(m) == 0);

    // Binding replacement does not double-count; erase decrements.
    CHECK(fgc_model_bind_set(m, 0, FGC_T_WQKV, 0x1300) == 0);
    CHECK(fgc_model_get_int(m, FGC_M_N_BINDINGS) == 13);
    CHECK(fgc_model_bind_get(m, 0, FGC_T_WQKV) == 0x1300);
    CHECK(fgc_model_bind_erase(m, 0, FGC_T_WQKV) == 0);
    CHECK(fgc_model_bind_get(m, 0, FGC_T_WQKV) == 0);
    CHECK(fgc_model_get_int(m, FGC_M_N_BINDINGS) == 12);

    // Invalid operations.
    CHECK(fgc_model_bind_set(m, 0, FGC_T_WO, 0) == -1);
    CHECK(fgc_model_layer_new(0, 0) == 0);
    CHECK(fgc_model_hparams_set(m, FGC_H_N_LAYER, 3) == 0);
    CHECK(fgc_model_validate(m) == -3);       // layer count mismatch
    CHECK(fgc_model_hparams_set(m, FGC_H_N_LAYER, 2) == 0);
    CHECK(fgc_model_validate(m) == 0);

    // Ownership: model owns records/vec/maps only; tensor handles are borrowed.
    CHECK(fgc_model_free(m) == 0);
    CHECK(fgc_test_record_frees() == 4);      // model + hparams + 2 layers
    CHECK(fgc_test_vec_frees() == 1);
    CHECK(fgc_test_map_frees() == 2);
    fgc_test_loader_free(loader);
    fgc_test_vocab_free(vocab);

    std::printf("{\"status\":\"PASS\",\"corpus\":\"fgc-model-storage\"}\n");
    return 0;
}
