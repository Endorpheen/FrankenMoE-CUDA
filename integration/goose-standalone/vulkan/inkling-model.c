#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200809L
#include "inkling-model.h"
#include "gguf.h"
#include "moe-bank.h"

#include <stdio.h>
#include <stdlib.h>

enum { PHYSICAL = 24, VISITS = 48, WIDTH = 2304, HEADS = 24, KV_HEADS = 8, D = 128,
    Q = HEADS * D, KV = KV_HEADS * D, R = HEADS * 16, QRS = Q + R + HEADS, BIAS = 1024 * 16,
    WINDOW = 1024, FF = 1152, EXPERTS = 128, K = 8, DEPTH = 12, CHUNK = 4 << 20 };
enum { I0, I1, I2, QRS_VIEW, KEY_VIEW, VALUE_VIEW, ATTN, GATED, P0, P1, P2, P3, P4, P5, P6, P7,
    GATE_UP, ACTIVATION, BRANCHES, ROUTER, ROUTES, ID, LOGITS, VIEW_COUNT };

typedef struct {
    fg_vk_view device;
    float *host;
    uint64_t offset, bytes;
} inkling_tensor;

typedef struct {
    fg_vk_buffer weights;
    inkling_tensor core[10], aux[30];
    fg_vk_expert_plane planes[3];
} inkling_layer;

typedef struct {
    fg_vk_view key, value;
    uint32_t rows;
    float kh[3 * KV], vh[3 * KV], ah[3 * WIDTH], fh[3 * WIDTH];
} inkling_cache;

struct fg_vk_inkling {
    fg_vk_device vk;
    fg_vk_graph graph;
    fg_vk_upload reset;
    fg_vk_expert_bank bank;
    fg_vk_buffer globals, io, state;
    inkling_tensor global[5];
    inkling_layer layers[PHYSICAL];
    inkling_cache cache[VISITS];
    fg_vk_view s[VIEW_COUNT];
    FILE *model;
    uint64_t file_bytes;
    int64_t visits[VISITS * 4];
    uint32_t capacity, position, token;
    int failed;
    float current[WIDTH], prelude[WIDTH], bank_output[WIDTH];
    float blocks[DEPTH * WIDTH], loop_bank[DEPTH * WIDTH], messages[4][WIDTH];
    int count, prelude_count, bank_count;
};

static sl_f32 span(const float *data, size_t count) {
    return (sl_f32){(float *)data, (int64_t)count};
}

static fg_vk_view view_at(fg_vk_inkling *m, const fg_vk_buffer *buffer, VkDeviceSize *end, VkDeviceSize bytes) {
    const VkDeviceSize align = m->vk.properties.limits.minStorageBufferOffsetAlignment;
    const VkDeviceSize start = (*end + align - 1) & ~(align - 1);
    *end = start + bytes;
    return (fg_vk_view){buffer, start, bytes};
}

static int allocate(fg_vk_inkling *m, fg_vk_buffer *buffer, VkDeviceSize bytes, int host) {
    VkResult result = fg_vk_alloc_buffer(&m->vk, buffer, bytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | (host ? 0 : VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        host ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        host ? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : 0);
    return result == VK_SUCCESS || fg_vk_error("allocate Vulkan Inkling buffer", result);
}

static int tensor_record(fg_gguf *h, int64_t id, inkling_tensor *tensor) {
    int64_t data[9];
    if (gr_tensor_info(FG_GGUF_WORDS(h->state), FG_GGUF_WORDS(h->tensors), id, FG_GGUF_WORDS(data)) ||
        data[7] <= 0 || data[7] % 4 || data[8] < 0) return 0;
    tensor->bytes = data[7];
    tensor->offset = data[8];
    return 1;
}

static int load_host_tensor(fg_vk_inkling *m, inkling_tensor *t) {
    t->host = malloc((size_t)t->bytes);
    return t->host && !fseeko(m->model, (int64_t)t->offset, SEEK_SET) &&
        fread(t->host, 1, (size_t)t->bytes, m->model) == t->bytes;
}

static int plan_tensor(fg_vk_inkling *m, fg_gguf *h, int64_t id, inkling_tensor *t,
    fg_vk_buffer *buffer, VkDeviceSize *size, int host) {
    if (!tensor_record(h, id, t)) return 0;
    if (host) return load_host_tensor(m, t);
    t->device = view_at(m, buffer, size, t->bytes);
    return 1;
}

static int upload_tensor(fg_vk_inkling *m, fg_vk_upload *upload, inkling_tensor *t, unsigned char *chunk) {
    return !t->device.size || fg_gguf_upload(m->model, upload, t->device.buffer,
        t->device.offset, t->offset, t->bytes, chunk);
}

static int allocate_work(fg_vk_inkling *m, fg_vk_upload *upload) {
    const unsigned counts[VIEW_COUNT] = {2 * WIDTH, WIDTH, WIDTH, QRS + BIAS, KV, KV, Q, Q,
        Q, KV, KV, R, HEADS, Q, 32, 32, 2 * FF, FF, K * WIDTH, EXPERTS, 2 * K, 1, FG_INKLING_VOCAB};
    VkDeviceSize io = 0, state = 0;
    for (unsigned i = 0; i < VIEW_COUNT; ++i) m->s[i] = view_at(m, &m->io, &io, counts[i] * 4);
    for (unsigned i = 0; i < VISITS; ++i) {
        inkling_cache *cache = &m->cache[i];
        const unsigned slot = (unsigned)m->visits[4 * i + 1];
        if (slot >= VISITS) return 0;
        cache = &m->cache[slot];
        if (cache->rows) return 0;
        cache->rows = m->visits[4 * i + 3] && m->capacity > WINDOW ? WINDOW : m->capacity;
        const VkDeviceSize bytes = (VkDeviceSize)cache->rows * (KV / 32 * 18);
        cache->key = view_at(m, &m->state, &state, bytes);
        cache->value = view_at(m, &m->state, &state, bytes);
    }
    return allocate(m, &m->io, io, 1) && allocate(m, &m->state, state, 0) &&
        fg_vk_upload_zero(upload, &m->state);
}

static int load_layer(fg_vk_inkling *m, fg_gguf *h, fg_vk_upload *upload, unsigned char *chunk,
    unsigned i, const int64_t *core, const int64_t *aux, uint64_t limits[3]) {
    inkling_layer *layer = &m->layers[i];
    VkDeviceSize size = 0;
    for (unsigned j = 0; j < 10; ++j) {
        inkling_tensor *t = &layer->core[j];
        if (j == 6 || j == 7) {
            if (!tensor_record(h, core[j], t) || t->bytes % EXPERTS) return 0;
            const uint64_t bytes = t->bytes / EXPERTS;
            layer->planes[j - 6] = (fg_vk_expert_plane){t->offset, bytes};
            if (bytes > limits[j - 6]) limits[j - 6] = bytes;
        } else if (!plan_tensor(m, h, core[j], t, &layer->weights, &size, j == 9)) return 0;
    }
    for (unsigned j = 0; j < 30; ++j) {
        const int host = j < 5 || j == 6 || j == 7 || (j >= 9 && j <= 14) || j == 17;
        if (aux[j] >= 0 && !plan_tensor(m, h, aux[j], &layer->aux[j], &layer->weights, &size, host)) return 0;
    }
    if (!allocate(m, &layer->weights, size, 0)) return 0;
    for (unsigned j = 0; j < 10; ++j) if (!upload_tensor(m, upload, &layer->core[j], chunk)) return 0;
    for (unsigned j = 0; j < 30; ++j) if (!upload_tensor(m, upload, &layer->aux[j], chunk)) return 0;
    return 1;
}

static int load_model(fg_vk_inkling *m) {
    fg_gguf *h = calloc(1, sizeof(*h));
    unsigned char *chunk = malloc(CHUNK);
    fg_vk_upload upload = {0};
    uint64_t limits[3] = {0};
    int64_t layout[9], globals[5], core[PHYSICAL * 10], aux[PHYSICAL * 30];
    const int64_t bytes = h ? fg_gguf_read_header(h, m->model) : -1;
    int ok = bytes > 0 && chunk &&
        i2_read_layout(h->data, FG_GGUF_WORDS(h->state), FG_GGUF_WORDS(h->metadata),
            FG_GGUF_WORDS(h->meta_slots), FG_GGUF_WORDS(layout), FG_GGUF_WORDS(m->visits)) == VISITS &&
        layout[0] == PHYSICAL && layout[1] == 6 && layout[2] == 18 && layout[3] == 3 && layout[5] == 6 &&
        layout[6] == WINDOW && layout[7] >= m->capacity &&
        i2_bind_text_weights(h->data, FG_GGUF_WORDS(h->state), FG_GGUF_WORDS(h->metadata),
            FG_GGUF_WORDS(h->meta_slots), FG_GGUF_WORDS(h->tensors), FG_GGUF_WORDS(h->tensor_slots),
            FG_GGUF_WORDS(layout), FG_GGUF_WORDS(globals), FG_GGUF_WORDS(core)) == PHYSICAL &&
        i2_bind_text_aux(h->data, FG_GGUF_WORDS(h->state), FG_GGUF_WORDS(h->metadata),
            FG_GGUF_WORDS(h->meta_slots), FG_GGUF_WORDS(h->tensors), FG_GGUF_WORDS(h->tensor_slots),
            FG_GGUF_WORDS(layout), FG_GGUF_WORDS(aux)) == PHYSICAL;
    if (!ok) fputs("Inkling Vulkan: incompatible GGUF layout\n", stderr);
    if (ok) {
        m->file_bytes = (uint64_t)bytes;
        VkDeviceSize size = 0;
        for (unsigned i = 0; i < 5 && ok; ++i) {
            ok = plan_tensor(m, h, globals[i], &m->global[i], &m->globals, &size, i != 0 && i != 2);
        }
        ok = ok && fg_vk_upload_open(&upload, &m->vk, CHUNK) && allocate(m, &m->globals, size, 0);
        for (unsigned i = 0; i < 5 && ok; ++i) ok = upload_tensor(m, &upload, &m->global[i], chunk);
        for (unsigned i = 0; i < PHYSICAL && ok; ++i) {
            ok = load_layer(m, h, &upload, chunk, i, core + 10 * i, aux + 30 * i, limits);
        }
        ok = ok && allocate_work(m, &upload) && fg_vk_expert_bank_open(&m->bank, &m->vk, K, limits);
    }
    fg_vk_upload_close(&upload);
    if (h) free(h->data.data);
    free(h);
    free(chunk);
    return ok;
}

void fg_vk_inkling_close(fg_vk_inkling *m) {
    if (!m) return;
    if (m->model) fclose(m->model);
    fg_vk_graph_close(&m->graph);
    fg_vk_upload_close(&m->reset);
    fg_vk_expert_bank_close(&m->bank);
    for (unsigned i = 0; i < PHYSICAL; ++i) {
        for (unsigned j = 0; j < 10; ++j) free(m->layers[i].core[j].host);
        for (unsigned j = 0; j < 30; ++j) free(m->layers[i].aux[j].host);
        fg_vk_free_buffer(&m->vk, &m->layers[i].weights);
    }
    for (unsigned i = 0; i < 5; ++i) free(m->global[i].host);
    fg_vk_free_buffer(&m->vk, &m->globals);
    fg_vk_free_buffer(&m->vk, &m->io);
    fg_vk_free_buffer(&m->vk, &m->state);
    fg_vk_close(&m->vk);
    free(m);
}

fg_vk_inkling *fg_vk_inkling_open(const char *path, uint32_t capacity, uint32_t device, int allow_cpu) {
    if (!capacity || capacity > 32768) return NULL;
    fg_vk_inkling *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->capacity = capacity;
    m->model = fopen(path, "rb");
    if (!m->model || !fg_vk_open(&m->vk, device, allow_cpu) || !load_model(m) ||
        !fg_vk_graph_open(&m->graph, &m->vk, 32) || !fg_vk_upload_open(&m->reset, &m->vk, 4)) {
        fg_vk_inkling_close(m);
        return NULL;
    }
    fprintf(stderr, "Vulkan Inkling on %s, context=%u, Q4_0 KV\n", m->vk.properties.deviceName, m->capacity);
    return m;
}

static int access_view(fg_vk_inkling *m, unsigned index, size_t bytes, void *data, int write) {
    const fg_vk_view view = m->s[index];
    return bytes <= view.size && fg_vk_access_buffer(&m->vk, &m->io, view.offset, data, bytes, write) == VK_SUCCESS;
}

static int project(fg_vk_inkling *m, fg_vk_op op, uint32_t width, uint32_t rows, unsigned input,
    const inkling_tensor *weight, unsigned output) {
    return fg_vk_graph_record(&m->graph, op, (fg_vk_shape){.width = width, .rows = rows, .batch = 1},
        m->s[input], weight->device, m->s[output]);
}

static int normalize(float *source, const float *weight, float *output, unsigned count) {
    return i2_rms_gain(span(source, count), span(weight, weight ? count : 0), 1e-6f, span(output, count)) == count;
}

static int depth_mix(const float *blocks, int count, const float *cur, const float *queries, unsigned how_many, float *output) {
    if (count < 0 || count >= DEPTH) return 0;
    float states[DEPTH * WIDTH], normalized[DEPTH * WIDTH], probability[DEPTH];
    for (int i = 0; i < count; ++i) {
        memcpy(states + i * WIDTH, blocks + (count - 1 - i) * WIDTH, WIDTH * sizeof(float));
    }
    memcpy(states + count * WIDTH, cur, WIDTH * sizeof(float));
    const size_t n = (count + 1) * WIDTH, q = how_many * WIDTH;
    return i2_depth_mix(span(states, n), WIDTH, span(queries, q),
        span(normalized, n), span(probability, count + 1), span(output, q)) == how_many;
}

static int control(fg_vk_inkling *m, const inkling_tensor *weights, float *cur, float *message) {
    float norm[2 * WIDTH], forget[WIDTH], gain[WIDTH];
    return normalize(cur, NULL, norm, WIDTH) && normalize(message, NULL, norm + WIDTH, WIDTH) &&
        access_view(m, I0, sizeof(norm), norm, 1) && fg_vk_graph_begin(&m->graph) &&
        project(m, FG_VK_Q8_0, 2 * WIDTH, 64, I0, weights, P0) &&
        project(m, FG_VK_Q8_0, 64, WIDTH, P0, weights + 1, I1) &&
        project(m, FG_VK_Q8_0, 64, WIDTH, P0, weights + 2, P5) &&
        fg_vk_graph_submit(&m->graph) &&
        access_view(m, I1, sizeof(forget), forget, 0) && access_view(m, P5, sizeof(gain), gain, 0) &&
        i2_loop_blend(span(cur, WIDTH), span(message, WIDTH), span(gain, WIDTH),
            span(forget, WIDTH), span(cur, WIDTH)) == WIDTH;
}

static int attention(fg_vk_inkling *m, inkling_layer *layer, inkling_cache *cache,
    float input[3][WIDTH], int sliding, float *output) {
    const inkling_tensor *a = layer->aux, *w = layer->core;
    const unsigned sizes[] = {Q, KV, KV, R, HEADS, Q, 32, 32};
    const unsigned inputs[] = {I0, I1, I2, I0, I0, I0, I1, I2};
    const inkling_tensor *weights[] = {w, w + 1, w + 2, a + 5, a + 8, w + 4, a + 15, a + 16};
    const unsigned num = sliding ? 6 : 8;
    for (unsigned i = 0; i < 3; ++i) {
        if (!access_view(m, I0 + i, sizeof(input[i]), input[i], 1)) return 0;
    }
    if (!fg_vk_graph_begin(&m->graph)) return 0;
    for (unsigned i = 0; i < num; ++i) {
        if (!project(m, FG_VK_Q8_0, WIDTH, sizes[i], inputs[i], weights[i], P0 + i)) return 0;
    }
    if (!fg_vk_graph_submit(&m->graph)) return 0;
    float projected[8664], qrs[QRS + BIAS], key[KV], value[KV];
    size_t at = 0;
    for (unsigned i = 0; i < num; ++i) {
        if (!access_view(m, P0 + i, sizes[i] * sizeof(float), projected + at, 0)) return 0;
        at += sizes[i];
    }
    if (i2_vk_prepare(span(projected, at), span(a[4].host, Q), span(a[7].host, R),
        span(a[0].host, 4 * KV), span(a[1].host, 4 * KV), m->position, sliding,
        span(cache->kh, 3 * KV), span(cache->vh, 3 * KV), span(qrs, QRS),
        span(key, KV), span(value, KV)) != 0) return 0;
    memcpy(qrs + QRS, a[6].host, BIAS * sizeof(float));
    const fg_vk_shape store = {.width = D, .rows = KV_HEADS, .batch = 1,
        .position = m->position % cache->rows, .context = cache->rows};
    const fg_vk_shape attend = {.width = D, .rows = HEADS, .batch = 1, .kv_heads = KV_HEADS,
        .position = m->position, .context = cache->rows, .rotary = sliding ? cache->rows : 0};
    const fg_vk_shape gate = {.width = Q, .batch = 1};
    const fg_vk_view *s = m->s;
    return access_view(m, QRS_VIEW, sizeof(qrs), qrs, 1) &&
        access_view(m, KEY_VIEW, sizeof(key), key, 1) && access_view(m, VALUE_VIEW, sizeof(value), value, 1) &&
        fg_vk_graph_begin(&m->graph) &&
        fg_vk_graph_record(&m->graph, FG_VK_KV_STORE_Q4, store, s[KEY_VIEW], s[KEY_VIEW], cache->key) &&
        fg_vk_graph_record(&m->graph, FG_VK_KV_STORE_Q4, store, s[VALUE_VIEW], s[VALUE_VIEW], cache->value) &&
        fg_vk_graph_inkling_attention(&m->graph, attend, s[QRS_VIEW], cache->key, cache->value, s[ATTN]) &&
        fg_vk_graph_record(&m->graph, FG_VK_SIGMOID_GATE, gate, s[ATTN], s[P5], s[GATED]) &&
        project(m, FG_VK_Q8_0, Q, WIDTH, GATED, w + 3, P0) &&
        fg_vk_graph_submit(&m->graph) && access_view(m, P0, WIDTH * sizeof(float), output, 0);
}

static int moe(fg_vk_inkling *m, inkling_layer *layer, float input[WIDTH], float output[WIDTH]) {
    const fg_vk_view *s = m->s;
    const fg_vk_shape route = {.width = EXPERTS, .rows = K, .batch = 1, .theta = 8.0f};
    const fg_vk_shape weighted = {.width = WIDTH, .rows = K, .batch = 1};
    uint32_t selected[K * 2];
    return access_view(m, I0, WIDTH * sizeof(float), input, 1) &&
        fg_vk_graph_begin(&m->graph) &&
        project(m, FG_VK_F32_PROJECT, WIDTH, EXPERTS, I0, layer->core + 5, ROUTER) &&
        fg_vk_graph_record(&m->graph, FG_VK_INKLING_ROUTE, route, s[ROUTER], layer->core[8].device, s[ROUTES]) &&
        fg_vk_graph_submit(&m->graph) && access_view(m, ROUTES, sizeof(selected), selected, 0) &&
        fg_vk_expert_bank_stage(&m->bank, m->model, m->file_bytes, EXPERTS, layer->planes, selected, K) &&
        fg_vk_graph_begin(&m->graph) &&
        fg_vk_expert_bank_record_inkling(&m->graph, &m->bank, WIDTH, FF, s[I0], s[GATE_UP], s[ACTIVATION], s[BRANCHES]) &&
        fg_vk_graph_record(&m->graph, FG_VK_MOE_WEIGHTED, weighted, s[BRANCHES], s[ROUTES], s[P0]) &&
        fg_vk_graph_submit(&m->graph) && access_view(m, P0, WIDTH * sizeof(float), output, 0);
}

static int visit(fg_vk_inkling *m, unsigned physical, unsigned slot, int pass, int sliding) {
    if (physical >= PHYSICAL || slot >= VISITS) return 0;
    inkling_layer *layer = &m->layers[physical];
    inkling_cache *cache = &m->cache[slot];
    const inkling_tensor *a = layer->aux;
    const size_t row = WIDTH * sizeof(float);

    if (physical == 6) {
        if (!pass) {
            memcpy(m->prelude, m->current, row);
            m->prelude_count = m->count;
        } else {
            m->bank_count = m->count - m->prelude_count - 1;
            if (m->bank_count < 0 || m->bank_count >= DEPTH) return 0;
            memcpy(m->loop_bank, m->blocks + (m->prelude_count + 1) * WIDTH, m->bank_count * row);
            memcpy(m->bank_output, m->current, row);
            memcpy(m->current, m->prelude, row);
            m->count = m->prelude_count;
        }
    }
    if (pass > 0 && !depth_mix(m->loop_bank, m->bank_count, m->bank_output,
        a[17].host, 4, m->messages[0])) return 0;

    float input[3][WIDTH], proj[WIDTH], next[WIDTH], ffn[WIDTH];
    if (a[10].host) {
        if (!depth_mix(m->blocks, m->count, m->current, a[10].host, sliding ? 1 : 3, input[0])) return 0;
        if (sliding) {
            memcpy(input[1], input[0], row);
            memcpy(input[2], input[0], row);
        }
    } else {
        for (unsigned i = 0; i < 3; ++i) memcpy(input[i], m->current, row);
    }
    for (unsigned i = 0; i < 3; ++i) {
        if ((pass > 0 && !control(m, a + 18 + i * 3, input[i], m->messages[i])) ||
            !normalize(input[i], a[sliding ? 11 : 12 + i].host, input[i], WIDTH)) return 0;
    }
    if (!attention(m, layer, cache, input, sliding, proj) ||
        i2_postconv_token(span(proj, WIDTH), span(a[2].host, 4 * WIDTH),
            span(cache->ah, 3 * WIDTH), span(next, WIDTH)) != WIDTH) return 0;
    if (physical % 6 == 0) {
        if (m->count >= DEPTH - 1) return 0;
        memcpy(m->blocks + m->count * WIDTH, m->current, row);
        ++m->count;
    } else {
        for (unsigned i = 0; i < WIDTH; ++i) next[i] += m->current[i];
    }
    if (!depth_mix(m->blocks, m->count, next, a[9].host, 1, ffn) ||
        (pass > 0 && !control(m, a + 27, ffn, m->messages[3])) ||
        !normalize(ffn, layer->core[9].host, ffn, WIDTH) || !moe(m, layer, ffn, proj) ||
        i2_postconv_token(span(proj, WIDTH), span(a[3].host, 4 * WIDTH),
            span(cache->fh, 3 * WIDTH), span(m->current, WIDTH)) != WIDTH) return 0;
    for (unsigned i = 0; i < WIDTH; ++i) m->current[i] += next[i];
    return 1;
}

static int embed(fg_vk_inkling *m) {
    const unsigned first = m->token / 32 * 32;
    const size_t row = (WIDTH / 32) * 34;
    fg_vk_view weights = m->global[0].device;
    weights.offset += (size_t)first * row;
    weights.size = 32 * row;
    const fg_vk_shape gather = {.width = WIDTH, .rows = 32, .batch = 1, .position = first};
    m->count = m->prelude_count = m->bank_count = 0;
    return access_view(m, ID, sizeof(m->token), &m->token, 1) &&
        fg_vk_graph_begin(&m->graph) &&
        fg_vk_graph_record(&m->graph, FG_VK_GATHER_Q8_0, gather, m->s[ID], weights, m->s[P0]) &&
        fg_vk_graph_submit(&m->graph) &&
        access_view(m, P0, sizeof(m->current), m->current, 0) &&
        normalize(m->current, m->global[1].host, m->current, WIDTH);
}

static int head(fg_vk_inkling *m) {
    float normalized[WIDTH];
    if (!depth_mix(m->blocks, m->count, m->current, m->global[4].host, 1, normalized) ||
        !normalize(normalized, m->global[3].host, normalized, WIDTH) ||
        !access_view(m, I0, sizeof(normalized), normalized, 1) || !fg_vk_graph_begin(&m->graph)) return 0;
    const size_t row = (WIDTH / 32) * 34;
    for (unsigned first = 0; first < FG_INKLING_VOCAB; first += 32768) {
        const unsigned left = FG_INKLING_VOCAB - first;
        const unsigned rows = left < 32768 ? left : 32768;
        fg_vk_view weights = m->global[2].device;
        weights.offset += (size_t)first * row;
        weights.size = (size_t)rows * row;
        const fg_vk_shape mm = {.width = WIDTH, .rows = rows, .batch = 1, .position = first, .context = FG_INKLING_VOCAB};
        if (!fg_vk_graph_record(&m->graph, FG_VK_Q8_0, mm, m->s[I0], weights, m->s[LOGITS])) return 0;
    }
    return fg_vk_graph_submit(&m->graph);
}

int64_t fg_vk_inkling_op(uint64_t owner, int64_t op, int64_t physical, int64_t slot, int64_t pass, uint8_t sliding) {
    fg_vk_inkling *m = (fg_vk_inkling *)(uintptr_t)owner;
    if (op == 0) return embed(m) ? 0 : -1;
    if (op == 1) return visit(m, (unsigned)physical, (unsigned)slot, (int)pass, sliding) ? 0 : -1;
    return op == 2 && head(m) ? 0 : -1;
}

int fg_vk_inkling_step(fg_vk_inkling *m, uint32_t token, float *logits) {
    if (m->failed || token >= FG_INKLING_VOCAB || m->position >= m->capacity) return 0;
    m->token = token;
    const int64_t status = fg_vk_inkling_forward((uintptr_t)m, FG_GGUF_WORDS(m->visits), logits != NULL);
    if (status) fprintf(stderr, "Inkling Vulkan: model step failed (%lld)\n", (long long)status);
    const int ok = !status && (!logits || (access_view(m, LOGITS, FG_INKLING_VOCAB * sizeof(float), logits, 0) &&
        i2_text_logits(span(logits, FG_INKLING_VOCAB), FG_INKLING_VOCAB, 30.0f) == FG_INKLING_VOCAB));
    if (ok) ++m->position;
    else m->failed = 1;
    return ok;
}

int fg_vk_inkling_reset(fg_vk_inkling *m) {
    if (!m->position && !m->failed) return 1;
    if (!fg_vk_upload_zero(&m->reset, &m->state)) return 0;
    for (unsigned i = 0; i < VISITS; ++i) {
        inkling_cache *c = &m->cache[i];
        memset(c->kh, 0, sizeof(c->kh));
        memset(c->vh, 0, sizeof(c->vh));
        memset(c->ah, 0, sizeof(c->ah));
        memset(c->fh, 0, sizeof(c->fh));
    }
    m->graph.recording = 0;
    m->position = 0;
    m->failed = 0;
    return 1;
}
