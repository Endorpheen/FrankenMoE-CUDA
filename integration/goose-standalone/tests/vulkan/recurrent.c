#include "graph.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    DIM = 128, KHEADS = 16, HEADS = 48, CHANNELS = (2 * KHEADS + HEADS) * DIM,
    OUTPUTS = HEADS * DIM, STATE_SIZE = OUTPUTS * DIM, STEPS = 3,
};
enum { INPUT, TAPS, HISTORY, CONV, QKV, GATES, STATE, OUTPUT, VIEWS };

static int host(fg_vk_device *vk, fg_vk_view view, void *data, int write) {
    return fg_vk_access_buffer(vk, view.buffer, view.offset, data, (size_t)view.size, write) == VK_SUCCESS;
}

static void reference_step(const float *input, const float *taps, const float *gates,
    float *history, double *state, double *output) {
    double qkv[CHANNELS];
    for (unsigned c = 0; c < CHANNELS; ++c) {
        double value = 0;
        for (unsigned i = 0; i < 3; ++i) value += (double)history[i * CHANNELS + c] * taps[4 * c + i];
        value += (double)input[c] * taps[4 * c + 3];
        qkv[c] = value / (1.0 + exp(-value));
        history[c] = history[CHANNELS + c];
        history[CHANNELS + c] = history[2 * CHANNELS + c];
        history[2 * CHANNELS + c] = input[c];
    }
    for (unsigned h = 0; h < 2 * KHEADS; ++h) {
        double square = 0;
        for (unsigned d = 0; d < DIM; ++d) square += qkv[h * DIM + d] * qkv[h * DIM + d];
        const double norm = fmax(sqrt(square), 1e-6);
        for (unsigned d = 0; d < DIM; ++d) qkv[h * DIM + d] /= norm;
    }
    for (unsigned h = 0; h < HEADS; ++h) {
        const double decay = exp(gates[h]);
        const double *q = qkv + (h % KHEADS) * DIM;
        const double *k = q + KHEADS * DIM;
        for (unsigned r = 0; r < DIM; ++r) {
            unsigned index = h * DIM + r;
            double *row = state + index * DIM;
            double dot = 0;
            for (unsigned d = 0; d < DIM; ++d) {
                row[d] *= decay;
                dot += row[d] * k[d];
            }
            const double delta = (qkv[2 * KHEADS * DIM + index] - dot) * gates[HEADS + h];
            double score = 0;
            for (unsigned d = 0; d < DIM; ++d) {
                row[d] += delta * k[d];
                score += row[d] * q[d];
            }
            output[index] = score / sqrt(DIM);
        }
    }
}

static int verify(const float *actual, const double *ref, unsigned count, double *worst) {
    for (unsigned i = 0; i < count; ++i) {
        const double error = fabs((double)actual[i] - ref[i]);
        if (!isfinite(actual[i]) || error > 2e-6 + 2e-5 * fabs(ref[i])) {
            fprintf(stderr, "recurrent reference mismatch at %u: device=%g reference=%g\n", i, actual[i], ref[i]);
            return 0;
        }
        if (error > *worst) *worst = error;
    }
    return 1;
}

int test_recurrent(fg_vk_graph *graph) {
    fg_vk_device *vk = graph->vk;
    fg_vk_buffer arena = {0};
    const VkDeviceSize alignment = vk->properties.limits.minStorageBufferOffsetAlignment;
    const unsigned counts[VIEWS] = {CHANNELS, 4 * CHANNELS, 3 * CHANNELS, CHANNELS, CHANNELS,
                                    2 * HEADS, STATE_SIZE, OUTPUTS};
    fg_vk_view v[VIEWS];
    VkDeviceSize end = 0;
    for (unsigned i = 0; i < VIEWS; ++i) {
        end = (end + alignment - 1) & ~(alignment - 1);
        v[i] = (fg_vk_view){&arena, end, counts[i] * sizeof(float)};
        end += v[i].size;
    }

    float *zero = calloc(STATE_SIZE, sizeof(*zero));
    float *observed = malloc(STATE_SIZE * sizeof(*observed));
    float *snapshot = malloc(STATE_SIZE * sizeof(*snapshot));
    double *ref = calloc(STATE_SIZE, sizeof(*ref));
    float history[3 * CHANNELS] = {0}, history_gpu[3 * CHANNELS], input[CHANNELS], taps[CHANNELS * 4], gates[HEADS * 2];
    float output_gpu[OUTPUTS], output_snapshot[OUTPUTS];
    double output[OUTPUTS], worst_state = 0, worst_output = 0;
    int ok = zero && observed && snapshot && ref &&
        fg_vk_alloc_buffer(vk, &arena, end, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == VK_SUCCESS;
    for (unsigned i = 0; i < 4 * CHANNELS; ++i) taps[i] = ((int)((i * 13) % 53) - 26) / 128.0f;
    const fg_vk_shape norm = {.width = DIM, .rows = 2 * KHEADS + HEADS, .kv_heads = KHEADS,
                              .batch = 1, .epsilon = 1e-6f};
    const fg_vk_shape gdn = {.width = DIM, .rows = HEADS, .kv_heads = KHEADS, .batch = 1};
    if (ok) ok = host(vk, v[TAPS], taps, 1);
    for (unsigned replay = 0; ok && replay < 2; ++replay) {
        ok = host(vk, v[STATE], zero, 1) && host(vk, v[HISTORY], zero, 1);
        for (unsigned step = 0; ok && step < STEPS; ++step) {
            for (unsigned i = 0; i < CHANNELS; ++i) {
                input[i] = ((int)((7 * i + step * 31) % 61) - 30) / 32.0f;
            }
            for (unsigned h = 0; h < HEADS; ++h) {
                gates[h] = -(float)((h + step) % 13 + 1) / 32.0f;
                gates[HEADS + h] = (float)((3 * h + step) % 27 + 1) / 32.0f;
            }
            ok = host(vk, v[INPUT], input, 1) && host(vk, v[GATES], gates, 1) &&
                fg_vk_graph_begin(graph) &&
                !fg_vk_graph_gdn(graph, gdn, v[CONV], v[CONV], v[GATES], v[OUTPUT]) &&
                fg_vk_graph_conv(graph, CHANNELS, v[INPUT], v[TAPS], v[HISTORY], v[CONV]) &&
                fg_vk_graph_record(graph, FG_VK_QK_NORM, norm, v[CONV], v[CONV], v[QKV]) &&
                fg_vk_graph_gdn(graph, gdn, v[QKV], v[STATE], v[GATES], v[OUTPUT]) &&
                fg_vk_graph_submit(graph) && host(vk, v[STATE], observed, 0) &&
                host(vk, v[OUTPUT], output_gpu, 0) && host(vk, v[HISTORY], history_gpu, 0);
            if (ok && replay == 0) {
                reference_step(input, taps, gates, history, ref, output);
                ok = verify(observed, ref, STATE_SIZE, &worst_state) &&
                     verify(output_gpu, output, OUTPUTS, &worst_output) &&
                     memcmp(history, history_gpu, sizeof(history)) == 0;
            }
        }
        if (ok && replay == 0) {
            memcpy(snapshot, observed, STATE_SIZE * sizeof(*snapshot));
            memcpy(output_snapshot, output_gpu, sizeof(output_snapshot));
        } else if (ok) {
            ok = memcmp(snapshot, observed, STATE_SIZE * sizeof(*snapshot)) == 0 &&
                 memcmp(output_snapshot, output_gpu, sizeof(output_snapshot)) == 0 &&
                 memcmp(history, history_gpu, sizeof(history)) == 0;
        }
    }
    if (ok) printf("Bonsai causal conv, Q/K L2, GDN 48:16 x 128, 3 steps: state=%g output=%g; bitwise replay PASS\n",
                   worst_state, worst_output);
    else fputs("Bonsai recurrent graph fixture failed\n", stderr);
    fg_vk_free_buffer(vk, &arena);
    free(ref); free(snapshot); free(observed); free(zero);
    return ok;
}
