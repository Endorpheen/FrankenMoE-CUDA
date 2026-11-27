#include "giga-dsa-image.h"

static struct {
    Module module;
    Function dsa_score;
    Function dsa_shared_q;
    Function dsa_top_wide;
    Function dsa_mask_clear;
    Function dsa_mask_scatter;
} giga_dsa;

static bool load_giga_dsa(void) {
    if (giga_dsa.module) return giga_dsa.dsa_score != NULL;
    if (cuda.module_load(&giga_dsa.module, giga_dsa_image) != 0) return false;
    if (cuda.function_get(&giga_dsa.dsa_score, giga_dsa.module, "goose_dsa_score") != 0) goto unavailable;
    if (cuda.function_get(&giga_dsa.dsa_shared_q, giga_dsa.module, "goose_giga_dsa_score_shared_q") != 0) goto unavailable;
    if (cuda.function_get(&giga_dsa.dsa_top_wide, giga_dsa.module, "goose_giga_dsa_top_wide") != 0) goto unavailable;
    if (cuda.function_get(&giga_dsa.dsa_mask_clear, giga_dsa.module, "goose_giga_mask_clear") != 0) goto unavailable;
    if (cuda.function_get(&giga_dsa.dsa_mask_scatter, giga_dsa.module, "goose_giga_mask_scatter") != 0) goto unavailable;
    return true;

unavailable:
    giga_dsa.dsa_score = NULL;
    return false;
}

int64_t fg_cuda_giga_dsa_mask(sl_u8 mask, sl_u8 picks, sl_u8 out, int64_t cells, int64_t count, int64_t rows, int64_t prefix) {
    if (!giga_small_expert_wave || rows > 4096) return 0;
    if (cells < 1 || cells > 1048576 || count < 1 || count > cells || rows < 1 || rows > 4096 ||
        prefix < 0 || prefix >= rows || mask.len != cells * rows * 2 || picks.len != count * (rows - prefix) * 4 || out.len != mask.len) return -1;
    const int active = start_context();
    if (active != 1) return active;
    if (!load_giga_dsa()) return finish_context() == 0 ? 0 : -1;
    DevicePtr device_mask = 0, device_picks = 0, output = 0;
    const uint64_t host = (uint64_t)(uintptr_t)out.data;
    const int64_t items = cells * rows, kept = count * (rows - prefix), prefix_items = cells * prefix;
    int error = activation_output(host, out.len, &output);
    if (error == 0) error = activation_input(mask, &cuda.input, &device_mask);
    if (error == 0) error = activation_input(picks, &cuda.jobs, &device_picks);
    void *clear_args[] = {&device_mask, &output, (void *)&items, (void *)&prefix_items};
    void *scatter_args[] = {&device_mask, &device_picks, &output, &cells, &count, (void *)&kept, &prefix};
    if (error == 0) error = cuda.launch(giga_dsa.dsa_mask_clear, (unsigned)((items+255)/256), 1, 1, 256, 1, 1, 0, NULL, clear_args, NULL);
    if (error == 0) error = cuda.launch(giga_dsa.dsa_mask_scatter, (unsigned)((kept+255)/256), 1, 1, 256, 1, 1, 0, NULL, scatter_args, NULL);
    if (error == 0) error = activation_publish(host, out.len, output, false);
    if (finish_context() != 0 || error != 0) return -1;
    if (getenv("GOOSE_GRAPH_LOG")) fprintf(stderr, "GigaChat4 CUDA DSA mask rows=%" PRId64 " cells=%" PRId64 "\n", rows, cells);
    return 1;
}

int64_t fg_cuda_giga_dsa_top_wide(sl_u8 scores, sl_u8 ids, int64_t cells, int64_t rows) {
    if (!giga_small_expert_wave || rows < 128) return 0;
    if (cells <= 4096 || cells > 1048576 || rows < 1 || rows > 4096 ||
        scores.len != cells*rows*4 || ids.len != rows*2048*4) return -1;
    const int active = start_context();
    if (active != 1) return active;
    if (!load_giga_dsa()) return finish_context() == 0 ? 0 : -1;
    DevicePtr input = 0, output = 0;
    const uint64_t host = (uint64_t)(uintptr_t)ids.data;
    int error = activation_output(host, ids.len, &output);
    if (error == 0) error = activation_input(scores, &cuda.input, &input);
    void *args[] = {&input,&output,&cells};
    if (error == 0) error = cuda.launch(giga_dsa.dsa_top_wide, (unsigned)rows, 1, 1, 256, 1, 1, 0, NULL, args, NULL);
    if (error == 0) error = activation_publish(host, ids.len, output, false);
    if (finish_context() != 0 || error != 0) return -1;
    if (getenv("GOOSE_GRAPH_LOG")) fprintf(stderr, "GigaChat4 CUDA wide top-k: %" PRId64 "x%" PRId64 "\n",rows,cells);
    return 1;
}

int64_t fg_cuda_dsa_score(sl_i64 config, sl_u8 q, sl_u8 k, sl_u8 weights, sl_u8 mask, sl_u8 out) {
    if (!giga_small_expert_wave) return 0;
    if (config.len != 6 || config.data[0] < 1 || config.data[0] > 1048576 ||
        config.data[1] < 1 || config.data[1] > 4096 ||
        q.len <= 0 || k.len <= 0 || weights.len <= 0 || mask.len <= 0 || out.len <= 0) return -1;
    const int active = start_context();
    if (active != 1) return active;
    if (!load_giga_dsa()) return finish_context() == 0 ? 0 : -1;

    DevicePtr key = 0;
    int error = map_state_address((uint64_t)(uintptr_t) k.data, k.len, 0, &key);
    const sl_u8 inputs[] = {q, key ? (sl_u8){0} : k, weights, mask,
        {(uint8_t *)config.data, config.len * (int64_t)sizeof(*config.data)}};
    struct device_buffer *buffers[] = {&cuda.input, &cuda.weight, &cuda.attention_value,
        &cuda.jobs, &cuda.attention_config};
    DevicePtr staged[5] = {0, key}, output = 0;
    const uint64_t host = (uint64_t)(uintptr_t)out.data;
    if (error == 0) error = activation_output(host, out.len, &output);
    if (error == 0) error = upload_inputs(inputs, buffers, staged, sizeof(inputs) / sizeof(*inputs));
    if (error == 0) {
        void *args[] = {&staged[0], &staged[1], &staged[2], &staged[3], &output, &staged[4]};
        const bool shared_q = giga_small_expert_wave && giga_dsa.dsa_shared_q && config.data[1] >= 128;
        error = cuda.launch(shared_q ? giga_dsa.dsa_shared_q : giga_dsa.dsa_score,
            shared_q ? (unsigned)((config.data[0] + 15) / 16) : (unsigned)((config.data[0] + 3) / 4),
            (unsigned)config.data[1], 1, 128, 1, 1, 0, NULL, args, NULL);
        if (error == 0 && shared_q && getenv("GOOSE_GRAPH_LOG"))
            fprintf(stderr, "GigaChat4 DSA shared Q: cells=%" PRId64 " rows=%" PRId64 "\n",config.data[0],config.data[1]);
    }
    if (error == 0) error = activation_publish(host, out.len, output, false);
    if (finish_context() != 0 || error != 0) return -1;
    return 1;
}

