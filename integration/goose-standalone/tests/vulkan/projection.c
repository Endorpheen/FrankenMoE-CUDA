#include "graph.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

enum { WIDTH = 64, VOCAB = 5, LOGITS = 67, BATCH = 3, FIRST_STRIPE = 64 };
enum { Q4_ROW = WIDTH / 32 * 18, Q8_ROW = WIDTH / 32 * 34 };
enum { IDS, EMBEDDING, HIDDEN, PROJECTION, LOGIT, BIAS, BIASED, SKIP, OUTPUT, VIEW_COUNT };

static double scale(unsigned block) {
    return block % 2 ? -0.25 : 0.125;
}

static double embedding_at(const unsigned char *packed, uint32_t token, unsigned column) {
    if (token >= VOCAB) return 0.0;
    unsigned block = token * (WIDTH / 32) + column / 32;
    unsigned word = packed[block * 18 + 2 + column % 16];
    unsigned nibble = (word >> (column % 32 >= 16 ? 4 : 0)) & 15;
    return scale(block) * ((int)nibble - 8);
}

static double projection_at(const unsigned char *packed, unsigned row, unsigned column) {
    unsigned block = row * (WIDTH / 32) + column / 32;
    int word = packed[block * 34 + 2 + column % 32];
    return scale(block) * (word >= 128 ? word - 256 : word);
}

int test_projection(fg_vk_graph *graph) {
    uint32_t ids[BATCH] = {4, 1, UINT32_MAX};
    unsigned char embedding[VOCAB * Q4_ROW], projection[LOGITS * Q8_ROW];
    float bias[LOGITS], skip[BATCH * LOGITS], output[BATCH * LOGITS], hidden[BATCH * WIDTH];
    memset(embedding, 0, sizeof(embedding));
    memset(projection, 0, sizeof(projection));
    for (unsigned i = 0; i < VOCAB * WIDTH / 32; ++i) {
        embedding[i * 18 + 1] = i % 2 ? 0xb4 : 0x30;
        for (unsigned j = 0; j < 16; ++j) embedding[i * 18 + 2 + j] = (unsigned char)(13 * i + 17 * j);
    }
    for (unsigned i = 0; i < LOGITS * WIDTH / 32; ++i) {
        projection[i * 34 + 1] = i % 2 ? 0xb4 : 0x30;
        for (unsigned j = 0; j < 32; ++j) projection[i * 34 + 2 + j] = (unsigned char)(128 + 37 * i + 13 * j);
    }
    for (unsigned j = 0; j < LOGITS; ++j) bias[j] = ((int)(j % 7) - 3) / 8.0f;
    for (unsigned i = 0; i < BATCH * LOGITS; ++i) skip[i] = ((int)(i % 23) - 11) / 16.0f;

    fg_vk_device *vk = graph->vk;
    fg_vk_buffer arena = {0};
    fg_vk_view views[VIEW_COUNT];
    const size_t sizes[VIEW_COUNT] = {sizeof(ids), sizeof(embedding), sizeof(hidden), sizeof(projection),
                                     sizeof(output), sizeof(bias), sizeof(output), sizeof(skip), sizeof(output)};
    const VkDeviceSize alignment = vk->properties.limits.minStorageBufferOffsetAlignment;
    VkDeviceSize end = 0, projected_half = 0;
    for (unsigned i = 0; i < VIEW_COUNT; ++i) {
        const VkDeviceSize begin = (end + alignment - 1) & ~(alignment - 1);
        views[i] = (fg_vk_view){&arena, begin, sizes[i]};
        end = begin + sizes[i];
        if (i == PROJECTION) {
            projected_half = (begin + FIRST_STRIPE * Q8_ROW + alignment - 1) & ~(alignment - 1);
            end = projected_half + (LOGITS - FIRST_STRIPE) * Q8_ROW;
        }
    }
    VkResult result = fg_vk_alloc_buffer(vk, &arena, end, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (result != VK_SUCCESS) return fg_vk_error("allocate quantized projection fixture arena", result);

    const unsigned indices[] = {IDS, EMBEDDING, BIAS, SKIP};
    void *host[] = {ids, embedding, bias, skip};
    for (unsigned i = 0; i < sizeof(indices) / sizeof(*indices) && result == VK_SUCCESS; ++i) {
        const fg_vk_view view = views[indices[i]];
        result = fg_vk_access_buffer(vk, &arena, view.offset, host[i], (size_t)view.size, 1);
    }
    const fg_vk_view first = {&arena, views[PROJECTION].offset, FIRST_STRIPE * Q8_ROW};
    const fg_vk_view second = {&arena, projected_half, (LOGITS - FIRST_STRIPE) * Q8_ROW};
    if (result == VK_SUCCESS) result = fg_vk_access_buffer(vk, &arena, first.offset, projection, (size_t)first.size, 1);
    if (result == VK_SUCCESS) result = fg_vk_access_buffer(vk, &arena, second.offset,
        projection + first.size, (size_t)second.size, 1);

    const fg_vk_shape gather = {.width = WIDTH, .rows = VOCAB, .batch = BATCH};
    const fg_vk_shape projection_a = {.width = WIDTH, .rows = FIRST_STRIPE, .batch = BATCH, .context = LOGITS};
    fg_vk_shape projection_b = projection_a;
    projection_b.rows = LOGITS - FIRST_STRIPE;
    projection_b.position = FIRST_STRIPE;
    fg_vk_shape invalid = projection_b;
    invalid.position++;
    const fg_vk_shape add = {.width = LOGITS, .batch = BATCH};
    int pass = result == VK_SUCCESS && fg_vk_graph_begin(graph) &&
        !fg_vk_graph_record(graph, FG_VK_Q8_0, invalid, views[HIDDEN], second, views[LOGIT]) &&
        fg_vk_graph_record(graph, FG_VK_GATHER_Q4_0, gather, views[IDS], views[EMBEDDING], views[HIDDEN]) &&
        fg_vk_graph_record(graph, FG_VK_Q8_0, projection_a, views[HIDDEN], first, views[LOGIT]) &&
        fg_vk_graph_record(graph, FG_VK_Q8_0, projection_b, views[HIDDEN], second, views[LOGIT]) &&
        fg_vk_graph_record(graph, FG_VK_BIAS, add, views[LOGIT], views[BIAS], views[BIASED]) &&
        fg_vk_graph_record(graph, FG_VK_ADD, add, views[BIASED], views[SKIP], views[OUTPUT]) &&
        fg_vk_graph_submit(graph) &&
        fg_vk_access_buffer(vk, &arena, views[HIDDEN].offset, hidden, sizeof(hidden), 0) == VK_SUCCESS &&
        fg_vk_access_buffer(vk, &arena, views[OUTPUT].offset, output, sizeof(output), 0) == VK_SUCCESS;

    for (unsigned t = 0; t < BATCH && pass; ++t) {
        for (unsigned i = 0; i < WIDTH; ++i) {
            if (hidden[t * WIDTH + i] != embedding_at(embedding, ids[t], i)) pass = 0;
        }
        for (unsigned row = 0; row < LOGITS; ++row) {
            const unsigned at = t * LOGITS + row;
            double dot = 0;
            for (unsigned i = 0; i < WIDTH; ++i) dot += embedding_at(embedding, ids[t], i) * projection_at(projection, row, i);
            const double expected = dot + bias[row] + skip[at];
            if (output[at] != expected) {
                fprintf(stderr, "projection mismatch at token=%u row=%u: device=%g reference=%g\n", t, row, output[at], expected);
                pass = 0;
                break;
            }
        }
    }
    if (pass) puts("Q4_0 gather, split Q8_0 projection, broadcast bias and residual: exact PASS");
    fg_vk_free_buffer(vk, &arena);
    return pass;
}
