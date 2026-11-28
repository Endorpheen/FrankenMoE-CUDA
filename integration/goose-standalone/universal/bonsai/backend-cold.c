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

_Static_assert(sizeof(pthread_mutex_t) <= 40 && _Alignof(pthread_mutex_t) <= 8, "Bonsai initialization mutex ABI");
#ifdef _WIN32
_Static_assert(PTHREAD_MUTEX_INITIALIZER == -1 && sizeof(pthread_mutex_t) == 8, "Bonsai Windows mutex initializer");
#endif

#ifdef _WIN32
#define BONSAI_SIZE_FMT "%llu"
#else
#define BONSAI_SIZE_FMT "%zu"
#endif

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

_Static_assert(offsetof(model, capture) == 15288 &&
    offsetof(model, q8_mode) == 4712 && offsetof(model, norm_wide) == 4720 &&
    CU_STREAM_NON_BLOCKING == 1 && CU_STREAM_CAPTURE_MODE_THREAD_LOCAL == 1, "Bonsai FFN driver ABI");

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

_Static_assert(offsetof(model, down_q8) == 4756 && FFN_TAIL == 36 && FFN_TAIL_Q8_PANEL == 75, "Bonsai FFN tail ABI");

_Static_assert(
    ADD == 3 && DECAY == 4 && CONV == 5 && GDN == 6 &&
    SPLIT == 7 && ROPE == 8 && Q4 == 9 && TILES == 10 &&
    GATE == 11 && EMBED == 12 && SWIGLU == 13 && DECAY_STAGE256 == 51 &&
    DECAY_WARPPACK == 52 && DECAY_REUSE8 == 35 && DECAY_REUSE4 == 34 && CONV_BATCH == 19 &&
    ROPE_PHASES == 16 && ROPE_APPLY == 17 && EMBED_BATCH == 18, "Bonsai elementary kernel IDs");
_Static_assert(NORM == 0 && NORM_WIDE_LARGE == 48, "Bonsai normalization kernel IDs");
_Static_assert(
    NORM == 0 && ROTATE == 1 && MATRIX == 2 && DECAY == 4 &&
    CONV == 5 && GDN == 6 && ROPE == 8 && TILES == 10 &&
    PACK_HALF == 20 && MMQ == 21 && MMQ_ASYNC == 31 && MMQ_WIDE == 33 &&
    MMQ_K64_64 == 37 && MMQ_K64_128 == 38 && MATRIX_ROWS2 == 41 && MV_PACK_Q8 == 44 &&
    MV_Q8_1 == 45 && MV_Q8_2 == 46 && FFN_Q8_PACK == 49 && FFN_Q8_MMQ == 50 &&
    MMQ_CACHE64 == 54 && MMQ_CACHE128 == 55 && MMQ_HALF_ACC32 == 57 && MMQ_HALF_FULL5120 == 58 &&
    MMQ_HALF_FULL17408 == 59 && MMQ_HALF_FULL6144 == 60 && MMQ_HALF_FULL_DOWN_HALF2 == 64 && FFN_Q8_FULL128 == 65 &&
    FFN_Q8_PAIRLOAD == 68 && FFN_Q8_PANEL_PACK == 69 && FFN_Q8_PANEL == 70 && FFN_Q8_SPREAD == 71 &&
    FFN_DOWN_Q8_PANEL == 76 && VERIFY_Q8_1 == 81 && VERIFY_Q8_2 == 82 && VERIFY_Q8_FOUR_1 == 89 &&
    VERIFY_Q8_FOUR_2 == 90 && VERIFY_Q8_FIVE_1 == 93 && VERIFY_Q8_FIVE_2 == 94 && VERIFY_MVQ_FOUR_PAIR == 97 &&
    VERIFY_MVQ_FOUR_QUAD == 98 && VERIFY_MVQ_FIVE_QUAD == 99, "Bonsai matrix dispatch IDs");
_Static_assert(offsetof(model, matvec_rows2) == 14756 && offsetof(model, ffn_q8) == 4724, "Bonsai matrix dispatch ABI");

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

_Static_assert(CHOICE_MASK_WORDS == 7760 && CHOICE_JOBS == 243, "Bonsai choice workspace ABI");

#include "mtp-abi.h"
_Static_assert(offsetof(model, choice_tiles) == 5632 && offsetof(model, choice_result) == 5640 &&
    offsetof(model, mtp) == 96 && offsetof(model, mtp_layer) == 128 && offsetof(model, draft_kv) == 248 &&
    offsetof(model, draft_rope_k) == 304 && offsetof(model, draft_q8_head) == 84 &&
    offsetof(model, draft_batch) == 88 && offsetof(model, device_commit) == 76, "Bonsai draft ABI");
_Static_assert(CHOICE_TILES == 77 && CHOICE_FINAL == 78 && CHOICE_JOBS == 243 &&
    MTP_EH == 79 && MTP_MAT == 80 && DRAFT_CONCAT == 84 && DRAFT_MAT_BATCH == 85, "Bonsai draft launch ABI");

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
