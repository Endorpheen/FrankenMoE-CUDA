#include "cuda-driver-reference.c"
#include "attention-image.h"

enum { I2_HEADS = 24, I2_QUERY = 3072, I2_KV = 1024, I2_REL = 384, I2_BIAS = 16384, I2_SLOTS = 48 };

struct i2_device_slot {
    const int64_t * identity;
    struct device_buffer storage;
    int64_t rows;
    int64_t last;
};

static struct {
    Module module;
    Function score, weights, values, merge;
    struct i2_device_slot slots[I2_SLOTS];
    struct device_buffer scratch;
    uint64_t calls, upload_bytes, download_bytes;
} i2_attn;

static int i2_upload(DevicePtr destination, const void *data, size_t bytes) {
    int error = cuda.upload(destination, data, bytes);
    if (error == 0) {
        i2_attn.upload_bytes += bytes;
    }
    return error;
}

static int i2_load_attention(void) {
    if (i2_attn.merge) {
        return 0;
    }
    int error = cuda.module_load(&i2_attn.module, attention_cubin);
    if (!error) error = cuda.function_get(&i2_attn.score, i2_attn.module, "goose_i2_attention_score");
    if (!error) error = cuda.function_get(&i2_attn.weights, i2_attn.module, "goose_i2_attention_softmax");
    if (!error) error = cuda.function_get(&i2_attn.values, i2_attn.module, "goose_i2_attention_value");
    if (!error) error = cuda.function_get(&i2_attn.merge, i2_attn.module, "goose_i2_attention_merge");
    return error;
}

static struct i2_device_slot *i2_slot(const int64_t *identity) {
    for (unsigned i = 0; i < I2_SLOTS; ++i) {
        if (i2_attn.slots[i].identity == identity) {
            return &i2_attn.slots[i];
        }
        if (!i2_attn.slots[i].identity) {
            i2_attn.slots[i].identity = identity;
            i2_attn.slots[i].last = -1;
            return &i2_attn.slots[i];
        }
    }
    return NULL;
}

int64_t i2_cuda_attention(sl_f32 query, sl_f32 keys, sl_f32 values, sl_f32 relative,
                         sl_f32 bias, sl_f32 sinks, sl_i64 positions, int64_t absolute,
                         int64_t window, sl_f32 output) {
    static int enabled = -1;
    if (enabled < 0) {
        const char *value = getenv("I2_DEVICE_ATTN");
        enabled = value && strcmp(value, "1") == 0;
    }
    if (!enabled || !cuda_inkling_quants) {
        return 0;
    }
    int64_t rows = positions.len;
    if (rows <= 0 || rows > 1048576 || absolute < 0 ||
        (window != 0 && window != 1024) ||
        (window == 0 && absolute >= rows) ||
        keys.len != rows * I2_KV || values.len != keys.len ||
        query.len != I2_QUERY || output.len < I2_QUERY ||
        relative.len != I2_REL || bias.len != I2_BIAS || sinks.len != I2_HEADS) {
        return -1;
    }
    const int64_t row = window ? absolute % rows : absolute;
    if (positions.data[row] != absolute || start_context() != 1) {
        return -1;
    }

    int error = decode_capture_submit();
    if (!error) error = i2_load_attention();
    struct i2_device_slot *slot = error ? NULL : i2_slot(positions.data);
    if (!slot || (slot->last >= 0 && absolute != slot->last + 1) ||
        (slot->rows && slot->rows != rows)) {
        (void)finish_context();
        return -1;
    }
    const size_t kv_bytes = (size_t)rows * I2_KV * sizeof(float);
    const size_t pos_bytes = (size_t)rows * sizeof(int64_t);
    const size_t row_bytes = I2_KV * sizeof(float);
    if (!error) error = reserve(&slot->storage, 2 * kv_bytes + pos_bytes + I2_BIAS * sizeof(float));
    DevicePtr k = slot->storage.address;
    DevicePtr v = k + kv_bytes;
    DevicePtr pos = v + kv_bytes;
    DevicePtr table = pos + pos_bytes;

    int64_t tiles = (rows + 255) / 256;
    const size_t score_bytes = (size_t)rows * I2_HEADS * sizeof(float);
    const size_t partial_bytes = (size_t)tiles * I2_QUERY * sizeof(double);
    const size_t fixed_bytes = (2 * I2_QUERY + I2_REL + 2 * I2_HEADS) * sizeof(float);
    if (!error) error = reserve(&i2_attn.scratch, fixed_bytes + score_bytes + partial_bytes);
    DevicePtr q = i2_attn.scratch.address;
    DevicePtr relatives = q + I2_QUERY * sizeof(float);
    DevicePtr sink = relatives + I2_REL * sizeof(float);
    DevicePtr inv = sink + I2_HEADS * sizeof(float);
    DevicePtr out = inv + I2_HEADS * sizeof(float);
    DevicePtr scores = out + I2_QUERY * sizeof(float);
    DevicePtr partial = scores + score_bytes;

    if (!error && slot->last < 0) {
        error = i2_upload(k, keys.data, kv_bytes);
        if (!error) error = i2_upload(v, values.data, kv_bytes);
        if (!error) error = i2_upload(pos, positions.data, pos_bytes);
        if (!error) error = i2_upload(table, bias.data, I2_BIAS * sizeof(float));
        slot->rows = rows;
    } else if (!error) {
        error = i2_upload(k + row * row_bytes, keys.data + row * I2_KV, row_bytes);
        if (!error) error = i2_upload(v + row * row_bytes, values.data + row * I2_KV, row_bytes);
        if (!error) error = i2_upload(pos + row * sizeof(int64_t), positions.data + row, sizeof(int64_t));
    }
    if (!error) error = i2_upload(q, query.data, I2_QUERY * sizeof(float));
    if (!error) error = i2_upload(relatives, relative.data, I2_REL * sizeof(float));
    if (!error) error = i2_upload(sink, sinks.data, I2_HEADS * sizeof(float));

    float scale = 0x1.6a09e6p-4f;
    void *a[] = {&q, &k, &relatives, &table, &sink, &pos, &scores, &rows, &absolute, &window, &scale};
    void *b[] = {&scores, &inv, &rows};
    void *c[] = {&v, &scores, &inv, &partial, &rows};
    void *e[] = {&partial, &out, &tiles};
    if (!error) error = cuda.launch(i2_attn.score, (unsigned)((rows + 127) / 128), I2_HEADS, 1, 128, 1, 1, 0, NULL, a, NULL);
    if (!error) error = cuda.launch(i2_attn.weights, I2_HEADS, 1, 1, 128, 1, 1, 0, NULL, b, NULL);
    if (!error) error = cuda.launch(i2_attn.values, 4, I2_HEADS, (unsigned)tiles, 32, 1, 1, 0, NULL, c, NULL);
    if (!error) error = cuda.launch(i2_attn.merge, I2_HEADS, 1, 1, 128, 1, 1, 0, NULL, e, NULL);
    if (!error) error = cuda.download(output.data, out, I2_QUERY * sizeof(float));
    if (!error) {
        slot->last = absolute;
        ++i2_attn.calls;
        i2_attn.download_bytes += I2_QUERY * sizeof(float);
    }
    if (finish_context() != 0 || error != 0) {
        fprintf(stderr, "Inkling device attention failed: %d\n", error);
        return -1;
    }
    return I2_QUERY;
}

void i2_cuda_attention_stats(void) {
    size_t bytes = i2_attn.scratch.bytes;
    for (unsigned i = 0; i < I2_SLOTS; ++i) {
        bytes += i2_attn.slots[i].storage.bytes;
    }
    printf("Inkling Attention calls=%" PRIu64 " H2D=%" PRIu64 " D2H=%" PRIu64 " resident=%zu\n",
           i2_attn.calls, i2_attn.upload_bytes, i2_attn.download_bytes, bytes);
}
