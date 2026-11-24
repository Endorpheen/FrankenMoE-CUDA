#include "main.h"
#include "ggml.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

extern int64_t fg_cuda_activation_read_address(uint64_t host, int64_t bytes);
extern int64_t i2_cpu_indexed(uint64_t weights, uint64_t input, uint64_t ids, uint64_t output,
                             int64_t kind, int64_t width, int64_t row_bytes, int64_t rows,
                             int64_t input_slots, int64_t count, int64_t tasks);
extern uint8_t __real_fg143_graph_regular(uint64_t address, sl_i64 packet, int64_t mode,
                                         sl_i64 sizes, uint64_t workspace);

static _Thread_local bool enabled;

void i2_simd_scope(bool active) {
    enabled = active;
}

int64_t i2_load_route(uint64_t address, int64_t route) {
    return ((const int32_t *)(uintptr_t)address)[route];
}

void i2_store_result(uint64_t address, int64_t index, float value) {
    ((float *)(uintptr_t)address)[index] = value;
}

static uint64_t data(const struct ggml_tensor *tensor) {
    return (uint64_t)(uintptr_t)tensor->data;
}

uint8_t __wrap_fg143_graph_regular(uint64_t address, sl_i64 packet, int64_t mode,
                                  sl_i64 sizes, uint64_t workspace) {
    const struct ggml_tensor *out = (const void *)(uintptr_t)address;
    if (!enabled || mode != 0 || out->op != GGML_OP_MUL_MAT_ID) {
        return __real_fg143_graph_regular(address, packet, mode, sizes, workspace);
    }
    const struct ggml_tensor *w = out->src[0], *input = out->src[1], *ids = out->src[2];
    if ((w->type != GGML_TYPE_IQ4_NL && w->type != GGML_TYPE_IQ4_XS) ||
        input->type != GGML_TYPE_F32 || out->type != GGML_TYPE_F32 || ids->type != GGML_TYPE_I32 ||
        (input->ne[1] != 1 && input->ne[1] != 8) || ids->ne[0] != 8 || ids->ne[1] != input->ne[2] ||
        w->ne[2] != 128 || w->ne[3] != 1 || input->ne[3] != 1 || ids->ne[2] != 1 || ids->ne[3] != 1 ||
        out->ne[0] != w->ne[1] || out->ne[1] != 8 || out->ne[2] != ids->ne[1] || out->ne[3] != 1 ||
        w->ne[0] <= 0 || w->ne[0] % 32 || w->ne[0] != input->ne[0] ||
        !ggml_is_contiguous(w) || !ggml_is_contiguous(input) ||
        !ggml_is_contiguous(ids) || !ggml_is_contiguous(out)) {
        return __real_fg143_graph_regular(address, packet, mode, sizes, workspace);
    }
    if (fg_cuda_activation_read_address(data(input), ggml_nbytes(input)) ||
        fg_cuda_activation_read_address(data(ids), ggml_nbytes(ids))) {
        return 0;
    }
    static int tasks;
    if (!tasks) {
        const char *setting = getenv("I2_CPU_EXPERT_THREADS");
        tasks = setting ? atoi(setting) : 1;
        if (tasks < 1 || tasks > 8) return 0;
    }
    return i2_cpu_indexed(data(w), data(input), data(ids), data(out), w->type, w->ne[0], w->nb[1],
                          w->ne[1], input->ne[1], ids->ne[1], tasks) == 8 * ids->ne[1];
}
