#include "graph.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

enum { HEADS = 24, KV_HEADS = 8, WIDTH = 128, REL = 16, EXTENT = 1024 };
enum { QUERY = HEADS * WIDTH, RELATIVE = HEADS * REL, INPUT = QUERY + RELATIVE + HEADS + EXTENT * REL };
enum { HOST_Q, HOST_K, HOST_V, CACHE_K, CACHE_V, OUTPUT, VIEW_COUNT };

static int compare_attention(const float *input, const float *keys, const float *values,
    const float *output, unsigned capacity, unsigned position, unsigned window) {
    double max_error = 0.0;
    for (unsigned head = 0; head < HEADS; ++head) {
        const unsigned hb = head * WIDTH, kb = head / (HEADS / KV_HEADS) * WIDTH;
        const unsigned first = window && position + 1 > window ? position + 1 - window : 0;
        double weighted[WIDTH] = {0}, denominator = 1.0, peak = 0.0;
        for (unsigned i = first; i <= position; ++i) {
            unsigned base = i % capacity * WIDTH * KV_HEADS + kb;
            unsigned offset = position - i;
            double dot = 0, rel = 0;
            for (unsigned j = 0; j < WIDTH; ++j) dot += (double)input[hb + j] * keys[base + j];
            if (offset < EXTENT) {
                for (unsigned j = 0; j < REL; ++j) {
                    rel += (double)input[QUERY + head * REL + j] *
                        input[QUERY + RELATIVE + HEADS + offset * REL + j];
                }
            }
            const double score = dot / sqrt(WIDTH) + rel - input[QUERY + RELATIVE + head];
            const double next = fmax(peak, score);
            const double old_scale = exp(peak - next), weight = exp(score - next);
            denominator = denominator * old_scale + weight;
            for (unsigned j = 0; j < WIDTH; ++j) weighted[j] = weighted[j] * old_scale + weight * values[base + j];
            peak = next;
        }
        for (unsigned j = 0; j < WIDTH; ++j) {
            const double target = weighted[j] / denominator;
            const double delta = fabs(output[hb + j] - target);
            if (!isfinite(output[hb + j]) || delta > 3e-6 * (1.0 + fabs(target))) {
                fprintf(stderr, "Inkling attention model head=%u col=%u actual=%g expected=%g\n", head, j, output[hb + j], target);
                return 0;
            }
            max_error = fmax(max_error, delta);
        }
    }
    printf("Inkling Q4 relative attention: context=%u absolute=%u window=%u max_abs=%g PASS\n",
        capacity, position, window, max_error);
    return 1;
}

static int run_attention(fg_vk_graph *graph, unsigned capacity, unsigned position, unsigned window) {
    float q[INPUT], output[QUERY];
    const unsigned elements = capacity * WIDTH * KV_HEADS;
    float *k = malloc(elements * sizeof(float)), *v = malloc(elements * sizeof(float));
    if (!k || !v) {
        free(k); free(v);
        return 0;
    }
    for (unsigned i = 0; i < INPUT; ++i) q[i] = ((int)((i * 13 + i / 17) % 29) - 14) / 32.0f;
    // Each block contains -8 and uses an exact power-of-two Q4 scale.
    for (unsigned i = 0; i < elements; ++i) {
        k[i] = ((int)((i * 9 + i / 32 + i / 1019) % 16) - 8) / 16.0f;
        v[i] = ((int)((i * 13 + i / 32 + i / 1021) % 16) - 8) / 8.0f;
    }
    fg_vk_device *vk = graph->vk;
    const VkDeviceSize alignment = vk->properties.limits.minStorageBufferOffsetAlignment;
    const VkDeviceSize lengths[VIEW_COUNT] = {
        sizeof(q), elements * 4, elements * 4, elements / 32 * 18, elements / 32 * 18, sizeof(output),
    };
    fg_vk_buffer arena = {0};
    fg_vk_view views[VIEW_COUNT];
    VkDeviceSize end = 0;
    for (unsigned i = 0; i < VIEW_COUNT; ++i) {
        const VkDeviceSize offset = (end + alignment - 1) & ~(alignment - 1);
        views[i] = (fg_vk_view){&arena, offset, lengths[i]};
        end = offset + lengths[i];
    }
    const fg_vk_shape store = {.width = WIDTH, .rows = KV_HEADS, .batch = capacity, .context = capacity};
    const fg_vk_shape attend = {.width = WIDTH, .rows = HEADS, .batch = 1, .position = position, .context = capacity,
        .kv_heads = KV_HEADS, .rotary = window};
    int pass = fg_vk_alloc_buffer(vk, &arena, end, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == VK_SUCCESS &&
        fg_vk_access_buffer(vk, &arena, views[HOST_Q].offset, q, lengths[HOST_Q], 1) == VK_SUCCESS &&
        fg_vk_access_buffer(vk, &arena, views[HOST_K].offset, k, lengths[HOST_K], 1) == VK_SUCCESS &&
        fg_vk_access_buffer(vk, &arena, views[HOST_V].offset, v, lengths[HOST_V], 1) == VK_SUCCESS &&
        fg_vk_graph_begin(graph) &&
        fg_vk_graph_record(graph, FG_VK_KV_STORE_Q4, store, views[HOST_K], views[HOST_K], views[CACHE_K]) &&
        fg_vk_graph_record(graph, FG_VK_KV_STORE_Q4, store, views[HOST_V], views[HOST_V], views[CACHE_V]) &&
        fg_vk_graph_inkling_attention(graph, attend, views[HOST_Q], views[CACHE_K], views[CACHE_V], views[OUTPUT]) &&
        fg_vk_graph_submit(graph) &&
        fg_vk_access_buffer(vk, &arena, views[OUTPUT].offset, output, lengths[OUTPUT], 0) == VK_SUCCESS &&
        compare_attention(q, k, v, output, capacity, position, window);
    fg_vk_free_buffer(vk, &arena);
    free(k); free(v);
    return pass;
}

int test_inkling_attention(fg_vk_graph *graph) {
    return run_attention(graph, 7, 6, 0) && run_attention(graph, 9, 12, 5) &&
        run_attention(graph, 1026, 1025, 0);
}
