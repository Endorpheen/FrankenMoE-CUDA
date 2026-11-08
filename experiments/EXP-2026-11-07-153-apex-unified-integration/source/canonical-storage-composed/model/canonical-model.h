#pragma once
// worker-4/canonical-model: APX030 MODEL/HPARAMS/model-binding storage schema.
//
// Schema is derived from inputs/native/src/llama-model.h,
// inputs/native/src/llama-hparams.h, inputs/parent-modules/apex-model/abi.json
// and binding-services.cpp. This header publishes the exact cross-module
// imports required from worker8 (canonical storage) and from the VOCAB (16) and
// loader (15) schemas. No guessed private offsets; all access is by record key.
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Record kinds owned by this schema.
enum fgc_model_kind {
    FGC_KIND_MODEL   = 1,
    FGC_KIND_HPARAMS = 2,
    FGC_KIND_LAYER   = 3,
};

// MODEL record keys.
enum fgc_model_key {
    FGC_M_ARCH = 0,
    FGC_M_NAME_PTR = 1,          // borrowed const char *
    FGC_M_N_TENSORS = 2,
    FGC_M_N_DEVICES = 3,
    FGC_M_N_GPU_LAYERS = 4,
    FGC_M_SPLIT_MODE = 5,
    FGC_M_HAS_TENSOR_OVERRIDES = 6,
    FGC_M_T_START_US = 7,
    FGC_M_T_LOAD_US = 8,
    FGC_M_VOCAB = 9,             // cross-schema VOCAB handle (borrowed)
    FGC_M_HPARAMS = 10,          // owned HPARAMS record
    FGC_M_LAYERS_VEC = 11,       // owned vec<i64> of LAYER handles
    FGC_M_LAYERS_MAP = 12,       // owned map<layer, LAYER handle>
    FGC_M_BINDINGS_MAP = 13,     // owned map<layer*64+kind, tensor handle>
    FGC_M_N_BINDINGS = 14,       // owned count
    FGC_M_N_LAYERS = 15,
    FGC_M_COUNT = 16,
};

// HPARAMS record keys (scalar; floats stored as f32 bits in i64).
enum fgc_hparams_key {
    FGC_H_N_EMBD = 0,
    FGC_H_N_LAYER = 1,
    FGC_H_N_LAYER_ALL = 2,
    FGC_H_N_LAYER_NEXTN = 3,
    FGC_H_N_ROT = 4,
    FGC_H_N_HEAD = 5,
    FGC_H_N_HEAD_KV = 6,
    FGC_H_N_EMBD_HEAD_K = 7,
    FGC_H_N_EMBD_HEAD_V = 8,
    FGC_H_N_EMBD_K_GQA = 9,
    FGC_H_N_EMBD_V_GQA = 10,
    FGC_H_N_EXPERT = 11,
    FGC_H_N_EXPERT_USED = 12,
    FGC_H_N_EMBD_R = 13,
    FGC_H_N_EMBD_S = 14,
    FGC_H_SSM_D_INNER = 15,
    FGC_H_SSM_D_STATE = 16,
    FGC_H_SSM_N_GROUP = 17,
    FGC_H_SSM_DT_RANK = 18,
    FGC_H_N_EXPERT_GROUPS = 19,
    FGC_H_N_DEEPSTACK_LAYERS = 20,
    FGC_H_N_POS_PER_EMBD = 21,
    FGC_H_ATTN_SOFT_CAP = 22,
    FGC_H_N_CTX_TRAIN = 23,
    FGC_H_N_CTX_ORIG_YARN = 24,
    FGC_H_ROPE_TYPE = 25,
    FGC_H_N_EMBD_INP = 26,
    FGC_H_N_EMBD_OUT = 27,
    FGC_H_F_ATTENTION_SCALE = 28,
    FGC_H_F_NORM_RMS_EPS = 29,
    FGC_H_EXPERT_WEIGHTS_SCALE = 30,
    FGC_H_F_ATTN_LOGIT_SOFTCAPPING = 31,
    FGC_H_F_MAX_ALIBI_BIAS = 32,
    FGC_H_F_EMBEDDING_SCALE = 33,
    FGC_H_FREQ_BASE = 34,
    FGC_H_FREQ_SCALE = 35,
    FGC_H_EXT_FACTOR = 36,
    FGC_H_ATTN_FACTOR = 37,
    FGC_H_BETA_FAST = 38,
    FGC_H_BETA_SLOW = 39,
    FGC_H_ROPE_SECTION0 = 40,
    FGC_H_ROPE_SECTION1 = 41,
    FGC_H_ROPE_SECTION2 = 42,
    FGC_H_ROPE_SECTION3 = 43,
    FGC_H_COUNT = 44,
};

// LAYER record keys (per-layer metadata; tensors live in the binding map).
enum fgc_layer_key {
    FGC_L_N_HEAD = 0,
    FGC_L_N_HEAD_KV = 1,
    FGC_L_N_FF = 2,
    FGC_L_N_FF_EXP = 3,
    FGC_L_N_EXPERT_USED = 4,
    FGC_L_IS_RECR = 5,
    FGC_L_ROPE_SECTION0 = 6,
    FGC_L_ROPE_SECTION1 = 7,
    FGC_L_ROPE_SECTION2 = 8,
    FGC_L_ROPE_SECTION3 = 9,
    FGC_L_COUNT = 10,
};

// Tensor binding kinds, in the exact order of
// inputs/parent-modules/apex-model/abi.json:weights.
enum fgc_tensor_kind {
    FGC_T_TOK_EMBD = 0,
    FGC_T_ATTN_NORM = 1,
    FGC_T_ATTN_POST_NORM = 2,
    FGC_T_OUTPUT_NORM = 3,
    FGC_T_OUTPUT_S = 4,
    FGC_T_OUTPUT = 5,
    FGC_T_FFN_GATE_INP_SHEXP = 6,
    FGC_T_WQKV = 7,
    FGC_T_WQKV_S = 8,
    FGC_T_WQKV_GATE = 9,
    FGC_T_WQKV_GATE_S = 10,
    FGC_T_ATTN_Q_NORM = 11,
    FGC_T_ATTN_K_NORM = 12,
    FGC_T_WO = 13,
    FGC_T_WO_S = 14,
    FGC_T_SSM_BETA = 15,
    FGC_T_SSM_BETA_S = 16,
    FGC_T_SSM_ALPHA = 17,
    FGC_T_SSM_ALPHA_S = 18,
    FGC_T_SSM_DT = 19,
    FGC_T_SSM_A = 20,
    FGC_T_SSM_CONV1D = 21,
    FGC_T_SSM_NORM = 22,
    FGC_T_SSM_OUT = 23,
    FGC_T_SSM_OUT_S = 24,
    FGC_T_FFN_GATE_INP = 25,
    FGC_T_FFN_UP_EXPS = 26,
    FGC_T_FFN_GATE_EXPS = 27,
    FGC_T_FFN_DOWN_EXPS = 28,
    FGC_T_FFN_GATE_UP_EXPS = 29,
    FGC_T_FFN_UP_EXPS_S = 30,
    FGC_T_FFN_GATE_EXPS_S = 31,
    FGC_T_FFN_DOWN_EXPS_S = 32,
    FGC_T_FFN_UP_SHEXP = 33,
    FGC_T_FFN_UP_SHEXP_S = 34,
    FGC_T_FFN_GATE_SHEXP = 35,
    FGC_T_FFN_GATE_SHEXP_S = 36,
    FGC_T_FFN_DOWN_SHEXP = 37,
    FGC_T_FFN_DOWN_SHEXP_S = 38,
    FGC_T_WQKV_B = 39,
    FGC_T_WQ_B = 40,
    FGC_T_WK_B = 41,
    FGC_T_WV_B = 42,
    FGC_T_WQ = 43,
    FGC_T_WQ_S = 44,
    FGC_T_WK = 45,
    FGC_T_WK_S = 46,
    FGC_T_WV = 47,
    FGC_T_WV_S = 48,
    FGC_T_COUNT = 49,
};

#define FGC_MODEL_GLOBAL_LAYER (-1)
#define FGC_MODEL_BIND_KEY(layer, kind) ((int64_t) (layer) * 64 + (int64_t) (kind))

// Public canonical-model C entries (strong generated wrappers in unit-model.c).
int64_t fgc_model_new(void);
int64_t fgc_model_free(int64_t model);
int64_t fgc_model_kind(int64_t model);
int64_t fgc_model_set_int(int64_t model, int64_t key, int64_t value);
int64_t fgc_model_get_int(int64_t model, int64_t key);
int64_t fgc_model_set_float(int64_t model, int64_t key, float value);
float   fgc_model_get_float(int64_t model, int64_t key);
int64_t fgc_model_set_handle(int64_t model, int64_t key, int64_t value);
int64_t fgc_model_hparams_get(int64_t model, int64_t key);
int64_t fgc_model_hparams_set(int64_t model, int64_t key, int64_t value);
int64_t fgc_model_n_layers(int64_t model);
int64_t fgc_model_layer_new(int64_t model, int64_t layer);
int64_t fgc_model_layer_get(int64_t model, int64_t layer);
int64_t fgc_model_layer_set_int(int64_t model, int64_t layer, int64_t key, int64_t value);
int64_t fgc_model_layer_get_int(int64_t model, int64_t layer, int64_t key);
int64_t fgc_model_bind_set(int64_t model, int64_t layer, int64_t kind, int64_t tensor);
int64_t fgc_model_bind_get(int64_t model, int64_t layer, int64_t kind);
int64_t fgc_model_bind_erase(int64_t model, int64_t layer, int64_t kind);
int64_t fgc_model_bind_from_loader(int64_t model, int64_t loader, int64_t layer, int64_t kind);
int64_t fgc_model_vocab_set(int64_t model, int64_t vocab);
int64_t fgc_model_vocab_get(int64_t model);
int64_t fgc_model_vocab_n_tokens(int64_t model);
int64_t fgc_model_validate(int64_t model);

// Cross-schema imports (published contract; implementations arrive from the
// owning workers). These are strong external symbols, never success stubs.
int64_t fgc_vocab_n_tokens(int64_t vocab);
int64_t fgc_loader_get_tensor(int64_t loader, int64_t layer, int64_t kind);
int64_t fgc_loader_handover(int64_t loader, int64_t model); // references only; loader keeps ownership

#ifdef __cplusplus
}
#endif
