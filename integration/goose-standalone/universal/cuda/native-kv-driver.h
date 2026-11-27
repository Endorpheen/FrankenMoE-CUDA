#include "native-kv.h"
#include "flash-native-image.h"
#include "flash-tiles-image.h"
#include "attention-gate-image.h"
#include "attention-input-image.h"

enum { NK_QUERY = 3072, NK_REL = 384, NK_KV = 1024, NK_HEADS = 24, NK_BIAS = 16384,
       NK_FLASH_QUERY_CHUNK = 128, NK_FLASH_KEY_PART = 2048 };

struct native_kv_slot {
    struct device_buffer cache, prepare_state;
    int length, rows;
    bool sliding;
    enum inkling_kv_format format;
};

struct native_kv_rows {
    DevicePtr keys, values, positions;
};

static struct {
    Module module;
    Function pack, attention, attention_decode, attention_prefill, merge;
    struct native_kv_slot slots[INKLING_VISITS];
    struct device_buffer window, input, staging, partials, projection;
    struct device_buffer raw, projected_input, prepared_kv;
    uint64_t calls, queries, uploads;
} nk;

uint64_t inkling_loop_storage(void) {
    return nk.projected_input.bytes >= 3 * (size_t)i2_loop_plane ? nk.projected_input.address : 0;
}

uint64_t inkling_prefill_full_scratch(size_t bytes) {
    return nk.raw.bytes >= bytes ? nk.raw.address : 0;
}

static size_t nk_row_bytes(enum inkling_kv_format format) {
    return format == INKLING_KV_Q4_0 ? NK_KV / 32 * 18 : NK_KV * sizeof(uint16_t);
}

static struct native_kv_rows nk_rows(DevicePtr base, int capacity, size_t row_bytes) {
    const DevicePtr size = (DevicePtr)capacity * row_bytes;
    return (struct native_kv_rows){base, base + size, base + 2 * size};
}

static size_t nk_cache_bytes(int rows, size_t row_bytes) {
    return (size_t)rows * (2 * row_bytes + sizeof(int64_t));
}

static int nk_load(void) {
    if (nk.merge) return 0;
    int error = cuda.module_load(&nk.module, flash_native_cubin);
    if (!error) error = cuda.function_get(&nk.pack, nk.module, "goose_i2_pack_f16");
    if (!error) error = cuda.function_get(&nk.attention, nk.module, "goose_i2_flash_mma");
    if (!error) error = cuda.function_get(&nk.merge, nk.module, "goose_i2_flash_mma_merge");
    return error;
}

static int nk_copy(struct native_kv_rows dst, int dst_first, struct native_kv_rows src, int src_first, int count, size_t row) {
    if (count == 0) return 0;
    const size_t bytes = row * count;
    int error = cuda.copy_device(dst.keys + dst_first * row, src.keys + src_first * row, bytes);
    if (!error) error = cuda.copy_device(dst.values + dst_first * row, src.values + src_first * row, bytes);
    if (!error) error = cuda.copy_device(dst.positions + dst_first * sizeof(int64_t),
        src.positions + src_first * sizeof(int64_t), (size_t)count * sizeof(int64_t));
    return error;
}

static int nk_upload(DevicePtr dst, const void *src, size_t bytes) {
    int error = cuda.upload(dst, src, bytes);
    if (!error) nk.uploads += bytes;
    return error;
}

static int nk_pack(DevicePtr dst, const float *src, int count) {
    int error = reserve(&nk.staging, (size_t)count * sizeof(float));
    if (!error) error = nk_upload(nk.staging.address, src, (size_t)count * sizeof(float));
    if (error) return error;
    void *args[] = {&nk.staging.address, &dst, &count};
    return cuda.launch(nk.pack, (unsigned)(count + 255) / 256, 1, 1, 256, 1, 1, 0, NULL, args, NULL);
}

static int nk_pack_q4(DevicePtr dst, const float *src, int rows) {
    uint64_t stride = NK_KV * sizeof(float);
    const size_t bytes = (size_t)rows * stride;
    int error = reserve(&nk.staging, bytes);
    if (!error) error = nk_upload(nk.staging.address, src, bytes);
    if (error) return error;
    uint32_t blocks = NK_KV / 32;
    uint64_t tasks = (uint64_t)rows * blocks;
    void *args[] = {&nk.staging.address, &dst, &stride, &blocks, &tasks};
    return cuda.launch(cuda.q4_rows, (unsigned)(tasks + 127) / 128, 1, 1, 128, 1, 1, 0, NULL, args, NULL);
}

static int nk_pack_kv(DevicePtr dst, const float *src, int rows, enum inkling_kv_format format) {
    return format == INKLING_KV_Q4_0 ? nk_pack_q4(dst, src, rows) : nk_pack(dst, src, rows * NK_KV);
}

static int nk_compute(struct native_kv_rows rows, int history, int total, const struct inkling_attention_batch *batch,
                      DevicePtr q, DevicePtr r, DevicePtr sink, DevicePtr query_pos, DevicePtr bias, DevicePtr out) {
    const int64_t window = batch->sliding ? INKLING_WINDOW : 0;
    const size_t row_bytes = nk_row_bytes(batch->format);
    const float scale = 0x1.6a09e6p-4f;
    const bool exact_tail = batch->count > 1 && batch->count <= 16 && i2_exact_verify;
    const int query_chunk = exact_tail ? 1 : NK_FLASH_QUERY_CHUNK;
    for (int first = 0; first < batch->count; first += query_chunk) {
        int64_t queries = batch->count - first;
        if (queries > query_chunk) queries = query_chunk;
        const int64_t last_key = history + first + queries;
        if (last_key > total) return -1;
        const int64_t lower = batch->sliding && history + first >= INKLING_WINDOW ?
                              history + first - INKLING_WINDOW + 1 : 0;
        int64_t keys = last_key - lower;
        int64_t parts = (keys + NK_FLASH_KEY_PART - 1) / NK_FLASH_KEY_PART;
        const size_t slots = (size_t)queries * NK_HEADS * parts;
        int error = reserve(&nk.partials, slots * (128 + 2) * sizeof(float));
        if (error) return error;
        DevicePtr partial = nk.partials.address, meta = partial + slots * 128 * sizeof(float);
        DevicePtr q0 = q + (size_t)first * NK_QUERY * sizeof(uint16_t);
        DevicePtr k0 = rows.keys + lower * row_bytes, v0 = rows.values + lower * row_bytes;
        DevicePtr r0 = r + (size_t)first * NK_REL * sizeof(float);
        DevicePtr s0 = sink + (size_t)first * NK_HEADS * sizeof(float);
        DevicePtr kp = rows.positions + lower * sizeof(int64_t);
        DevicePtr qp = query_pos + (size_t)first * sizeof(int64_t);
        DevicePtr out0 = out + (size_t)first * NK_QUERY * sizeof(float);
        float scale_arg = scale;
        int64_t window_arg = window;
        int32_t q4 = batch->format == INKLING_KV_Q4_0;
        if (queries == 1 && (batch->count == 1 || exact_tail) &&
                fg_standalone_option_enabled("I2_FLASH_PARALLEL_SCORES")) q4 |= 2;
        void *a[] = {&q0, &k0, &v0, &r0, &bias, &s0, &kp, &qp, &partial, &meta,
                     &keys, &queries, &parts, &window_arg, &scale_arg, &q4};
        void *b[] = {&partial, &meta, &out0, &parts};
        Function kernel = nk.attention;
        if ((q4 & 3) == 3 && fg_standalone_option_enabled("I2_DECODE_WARP_SOFTMAX")) {
            if (!nk.attention_decode) {
                error = cuda.function_get(&nk.attention_decode, nk.module, "goose_i2_flash_mma_decode");
                if (error) return error;
            }
            kernel = nk.attention_decode;
        } else if (queries > 1 && q4 == 1 && fg_standalone_option_enabled("I2_PREFILL_PARALLEL_RELATIVE")) {
            if (!nk.attention_prefill) {
                error = cuda.function_get(&nk.attention_prefill, nk.module, "goose_i2_flash_mma_prefill");
                if (error) return error;
            }
            kernel = nk.attention_prefill;
            static int use_tiles = -1;
            if (use_tiles < 0) {
                const char *setting = getenv("I2_FLASH_TILE_SOFTMAX");
                use_tiles = !setting || strcmp(setting, "0") != 0;
            }
            if (use_tiles && !batch->sliding) {
                static Module module;
                static Function selected;
                if (!module) error = cuda.module_load(&module, flash_tiles_cubin);
                if (!error && !selected) {
                    error = cuda.function_get(&selected, module, "i2_flash_prefill_all_softmax");
                    if (!error) fputs("Inkling Flash: tile softmax active for full attention\n", stderr);
                }
                if (error) return error;
                kernel = selected;
            }
        }
        error = cuda.launch(kernel, (unsigned)(queries + 15) / 16, NK_HEADS, (unsigned)parts,
                            128, 1, 1, 0, NULL, a, NULL);
        if (!error) error = cuda.launch(nk.merge, (unsigned)queries * NK_HEADS, 1, 1, 128, 1, 1, 0, NULL, b, NULL);
        if (error) return error;
    }
    return 0;
}

#include "i2-decode-q8-image.h"

static int nk_project_decode_inputs(const DevicePtr *input, DevicePtr output,
                                    const uint8_t *const *sources, const int *widths, bool sliding) {
    static Module module;
    static Function kernel;
    static bool reported;
    DevicePtr weights[8] = {0};
    const int count = sliding ? 6 : 8;
    for (int i = 0; i < count; ++i) {
        if (!dense_weight((uint64_t)(uintptr_t)sources[i], (int64_t)widths[i] * 2448, &weights[i]))
            return 0;
    }
    int error = module ? 0 : cuda.module_load(&module, i2_decode_q8_image);
    if (!error && !kernel) {
        error = cuda.function_get(&kernel, module, "goose_i2_matrix_q8_attention_projections");
    }
    DevicePtr q = input[0], k = input[1], v = input[2];
    void *args[] = {&q, &k, &v, &output, &weights[0], &weights[1], &weights[2], &weights[3],
                    &weights[4], &weights[5], &weights[6], &weights[7]};
    if (!error) error = cuda.launch(kernel, (unsigned)(sliding ? 8600 : 8664) / 4, 1, 1,
                                    128, 1, 1, 0, NULL, args, NULL);
    if (error) return -1;
    ++cuda_matrix_launches;
    if (!reported) {
        fprintf(stderr, "Inkling fused Q8 decode projections active\n");
        reported = true;
    }
    return 1;
}

static int nk_project_q8(DevicePtr input, DevicePtr output, const uint8_t *weight, int width,
                         int rows, int count) {
    const int64_t stride = width / 32 * 34;
    int64_t jobs[2 * INKLING_PREFILL_LIMIT];
    for (int i = 0; i < count; ++i) {
        jobs[2 * i] = (int64_t)i * width * sizeof(float);
        jobs[2 * i + 1] = (int64_t)i * rows * sizeof(float);
    }
    const DevicePtr prior_input = matrix_input, prior_output = matrix_output;
    matrix_input = input;
    matrix_output = output;
    const int error = fg_cuda_plane((uint64_t)(uintptr_t)weight, rows * stride, stride, width, 8,
                                    rows, (sl_i64){jobs, 2 * count});
    matrix_input = prior_input;
    matrix_output = prior_output;
    return error;
}

static int nk_pack_device(DevicePtr dst, DevicePtr src, int count, enum inkling_kv_format format) {
    if (format != INKLING_KV_Q4_0) {
        int elements = count * NK_KV;
        void *args[] = {&src, &dst, &elements};
        return cuda.launch(nk.pack, (unsigned)(elements + 255) / 256, 1, 1, 256, 1, 1, 0, NULL, args, NULL);
    }
    uint64_t stride = NK_KV * sizeof(float);
    uint32_t blocks = NK_KV / 32;
    uint64_t tasks = (uint64_t)count * blocks;
    void *args[] = {&src, &dst, &stride, &blocks, &tasks};
    return cuda.launch(cuda.q4_rows, (unsigned)(tasks + 127) / 128, 1, 1, 128, 1, 1, 0, NULL, args, NULL);
}

static int nk_prepare_device(struct native_kv_slot *slot, const struct inkling_attention_batch *batch,
    const struct inkling_attention_input_projections *src, DevicePtr q, DevicePtr r,
    DevicePtr *sink, DevicePtr *gate, DevicePtr key_dst, DevicePtr value_dst, bool graph) {
    enum { hidden = 2304, packed = 8664, coeffs = 3072 + 384 + 8192, history = 2 * 3 * NK_KV };
    int count = batch->count, first = batch->first, sliding = batch->sliding;
    const size_t row_input = hidden * sizeof(float) * (size_t)count;
    const size_t row_kv = NK_KV * sizeof(float) * (size_t)count;
    const size_t coeff_bytes = coeffs * sizeof(float);
    const int widths[] = {3072, 1024, 1024, 384, 24, 3072, 32, 32};
    const int source[] = {0, 1, 2, 0, 0, 0, 1, 2};
    static Module module;
    static Function prepare, update;
    int error = 0;
    if (!module) error = cuda.module_load(&module, attention_input_cubin);
    if (!error && !prepare) error = cuda.function_get(&prepare, module, "goose_i2_prepare_attention");
    if (!error && !update) error = cuda.function_get(&update, module, "goose_i2_update_history");
    if (!error) error = reserve(&nk.raw, (size_t)count * packed * sizeof(float));
    if (!error) error = reserve(&nk.projected_input, 3 * row_input);
    if (!error) error = reserve(&nk.prepared_kv, 2 * row_kv);
    if (!error && !slot->prepare_state.address) {
        error = reserve(&slot->prepare_state, coeff_bytes + history * sizeof(float));
        const DevicePtr base = slot->prepare_state.address;
        if (!error) error = nk_upload(base, src->q_clip, 3072 * sizeof(float));
        if (!error) error = nk_upload(base + 3072 * sizeof(float), src->r_clip, 384 * sizeof(float));
        if (!error) error = nk_upload(base + (3072 + 384) * sizeof(float), src->key_kernel, 4096 * sizeof(float));
        if (!error) error = nk_upload(base + (3072 + 384 + 4096) * sizeof(float), src->value_kernel, 4096 * sizeof(float));
    }
    DevicePtr past = slot->prepare_state.address + coeff_bytes;
    if (!error && first == 0) error = cuda.zero(past, 0, history * sizeof(float));
    DevicePtr input[3] = {0};
    for (int i = 0; !error && i < 3; ++i) {
        const uint64_t host = (uint64_t)(uintptr_t)src->input[i];
        if (graph) input[i] = activation_device(host, row_input);
        if (!input[i]) {
            if ((i2_device_loop.storage && nk.projected_input.address == i2_device_loop.storage) ||
                (inkling_moe_current && nk.projected_input.address == inkling_moe_current)) {
                fputs("Inkling loop storage conflicts with attention input staging\n", stderr);
                return -1;
            }
            if (graph) error = activation_sync_range(host, row_input, false);
            input[i] = nk.projected_input.address + i * row_input;
            if (!error) error = nk_upload(input[i], src->input[i], row_input);
        }
    }
    int packed_projection = 0;
    const char *fused = getenv("I2_DECODE_Q8_PROJECTION_PACK");
    if (!error && graph && count == 1 && i2_matrix_enabled() &&
        (!fused || strcmp(fused, "0") != 0)) {
        packed_projection = nk_project_decode_inputs(input, nk.raw.address, src->weights, widths, sliding);
        if (packed_projection < 0) error = -1;
    }
    if (packed_projection == 0) {
        DevicePtr next = nk.raw.address;
        for (int i = 0; !error && i < (sliding ? 6 : 8); ++i) {
            error = nk_project_q8(input[source[i]], next,
                                  src->weights[i], hidden, widths[i], count);
            next += (size_t)count * widths[i] * sizeof(float);
        }
    }
    DevicePtr key = nk.prepared_kv.address, value = key + row_kv;
    void *args[] = {&nk.raw.address, &slot->prepare_state.address, &past, &q, &r, &key,
                    &value, &count, &first, &sliding};
    if (!error) error = cuda.launch(prepare, 64, count, 1, 128, 1, 1, 0, NULL, args, NULL);
    void *advance[] = {&nk.raw.address, &past, &count};
    if (!error) error = cuda.launch(update, 16, 1, 1, 128, 1, 1, 0, NULL, advance, NULL);
    if (!error) error = nk_pack_device(key_dst, key, count, batch->format);
    if (!error) error = nk_pack_device(value_dst, value, count, batch->format);
    *sink = nk.raw.address + (size_t)5504 * count * sizeof(float);
    *gate = nk.raw.address + (size_t)5528 * count * sizeof(float);
    return error;
}

#include "i2-parallel-conv-image.h"

static _Thread_local const float *i2_conv_weights;
static _Thread_local float *i2_conv_history;

void fg_cuda_i2_prefill_conv(const float *weights, float *history) {
    i2_conv_weights = weights;
    i2_conv_history = history;
}

static int nk_parallel_conv(DevicePtr source, DevicePtr output, int rows) {
    enum { row_bytes = 2304 * sizeof(float) };
    static Module module;
    static Function apply, commit;
    static struct device_buffer state;
    static bool reported;
    int error = module ? 0 : cuda.module_load(&module, i2_parallel_conv_image);
    if (!error && !apply) error = cuda.function_get(&apply, module, "i2_postconv_parallel");
    if (!error && !commit) error = cuda.function_get(&commit, module, "i2_postconv_commit");
    if (!error) error = reserve(&state, 7 * row_bytes);
    DevicePtr weights = state.address;
    DevicePtr history = weights + 4 * row_bytes;
    if (!error) error = nk_upload(weights, i2_conv_weights, 4 * row_bytes);
    if (!error) error = nk_upload(history, i2_conv_history, 3 * row_bytes);
    void *args[] = {&source, &weights, &history, &output, &rows};
    void *save[] = {&source, &history, &rows};
    if (!error) error = cuda.launch(apply, 18, rows, 1, 128, 1, 1, 0, NULL, args, NULL);
    if (!error) error = cuda.launch(commit, 18, 1, 1, 128, 1, 1, 0, NULL, save, NULL);
    if (!error) error = cuda.download(i2_conv_history, history, 3 * row_bytes);
    if (!error && !reported) {
        fputs("Inkling batched CUDA convolution active\n", stderr);
        reported = true;
    }
    return error;
}

static int nk_draft_fingerprint(const char *label, int count, DevicePtr device, size_t bytes) {
    unsigned char host[4 * NK_QUERY];
    if (bytes > sizeof host) return -1;
    const int error = cuda.download(host, device, bytes);
    if (error) return error;
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < bytes; ++i) hash = (hash ^ host[i]) * UINT64_C(1099511628211);
    fprintf(stderr, "Inkling draft stage %s count=%d hash=%016" PRIx64 "\n", label, count, hash);
    return 0;
}

static int nk_output_project(const struct inkling_attention_batch *batch,
                             const struct inkling_attention_projection *projection, DevicePtr out, DevicePtr device_gate, bool graph) {
    const size_t rows = projection->rows;
    static Module module;
    static Function gate;
    int error = 0;
    if (!module) error = cuda.module_load(&module, attention_gate_cubin);
    if (!error && !gate) error = cuda.function_get(&gate, module, "goose_i2_attention_gate");

    uint32_t elements = (uint32_t)batch->count * NK_QUERY;
    const size_t gate_bytes = (size_t)elements * sizeof(float);
    const size_t target_bytes = (size_t)batch->count * rows * sizeof(float);
    if (!error && !device_gate) {
        error = reserve(&nk.staging, gate_bytes);
        if (!error) error = nk_upload(nk.staging.address, projection->gate, gate_bytes);
        device_gate = nk.staging.address;
    }
    DevicePtr final = 0;
    const uint64_t output = (uint64_t)(uintptr_t)batch->output;
    const bool retain_conv = graph && output == i2_residual_host && output &&
        target_bytes == 36 * (size_t)MIB_BYTES && i2_conv_weights && i2_conv_history;
    if (!error && retain_conv) {
        if (!cuda.activations.address) error = reserve(&cuda.activations, cuda.activation_capacity);
        const size_t split = activation_usable_bytes();
        if (error || cuda.activations.bytes - split < 6 * target_bytes) return -1;
        final = cuda.activations.address + split + 4 * target_bytes;
    } else if (!error && graph) {
        error = activation_output(output, target_bytes, &final);
    }
    if (!error && !graph) {
        error = reserve(&nk.projection, target_bytes);
        final = nk.projection.address;
    }
    void *args[] = {&out, &device_gate, &elements};
    if (!error) error = cuda.launch(gate, (elements + 255) / 256, 1, 1, 256, 1, 1, 0, NULL, args, NULL);
    if (error) return error;

    const bool convolve = graph && batch->count > 1 && i2_conv_weights && i2_conv_history;
    DevicePtr projected = convolve ? nk.raw.address : final;
    if (convolve && nk.raw.bytes < target_bytes) return -1;
    error = nk_project_q8(out, projected, projection->weights, NK_QUERY, rows, batch->count);
    const int row = 4097 - batch->first;
    const bool observe = batch->slot == 0 && row >= 0 && row < batch->count &&
        batch->count <= 16 && getenv("I2_DRAFT_PARITY");
    if (!error && observe) error = nk_draft_fingerprint("output-project", batch->count, projected + row * rows * sizeof(float), rows * sizeof(float));
    if (!error && convolve) error = nk_parallel_conv(projected, final, batch->count);
    if (!error && observe && convolve) error = nk_draft_fingerprint("gpu-postconv", batch->count, final + row * rows * sizeof(float), rows * sizeof(float));
    if (!error && retain_conv) {
        if (nk.raw.bytes < 2 * target_bytes) return -1;
        i2_residual_device = final;
        i2_attention_scratch = nk.raw.address;
        return 0;
    }
    if (!error) error = graph ? activation_publish(output, target_bytes, final, false)
                              : cuda.download(batch->output, final, target_bytes);
    return error;
}

static int nk_execute(const struct inkling_attention_batch *batch,
    const struct inkling_attention_projection *projection,
    const struct inkling_attention_input_projections *inputs, bool graph) {
    if (!batch || batch->slot < 0 || batch->slot >= INKLING_VISITS ||
        batch->capacity < 1 || batch->capacity > INKLING_CONTEXT_LIMIT ||
        batch->first < 0 || batch->count < 1 || batch->count > INKLING_PREFILL_LIMIT ||
        batch->first > batch->capacity || batch->count > batch->capacity - batch->first ||
        (batch->format != INKLING_KV_F16 && batch->format != INKLING_KV_Q4_0) ||
        !batch->bias || !batch->output ||
        (!inputs && (!batch->q || !batch->k || !batch->v || !batch->relative || !batch->sinks))) return -1;
    if (projection && ((!inputs && !projection->gate) || !projection->weights || projection->rows < 1 ||
        projection->rows > 8192 || projection->weight_bytes != (int64_t)projection->rows * (NK_QUERY / 32 * 34))) {
        return -1;
    }
    if (start_context() != 1) return -1;
    struct native_kv_slot *slot = &nk.slots[batch->slot];
    const int capacity = batch->sliding && batch->capacity > INKLING_WINDOW ? INKLING_WINDOW : batch->capacity;
    const size_t row_bytes = nk_row_bytes(batch->format);
    int error = decode_capture_submit();
    if (!error) error = nk_load();
    if (!error && (slot->length != batch->first || (slot->rows && (slot->rows != capacity ||
                              slot->sliding != batch->sliding || slot->format != batch->format)))) error = -1;
    if (!error && !slot->rows) {
        error = reserve(&slot->cache, nk_cache_bytes(capacity, row_bytes) + NK_BIAS * sizeof(float));
        if (!error) {
            slot->rows = capacity;
            slot->sliding = batch->sliding;
            slot->format = batch->format;
            error = nk_upload(slot->cache.address + nk_cache_bytes(capacity, row_bytes), batch->bias, NK_BIAS * sizeof(float));
        }
    }
    const struct native_kv_rows resident = nk_rows(slot->cache.address, capacity, row_bytes);
    struct native_kv_rows rows = resident;
    const int history = batch->sliding && batch->first > capacity ? capacity : batch->first;
    const int total = history + batch->count;
    if (!error && batch->sliding) {
        error = reserve(&nk.window, nk_cache_bytes(total, row_bytes));
        rows = nk_rows(nk.window.address, total, row_bytes);
        if (!error) error = nk_copy(rows, 0, resident, 0, history, row_bytes);
    }
    int64_t host_pos[INKLING_PREFILL_LIMIT];
    for (int i = 0; i < batch->count; ++i) host_pos[i] = batch->first + i;
    const DevicePtr query_pos = rows.positions + (size_t)history * sizeof(int64_t);
    if (!error) error = nk_upload(query_pos, host_pos, (size_t)batch->count * sizeof(int64_t));

    const size_t q_bytes = (size_t)batch->count * NK_QUERY * sizeof(uint16_t);
    const size_t relative_bytes = (size_t)batch->count * NK_REL * sizeof(float);
    const size_t sink_bytes = (size_t)batch->count * NK_HEADS * sizeof(float);
    const size_t output_bytes = (size_t)batch->count * NK_QUERY * sizeof(float);
    if (!error) error = reserve(&nk.input, q_bytes + relative_bytes + sink_bytes + output_bytes);
    const DevicePtr q = nk.input.address, r = q + q_bytes, out = r + relative_bytes + sink_bytes;
    DevicePtr sink = r + relative_bytes, gate = 0;
    DevicePtr key_dst = rows.keys + (size_t)history * row_bytes;
    DevicePtr value_dst = rows.values + (size_t)history * row_bytes;
    if (!error && inputs) {
        error = nk_prepare_device(slot, batch, inputs, q, r, &sink, &gate, key_dst, value_dst, graph);
    } else if (!error) {
        error = nk_pack_kv(key_dst, batch->k, batch->count, batch->format);
        if (!error) error = nk_pack_kv(value_dst, batch->v, batch->count, batch->format);
        if (!error) error = nk_pack(q, batch->q, batch->count * NK_QUERY);
        if (!error) error = nk_upload(r, batch->relative, relative_bytes);
        if (!error) error = nk_upload(sink, batch->sinks, sink_bytes);
    }
    const int row = 4097 - batch->first;
    const bool observe = batch->slot == 0 && row >= 0 && row < batch->count &&
        batch->count <= 16 && getenv("I2_DRAFT_PARITY");
    if (!error && observe) error = nk_draft_fingerprint("query", batch->count, q + row * (NK_QUERY * sizeof(uint16_t)), NK_QUERY * sizeof(uint16_t));
    if (!error && observe) error = nk_draft_fingerprint("relative", batch->count, r + row * (NK_REL * sizeof(float)), NK_REL * sizeof(float));
    if (!error && observe) error = nk_draft_fingerprint("key", batch->count, key_dst + row * (row_bytes), row_bytes);
    if (!error && observe) error = nk_draft_fingerprint("value", batch->count, value_dst + row * (row_bytes), row_bytes);
    if (!error) error = nk_compute(rows, history, total, batch, q, r, sink, query_pos,
                                  slot->cache.address + nk_cache_bytes(capacity, row_bytes), out);
    if (!error && observe) error = nk_draft_fingerprint("flash-output", batch->count, out + row * (NK_QUERY * sizeof(float)), NK_QUERY * sizeof(float));
    if (!error) {
        error = projection ? nk_output_project(batch, projection, out, gate, graph)
                           : cuda.download(batch->output, out, output_bytes);
    }
    if (!error && batch->sliding) {
        const int keep = total < capacity ? total : capacity;
        error = nk_copy(resident, 0, rows, total - keep, keep, row_bytes);
    }
    if (!error) {
        slot->length = batch->first + batch->count;
        ++nk.calls;
        nk.queries += batch->count;
    }
    const int popped = finish_context();
    if (error || popped) {
        fprintf(stderr, "Inkling native attention: CUDA error %d, context %d, slot %d at %d\n",
                error, popped, batch->slot, batch->first);
        return -1;
    }
    return 0;
}

int inkling_attention_execute(const struct inkling_attention_batch *batch) {
    return nk_execute(batch, NULL, NULL, false);
}

int inkling_attention_execute_project(const struct inkling_attention_batch *batch,
                                     const struct inkling_attention_projection *projection) {
    if (!projection) return -1;
    return nk_execute(batch, projection, NULL, false);
}

int inkling_attention_execute_resident(const struct inkling_attention_batch *batch,
    const struct inkling_attention_input_projections *inputs,
    const struct inkling_attention_projection *output) {
    if (!batch || !inputs || !output || !inputs->q_clip || !inputs->r_clip ||
            !inputs->key_kernel || !inputs->value_kernel) return -1;
    for (int i = 0; i < 3; ++i) if (!inputs->input[i]) return -1;
    for (int i = 0; i < (batch->sliding ? 6 : 8); ++i) if (!inputs->weights[i]) return -1;
    return nk_execute(batch, output, inputs, false);
}

int inkling_attention_graph_execute(const struct inkling_attention_graph_call *call) {
    if (!call) return -1;
    return nk_execute(&call->batch, &call->output, &call->inputs, true);
}

static struct device_buffer draft_native_backup;

bool inkling_draft_native_snapshot(bool restore, int position) {
    enum { prepare_bytes = 6 * NK_KV * sizeof(float) };
    if (start_context() != 1) return false;
    int error = cuda.sync();
    size_t size = 0;
    for (int i = 0; i < INKLING_VISITS && !error; ++i) {
        const struct native_kv_slot *slot = &nk.slots[i];
        if (!slot->cache.address || slot->prepare_state.bytes < prepare_bytes ||
            (!restore && slot->length != position) || (restore && slot->length < position)) {
            error = -1;
        } else {
            size += prepare_bytes;
            if (slot->sliding) size += nk_cache_bytes(slot->rows, nk_row_bytes(slot->format));
        }
    }
    if (!error && !restore) error = reserve(&draft_native_backup, size);
    if (!error && draft_native_backup.bytes < size) error = -1;
    DevicePtr backup = draft_native_backup.address;
    for (int i = 0; i < INKLING_VISITS && !error; ++i) {
        struct native_kv_slot *slot = &nk.slots[i];
        const DevicePtr past = slot->prepare_state.address + slot->prepare_state.bytes - prepare_bytes;
        error = restore ? cuda.copy_device(past, backup, prepare_bytes) :
                          cuda.copy_device(backup, past, prepare_bytes);
        backup += prepare_bytes;
        if (!error && slot->sliding) {
            const size_t bytes = nk_cache_bytes(slot->rows, nk_row_bytes(slot->format));
            error = restore ? cuda.copy_device(slot->cache.address, backup, bytes) :
                              cuda.copy_device(backup, slot->cache.address, bytes);
            backup += bytes;
        }
        if (!error && restore) slot->length = position;
    }
    const int popped = finish_context();
    if (!error && !popped) fprintf(stderr, "Inkling draft native restore=%d bytes=%zu\n", restore, size);
    return !error && !popped;
}

int inkling_attention_reset(void) {
    if (!nk.module) return 0;
    if (start_context() != 1) return -1;
    const int error = cuda.sync();
    if (!error) {
        for (int i = 0; i < INKLING_VISITS; ++i) nk.slots[i].length = 0;
    }
    return finish_context() == 0 && !error ? 0 : -1;
}

void inkling_attention_report(void) {
    size_t resident = 0;
    for (int i = 0; i < INKLING_VISITS; ++i) resident += nk.slots[i].cache.bytes + nk.slots[i].prepare_state.bytes;
    printf("Inkling native attention: calls=%" PRIu64 " queries=%" PRIu64 " KV+bias=%zu workspace=%zu H2D=%" PRIu64 "\n",
        nk.calls, nk.queries, resident, nk.window.bytes + nk.input.bytes + nk.staging.bytes + nk.partials.bytes + nk.projection.bytes + nk.raw.bytes + nk.projected_input.bytes + nk.prepared_kv.bytes, nk.uploads);
}
