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

extern void fg_bonsai_bind_cuda(void);
int32_t fg_bonsai_bind_cuda_once(void) {
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    return pthread_once(&once, fg_bonsai_bind_cuda);
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

_Static_assert(offsetof(model, capture) == 15288 &&
    offsetof(model, q8_mode) == 4712 && offsetof(model, norm_wide) == 4720 &&
    CU_STREAM_NON_BLOCKING == 1 && CU_STREAM_CAPTURE_MODE_THREAD_LOCAL == 1, "Bonsai FFN driver ABI");


extern int32_t fg_bonsai_verify_launch(model *m, enum kernel kernel, uint32_t x, uint32_t y, uint32_t z,
                                       uint32_t threads, void **args);
#define launch fg_bonsai_verify_launch
extern int32_t fg_bonsai_launch_gdn(model *m, const CUdeviceptr *args, unsigned tokens);
extern int32_t fg_bonsai_flash_f32(model *m, CUdeviceptr q, CUdeviceptr k, CUdeviceptr v, CUdeviceptr dst);

_Static_assert(sizeof(CUdeviceptr) == 8 &&
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

_Static_assert(offsetof(model, use_flash_f32) == 14664 && offsetof(model, memory) == 5656 &&
    offsetof(model, phases) == 14632 && offsetof(model, packed) == 14640 && offsetof(model, weights) == 5648,
    "Bonsai workspace ABI");

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
_Static_assert(CHOICE_TILES == 77 && CHOICE_FINAL == 78 && CHOICE_JOBS == 243 &&
    MTP_EH == 79 && MTP_MAT == 80 && DRAFT_CONCAT == 84 && DRAFT_MAT_BATCH == 85, "Bonsai draft launch ABI");

extern const unsigned char gooseinfer_bonsai_pipeline_image[];
extern const unsigned char gooseinfer_bonsai_frozen_image[];
extern const unsigned char gooseinfer_bonsai_mtp_image[];

extern int32_t fg_bonsai_load_images(model *m, const char *cubin, const char *frozen, const char *mtp_image,
    const void *pipeline_image, const void *frozen_image, const void *draft_image);
_Static_assert(offsetof(model, module) == 4680 && offsetof(model, frozen) == 4688 &&
    offsetof(model, mtp_module) == 8 && offsetof(model, decode_kernel) == 4696 &&
    KERNEL_COUNT == 100 && CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES == 8, "Bonsai image loader ABI");

_Static_assert(
    offsetof(model, async_fragments) == 14680 && offsetof(model, async_mmq) == 14684 &&
    offsetof(model, compact_heads) == 14676 && offsetof(model, decay_reuse) == 14696 &&
    offsetof(model, decay_small) == 4780 && offsetof(model, decode_half2) == 4752 &&
    offsetof(model, decode_kernel) == 4696 && offsetof(model, decode_mma) == 4744 &&
    offsetof(model, decode_ordered_merge) == 4748 && offsetof(model, device_commit) == 76 &&
    offsetof(model, down_q8) == 4756 && offsetof(model, draft_q8_head) == 84 &&
    offsetof(model, ffn_q8) == 4724 && offsetof(model, ffn_tail) == 14700 &&
    offsetof(model, flash_grouped) == 14760 && offsetof(model, flash_panel) == 4784 &&
    offsetof(model, gdn_prefill_v32) == 14688 && offsetof(model, grouped_decode) == 14752 &&
    offsetof(model, k64_mmq) == 14704 && offsetof(model, matvec_rows2) == 14756 &&
    offsetof(model, mmq_cached) == 4788 && offsetof(model, mmq_half2_down) == 4812 &&
    offsetof(model, mmq_half_acc) == 4796 && offsetof(model, mmq_half_full) == 4800 &&
    offsetof(model, mmq_half_full6144) == 4804 && offsetof(model, mtp_enabled) == 16 &&
    offsetof(model, norm_wide) == 4720 && offsetof(model, ordered_merge) == 14672 &&
    offsetof(model, q8_full) == 4728 && offsetof(model, q8_mode) == 4712 &&
    offsetof(model, q8_pairload) == 4732 && offsetof(model, q8_panel) == 4736 &&
    offsetof(model, q8_spread) == 4740 && offsetof(model, stage_kv) == 14668 &&
    offsetof(model, use_flash_f32) == 14664 && offsetof(model, verify_attention) == 64 &&
    offsetof(model, verify_attn_tail) == 72 && offsetof(model, verify_merge) == 68 &&
    offsetof(model, verify_native) == 56 && offsetof(model, verify_q8) == 60 &&
    offsetof(model, verify_rows) == 52 && offsetof(model, wide_mmq) == 14692,
    "Bonsai profile ABI");
_Static_assert(sizeof(model) == 16344 && offsetof(model, epsilon) == 16336 && CONTEXT_MIN == 1 && POSITIONS_MAX == 262144, "Bonsai model loader ABI");

int32_t fg_bonsai_embedded_images(model *m, const char *cubin, const char *frozen, const char *mtp_image) {
    return fg_bonsai_load_images(m, cubin, frozen, mtp_image, gooseinfer_bonsai_pipeline_image,
        gooseinfer_bonsai_frozen_image, gooseinfer_bonsai_mtp_image);
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
_Static_assert(GDN_STATE * 4 == 3145728 && GDN_HISTORY * 4 == 122880 &&
    offsetof(struct token_choice, token) == 0 && offsetof(struct token_choice, invalid) == 8, "Bonsai session state ABI");

_Static_assert(offsetof(model, mtp_pending) == 24 && offsetof(model, verify_rows) == 52 &&
    offsetof(model, draft_chain) == 296 && VERIFY_ROWS == 5, "Bonsai MTP session ABI");
