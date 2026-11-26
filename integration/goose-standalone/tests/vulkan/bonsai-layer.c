#include "bonsai-layer.h"
#include "upload.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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

static int read_weights(FILE *fixture, fg_vk_device *vk, const fg_vk_bonsai_layer *layer) {
    fg_vk_upload upload = {0};
    unsigned char *chunk = malloc(IO_CHUNK);
    int ok = chunk && fg_vk_upload_open(&upload, vk, IO_CHUNK);
    for (unsigned i = 0; i < BW_COUNT && ok; ++i) {
        const fg_vk_view v = layer->weights[i];
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

static int check(fg_vk_device *vk, fg_vk_view v, FILE *fixture, float *reference, float *actual, double *worst) {
    const size_t count = (size_t)v.size / sizeof(float);
    if (fread(reference, sizeof(float), count, fixture) != count || !access_view(vk, v, actual, 0)) return 0;
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
        read_weights(fixture, vk, &layer);
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
