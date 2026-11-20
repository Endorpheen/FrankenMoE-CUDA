#include "main.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { N_LAYERS = 3, N_EXPERTS = 2, N_PROJECTIONS = 3, N_SLOTS = 2 };
enum { SPAN0 = 192, SPAN1 = 160 };
enum { STATE_MAP_WORDS = N_LAYERS * N_EXPERTS, GROUP_WORDS = 4 * N_SLOTS + 4 + 5 * N_EXPERTS };

extern void fg_cuda_expert_stats(sl_i64 stats);

static unsigned char weights[N_LAYERS][N_PROJECTIONS][192];
static unsigned char device[N_SLOTS * (SPAN0 + SPAN1)];
static int uploads, fail_on;

int64_t __wrap_fg_cuda_expert_upload(int64_t offset, uint64_t source, int64_t bytes) {
    assert(offset >= 0 && bytes > 0 && (uint64_t) offset + (uint64_t) bytes <= sizeof device);
    ++uploads;
    if (uploads == fail_on) return -1;
    memcpy(device + offset, (const void *) (uintptr_t) source, (size_t) bytes);
    return 0;
}

static const int64_t sizes[2][N_PROJECTIONS] = {{32, 64, 96}, {64, 32, 64}};

static const int active_group[N_LAYERS] = {0, 0, 1};

static int marker(int layer, int projection, int expert) {
    return 1 + (layer * N_PROJECTIONS + projection) * N_EXPERTS + expert;
}

static int64_t prepare(sl_i64 groups, sl_i64 layers, uint64_t state, sl_i64 work,
                       int layer, int projection, int64_t first, int64_t second) {
    int64_t heads[N_EXPERTS] = {first, second};
    int64_t offsets[N_EXPERTS] = {-2, -2};
    const int64_t bytes = sizes[active_group[layer]][projection];
    const uint64_t source = (uint64_t) (uintptr_t) weights[layer][projection];
    const int64_t result = ec_prepare(groups, layers, state, work, source, bytes,
        (sl_i64) { heads, N_EXPERTS }, (sl_i64) { offsets, N_EXPERTS });
    if (result != 1) return result;
    for (int e = 0; e < N_EXPERTS; ++e) {
        if (heads[e] < 0) {
            assert(offsets[e] == -1);
            continue;
        }
        assert(offsets[e] >= 0 && offsets[e] + bytes <= (int64_t) sizeof device);
        assert((offsets[e] >= N_SLOTS * SPAN0) == (active_group[layer] == 1));
        for (int i = 0; i < bytes; ++i) {
            assert(device[offsets[e] + i] == marker(layer, projection, e));
        }
    }
    return result;
}

int main(void) {
    goose_init(0, NULL);
    int64_t group_table[] = {
        32, 64, 96, N_EXPERTS, 2, N_SLOTS, 0, STATE_MAP_WORDS, N_LAYERS,
        64, 32, 64, N_EXPERTS, 1, N_SLOTS, N_SLOTS * SPAN0,
        STATE_MAP_WORDS + GROUP_WORDS, N_LAYERS + N_EXPERTS + 5,
    };
    int64_t layer_table[4 * N_LAYERS];
    int32_t state32[STATE_MAP_WORDS + 2 * GROUP_WORDS];
    int64_t state64[N_LAYERS + 2 * (N_EXPERTS + 5)];
    for (int layer = 0; layer < N_LAYERS; ++layer) {
        layer_table[4 * layer] = active_group[layer];
        for (int p = 0; p < N_PROJECTIONS; ++p) {
            const size_t bytes = (size_t) sizes[active_group[layer]][p];
            layer_table[4 * layer + p + 1] = (int64_t) (uintptr_t) weights[layer][p];
            for (int e = 0; e < N_EXPERTS; ++e) {
                memset(weights[layer][p] + e * bytes, marker(layer, p, e), bytes);
            }
        }
    }
    const sl_i64 groups = {group_table, sizeof group_table / sizeof *group_table};
    const sl_i64 layers = {layer_table, sizeof layer_table / sizeof *layer_table};
    const sl_i64 work = {state64, sizeof state64 / sizeof *state64};
    const uint64_t state = (uint64_t) (uintptr_t) state32;
    ec_reset(groups, layers, state, work);

    assert(prepare(groups, layers, state, work, 0, 0, 0, -1) == 1 && uploads == 3);
    assert(prepare(groups, layers, state, work, 0, 1, 0, -1) == 1 && uploads == 3);
    assert(prepare(groups, layers, state, work, 0, 2, 0, 1) == 1 && uploads == 6);
    assert(prepare(groups, layers, state, work, 1, 0, 0, 1) == 1 && uploads == 12);
    const int32_t * evicted_map = (const int32_t *) (uintptr_t) state64[0];
    assert(evicted_map[0] == -1 && evicted_map[1] == -1);
    assert(prepare(groups, layers, state, work, 2, 2, 0, 1) == 1 && uploads == 18);
    assert(prepare(groups, layers, state, work, 1, 1, 0, 1) == 1 && uploads == 18);
    int64_t stats[6] = {0};
    fg_cuda_expert_stats((sl_i64) {stats, 6});
    assert(stats[0] == 4 && stats[1] == 6 && stats[2] == 2);
    assert(stats[3] == 4 * SPAN0 + 2 * SPAN1);

    fail_on = uploads + 2;
    assert(prepare(groups, layers, state, work, 0, 0, 0, -1) == -1 && uploads == 20);
    ec_reset(groups, layers, state, work);
    assert(prepare(groups, layers, state, work, 0, 2, 0, -1) == 1 && uploads == 23);
    fg_cuda_expert_stats((sl_i64) {stats, 6});
    assert(stats[0] == 4 && stats[1] == 7 && stats[2] == 2);
    assert(stats[3] == 5 * SPAN0 + 2 * SPAN1 && stats[4] == 0 && stats[5] == 0);
    puts("Goose SLRU: three-plane fills, cross-layer eviction, separate groups and failure reset verified");
}
