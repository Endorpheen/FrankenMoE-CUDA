#include "moe-epilogue.h"
#include "i2-probe-moe-image.h"

static struct epilogue_state {
        struct device_buffer buffer;
        int length;
} i2_epilogue_states[INKLING_VISITS];

int64_t fg_cuda_inkling_moe_epilogue(sl_u8 input, sl_u8 residual, sl_u8 weights, sl_u8 output,
                                    int slot, int position, int rows) {
    enum { width = INKLING_MOE_EPILOGUE_WIDTH };
    const int64_t row_bytes = width * sizeof(float);
    if (slot < 0 || slot >= INKLING_VISITS || position < 0 || rows <= 0 ||
        rows > INKLING_PREFILL_LIMIT || position > INKLING_CONTEXT_LIMIT - rows ||
        input.len != row_bytes * rows || residual.len != input.len ||
        output.len != input.len || weights.len != 4 * row_bytes)
        return -1;

    if (start_context() != 1) return -1;
    static Module module;
    static Function kernel;
    struct epilogue_state *state = &i2_epilogue_states[slot];
    int error = position > 0 && state->length != position ? -1 : 0;
    if (!error && !module) error = cuda.module_load(&module, moe_epilogue_cubin);
    if (!error && !kernel) error = cuda.function_get(&kernel, module, "goose_i2_moe_epilogue");
    if (!error && !state->buffer.address) {
        error = reserve(&state->buffer, 7 * row_bytes);
        if (!error) error = cuda.upload(state->buffer.address, weights.data, weights.len);
    }
    DevicePtr coefficient = state->buffer.address;
    DevicePtr history = coefficient + 4 * row_bytes;
    if (!error && position == 0) error = cuda.zero(history, 0, 3 * row_bytes);

    const uint64_t host = (uint64_t)(uintptr_t)output.data;
    DevicePtr src = 0, res = 0, dst = 0;
    const bool retain_output = i2_arena_probe.active && host && host == inkling_moe_resident_host &&
        output.len == 36 * (int64_t)MIB_BYTES;
    if (!error && retain_output) {
        const size_t split = activation_usable_bytes();
        if (cuda.activations.bytes - split < 6 * (size_t)output.len) error = -1;
        else dst = inkling_moe_output_slot ? inkling_moe_output_slot :
                   cuda.activations.address + split + 3 * (size_t)output.len;
    } else if (!error) {
        error = activation_output_for_input(host, output.len, &dst, input, NULL);
    }
    if (!error) error = activation_input(input, &cuda.input, &src);
    if (!error) error = activation_input(residual, &cuda.weight, &res);
    void *args[] = {&src, &res, &coefficient, &history, &dst, &rows};
    if (!error) error = cuda.launch(kernel, width / 128, 1, 1, 128, 1, 1, 0, NULL, args, NULL);
    if (!error && !retain_output) error = activation_publish(host, output.len, dst, false);
    if (!error) {
        inkling_moe_current = retain_output ? dst : 0;
        state->length = position + rows;
    }
    const int popped = finish_context();
    if (!error && !popped) return 1;
    fprintf(stderr, "Goose CUDA Inkling MoE epilogue: slot=%d position=%d error=%d context=%d\n",
            slot, position, error, popped);
    return -1;
}

static struct device_buffer i2_epilogue_backup;

bool inkling_draft_moe_snapshot(bool restore, int position) {
    enum { row_bytes = INKLING_MOE_EPILOGUE_WIDTH * sizeof(float), bytes = 3 * row_bytes };
    if (start_context() != 1) return false;
    int error = cuda.sync();
    if (!error && !restore) error = reserve(&i2_epilogue_backup, INKLING_VISITS * (size_t)bytes);
    if (!error && i2_epilogue_backup.bytes < INKLING_VISITS * (size_t)bytes) error = -1;
    for (int i = 0; i < INKLING_VISITS && !error; ++i) {
        struct epilogue_state *state = &i2_epilogue_states[i];
        if (!state->buffer.address || (!restore && state->length != position) ||
            (restore && state->length < position)) {
            error = -1;
            break;
        }
        const DevicePtr history = state->buffer.address + 4 * row_bytes;
        const DevicePtr backup = i2_epilogue_backup.address + i * (size_t)bytes;
        error = restore ? cuda.copy_device(history, backup, bytes) :
                          cuda.copy_device(backup, history, bytes);
        if (!error && restore) state->length = position;
    }
    const int popped = finish_context();
    if (!error && !popped) fprintf(stderr, "Inkling draft MoE restore=%d bytes=%d\n",
                                    restore, INKLING_VISITS * bytes);
    return !error && !popped;
}
