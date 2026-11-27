#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200809L
#include "moe-bank.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { SELECTED = 8 };
enum { INPUT, NORM, NORMALIZED, ROUTER, SELECTION_BIAS, LOGITS, ROUTES, GATE, UP, ACTIVATION, BRANCHES, OUTPUT, VIEW_COUNT };

static int run_experts(fg_vk_graph *graph, FILE *model, uint64_t file_bytes,
    const fg_vk_expert_plane planes[3], float *input, float *norm, float *router, float *bias,
    const uint32_t *expected, float *out, uint32_t width, uint32_t experts, uint32_t ff) {
    const int inkling = bias != NULL;
    fg_vk_device *vk = graph->vk;
    const VkDeviceSize alignment = vk->properties.limits.minStorageBufferOffsetAlignment;
    if ((width * sizeof(float)) % alignment) return 0;
    const uint64_t sizes[3] = {inkling ? 2304 * (2304 / 256 * 136) : 512 * 1152,
        inkling ? 2304 * (1152 / 32 * 18) : 512 * 1152, inkling ? 0 : 2048 * 420};
    for (unsigned p = 0; p < 3; ++p) if (planes[p].expert_bytes != sizes[p]) return 0;

    const VkDeviceSize lengths[VIEW_COUNT] = {width * 4, width * 4, width * 4, experts * width * 4, experts * 4, experts * 4, SELECTED * 8,
        (inkling ? 2 : 1) * ff * 4, ff * 4, ff * 4, SELECTED * width * 4, width * 4};
    fg_vk_buffer arena = {0};
    fg_vk_view views[VIEW_COUNT];
    VkDeviceSize end = 0;
    for (unsigned i = 0; i < VIEW_COUNT; ++i) {
        const VkDeviceSize offset = (end + alignment - 1) & ~(alignment - 1);
        views[i] = (fg_vk_view){&arena, offset, lengths[i]};
        end = offset + lengths[i];
    }
    const fg_vk_shape router_shape = {.width = width, .rows = experts, .batch = 1};
    const fg_vk_shape route_shape = {.width = experts, .rows = SELECTED, .batch = 1, .theta = inkling ? 8.0f : 0.0f};
    const fg_vk_shape norm_shape = {.width = width, .batch = 1, .epsilon = 1e-6f};
    const fg_vk_shape weighted = {.width = width, .rows = SELECTED, .batch = 1};
    const fg_vk_op route_op = inkling ? FG_VK_INKLING_ROUTE : FG_VK_MOE_ROUTE;
    const fg_vk_view route_bias = inkling ? views[SELECTION_BIAS] : views[LOGITS];
    uint32_t routes[2 * SELECTED];
    fg_vk_expert_bank bank = {0};

    int pass = fg_vk_expert_bank_open(&bank, vk, SELECTED, sizes) &&
        fg_vk_alloc_buffer(vk, &arena, end, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == VK_SUCCESS &&
        fg_vk_access_buffer(vk, &arena, views[INPUT].offset, input, lengths[INPUT], 1) == VK_SUCCESS &&
        fg_vk_access_buffer(vk, &arena, views[NORM].offset, norm, lengths[NORM], 1) == VK_SUCCESS &&
        fg_vk_access_buffer(vk, &arena, views[ROUTER].offset, router, lengths[ROUTER], 1) == VK_SUCCESS &&
        (!inkling || fg_vk_access_buffer(vk, &arena, views[SELECTION_BIAS].offset,
            bias, lengths[SELECTION_BIAS], 1) == VK_SUCCESS) &&
        fg_vk_graph_begin(graph) &&
        fg_vk_graph_record(graph, FG_VK_RMSNORM, norm_shape, views[INPUT], views[NORM], views[NORMALIZED]) &&
        fg_vk_graph_record(graph, FG_VK_F32_PROJECT, router_shape, views[NORMALIZED], views[ROUTER], views[LOGITS]) &&
        fg_vk_graph_record(graph, route_op, route_shape, views[LOGITS], route_bias, views[ROUTES]) &&
        fg_vk_graph_submit(graph) &&
        fg_vk_access_buffer(vk, &arena, views[ROUTES].offset, routes, sizeof(routes), 0) == VK_SUCCESS &&
        !memcmp(routes, expected, SELECTED * sizeof(uint32_t));
    if (pass) {
        const uint32_t invalid = experts;
        pass = !fg_vk_expert_bank_stage(&bank, model, file_bytes, experts, planes, &invalid, 1) &&
            bank.loaded == 0 &&
            fg_vk_expert_bank_stage(&bank, model, file_bytes, experts, planes, routes, SELECTED) &&
            bank.loaded == SELECTED && fg_vk_graph_begin(graph);
    }
    if (pass) {
        const fg_vk_op ops[] = {FG_VK_Q4_K, FG_VK_Q4_K, FG_VK_Q6_K};
        pass = (inkling ? fg_vk_expert_bank_record_inkling(graph, &bank, width, ff,
                    views[NORMALIZED], views[GATE], views[ACTIVATION], views[BRANCHES]) :
                    fg_vk_expert_bank_record_split(graph, &bank, width, ff, ops, views[NORMALIZED], views + GATE, views[BRANCHES])) &&
               fg_vk_graph_record(graph, FG_VK_MOE_WEIGHTED, weighted, views[BRANCHES], views[ROUTES], views[OUTPUT]) &&
               fg_vk_graph_submit(graph) &&
               fg_vk_access_buffer(vk, &arena, views[OUTPUT].offset, out, lengths[OUTPUT], 0) == VK_SUCCESS;
    }
    fg_vk_expert_bank_close(&bank);
    fg_vk_free_buffer(vk, &arena);
    return pass;
}

int test_routed_experts(fg_vk_graph *graph, const char *path, const char *fixture, int inkling) {
    const uint32_t width = inkling ? 2304 : 2048;
    const uint32_t experts = inkling ? 128 : 256;
    const uint32_t ff = inkling ? 1152 : 512;
    FILE *model = fopen(path, "rb");
    FILE *file = fopen(fixture, "rb");
    if (!model || !file) {
        if (model) fclose(model);
        if (file) fclose(file);
        return 0;
    }
    const int64_t file_bytes = fseeko(model, 0, SEEK_END) == 0 ? ftello(model) : -1;
    char magic[8];
    float input[width], norm[width], expected[width], output[width], bias[experts];
    float *router = malloc(width * experts * sizeof(float));
    uint32_t ids[SELECTED];
    fg_vk_expert_plane planes[3];
    int pass = file_bytes > 0 && router && fread(magic, sizeof(magic), 1, file) == 1 &&
        !memcmp(magic, inkling ? "FGMOEI1" : "FGMOEE2", sizeof(magic)) &&
        fread(planes, sizeof(planes), 1, file) == 1 &&
        fread(input, sizeof(input), 1, file) == 1 &&
        fread(norm, sizeof(norm), 1, file) == 1 &&
        fread(router, width * experts * sizeof(float), 1, file) == 1 &&
        (!inkling || fread(bias, sizeof(bias), 1, file) == 1) &&
        fread(ids, sizeof(ids), 1, file) == 1 &&
        fread(expected, sizeof(expected), 1, file) == 1 &&
        fgetc(file) == EOF && !ferror(file);
    fclose(file);
    if (pass) pass = run_experts(graph, model, (uint64_t)file_bytes, planes, input, norm, router, inkling ? bias : NULL, ids, output, width, experts, ff);
    free(router);
    fclose(model);

    float max_error = 0.0f, peak = 0.0f;
    if (pass) for (unsigned i = 0; i < width; ++i) peak = fmaxf(peak, fabsf(expected[i]));
    pass = pass && peak > 0;
    for (unsigned i = 0; i < width && pass; ++i) {
        const float delta = fabsf(output[i] - expected[i]);
        max_error = fmaxf(max_error, delta);
        pass = isfinite(output[i]) && delta <= peak * 5e-7f + fabsf(expected[i]) * 2e-5f;
    }
    printf("%s routed experts: exact routes, real GGUF staging, %s, weighted output max_abs=%g %s\n",
        inkling ? "Inkling" : "Apex", inkling ? "IQ4_XS/SiTU/IQ4_NL" : "Q4_K/SwiGLU/Q6_K",
        max_error, pass ? "PASS" : "FAIL");
    return pass;
}
