#include <stddef.h>

struct verify_position {
    int32_t rope[4];
    int64_t attention[22];
};

static int exact_verify(const model *m, unsigned count) {
    return m->verify_native && m->verify_q8 && m->in_decode &&
           m->q8_mode == 1 && count == m->verify_rows;
}

static int init_verify_batch(model *m) {
    struct verify_position rows[VERIFY_ROWS];
    for (unsigned t = 0; t < m->verify_rows; ++t) {
        const int64_t pos = m->position + t, keys = pos + 1;
        const int64_t tiles = (keys + 255) / 256;
        rows[t] = (struct verify_position){
            {pos, pos, pos, 0},
            {256, 256, 1, 24, 4, 4, keys, 24576, 1024, KV_BYTES, 144, KV_BYTES, 144,
             0, 1024, 24576, 256, tiles, 24, 24, 24 * tiles * 258 * 4, keys},
        };
    }
    return check(bonsai_cuda.fn_cuMemcpyHtoD(m->verify_positions, rows, m->verify_rows * sizeof(*rows)), "verify row configs");
}

static int launch_shared_verify_attention(model *m, const CUdeviceptr p[8],
                                          unsigned first_keys, unsigned max_tiles) {
    CUdeviceptr q = p[0], k = p[1], v = p[2];
    float scale = 1.0f / 16.0f;

    enum kernel attention = m->verify_attn_tail ? VERIFY_ATTN_TAIL : VERIFY_ATTN;
    unsigned groups = 12, panels = 1;
    if (m->verify_rows == 4) {
        attention = VERIFY_ATTN_FOUR;
        groups = 8;
        panels = 2;
    } else if (m->verify_rows == 5) {
        attention = VERIFY_ATTN_FIVE;
        panels = 2;
    }
    if (launch(m, attention, groups, max_tiles, panels, 32,
               (void *[]){ &q, &k, &v, &m->partial, &first_keys, &max_tiles, &scale })) return -1;
    if (m->verify_merge) {
        CUdeviceptr output = p[3];
        return launch(m, VERIFY_MERGE, m->verify_rows * 24, 1, 1, 128,
                      (void *[]){ &m->partial, &output, &first_keys, &max_tiles });
    }
    const size_t partial_bytes = (size_t)24 * max_tiles * 258 * sizeof(float);
    for (unsigned t = 0; t < m->verify_rows; ++t) {
        CUdeviceptr partial = m->partial + t * partial_bytes;
        CUdeviceptr output = p[3] + (size_t)t * 6144 * sizeof(float);
        CUdeviceptr cfg = m->verify_positions + t * sizeof(struct verify_position) +
                          offsetof(struct verify_position, attention);
        int start = 0;
        if (launch(m, MERGE, 24, 1, 1, 32, (void *[]){ &partial, &output, &cfg, &start })) return -1;
    }
    return 0;
}

static int launch_verify_rows(model *m, enum kernel op, const CUdeviceptr p[8]) {
    if (op == TILES && !m->decode_mma && m->verify_attention && m->grouped_decode) {
        const unsigned first_keys = m->position + 1;
        const unsigned max_tiles = (first_keys + m->verify_rows - 1 + 255) / 256;
        const size_t bytes = (size_t)m->verify_rows * 24 * max_tiles * 258 * sizeof(float);
        if (bytes <= m->partial_capacity) {
            return launch_shared_verify_attention(m, p, first_keys, max_tiles);
        }
    }
    const unsigned narrow = 1024;
    for (unsigned t = 0; t < m->verify_rows; ++t) {
        CUdeviceptr row[8];
        memcpy(row, p, sizeof(row));
        int status = -1;
        switch (op) {
        case MATRIX: {
            unsigned width = (unsigned)p[3], output = (unsigned)p[4];
            if (output != narrow) return -1;
            row[1] += (size_t)t * width * sizeof(float);
            row[2] += (size_t)t * output * sizeof(float);
            status = launch(m, MATRIX, (output+3)/4, 1, 1, 128,
                            (void *[]){ &row[0], &row[1], &row[2], &width, &output });
            break;
        }
        case DECAY:
            row[0] += (size_t)t * 5120 * sizeof(float);
            row[5] += (size_t)t * 48 * sizeof(float);
            row[6] += (size_t)t * 48 * sizeof(float);
            status = launch(m, m->decay_small ? DECAY_STAGE256 : DECAY, 48, 1, 1,
                            m->decay_small ? 256 : 32,
                            (void *[]){ &row[0], &row[1], &row[2], &row[3], &row[4], &row[5], &row[6] });
            break;
        case CONV:
            row[0] += (size_t)t * 10240 * sizeof(float);
            row[3] += (size_t)t * 10240 * sizeof(float);
            status = launch(m, CONV, 40, 1, 1, 256, (void *[]){ &row[0], &row[1], &row[2], &row[3] });
            break;
        case GDN:
            row[0] += (size_t)t * 4096 * sizeof(float);
            row[1] += (size_t)t * 4096 * sizeof(float);
            row[2] += (size_t)t * 10240 * sizeof(float);
            row[3] += (size_t)t * 48 * sizeof(float);
            row[4] += (size_t)t * 48 * sizeof(float);
            row[6] += (size_t)t * 6144 * sizeof(float);
            if (t % 2) {
                row[5] = p[7];
                row[7] = p[5];
            }
            status = gdn(m, row, 1);
            break;
        case ROPE: {
            const unsigned heads = (unsigned)p[2];
            CUdeviceptr conf = heads == 24 ? m->verify_rope_q : m->verify_rope_k;
            CUdeviceptr positions = m->verify_positions + t * sizeof(struct verify_position);
            CUdeviceptr no_factors = 0;
            row[0] += (size_t)t * heads * 256 * sizeof(float);
            row[1] += (size_t)t * heads * 256 * sizeof(float);
            status = launch(m, ROPE, heads, 1, 1, 128,
                            (void *[]){ &row[0], &positions, &no_factors, &row[1], &conf });
            break;
        }
        case TILES: {
            row[0] += (size_t)t * 6144 * sizeof(float);
            row[3] += (size_t)t * 6144 * sizeof(float);
            unsigned keys = m->position + t + 1, tiles = (keys + 255) / 256;
            unsigned heads = keys >= 32768 ? 3 : 2;
            int first = 0;
            float scale = 1.0f / 16.0f;
            CUdeviceptr cfg = m->verify_positions + t * sizeof(struct verify_position) +
                              offsetof(struct verify_position, attention);
            CUdeviceptr no_mask = 0;
            const int direct = m->grouped_decode && m->decode_mma &&
                               (keys >= 32768 || m->decode_mma == 2);
            const int tile_status = direct
                ? launch(m, m->decode_half2 ? DECODE_DIRECT_HALF2 : DECODE_DIRECT_MMA,
                         4, tiles, 1, 128, (void *[]){ &row[0], &row[1], &row[2], &m->partial, &keys, &tiles })
                : m->grouped_decode
                ? launch(m, heads == 3 ? DECODE_GQA3 : DECODE_GQA2, 24 / heads, tiles, 1, 32,
                         (void *[]){ &row[0], &row[1], &row[2], &m->partial, &keys, &tiles, &scale })
                : launch(m, TILES, 24, tiles, 1, 32,
                         (void *[]){ &row[0], &row[1], &row[2], &no_mask, &m->partial, &cfg, &scale, &first });
            if (tile_status) return -1;
            if (m->grouped_decode && m->decode_ordered_merge && keys >= 32768) {
                status = launch(m, DECODE_ORDERED_MERGE, 24, 1, 1, 128,
                                (void *[]){ &m->partial, &row[3], &keys, &tiles });
                break;
            }
            status = launch(m, MERGE, 24, 1, 1, 32,
                            (void *[]){ &m->partial, &row[3], &cfg, &first });
            break;
        }
        default:
            return -1;
        }
        if (status) return status;
    }
    return 0;
}
