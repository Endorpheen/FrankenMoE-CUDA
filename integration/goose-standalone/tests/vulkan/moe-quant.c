#include "graph.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

enum { WIDTH = 512, ROWS = 7, BATCH = 3, MAX_WEIGHT = WIDTH / 32 * 34 * ROWS };
enum { INPUT, WEIGHT, DOT, IDS, GATHER, VIEW_COUNT };

static int run_quant(fg_vk_graph *graph, FILE *file, const char *name,
    fg_vk_op op, fg_vk_op gather, uint32_t expected_bytes) {
    uint32_t bytes;
    unsigned char packed[MAX_WEIGHT];
    float reference[ROWS * BATCH], reference_gather[WIDTH * BATCH];
    if (expected_bytes > sizeof(packed) || fread(&bytes, sizeof(bytes), 1, file) != 1 ||
        bytes != expected_bytes || fread(packed, 1, bytes, file) != bytes ||
        fread(reference, sizeof(reference), 1, file) != 1 ||
        (gather != FG_VK_OP_COUNT && fread(reference_gather, sizeof(reference_gather), 1, file) != 1)) return 0;

    float input[WIDTH * BATCH], dot[ROWS * BATCH], hidden[WIDTH * BATCH];
    for (unsigned i = 0; i < WIDTH * BATCH; ++i) input[i] = ((int)(i % 31) - 15) / 8.0f;
    uint32_t ids[BATCH] = {ROWS + 10, 11, 10};
    fg_vk_device *vk = graph->vk;
    fg_vk_buffer arena = {0};
    const VkDeviceSize alignment = vk->properties.limits.minStorageBufferOffsetAlignment;
    const VkDeviceSize sizes[VIEW_COUNT] = {sizeof(input), bytes, sizeof(dot), sizeof(ids), sizeof(hidden)};
    fg_vk_view views[VIEW_COUNT];
    VkDeviceSize end = 0;
    for (unsigned i = 0; i < VIEW_COUNT; ++i) {
        const VkDeviceSize offset = (end + alignment - 1) & ~(alignment - 1);
        views[i] = (fg_vk_view){&arena, offset, sizes[i]};
        end = offset + sizes[i];
    }
    VkResult result = fg_vk_alloc_buffer(vk, &arena, end, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (result != VK_SUCCESS) return fg_vk_error("allocate MoE quantization test arena", result);

    fg_vk_shape shape = {.width = WIDTH, .rows = ROWS, .batch = BATCH};
    fg_vk_shape gather_shape = shape;
    gather_shape.position = 11;
    int pass = fg_vk_access_buffer(vk, &arena, views[INPUT].offset, input, sizeof(input), 1) == VK_SUCCESS &&
        fg_vk_access_buffer(vk, &arena, views[WEIGHT].offset, packed, bytes, 1) == VK_SUCCESS &&
        fg_vk_access_buffer(vk, &arena, views[IDS].offset, ids, sizeof(ids), 1) == VK_SUCCESS &&
        fg_vk_graph_begin(graph) &&
        fg_vk_graph_record(graph, op, shape, views[INPUT], views[WEIGHT], views[DOT]) &&
        (gather == FG_VK_OP_COUNT ||
            fg_vk_graph_record(graph, gather, gather_shape, views[IDS], views[WEIGHT], views[GATHER])) &&
        fg_vk_graph_submit(graph) &&
        fg_vk_access_buffer(vk, &arena, views[DOT].offset, dot, sizeof(dot), 0) == VK_SUCCESS;
    float max_error = 0;
    for (unsigned i = 0; i < ROWS * BATCH && pass; ++i) {
        const float delta = fabsf(dot[i] - reference[i]);
        max_error = fmaxf(max_error, delta);
        pass = isfinite(dot[i]) && delta <= 2e-5f * (1.0f + fabsf(reference[i]));
    }
    if (pass && gather != FG_VK_OP_COUNT) {
        pass = fg_vk_access_buffer(vk, &arena, views[GATHER].offset, hidden, sizeof(hidden), 0) == VK_SUCCESS;
        for (unsigned i = 0; i < WIDTH * BATCH && pass; ++i) pass = hidden[i] == reference_gather[i];
    }
    printf("%s real-weight projection%s: max_abs=%g %s\n", name,
        gather == FG_VK_OP_COUNT ? "" : " and exact gather", max_error, pass ? "PASS" : "FAIL");
    fg_vk_free_buffer(vk, &arena);
    return pass;
}

int test_moe_quant(fg_vk_graph *graph, const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    char magic[8];
    int pass = fread(magic, 1, sizeof(magic), file) == sizeof(magic) &&
        !memcmp(magic, "FGMOEQ1", sizeof(magic)) &&
        run_quant(graph, file, "Q4_K", FG_VK_Q4_K, FG_VK_GATHER_Q4_K, ROWS * (WIDTH / 256) * 144) &&
        run_quant(graph, file, "Q6_K", FG_VK_Q6_K, FG_VK_OP_COUNT, ROWS * (WIDTH / 256) * 210) &&
        run_quant(graph, file, "IQ4_NL", FG_VK_IQ4_NL, FG_VK_OP_COUNT, ROWS * (WIDTH / 32) * 18) &&
        run_quant(graph, file, "IQ4_XS", FG_VK_IQ4_XS, FG_VK_OP_COUNT, ROWS * (WIDTH / 256) * 136) &&
        run_quant(graph, file, "Q8_0", FG_VK_Q8_0, FG_VK_GATHER_Q8_0, ROWS * (WIDTH / 32) * 34) &&
        fgetc(file) == EOF && !ferror(file);
    fclose(file);
    return pass;
}
