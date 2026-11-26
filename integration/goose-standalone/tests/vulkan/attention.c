#include "graph.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

enum { DIM = 64, HEADS = 14, KV_HEADS = 2, TOKENS = 5, CAPACITY = 8 };
enum { QUERY_FLOATS = HEADS * DIM, KV_FLOATS = KV_HEADS * DIM };
enum { QUERY, KEY, VALUE, ROTATED_Q, ROTATED_K, CACHE_K, CACHE_V, OUTPUT, VIEW_COUNT };

static fg_vk_view rows_at(fg_vk_view view, unsigned start, unsigned count, unsigned stride) {
    view.offset += (VkDeviceSize)start * stride * sizeof(float);
    view.size = (VkDeviceSize)count * stride * sizeof(float);
    return view;
}

static int record_batch(fg_vk_graph *graph, fg_vk_view views[VIEW_COUNT], unsigned position, unsigned count) {
    const fg_vk_view q = rows_at(views[QUERY], position, count, QUERY_FLOATS);
    const fg_vk_view k = rows_at(views[KEY], position, count, KV_FLOATS);
    const fg_vk_view v = rows_at(views[VALUE], position, count, KV_FLOATS);
    const fg_vk_view qr = rows_at(views[ROTATED_Q], 0, count, QUERY_FLOATS);
    const fg_vk_view kr = rows_at(views[ROTATED_K], 0, count, KV_FLOATS);
    const fg_vk_view out = rows_at(views[OUTPUT], position, count, QUERY_FLOATS);
    fg_vk_shape qs = {.width = DIM, .rows = HEADS, .batch = count, .position = position,
                      .kv_heads = KV_HEADS, .context = CAPACITY, .theta = 1000000.0f};
    fg_vk_shape ks = qs;
    ks.rows = KV_HEADS;
    fg_vk_shape invalid = qs;
    invalid.kv_heads = 3;
    return fg_vk_graph_begin(graph) &&
        !fg_vk_graph_attention(graph, invalid, qr, views[CACHE_K], views[CACHE_V], out) &&
        fg_vk_graph_record(graph, FG_VK_ROPE, qs, q, q, qr) &&
        fg_vk_graph_record(graph, FG_VK_ROPE, ks, k, k, kr) &&
        fg_vk_graph_record(graph, FG_VK_KV_STORE, ks, kr, kr, views[CACHE_K]) &&
        fg_vk_graph_record(graph, FG_VK_KV_STORE, ks, v, v, views[CACHE_V]) &&
        fg_vk_graph_attention(graph, qs, qr, views[CACHE_K], views[CACHE_V], out) &&
        fg_vk_graph_submit(graph);
}

static double rotate(const float *row, unsigned position, unsigned column) {
    unsigned i = column % (DIM / 2);
    double angle = position * pow(1000000.0, -2.0 * i / DIM);
    double a = row[i], b = row[i + DIM / 2];
    return column < DIM / 2 ? a * cos(angle) - b * sin(angle) : b * cos(angle) + a * sin(angle);
}

static int verify(const float *q, const float *k, const float *v, const float *out) {
    double max_error = 0;
    for (unsigned t = 0; t < TOKENS; ++t) {
        for (unsigned h = 0; h < HEADS; ++h) {
            unsigned group = h / (HEADS / KV_HEADS);
            double probabilities[TOKENS];
            double peak = -INFINITY, norm = 0;
            for (unsigned past = 0; past <= t; ++past) {
                double dot = 0;
                for (unsigned dim = 0; dim < DIM; ++dim) {
                    const float *qr = q + t * QUERY_FLOATS + h * DIM;
                    const float *kr = k + past * KV_FLOATS + group * DIM;
                    dot += rotate(qr, t, dim) * rotate(kr, past, dim);
                }
                probabilities[past] = dot / sqrt((double)DIM);
                if (probabilities[past] > peak) peak = probabilities[past];
            }
            for (unsigned past = 0; past <= t; ++past) {
                probabilities[past] = exp(probabilities[past] - peak);
                norm += probabilities[past];
            }
            for (unsigned dim = 0; dim < DIM; ++dim) {
                double reference = 0;
                for (unsigned past = 0; past <= t; ++past) {
                    reference += probabilities[past] * v[past * KV_FLOATS + group * DIM + dim];
                }
                float result = out[t * QUERY_FLOATS + h * DIM + dim];
                double error = fabs(result - reference / norm);
                if (!isfinite(result) || error > 2e-5) {
                    fprintf(stderr, "attention mismatch at token=%u head=%u dim=%u, error=%g\n", t, h, dim, error);
                    return 0;
                }
                if (error > max_error) max_error = error;
            }
        }
    }
    printf("causal GQA 14:2 width=64 tokens=5 max_error=%g PASS\n", max_error);
    return 1;
}

int test_attention(fg_vk_graph *graph) {
    fg_vk_device *vk = graph->vk;
    float q[QUERY_FLOATS * TOKENS], k[KV_FLOATS * TOKENS], v[KV_FLOATS * TOKENS];
    float batch_out[QUERY_FLOATS * TOKENS], split_out[QUERY_FLOATS * TOKENS];
    float poison[KV_FLOATS * CAPACITY], cache[KV_FLOATS * CAPACITY];
    for (unsigned i = 0; i < QUERY_FLOATS * TOKENS; ++i) q[i] = ((int)((i * 3) % 29) - 14) / 11.0f;
    for (unsigned i = 0; i < KV_FLOATS * TOKENS; ++i) {
        k[i] = ((int)((i * 7) % 31) - 15) / 12.0f;
        v[i] = ((int)((i * 5 + i / KV_FLOATS) % 23) - 11) / 7.0f;
    }
    for (unsigned i = 0; i < KV_FLOATS * CAPACITY; ++i) poison[i] = 10000.0f;

    const size_t sizes[VIEW_COUNT] = {sizeof(q), sizeof(k), sizeof(v), sizeof(q), sizeof(k),
                                      sizeof(poison), sizeof(poison), sizeof(batch_out)};
    VkDeviceSize alignment = vk->properties.limits.minStorageBufferOffsetAlignment, end = 0;
    fg_vk_buffer arena = {0};
    fg_vk_view views[VIEW_COUNT];
    for (unsigned i = 0; i < VIEW_COUNT; ++i) {
        VkDeviceSize start = (end + alignment - 1) & ~(alignment - 1);
        views[i] = (fg_vk_view){&arena, start, sizes[i]};
        end = start + sizes[i];
    }
    VkResult result = fg_vk_alloc_buffer(vk, &arena, end, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (result != VK_SUCCESS) return fg_vk_error("allocate attention fixture arena", result);

    void *host[VIEW_COUNT] = {[QUERY] = q, [KEY] = k, [VALUE] = v, [CACHE_K] = poison, [CACHE_V] = poison};
    for (unsigned i = 0; i < VIEW_COUNT && result == VK_SUCCESS; ++i) {
        if (host[i]) result = fg_vk_access_buffer(vk, &arena, views[i].offset, host[i], sizes[i], 1);
    }
    int pass = result == VK_SUCCESS && record_batch(graph, views, 0, TOKENS) &&
        fg_vk_access_buffer(vk, &arena, views[OUTPUT].offset, batch_out, sizeof(batch_out), 0) == VK_SUCCESS &&
        verify(q, k, v, batch_out) &&
        record_batch(graph, views, 0, 2) && record_batch(graph, views, 2, 1) && record_batch(graph, views, 3, 2) &&
        fg_vk_access_buffer(vk, &arena, views[OUTPUT].offset, split_out, sizeof(split_out), 0) == VK_SUCCESS &&
        fg_vk_access_buffer(vk, &arena, views[CACHE_V].offset, cache, sizeof(cache), 0) == VK_SUCCESS &&
        memcmp(batch_out, split_out, sizeof(batch_out)) == 0 &&
        memcmp(cache, v, sizeof(v)) == 0 &&
        memcmp(cache + KV_FLOATS * TOKENS, poison + KV_FLOATS * TOKENS, sizeof(cache) - sizeof(v)) == 0;

    /* Rewind the visible prefix and overwrite its first row. Leave old later rows resident. */
    if (pass) {
        for (unsigned i = 0; i < KV_FLOATS; ++i) v[i] = 3.25f;
        pass = fg_vk_access_buffer(vk, &arena, views[VALUE].offset, v, KV_FLOATS * sizeof(float), 1) == VK_SUCCESS &&
            record_batch(graph, views, 0, 1) &&
            fg_vk_access_buffer(vk, &arena, views[OUTPUT].offset, split_out, QUERY_FLOATS * sizeof(float), 0) == VK_SUCCESS;
        for (unsigned i = 0; pass && i < QUERY_FLOATS; ++i) pass = split_out[i] == 3.25f;
    }
    if (pass) puts("prefill=5 split=2+1+2 bitwise PASS, KV bounds and rewind PASS");
    else fputs("attention graph fixture failed\n", stderr);
    fg_vk_free_buffer(vk, &arena);
    return pass;
}
