enum { I2_LFU_EXPERTS = 128, I2_LFU_LAYERS = 24, I2_LFU_KEYS = I2_LFU_LAYERS * I2_LFU_EXPERTS };

extern void i2_lfu_init(int argc, char **argv);
extern int64_t i2_ec_prepare_lfu(sl_i64 groups, sl_i64 layers, uint64_t words32, sl_i64 words64,
    uint64_t tensor, int64_t bytes, sl_i64 heads, sl_i64 result, uint8_t stage_all);

static _Thread_local struct {
    uint32_t frequency[I2_LFU_KEYS], last_visit[I2_LFU_KEYS], visit;
    bool active;
} i2_lfu;

static void i2_lfu_init_goose(void) {
    i2_lfu_init(0, NULL);
}

static void i2_lfu_phase(bool prefill, int position, int count) {
    static int enabled = -1;
    if (enabled < 0) {
        const char *setting = getenv("I2_DECODE_LFU");
        enabled = setting && strcmp(setting, "1") == 0;
    }
    if (!enabled) return;
    if (prefill && position == 0) memset(&i2_lfu, 0, sizeof i2_lfu);
    i2_lfu.active = !prefill && count == 1;
    if (i2_lfu.active) {
        static pthread_once_t init = PTHREAD_ONCE_INIT;
        pthread_once(&init, i2_lfu_init_goose);
    }
}

static int i2_lfu_layer(uint64_t tensor, int64_t count) {
    const struct expert_cache *cache = &cuda.experts;
    if (!i2_lfu.active || count != I2_LFU_EXPERTS ||
        cache->groups.len != 9 || cache->groups.data[3] != I2_LFU_EXPERTS ||
        cache->layers.len != 4 * I2_LFU_LAYERS) return -1;
    for (int layer = 0; layer < I2_LFU_LAYERS; ++layer) {
        const int64_t *entry = cache->layers.data + 4 * layer;
        if ((uint64_t)entry[1] == tensor || (uint64_t)entry[3] == tensor) return layer;
    }
    return -1;
}

static void i2_lfu_observe(int layer, uint64_t tensor, sl_i64 active) {
    if ((uint64_t)cuda.experts.layers.data[4 * layer + 1] != tensor) return;
    ++i2_lfu.visit;
    for (int e = 0; e < I2_LFU_EXPERTS; ++e) {
        if (active.data[e] < 0) continue;
        const int key = layer * I2_LFU_EXPERTS + e;
        ++i2_lfu.frequency[key];
        i2_lfu.last_visit[key] = i2_lfu.visit;
    }
}

int64_t fg_i2_lfu_victims(uint64_t address, int64_t slots, int64_t layer, sl_i64 seen,
                          int64_t generation, sl_i64 victims) {
    if (!address || slots <= 0 || slots > I2_LFU_KEYS || layer < 0 || layer >= I2_LFU_LAYERS ||
        !seen.data || seen.len != I2_LFU_EXPERTS || !victims.data || victims.len <= 0 || victims.len > 8) return -1;
    const int32_t *keys = (const int32_t *)(uintptr_t)address;
    uint64_t scores[8];
    int64_t count = 0;
    for (int64_t slot = 0; slot < slots; ++slot) {
        const int32_t key = keys[slot];
        if (key >= I2_LFU_KEYS) return -1;
        if (key >= 0 && key / I2_LFU_EXPERTS == layer && seen.data[key % I2_LFU_EXPERTS] == generation) continue;
        // The Inkling context limit allows fewer than 2^26 logical visits.
        // Empty slots precede all occupied ones, then frequency, age and ID.
        const uint64_t score = key < 0 ? 0 :
            ((uint64_t)i2_lfu.frequency[key] << 38) |
            ((uint64_t)i2_lfu.last_visit[key] << 12) | (uint64_t)(key + 1);
        int64_t index = count;
        if (index == victims.len) {
            --index;
            if (score >= scores[index]) continue;
        } else {
            ++count;
        }
        while (index > 0 && score < scores[index - 1]) {
            scores[index] = scores[index - 1];
            victims.data[index] = victims.data[index - 1];
            --index;
        }
        scores[index] = score;
        victims.data[index] = slot;
    }
    return count;
}
