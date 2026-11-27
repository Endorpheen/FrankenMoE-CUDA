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

struct giga_mla_waves {
    void **tile_args;
    void **merge_args;
    int *start;
    bool attempted;
    Function tiles[6], merges[3];
};

int32_t giga_mla_launch_wave(uint64_t address, int32_t start, int32_t count, int32_t tile,
                             int32_t merge, int32_t blocks, int32_t tile_rows, int32_t threads) {
    struct giga_mla_waves *wave = (void *)(uintptr_t)address;
    *wave->start = start;
    wave->attempted = true;
    const int error = cuda.launch(wave->tiles[tile], blocks, tile_rows, 1, threads, 1, 1, 0, NULL, wave->tile_args, NULL);
    if (error) return error;
    return cuda.launch(wave->merges[merge], count, 1, 1, 32, 1, 1, 0, NULL, wave->merge_args, NULL);
}

int64_t fg_cuda_giga_mla_attention(sl_i64 config, sl_u8 q, sl_u8 k, sl_u8 v, sl_u8 mask, sl_u8 out, float scale) {
    if (!giga_small_expert_wave || config.len != 23 || config.data[22] != 1) return 0;
    const char *mla_setting = getenv("GOOSE_GIGACHAT4_MLA_ATTENTION");
    if (mla_setting && strcmp(mla_setting, "0") == 0) return 0;
    int64_t lengths[] = {q.len, k.len, v.len, mask.len, out.len};
    if (giga_cuda_mla_validate(config, (sl_i64){lengths, 5}) != 1) return -1;
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
    int64_t plan[4];
    giga_cuda_mla_plan(config, capacity, giga_mla.mla_attention_pair != NULL,
        giga_mla.mla_attention_batch_pair && giga_mla.mla_attention_batch_merge,
        giga_mla.mla_mma_tile != NULL, (sl_i64){plan, 4});
    capacity = (int)plan[0];
    const bool paired_shape = plan[1] != 0, mla_mma = plan[2] != 0;
    bool indexed = false;
    if (error == 0 && plan[3] && staged[3] &&
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
            const int next_capacity = error == 0
                ? giga_cuda_mla_indexed_capacity((sl_i32){counts, positions}, task_bytes, capacity, tasks) : 0;
            indexed = next_capacity > 0;
            if (indexed) capacity = next_capacity;
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
    struct giga_mla_waves wave = {
        .tile_args = tile_args, .merge_args = merge_args, .start = &start,
        .tiles = {giga_mla.mla_indexed_tile, giga_mla.mla_visibility_tile, giga_mla.mla_mma_tile,
                  giga_mla.mla_attention_pair, giga_mla.mla_attention_batch_pair, giga_mla.mla_attention_tiles},
        .merges = {giga_mla.mla_indexed_merge, giga_mla.mla_attention_batch_merge, giga_mla.mla_attention_merge},
    };
    if (error == 0) error = giga_cuda_mla_dispatch((uintptr_t)&wave, tasks, capacity, (int32_t)tiles,
        positions, paired_shape, mla_mma, indexed, visible);
    if (error == 0 && indexed && getenv("GOOSE_GRAPH_LOG"))
        fprintf(stderr, "GigaChat4 indexed MLA: positions=%" PRId64 " cells=%" PRId64 "\n", positions, config.data[6]);
    if (error == 0 && mla_mma && getenv("GOOSE_GRAPH_LOG"))
        fprintf(stderr, "GigaChat4 MLA MMA: positions=%" PRId64 " cells=%" PRId64 "\n", positions, config.data[6]);
    if (error == 0) error = activation_publish(host, out.len, output, false);
    if (error != 0 && wave.attempted) (void) cuda.sync();
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

