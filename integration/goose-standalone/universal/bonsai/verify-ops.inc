#include <stddef.h>

struct verify_position {
    int32_t rope[4];
    int64_t attention[22];
};

static int exact_verify(const model *m, unsigned count) {
    return m->verify_native && m->verify_q8 && m->in_decode &&
           m->q8_mode == 1 && count == m->verify_rows;
}

extern uint8_t bonsai_verify_position_packets(uint64_t position, int64_t count, bonsai_words packets);

static int init_verify_batch(model *m) {
    _Static_assert(sizeof(struct verify_position) == 192 && offsetof(struct verify_position, attention) == 16 &&
                   KV_BYTES == 576, "Bonsai verify position ABI");
    uint64_t packets[VERIFY_ROWS * 24];
    if (!bonsai_verify_position_packets(m->position, m->verify_rows,
            (bonsai_words){packets, m->verify_rows * 24})) return -1;
    return check(bonsai_cuda.fn_cuMemcpyHtoD(m->verify_positions, packets,
            m->verify_rows * sizeof(struct verify_position)), "verify row configs");
}

extern int32_t bonsai_verify_attention(uint64_t context, bonsai_const_words pointers, uint32_t rows,
                                       uint32_t first_keys, uint32_t max_tiles, uint8_t tail, uint8_t fused_merge);

int32_t bonsai_verify_attn_launch(uint64_t context, uint32_t role, uint32_t x, uint32_t y, uint32_t z,
                                  uint32_t threads, uint64_t args) {
    static const enum kernel kernels[] = {VERIFY_ATTN, VERIFY_ATTN_TAIL, VERIFY_ATTN_FOUR, VERIFY_ATTN_FIVE,
                                           VERIFY_MERGE, MERGE, MATRIX, DECAY_STAGE256, DECAY, CONV, ROPE,
                                           DECODE_DIRECT_HALF2, DECODE_DIRECT_MMA, DECODE_GQA3, DECODE_GQA2,
                                           TILES, DECODE_ORDERED_MERGE};
    if (role >= sizeof(kernels) / sizeof(*kernels)) return -1;
    return launch((model *)(uintptr_t)context, kernels[role], x, y, z, threads, (void *)(uintptr_t)args);
}

_Static_assert(MATRIX == 2 && DECAY == 4 && CONV == 5 && GDN == 6 && ROPE == 8 && TILES == 10,
               "Bonsai verify operation ABI");

extern int32_t bonsai_verify_launch_rows(uint64_t context, int32_t operation,
                                       bonsai_const_words input, bonsai_const_words config);

static int launch_verify_rows(model *m, enum kernel op, const CUdeviceptr p[8]) {
    const uint64_t flags = (m->decay_small ? 1 : 0) | (m->grouped_decode ? 2 : 0) | (m->decode_mma ? 4 : 0) |
        (m->decode_mma == 2 ? 8 : 0) | (m->decode_half2 ? 16 : 0) | (m->decode_ordered_merge ? 32 : 0) |
        (m->verify_attention ? 64 : 0) | (m->verify_attn_tail ? 128 : 0) | (m->verify_merge ? 256 : 0);
    const uint64_t config[] = {m->position, m->verify_rows, flags, m->partial_capacity,
        m->partial, m->verify_positions, m->verify_rope_q, m->verify_rope_k};
    return bonsai_verify_launch_rows((uintptr_t)m, op, (bonsai_const_words){(const uint64_t *)p, 8},
                                    (bonsai_const_words){config, 8});
}
