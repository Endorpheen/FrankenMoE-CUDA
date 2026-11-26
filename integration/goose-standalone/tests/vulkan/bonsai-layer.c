#include "bonsai-layer.h"
#include "upload.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { H = FG_BONSAI_HIDDEN, D = FG_BONSAI_DIM, HS = FG_BONSAI_HEADS, V = FG_BONSAI_VALUES,
       M = FG_BONSAI_MIX, F = FG_BONSAI_FFN, HISTORY = 3 * M, STATE = V * D, STEPS = 4,
       IO_CHUNK = 4 << 20 };

static VkDeviceSize packed_size(unsigned width, unsigned rows) {
    return (VkDeviceSize)(width / 128) * rows * 34;
}

static fg_vk_view add_view(const fg_vk_buffer *buffer, VkDeviceSize *end, VkDeviceSize size, VkDeviceSize alignment) {
    const VkDeviceSize start = (*end + alignment - 1) & ~(alignment - 1);
    *end = start + size;
    return (fg_vk_view){buffer, start, size};
}

static int read_weights(FILE *fixture, fg_vk_device *vk, const fg_vk_view *weights, unsigned views) {
    fg_vk_upload upload = {0};
    unsigned char *chunk = malloc(IO_CHUNK);
    int ok = chunk && fg_vk_upload_open(&upload, vk, IO_CHUNK);
    for (unsigned i = 0; i < views && ok; ++i) {
        const fg_vk_view v = weights[i];
        for (VkDeviceSize at = 0; at < v.size && ok;) {
            const size_t count = v.size - at > IO_CHUNK ? IO_CHUNK : (size_t)(v.size - at);
            ok = fread(chunk, 1, count, fixture) == count &&
                fg_vk_upload_write(&upload, v.buffer, v.offset + at, chunk, count);
            at += count;
        }
    }
    fg_vk_upload_close(&upload);
    free(chunk);
    return ok;
}

static int access_view(fg_vk_device *vk, fg_vk_view v, void *data, int write) {
    return fg_vk_access_buffer(vk, v.buffer, v.offset, data, (size_t)v.size, write) == VK_SUCCESS;
}

static int compare(const float *actual, const float *reference, size_t count, double *worst) {
    for (size_t i = 0; i < count; ++i) {
        const double error = fabs((double)actual[i] - reference[i]);
        if (!isfinite(actual[i]) || !isfinite(reference[i]) || error > 2e-5 + 5e-4 * fabs(reference[i])) {
            fprintf(stderr, "Bonsai real layer mismatch at %llu: Vulkan=%g CPU=%g\n",
                    (unsigned long long)i, actual[i], reference[i]);
            return 0;
        }
        if (error > *worst) *worst = error;
    }
    return 1;
}

static int check(fg_vk_device *vk, fg_vk_view v, FILE *fixture, float *reference, float *actual, double *worst) {
    const size_t count = (size_t)v.size / sizeof(float);
    return fread(reference, sizeof(float), count, fixture) == count && access_view(vk, v, actual, 0) &&
        compare(actual, reference, count, worst);
}

int test_bonsai_layer(fg_vk_graph *graph, const char *path) {
    FILE *fixture = fopen(path, "rb");
    if (!fixture) { perror(path); return 0; }
    fg_vk_device *vk = graph->vk;
    const VkDeviceSize alignment = vk->properties.limits.minStorageBufferOffsetAlignment;
    fg_vk_buffer weights = {0}, scratch = {0}, host = {0};
    fg_vk_bonsai_layer layer = {0};
    VkDeviceSize weights_end = 0, scratch_end = 0, host_end = 0;
    const VkDeviceSize sizes[BW_COUNT] = {
        H * 4, V * 4, F * 4, H * 4, packed_size(H, M), packed_size(H, V), 2 * H * HS * 4,
        2 * HS * 4, 4 * M * 4, D * 4, packed_size(V, H), H * 4, packed_size(H, F), packed_size(H, F), packed_size(F, H),
    };
    for (unsigned i = 0; i < BW_COUNT; ++i) layer.weights[i] = add_view(&weights, &weights_end, sizes[i], alignment);
    const unsigned counts[BS_COUNT] = {
        H, H, M, V, 2 * HS, M, M, V, V, V, H, H, F, F, F, F
    };
    fg_vk_view s[BS_COUNT];
    for (unsigned i = 0; i < BS_COUNT; ++i) s[i] = add_view(&scratch, &scratch_end, counts[i] * 4, alignment);
    layer.history = add_view(&host, &host_end, HISTORY * 4, alignment);
    layer.state = add_view(&host, &host_end, STATE * 4, alignment);
    const fg_vk_view input = add_view(&host, &host_end, H * 4, alignment);
    const fg_vk_view output = add_view(&host, &host_end, H * 4, alignment);

    float *zero = calloc(STATE, sizeof(*zero));
    float *actual = malloc(STATE * sizeof(*actual));
    float *reference = malloc(STATE * sizeof(*reference));
    double error_output = 0, error_history = 0, error_state = 0;
    int ok = zero && actual && reference &&
        fg_vk_alloc_buffer(vk, &host, host_end, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == VK_SUCCESS &&
        fg_vk_alloc_buffer(vk, &weights, weights_end, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0) == VK_SUCCESS &&
        fg_vk_alloc_buffer(vk, &scratch, scratch_end, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0) == VK_SUCCESS &&
        access_view(vk, layer.state, zero, 1) && access_view(vk, layer.history, zero, 1) &&
        read_weights(fixture, vk, layer.weights, BW_COUNT);
    for (unsigned step = 0; ok && step < STEPS; ++step) {
        ok = fread(reference, sizeof(*reference), H, fixture) == H && access_view(vk, input, reference, 1) &&
            fg_vk_graph_begin(graph) && fg_vk_bonsai_recurrent_layer(graph, &layer, s, input, output, 1e-6f) &&
            fg_vk_graph_submit(graph) && check(vk, output, fixture, reference, actual, &error_output) &&
            check(vk, layer.history, fixture, reference, actual, &error_history) &&
            check(vk, layer.state, fixture, reference, actual, &error_state);
    }
    ok = ok && fgetc(fixture) == EOF;
    if (ok) printf("Bonsai real recurrent layer 0, 4 tokens: output=%g history=%g state=%g PASS\n",
                   error_output, error_history, error_state);
    else fputs("Bonsai real recurrent layer fixture failed\n", stderr);
    free(reference); free(actual); free(zero);
    fg_vk_free_buffer(vk, &host);
    fg_vk_free_buffer(vk, &scratch);
    fg_vk_free_buffer(vk, &weights);
    fclose(fixture);
    return ok;
}

static unsigned round_even(double value) {
    const unsigned integral = (unsigned)value;
    const double fraction = value - integral;
    return integral + (fraction > 0.5 || (fraction == 0.5 && (integral & 1)));
}

static unsigned fp16_bits(float value) {
    const unsigned sign = signbit(value) ? 0x8000 : 0;
    const double magnitude = fabs(value);
    if (magnitude < 0x1p-14) return sign | round_even(ldexp(magnitude, 24));
    if (magnitude >= 65520) return sign | 0x7c00;
    int exp;
    const double fraction = frexp(magnitude, &exp);
    return sign | ((unsigned)((exp + 14) * 1024) + round_even(ldexp(fraction, 11)) - 1024);
}

static void pack_row_q4(const float *row, unsigned char *out) {
    for (unsigned block = 0; block < FG_BONSAI_KV / 32; ++block) {
        const float *x = row + block * 32;
        const unsigned at = block * 18;
        float largest = 0;
        for (unsigned i = 0; i < 32; ++i) if (fabsf(x[i]) > fabsf(largest)) largest = x[i];
        const float scale = largest / -8.0f;
        const float inv = scale != 0 ? 1.0f / scale : 0;
        const unsigned bits = fp16_bits(scale);
        out[at] = (unsigned char)bits;
        out[at + 1] = (unsigned char)(bits >> 8);
        for (unsigned i = 0; i < 16; ++i) {
            const volatile float lo_product = x[i] * inv, hi_product = x[16 + i] * inv;
            const unsigned lo = (unsigned)(lo_product + 8.5f);
            const unsigned hi = (unsigned)(hi_product + 8.5f);
            out[at + i + 2] = (unsigned char)((lo < 15 ? lo : 15) | ((hi < 15 ? hi : 15) << 4));
        }
    }
}

int test_bonsai_attention_layer(fg_vk_graph *graph, const char *path) {
    FILE *fixture = fopen(path, "rb");
    if (!fixture) { perror(path); return 0; }
    enum { KV = FG_BONSAI_KV, KV_BYTES = FG_BONSAI_KV_BYTES, CAPACITY = STEPS + 1 };
    fg_vk_device *vk = graph->vk;
    const VkDeviceSize alignment = vk->properties.limits.minStorageBufferOffsetAlignment;
    fg_vk_buffer weights = {0}, scratch = {0}, host = {0};
    fg_vk_bonsai_attention layer = {0};
    VkDeviceSize weights_end = 0, scratch_end = 0, host_end = 0;
    const VkDeviceSize common[BW_COUNT] = {
        [BW_SIGN_H] = H * 4, [BW_SIGN_V] = V * 4, [BW_SIGN_F] = F * 4, [BW_ATTN_NORM] = H * 4,
        [BW_POST_NORM] = H * 4, [BW_FFN_GATE] = packed_size(H, F), [BW_FFN_UP] = packed_size(H, F),
        [BW_FFN_DOWN] = packed_size(F, H),
    };
    const VkDeviceSize sizes[BA_COUNT] = {
        packed_size(H, 2 * V), packed_size(H, KV), packed_size(H, KV),
        FG_BONSAI_HEAD_WIDTH * 4, FG_BONSAI_HEAD_WIDTH * 4, packed_size(V, H),
    };
    for (unsigned i = 0; i < BW_COUNT; ++i) {
        if (common[i]) layer.common[i] = add_view(&weights, &weights_end, common[i], alignment);
    }
    for (unsigned i = 0; i < BA_COUNT; ++i) layer.attention[i] = add_view(&weights, &weights_end, sizes[i], alignment);
    const unsigned counts[BS_COUNT] = { H, H, 2 * V, KV, KV, KV, KV, V, V, V, H, H, F, F, F, F };
    fg_vk_view s[BS_COUNT];
    for (unsigned i = 0; i < BS_COUNT; ++i) s[i] = add_view(&scratch, &scratch_end, counts[i] * 4, alignment);
    layer.keys = add_view(&host, &host_end, CAPACITY * KV_BYTES, alignment);
    layer.values = add_view(&host, &host_end, CAPACITY * KV_BYTES, alignment);
    const fg_vk_view input = add_view(&host, &host_end, H * 4, alignment);
    const fg_vk_view output = add_view(&host, &host_end, H * 4, alignment);
    float actual[H], reference[H], row[KV];
    unsigned char poison[CAPACITY * KV_BYTES], cache[CAPACITY * KV_BYTES];
    unsigned char cpu_cache[2][CAPACITY * KV_BYTES];
    unsigned char expected[2][CAPACITY * KV_BYTES];
    memset(poison, 0xa5, sizeof(poison));
    memcpy(expected[0], poison, sizeof(poison));
    memcpy(expected[1], poison, sizeof(poison));
    memcpy(cpu_cache[0], poison, sizeof(poison));
    memcpy(cpu_cache[1], poison, sizeof(poison));
    const fg_vk_view cache_view[] = {layer.keys, layer.values};
    const fg_vk_view row_view[] = {s[BS_QKV], s[BS_CONV]};
    double error_output = 0, same_cache_error = 0;
    unsigned independent_byte_differences = 0;
    int ok = fg_vk_alloc_buffer(vk, &host, host_end, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == VK_SUCCESS &&
        fg_vk_alloc_buffer(vk, &weights, weights_end, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0) == VK_SUCCESS &&
        fg_vk_alloc_buffer(vk, &scratch, scratch_end, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == VK_SUCCESS &&
        access_view(vk, layer.keys, poison, 1) && access_view(vk, layer.values, poison, 1) &&
        read_weights(fixture, vk, layer.common, BW_COUNT) && read_weights(fixture, vk, layer.attention, BA_COUNT);
    for (unsigned step = 0; step < STEPS && ok; ++step) {
        ok = fread(reference, sizeof(*reference), H, fixture) == H && access_view(vk, input, reference, 1) &&
            fg_vk_graph_begin(graph) &&
            fg_vk_bonsai_attention_layer(graph, &layer, s, input, output, 1e-6f, step, CAPACITY) &&
            fg_vk_graph_submit(graph) && fread(reference, sizeof(float), H, fixture) == H &&
            access_view(vk, output, actual, 0);
        for (unsigned j = 0; j < H && ok; ++j) {
            ok = isfinite(actual[j]) && isfinite(reference[j]);
            error_output = fmax(error_output, fabs((double)actual[j] - reference[j]));
        }
        for (unsigned i = 0; i < 2 && ok; ++i) {
            unsigned char *cpu_row = cpu_cache[i] + step * KV_BYTES;
            ok = fread(cpu_row, 1, KV_BYTES, fixture) == KV_BYTES &&
                access_view(vk, row_view[i], row, 0) && access_view(vk, cache_view[i], cache, 0);
            if (!ok) break;
            for (unsigned j = 0; j < KV; ++j) ok = ok && isfinite(row[j]);
            if (!ok) break;
            pack_row_q4(row, expected[i] + step * KV_BYTES);
            for (unsigned j = 0; j < KV_BYTES; ++j) {
                independent_byte_differences += cpu_row[j] != cache[step * KV_BYTES + j];
            }
            ok = !memcmp(expected[i], cache, sizeof(cache));
            if (!ok) {
                for (unsigned j = 0; j < sizeof(cache); ++j) {
                    if (expected[i][j] == cache[j]) continue;
                    fprintf(stderr, "Bonsai Q4 cache %s byte %u at step %u: device=%u same-input CPU=%u\n",
                        i == 0 ? "K" : "V", j, step, cache[j], expected[i][j]);
                    break;
                }
            }
        }
        /* Reuse the same input but supply NumPy's cache bytes; isolate FP32
         * projection rounding before the discontinuous Q4 quantizer. */
        ok = ok && access_view(vk, layer.keys, cpu_cache[0], 1) &&
            access_view(vk, layer.values, cpu_cache[1], 1) && fg_vk_graph_begin(graph) &&
            fg_vk_bonsai_attention_prepare(graph, &layer, s, input, 1e-6f, step) &&
            fg_vk_bonsai_attention_finish(graph, &layer, s, input, output, 1e-6f, step, CAPACITY) &&
            fg_vk_graph_submit(graph) && access_view(vk, output, actual, 0) &&
            compare(actual, reference, H, &same_cache_error) &&
            access_view(vk, layer.keys, expected[0], 1) && access_view(vk, layer.values, expected[1], 1);
    }
    ok = ok && fgetc(fixture) == EOF;
    if (ok) printf("Bonsai real attention layer 3, 4 tokens: independent output=%g, Q4 differing bytes=%u; "
                   "same-cache output=%g, same-input Q4 and untouched rows exact PASS\n",
                   error_output, independent_byte_differences, same_cache_error);
    else fputs("Bonsai real attention layer fixture failed\n", stderr);
    fg_vk_free_buffer(vk, &host);
    fg_vk_free_buffer(vk, &scratch);
    fg_vk_free_buffer(vk, &weights);
    fclose(fixture);
    return ok;
}
