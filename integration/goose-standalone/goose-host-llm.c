#define _POSIX_C_SOURCE 200809L   // clock_gettime / CLOCK_MONOTONIC
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
#include "ggml-alloc.h"
#if __has_include("unit.h")
#include "unit.h"
#else
#include "main.h"
#endif
#include "goose-host-llm.h"

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

const char * die(const char * what) { fprintf(stderr, "GENERATE %s\n", what); exit(1); }

static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double) ts.tv_sec + (double) ts.tv_nsec / 1e9;
}

int64_t bits(float f) { int32_t b; memcpy(&b, &f, 4); return b; }

int64_t integer(gguf_context * m, const char * key) {
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

float real_key(gguf_context * m, const char * key) {
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

host_t H;

ggml_tensor * weight(const char * name) {
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

int64_t find_key(const char * key) {
    int64_t n = 0;
    while (key[n]) n++;
    const int64_t hdr = H.size < 67108864 ? (int64_t) H.size : 67108864;
    return gr_find_kv((sl_u8){ H.base, hdr }, (sl_i64){ H.state, 8 }, (sl_i64){ H.kv, 256 * 8 },
                      (sl_i64){ H.kvsl, 512 }, (sl_u8){ (unsigned char *) key, n });
}

void load_vocab(void) {
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

int64_t tokenize(const unsigned char * text, int64_t len, int64_t * ids, int64_t cap) {
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

int64_t detokenize(const int64_t * ids, int64_t n, unsigned char * out, int64_t cap) {
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

void init_trunk(void) {
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
//
// The graph is built with no_alloc and then allocated through ggml_gallocr, which
// reuses buffers by node lifetime. A fixed arena per step could not serve a
// prompt longer than a few hundred tokens: the requirement tracked the arena and
// the failure only moved. Inputs are filled AFTER allocation, because with
// no_alloc the tensors have no data until the allocator places them.
int64_t model_step(const int32_t * tokens, int64_t count, float ** logits) {
    const int64_t rows = q35_kv_read_rows((sl_i64){ H.kv_state, 6 }, count);
    if (rows <= 0) return -1;

    ggml_context * c = ggml_init((struct ggml_init_params){ 256u * 1024u * 1024u, NULL, true });
    if (!c) return -1;

    ggml_tensor * ids = ggml_new_tensor_1d(c, GGML_TYPE_I32, count);
    ggml_tensor * positions = ggml_new_tensor_1d(c, GGML_TYPE_I32, count * 4);
    ggml_tensor * indices = ggml_new_tensor_1d(c, GGML_TYPE_I64, count);
    ggml_tensor * mask = ggml_new_tensor_2d(c, GGML_TYPE_F16, rows, count);

    const int64_t ticket = q35_kv_begin((sl_i64){ H.kv_state, 6 }, count);
    if (ticket <= 0) { ggml_free(c); return -1; }

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
    if (!result) { free(frames); free(roots); ggml_free(c); return -1; }

    ggml_cgraph * graph = ggml_new_graph_custom(c, 16384, false);
    for (int64_t i = 1; i <= roots[0]; ++i) ggml_build_forward_expand(graph, (ggml_tensor *) (intptr_t) roots[i]);

    static ggml_gallocr_t alloc = NULL;
    static ggml_backend_t backend = NULL;
    static int backend_threads = 0;
    if (!alloc) alloc = ggml_gallocr_new(ggml_backend_cpu_buffer_type());
    if (!backend) backend = ggml_backend_cpu_init();
    if (!alloc || !backend) { free(frames); free(roots); ggml_free(c); return -1; }
    if (backend_threads == 0) {
        const char * e = getenv("GOOSE_THREADS");
        backend_threads = e ? atoi(e) : 4;
        if (backend_threads <= 0) backend_threads = 4;
        ggml_backend_cpu_set_n_threads(backend, backend_threads);
    }
    static int64_t logged = 0;
    if (getenv("GOOSE_GRAPH_LOG") && (logged++ < 3 || count != 1)) {
        printf("GRAPH count=%lld nodes=%d used_mem=%.0f MiB alloc_buf=%.0f MiB\n", (long long) count,
               ggml_graph_n_nodes(graph), (double) ggml_used_mem(c) / 1048576.0,
               (double) ggml_gallocr_get_buffer_size(alloc, 0) / 1048576.0);
        fflush(stdout);
    }
    if (!ggml_gallocr_alloc_graph(alloc, graph)) {
        free(frames); free(roots); ggml_free(c); return -1;
    }

    // inputs, now that the allocator has placed them
    memcpy(ids->data, tokens, (size_t) count * 4);
    int64_t * pos = calloc((size_t) count * 4, sizeof(int64_t));
    if (q35_kv_positions((sl_i64){ H.kv_state, 6 }, count, (sl_i64){ pos, count * 4 }) != count * 4) {
        free(pos); free(frames); free(roots); ggml_free(c); return -1;
    }
    for (int s = 0; s < 4; ++s)
        for (int64_t t = 0; t < count; ++t)
            ((int32_t *) positions->data)[s * count + t] = (int32_t) pos[s * count + t];
    if (q35_kv_indices((sl_i64){ H.kv_state, 6 }, count, (sl_i64){ (int64_t *) indices->data, count }) != count) {
        free(pos); free(frames); free(roots); ggml_free(c); return -1;
    }
    float * raw = calloc((size_t) rows * count, sizeof(float));
    if (q35_kv_mask((sl_i64){ H.kv_state, 6 }, count, (sl_f32){ raw, rows * count }) != rows) {
        free(pos); free(raw); free(frames); free(roots); ggml_free(c); return -1;
    }
    for (int64_t t = 0; t < count; ++t)
        for (int64_t k = 0; k < rows; ++k)
            ((ggml_fp16_t *) mask->data)[t * rows + k] = ggml_fp32_to_fp16(raw[t * rows + k]);

    if (ggml_backend_graph_compute(backend, graph) != GGML_STATUS_SUCCESS) {
        free(pos); free(raw); free(frames); free(roots); ggml_free(c); return -1;
    }

    ggml_tensor * lg = (ggml_tensor *) (intptr_t) result;
    const int64_t n = ggml_nelements(lg);
    *logits = malloc((size_t) n * sizeof(float));
    memcpy(*logits, lg->data, (size_t) n * sizeof(float));

    if (q35_kv_commit((sl_i64){ H.kv_state, 6 }, ticket) < 0) return -1;
    free(pos); free(raw); free(frames); free(roots);
    ggml_free(c);
    return n;
}


// ---- API ---------------------------------------------------------------

void hose_host_load(const char * path) {
    struct stat st;
    H.fd = open(path, O_RDONLY);
    if (H.fd < 0 || fstat(H.fd, &st) != 0) die("model open");
    H.size = (size_t) st.st_size;
    H.base = mmap(NULL, H.size, PROT_READ, MAP_PRIVATE, H.fd, 0);
    if (H.base == MAP_FAILED) die("model mmap");
    struct gguf_init_params mp = { true, &H.meta_tensors };
    H.meta = gguf_init_from_file(path, mp);
    if (!H.meta) die("gguf metadata");
    H.weights = ggml_init((struct ggml_init_params){ 8u * 1024u * 1024u, NULL, true });
    if (!H.weights) die("weights ctx");
    load_vocab();
    init_trunk();
}

int64_t hose_tokenize(const unsigned char * text, int64_t len, int64_t * ids, int64_t cap) {
    return tokenize(text, len, ids, cap);
}

int64_t hose_detokenize(const int64_t * ids, int64_t n, unsigned char * out, int64_t cap) {
    return detokenize(ids, n, out, cap);
}

void hose_reset_state(void) { reset_state(); }

int64_t hose_step(const int32_t * tokens, int64_t count, float ** logits) {
    return model_step(tokens, count, logits);
}

int64_t hose_eog_is(int64_t id) {
    return bt_is_eog((sl_i64){ H.eog, H.n_eog }, H.n_eog, id);
}

void hose_stats(int64_t * out5) {
    out5[0] = H.n_tok;
    out5[1] = H.n_special;
    out5[2] = H.n_eog;
    out5[3] = H.eos;
    out5[4] = H.layer_count;
}

// ---- generation loop ---------------------------------------------------
// One request: reset, tokenise, step until the Goose end-of-generation set or
// the token cap, then detokenise. Both the CLI and the server call this, so the
// loop exists once.
int64_t hose_generate(const unsigned char * text, int64_t len, int64_t max_tokens,
                      int64_t * out_ids, unsigned char * out_text, int64_t text_cap,
                      double * timing) {
    if (timing) { timing[0] = 0.0; timing[1] = 0.0; timing[2] = 0.0; }
    static int64_t ids[MAX_REQ];
    const int64_t n_ids = tokenize(text, len, ids, MAX_REQ);
    if (n_ids <= 0) {
        return -1;
    }
    reset_state();

    int32_t * batch = malloc((size_t) n_ids * 4);
    if (!batch) { return -1; }
    for (int64_t i = 0; i < n_ids; ++i) { batch[i] = (int32_t) ids[i]; }

    const double t0 = now_s();
    double t_prefill = 0.0;
    int64_t produced = 0;
    int64_t past = n_ids;
    for (int64_t step = 0; step < max_tokens; ++step) {
        float * logits = NULL;
        const int64_t n_logits = model_step(batch, past, &logits);
        free(batch);
        batch = NULL;
        if (n_logits <= 0) { break; }
        if (step == 0) { t_prefill = now_s() - t0; }
        const int64_t vocab = n_logits / (past > 0 ? past : 1);
        if (vocab <= 0) { free(logits); break; }
        const int64_t next = sm_greedy((sl_f32){ logits + (past - 1) * vocab, vocab }, vocab);
        free(logits);
        if (next < 0) { break; }
        out_ids[produced++] = next;
        if (bt_is_eog((sl_i64){ H.eog, H.n_eog }, H.n_eog, next) != 0) { break; }
        batch = malloc(4);
        if (!batch) { break; }
        batch[0] = (int32_t) next;
        past = 1;
    }
    free(batch);

    const int64_t tlen = produced > 0 ? detokenize(out_ids, produced, out_text, text_cap) : 0;
    if (timing) {
        const double all = now_s() - t0;
        timing[0] = t_prefill;
        timing[1] = all - t_prefill;
        timing[2] = (double) n_ids;
    }
    return produced;
}
