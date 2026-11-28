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

_Static_assert(offsetof(struct bonsai_ffn_capture, stream) == 0 &&
    offsetof(struct bonsai_ffn_capture, graphs) == 8 && offsetof(struct bonsai_ffn_capture, norm) == 520 &&
    offsetof(struct bonsai_ffn_capture, phase) == 1032 && offsetof(struct bonsai_ffn_capture, layer) == 1036 &&
    offsetof(struct bonsai_ffn_capture, next) == 1040 && offsetof(struct bonsai_ffn_capture, enabled) == 1044 &&
    LAYERS == 64, "Bonsai FFN capture ABI");

extern void bonsai_capture_abort(uint64_t capture);
extern int32_t bonsai_capture_begin(uint64_t capture, uint64_t norm);
extern int32_t bonsai_capture_finish(uint64_t capture);

extern void fg_bonsai_bind_ffn_graph(void);
extern void fg_bonsai_ffn_graph_open(model *m);
int32_t fg_bonsai_ffn_bind_once(void) {
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    return pthread_once(&once, fg_bonsai_bind_ffn_graph);
}

_Static_assert(offsetof(struct bonsai_cuda_api, library) == 152 && offsetof(model, capture) == 15288 &&
    offsetof(model, q8_mode) == 4712 && offsetof(model, norm_wide) == 4720 &&
    CU_STREAM_NON_BLOCKING == 1 && CU_STREAM_CAPTURE_MODE_THREAD_LOCAL == 1, "Bonsai FFN driver ABI");


extern int32_t fg_bonsai_verify_launch(model *m, enum kernel kernel, uint32_t x, uint32_t y, uint32_t z,
                                       uint32_t threads, void **args);
#define launch fg_bonsai_verify_launch
extern int32_t fg_bonsai_launch_gdn(model *m, const CUdeviceptr *args, unsigned tokens);
extern int32_t fg_bonsai_flash_f32(model *m, CUdeviceptr q, CUdeviceptr k, CUdeviceptr v, CUdeviceptr dst);

_Static_assert(offsetof(struct bonsai_cuda_api, fn_cuLaunchKernel) == 144 && sizeof(CUdeviceptr) == 8 &&
    offsetof(model, kernel) == 4824 && offsetof(model, gdn_prefill_v32) == 14688 &&
    offsetof(model, capture.next) == 16328 && offsetof(model, flash_fp8_pv) == 4816 &&
    offsetof(model, stage_k) == 14712 && offsetof(model, stage_v) == 14720 &&
    offsetof(model, stage_kv) == 14668 && offsetof(model, ordered_merge) == 14672 &&
    offsetof(model, compact_heads) == 14676 && offsetof(model, async_fragments) == 14680 &&
    offsetof(model, flash_grouped) == 14760 && offsetof(model, flash_panel) == 4784 &&
    offsetof(model, flash_fused_ordered) == 4792 && offsetof(model, flash_fp8_qk) == 4808 &&
    offsetof(model, stage_capacity_chunks) == 14728, "Bonsai launch ABI");
_Static_assert(NORM_WIDE_SMALL == 47 && ROTATE == 1 && MV_PACK_Q8 == 44 && MV_Q8_2 == 46 &&
    SWIGLU == 13 && ADD == 3 && GDN_V32 == 32 && GDN_BATCH == 15 && GDN == 6 &&
    FLASH_K_FP8 == 61 && FLASH_V_ONLY == 62 && FLASH_FP8_QK == 63 && FLASH_STAGE_KV == 25 &&
    FLASH_FUSED_ORDERED == 56 && FLASH_TILE_F32 == 22 && FLASH_KV_PANEL == 53 &&
    FLASH_GROUP4 == 43 && FLASH_GROUP2 == 42 && FLASH_TILE_ASYNC == 30 &&
    FLASH_TILE_COMPACT == 27 && FLASH_TILE_STAGED == 24 && FLASH_MERGE_COMPACT_ORDERED == 29 &&
    FLASH_MERGE_COMPACT == 28 && FLASH_MERGE_ORDERED == 26 && FLASH_MERGE_F32 == 23 &&
    FLASH_V_FP8 == 66 && FLASH_FP8_QKPV == 67 && FLASH_PANEL_SHARED == 50176, "Bonsai launch kernel IDs");

extern int32_t fg_bonsai_batch_verify(model *m, uint32_t count);
extern int32_t fg_bonsai_launch_verify_rows(model *m, enum kernel op, const CUdeviceptr *args);
extern int32_t fg_bonsai_transaction_conv(model *m, const CUdeviceptr *args);
extern int32_t fg_bonsai_transaction_gdn(model *m, const CUdeviceptr *args);

_Static_assert(offsetof(model, txn.layer_ids) == 328 && offsetof(model, txn.prefix) == 584 &&
    offsetof(model, verify_native) == 56 && offsetof(model, verify_q8) == 60 &&
    offsetof(model, partial_capacity) == 14656 && offsetof(model, partial) == 5696 &&
    offsetof(model, verify_positions) == 280 && offsetof(model, verify_rope_q) == 264 &&
    offsetof(model, verify_rope_k) == 272 && offsetof(model, decay_small) == 4780 &&
    offsetof(model, grouped_decode) == 14752 && offsetof(model, decode_mma) == 4744 &&
    offsetof(model, decode_half2) == 4752 && offsetof(model, decode_ordered_merge) == 4748 &&
    offsetof(model, verify_attention) == 64 && offsetof(model, verify_attn_tail) == 72 &&
    offsetof(model, verify_merge) == 68, "Bonsai verify ABI");
_Static_assert(VERIFY_CONV_FIVE == 96 && VERIFY_CONV_FOUR == 92 && VERIFY_CONV == 83 &&
    VERIFY_ATTN == 86 && VERIFY_ATTN_TAIL == 88 && VERIFY_ATTN_FOUR == 91 && VERIFY_ATTN_FIVE == 95 &&
    VERIFY_MERGE == 87 && MERGE == 14 && MATRIX == 2 && DECAY_STAGE256 == 51 && DECAY == 4 &&
    CONV == 5 && ROPE == 8 && DECODE_DIRECT_HALF2 == 74 && DECODE_DIRECT_MMA == 72 &&
    DECODE_GQA3 == 40 && DECODE_GQA2 == 39 && TILES == 10 && DECODE_ORDERED_MERGE == 73,
    "Bonsai verify kernel IDs");

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
        if (op == CONV) return fg_bonsai_transaction_conv(m, p);
        if (op == GDN) return fg_bonsai_transaction_gdn(m, p);
    }
    if (fg_bonsai_batch_verify(m, count) &&
        ((op == MATRIX && e == 1024) || op == DECAY || op == ROPE || op == TILES))
        return fg_bonsai_launch_verify_rows(m, (enum kernel)op, p);
    switch (op) {
        case NORM:
            if (m->capture.enabled && m->in_decode && count == 1 &&
                    p[0] == (CUdeviceptr)m->b[13] && p[3] == (CUdeviceptr)m->b[1] &&
                    bonsai_capture_begin((uintptr_t)&m->capture, p[1])) return -1;
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
            if (fg_bonsai_batch_verify(m, count) &&
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
            return bonsai_capture_finish((uintptr_t)&m->capture);
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
            return fg_bonsai_launch_gdn(m, p, count);
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
                const int rc = fg_bonsai_flash_f32(m, p[0], p[1], p[2], p[3]);
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

extern int32_t bonsai_rope_config(uint64_t destination, int64_t heads, int64_t tokens);

_Static_assert(offsetof(model, use_flash_f32) == 14664 && offsetof(model, memory) == 5656 &&
    offsetof(model, phases) == 14632 && offsetof(model, packed) == 14640 && offsetof(model, weights) == 5648,
    "Bonsai workspace ABI");

extern int32_t bonsai_load_weights_plan(uint64_t context, sl_u8 file, uint64_t mapping, uint64_t length,
    uint8_t mtp_enabled, sl_i64 globals, sl_i64 bound, sl_i64 draft, sl_i64 draft_layer,
    sl_i64 eog, sl_f32 epsilon, sl_i64 result);

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

extern int32_t fg_bonsai_choice_open(model *m);
_Static_assert(CHOICE_MASK_WORDS == 7760 && CHOICE_JOBS == 243, "Bonsai choice workspace ABI");

#include "mtp-abi.h"
extern int32_t fg_bonsai_draft_open(model *m);
_Static_assert(offsetof(model, choice_tiles) == 5632 && offsetof(model, choice_result) == 5640 &&
    offsetof(model, mtp) == 96 && offsetof(model, mtp_layer) == 128 && offsetof(model, draft_kv) == 248 &&
    offsetof(model, draft_rope_k) == 304 && offsetof(model, draft_q8_head) == 84 &&
    offsetof(model, draft_batch) == 88 && offsetof(model, device_commit) == 76, "Bonsai draft ABI");
_Static_assert(offsetof(struct bonsai_cuda_api, fn_cuMemAlloc) == 80 &&
    offsetof(struct bonsai_cuda_api, fn_cuMemcpyDtoDAsync) == 104 &&
    CHOICE_TILES == 77 && CHOICE_FINAL == 78 && CHOICE_JOBS == 243 &&
    MTP_EH == 79 && MTP_MAT == 80 && DRAFT_CONCAT == 84 && DRAFT_MAT_BATCH == 85, "Bonsai draft launch ABI");

extern const unsigned char gooseinfer_bonsai_pipeline_image[];
extern const unsigned char gooseinfer_bonsai_frozen_image[];
extern const unsigned char gooseinfer_bonsai_mtp_image[];

static CUresult bonsai_load_module(CUmodule *module, const char *path, const unsigned char *image) {
    return path && *path ? bonsai_cuda.fn_cuModuleLoad(module, path) :
        bonsai_cuda.fn_cuModuleLoadData(module, image);
}

model *fg_bonsai_model_open(const char *gguf, const char *cubin, const char *frozen, unsigned context, unsigned ubatch) {
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
    if (device_choice && !strcmp(device_choice, "1") && fg_bonsai_choice_open(m) != 0) goto error;
    if (m->mtp_enabled) {
        bonsai_mtp_goose_init(0, NULL);
        if (!m->choice_mask || fg_bonsai_draft_open(m)) goto error;
    }
    fg_bonsai_ffn_graph_open(m);
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

_Static_assert(offsetof(struct bonsai_cuda_api, fn_cuMemcpyHtoD) == 96 &&
    offsetof(struct bonsai_cuda_api, fn_cuMemcpyDtoH) == 112, "Bonsai CUDA copy dispatch layout");

const void *fg_bonsai_cuda_dispatch(void) { return &bonsai_cuda; }
int32_t fg_bonsai_cuda_check(CUresult status, const char *where) { return check(status, where); }


void fg_bonsai_batch_graph_error(model *m, int64_t status) {
    if (m->capture.stream) bonsai_capture_abort((uintptr_t)&m->capture);
    fprintf(stderr, "Goose graph failed: %lld\n", (long long)status);
}

#include "session.h"

struct bonsai_session {
    model *runtime;
    int needs_reset;
};

_Static_assert(sizeof(struct bonsai_session) == 16 && UBATCH_MAX == 1024 &&
    offsetof(struct bonsai_session, runtime) == 0 && offsetof(struct bonsai_session, needs_reset) == 8 &&
    offsetof(model, cuda) == 0 && offsetof(model, draft_carry) == 288 && offsetof(model, mtp_next) == 44 &&
    offsetof(model, mtp_end) == 48 && offsetof(model, mtp_request) == 20 && offsetof(model, mtp_enabled) == 16 &&
    offsetof(model, choice_mask) == 5624 && offsetof(model, eog_count) == 15280 && offsetof(model, eog) == 14768,
    "Bonsai session field layout");
_Static_assert(offsetof(struct bonsai_cuda_api, fn_cuCtxDestroy) == 24 &&
    offsetof(struct bonsai_cuda_api, fn_cuCtxPushCurrent) == 32 &&
    offsetof(struct bonsai_cuda_api, fn_cuCtxPopCurrent) == 40 &&
    offsetof(struct bonsai_cuda_api, fn_cuMemsetD8) == 120, "Bonsai session CUDA dispatch layout");
_Static_assert(GDN_STATE * 4 == 3145728 && GDN_HISTORY * 4 == 122880 &&
    offsetof(struct token_choice, token) == 0 && offsetof(struct token_choice, invalid) == 8, "Bonsai session state ABI");

_Static_assert(offsetof(model, mtp_pending) == 24 && offsetof(model, verify_rows) == 52 &&
    offsetof(model, draft_chain) == 296 && VERIFY_ROWS == 5, "Bonsai MTP session ABI");
