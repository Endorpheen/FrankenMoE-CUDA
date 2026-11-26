#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200809L
#include "moe-bank.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WIDTH = 2048, EXPERTS = 256, SELECTED = 8, FF = 512 };
enum { INPUT, NORM, NORMALIZED, ROUTER, LOGITS, ROUTES, GATE, UP, ACTIVATION, BRANCHES, OUTPUT, VIEW_COUNT };

static int run_experts(fg_vk_graph *graph, FILE *model, uint64_t file_bytes,
    const fg_vk_expert_plane planes[3], float *input, float *norm, float *router, const uint32_t *expected, float *out) {
    fg_vk_device *vk = graph->vk;
    const VkDeviceSize alignment = vk->properties.limits.minStorageBufferOffsetAlignment;
    if ((WIDTH * sizeof(float)) % alignment) return 0;
    const uint64_t sizes[3] = {512 * 1152, 512 * 1152, 2048 * 420};
    for (unsigned p = 0; p < 3; ++p) if (planes[p].expert_bytes != sizes[p]) return 0;

    const VkDeviceSize lengths[VIEW_COUNT] = {WIDTH * 4, WIDTH * 4, WIDTH * 4, EXPERTS * WIDTH * 4, EXPERTS * 4, SELECTED * 8,
        FF * 4, FF * 4, FF * 4, SELECTED * WIDTH * 4, WIDTH * 4};
    fg_vk_buffer arena = {0};
    fg_vk_view views[VIEW_COUNT];
    VkDeviceSize end = 0;
    for (unsigned i = 0; i < VIEW_COUNT; ++i) {
        const VkDeviceSize offset = (end + alignment - 1) & ~(alignment - 1);
        views[i] = (fg_vk_view){&arena, offset, lengths[i]};
        end = offset + lengths[i];
    }
    const fg_vk_shape router_shape = {.width = WIDTH, .rows = EXPERTS, .batch = 1};
    const fg_vk_shape route_shape = {.width = EXPERTS, .rows = SELECTED, .batch = 1};
    const fg_vk_shape norm_shape = {.width = WIDTH, .batch = 1, .epsilon = 1e-6f};
    const fg_vk_shape weighted = {.width = WIDTH, .rows = SELECTED, .batch = 1};
    uint32_t routes[2 * SELECTED];
    fg_vk_expert_bank bank = {0};

    int pass = fg_vk_expert_bank_open(&bank, vk, SELECTED, sizes) &&
        fg_vk_alloc_buffer(vk, &arena, end, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == VK_SUCCESS &&
        fg_vk_access_buffer(vk, &arena, views[INPUT].offset, input, lengths[INPUT], 1) == VK_SUCCESS &&
        fg_vk_access_buffer(vk, &arena, views[NORM].offset, norm, lengths[NORM], 1) == VK_SUCCESS &&
        fg_vk_access_buffer(vk, &arena, views[ROUTER].offset, router, lengths[ROUTER], 1) == VK_SUCCESS &&
        fg_vk_graph_begin(graph) &&
        fg_vk_graph_record(graph, FG_VK_RMSNORM, norm_shape, views[INPUT], views[NORM], views[NORMALIZED]) &&
        fg_vk_graph_record(graph, FG_VK_F32_PROJECT, router_shape, views[NORMALIZED], views[ROUTER], views[LOGITS]) &&
        fg_vk_graph_record(graph, FG_VK_MOE_ROUTE, route_shape, views[LOGITS], views[LOGITS], views[ROUTES]) &&
        fg_vk_graph_submit(graph) &&
        fg_vk_access_buffer(vk, &arena, views[ROUTES].offset, routes, sizeof(routes), 0) == VK_SUCCESS &&
        !memcmp(routes, expected, SELECTED * sizeof(uint32_t));
    if (pass) {
        const uint32_t invalid = EXPERTS;
        pass = !fg_vk_expert_bank_stage(&bank, model, file_bytes, EXPERTS, planes, &invalid, 1) &&
            bank.loaded == 0 &&
            fg_vk_expert_bank_stage(&bank, model, file_bytes, EXPERTS, planes, routes, SELECTED) &&
            bank.loaded == SELECTED && fg_vk_graph_begin(graph);
    }
    if (pass) {
        const fg_vk_op ops[] = {FG_VK_Q4_K, FG_VK_Q4_K, FG_VK_Q6_K};
        pass = fg_vk_expert_bank_record_split(graph, &bank, WIDTH, FF, ops, views[NORMALIZED], views + GATE, views[BRANCHES]) &&
               fg_vk_graph_record(graph, FG_VK_MOE_WEIGHTED, weighted, views[BRANCHES], views[ROUTES], views[OUTPUT]) &&
               fg_vk_graph_submit(graph) &&
               fg_vk_access_buffer(vk, &arena, views[OUTPUT].offset, out, lengths[OUTPUT], 0) == VK_SUCCESS;
    }
    fg_vk_expert_bank_close(&bank);
    fg_vk_free_buffer(vk, &arena);
    return pass;
}

int test_apex_experts(fg_vk_graph *graph, const char *path, const char *fixture) {
    FILE *model = fopen(path, "rb");
    FILE *file = fopen(fixture, "rb");
    if (!model || !file) {
        if (model) fclose(model);
        if (file) fclose(file);
        return 0;
    }
    const int64_t file_bytes = fseeko(model, 0, SEEK_END) == 0 ? ftello(model) : -1;
    char magic[8];
    float input[WIDTH], norm[WIDTH], expected[WIDTH], output[WIDTH];
    float *router = malloc(WIDTH * EXPERTS * sizeof(float));
    uint32_t ids[SELECTED];
    fg_vk_expert_plane planes[3];
    int pass = file_bytes > 0 && router && fread(magic, sizeof(magic), 1, file) == 1 &&
        !memcmp(magic, "FGMOEE2", sizeof(magic)) &&
        fread(planes, sizeof(planes), 1, file) == 1 &&
        fread(input, sizeof(input), 1, file) == 1 &&
        fread(norm, sizeof(norm), 1, file) == 1 &&
        fread(router, WIDTH * EXPERTS * sizeof(float), 1, file) == 1 &&
        fread(ids, sizeof(ids), 1, file) == 1 &&
        fread(expected, sizeof(expected), 1, file) == 1 &&
        fgetc(file) == EOF && !ferror(file);
    fclose(file);
    if (pass) pass = run_experts(graph, model, (uint64_t)file_bytes, planes, input, norm, router, ids, output);
    free(router);
    fclose(model);

    float max_error = 0.0f, peak = 0.0f;
    if (pass) for (unsigned i = 0; i < WIDTH; ++i) peak = fmaxf(peak, fabsf(expected[i]));
    pass = pass && peak > 0;
    for (unsigned i = 0; i < WIDTH && pass; ++i) {
        const float delta = fabsf(output[i] - expected[i]);
        max_error = fmaxf(max_error, delta);
        pass = isfinite(output[i]) && delta <= peak * 5e-7f + fabsf(expected[i]) * 2e-5f;
    }
    printf("Apex routed experts: exact routes, real GGUF staging, Q4_K–SwiGLU–Q6_K, weighted output max_abs=%g %s\n",
        max_error, pass ? "PASS" : "FAIL");
    return pass;
}
