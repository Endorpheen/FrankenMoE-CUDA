#include <stddef.h>

struct verify_position {
    int32_t rope[4];
    int64_t attention[22];
};

static int exact_verify(const model *m, unsigned count) {
    return m->verify_native && m->verify_q8 && m->in_decode &&
           m->q8_mode == 1 && count == VERIFY_ROWS;
}

static int init_verify_batch(model *m) {
    struct verify_position rows[VERIFY_ROWS];
    for (unsigned t = 0; t < VERIFY_ROWS; ++t) {
        const int64_t pos = m->position + t, keys = pos + 1;
        const int64_t tiles = (keys + 255) / 256;
        rows[t] = (struct verify_position){
            {pos, pos, pos, 0},
            {256, 256, 1, 24, 4, 4, keys, 24576, 1024, KV_BYTES, 144, KV_BYTES, 144,
             0, 1024, 24576, 256, tiles, 24, 24, 24 * tiles * 258 * 4, keys},
        };
    }
    return check(bonsai_cuda.fn_cuMemcpyHtoD(m->verify_positions, rows, sizeof(rows)), "verify row configs");
}

static int launch_verify_rows(model *m, enum kernel op, const CUdeviceptr p[8]) {
    const unsigned narrow = 1024;
    for (unsigned t = 0; t < VERIFY_ROWS; ++t) {
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
            const int tile_status = m->grouped_decode
                ? launch(m, heads == 3 ? DECODE_GQA3 : DECODE_GQA2, 24 / heads, tiles, 1, 32,
                         (void *[]){ &row[0], &row[1], &row[2], &m->partial, &keys, &tiles, &scale })
                : launch(m, TILES, 24, tiles, 1, 32,
                         (void *[]){ &row[0], &row[1], &row[2], &no_mask, &m->partial, &cfg, &scale, &first });
            if (tile_status) return -1;
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
