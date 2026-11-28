#define _POSIX_C_SOURCE 200809L

#include "bonsai.h"
#include <cuda.h>
#include <fcntl.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include "windows/platform.h"
#else
#include <sys/mman.h>
#endif
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#ifdef _WIN32
#define dlsym(module, name) ((void *)GetProcAddress((HMODULE)(module), (name)))
#define dlclose(module) FreeLibrary((HMODULE)(module))
#else
#include <dlfcn.h>
#endif
#include <pthread.h>

#ifdef _WIN32
#define BONSAI_SIZE_FMT "%llu"
#else
#define BONSAI_SIZE_FMT "%zu"
#endif

#define BONSAI_CUDA_API(X) \
    X(cuInit) \
    X(cuDeviceGet) \
    X(cuCtxCreate) \
    X(cuCtxDestroy) \
    X(cuCtxPushCurrent) \
    X(cuCtxPopCurrent) \
    X(cuCtxSynchronize) \
    X(cuModuleLoad) \
    X(cuModuleLoadData) \
    X(cuModuleGetFunction) \
    X(cuMemAlloc) \
    X(cuMemGetInfo) \
    X(cuMemcpyHtoD) \
    X(cuMemcpyDtoDAsync) \
    X(cuMemcpyDtoH) \
    X(cuMemsetD8) \
    X(cuFuncSetAttribute) \
    X(cuGetErrorString) \
    X(cuLaunchKernel)

static struct bonsai_cuda_api {
#define BONSAI_DECLARE(name) __typeof__(name) *fn_##name;
    BONSAI_CUDA_API(BONSAI_DECLARE)
#undef BONSAI_DECLARE
    void *library;
} bonsai_cuda;

#define BONSAI_STRING_(name) #name
#define BONSAI_STRING(name) BONSAI_STRING_(name)

typedef struct { const uint64_t *data; int64_t len; } bonsai_const_words;
typedef struct { uint64_t *data; int64_t len; } bonsai_words;
extern int32_t bonsai_resolve_cuda_driver(uint8_t windows, bonsai_const_words names, bonsai_words addresses);

static void bonsai_bind_cuda(void) {
#define BONSAI_SYMBOL(name) {(uint8_t *)BONSAI_STRING(name), sizeof(BONSAI_STRING(name)) - 1},
    static const sl_u8 names[] = {BONSAI_CUDA_API(BONSAI_SYMBOL)};
#undef BONSAI_SYMBOL
    enum { COUNT = sizeof(names) / sizeof(*names) };
    uint64_t addresses[COUNT + 1], symbol_names[2 * COUNT];
    for (unsigned i = 0; i < COUNT; ++i) {
        symbol_names[2 * i] = (uintptr_t)names[i].data;
        symbol_names[2 * i + 1] = names[i].len;
    }
    _Static_assert(offsetof(struct bonsai_cuda_api, library) == COUNT * sizeof(uint64_t) &&
        sizeof(struct bonsai_cuda_api) == sizeof(addresses), "Bonsai CUDA dispatch ABI");
#ifdef _WIN32
    const uint8_t windows = 1;
#else
    const uint8_t windows = 0;
#endif
    const int32_t status = bonsai_resolve_cuda_driver(windows, (bonsai_const_words){symbol_names, 2 * COUNT},
                                                     (bonsai_words){addresses, COUNT + 1});
    if (status == -1) memcpy(&bonsai_cuda, addresses, sizeof(addresses));
    else if (status >= 0) fprintf(stderr, "Bonsai CUDA driver missing %s\n", (const char *)names[status].data);
    else fputs("Bonsai CUDA driver unavailable\n", stderr);
}

static int bonsai_cuda_ready(void) {
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    return pthread_once(&once, bonsai_bind_cuda) == 0 && bonsai_cuda.library != NULL;
}

enum { LAYERS = 64, POSITIONS_MAX = 262144, VOCAB = 248320, KV_BYTES = 576,
       VERIFY_ROWS = 5, GDN_STATE = 48 * 128 * 128, GDN_HISTORY = 3 * 10240, CONTEXT_MIN = 1, UBATCH_MAX = 1024, FLASH_PANEL_SHARED = 50176, ATTENTION_BYTES = 32 * 1024 * 1024 };

enum kernel {
    NORM, ROTATE, MATRIX, ADD, DECAY, CONV, GDN, SPLIT, ROPE, Q4, TILES, GATE,
    EMBED, SWIGLU, MERGE, GDN_BATCH, ROPE_PHASES, ROPE_APPLY,
    EMBED_BATCH, CONV_BATCH, PACK_HALF, MMQ, FLASH_TILE_F32, FLASH_MERGE_F32, FLASH_TILE_STAGED, FLASH_STAGE_KV, FLASH_MERGE_ORDERED, FLASH_TILE_COMPACT, FLASH_MERGE_COMPACT, FLASH_MERGE_COMPACT_ORDERED, FLASH_TILE_ASYNC, MMQ_ASYNC, GDN_V32, MMQ_WIDE, DECAY_REUSE4, DECAY_REUSE8, FFN_TAIL, MMQ_K64_64, MMQ_K64_128, DECODE_GQA2, DECODE_GQA3, MATRIX_ROWS2, FLASH_GROUP2, FLASH_GROUP4, MV_PACK_Q8, MV_Q8_1, MV_Q8_2, NORM_WIDE_SMALL, NORM_WIDE_LARGE, FFN_Q8_PACK, FFN_Q8_MMQ, DECAY_STAGE256, DECAY_WARPPACK, FLASH_KV_PANEL, MMQ_CACHE64, MMQ_CACHE128, FLASH_FUSED_ORDERED, MMQ_HALF_ACC32, MMQ_HALF_FULL5120, MMQ_HALF_FULL17408, MMQ_HALF_FULL6144, FLASH_K_FP8, FLASH_V_ONLY, FLASH_FP8_QK, MMQ_HALF_FULL_DOWN_HALF2, FFN_Q8_FULL128, FLASH_V_FP8, FLASH_FP8_QKPV, FFN_Q8_PAIRLOAD, FFN_Q8_PANEL_PACK, FFN_Q8_PANEL, FFN_Q8_SPREAD, DECODE_DIRECT_MMA, DECODE_ORDERED_MERGE, DECODE_DIRECT_HALF2, FFN_TAIL_Q8_PANEL, FFN_DOWN_Q8_PANEL, CHOICE_TILES, CHOICE_FINAL, MTP_EH, MTP_MAT, VERIFY_Q8_1, VERIFY_Q8_2, VERIFY_CONV, DRAFT_CONCAT, DRAFT_MAT_BATCH, VERIFY_ATTN, VERIFY_MERGE, VERIFY_ATTN_TAIL, VERIFY_Q8_FOUR_1, VERIFY_Q8_FOUR_2, VERIFY_ATTN_FOUR, VERIFY_CONV_FOUR, VERIFY_Q8_FIVE_1, VERIFY_Q8_FIVE_2, VERIFY_ATTN_FIVE, VERIFY_CONV_FIVE, VERIFY_MVQ_FOUR_PAIR, VERIFY_MVQ_FOUR_QUAD, VERIFY_MVQ_FIVE_QUAD, KERNEL_COUNT
};

enum target_phase { TXN_IDLE, TXN_RUNNING, TXN_READY, TXN_FAILED };

typedef struct {
    CUcontext cuda;
    CUmodule mtp_module;
    int mtp_enabled, mtp_request;
    unsigned mtp_pending[VERIFY_ROWS], mtp_next, mtp_end;
    unsigned verify_rows;
    int verify_native, verify_q8, verify_attention, verify_merge, verify_attn_tail;
    int device_commit, device_choice, draft_q8_head, draft_batch;
    int64_t mtp[4], mtp_layer[15], draft_kv[2];
    CUdeviceptr verify_rope_q, verify_rope_k, verify_positions;
    CUdeviceptr draft_carry, draft_chain, draft_rope_k;
    struct {
        enum target_phase phase;
        unsigned start, layers, next_layer;
        unsigned layer_ids[LAYERS];
        CUdeviceptr prefix[LAYERS][VERIFY_ROWS - 1][2];
    } txn;
    CUmodule module, frozen;
    const char *decode_kernel;
    CUdeviceptr q8_input;
    int q8_mode, in_decode, norm_wide, ffn_q8, q8_full, q8_pairload, q8_panel, q8_spread, decode_mma, decode_ordered_merge, decode_half2, down_q8;
    CUdeviceptr ffn_q8_input, down_q8_owner;
    unsigned ffn_layer;
    int decay_small, flash_panel, mmq_cached, flash_fused_ordered, mmq_half_acc, mmq_half_full, mmq_half_full6144, flash_fp8_qk, mmq_half2_down, flash_fp8_pv;
    CUfunction kernel[KERNEL_COUNT];
    CUdeviceptr choice_mask, choice_tiles, choice_result;
    CUdeviceptr weights, memory, positions, rope_q, rope_k, attention, partial;
    int64_t bound[LAYERS * 15], states[LAYERS * 2], globals[3], b[24];
    CUdeviceptr token_ids, phases, packed, packed_source;
    size_t partial_capacity;
    int use_flash_f32, stage_kv, ordered_merge, compact_heads, async_fragments, async_mmq, gdn_prefill_v32, wide_mmq, decay_reuse, ffn_tail, k64_mmq;
    CUdeviceptr stage_k, stage_v;
    unsigned stage_capacity_chunks;
    unsigned context, position, ubatch, count, wave;
    int grouped_decode, matvec_rows2, flash_grouped;
    int64_t eog[64], eog_count;
    struct bonsai_ffn_capture {
        CUstream stream;
        CUgraphExec graphs[LAYERS];
        CUdeviceptr norm[LAYERS];
        unsigned phase, layer, next, enabled;
    } capture;
    float epsilon;
} model;

static int check(CUresult status, const char *where) {
    if (status == CUDA_SUCCESS) return 0;
    const char *message = NULL;
    (void) bonsai_cuda.fn_cuGetErrorString(status, &message);
    fprintf(stderr, "%s: CUDA %d (%s)\n", where, status, message ? message : "unknown");
    return -1;
}

#include "graph.inc"

static int launch(model *m, enum kernel id, unsigned x, unsigned y, unsigned z,
                  unsigned threads, void **args) {
    CUstream stream = NULL;
    if (m->capture.phase) {
        if (m->capture.next >= sizeof(ffn_graph_nodes)/sizeof(*ffn_graph_nodes) ||
            ffn_graph_nodes[m->capture.next++] != id) return -1;
        if (m->capture.phase == 2) return 0;
        stream = m->capture.stream;
    }
    const unsigned shared = id == FLASH_KV_PANEL ? FLASH_PANEL_SHARED : 0;
    return check(bonsai_cuda.fn_cuLaunchKernel(m->kernel[id], x, y, z, threads, 1, 1, shared, stream, args, NULL), "kernel launch");
}

static uint32_t f32_bits(float x) {
    uint32_t bits;
    memcpy(&bits, &x, sizeof(bits));
    return bits;
}

extern uint8_t bonsai_gdn_packet(bonsai_const_words input, uint32_t tokens, bonsai_words packet);

static int gdn(model *m, const CUdeviceptr p[8], unsigned tokens) {
    uint64_t packet[57];
    _Static_assert(sizeof(CUdeviceptr) == sizeof(uint64_t), "Bonsai GDN packet ABI");
    if (!bonsai_gdn_packet((bonsai_const_words){(const uint64_t *)p, 8}, tokens,
                          (bonsai_words){packet, 57})) return -1;
    if (m->gdn_prefill_v32 && tokens >= 64) {
        return check(bonsai_cuda.fn_cuLaunchKernel(m->kernel[GDN_V32], 192, 1, 1, 256, 1, 1, 38016, NULL,
                    (void *[]){ packet }, NULL), "Bonsai GDN V32");
    }
    return launch(m, tokens > 1 ? GDN_BATCH : GDN, 48, 1, 8, 128, (void *[]){ packet });
}

typedef struct { const uint32_t *data; int64_t len; } bonsai_indices;
extern int32_t bonsai_verify_conv_args(bonsai_const_words input, bonsai_const_words prefix,
                                       uint32_t rows, bonsai_words output);
extern int32_t bonsai_verify_gdn_rows(uint64_t context, bonsai_const_words input, bonsai_const_words prefix,
                                      uint64_t state, uint32_t rows);
extern int64_t bonsai_verify_commit(sl_i64 states, bonsai_words prefix, bonsai_indices layers,
                                    uint32_t start, uint32_t rows, uint32_t kept);

int64_t bonsai_gdn_defer_state_swap(uint64_t context) {
    return ((const model *)(uintptr_t)context)->txn.phase == TXN_RUNNING;
}

int32_t bonsai_verify_emit_gdn(uint64_t context, bonsai_const_words args) {
    return gdn((model *)(uintptr_t)context, (const CUdeviceptr *)args.data, 1);
}

static int launch_transaction_conv(model *m, const CUdeviceptr p[8]) {
    const unsigned i = m->txn.next_layer;
    if (i >= m->txn.layers) return -1;
    uint64_t args[8];
    const int32_t count = bonsai_verify_conv_args((bonsai_const_words){(const uint64_t *)p, 8},
        (bonsai_const_words){(const uint64_t *)m->txn.prefix[i], 8}, m->verify_rows, (bonsai_words){args, 8});
    if (count < 0) return -1;
    void *params[8];
    for (int32_t a = 0; a < count; ++a) params[a] = args + a;
    const enum kernel kernel = count == 8 ? VERIFY_CONV_FIVE : count == 7 ? VERIFY_CONV_FOUR : VERIFY_CONV;
    return launch(m, kernel, 40, 1, 1, 256, params);
}

static int launch_transaction_gdn(model *m, const CUdeviceptr p[8]) {
    const unsigned i = m->txn.next_layer;
    if (i >= m->txn.layers) return -1;
    const unsigned layer = m->txn.layer_ids[i];
    if (bonsai_verify_gdn_rows((uintptr_t)m, (bonsai_const_words){(const uint64_t *)p, 8},
            (bonsai_const_words){(const uint64_t *)m->txn.prefix[i], 8},
            (uint64_t)m->states[2 * layer + 1], m->verify_rows) != 0) return -1;
    ++m->txn.next_layer;
    return 0;
}

static int commit_target(model *m, unsigned kept) {
    if (m->txn.phase != TXN_READY) return -1;
    const int64_t position = bonsai_verify_commit((sl_i64){m->states, 2 * LAYERS},
        (bonsai_words){(uint64_t *)m->txn.prefix, 8 * LAYERS},
        (bonsai_indices){m->txn.layer_ids, m->txn.layers}, m->txn.start, m->verify_rows, kept);
    if (position < 0) return -1;
    m->position = (unsigned)position;
    m->txn.phase = TXN_IDLE;
    return 0;
}
#include "verify-ops.inc"

extern int32_t bonsai_flash_schedule(uint64_t context, uint64_t position, uint64_t count,
                                     uint64_t partial_bytes, uint64_t stage_capacity, uint64_t flags,
                                     bonsai_const_words pointers);

int32_t bonsai_flash_launch_raw(uint64_t context, uint32_t role, uint32_t x, uint32_t y,
                                uint32_t threads, uint32_t shared, uint64_t args) {
    model *m = (void *)(uintptr_t)context;
    static const enum kernel roles[] = {
        FLASH_K_FP8, FLASH_V_ONLY, FLASH_FP8_QK, FLASH_STAGE_KV, FLASH_FUSED_ORDERED,
        FLASH_TILE_F32, FLASH_KV_PANEL, FLASH_GROUP4, FLASH_GROUP2, FLASH_TILE_ASYNC,
        FLASH_TILE_COMPACT, FLASH_TILE_STAGED, FLASH_MERGE_COMPACT_ORDERED, FLASH_MERGE_COMPACT,
        FLASH_MERGE_ORDERED, FLASH_MERGE_F32,
    };
    if (role >= sizeof(roles) / sizeof(*roles)) return -1;
    enum kernel kernel = roles[role];
    if (m->flash_fp8_pv && role == 1) kernel = FLASH_V_FP8;
    if (m->flash_fp8_pv && role == 2) kernel = FLASH_FP8_QKPV;
    void **parameters = (void *)(uintptr_t)args;
    if (role == 2 || role == 4) return check(bonsai_cuda.fn_cuLaunchKernel(m->kernel[kernel],
        x, y, 1, threads, 1, 1, shared, NULL, parameters, NULL),
        role == 2 ? "Bonsai FP8 QK" : "Bonsai ordered fused Flash");
    return launch(m, kernel, x, y, 1, threads, parameters);
}

static int flash_f32(model *m, CUdeviceptr q, CUdeviceptr k, CUdeviceptr v, CUdeviceptr dst) {
    const uint64_t pointers[] = {q, k, v, dst, m->stage_k, m->stage_v, m->partial};
    const uint64_t flags = (m->stage_kv ? 1 : 0) | (m->ordered_merge ? 2 : 0) | (m->compact_heads ? 4 : 0) |
        (m->async_fragments ? 8 : 0) | (m->flash_grouped ? 16 : 0) | (m->flash_panel ? 32 : 0) |
        (m->flash_fused_ordered ? 64 : 0) | (m->flash_fp8_qk ? 128 : 0);
    return bonsai_flash_schedule((uintptr_t)m, m->position, m->count, m->partial_capacity,
                                  m->stage_capacity_chunks, flags, (bonsai_const_words){pointers, 7});
}

int64_t bonsai_ffn_tail_launch(uint64_t context,int64_t g,int64_t u,int64_t s,int64_t owner) {
    model *m=(model *)(uintptr_t)context;
    if (!m->ffn_tail || m->count<32) return 0;
    CUdeviceptr gate=g,up=u,signs=s;
    unsigned width=17408;
    m->down_q8_owner = 0;
    if (m->down_q8 && owner && m->count % 128 == 0 &&
            (size_t)m->count * (width / 128) * sizeof(float) <= m->partial_capacity) {
        if (launch(m,FFN_TAIL_Q8_PANEL,17,m->count,1,256,
                (void *[]){&gate,&up,&signs,&m->packed,&m->partial,&width})) return -1;
        m->down_q8_owner=(CUdeviceptr)owner;
        m->packed_source=0;
        m->ffn_q8_input=0;
        return 1;
    }
    if (launch(m,FFN_TAIL,17,m->count,1,256,(void *[]){&gate,&up,&signs,&m->packed,&width})) return -1;
    // The next projection is the only consumer of this rotation output.
    m->packed_source=(CUdeviceptr)owner;
    return 1;
}

int64_t bonsai_launch(uint64_t context, int64_t op, int64_t a, int64_t b, int64_t c,
                      int64_t d, int64_t e, int64_t f, int64_t g, int64_t h) {
    model *m = (model *)(uintptr_t)context;
    CUdeviceptr p[] = { a, b, c, d, e, f, g, h };
    unsigned n = (unsigned)e, rows = (unsigned)f, mode = (unsigned)g;
    const unsigned count = m->count;
    unsigned stride = mode == 1 ? 10240 : n * rows;
    unsigned out_width = 0, in_width = 0, heads = 0;
    float scale = 1.0f / 16.0f;
    int start = 0;
    if (m->txn.phase == TXN_RUNNING) {
        if (op == CONV) return launch_transaction_conv(m, p);
        if (op == GDN) return launch_transaction_gdn(m, p);
    }
    if (exact_verify(m, count) &&
        ((op == MATRIX && e == 1024) || op == DECAY || op == ROPE || op == TILES))
        return launch_verify_rows(m, (enum kernel)op, p);
    switch (op) {
        case NORM:
            if (m->capture.enabled && m->in_decode && count == 1 &&
                    p[0] == (CUdeviceptr)m->b[13] && p[3] == (CUdeviceptr)m->b[1] &&
                    ffn_graph_begin(m, p[1])) return -1;
            if (m->norm_wide && mode == 0 && n == 5120 && rows == 1 && stride == n) {
                const int small = count <= 512;
                return launch(m, small ? NORM_WIDE_SMALL : NORM_WIDE_LARGE, count, 1, 1,
                    small ? 512 : 128, (void *[]){ &p[0], &p[1], &p[3], &m->epsilon });
            }
            return launch(m, NORM, rows * count, 1, 1, 32,
                (void *[]){ &p[0], &p[1], &p[2], &p[3], &n, &m->epsilon, &mode, &rows, &stride });
        case ROTATE: {
            unsigned width = (unsigned)d, inverse = (unsigned)e;
            unsigned head = f ? 128 : 0, keys = f ? 16 : 0, repeat = f ? 3 : 1;
            m->packed_source = 0;
            m->q8_input = 0;
            m->ffn_q8_input = 0;
            return launch(m, ROTATE, width/1024, count, 1, 256,
                (void *[]){ &p[0], &p[1], &p[2], &inverse, &head, &keys, &repeat, &width });
        }
        case MATRIX:
            in_width = (unsigned)d; out_width = (unsigned)e;
            if (count == 1) {
                const int ffn = p[2] == (CUdeviceptr)m->b[14] ||
                    p[2] == (CUdeviceptr)m->b[15] || p[2] == (CUdeviceptr)m->b[18];
                const int use_q8 = m->in_decode && out_width > 1024 &&
                    (m->q8_mode == 1 || (m->q8_mode == 2 && ffn) ||
                     (m->q8_mode == 3 && out_width == VOCAB));
                if (use_q8) {
                    if (m->q8_input != p[1]) {
                        unsigned groups = in_width / 32;
                        if (launch(m, MV_PACK_Q8, (groups+3)/4, 1, 1, 128,
                            (void *[]){ &p[1], &m->packed, &groups })) return -1;
                        m->q8_input = p[1];
                    }
                    const int vocabulary = out_width == VOCAB;
                    unsigned rows_per_cta = vocabulary ? 4 : 8;
                    return launch(m, vocabulary ? MV_Q8_1 : MV_Q8_2,
                        (out_width+rows_per_cta-1)/rows_per_cta, 1, 1, 128,
                        (void *[]){ &p[0], &m->packed, &p[2], &in_width, &out_width });
                }
                const int grouped = m->matvec_rows2 && out_width > 1024;
                const unsigned rows_per_cta = grouped ? 8 : 4;
                return launch(m, grouped ? MATRIX_ROWS2 : MATRIX,
                    (out_width + rows_per_cta - 1) / rows_per_cta, 1, 1, 128,
                    (void *[]){ &p[0], &p[1], &p[2], &in_width, &out_width });
            }
            if (exact_verify(m, count) &&
                out_width > 1024) {
                if (m->q8_input != p[1]) {
                    unsigned groups = count * (in_width / 32);
                    if (launch(m, MV_PACK_Q8, (groups + 3) / 4, 1, 1, 128,
                               (void *[]){ &p[1], &m->packed, &groups })) return -1;
                    m->q8_input = p[1];
                }
                const int vocabulary = out_width == VOCAB;
                unsigned rows_per_cta = vocabulary ? 4 : 8;
                enum kernel projection = vocabulary ? VERIFY_Q8_1 : VERIFY_Q8_2;
                if (count == 4) {
                    projection = vocabulary ? VERIFY_Q8_FOUR_1 : VERIFY_Q8_FOUR_2;
                    if (m->mtp_enabled) {
                        projection = vocabulary ? VERIFY_MVQ_FOUR_PAIR : VERIFY_MVQ_FOUR_QUAD;
                        rows_per_cta = vocabulary ? 8 : 16;
                    }
                }
                if (count == 5) {
                    projection = vocabulary ? VERIFY_Q8_FIVE_1 : VERIFY_Q8_FIVE_2;
                    if (m->mtp_enabled) {
                        projection = VERIFY_MVQ_FIVE_QUAD;
                        rows_per_cta = 16;
                    }
                }
                return launch(m, projection,
                    (out_width + rows_per_cta - 1) / rows_per_cta, 1, 1, 128,
                    (void *[]){ &p[0], &m->packed, &p[2], &in_width, &out_width });
            }
            unsigned pairs = count * in_width / 2, tokens = count;
            if (m->down_q8_owner && p[1] == m->down_q8_owner) {
                m->down_q8_owner = 0;
                if (in_width != 17408 || out_width != 5120 || count % 128 != 0 ||
                    p[2] != (CUdeviceptr)m->b[18]) return -1;
                return launch(m, FFN_DOWN_Q8_PANEL, out_width/64, count/128, 1, 256,
                    (void *[]){ &p[0], &m->packed, &m->partial, &p[2], &in_width, &out_width, &tokens });
            }
            const size_t scale_bytes = (size_t)count * (in_width / 128) * sizeof(float);
            const int ffn_gate = count >= 128 && in_width == 5120 && out_width == 17408 &&
                p[2] == (CUdeviceptr)m->b[14];
            const int ffn_up = count >= 128 && in_width == 5120 && out_width == 17408 &&
                p[2] == (CUdeviceptr)m->b[15];
            if (ffn_gate) ++m->ffn_layer;
            if (m->ffn_q8 && (ffn_gate || ffn_up) && scale_bytes <= m->partial_capacity &&
                (m->ffn_q8 == 1 || m->ffn_layer > (unsigned)(LAYERS - m->ffn_q8))) {
                const int full_q8 = m->q8_full && count % 128 == 0;
                const int panel = full_q8 && m->q8_pairload && m->q8_panel;
                unsigned groups = count * (in_width / 128);
                if (m->ffn_q8_input != p[1]) {
                    if (launch(m, panel ? FFN_Q8_PANEL_PACK : FFN_Q8_PACK, (groups + 3) / 4, 1, 1, 128,
                        (void *[]){ &p[1], &m->packed, &m->partial, &groups })) return -1;
                    m->ffn_q8_input = p[1];
                    m->packed_source = 0;
                }
                const unsigned tokens_per_tile = full_q8 ? 128 : 64;
                const enum kernel kernel = panel ?
                    (m->q8_spread ? FFN_Q8_SPREAD : FFN_Q8_PANEL) : full_q8 ?
                    (m->q8_pairload ? FFN_Q8_PAIRLOAD : FFN_Q8_FULL128) : FFN_Q8_MMQ;
                return launch(m, kernel,
                    (out_width+63)/64, (count+tokens_per_tile-1)/tokens_per_tile, 1, 256,
                    (void *[]){ &p[0], &m->packed, &m->partial, &p[2], &in_width, &out_width, &tokens });
            }
            if (m->packed_source != p[1]) {
                if (launch(m, PACK_HALF, (pairs+255)/256, 1, 1, 256,
                    (void *[]){ &p[1], &m->packed, &pairs })) return -1;
                m->packed_source = p[1];
            }
            const int wide = m->async_mmq && m->wide_mmq && count >= 128;
            const unsigned tile = wide ? 64 : 32;
            const int k64 = wide && m->k64_mmq;
            const int partial_half = k64 && m->mmq_cached && m->mmq_half_acc;
            const unsigned tokens_per_tile = k64 && (count >= 512 || partial_half) ? 128 : tile;
            enum kernel projection = partial_half ? MMQ_HALF_ACC32 : k64
                ? (m->mmq_cached ? (tokens_per_tile == 128 ? MMQ_CACHE128 : MMQ_CACHE64)
                                 : (tokens_per_tile == 128 ? MMQ_K64_128 : MMQ_K64_64))
                : (wide ? MMQ_WIDE : m->async_mmq ? MMQ_ASYNC : MMQ);
            if (partial_half && m->mmq_half_full && out_width % 64 == 0 && count % 128 == 0) {
                if (in_width == 5120) projection = MMQ_HALF_FULL5120;
                if (in_width == 17408) {
                    projection = m->mmq_half2_down && out_width == 5120 &&
                        p[2] == (CUdeviceptr)m->b[18] ? MMQ_HALF_FULL_DOWN_HALF2 : MMQ_HALF_FULL17408;
                }
                if (in_width == 6144 && m->mmq_half_full6144) projection = MMQ_HALF_FULL6144;
            }
            return launch(m, projection, (out_width+tile-1)/tile,
                (count+tokens_per_tile-1)/tokens_per_tile, 1, wide ? 256 : 128,
                (void *[]){ &p[0], &m->packed, &p[2], &in_width, &out_width, &tokens });
        case ADD: {
            const int status = launch(m, ADD, 20 * count, 1, 1, 256,
                (void *[]){ &p[0], &p[1], &p[2] });
            if (status || !m->capture.phase) return status;
            if (p[0] != (CUdeviceptr)m->b[13] || p[1] != (CUdeviceptr)m->b[18] ||
                    p[2] != (CUdeviceptr)m->b[0]) return -1;
            return ffn_graph_finish(m);
        }
        case DECAY:
            if (m->decay_small && count == 1) {
                return launch(m, DECAY_STAGE256, 48, 1, 1, 256,
                    (void *[]){ &p[0], &p[1], &p[2], &p[3], &p[4], &p[5], &p[6] });
            }
            if (m->decay_small && count < 32) {
                unsigned tokens=count;
                return launch(m, DECAY_WARPPACK, 12 * count, 1, 1, 128,
                    (void *[]){ &p[0], &p[1], &p[2], &p[3], &p[4], &p[5], &p[6], &tokens });
            }
            if (m->decay_reuse && count>=32) {
                unsigned tokens=count;
                const unsigned tile=count>=256 ? 8 : 4;
                return launch(m,tile==8 ? DECAY_REUSE8 : DECAY_REUSE4,12*((count+tile-1)/tile),
                    1,1,128,(void *[]){&p[0],&p[1],&p[2],&p[3],&p[4],&p[5],&p[6],&tokens});
            }
            return launch(m, DECAY, 48 * count, 1, 1, 32, (void *[]){ &p[0], &p[1], &p[2], &p[3], &p[4], &p[5], &p[6] });
        case CONV: {
            unsigned tokens = count;
            if (count > 1) {
                return launch(m, CONV_BATCH, 40, 1, 1, 256, (void *[]){ &p[0], &p[1], &p[2], &p[3], &tokens });
            }
            return launch(m, CONV, 40, 1, 1, 256, (void *[]){ &p[0], &p[1], &p[2], &p[3] });
        }
        case GDN:
            return gdn(m, p, count);
        case SPLIT:
            return launch(m, SPLIT, 24 * count, 1, 1, 256, (void *[]){ &p[0], &p[1], &p[2] });
        case ROPE: {
            heads = (unsigned)c;
            CUdeviceptr config = heads == 24 ? m->rope_q : m->rope_k, no_factors = 0;
            if (count == 1) {
                return launch(m, ROPE, heads, 1, 1, 128, (void *[]){ &p[0], &m->positions, &no_factors, &p[1], &config });
            }
            if (launch(m, ROPE_PHASES, (count+127)/128, 1, 1, 128,
                (void *[]){ &m->positions, &no_factors, &m->phases, &config })) return -1;
            return launch(m, ROPE_APPLY, heads * count, 1, 1, 128,
                (void *[]){ &p[0], &p[1], &m->phases, &config });
        }
        case Q4: {
            uint64_t stride = 4096, tasks = 32 * count;
            unsigned blocks = 32;
            return launch(m, Q4, (tasks+127)/128, 1, 1, 128, (void *[]){ &p[0], &p[1], &stride, &blocks, &tasks });
        }
        case TILES: {
            if (m->use_flash_f32 && count >= 8) {
                const int rc = flash_f32(m, p[0], p[1], p[2], p[3]);
                if (rc <= 0) return rc;
            }
            CUdeviceptr no_mask = 0;
            const unsigned tiles = ((unsigned)e + count + 254)/256;
            const unsigned tasks = 24 * count;
            if (m->grouped_decode && count == 1 && m->wave >= tasks) {
                unsigned keys = m->position + 1;
                unsigned n_tiles = tiles;
                unsigned heads = keys >= 32768 ? 3 : 2;
                if (m->decode_mma && (keys >= 32768 || m->decode_mma == 2)) {
                    const enum kernel tile = m->decode_half2 ?
                        DECODE_DIRECT_HALF2 : DECODE_DIRECT_MMA;
                    if (launch(m, tile, 4, tiles, 1, 128,
                        (void *[]){ &p[0], &p[1], &p[2], &m->partial, &keys, &n_tiles })) return -1;
                } else if (launch(m, heads == 3 ? DECODE_GQA3 : DECODE_GQA2, 24 / heads, tiles, 1, 32,
                    (void *[]){ &p[0], &p[1], &p[2], &m->partial, &keys, &n_tiles, &scale })) return -1;
                if (m->decode_ordered_merge && keys >= 32768) {
                    return launch(m, DECODE_ORDERED_MERGE, tasks, 1, 1, 128,
                        (void *[]){ &m->partial, &p[3], &keys, &n_tiles });
                }
                return launch(m, MERGE, tasks, 1, 1, 32,
                    (void *[]){ &m->partial, &p[3], &m->attention, &start });
            }
            for (; (unsigned)start < tasks; start += (int)m->wave) {
                const unsigned left = tasks - (unsigned)start;
                const unsigned active = left < m->wave ? left : m->wave;
                if (launch(m, TILES, active, tiles, 1, 32, (void *[]){
                    &p[0], &p[1], &p[2], &no_mask, &m->partial, &m->attention, &scale, &start }) ||
                    launch(m, MERGE, active, 1, 1, 32,
                        (void *[]){ &m->partial, &p[3], &m->attention, &start })) return -1;
            }
            return 0;
        }
        case GATE:
            return launch(m, GATE, 24 * count, 1, 1, 256, (void *[]){ &p[0], &p[1], &p[2] });
        case EMBED: {
            if (count > 1) {
                return launch(m, EMBED_BATCH, 20, count, 1, 256, (void *[]){ &p[0], &p[1], &m->token_ids });
            }
            unsigned id = (unsigned)c, width = 5120;
            return launch(m, EMBED, 20, 1, 1, 256, (void *[]){ &p[0], &p[1], &width, &id });
        }
        case SWIGLU:
            out_width = 17408 * count;
            return launch(m, SWIGLU, 68 * count, 1, 1, 256, (void *[]){ &p[0], &p[1], &p[2], &out_width });
        default:
            return -1;
    }
}

static sl_i64 slice(int64_t *data, int64_t len) {
    return (sl_i64){ data, len };
}

static int rope_config(CUdeviceptr dest, int64_t heads, int64_t tokens) {
    const int64_t conf[27] = {
        tokens == 1 ? 2 : 0, heads * tokens, 1, (tokens+127)/128, 1024 * tokens,
        256, heads, tokens, 1, 64, 40, 0, 11, 11, 10, 0,
        4, 1024, heads * 1024, tokens * heads * 1024, 1,
        f32_bits(powf(1e7f, -1.0f / 32)), f32_bits(1), 0, f32_bits(1), 0, 0,
    };
    return check(bonsai_cuda.fn_cuMemcpyHtoD(dest, conf, sizeof(conf)), "RoPE configuration");
}

typedef struct { uint32_t *data; int64_t len; } bonsai_mutable_indices;
extern uint8_t bonsai_workspace_plan(uint64_t context, uint64_t batch, uint64_t rows, uint64_t flags,
    sl_i64 bound, sl_i64 states, sl_i64 scratch, bonsai_words prefix,
    bonsai_mutable_indices layer_ids, bonsai_words layout);
extern void bonsai_workspace_rebase(uint64_t base, uint64_t rows, uint64_t flags,
    sl_i64 states, sl_i64 scratch, bonsai_words prefix, bonsai_words layout);

static int workspace(model *m, const int32_t *sign) {
    _Static_assert(LAYERS == 64 && VERIFY_ROWS == 5 && KV_BYTES == 576 && VOCAB == 248320 &&
        ATTENTION_BYTES == 32 * 1024 * 1024 && sizeof(struct verify_position) == 192, "Bonsai workspace ABI");
    const uint64_t flags = (m->use_flash_f32 ? 1 : 0) | (m->mtp_enabled ? 2 : 0) |
        (m->device_commit ? 4 : 0) | (m->verify_native ? 8 : 0) | (m->stage_kv ? 16 : 0);
    uint64_t values[17] = {0};
    const bonsai_words layout = {values, 17};
    const sl_i64 states = {m->states, 2 * LAYERS}, scratch = {m->b, 24};
    const bonsai_words prefix = {(uint64_t *)m->txn.prefix, 8 * LAYERS};
    if (!bonsai_workspace_plan(m->context, m->ubatch, m->verify_rows, flags,
            (sl_i64){m->bound, 15 * LAYERS}, states, scratch, prefix,
            (bonsai_mutable_indices){m->txn.layer_ids, LAYERS}, layout)) return -1;
    m->partial_capacity = values[1];
    m->txn.layers = values[2];
    if (check(bonsai_cuda.fn_cuMemAlloc(&m->memory, values[0]), "state and scratch") ||
        check(bonsai_cuda.fn_cuMemsetD8(m->memory, 0, values[0]), "state initialization")) return -1;
    bonsai_workspace_rebase(m->memory, m->verify_rows, flags, states, scratch, prefix, layout);
    m->positions = values[3]; m->phases = values[4]; m->token_ids = values[5]; m->packed = values[6];
    m->rope_q = values[7]; m->rope_k = values[8]; m->attention = values[9]; m->partial = values[10];
    if (m->verify_native) {
        m->verify_rope_q = values[11]; m->verify_rope_k = values[12]; m->verify_positions = values[13];
        if (rope_config(m->verify_rope_q, 24, 1) || rope_config(m->verify_rope_k, 4, 1)) return -1;
    }
    m->stage_capacity_chunks = values[14]; m->stage_k = values[15]; m->stage_v = values[16];
    const unsigned signs[] = {5120, 6144, 17408};
    for (unsigned i = 0; i < 3; ++i) {
        if (check(bonsai_cuda.fn_cuMemcpyHtoD(m->b[20 + i], sign, signs[i] * sizeof(*sign)), "Hadamard signs")) return -1;
        sign += signs[i];
    }
    fprintf(stderr, "Device workspace: %llu bytes (Q4_0 K/V, GDN, activations)\n", (unsigned long long)values[0]);
    return 0;
}

typedef struct { const int32_t *data; int64_t len; } bonsai_const_i32;
extern int32_t bonsai_load_weights_plan(uint64_t context, sl_u8 file, uint64_t mapping, uint64_t length,
    uint8_t mtp_enabled, sl_i64 globals, sl_i64 bound, sl_i64 draft, sl_i64 draft_layer,
    sl_i64 eog, sl_f32 epsilon, sl_i64 result);

uint64_t bonsai_upload_weights(uint64_t context, uint64_t source, uint64_t bytes) {
    model *m = (void *)(uintptr_t)context;
    if (check(bonsai_cuda.fn_cuMemAlloc(&m->weights, bytes), "resident GGUF weights") ||
        check(bonsai_cuda.fn_cuMemcpyHtoD(m->weights, (const void *)(uintptr_t)source, bytes),
              "resident GGUF upload")) return 0;
    return m->weights;
}

int32_t bonsai_bind_weight_workspace(uint64_t context, uint64_t bytes, bonsai_const_i32 signs) {
    fprintf(stderr, "Device GGUF payload: " BONSAI_SIZE_FMT " bytes (includes inactive MTP tensors)\n", (size_t)bytes);
    return workspace((model *)(uintptr_t)context, signs.data);
}

void bonsai_log_gguf_parse(int64_t status) {
    fprintf(stderr, "GGUF parse: %lld\n", (long long)status);
}

static int load_weights(model *m, uint8_t *mapping, size_t length) {
    const sl_u8 file = {mapping, length > 67108864 ? 67108864 : (int64_t)length};
    return bonsai_load_weights_plan((uintptr_t)m, file, (uintptr_t)mapping, length, m->mtp_enabled != 0,
        slice(m->globals, 3), slice(m->bound, LAYERS * 15), slice(m->mtp, 4),
        slice(m->mtp_layer, 15), slice(m->eog, 64), (sl_f32){&m->epsilon, 1}, slice(&m->eog_count, 1));
}

struct token_choice {
    uint32_t token;
    float logit;
    uint32_t invalid;
};
_Static_assert(sizeof(struct token_choice) == 12, "CUDA token choice ABI");
enum { CHOICE_JOBS = (VOCAB + 1023) / 1024, CHOICE_MASK_WORDS = (VOCAB + 31) / 32 };
_Static_assert(CHOICE_JOBS <= 256, "one final block covers all partials");

static int choice_open(model *m) {
    uint32_t tokens[CHOICE_MASK_WORDS] = {0};
    for (int64_t j = 0; j < m->eog_count; ++j) {
        const uint32_t id = (uint32_t)m->eog[j];
        if (id >= VOCAB) return -1;
        tokens[id / 32] |= 1U << (id % 32);
    }
    const unsigned choice_rows = m->mtp_enabled ? m->verify_rows : 1;
    const size_t partial_bytes = choice_rows * CHOICE_JOBS * sizeof(struct token_choice);
    const size_t bytes = sizeof(tokens) + partial_bytes + choice_rows * sizeof(struct token_choice);
    if (check(bonsai_cuda.fn_cuMemAlloc(&m->choice_mask, bytes), "choice workspace") ||
        check(bonsai_cuda.fn_cuMemcpyHtoD(m->choice_mask, tokens, sizeof(tokens)), "choice EOG mask")) return -1;
    m->choice_tiles = m->choice_mask + sizeof(tokens);
    m->choice_result = m->choice_tiles + partial_bytes;
    return 0;
}

static int choose_device(model *m, int ignore_eos, struct token_choice *result) {
    CUdeviceptr logits = m->b[19], mask = ignore_eos ? m->choice_mask : 0;
    unsigned width = VOCAB, jobs = CHOICE_JOBS;
    return launch(m, CHOICE_TILES, jobs, 1, 1, 256,
                  (void *[]){ &logits, &mask, &m->choice_tiles, &width, &jobs }) ||
           launch(m, CHOICE_FINAL, 1, 1, 1, 256,
                  (void *[]){ &m->choice_tiles, &m->choice_result, &jobs }) ||
           check(bonsai_cuda.fn_cuMemcpyDtoH(result, m->choice_result, sizeof(*result)), "token choice");
}

#include "mtp-abi.h"
#include "draft.inc"

extern const unsigned char gooseinfer_bonsai_pipeline_image[];
extern const unsigned char gooseinfer_bonsai_frozen_image[];
extern const unsigned char gooseinfer_bonsai_mtp_image[];

static CUresult bonsai_load_module(CUmodule *module, const char *path, const unsigned char *image) {
    return path && *path ? bonsai_cuda.fn_cuModuleLoad(module, path) :
        bonsai_cuda.fn_cuModuleLoadData(module, image);
}

static model *bonsai_open(const char *gguf, const char *cubin, const char *frozen, unsigned context, unsigned ubatch) {
    if (context < CONTEXT_MIN || context > POSITIONS_MAX) return NULL;
    if (!bonsai_cuda_ready()) return NULL;
    model *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->context = context; m->ubatch = ubatch;
    const char *mtp_image = getenv("BONSAI_HTTP_MTP_IMAGE");
    if (!mtp_image || *mtp_image) {
        m->mtp_enabled = m->verify_native = m->verify_q8 = m->device_commit = 1;
        m->verify_attention = m->verify_merge = m->verify_attn_tail = m->draft_q8_head = 1;
        m->verify_rows = 4;
    }
    const char *flag = getenv("BONSAI_FLASH_F32");
    m->use_flash_f32 = flag && strcmp(flag, "1") == 0;
    const char *stage = getenv("BONSAI_FLASH_REUSE");
    m->stage_kv = m->use_flash_f32 && stage && strcmp(stage, "1") == 0;
    const char *merge = getenv("BONSAI_FLASH_ORDERED_MERGE");
    m->ordered_merge = m->stage_kv && merge && strcmp(merge, "1") == 0;
    const char *compact = getenv("BONSAI_FLASH_COMPACT");
    m->compact_heads = m->stage_kv && compact && strcmp(compact, "1") == 0;
    const char *frags = getenv("BONSAI_FLASH_ASYNC");
    m->async_fragments = m->compact_heads && frags && strcmp(frags, "1") == 0;
    if (m->async_fragments) fputs("Flash fragments: async staging and ldmatrix\n", stderr);
    const char *panel = getenv("BONSAI_FLASH_KV_PANEL");
    m->flash_panel = !panel || strcmp(panel, "1") == 0;
    const char *reuse = getenv("BONSAI_FLASH_TILE_REUSE");
    m->flash_grouped = !reuse || strcmp(reuse, "1") == 0;
    if (m->async_fragments && m->flash_grouped) fputs("Flash: reuse Q across KV tiles\n", stderr);
    const char *mmq = getenv("BONSAI_PQ2_MMQ_ASYNC");
    m->async_mmq = !mmq || strcmp(mmq,"1") == 0;
    if (m->async_mmq) fputs("PQ2 MMQ: async activation and warp fragment loads\n",stderr);
    const char *gdn_mode = getenv("BONSAI_GDN_V32");
    m->gdn_prefill_v32 = !gdn_mode || strcmp(gdn_mode,"1") == 0;
    if (m->gdn_prefill_v32) fputs("Prefill GDN: frozen Goose MMA V32\n",stderr);
    const char *wide = getenv("BONSAI_PQ2_MMQ_WIDE");
    m->wide_mmq = !wide || strcmp(wide,"1") == 0;
    if (m->wide_mmq) fputs("PQ2 MMQ: wide tiles for 128 or more tokens\n",stderr);
    const char *kchunk=getenv("BONSAI_PQ2_MMQ_K64");
    m->k64_mmq=!kchunk || strcmp(kchunk,"1")==0;
    if (m->k64_mmq) fputs("PQ2 MMQ: K64 exact wide tiles\n",stderr);
    const char *half2_down = getenv("BONSAI_MMQ_HALF2_DOWN");
    m->mmq_half2_down = !half2_down || strcmp(half2_down,"0") != 0;
    const char *full6144 = getenv("BONSAI_MMQ_HALF_FULL6144");
    m->mmq_half_full6144 = !full6144 || strcmp(full6144,"0") != 0;
    const char *full = getenv("BONSAI_MMQ_HALF_FULL");
    m->mmq_half_full = !full || strcmp(full,"0") != 0;
    const char *half_acc = getenv("BONSAI_MMQ_HALF_ACC");
    m->mmq_half_acc = half_acc && strcmp(half_acc,"1") == 0;
    const char *cached = getenv("BONSAI_MMQ_CACHED_PQ2");
    m->mmq_cached = !cached || strcmp(cached, "1") == 0;
    if (m->k64_mmq && m->mmq_cached) fputs("PQ2 MMQ: reuse packed weight scales\n", stderr);
    const char *small=getenv("BONSAI_DECAY_SMALL");
    m->decay_small=!small || strcmp(small,"1")==0;
    if (m->decay_small) fputs("Bonsai decay: staged single-token products\n",stderr);
    const char *decay=getenv("BONSAI_DECAY_REUSE");
    m->decay_reuse=!decay || strcmp(decay,"1")==0;
    if (m->decay_reuse) fputs("Batch decay: reuse alpha/beta weights\n",stderr);
    const char *tail=getenv("BONSAI_FFN_TAIL");
    m->ffn_tail=!tail || strcmp(tail,"1")==0;
    if (m->ffn_tail) fputs("FFN tail: fused SwiGLU, FWHT and FP16 packing\n",stderr);
    if (m->compact_heads) fputs("Flash query layout: compact 6:1\n", stderr);
    if (m->ordered_merge) fputs("Flash merge: ordered parallel coefficients\n", stderr);
    fprintf(stderr, "Prefill attention: %s\n", !m->use_flash_f32 ? "tiled Q4" :
        m->stage_kv ? "staged Bonsai FP32 Flash" : "direct Bonsai FP32 Flash");
    const char *decode_merge = getenv("BONSAI_DECODE_ORDERED_MERGE");
    m->decode_ordered_merge = decode_merge && !strcmp(decode_merge, "1");
    const char *decode_half2 = getenv("BONSAI_DECODE_DIRECT_HALF2");
    m->decode_half2 = decode_half2 && !strcmp(decode_half2, "1");
    const char *mma = getenv("BONSAI_DECODE_DIRECT_MMA");
    m->decode_mma = !mma ? 0 : !strcmp(mma, "1") ? 1 : !strcmp(mma, "diagnostic") ? 2 : 0;
    if (m->decode_mma) fputs("Bonsai single-query attention: direct Q4 MMA\n", stderr);
    const char *grouped = getenv("BONSAI_DECODE_GQA");
    m->grouped_decode = !grouped || strcmp(grouped, "1") == 0;
    if (m->grouped_decode) fputs("Decode attention: grouped Q4 K/V\n", stderr);
    const char *q8 = getenv("BONSAI_DECODE_Q8");
    m->q8_mode = 1;
    if (q8) {
        if (strcmp(q8, "off") == 0) m->q8_mode = 0;
        else if (strcmp(q8, "full") == 0) m->q8_mode = 1;
        else if (strcmp(q8, "ffn") == 0) m->q8_mode = 2;
        else if (strcmp(q8, "vocab") == 0) m->q8_mode = 3;
        else goto error;
    }
    if (m->q8_mode) fprintf(stderr, "Bonsai Q8 decode: %s\n", q8 ? q8 : "full");
    const char *norm = getenv("BONSAI_WIDE_NORM");
    m->norm_wide = !norm || strcmp(norm, "1") == 0;
    if (m->norm_wide) fputs("Bonsai RMSNorm: parallel width 5120\n", stderr);
    const char *full_q8 = getenv("BONSAI_FFN_Q8_FULL128");
    m->q8_full = !full_q8 || strcmp(full_q8,"0") != 0;
    const char *spread = getenv("BONSAI_FFN_Q8_SPREAD");
    m->q8_spread = spread && strcmp(spread, "1") == 0;
    const char *down_q8 = getenv("BONSAI_FFN_DOWN_Q8_FUSED");
    m->down_q8 = down_q8 && !strcmp(down_q8, "1");
    if (m->down_q8) fputs("Bonsai prefill: fused Q8 FFN down\n", stderr);
    const char *q8_panel = getenv("BONSAI_FFN_Q8_PANEL");
    m->q8_panel = q8_panel && strcmp(q8_panel, "1") == 0;
    if (m->q8_panel) fputs("Bonsai prefill: panel-major Q8 activations\n", stderr);
    const char *pairload = getenv("BONSAI_FFN_Q8_PAIRLOAD");
    m->q8_pairload = pairload && strcmp(pairload, "1") == 0;
    if (m->q8_pairload) fputs("Bonsai prefill: paired Q8 weight loads\n", stderr);
    const char *ffn_q8 = getenv("BONSAI_FFN_Q8_PREFILL");
    m->ffn_q8 = 16;
    if (ffn_q8) {
        if (strcmp(ffn_q8, "0") == 0) m->ffn_q8 = 0;
        else if (strcmp(ffn_q8, "1") == 0) m->ffn_q8 = 1;
        else if (strcmp(ffn_q8, "last16") == 0) m->ffn_q8 = 16;
        else if (strcmp(ffn_q8, "last32") == 0) m->ffn_q8 = 32;
        else goto error;
    }
    if (m->ffn_q8) fputs("Bonsai prefill: Q8 FFN gate/up\n", stderr);
    CUdevice device;
    const char *mode = getenv("BONSAI_MATVEC");
    m->matvec_rows2 = !mode || strcmp(mode, "rows2") == 0;
    if (m->matvec_rows2 || strcmp(mode, "ordered") == 0) {
        m->decode_kernel = "bonsai_pq2_matvec_ordered";
    } else if (strcmp(mode, "exchange") == 0) {
        m->decode_kernel = "bonsai_pq2_matvec_exchange";
    } else if (strcmp(mode, "scalar") == 0) {
        m->decode_kernel = "bonsai_pq2_matvec";
    } else {
        fprintf(stderr, "Invalid BONSAI_MATVEC mode\n");
        goto error;
    }
    fprintf(stderr, "Decode matrix: %s\n", m->matvec_rows2 ? "bonsai_pq2_matvec_rows2" : m->decode_kernel);
    const char *names[KERNEL_COUNT] = {
        "bonsai_norm", "bonsai_fwht1024", m->decode_kernel, "bonsai_residual", "bonsai_decay",
        "bonsai_conv_step", "goose_gdn_decode", "bonsai_split_q_gate", "goose_rope_decode",
        "goose_set_q4_rows", "goose_q4_attention_tiles", "bonsai_attention_gate", "bonsai_pq2_row",
        "bonsai_swiglu", "goose_q4_attention_merge", "goose_gated_delta_net",
        "goose_rope_phases", "goose_rope_apply", "bonsai_embed_batch", "bonsai_conv_batch",
        "bonsai_pack_half", "bonsai_pq2_mmq", "bonsai_flash_f32_tile", "bonsai_flash_f32_merge",
        "bonsai_flash_staged_tile", "bonsai_flash_stage_kv", "bonsai_flash_ordered_merge",
        "bonsai_flash_compact_tile", "bonsai_flash_compact_merge", "bonsai_flash_compact_ordered_merge", "bonsai_flash_async_tile", "bonsai_pq2_mmq_async", "goose_gdn_mma_v32", "bonsai_pq2_mmq_64x64", "bonsai_decay_reuse4", "bonsai_decay_reuse8", "bonsai_ffn_tail", "bonsai_pq2_mmq_64x64_k64", "bonsai_pq2_mmq_64x128_k64", "bonsai_decode_gqa2", "bonsai_decode_gqa3", "bonsai_pq2_matvec_rows2", "bonsai_flash_tile_group2", "bonsai_flash_tile_group4",
        "bonsai_mv_pack_q8", "bonsai_pq2_mvq_q8_1", "bonsai_pq2_mvq_q8_2", "bonsai_norm_wide_512_1", "bonsai_norm_wide_128_0", "bonsai_pack_q8_128", "bonsai_pq2_mmq_int8", "bonsai_decay_stage_256_0", "bonsai_decay_warp_pack", "bonsai_flash_kv_lean2", "bonsai_pq2_cached_64x64_k64", "bonsai_pq2_cached_64x128_k64", "bonsai_flash_kv_fused_ordered", "bonsai_pq2_cached_fp16_acc_k32", "bonsai_pq2_half_full_5120", "bonsai_pq2_half_full_17408", "bonsai_pq2_half_full_6144", "bonsai_flash_stage_k_fp8", "bonsai_flash_stage_v_only", "bonsai_flash_fp8_qk", "bonsai_pq2_half_unpack_half2_17408", "bonsai_pq2_mmq_int8_full_n128", "bonsai_flash_stage_v_fp8", "bonsai_flash_fp8_qkpv", "bonsai_pq2_mmq_int8_full_n128_pairload", "bonsai_pack_q8_panel128", "bonsai_pq2_mmq_int8_full_n128_panel", "bonsai_pq2_mmq_int8_full_n128_spread", "bonsai_decode_direct_q4_mma", "bonsai_verify_merge_parallel", "bonsai_decode_direct_q4_mma_half2", "bonsai_ffn_tail_q8_panel", "bonsai_pq2_mmq_int8_down_n128_spread", "bonsai_choice_tiles", "bonsai_choice_final",
    };
    if (check(bonsai_cuda.fn_cuInit(0), "driver") || check(bonsai_cuda.fn_cuDeviceGet(&device, 0), "device") ||
        check(bonsai_cuda.fn_cuCtxCreate(&m->cuda, 0, device), "context") ||
        check(bonsai_load_module(&m->module, cubin, gooseinfer_bonsai_pipeline_image), "Bonsai CUDA image") ||
        check(bonsai_load_module(&m->frozen, frozen, gooseinfer_bonsai_frozen_image), "frozen Goose CUDA image")) goto error;
    if (m->mtp_enabled &&
        check(bonsai_load_module(&m->mtp_module, mtp_image, gooseinfer_bonsai_mtp_image), "Bonsai MTP CUDA image")) goto error;
    const char *mtp_names[KERNEL_COUNT] = {
        [MTP_EH] = "bonsai_mtp_eh_q4",
         [MTP_MAT] = "bonsai_mtp_mat_q4",
         [VERIFY_Q8_1] = "bonsai_pq2_mvq_q8_three_1",
         [VERIFY_Q8_2] = "bonsai_pq2_mvq_q8_three_2",
         [VERIFY_CONV] = "bonsai_conv_verify_prefix",
         [DRAFT_CONCAT] = "bonsai_mtp_concat",
         [DRAFT_MAT_BATCH] = "bonsai_mtp_q4_four",
         [VERIFY_ATTN] = "bonsai_verify_q4_gqa2",
         [VERIFY_MERGE] = "bonsai_verify_merge_parallel",
         [VERIFY_ATTN_TAIL] = "bonsai_verify_q4_tail",
         [VERIFY_Q8_FOUR_1] = "bonsai_pq2_mvq_q8_four_1",
         [VERIFY_Q8_FOUR_2] = "bonsai_pq2_mvq_q8_four_2",
         [VERIFY_ATTN_FOUR] = "bonsai_verify_pair_tail3",
         [VERIFY_CONV_FOUR] = "bonsai_conv_verify_prefix_four",
         [VERIFY_Q8_FIVE_1] = "bonsai_pq2_mvq_q8_five_1",
         [VERIFY_Q8_FIVE_2] = "bonsai_pq2_mvq_q8_five_2",
         [VERIFY_ATTN_FIVE] = "bonsai_verify_five_32",
         [VERIFY_CONV_FIVE] = "bonsai_conv_verify_prefix_five",
         [VERIFY_MVQ_FOUR_PAIR] = "bonsai_mvq_4_permutation_2",
         [VERIFY_MVQ_FOUR_QUAD] = "bonsai_mvq_4_permutation_4",
         [VERIFY_MVQ_FIVE_QUAD] = "bonsai_mvq_5_spread_4",
    };
    for (unsigned i = 0; i < KERNEL_COUNT; ++i) {
        if (i > CHOICE_FINAL) {
            if (m->mtp_enabled && check(bonsai_cuda.fn_cuModuleGetFunction(&m->kernel[i],
                m->mtp_module, mtp_names[i]), mtp_names[i])) goto error;
            continue;
        }
        CUmodule module = strncmp(names[i], "goose_", 6) == 0 ? m->frozen : m->module;
        if (check(bonsai_cuda.fn_cuModuleGetFunction(&m->kernel[i], module, names[i]), names[i])) goto error;
    }
    const char *pv = getenv("BONSAI_FLASH_FP8_PV");
    m->flash_fp8_pv = pv && strcmp(pv,"1") == 0 &&
        bonsai_cuda.fn_cuFuncSetAttribute(m->kernel[FLASH_FP8_QKPV],
            CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES, 41472) == CUDA_SUCCESS;
    const char *fp8 = getenv("BONSAI_FLASH_FP8_QK");
    m->flash_fp8_qk = fp8 && strcmp(fp8,"1") == 0 &&
        bonsai_cuda.fn_cuFuncSetAttribute(m->kernel[FLASH_FP8_QK],
            CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES, 41472) == CUDA_SUCCESS;
    const char *fused = getenv("BONSAI_FLASH_FUSED_ORDERED");
    m->flash_fused_ordered = (!fused || strcmp(fused, "0") != 0) &&
        bonsai_cuda.fn_cuFuncSetAttribute(m->kernel[FLASH_FUSED_ORDERED],
            CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES, 50176) == CUDA_SUCCESS;
    if (m->flash_panel) {
        m->flash_panel = bonsai_cuda.fn_cuFuncSetAttribute(m->kernel[FLASH_KV_PANEL],
            CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES, FLASH_PANEL_SHARED) == CUDA_SUCCESS;
        if (m->flash_panel) fputs("Bonsai Flash: shared K/V query panels\n",stderr);
    }
    if (m->gdn_prefill_v32 && check(bonsai_cuda.fn_cuFuncSetAttribute(m->kernel[GDN_V32],
        CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES,38016),"GDN V32 shared memory")) goto error;
#ifdef _WIN32
    size_t model_bytes = 0;
    uint8_t *mapping = fg_win_map_model(gguf, &model_bytes);
    if (!mapping) goto error;
    const int loaded = model_bytes < 24 ? -1 : load_weights(m, mapping, model_bytes);
    UnmapViewOfFile(mapping);
#else
    int fd = open(gguf, O_RDONLY);
    if (fd < 0) { perror(gguf); goto error; }
    struct stat info;
    if (fstat(fd, &info) != 0 || info.st_size < 24) { close(fd); goto error; }
    uint8_t *mapping = mmap(NULL, info.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (mapping == MAP_FAILED) { perror("mmap"); goto error; }
    const int loaded = load_weights(m, mapping, (size_t)info.st_size);
    munmap(mapping, info.st_size);
#endif
    if (loaded != 0) goto error;
    const char *device_choice = getenv("BONSAI_HTTP_DEVICE_CHOICE");
    if (device_choice && !strcmp(device_choice, "1") && choice_open(m) != 0) goto error;
    if (m->mtp_enabled) {
        bonsai_mtp_goose_init(0, NULL);
        if (!m->choice_mask || draft_open(m)) goto error;
    }
    ffn_graph_open(m);
    size_t available = 0, total = 0;
    if (!check(bonsai_cuda.fn_cuMemGetInfo(&available, &total), "free VRAM")) {
        fprintf(stderr, "Device free: " BONSAI_SIZE_FMT " / " BONSAI_SIZE_FMT " bytes\n", available, total);
    }
    return m;
error:
    if (m->cuda) bonsai_cuda.fn_cuCtxDestroy(m->cuda);
    free(m);
    return NULL;
}

_Static_assert(
    offsetof(model, ubatch) == 14740 && offsetof(model, context) == 14732 &&
    offsetof(model, position) == 14736 && offsetof(model, wave) == 14748 &&
    offsetof(model, count) == 14744 && offsetof(model, rope_q) == 5672 &&
    offsetof(model, rope_k) == 5680 && offsetof(model, packed_source) == 14648 &&
    offsetof(model, q8_input) == 4704 && offsetof(model, ffn_q8_input) == 4760 &&
    offsetof(model, ffn_layer) == 4776 && offsetof(model, down_q8_owner) == 4768 &&
    offsetof(model, positions) == 5664 && offsetof(model, attention) == 5688 &&
    offsetof(model, token_ids) == 14624 && offsetof(model, capture.layer) == 16324 &&
    offsetof(model, txn.phase) == 312 && offsetof(model, txn.start) == 316 &&
    offsetof(model, txn.next_layer) == 324 && offsetof(model, txn.layers) == 320 &&
    offsetof(model, bound) == 5704 && offsetof(model, states) == 13384 &&
    offsetof(model, globals) == 14408 && offsetof(model, b) == 14432 &&
    offsetof(model, capture.phase) == 16320 && offsetof(model, capture.enabled) == 16332 &&
    offsetof(model, in_decode) == 4716,
    "Bonsai batch field layout");
_Static_assert(TXN_IDLE == 0 && TXN_RUNNING == 1 && TXN_READY == 2 && LAYERS == 64 &&
    VOCAB == 248320 && UBATCH_MAX == 1024 && KV_BYTES == 576 && ATTENTION_BYTES == 33554432, "Bonsai batch constants");

int32_t fg_bonsai_batch_rope(uint64_t q, uint64_t k, uint32_t count) {
    return rope_config(q, 24, count) || rope_config(k, 4, count);
}

int32_t fg_bonsai_batch_upload(uint64_t destination, const void *source, size_t size, uint32_t role) {
    const char *messages[] = {"positions", "attention configuration", "token ids"};
    return check(bonsai_cuda.fn_cuMemcpyHtoD(destination, source, size), messages[role]);
}

int32_t fg_bonsai_batch_verify(model *m, uint32_t count) { return exact_verify(m, count); }
int32_t fg_bonsai_batch_init_verify(model *m) { return init_verify_batch(m); }

void fg_bonsai_batch_graph_error(model *m, int64_t status) {
    if (m->capture.stream) ffn_graph_abort(m);
    fprintf(stderr, "Goose graph failed: %lld\n", (long long)status);
}

int32_t fg_bonsai_batch_download(void *output, uint64_t source) {
    return check(bonsai_cuda.fn_cuMemcpyDtoH(output, source, VOCAB * sizeof(float)), "logits");
}

extern int evaluate_batch_output(model *m, const unsigned *tokens, unsigned count, float *logits, int device_only);

static int evaluate_batch(model *m, const unsigned *tokens, unsigned count, float *logits) {
    return evaluate_batch_output(m, tokens, count, logits, 0);
}

#include "session.h"

struct bonsai_session {
    model *runtime;
    int needs_reset;
};

struct bonsai_session *bonsai_session_open(const char *gguf, const char *cubin,
    const char *frozen_ptx, unsigned context, unsigned ubatch) {
    if (!gguf || ubatch == 0 || ubatch > UBATCH_MAX) return NULL;
    struct bonsai_session *session = calloc(1, sizeof(*session));
    if (!session) return NULL;
    goose_init(0, NULL);
    session->runtime = bonsai_open(gguf, cubin, frozen_ptx, context, ubatch);
    if (!session->runtime) {
        free(session);
        return NULL;
    }
    CUcontext current;
    if (check(bonsai_cuda.fn_cuCtxPopCurrent(&current), "detach Bonsai context")) {
        bonsai_session_close(session);
        return NULL;
    }
    return session;
}

void bonsai_session_close(struct bonsai_session *session) {
    if (!session) return;
    ffn_graph_close(session->runtime);
    (void) check(bonsai_cuda.fn_cuCtxDestroy(session->runtime->cuda), "destroy Bonsai context");
    free(session->runtime);
    free(session);
}

static int finish_session_call(struct bonsai_session *session, int status) {
    CUcontext current;
    if (check(bonsai_cuda.fn_cuCtxPopCurrent(&current), "restore caller context")) status = -1;
    session->needs_reset = status != 0;
    return status;
}

int bonsai_session_reset(struct bonsai_session *session) {
    if (!session) return -1;
    model *m = session->runtime;
    if (check(bonsai_cuda.fn_cuCtxPushCurrent(m->cuda), "bind Bonsai context")) return -1;
    int status = 0;
    for (unsigned layer = 0; layer < LAYERS && !status; ++layer) {
        if (m->bound[15 * layer] == 0) continue;
        status = check(bonsai_cuda.fn_cuMemsetD8(m->states[2 * layer], 0, 4 * GDN_HISTORY), "reset convolution");
        if (!status) {
            status = check(bonsai_cuda.fn_cuMemsetD8(m->states[2 * layer + 1], 0, 4 * GDN_STATE), "reset GDN");
        }
    }
    if (!status) status = check(bonsai_cuda.fn_cuMemsetD8(m->b[23], 0, 4 * GDN_STATE), "reset GDN scratch");
    if (!status && m->mtp_enabled) {
        status = check(bonsai_cuda.fn_cuMemsetD8(m->draft_carry, 0, DRAFT_HIDDEN_BYTES), "reset draft seed");
    }
    if (!status) {
        m->mtp_next = m->mtp_end = 0;
        m->mtp_request = 0;
        m->txn.phase = TXN_IDLE;
        /* Attention writes new rows before reading only the active prefix. */
        m->in_decode = 0;
        m->position = 0;
        m->count = 0;
        m->packed_source = 0;
    }
    status = finish_session_call(session, status);
    if (!status && m->position == 0) fputs("[bench-cold] reset position=0\n", stderr);
    return status;
}

int bonsai_session_step(struct bonsai_session *session, const uint32_t *tokens,
    unsigned count, float logits[BONSAI_SESSION_VOCAB]) {
    if (!session || session->needs_reset || !tokens || !count) return -1;
    model *m = session->runtime;
    if (count > m->context - m->position) return -1;
    for (unsigned i = 0; i < count; ++i) {
        if (tokens[i] >= VOCAB) return -1;
    }
    if (check(bonsai_cuda.fn_cuCtxPushCurrent(m->cuda), "bind Bonsai context")) return -1;
    int status = 0;
    for (unsigned at = 0; at < count && !status;) {
        const unsigned remaining = count - at;
        const unsigned available = remaining - (logits && remaining > 1);
        const unsigned batch = available < m->ubatch ? available : m->ubatch;
        status = evaluate_batch(m, tokens + at, batch, remaining == 1 ? logits : NULL);
        if (!status && m->mtp_request && !m->in_decode) status = draft_prefill(m, tokens + at, batch);
        at += batch;
    }
    status = finish_session_call(session, status);
    if (!status && logits && m->position == 32768) fputs("[bench-cold] prefill position=32768\n", stderr);
    return status;
}

int bonsai_session_step_phase(struct bonsai_session *session, const uint32_t *ids,
    unsigned count, float *logits, int decode) {
    session->runtime->in_decode = decode;
    const int status = bonsai_session_step(session, ids, count, logits);
    session->runtime->in_decode = 0;
    return status;
}

int bonsai_session_choice_available(const struct bonsai_session *session, const int64_t *eog, int64_t n) {
    if (!session || !session->runtime->choice_mask || !eog || n != session->runtime->eog_count) return 0;
    const model *m = session->runtime;
    for (int64_t i = 0; i < n; ++i) {
        if (eog[i] != m->eog[i]) return 0;
    }
    return 1;
}

int64_t bonsai_session_choose_next(struct bonsai_session *session, uint32_t id, int ignore_eos) {
    if (!session || session->needs_reset || !session->runtime->choice_mask || id >= VOCAB) return -1;
    model *m = session->runtime;
    if (m->position >= m->context ||
        check(bonsai_cuda.fn_cuCtxPushCurrent(m->cuda), "bind Bonsai choice context")) return -1;
    m->in_decode = 1;
    struct token_choice choice;
    const int status = evaluate_batch_output(m, &id, 1, NULL, 1) ||
                       choose_device(m, ignore_eos, &choice);
    m->in_decode = 0;
    if (finish_session_call(session, status)) return -1;
    return choice.invalid || choice.token >= VOCAB ? -2 : (int64_t)choice.token;
}

int bonsai_session_read_choice_logits(struct bonsai_session *session, float *logits) {
    if (!session || session->needs_reset || !logits) return -1;
    model *m = session->runtime;
    if (check(bonsai_cuda.fn_cuCtxPushCurrent(m->cuda), "bind Bonsai fallback context")) return -1;
    return finish_session_call(session, check(
        bonsai_cuda.fn_cuMemcpyDtoH(logits, m->b[19], VOCAB * sizeof(float)), "fallback logits"));
}

#include "http-mtp.inc"
