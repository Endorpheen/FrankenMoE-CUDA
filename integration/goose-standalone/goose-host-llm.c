#define _POSIX_C_SOURCE 200809L   // clock_gettime / CLOCK_MONOTONIC
// Generation inside the standalone runtime: HTTP in, real tokens out.
//
// The unit carries the decisions (trunk plan, attention/GDN branches, MoE
// routing, KV bookkeeping, sampling, detokenisation). This host binds the real
// quantised weights by offset and owns the buffers, exactly as the gate does;
// it chooses no layer, no branch and no token.
//
// The Goose reader and model owner bind GGUF tensors. The host maps the file
// and passes graph execution to the CPU backend.

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

#include "ggml.h"
#include "graph-execute.h"
#if __has_include("unit.h")
#include "unit.h"
#else
#include "main.h"
#endif
#include "goose-host-llm.h"

typedef struct ggml_context ggml_context;
typedef struct ggml_tensor  ggml_tensor;
typedef struct ggml_cgraph  ggml_cgraph;
typedef enum ggml_type ggml_type;

#define MAX_REQ   262144

const char * die(const char * what) { fprintf(stderr, "GENERATE %s\n", what); exit(1); }

double fg_q35_generation_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double) ts.tv_sec + (double) ts.tv_nsec / 1e9;
}

typedef struct {
    int fd;
    uint8_t * base;
    size_t size;
    int64_t owner[12];
    // Goose vocabulary tables
    int64_t state[8];
    int64_t * kv, * kvsl, * tens, * tensl;
    uint8_t * blob;
    int64_t * tok_off, * tok_len, * tok_slots, * mrg_off, * mrg_len, * mrg_slots, * attr, * order;
    int64_t n_tok, n_mrg, n_special, n_tok_slots, n_mrg_slots, blob_len;
    int64_t * eog;
    int64_t n_eog, eos;
    // trunk state
    int64_t layer_count, acfg[14], gcfg[7], mcfg[3], kv_state[6];
    int64_t rope_sections;
    int64_t * records;
    ggml_tensor * emb, * head, * norm;
    // Per-request state that must start at zero: GDN conv/state and the KV cells.
    ggml_tensor ** reset_tensors;
    int64_t reset_count;
    int64_t * scratch_cpts, * scratch_seg, * scratch_sym, * scratch_heap, * scratch_hc, * scratch_info;
    uint8_t * scratch_enc;
} host_t;

host_t H;

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
    if (!H.attr || !ttype) die("token type workspaces");
    const sl_i64 state = { H.state, 8 };
    const sl_i64 kv = { H.kv, 256 * 8 };
    if (gr_arr_n(state, kv, yi) != H.n_tok) die("token type count");
    for (int64_t i = 0; i < H.n_tok; ++i) {
        if (gr_array_int((sl_u8){ H.base, hdr }, state, kv, yi, i, (sl_i64){ &ttype[i], 1 }) != 0) {
            die("token type value");
        }
    }
    if (bt_attrs((sl_u8){ H.blob, H.blob_len }, (sl_i64){ H.tok_off, H.n_tok }, (sl_i64){ H.tok_len, H.n_tok },
                 (sl_i64){ ttype, H.n_tok }, (sl_i64){ H.attr, H.n_tok }) != 0) die("attrs");
    free(ttype);
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
                       (sl_i64){ H.tok_off, H.n_tok }, (sl_i64){ H.tok_len, H.n_tok },
                       (sl_i64){ H.tok_slots, H.n_tok_slots },
                       (sl_i64){ H.mrg_off, H.n_mrg }, (sl_i64){ H.mrg_len, H.n_mrg },
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

static void init_trunk(int64_t capacity) {
    const int64_t hdr = H.size < 67108864 ? (int64_t) H.size : 67108864;
    const int64_t layer_slots = 128 * 32;
    const int64_t scratch_slots = H.state[3] + 4;
    H.records = calloc((size_t) layer_slots, sizeof(int64_t));
    int64_t *scratch = calloc((size_t) scratch_slots, sizeof(int64_t));
    if (!H.records || !scratch) die("model loader workspaces");
    int64_t handles[4] = {0};
    int64_t sections[4] = {0};
    uint8_t name[128] = {0};
    H.layer_count = qr_model_open((sl_u8){ H.base, hdr }, (uint64_t) (uintptr_t) H.base,
        (int64_t) H.size, (sl_i64){ H.state, 8 }, (sl_i64){ H.kv, 256 * 8 },
        (sl_i64){ H.tens, 2048 * 12 }, (sl_i64){ H.kvsl, 512 }, (sl_i64){ H.tensl, 8192 },
        capacity, INT32_MAX, (sl_i64){ H.owner, 12 }, (sl_i64){ H.records, layer_slots },
        (sl_i64){ handles, 4 }, (sl_i64){ H.acfg, 14 }, (sl_i64){ H.gcfg, 7 },
        (sl_i64){ H.mcfg, 3 }, (sl_i64){ sections, 4 }, (sl_i64){ H.kv_state, 6 },
        (sl_u8){ name, sizeof name }, (sl_i64){ scratch, scratch_slots });
    free(scratch);
    if (H.layer_count <= 0) {
        fprintf(stderr, "Goose model loader returned %lld\n", (long long) H.layer_count);
        die("model loader");
    }
    if (H.owner[8] != H.n_tok) die("model vocabulary size");
    H.emb = (ggml_tensor *) (intptr_t) handles[0];
    H.head = (ggml_tensor *) (intptr_t) handles[1];
    H.norm = (ggml_tensor *) (intptr_t) handles[2];
    H.rope_sections = handles[3];
    H.reset_tensors = calloc((size_t) H.layer_count * 2, sizeof(*H.reset_tensors));
    if (!H.reset_tensors) die("model state handles");
    for (int64_t i = 0; i < H.layer_count; ++i) {
        const int64_t *record = H.records + i * 32;
        const int64_t state_slot = record[0] == 1 ? 20 : 22;
        H.reset_tensors[H.reset_count++] = (ggml_tensor *) (intptr_t) record[state_slot];
        H.reset_tensors[H.reset_count++] = (ggml_tensor *) (intptr_t) record[state_slot + 1];
    }
}

// Start a request from zero: KV bookkeeping plus every recurrent/KV cell buffer.
static void reset_state(void) {
    if (q35_kv_init(H.owner[10], 32, (sl_i64){ H.kv_state, 6 }) != 0) die("kv reset");
    for (int64_t i = 0; i < H.reset_count; ++i) {
        memset(H.reset_tensors[i]->data, 0, ggml_nbytes(H.reset_tensors[i]));
    }
}

// One full-model step: build the graph, evaluate it and publish the KV ticket.
static int64_t model_step(const int32_t * tokens, int64_t count, int64_t logit_rows, sl_f32 output) {
    sl_i64 state = { H.kv_state, 6 };
    const int64_t rows = q35_kv_read_rows(state, count);
    if (!tokens || rows <= 0 || count > 4096) return -1;
    for (int64_t i = 0; i < count; ++i) {
        if (tokens[i] < 0 || tokens[i] >= H.n_tok) return -1;
    }

    ggml_context * c = ggml_init((struct ggml_init_params){ 256u * 1024u * 1024u, NULL, true });
    if (!c) return -1;

    const int64_t ticket = q35_kv_begin(state, count);
    if (ticket <= 0) { ggml_free(c); return -1; }

    int64_t n = -1;
    void * storage = NULL;
    bool executed = false;

    ggml_tensor * ids = ggml_new_tensor_1d(c, GGML_TYPE_I32, count);
    ggml_tensor * positions = ggml_new_tensor_1d(c, GGML_TYPE_I32, count * 4);
    ggml_tensor * indices = ggml_new_tensor_1d(c, GGML_TYPE_I64, count);
    ggml_tensor * mask = ggml_new_tensor_2d(c, GGML_TYPE_F16, rows, count);

    int64_t work[16] = {0}, bn[40] = {0}, mn[42] = {0};
    int64_t * frames = calloc((size_t) (4 * H.layer_count + 4), sizeof(int64_t));
    int64_t * roots = calloc((size_t) (2 * H.layer_count + 2), sizeof(int64_t));
    int64_t * pos = calloc((size_t) count * 4, sizeof(int64_t));
    float * raw = calloc((size_t) rows * (size_t) count, sizeof(float));
    if (!frames || !roots || !pos || !raw) goto done;

    const int64_t result = q35_trunk_output((int64_t) (intptr_t) c, (int64_t) (intptr_t) ids,
                                     (int64_t) (intptr_t) H.emb, (int64_t) (intptr_t) H.head, (int64_t) (intptr_t) H.norm,
                                     (sl_i64){ H.records, H.layer_count * 32 }, H.layer_count,
                                     (int64_t) (intptr_t) positions, (int64_t) (intptr_t) mask, (int64_t) (intptr_t) indices,
                                     (sl_i64){ H.acfg, 14 }, (sl_i64){ H.gcfg, 7 }, (sl_i64){ H.mcfg, 3 },
                                     H.rope_sections, (sl_i64){ H.kv_state, 6 }, ticket,
                                     (sl_i64){ work, 16 }, (sl_i64){ bn, 40 }, (sl_i64){ mn, 42 },
                                     (sl_i64){ frames, 4 * H.layer_count + 4 },
                                     (sl_i64){ roots, 2 * H.layer_count + 2 }, logit_rows);
    if (!result) goto done;

    ggml_cgraph * graph = ggml_new_graph_custom(c, 16384, false);
    for (int64_t i = 1; i <= roots[0]; ++i) {
        ggml_build_forward_expand(graph, (ggml_tensor *) (intptr_t) roots[i]);
    }

    storage = fg_standalone_graph_alloc(graph);
    if (!storage) goto done;
    if (!ids->data || !positions->data || !indices->data || !mask->data) goto done;

    memcpy(ids->data, tokens, (size_t) count * sizeof(*tokens));
    if (q35_kv_positions(state, count, (sl_i64){ pos, count * 4 }) != count * 4) goto done;
    for (int64_t i = 0; i < count * 4; ++i) {
        ((int32_t *) positions->data)[i] = (int32_t) pos[i];
    }
    if (q35_kv_indices(state, count, (sl_i64){ indices->data, count }) != count) goto done;
    if (q35_kv_mask(state, count, (sl_f32){ raw, rows * count }) != rows) goto done;
    for (int64_t i = 0; i < rows * count; ++i) {
        ((ggml_fp16_t *) mask->data)[i] = ggml_fp32_to_fp16(raw[i]);
    }

    const ggml_tensor * lg = (const ggml_tensor *) (intptr_t) result;
    if (logit_rows != 0 && (!output.data || output.len != ggml_nelements(lg))) goto done;
    executed = true;
    if (fg_standalone_graph_compute(graph) != GGML_STATUS_SUCCESS) goto done;
    if (q35_kv_commit(state, ticket) < 0) goto done;
    n = 0;
    if (logit_rows != 0) {
        memcpy(output.data, lg->data, ggml_nbytes(lg));
        n = output.len;
    }

done:
    if (n < 0) {
        // Recurrent state can already have been written by a partial graph.
        if (executed) reset_state();
        else q35_kv_abort(state, ticket);
    }
    free(storage);
    free(pos);
    free(raw);
    free(frames);
    free(roots);
    ggml_free(c);
    return n;
}


// ---- API ---------------------------------------------------------------

void hose_host_load_context(const char * path, int64_t capacity) {
    struct stat st;
    H.fd = open(path, O_RDONLY);
    if (H.fd < 0 || fstat(H.fd, &st) != 0) die("model open");
    H.size = (size_t) st.st_size;
    H.base = mmap(NULL, H.size, PROT_READ, MAP_PRIVATE, H.fd, 0);
    if (H.base == MAP_FAILED) die("model mmap");
    load_vocab();
    init_trunk(capacity);
}

void hose_host_load(const char * path) {
    hose_host_load_context(path, HOSE_DEFAULT_CONTEXT);
}

int64_t hose_tokenize(const unsigned char * text, int64_t len, int64_t * ids, int64_t cap) {
    return tokenize(text, len, ids, cap);
}

int64_t hose_detokenize(const int64_t * ids, int64_t n, unsigned char * out, int64_t cap) {
    return detokenize(ids, n, out, cap);
}

void hose_reset_state(void) { reset_state(); }

int64_t hose_step(const int32_t * tokens, int64_t count, float ** logits) {
    if (!logits) return -1;
    *logits = NULL;
    if (count <= 0 || count > 4096 || H.n_tok <= 0 || (uint64_t) H.n_tok > SIZE_MAX / sizeof(float) / count) {
        return -1;
    }
    const int64_t capacity = count * H.n_tok;
    float * buffer = malloc((size_t) capacity * sizeof(*buffer));
    if (!buffer) return -1;
    const int64_t n = model_step(tokens, count, -1, (sl_f32) { buffer, capacity });
    if (n < 0) free(buffer);
    else *logits = buffer;
    return n;
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

int64_t hose_context_capacity(void) { return H.owner[10]; }

int64_t fg_q35_generation_eval(sl_i32 tokens, sl_f32 output) {
    return model_step(tokens.data, tokens.len, output.len != 0, output);
}

int64_t hose_generate_ids(const int64_t * prompt, int64_t count, int64_t max_tokens,
                          bool ignore_eos, int64_t * out_ids, double * timing) {
    if (!prompt || count <= 0 || !out_ids || max_tokens <= 0) return -1;
    return q35_generate((sl_i64) { (int64_t *) prompt, count }, H.n_tok, H.owner[10],
        (sl_i64) { H.eog, H.n_eog }, max_tokens, ignore_eos, (sl_i64) { out_ids, max_tokens },
        (sl_f64) { timing, timing ? 3 : 0 });
}

int64_t hose_generate_text(const unsigned char * text, int64_t len, int64_t max_tokens,
                           bool ignore_eos, int64_t * out_ids, double * timing) {
    if (timing) { timing[0] = 0.0; timing[1] = 0.0; timing[2] = 0.0; }
    if (!text || len <= 0 || len > MAX_REQ || max_tokens <= 0 || !out_ids) return -1;
    static int64_t ids[MAX_REQ];
    const int64_t n_ids = tokenize(text, len, ids, MAX_REQ);
    return hose_generate_ids(ids, n_ids, max_tokens, ignore_eos, out_ids, timing);
}

int64_t hose_generate(const unsigned char * text, int64_t len, int64_t max_tokens,
                      int64_t * out_ids, unsigned char * out_text, int64_t text_cap,
                      double * timing) {
    const int64_t produced = hose_generate_text(text, len, max_tokens, false, out_ids, timing);
    if (produced >= 0 && out_text && detokenize(out_ids, produced, out_text, text_cap) < 0) return -1;
    return produced;
}
