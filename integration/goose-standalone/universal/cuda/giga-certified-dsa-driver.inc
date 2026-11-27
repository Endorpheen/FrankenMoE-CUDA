#include "giga-certified-dsa-image.h"

enum dsa_kernel {
    DSA_PACK, DSA_METADATA, DSA_BOUND, DSA_SCORE, DSA_CUTOFF, DSA_COMPACT,
    DSA_REPLAY, DSA_FINALIZE, DSA_FALLBACK_SCORE, DSA_FALLBACK_TOP, DSA_KERNEL_COUNT
};

static const char *const dsa_kernel_names[DSA_KERNEL_COUNT] = {
    "goose_dsa_pack_power2", "goose_dsa_bound_metadata", "goose_dsa_bound_query",
    "goose_dsa_integer_score", "goose_dsa_score_cutoff", "goose_dsa_compact",
    "goose_dsa_replay", "goose_dsa_finalize", "goose_dsa_fallback_score", "goose_dsa_fallback_top"
};

static struct {
    Module module;
    Function kernels[DSA_KERNEL_COUNT];
    struct device_buffer scratch;
    int (*zero)(DevicePtr, unsigned char, size_t, Stream);
} certified_dsa;

static bool load_certified_dsa(void) {
    if (certified_dsa.module) return true;
    certified_dsa.zero = dlsym(cuda.library, "cuMemsetD8Async");
    if (!certified_dsa.zero ||
        cuda.module_load(&certified_dsa.module, giga_certified_dsa_image) != 0) return false;
    for (int i = 0; i < DSA_KERNEL_COUNT; ++i) {
        if (cuda.function_get(&certified_dsa.kernels[i], certified_dsa.module, dsa_kernel_names[i]) != 0) {
            int (*unload)(Module) = dlsym(cuda.library, "cuModuleUnload");
            if (unload) (void) unload(certified_dsa.module);
            certified_dsa.module = NULL;
            return false;
        }
    }
    return true;
}

int32_t giga_dsa_schedule_zero(uint64_t address, uint64_t bytes) {
    return certified_dsa.zero(address, 0, (size_t)bytes, NULL);
}

int32_t giga_dsa_schedule_launch(int32_t kernel, int64_t columns, int64_t rows,
                                  int64_t threads, sl_u64 packed) {
    void *args[10];
    if (kernel < 0 || kernel >= DSA_KERNEL_COUNT || packed.len < 1 || packed.len > 10) return -1;
    for (int64_t i = 0; i < packed.len; ++i) args[i] = &packed.data[i];
    return cuda.launch(certified_dsa.kernels[kernel], (unsigned)columns, (unsigned)rows, 1,
                       (unsigned)threads, 1, 1, 0, NULL, args, NULL);
}

int64_t fg_cuda_giga_dsa_certified(sl_i64 config, sl_u8 q, sl_u8 k,
                                  sl_u8 weights, sl_u8 mask, sl_u8 ids) {
    if (config.len != 6 || !config.data || !q.data || !k.data ||
        !weights.data || !mask.data || !ids.data) return 0;
    const int64_t cells = config.data[0], rows = config.data[1], stride = config.data[2];
    const size_t scratch = giga_dsa_schedule_workspace(cells, rows);
    if (!scratch || stride < 256 || stride > INT32_MAX || stride % 2 ||
        config.data[3] != 32768 || config.data[4] != 256 || config.data[5] != 2 * cells ||
        q.len != rows * 32768 || k.len < (cells - 1) * stride + 256 ||
        weights.len != rows * 256 || mask.len != rows * cells * 2 || ids.len != rows * 8192)
        return 0;

    const int active = start_context();
    if (active != 1) return active;
    const bool available = giga_small_expert_wave && load_certified_dsa();
    if (!available) return finish_context() == 0 ? 0 : -1;

    const int64_t tile = rows < 512 ? rows : 512;
    const size_t score_bytes = (size_t)cells * (size_t)tile * sizeof(float);
    int error = reserve(&certified_dsa.scratch, score_bytes + scratch);
    if (error != 0) return finish_context() == 0 ? 0 : -1;
    DevicePtr key = 0, output = 0;
    error = map_state_address((uint64_t)(uintptr_t)k.data, k.len, 0, &key);
    const sl_u8 inputs[] = {q, key ? (sl_u8){0} : k, weights, mask,
        {(uint8_t *)config.data, config.len * (int64_t)sizeof(*config.data)}};
    struct device_buffer *buffers[] = {&cuda.input, &cuda.weight, &cuda.attention_value,
        &cuda.jobs, &cuda.attention_config};
    DevicePtr staged[5] = {0, key};
    const uint64_t host = (uint64_t)(uintptr_t)ids.data;
    if (error == 0) error = activation_output(host, ids.len, &output);
    if (error == 0) error = upload_inputs(inputs, buffers, staged, 5);
    if (error == 0) {
        const DevicePtr scores = certified_dsa.scratch.address;
        uint64_t scheduled[] = {staged[0], staged[1], staged[2], staged[3],
            scores, output, staged[4], scores + score_bytes};
        if (giga_dsa_schedule(config, (sl_u64){scheduled, 8}, scratch) != 1) error = -1;
    }
    if (error == 0) error = activation_publish(host, ids.len, output, false);
    if (error == 0 && getenv("GOOSE_GIGACHAT4_TRACE"))
        fprintf(stderr, "GigaChat4 CUDA certified DSA: cells=%" PRId64 " rows=%" PRId64
            " tile=%" PRId64 " scratch=%zu\n", cells, rows, tile, score_bytes + scratch);
    return finish_context() == 0 && error == 0 ? 1 : -1;
}
uint8_t fg_cuda_giga_dsa_certified_enabled(void) {
    if (!giga_small_expert_wave) return false;
    const char *mode = getenv("GOOSE_GIGA_DSA_CERTIFIED");
    return mode && strcmp(mode, "1") == 0;
}

int64_t fg_cuda_giga_dsa_certified_address(sl_i64 config, sl_u64 inputs,
                                         sl_i64 sizes, uint64_t out, int64_t out_bytes) {
    if (inputs.len != 4 || sizes.len != 4) return 0;
    sl_u8 data[4];
    for (int i = 0; i < 4; ++i)
        data[i] = (sl_u8){(void *)(uintptr_t)inputs.data[i], sizes.data[i]};
    return fg_cuda_giga_dsa_certified(config, data[0], data[1], data[2], data[3],
        (sl_u8){(void *)(uintptr_t)out, out_bytes});
}
