#include "graph.h"

#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WIDTH = 96, ROWS = 67, BATCH = 3, Q4_BYTES = (WIDTH / 32 * ROWS * 18 + 3) & ~3 };

static int run_case(fg_vk_graph *graph, unsigned width, unsigned rows, unsigned batch) {
    fg_vk_device *vk = graph->vk;
    float input[WIDTH * BATCH], norm[WIDTH], gate[ROWS * BATCH], output[ROWS * BATCH];
    unsigned char weight[Q4_BYTES] = {0};
    for (unsigned i = 0; i < width * batch; ++i) input[i] = ((int)(i % 31) - 15) / 8.0f;
    for (unsigned i = 0; i < width; ++i) norm[i] = 0.75f + (i % 7) / 16.0f;
    for (unsigned i = 0; i < rows * batch; ++i) gate[i] = ((int)(i % 13) - 6) / 5.0f;
    for (unsigned b = 0; b < rows * (width / 32); ++b) {
        weight[b * 18 + 1] = b % 3 == 0 ? 0xb4 : 0x30;
        for (unsigned q = 0; q < 16; ++q) weight[b * 18 + 2 + q] = (unsigned char)(31 * q + 53 * b);
    }

    fg_vk_buffer arena = {0};
    VkDeviceSize alignment = vk->properties.limits.minStorageBufferOffsetAlignment;
    VkDeviceSize sizes[] = {width * batch * 4, width * 4, width * batch * 4, Q4_BYTES,
                            rows * batch * 4, rows * batch * 4, rows * batch * 4};
    fg_vk_view views[7];
    VkDeviceSize end = 0;
    for (unsigned i = 0; i < 7; ++i) {
        const VkDeviceSize begin = (end + alignment - 1) & ~(alignment - 1);
        views[i] = (fg_vk_view){&arena, begin, sizes[i]};
        end = begin + sizes[i];
    }
    VkResult result = fg_vk_alloc_buffer(vk, &arena, end, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (result != VK_SUCCESS) return fg_vk_error("allocate graph fixture arena", result);

    void *host[] = {input, norm, NULL, weight, NULL, gate, NULL};
    for (unsigned i = 0; i < 7 && result == VK_SUCCESS; ++i) {
        if (host[i]) result = fg_vk_access_buffer(vk, &arena, views[i].offset, host[i], (size_t)sizes[i], 1);
    }
    const fg_vk_shape rms = {.width = width, .batch = batch, .epsilon = 1e-5f};
    const fg_vk_shape mat = {.width = width, .rows = rows, .batch = batch};
    const fg_vk_shape activation = {.width = rows, .batch = batch};
    fg_vk_view short_weight = views[3];
    short_weight.size = 4;
    int pass = result == VK_SUCCESS && fg_vk_graph_begin(graph) &&
        !fg_vk_graph_record(graph, FG_VK_RMSNORM, rms, views[0], views[1], views[0]) &&
        !fg_vk_graph_record(graph, FG_VK_Q4_0, mat, views[2], short_weight, views[4]) &&
        fg_vk_graph_record(graph, FG_VK_RMSNORM, rms, views[0], views[1], views[2]) &&
        fg_vk_graph_record(graph, FG_VK_Q4_0, mat, views[2], views[3], views[4]) &&
        fg_vk_graph_record(graph, FG_VK_SWIGLU, activation, views[4], views[5], views[6]) &&
        fg_vk_graph_submit(graph) &&
        fg_vk_access_buffer(vk, &arena, views[6].offset, output, rows * batch * 4, 0) == VK_SUCCESS;

    double max_err = 0;
    for (unsigned t = 0; pass && t < batch; ++t) {
        double sum_squares = 0;
        for (unsigned i = 0; i < width; ++i) {
            double value = input[t * width + i];
            sum_squares += value * value;
        }
        const double inv_rms = 1.0 / sqrt(sum_squares / width + rms.epsilon);
        for (unsigned row = 0; row < rows; ++row) {
            double dot = 0;
            for (unsigned i = 0; i < width; ++i) {
                unsigned b = row * (width / 32) + i / 32;
                unsigned bits = weight[18 * b + 2 + (i % 16)];
                int quant = (int)((bits >> ((i % 32 >= 16) ? 4 : 0)) & 15u) - 8;
                double scale = b % 3 == 0 ? -0.25 : 0.125;
                dot += quant * scale * input[t * width + i] * inv_rms * norm[i];
            }
            unsigned index = t * rows + row;
            double reference = dot / (1 + exp(-dot)) * gate[index];
            double error = fabs(output[index] - reference);
            if (error > max_err) max_err = error;
            if (!isfinite(output[index]) || error > 2e-5 * (1 + fabs(reference))) {
                fprintf(stderr, "compute mismatch at %u: device=%g reference=%g\n", index, output[index], reference);
                pass = 0;
                break;
            }
        }
    }
    if (pass) printf("width=%u rows=%u batch=%u graph_epoch=%" PRIu64 " max_error=%g PASS\n",
                     width, rows, batch, graph->epoch, max_err);
    fg_vk_free_buffer(vk, &arena);
    return pass;
}

int test_attention(fg_vk_graph *graph);
int test_projection(fg_vk_graph *graph);
int test_moe_quant(fg_vk_graph *graph, const char *path);
int test_moe_router(fg_vk_graph *graph, const char *path);
int test_apex_experts(fg_vk_graph *graph, const char *path, const char *fixture);
int test_recurrent(fg_vk_graph *graph);
int test_bonsai(fg_vk_graph *graph, const char *path);
int test_bonsai_layer(fg_vk_graph *graph, const char *path);
int test_bonsai_attention_layer(fg_vk_graph *graph, const char *path);

int main(int argc, char **argv) {
    if (argc < 3 || argc > 6) {
        fputs("Usage: frankenmoe-vulkan-compute-test DEVICE_INDEX ALLOW_CPU_0_OR_1 "
              "[FFN_FIXTURE [GDN_FIXTURE [ATTENTION_FIXTURE]] | --moe-quant FIXTURE [ROUTER_FIXTURE] | --apex-experts GGUF FIXTURE]\n", stderr);
        return 2;
    }
    char *end;
    uintmax_t index = strtoumax(argv[1], &end, 10);
    if (argv[1][0] < '0' || argv[1][0] > '9' || *end || index >= FG_VK_AUTO_DEVICE ||
        (argv[2][0] != '0' && argv[2][0] != '1') || argv[2][1]) return 2;
    fg_vk_device vk;
    if (!fg_vk_open(&vk, (uint32_t)index, argv[2][0] == '1')) return 1;
    fg_vk_graph graph;
    const int moe_quant = argc >= 5 && !strcmp(argv[3], "--moe-quant");
    const int apex_experts = argc == 6 && !strcmp(argv[3], "--apex-experts");
    int pass = fg_vk_graph_open(&graph, &vk, 40) &&
               run_case(&graph, WIDTH, ROWS, BATCH) && run_case(&graph, 32, 1, 1) && test_attention(&graph) && test_projection(&graph) && test_recurrent(&graph) &&
               (moe_quant ? test_moe_quant(&graph, argv[4]) && (argc < 6 || test_moe_router(&graph, argv[5])) :
                   apex_experts ? test_apex_experts(&graph, argv[4], argv[5]) :
                   (argc == 3 || test_bonsai(&graph, argv[3])) &&
                   (argc < 5 || test_bonsai_layer(&graph, argv[4])) &&
                   (argc < 6 || test_bonsai_attention_layer(&graph, argv[5])));
    fg_vk_graph_close(&graph);
    fg_vk_close(&vk);
    return pass ? 0 : 1;
}
