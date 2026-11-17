// Generation inside the standalone runtime: HTTP in, real tokens out.
//
// The unit carries the decisions (trunk plan, attention/GDN branches, MoE
// routing, KV bookkeeping, sampling, detokenisation). This host binds the real
// quantised weights by offset and owns the buffers, exactly as the gate does;
// it chooses no layer, no branch and no token.
//
// Substrate kept native and audited (P10-12 G8): the GGUF container walk
// (gguf_init_from_file / gguf_find_tensor / tensor offsets) and the ggml CPU
// compute call. Everything above them is Goose.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#include "gguf.h"
#include "ggml.h"
#include "ggml-cpu.h"
#include "unit.h"

// gguf.h declares struct tags only; C has no implicit typedef.
typedef struct gguf_context gguf_context;
typedef struct ggml_context ggml_context;
typedef struct ggml_tensor  ggml_tensor;
typedef struct ggml_cgraph  ggml_cgraph;
typedef enum gguf_type gguf_type;
typedef enum ggml_type ggml_type;

#define MAX_REQ   262144
#define MAX_IDS   65536
#define KV_CAP    2048
#define MAX_TOKENS 256

static const char * die(const char * what) { fprintf(stderr, "GENERATE %s\n", what); exit(1); }

static int64_t bits(float f) { int32_t b; memcpy(&b, &f, 4); return b; }

static int64_t integer(gguf_context * m, const char * key) {
    const int64_t k = gguf_find_key(m, key);
    if (k < 0) die(key);
    const gguf_type t = gguf_get_kv_type(m, k);
    if (t == GGUF_TYPE_UINT32) return (int64_t) gguf_get_val_u32(m, k);
    if (t == GGUF_TYPE_INT32)  return (int64_t) gguf_get_val_i32(m, k);
    if (t == GGUF_TYPE_UINT64) return (int64_t) gguf_get_val_u64(m, k);
    if (t == GGUF_TYPE_INT64)  return gguf_get_val_i64(m, k);
    die("integer key type");
    return 0;
}

static float real_key(gguf_context * m, const char * key) {
    const int64_t k = gguf_find_key(m, key);
    if (k < 0) die(key);
    return gguf_get_val_f32(m, k);
}

typedef struct {
    int fd;
    uint8_t * base;
    size_t size;
    gguf_context * meta;
    ggml_context * meta_tensors;
    ggml_context * weights;
    int bindings;
    // Goose vocabulary tables
    int64_t state[8];
    int64_t * kv, * kvsl, * tens, * tensl;
    uint8_t * blob;
    int64_t * tok_off, * tok_len, * tok_slots, * mrg_off, * mrg_len, * mrg_slots, * attr, * order;
    int64_t n_tok, n_mrg, n_special, n_tok_slots, n_mrg_slots, blob_len;
    int64_t * eog;
    int64_t n_eog, eos;
    // trunk state
    int64_t layer_count, hidden, acfg[14], gcfg[7], mcfg[3], kv_state[6];
    int sections[4];
    int64_t * records;
    ggml_tensor * emb, * head, * norm;
    // Per-request state that must start at zero: GDN conv/state and the KV cells.
    ggml_tensor ** reset_tensors;
    int64_t reset_count;
    int64_t * scratch_cpts, * scratch_seg, * scratch_sym, * scratch_heap, * scratch_hc, * scratch_info;
    uint8_t * scratch_enc;
} host_t;

static host_t H;

static ggml_tensor * weight(const char * name) {
    const int64_t id = gguf_find_tensor(H.meta, name);
    if (id < 0) die(name);
    const int64_t * ne = gguf_get_tensor_ne(H.meta, id);
    const ggml_type ty = gguf_get_tensor_type(H.meta, id);
    const uint64_t offset = gguf_get_data_offset(H.meta) + gguf_get_tensor_offset(H.meta, id);
    int64_t shape[4];
    memcpy(shape, ne, sizeof shape);
    const int64_t t = q35_bind_weight((int64_t) (intptr_t) H.weights, (uint64_t) (uintptr_t) H.base, (int64_t) H.size,
                                      (int64_t) offset, (int64_t) ty, (sl_i64){ shape, 4 });
    if (!t) die(name);
    H.bindings++;
    return (ggml_tensor *) (intptr_t) t;
}

static int64_t find_key(const char * key) {
    int64_t n = 0;
    while (key[n]) n++;
    const int64_t hdr = H.size < 67108864 ? (int64_t) H.size : 67108864;
    return gr_find_kv((sl_u8){ H.base, hdr }, (sl_i64){ H.state, 8 }, (sl_i64){ H.kv, 256 * 8 },
                      (sl_i64){ H.kvsl, 512 }, (sl_u8){ (unsigned char *) key, n });
}

static void load_vocab(void) {
    const int64_t hdr = H.size < 67108864 ? (int64_t) H.size : 67108864;
    H.kv = calloc(256 * 8, sizeof(int64_t));
    H.kvsl = calloc(512, sizeof(int64_t));
    H.tens = calloc(2048 * 12, sizeof(int64_t));
    H.tensl = calloc(8192, sizeof(int64_t));
    if (gr_parse((sl_u8){ H.base, hdr }, (int64_t) H.size, (sl_i64){ H.state, 8 },
                 (sl_i64){ H.kv, 256 * 8 }, (sl_i64){ H.tens, 2048 * 12 },
                 (sl_i64){ H.kvsl, 512 }, (sl_i64){ H.tensl, 8192 }) != 0) {
        die("reader");
    }
    const int64_t ti = find_key("tokenizer.ggml.tokens");
    const int64_t mi = find_key("tokenizer.ggml.merges");
    const int64_t yi = find_key("tokenizer.ggml.token_type");
    if (ti < 0 || mi < 0 || yi < 0) die("tokenizer keys");

    int64_t need[5] = {0};
    if (bt_prepare_need((sl_u8){ H.base, hdr }, (sl_i64){ H.state, 8 }, (sl_i64){ H.kv, 256 * 8 },
                        ti, mi, (sl_i64){ need, 5 }) != 0) die("prepare_need");
    H.n_tok = need[1];
    H.n_mrg = need[2];
    H.n_tok_slots = need[3];
    H.n_mrg_slots = need[4];
    H.blob_len = need[0];
    H.blob = malloc((size_t) H.blob_len);
    H.tok_off = calloc((size_t) H.n_tok, sizeof(int64_t));
    H.tok_len = calloc((size_t) H.n_tok, sizeof(int64_t));
    H.tok_slots = calloc((size_t) H.n_tok_slots, sizeof(int64_t));
    H.mrg_off = calloc((size_t) H.n_mrg, sizeof(int64_t));
    H.mrg_len = calloc((size_t) H.n_mrg, sizeof(int64_t));
    H.mrg_slots = calloc((size_t) H.n_mrg_slots, sizeof(int64_t));
    if (bt_prepare_from_gguf((sl_u8){ H.base, hdr }, (sl_i64){ H.state, 8 }, (sl_i64){ H.kv, 256 * 8 }, ti, mi,
                             (sl_u8){ H.blob, H.blob_len },
                             (sl_i64){ H.tok_off, H.n_tok }, (sl_i64){ H.tok_len, H.n_tok },
                             (sl_i64){ H.tok_slots, H.n_tok_slots },
                             (sl_i64){ H.mrg_off, H.n_mrg }, (sl_i64){ H.mrg_len, H.n_mrg },
                             (sl_i64){ H.mrg_slots, H.n_mrg_slots }, (sl_i64){ need, 5 }) != 0) die("prepare");

    H.attr = calloc((size_t) H.n_tok, sizeof(int64_t));
    int64_t * ttype = calloc((size_t) H.n_tok, sizeof(int64_t));
    int64_t span[4] = {0};
    if (gr_val_span((sl_i64){ H.state, 8 }, (sl_i64){ H.kv, 256 * 8 }, yi, (sl_i64){ span, 4 }) != 0) die("token_type span");
    const int32_t * tt = (const int32_t *) (H.base + span[1]);
    for (int64_t i = 0; i < H.n_tok; ++i) ttype[i] = tt[i];
    if (bt_attrs((sl_u8){ H.blob, H.blob_len }, (sl_i64){ H.tok_off, H.n_tok }, (sl_i64){ H.tok_len, H.n_tok },
                 (sl_i64){ ttype, H.n_tok }, (sl_i64){ H.attr, H.n_tok }) != 0) die("attrs");
    int64_t stats[4] = {0};
    if (bt_build((sl_u8){ H.blob, H.blob_len }, (sl_i64){ H.tok_off, H.n_tok }, (sl_i64){ H.tok_len, H.n_tok },
                 (sl_i64){ H.tok_slots, H.n_tok_slots }, (sl_i64){ H.mrg_off, H.n_mrg }, (sl_i64){ H.mrg_len, H.n_mrg },
                 (sl_i64){ H.mrg_slots, H.n_mrg_slots }, (sl_i64){ stats, 4 }) != 0) die("bt_build");
    H.order = calloc((size_t) H.n_tok, sizeof(int64_t));
    H.n_special = bt_special_order((sl_u8){ H.blob, H.blob_len }, (sl_i64){ H.tok_off, H.n_tok },
                                   (sl_i64){ H.tok_len, H.n_tok }, (sl_i64){ H.attr, H.n_tok },
                                   (sl_i64){ H.order, H.n_tok });
    if (H.n_special < 0) H.n_special = 0;

    // End-of-generation set, decided by Goose from the token texts.
    H.eos = -1;
    {
        const int64_t ei = find_key("tokenizer.ggml.eos_token_id");
        if (ei >= 0) {
            int64_t one = 0;
            gr_meta_int((sl_u8){ H.base, hdr }, (sl_i64){ H.state, 8 }, (sl_i64){ H.kv, 256 * 8 },
                        ei, (sl_i64){ &one, 1 });
            H.eos = one;
        }
    }
    H.eog = calloc((size_t) H.n_tok, sizeof(int64_t));
    H.n_eog = bt_eog_ids((sl_u8){ H.blob, H.blob_len }, (sl_i64){ H.tok_off, H.n_tok },
                         (sl_i64){ H.tok_len, H.n_tok }, H.eos, -1, -1, (sl_i64){ H.eog, H.n_tok });
    if (H.n_eog < 0) H.n_eog = 0;

    const int64_t cap = MAX_REQ + 1;
    H.scratch_cpts = calloc((size_t) cap, sizeof(int64_t));
    H.scratch_seg = calloc(4 * (size_t) cap, sizeof(int64_t));
    H.scratch_enc = calloc(8 * (size_t) cap, 1);
    H.scratch_sym = calloc(24 * (size_t) cap, sizeof(int64_t));
    H.scratch_heap = calloc(15 * (size_t) cap + 40, sizeof(int64_t));
    H.scratch_hc = calloc(2, sizeof(int64_t));
    H.scratch_info = calloc(8, sizeof(int64_t));
}

static int64_t tokenize(const unsigned char * text, int64_t len, int64_t * ids, int64_t cap) {
    return bt_tokenize((sl_u8){ H.blob, H.blob_len },
                       (sl_i64){ H.tok_off, H.n_tok_slots }, (sl_i64){ H.tok_len, H.n_tok_slots },
                       (sl_i64){ H.tok_slots, H.n_tok_slots },
                       (sl_i64){ H.mrg_off, H.n_mrg_slots }, (sl_i64){ H.mrg_len, H.n_mrg_slots },
                       (sl_i64){ H.mrg_slots, H.n_mrg_slots },
                       (sl_i64){ H.order, H.n_tok }, H.n_special,
                       (sl_u8){ (unsigned char *) text, len }, (sl_i64){ ids, cap }, 1,
                       (sl_i64){ H.scratch_cpts, MAX_REQ + 1 }, (sl_i64){ H.scratch_seg, 4 * (MAX_REQ + 1) },
                       (sl_u8){ H.scratch_enc, 8 * (MAX_REQ + 1) },
                       (sl_i64){ H.scratch_sym, 24 * (MAX_REQ + 1) },
                       (sl_i64){ H.scratch_heap, 15 * (MAX_REQ + 1) + 40 },
                       (sl_i64){ H.scratch_hc, 2 }, (sl_i64){ H.scratch_info, 8 });
}

static int64_t detokenize(const int64_t * ids, int64_t n, unsigned char * out, int64_t cap) {
    int64_t at = 0;
    for (int64_t i = 0; i < n; ++i) {
        if (ids[i] < 0 || ids[i] >= H.n_tok || at >= cap) return -1;
        const int64_t got = bt_token_to_piece((sl_u8){ H.blob, H.blob_len },
                                             (sl_i64){ H.tok_off, H.n_tok }, (sl_i64){ H.tok_len, H.n_tok },
                                             (sl_i64){ H.attr, H.n_tok }, ids[i],
                                             (sl_u8){ out + at, cap - at }, 0, 0, 1, 0);
        if (got < 0) return -1;
        at += got;
    }
    return at;
}

static void init_trunk(void) {
    H.layer_count = integer(H.meta, "qwen35moe.block_count") - integer(H.meta, "qwen35moe.nextn_predict_layers");
    if (H.layer_count < 1 || H.layer_count > 41) die("layer count");
    H.hidden = integer(H.meta, "qwen35moe.embedding_length");

    const int64_t h  = integer(H.meta, "qwen35moe.ssm.state_size");
    const int64_t nk = integer(H.meta, "qwen35moe.ssm.group_count");
    const int64_t nv = integer(H.meta, "qwen35moe.ssm.time_step_rank");
    const int64_t kernel = integer(H.meta, "qwen35moe.ssm.conv_kernel");
    const int64_t heads = integer(H.meta, "qwen35moe.attention.head_count");
    const int64_t hk = integer(H.meta, "qwen35moe.attention.head_count_kv");
    const int64_t hd = integer(H.meta, "qwen35moe.attention.key_length");
    const float eps = real_key(H.meta, "qwen35moe.attention.layer_norm_rms_epsilon");
    if (q35_gdn_config(h, nk, nv, kernel, bits(eps), (sl_i64){ H.gcfg, 7 }) != 7) die("gdn config");

    int64_t a[14] = { heads, hk, integer(H.meta, "qwen35moe.rope.dimension_count"), GGML_ROPE_TYPE_IMROPE,
                      integer(H.meta, "qwen35moe.context_length"), bits(eps), bits(real_key(H.meta, "qwen35moe.rope.freq_base")),
                      bits(1.0f), bits(0.0f), bits(1.0f), bits(32.0f), bits(1.0f), bits(1.0f),
                      GGML_PREC_F32 };
    a[12] = bits(1.0f / sqrtf((float) hd));
    memcpy(H.acfg, a, sizeof a);
    H.mcfg[0] = integer(H.meta, "qwen35moe.expert_count");
    H.mcfg[1] = integer(H.meta, "qwen35moe.expert_used_count");
    H.mcfg[2] = bits(0.0f);

    const int64_t sk = gguf_find_key(H.meta, "qwen35moe.rope.dimension_sections");
    if (sk < 0 || gguf_get_arr_n(H.meta, sk) != 4) die("rope sections");
    memcpy(H.sections, gguf_get_arr_data(H.meta, sk), sizeof H.sections);
    const int64_t fk = gguf_find_key(H.meta, "qwen35moe.attention.recurrent_layers");
    if (fk < 0 || gguf_get_arr_type(H.meta, fk) != GGUF_TYPE_BOOL) die("layer flags");
    const uint8_t * flags = (const uint8_t *) gguf_get_arr_data(H.meta, fk);

    H.records = calloc((size_t) H.layer_count * 32, sizeof(int64_t));
    H.reset_tensors = calloc((size_t) H.layer_count * 2, sizeof(ggml_tensor *));
    H.reset_count = 0;
    static const char * recurrent[11] = { "attn_norm.weight", "attn_qkv.weight", "attn_gate.weight", "ssm_beta.weight",
                                          "ssm_alpha.weight", "ssm_dt.bias", "ssm_a", "ssm_conv1d.weight",
                                          "ssm_norm.weight", "ssm_out.weight", "post_attention_norm.weight" };
    static const char * attention[8] = { "attn_norm.weight", "attn_q.weight", "attn_k.weight", "attn_v.weight",
                                         "attn_output.weight", "attn_q_norm.weight", "attn_k_norm.weight",
                                         "post_attention_norm.weight" };
    static const char * moe_names[8] = { "ffn_gate_inp.weight", "ffn_up_exps.weight", "ffn_gate_exps.weight",
                                         "ffn_down_exps.weight", "ffn_up_shexp.weight", "ffn_gate_shexp.weight",
                                         "ffn_down_shexp.weight", "ffn_gate_inp_shexp.weight" };

    ggml_context * owner = ggml_init((struct ggml_init_params){ 192u * 1024u * 1024u, NULL, false });
    if (!owner) die("state arena");

    char name[128];
    for (int64_t i = 0; i < H.layer_count; ++i) {
        int64_t * row = H.records + 32 * i;
        row[0] = flags[i] ? 1 : 0;
        const int64_t nb = flags[i] ? 11 : 8;
        for (int64_t j = 0; j < nb; ++j) {
            snprintf(name, sizeof name, "blk.%lld.%s", (long long) i, flags[i] ? recurrent[j] : attention[j]);
            row[1 + j] = (int64_t) (intptr_t) weight(name);
        }
        for (int64_t j = 0; j < 8; ++j) {
            snprintf(name, sizeof name, "blk.%lld.%s", (long long) i, moe_names[j]);
            row[12 + j] = (int64_t) (intptr_t) weight(name);
        }
        if (flags[i]) {
            ggml_tensor * conv  = ggml_new_tensor_2d(owner, GGML_TYPE_F32, kernel - 1, h * (2 * nk + nv));
            ggml_tensor * state = ggml_new_tensor_4d(owner, GGML_TYPE_F32, h, h, nv, 1);
            memset(conv->data, 0, ggml_nbytes(conv));
            memset(state->data, 0, ggml_nbytes(state));
            row[20] = (int64_t) (intptr_t) conv;
            row[21] = (int64_t) (intptr_t) state;
            H.reset_tensors[H.reset_count++] = conv;
            H.reset_tensors[H.reset_count++] = state;
        } else {
            ggml_tensor * kc = ggml_new_tensor_2d(owner, GGML_TYPE_Q4_0, hd * hk, KV_CAP);
            ggml_tensor * vc = ggml_new_tensor_2d(owner, GGML_TYPE_Q4_0, hd * hk, KV_CAP);
            memset(kc->data, 0, ggml_nbytes(kc));
            memset(vc->data, 0, ggml_nbytes(vc));
            row[22] = (int64_t) (intptr_t) kc;
            row[23] = (int64_t) (intptr_t) vc;
            H.reset_tensors[H.reset_count++] = kc;
            H.reset_tensors[H.reset_count++] = vc;
        }
    }
    H.emb = weight("token_embd.weight");
    H.head = gguf_find_tensor(H.meta, "output.weight") < 0 ? H.emb : weight("output.weight");
    H.norm = weight("output_norm.weight");
    if (q35_kv_init(KV_CAP, 32, (sl_i64){ H.kv_state, 6 }) != 0) die("kv init");
}

// Start a request from zero: KV bookkeeping plus every recurrent/KV cell buffer.
static void reset_state(void) {
    if (q35_kv_init(KV_CAP, 32, (sl_i64){ H.kv_state, 6 }) != 0) die("kv reset");
    for (int64_t i = 0; i < H.reset_count; ++i) {
        memset(H.reset_tensors[i]->data, 0, ggml_nbytes(H.reset_tensors[i]));
    }
}

// One full-model step: prompt ids in, logits out. Returns the logit count.
static int64_t model_step(const int32_t * tokens, int64_t count, float ** logits) {
    const int64_t rows = q35_kv_read_rows((sl_i64){ H.kv_state, 6 }, count);
    if (rows <= 0) return -1;
    // The graph arena holds every node's data, so it scales with the prompt.
    // A fixed 1 GiB was enough for a five-token smoke prompt and crashed on a
    // normal one ("not enough space in the context's memory pool").
    size_t arena = 1280u * 1024u * 1024u + (size_t) count * 6u * 1024u * 1024u;
    const char * arena_env = getenv("GOOSE_ARENA_MIB");
    if (arena_env) arena = (size_t) atoll(arena_env) * 1024u * 1024u;
    ggml_context * c = ggml_init((struct ggml_init_params){ arena, NULL, false });
    if (!c) { printf("GRAPH arena=%zu failed\n", arena); fflush(stdout); return -1; }
    if (getenv("GOOSE_GRAPH_LOG")) { printf("GRAPH count=%lld arena=%zu MiB\n", (long long) count, arena / (1024u * 1024u)); fflush(stdout); }

    ggml_tensor * ids = ggml_new_tensor_1d(c, GGML_TYPE_I32, count);
    memcpy(ids->data, tokens, (size_t) count * 4);

    ggml_tensor * positions = ggml_new_tensor_1d(c, GGML_TYPE_I32, count * 4);
    int64_t * pos = calloc((size_t) count * 4, sizeof(int64_t));
    if (q35_kv_positions((sl_i64){ H.kv_state, 6 }, count, (sl_i64){ pos, count * 4 }) != count * 4) return -1;
    for (int s = 0; s < 4; ++s)
        for (int64_t t = 0; t < count; ++t) ((int32_t *) positions->data)[s * count + t] = (int32_t) pos[s * count + t];

    ggml_tensor * indices = ggml_new_tensor_1d(c, GGML_TYPE_I64, count);
    if (q35_kv_indices((sl_i64){ H.kv_state, 6 }, count, (sl_i64){ (int64_t *) indices->data, count }) != count) return -1;

    ggml_tensor * mask = ggml_new_tensor_2d(c, GGML_TYPE_F16, rows, count);
    float * raw = calloc((size_t) rows * count, sizeof(float));
    if (q35_kv_mask((sl_i64){ H.kv_state, 6 }, count, (sl_f32){ raw, rows * count }) != rows) return -1;
    for (int64_t t = 0; t < count; ++t)
        for (int64_t k = 0; k < rows; ++k)
            ((ggml_fp16_t *) mask->data)[t * rows + k] = ggml_fp32_to_fp16(raw[t * rows + k]);

    const int64_t ticket = q35_kv_begin((sl_i64){ H.kv_state, 6 }, count);
    if (ticket <= 0) return -1;

    int64_t work[16] = {0}, bn[40] = {0}, mn[42] = {0};
    int64_t * frames = calloc((size_t) (4 * H.layer_count + 4), sizeof(int64_t));
    int64_t * roots  = calloc((size_t) (2 * H.layer_count + 2), sizeof(int64_t));

    const int64_t result = q35_trunk((int64_t) (intptr_t) c, (int64_t) (intptr_t) ids,
                                     (int64_t) (intptr_t) H.emb, (int64_t) (intptr_t) H.head, (int64_t) (intptr_t) H.norm,
                                     (sl_i64){ H.records, H.layer_count * 32 }, H.layer_count,
                                     (int64_t) (intptr_t) positions, (int64_t) (intptr_t) mask, (int64_t) (intptr_t) indices,
                                     (sl_i64){ H.acfg, 14 }, (sl_i64){ H.gcfg, 7 }, (sl_i64){ H.mcfg, 3 },
                                     (int64_t) (intptr_t) H.sections, (sl_i64){ H.kv_state, 6 }, ticket,
                                     (sl_i64){ work, 16 }, (sl_i64){ bn, 40 }, (sl_i64){ mn, 42 },
                                     (sl_i64){ frames, 4 * H.layer_count + 4 }, (sl_i64){ roots, 2 * H.layer_count + 2 });
    if (!result) return -1;

    ggml_cgraph * graph = ggml_new_graph_custom(c, 16384, false);
    for (int64_t i = 1; i <= roots[0]; ++i) ggml_build_forward_expand(graph, (ggml_tensor *) (intptr_t) roots[i]);
    static int threads = 0;
    if (threads == 0) {
        const char * e = getenv("GOOSE_THREADS");
        threads = e ? atoi(e) : 4;
        if (threads <= 0) threads = 4;
    }
    if (ggml_graph_compute_with_ctx(c, graph, threads) != GGML_STATUS_SUCCESS) return -1;

    ggml_tensor * lg = (ggml_tensor *) (intptr_t) result;
    const int64_t n = ggml_nelements(lg);
    *logits = malloc((size_t) n * sizeof(float));
    memcpy(*logits, lg->data, (size_t) n * sizeof(float));

    if (q35_kv_commit((sl_i64){ H.kv_state, 6 }, ticket) < 0) return -1;
    free(pos); free(raw); free(frames); free(roots);
    ggml_free(c);
    return n;
}

// ---- generation over the listener ----------------------------------------

static int64_t g_served = 0;

static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double) ts.tv_sec + (double) ts.tv_nsec / 1e9;
}

int64_t generation_serve(int64_t conn) {
    static unsigned char req[MAX_REQ], head[512], body[MAX_REQ * 2], path[1024];
    static int64_t ids[MAX_IDS], out_ids[MAX_TOKENS];

    int64_t info[4] = {0, 0, 0, 0};
    const int64_t n = ht_read_request(conn, (sl_u8){ req, MAX_REQ }, (sl_i64){ info, 4 });
    if (n <= 0) return -1;
    const int64_t method = info[1], poff = info[2], plen = info[3];
    if (poff < 0 || plen < 0 || poff + plen > (int64_t) sizeof(path)) return -1;
    memcpy(path, req + poff, (size_t) plen);

    const int64_t route = route_id(method, (sl_u8){ path, plen });
    const int64_t keep = ht_keep_alive((sl_u8){ req, n });
    int64_t blen = 0, status = 200;

    const int is_completion = (plen == 11 && memcmp(path, "/completion", 11) == 0 && method == 2);
    if (is_completion) {
        // Requests are independent: the KV bookkeeping starts from zero for each
        // one, so a second prompt never sees the first prompt's positions.
        reset_state();
        const int64_t cnt = tokenize(req + info[0], n - info[0], ids, MAX_IDS);
        if (cnt <= 0) {
            blen = (int64_t) sprintf((char *) body, "{\"error\":\"tokenize failed\"}");
            status = 400;
        } else {
            float * logits = NULL;
            int64_t n_logits = 0, produced = 0;
            int32_t * batch = malloc((size_t) cnt * 4);
            for (int64_t i = 0; i < cnt; ++i) batch[i] = (int32_t) ids[i];

            int64_t past = cnt;
            const double t0 = now_s();
            double t_prefill = 0.0;
            for (int64_t step = 0; step < MAX_TOKENS; ++step) {
                n_logits = model_step(batch, past, &logits);
                free(batch);
                if (n_logits <= 0) { printf("STEP failed at %lld past=%lld\n", (long long) step, (long long) past); fflush(stdout); break; }
                if (step == 0) t_prefill = now_s() - t0;
                // logits are [n_vocab, n_tokens]; the next token comes from the
                // last position's row, and sm_greedy takes the count as its
                // second argument.
                const int64_t vocab = n_logits / (past > 0 ? past : 1);
                if (vocab <= 0) break;
                const float * row = logits + (int64_t) (past - 1) * vocab;
                const int64_t next = sm_greedy((sl_f32){ (float *) row, vocab }, vocab);
                free(logits);
                logits = NULL;
                if (next < 0) { printf("SAMPLE failed\n"); fflush(stdout); break; }
                out_ids[produced++] = next;
                if (bt_is_eog((sl_i64){ H.eog, H.n_eog }, H.n_eog, next) != 0) break;
                batch = malloc(4);
                batch[0] = (int32_t) next;
                past = 1;
            }

            const double t_all = now_s() - t0;
            const double t_dec = t_all - t_prefill;
            printf("TIME prompt=%lld prefill_s=%.2f prefill_tok_s=%.1f decode_n=%lld decode_s=%.2f decode_tok_s=%.2f\n",
                   (long long) cnt, t_prefill, cnt > 0 ? (double) cnt / (t_prefill > 0 ? t_prefill : 1e-9) : 0.0,
                   (long long) (produced > 0 ? produced - 1 : 0), t_dec,
                   (produced > 1 && t_dec > 0) ? (double) (produced - 1) / t_dec : 0.0);
            fflush(stdout);

            unsigned char text[MAX_REQ];
            const int64_t tlen = produced ? detokenize(out_ids, produced, text, sizeof text) : 0;
            int64_t at = (int64_t) sprintf((char *) body, "{\"ids\":[");
            for (int64_t i = 0; i < produced && at < (int64_t) sizeof(body) - 40; ++i)
                at += (int64_t) sprintf((char *) body + at, i ? ",%lld" : "%lld", (long long) out_ids[i]);
            at += (int64_t) sprintf((char *) body + at, "],\"text\":\"");
            for (int64_t i = 0; i < tlen && at < (int64_t) sizeof(body) - 12; ++i) {
                const unsigned char ch = text[i];
                if (ch == '"' || ch == '\\') { body[at++] = '\\'; body[at++] = ch; }
                else if (ch == 10) { body[at++] = '\\'; body[at++] = 'n'; }
                else if (ch == 13) { body[at++] = '\\'; body[at++] = 'r'; }
                else if (ch == 9)  { body[at++] = '\\'; body[at++] = 't'; }
                else if (ch < 32)  { at += (int64_t) sprintf((char *) body + at, "\\u%04x", ch); }
                else body[at++] = ch;
            }
            at += (int64_t) sprintf((char *) body + at, "\",\"count\":%lld}", (long long) produced);
            blen = at;
        }
    } else if (plen == 6 && memcmp(path, "/props", 6) == 0) {
        blen = (int64_t) sprintf((char *) body,
            "{\"n_vocab\":%lld,\"n_special\":%lld,\"n_eog\":%lld,\"n_layer\":%lld,\"n_embd\":%lld,\"bindings\":%d}",
            (long long) H.n_tok, (long long) H.n_special, (long long) H.n_eog,
            (long long) H.layer_count, (long long) H.hidden, H.bindings);
    } else if (route > 0) {
        blen = (int64_t) sprintf((char *) body, "{\"route\":%lld}", (long long) route);
    } else {
        status = 404;
        blen = (int64_t) sprintf((char *) body, "{\"error\":{\"code\":404}}");
    }

respond:
    {
    const int64_t hl = ht_response_head(status, blen, keep, (sl_u8){ head, sizeof(head) });
    if (hl < 0) return -1;
    if (ht_write_all(conn, (sl_u8){ head, hl }) != 0) return -1;
    if (ht_write_all(conn, (sl_u8){ body, blen }) != 0) return -1;
    printf("REQ method=%lld path=%.*s status=%lld\n", (long long) method, (int) plen, path, (long long) status);
    return keep ? 0 : 1;
    }
}

int main(int argc, char ** argv) {
    goose_init(0, NULL);
    if (argc < 3) {
        fprintf(stderr, "usage: %s <model.gguf> <port> [max_requests]\n", argv[0]);
        return 2;
    }
    struct stat st;
    H.fd = open(argv[1], O_RDONLY);
    if (H.fd < 0 || fstat(H.fd, &st) != 0) die("model open");
    H.size = (size_t) st.st_size;
    H.base = mmap(NULL, H.size, PROT_READ, MAP_PRIVATE, H.fd, 0);
    if (H.base == MAP_FAILED) die("model mmap");

    struct gguf_init_params mp = { true, &H.meta_tensors };
    H.meta = gguf_init_from_file(argv[1], mp);
    if (!H.meta) die("gguf metadata");
    H.weights = ggml_init((struct ggml_init_params){ 8u * 1024u * 1024u, NULL, true });
    if (!H.weights) die("weights ctx");

    load_vocab();
    init_trunk();
    printf("SERVER vocab=%lld special=%lld eog=%lld eos=%lld layers=%lld bindings=%d\n",
           (long long) H.n_tok, (long long) H.n_special, (long long) H.n_eog, (long long) H.eos,
           (long long) H.layer_count, H.bindings);
    fflush(stdout);

    const int64_t max_req = argc > 3 ? atoll(argv[3]) : 1;
    int64_t opts[1] = { 0 };
    const int64_t lfd = ht_open(atoll(argv[2]), (sl_i64){ opts, 1 });
    if (lfd < 0) { printf("SERVER open failed step=%lld\n", (long long) (-lfd)); return 1; }
    printf("SERVER listening fd=%lld\n", (long long) lfd);
    fflush(stdout);

    static unsigned char addr[16];
    int64_t alen[1] = { 16 }, ainfo[2] = { 0, 0 };
    while (g_served < max_req) {
        const int64_t conn = ht_accept(lfd, (sl_u8){ addr, 16 }, (sl_i64){ alen, 1 }, (sl_i64){ ainfo, 2 });
        if (conn < 0) break;
        g_served++;
        generation_serve(conn);
        ht_close(conn);
    }
    ht_close(lfd);
    printf("SERVER done served=%lld\n", (long long) g_served);
    return 0;
}
