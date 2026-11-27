#include "graph.h"
#include "upload.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { HIDDEN = 5120, GDN = 6144, FFN = 17408, SIGN_COUNT = HIDDEN + GDN + FFN,
       WEIGHT_BYTES = HIDDEN * FFN / 128 * 34, CHUNK = 4 << 20 };
enum { SIGN_H, SIGN_F, GATE_W, UP_W, DOWN_W, INPUT, ROTATED, GATE, UP, PRODUCT, FFN_ROTATED, OUTPUT, VIEWS };

static void fwht(float *out, const float *in, const int32_t *signs, unsigned width) {
    for (unsigned i = 0; i < width; ++i) out[i] = in[i] * signs[i];
    for (unsigned base = 0; base < width; base += 1024) {
        for (unsigned span = 1; span < 1024; span *= 2) {
            for (unsigned i = 0; i < 512; ++i) {
                unsigned at = base + (i / span) * (2 * span) + i % span;
                float a = out[at], b = out[at + span];
                out[at] = a + b;
                out[at + span] = a - b;
            }
        }
    }
    for (unsigned i = 0; i < width; ++i) out[i] *= 0.03125f;
}

static double half(const unsigned char *p) {
    unsigned code = p[0] | (unsigned)p[1] << 8;
    unsigned exponent = (code >> 10) & 31;
    if (exponent == 31) return NAN;
    unsigned fraction = code & 1023;
    double value = exponent ? ldexp(1024 + fraction, (int)exponent - 25) : ldexp(fraction, -24);
    return code >> 15 ? -value : value;
}

static void project(float *out, const unsigned char *weights, const float *in, unsigned width, unsigned rows) {
    for (unsigned row = 0; row < rows; ++row) {
        double sum = 0;
        for (unsigned block = 0; block < width / 128; ++block) {
            const unsigned char *p = weights + ((size_t)row * (width / 128) + block) * 34;
            const double scale = half(p);
            for (unsigned col = 0; col < 128; ++col) {
                const int code = (p[2 + col / 4] >> (2 * (col % 4))) & 3;
                sum += (code - 1) * scale * in[block * 128 + col];
            }
        }
        out[row] = (float)sum;
    }
}

static int upload_weights(fg_vk_device *vk, const fg_vk_buffer *buffer, size_t offset,
    unsigned char *packed, size_t size) {
    fg_vk_upload upload = {0};
    int ok = fg_vk_upload_open(&upload, vk, CHUNK);
    for (size_t at = 0; ok && at < size;) {
        const size_t count = size - at > CHUNK ? CHUNK : size - at;
        ok = fg_vk_upload_write(&upload, buffer, offset + at, packed + at, count);
        at += count;
    }
    fg_vk_upload_close(&upload);
    return ok;
}

static int compare(const float *out, const float *ref) {
    double squared_error = 0, squared_ref = 0, max_error = 0;
    for (unsigned i = 0; i < HIDDEN; ++i) {
        if (!isfinite(out[i]) || !isfinite(ref[i])) return 0;
        const double error = out[i] - (double)ref[i];
        squared_error += error * error;
        squared_ref += (double)ref[i] * ref[i];
        if (fabs(error) > max_error) max_error = fabs(error);
    }
    const double l2 = sqrt(squared_error / squared_ref);
    printf("Bonsai layer 0 FFN width=5120/17408 rows=5120 relative_l2=%g max_error=%g %s\n",
           l2, max_error, l2 < 5e-5 ? "PASS" : "FAIL");
    return l2 < 5e-5;
}

static int check_rotations_and_row(fg_vk_graph *graph, fg_vk_buffer *arena, fg_vk_view view[VIEWS],
    const float *input, const int32_t *signs, const unsigned char *packed) {
    fg_vk_device *vk = graph->vk;
    float actual[GDN], source[GDN], permuted[GDN], expected[GDN];
    for (unsigned i = 0; i < GDN; ++i) source[i] = ((int)(i % 43) - 21) / 16.0f;
    const fg_vk_shape inverse = {.width = HIDDEN, .batch = 1, .position = 1};
    const fg_vk_shape grouped = {.width = GDN, .rows = 128, .batch = 1, .kv_heads = 16, .context = 3};
    fg_vk_shape invalid = grouped;
    invalid.kv_heads = 15;
    if (fg_vk_access_buffer(vk, arena, view[UP].offset, source, sizeof(source), 1) != VK_SUCCESS ||
        fg_vk_access_buffer(vk, arena, view[SIGN_F].offset, (void *)(signs + HIDDEN), GDN * 4, 1) != VK_SUCCESS ||
        !fg_vk_graph_begin(graph) ||
        fg_vk_graph_record(graph, FG_VK_FWHT, invalid, view[UP], view[SIGN_F], view[PRODUCT]) ||
        !fg_vk_graph_record(graph, FG_VK_FWHT, inverse, view[ROTATED], view[SIGN_H], view[OUTPUT]) ||
        !fg_vk_graph_record(graph, FG_VK_FWHT, grouped, view[UP], view[SIGN_F], view[PRODUCT]) ||
        !fg_vk_graph_submit(graph) ||
        fg_vk_access_buffer(vk, arena, view[OUTPUT].offset, actual, HIDDEN * 4, 0) != VK_SUCCESS) {
        fputs("Bonsai inverse/grouped dispatch failed\n", stderr);
        return 0;
    }
    for (unsigned i = 0; i < HIDDEN; ++i) {
        if (input[i] != actual[i]) {
            fprintf(stderr, "Bonsai inverse mismatch at %u: device=%g input=%g\n", i, actual[i], input[i]);
            return 0;
        }
    }
    for (unsigned h = 0; h < 48; ++h) {
        memcpy(permuted + h * 128, source + (h % 3 * 16 + h / 3) * 128, 128 * sizeof(float));
    }
    fwht(expected, permuted, signs + HIDDEN, GDN);
    if (fg_vk_access_buffer(vk, arena, view[PRODUCT].offset, actual, sizeof(actual), 0) != VK_SUCCESS ||
        memcmp(actual, expected, sizeof(actual))) {
        fputs("Bonsai grouped FWHT mismatch\n", stderr);
        return 0;
    }
    enum { STRIPE = 16, FIRST_ROW = FFN - STRIPE, ROW_BYTES = HIDDEN / 128 * 34 };
    const fg_vk_shape gather = {.width = HIDDEN, .rows = STRIPE, .batch = 1, .position = FIRST_ROW};
    fg_vk_view weights = view[GATE_W];
    weights.offset += FIRST_ROW * ROW_BYTES;
    weights.size = STRIPE * ROW_BYTES;
    uint32_t token = FIRST_ROW + 1;
    if (fg_vk_access_buffer(vk, arena, view[INPUT].offset, &token, sizeof(token), 1) != VK_SUCCESS ||
        !fg_vk_graph_begin(graph) ||
        !fg_vk_graph_record(graph, FG_VK_GATHER_PQ2_0, gather, view[INPUT], weights, view[OUTPUT]) ||
        !fg_vk_graph_submit(graph) ||
        fg_vk_access_buffer(vk, arena, view[OUTPUT].offset, actual, HIDDEN * 4, 0) != VK_SUCCESS) {
        fputs("Bonsai PQ2 row gather dispatch failed\n", stderr);
        return 0;
    }
    for (unsigned i = 0; i < HIDDEN; ++i) {
        const unsigned char *p = packed + token * ROW_BYTES + (i / 128) * 34;
        const int c = (p[2 + i % 128 / 4] >> (2 * (i % 4))) & 3;
        if (actual[i] != (float)((c - 1) * half(p))) return 0;
    }
    puts("Bonsai inverse/grouped FWHT and offset PQ2 gather: exact PASS");
    return 1;
}

int test_bonsai(fg_vk_graph *graph, const char *path) {
    int32_t signs[SIGN_COUNT];
    FILE *fixture = fopen(path, "rb");
    unsigned char *packed = malloc(3 * WEIGHT_BYTES);
    int ok = fixture && packed && fread(signs, sizeof(*signs), SIGN_COUNT, fixture) == SIGN_COUNT &&
        fread(packed, 1, 3 * WEIGHT_BYTES, fixture) == 3 * WEIGHT_BYTES && fgetc(fixture) == EOF;
    if (fixture) fclose(fixture);
    if (!ok) {
        free(packed);
        fputs("Unable to read the Bonsai FFN fixture\n", stderr);
        return 0;
    }
    for (unsigned i = 0; i < SIGN_COUNT; ++i) {
        if (signs[i] != -1 && signs[i] != 1) { free(packed); return 0; }
    }

    fg_vk_device *vk = graph->vk;
    fg_vk_buffer arena = {0}, weights = {0};
    const VkDeviceSize alignment = vk->properties.limits.minStorageBufferOffsetAlignment;
    const VkDeviceSize sizes[VIEWS] = {HIDDEN * 4, FFN * 4, WEIGHT_BYTES, WEIGHT_BYTES, WEIGHT_BYTES,
        HIDDEN * 4, HIDDEN * 4, FFN * 4, FFN * 4, FFN * 4, FFN * 4, HIDDEN * 4};
    fg_vk_view view[VIEWS];
    VkDeviceSize end = 0;
    for (unsigned i = 0; i < VIEWS; ++i) {
        end = (end + alignment - 1) & ~(alignment - 1);
        view[i] = (fg_vk_view){&arena, end, sizes[i]};
        if (i < GATE_W || i > DOWN_W) end += sizes[i];
    }
    for (unsigned i = 0; i < 3; ++i) view[GATE_W + i] = (fg_vk_view){&weights, i * WEIGHT_BYTES, WEIGHT_BYTES};
    float input[HIDDEN], rotated[HIDDEN], gate[FFN], up[FFN], product[FFN], ffn_rotated[FFN], ref[HIDDEN], out[HIDDEN];
    for (unsigned i = 0; i < HIDDEN; ++i) input[i] = ((int)((i * 13) % 97) - 48) / 32.0f;

    ok = fg_vk_alloc_buffer(vk, &arena, end, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == VK_SUCCESS &&
        fg_vk_alloc_buffer(vk, &weights, 3 * WEIGHT_BYTES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0) == VK_SUCCESS &&
        fg_vk_access_buffer(vk, &arena, view[INPUT].offset, input, sizeof(input), 1) == VK_SUCCESS &&
        fg_vk_access_buffer(vk, &arena, view[SIGN_H].offset, signs, HIDDEN * 4, 1) == VK_SUCCESS &&
        fg_vk_access_buffer(vk, &arena, view[SIGN_F].offset, signs + HIDDEN + GDN, FFN * 4, 1) == VK_SUCCESS &&
        upload_weights(vk, &weights, 0, packed, 3 * WEIGHT_BYTES);
    const fg_vk_shape hidden = {.width = HIDDEN, .batch = 1};
    const fg_vk_shape wide = {.width = FFN, .batch = 1};
    const fg_vk_shape up_shape = {.width = HIDDEN, .rows = FFN, .batch = 1};
    const fg_vk_shape down_shape = {.width = FFN, .rows = HIDDEN, .batch = 1};
    ok = ok && fg_vk_graph_begin(graph) &&
        fg_vk_graph_record(graph, FG_VK_FWHT, hidden, view[INPUT], view[SIGN_H], view[ROTATED]) &&
        fg_vk_graph_record(graph, FG_VK_PQ2_0, up_shape, view[ROTATED], view[GATE_W], view[GATE]) &&
        fg_vk_graph_record(graph, FG_VK_PQ2_0, up_shape, view[ROTATED], view[UP_W], view[UP]) &&
        fg_vk_graph_record(graph, FG_VK_SWIGLU, wide, view[GATE], view[UP], view[PRODUCT]) &&
        fg_vk_graph_record(graph, FG_VK_FWHT, wide, view[PRODUCT], view[SIGN_F], view[FFN_ROTATED]) &&
        fg_vk_graph_record(graph, FG_VK_PQ2_0, down_shape, view[FFN_ROTATED], view[DOWN_W], view[OUTPUT]) &&
        fg_vk_graph_submit(graph) &&
        fg_vk_access_buffer(vk, &arena, view[OUTPUT].offset, out, sizeof(out), 0) == VK_SUCCESS &&
        fg_vk_access_buffer(vk, &arena, view[ROTATED].offset, rotated, sizeof(rotated), 0) == VK_SUCCESS;
    if (ok) {
        float expected[HIDDEN];
        fwht(expected, input, signs, HIDDEN);
        ok = memcmp(rotated, expected, sizeof(rotated)) == 0;
        if (!ok) fputs("Bonsai signed FWHT mismatch\n", stderr);
    }
    if (ok) {
        project(gate, packed, rotated, HIDDEN, FFN);
        project(up, packed + WEIGHT_BYTES, rotated, HIDDEN, FFN);
        for (unsigned i = 0; i < FFN; ++i) product[i] = (float)(gate[i] / (1.0 + exp(-gate[i])) * up[i]);
        fwht(ffn_rotated, product, signs + HIDDEN + GDN, FFN);
        project(ref, packed + 2 * WEIGHT_BYTES, ffn_rotated, FFN, HIDDEN);
        ok = compare(out, ref) && check_rotations_and_row(graph, &arena, view, input, signs, packed);
    }
    fg_vk_free_buffer(vk, &weights);
    fg_vk_free_buffer(vk, &arena);
    free(packed);
    return ok;
}
