#include "giga-mla-image.h"

static struct {
    Module module;
    Function mla_attention_batch_merge;
    Function mla_attention_batch_pair;
    Function mla_attention_merge;
    Function mla_attention_pair;
    Function mla_attention_tiles;
    Function mla_compact_mask;
    Function mla_indexed_merge;
    Function mla_indexed_tile;
    Function mla_mma_tile;
    Function mla_visibility_build;
    Function mla_visibility_tile;
    struct device_buffer mla_indices, mla_visibility;
    bool available;
} giga_mla;

static bool load_giga_mla(void) {
    if (giga_mla.module) return giga_mla.available;
    if (cuda.module_load(&giga_mla.module, giga_mla_image) != 0) return false;
    int error = 0;
    if (error == 0) error = cuda.function_get(&giga_mla.mla_attention_tiles, giga_mla.module, "goose_mla_attention_tiles");
    if (error == 0) error = cuda.function_get(&giga_mla.mla_attention_merge, giga_mla.module, "goose_mla_attention_merge");
    const char * mla_pair_setting = getenv("GOOSE_GIGA_MLA_PAIR");
    if (error == 0 && (!mla_pair_setting || strcmp(mla_pair_setting, "0") != 0))
        (void) cuda.function_get(&giga_mla.mla_attention_pair, giga_mla.module, "goose_mla_attention_pair");
    const char * mla_batch_pair_setting = getenv("GOOSE_GIGA_MLA_BATCH_PAIR");
    if (error == 0 && (!mla_batch_pair_setting || strcmp(mla_batch_pair_setting, "0") != 0)) {
        (void) cuda.function_get(&giga_mla.mla_attention_batch_pair, giga_mla.module, "goose_mla_attention_batch_pair");
        (void) cuda.function_get(&giga_mla.mla_attention_batch_merge, giga_mla.module, "goose_mla_attention_batch_merge");
    }
    const char * mla_mma_setting = getenv("GOOSE_GIGA_MLA_MMA");
    if (error == 0 && (!mla_mma_setting || strcmp(mla_mma_setting, "0") != 0)) {
        const char * pv_mma_setting = getenv("GOOSE_GIGA_MLA_PV_MMA");
        if (!pv_mma_setting || strcmp(pv_mma_setting, "0") != 0)
            (void) cuda.function_get(&giga_mla.mla_mma_tile, giga_mla.module, "goose_giga_mla_mma_pv_tile");
        const char * visibility = getenv("GOOSE_GIGA_MLA_VISIBILITY");
        if (giga_mla.mla_mma_tile && (!visibility || strcmp(visibility, "0") != 0)) {
            (void) cuda.function_get(&giga_mla.mla_visibility_tile, giga_mla.module, "goose_giga_mla_mma_pv_visible_tile");
            (void) cuda.function_get(&giga_mla.mla_visibility_build, giga_mla.module, "goose_giga_mla_visibility");
        }
        const char * indexed = getenv("GOOSE_GIGA_MLA_INDEXED");
        if (giga_mla.mla_mma_tile && (!indexed || strcmp(indexed, "0") != 0)) {
            (void) cuda.function_get(&giga_mla.mla_compact_mask, giga_mla.module, "goose_giga_mla_compact_mask");
            (void) cuda.function_get(&giga_mla.mla_indexed_tile, giga_mla.module, "goose_giga_mla_mma_pv_indexed");
            (void) cuda.function_get(&giga_mla.mla_indexed_merge, giga_mla.module, "goose_giga_mla_indexed_merge");
        }
        if (!giga_mla.mla_mma_tile)
            (void) cuda.function_get(&giga_mla.mla_mma_tile, giga_mla.module, "goose_giga_mla_mma_tile");
    }
    giga_mla.available = error == 0;
    return giga_mla.available;
}

int64_t fg_cuda_giga_mla_attention(sl_i64 config, sl_u8 q, sl_u8 k, sl_u8 v, sl_u8 mask, sl_u8 out, float scale) {
    if (!giga_small_expert_wave || config.len != 23 || config.data[22] != 1) return 0;
    const char *mla_setting = getenv("GOOSE_GIGACHAT4_MLA_ATTENTION");
    if (mla_setting && strcmp(mla_setting, "0") == 0) return 0;
    if (config.data[17] <= 0 || config.data[17] > 65535 ||
        config.data[18] <= 0 || config.data[18] > INT_MAX ||
        config.data[19] <= 0 || config.data[19] > config.data[18] || config.data[20] <= 0 ||
        config.data[20] % config.data[19] != 0 || (uint64_t) config.data[20] > SIZE_MAX ||
        q.len <= 0 || k.len <= 0 || v.len <= 0 || mask.len < 0 || out.len <= 0) return -1;
    const int active = start_context();
    if (active != 1) return active;
    if (!load_giga_mla()) return finish_context() == 0 ? 0 : -1;

    DevicePtr key = 0, value = 0;
    int error = map_state_address((uint64_t) (uintptr_t) k.data, k.len, 0, &key);
    if (error == 0) error = map_state_address((uint64_t) (uintptr_t) v.data, v.len, 0, &value);
    const bool resident = key != 0 && value != 0;
    const sl_u8 data[] = {q, key ? (sl_u8) {0} : k, value ? (sl_u8) {0} : v, mask,
        {(uint8_t *) config.data, config.len * (int64_t) sizeof(*config.data)}};
    struct device_buffer * buffers[] = {&cuda.input, &cuda.weight, &cuda.attention_value, &cuda.jobs, &cuda.attention_config};
    DevicePtr staged[] = {0, key, value, 0, 0}, output = 0;
    const uint64_t host = (uint64_t) (uintptr_t) out.data;
    if (error == 0) error = activation_output(host, out.len, &output);
    if (error == 0) error = upload_inputs(data, buffers, staged, sizeof(data) / sizeof(*data));

    const int tasks = (int) config.data[18];
    const int requested = (int) config.data[19];
    const size_t task_bytes = (size_t) (config.data[20] / requested);
    int capacity = requested;
    if (error == 0) {
        while ((error = reserve(&cuda.attention_scratch, task_bytes * (size_t) capacity)) == 2 &&
               capacity > 1) {
            capacity /= 2;
        }
        if (error == 0 && capacity != requested && getenv("GOOSE_GRAPH_LOG")) {
            fprintf(stderr, "Goose CUDA: attention scratch wave %d of %d tasks\n", capacity, requested);
        }
    }
    const unsigned tiles = (unsigned) config.data[17];
    const int64_t positions = config.data[2];
    const int64_t pair_rows = positions > 0 && positions <= INT_MAX / 2 ? 2 * positions : 0;
    const bool paired_shape = giga_mla.mla_attention_pair &&
        config.data[0] == 576 && config.data[1] == 512 && config.data[4] == 1 && config.data[5] == 1 &&
        pair_rows > 0 && tasks % pair_rows == 0 && (positions == 1 ||
        (giga_mla.mla_attention_batch_pair && giga_mla.mla_attention_batch_merge && capacity >= 2));
    const bool mla_mma = giga_mla.mla_mma_tile && config.data[0] == 576 &&
        config.data[1] == 512 && positions >= 8 && positions % 8 == 0 && config.data[6] % 16 == 0 &&
        config.data[4] == 1 && config.data[5] == 1 && config.data[16] == 256 && tasks % 8 == 0 && capacity >= 8;
    if (mla_mma) capacity -= capacity % 8;
    else if (paired_shape && capacity >= 2) capacity -= capacity % 2;
    bool indexed = false;
    if (error == 0 && mla_mma && staged[3] && positions <= 4096 &&
        config.data[6] >= 8192 && config.data[3] == 32 &&
        giga_mla.mla_compact_mask && giga_mla.mla_indexed_tile && giga_mla.mla_indexed_merge) {
        const size_t key_bytes = (size_t)positions * 2048 * sizeof(int32_t);
        const size_t length_bytes = (size_t)positions * sizeof(int32_t);
        const int admitted = reserve(&giga_mla.mla_indices, key_bytes + length_bytes);
        if (admitted == 0) {
            DevicePtr lengths = giga_mla.mla_indices.address + key_bytes;
            void * args[] = {&staged[3], &giga_mla.mla_indices.address, &lengths, &staged[4]};
            error = cuda.launch(giga_mla.mla_compact_mask, (unsigned)((positions + 3) / 4),
                1, 1, 128, 1, 1, 0, NULL, args, NULL);
            int32_t counts[4096];
            if (error == 0) error = cuda.download(counts, lengths, length_bytes);
            indexed = error == 0;
            for (int64_t row = 0; indexed && row < positions; ++row)
                indexed = counts[row] == 2048;
            if (indexed) {
                size_t cap = (task_bytes * (size_t)capacity) / (8 * 514 * sizeof(float));
                capacity = cap < (size_t) tasks ? (int)cap : tasks;
                capacity -= capacity % 8;
            }
        } else if (admitted != 2) error = admitted;
    }
    bool visible = false;
    if (error == 0 && mla_mma && !indexed && staged[3] && giga_mla.mla_visibility_build && giga_mla.mla_visibility_tile) {
        const size_t flag_bytes = (size_t) (positions / 8) * tiles * sizeof(float);
        const int admitted = reserve(&giga_mla.mla_visibility, flag_bytes);
        if (admitted == 0) {
            void * args[] = {&staged[3], &giga_mla.mla_visibility.address, &staged[4]};
            error = cuda.launch(giga_mla.mla_visibility_build, (unsigned) (positions / 8), tiles,
                1, 256, 1, 1, 0, NULL, args, NULL);
            visible = error == 0;
        } else if (admitted != 2) error = admitted;
    }
    int start = 0;
    DevicePtr flags = indexed ? giga_mla.mla_indices.address : giga_mla.mla_visibility.address;
    void * tile_args[] = {&staged[0], &staged[1], &staged[2],
        &staged[3], &cuda.attention_scratch.address, &staged[4], &scale, &start, &flags};
    void * merge_args[] = {&cuda.attention_scratch.address, &output, &staged[4], &start};
    while (error == 0 && start < tasks) {
        const int count = tasks - start < capacity ? tasks - start : capacity;
        const bool pair = !mla_mma && paired_shape && count % 2 == 0 && start % 2 == 0;
        const Function tile = indexed ? giga_mla.mla_indexed_tile : visible ? giga_mla.mla_visibility_tile : mla_mma ? giga_mla.mla_mma_tile : pair ? (positions == 1 ? giga_mla.mla_attention_pair : giga_mla.mla_attention_batch_pair) :
            giga_mla.mla_attention_tiles;
        error = cuda.launch(tile, (unsigned) count / (mla_mma ? 8 : pair ? 2 : 1), indexed ? 8 : tiles, 1, mla_mma ? 256 : 32, 1, 1, 0, NULL, tile_args, NULL);
        const Function merge = indexed ? giga_mla.mla_indexed_merge : pair && positions > 1 ? giga_mla.mla_attention_batch_merge :
            giga_mla.mla_attention_merge;
        if (error == 0) error = cuda.launch(merge, (unsigned) count, 1, 1, 32, 1, 1, 0, NULL, merge_args, NULL);
        start += count;
    }
    if (error == 0 && indexed && getenv("GOOSE_GRAPH_LOG"))
        fprintf(stderr, "GigaChat4 indexed MLA: positions=%" PRId64 " cells=%" PRId64 "\n", positions, config.data[6]);
    if (error == 0 && mla_mma && getenv("GOOSE_GRAPH_LOG"))
        fprintf(stderr, "GigaChat4 MLA MMA: positions=%" PRId64 " cells=%" PRId64 "\n", positions, config.data[6]);
    if (error == 0) error = activation_publish(host, out.len, output, false);
    if (error != 0 && start > 0) (void) cuda.sync();
    const int popped = finish_context();
    if (error != 0 || popped != 0) {
        fprintf(stderr, "Goose CUDA: GigaChat4 MLA failed (%d)\n", error ? error : popped);
        return -1;
    }
    ++cuda_attentions;
    
    if (config.data[21] > 0) ++cuda_causal_attentions;
    if (resident) ++cuda_resident_kv;
    return 1;
}

