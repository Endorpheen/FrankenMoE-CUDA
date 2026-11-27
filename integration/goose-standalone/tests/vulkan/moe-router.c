#include "graph.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WIDTH = 2048, EXPERTS = 256, SELECTED = 8, BATCH = 4 };
enum { INPUT, WEIGHT, LOGITS, ROUTES, VIEW_COUNT };

int test_moe_router(fg_vk_graph *graph, const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) return 0;
    float input[WIDTH * BATCH];
    const size_t weight_bytes = EXPERTS * WIDTH * sizeof(float);
    float *weights = malloc(weight_bytes);
    uint32_t ids[BATCH][SELECTED];
    float reference[BATCH][SELECTED];
    int pass = weights &&
        fread(input, sizeof(input), 1, file) == 1 &&
        fread(weights, weight_bytes, 1, file) == 1 &&
        fread(ids, sizeof(ids), 1, file) == 1 &&
        fread(reference, sizeof(reference), 1, file) == 1 &&
        fgetc(file) == EOF && !ferror(file);
    fclose(file);
    uint32_t routes[BATCH][2 * SELECTED];
    if (pass) {
        fg_vk_device *vk = graph->vk;
        fg_vk_buffer arena = {0};
        const VkDeviceSize alignment = vk->properties.limits.minStorageBufferOffsetAlignment;
        const VkDeviceSize sizes[] = {sizeof(input), weight_bytes, BATCH * EXPERTS * sizeof(float), sizeof(routes)};
        fg_vk_view views[VIEW_COUNT];
        VkDeviceSize end = 0;
        for (unsigned i = 0; i < VIEW_COUNT; ++i) {
            const VkDeviceSize offset = (end + alignment - 1) & ~(alignment - 1);
            views[i] = (fg_vk_view){&arena, offset, sizes[i]};
            end = offset + sizes[i];
        }
        const fg_vk_shape projection = {.width = WIDTH, .rows = EXPERTS, .batch = BATCH};
        const fg_vk_shape route = {.width = EXPERTS, .rows = SELECTED, .batch = BATCH, .theta = 0.75f};
        pass = fg_vk_alloc_buffer(vk, &arena, end, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == VK_SUCCESS &&
            fg_vk_access_buffer(vk, &arena, views[INPUT].offset, input, sizeof(input), 1) == VK_SUCCESS &&
            fg_vk_access_buffer(vk, &arena, views[WEIGHT].offset, weights, weight_bytes, 1) == VK_SUCCESS &&
            fg_vk_graph_begin(graph) &&
            fg_vk_graph_record(graph, FG_VK_F32_PROJECT, projection, views[INPUT], views[WEIGHT], views[LOGITS]) &&
            fg_vk_graph_record(graph, FG_VK_MOE_ROUTE, route, views[LOGITS], views[LOGITS], views[ROUTES]) &&
            fg_vk_graph_submit(graph) &&
            fg_vk_access_buffer(vk, &arena, views[ROUTES].offset, routes, sizeof(routes), 0) == VK_SUCCESS;
        fg_vk_free_buffer(vk, &arena);
    }
    free(weights);

    float max_error = 0;
    for (unsigned t = 0; t < BATCH && pass; ++t) {
        for (unsigned i = 0; i < SELECTED && pass; ++i) {
            float weight;
            memcpy(&weight, &routes[t][SELECTED + i], sizeof(weight));
            const float delta = fabsf(weight - reference[t][i]);
            max_error = fmaxf(max_error, delta);
            pass = routes[t][i] == ids[t][i] && isfinite(weight) && delta <= 2e-6f;
            if (!pass) fprintf(stderr, "route token=%u slot=%u: id=%u vs %u, weight=%g vs %g\n",
                t, i, routes[t][i], ids[t][i], weight, reference[t][i]);
        }
    }
    printf("Apex real-weight router: exact IDs, tied and invalid logits, max_weight_error=%g %s\n",
        max_error, pass ? "PASS" : "FAIL");
    return pass;
}
