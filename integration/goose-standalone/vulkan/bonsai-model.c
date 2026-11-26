#include "bonsai-model.h"
#include "bonsai-layer.h"
#include "gguf.h"

#include <stdio.h>
#include <stdlib.h>

enum { LAYERS = 64, H = FG_BONSAI_HIDDEN, V = FG_BONSAI_VALUES, M = FG_BONSAI_MIX, F = FG_BONSAI_FFN,
       ROW_BYTES = H / 128 * 34, VOCAB_STRIPE = 32768, EMBED_STRIPE = 16, CHUNK = 4 << 20 };
enum { G_EMBED, G_RMS, G_OUTPUT, G_SIGNS, G_COUNT };
enum { GX_EMBED, GX_H0, GX_H1, GX_NORM, GX_ROTATED, GX_IDS, GX_LOGITS, GX_COUNT };

typedef struct {
    int attention;
    fg_vk_buffer buffer;
    union { fg_vk_bonsai_layer recurrent; fg_vk_bonsai_attention full; } layer;
} fg_bonsai_slot;

struct fg_vk_bonsai {
    fg_vk_device vk;
    fg_vk_graph graph;
    fg_bonsai_slot layers[LAYERS];
    fg_vk_buffer globals[G_COUNT], state, scratch, host;
    fg_vk_view signs[3], extra[GX_COUNT], s[BS_COUNT];
    uint32_t capacity, position, token;
    float epsilon;
    int failed;
    int64_t eog[64], eog_count;
};

typedef struct {
    FILE *file;
    fg_gguf *header;
    fg_vk_upload *upload;
    unsigned char *chunk;
    VkDeviceSize alignment;
} fg_bonsai_loader;

typedef struct {
    VkDeviceSize end;
    struct { uint64_t file_offset, bytes; VkDeviceSize offset; } parts[BA_COUNT + BW_COUNT + 2];
    unsigned count;
} fg_bonsai_plan;

static int alloc(fg_vk_bonsai *m, fg_vk_buffer *buffer, VkDeviceSize bytes, int host, int transfer) {
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (transfer) usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkResult status = fg_vk_alloc_buffer(&m->vk, buffer, bytes, usage,
        host ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        host ? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : 0);
    return status == VK_SUCCESS || fg_vk_error("allocate Bonsai Vulkan buffer", status);
}

static fg_vk_view view_at(const fg_vk_buffer *buffer, VkDeviceSize *end, VkDeviceSize bytes, VkDeviceSize align) {
    const VkDeviceSize start = (*end + align - 1) & ~(align - 1);
    *end = start + bytes;
    return (fg_vk_view){buffer, start, bytes};
}

static int plan_tensor(fg_bonsai_loader *loader, fg_bonsai_plan *plan, const fg_vk_buffer *buffer,
    fg_vk_view *view, int64_t index, int64_t second_index) {
    int64_t a[9], b[9] = {0};
    fg_gguf *h = loader->header;
    if (plan->count + (second_index >= 0 ? 2 : 1) > sizeof(plan->parts) / sizeof(*plan->parts) ||
        gr_tensor_info(FG_GGUF_WORDS(h->state), FG_GGUF_WORDS(h->tensors), index, FG_GGUF_WORDS(a)) ||
        (second_index >= 0 &&
         gr_tensor_info(FG_GGUF_WORDS(h->state), FG_GGUF_WORDS(h->tensors), second_index, FG_GGUF_WORDS(b))) ||
        a[7] <= 0 || a[7] % 4 || a[8] < 0 ||
        (second_index >= 0 && (b[7] <= 0 || b[7] % 4 || b[8] < 0))) return 0;
    *view = view_at(buffer, &plan->end, (VkDeviceSize)a[7] + (VkDeviceSize)b[7], loader->alignment);
    unsigned n = plan->count++;
    plan->parts[n].file_offset = a[8]; plan->parts[n].bytes = a[7]; plan->parts[n].offset = view->offset;
    if (second_index >= 0) {
        n = plan->count++;
        plan->parts[n].file_offset = b[8]; plan->parts[n].bytes = b[7]; plan->parts[n].offset = view->offset + a[7];
    }
    return 1;
}

static int materialize(fg_vk_bonsai *m, fg_bonsai_loader *loader, fg_vk_buffer *buffer, const fg_bonsai_plan *plan) {
    if (!alloc(m, buffer, plan->end, 0, 1)) return 0;
    for (unsigned i = 0; i < plan->count; ++i) {
        if (!fg_gguf_upload(loader->file, loader->upload, buffer, plan->parts[i].offset,
            plan->parts[i].file_offset, plan->parts[i].bytes, loader->chunk)) return 0;
    }
    return 1;
}

static int load_layer(fg_vk_bonsai *m, fg_bonsai_loader *loader, fg_bonsai_slot *slot, const int64_t *t) {
    fg_bonsai_plan plan = {0};
    if (t[0] != 0 && t[0] != 1) return 0;
    slot->attention = t[0] == 0;
    fg_vk_view *w = slot->attention ? slot->layer.full.common : slot->layer.recurrent.weights;
    for (unsigned i = 0; i < 3; ++i) w[BW_SIGN_H + i] = m->signs[i];
    if (!plan_tensor(loader, &plan, &slot->buffer, &w[BW_ATTN_NORM], t[1], -1) ||
        !plan_tensor(loader, &plan, &slot->buffer, &w[BW_POST_NORM], t[slot->attention ? 8 : 11], -1)) return 0;
    for (unsigned i = 0; i < 3; ++i) {
        if (!plan_tensor(loader, &plan, &slot->buffer, &w[BW_FFN_GATE + i], t[12 + i], -1)) return 0;
    }
    if (slot->attention) {
        static const unsigned source[BA_COUNT] = {2, 3, 4, 6, 7, 5};
        for (unsigned i = 0; i < BA_COUNT; ++i) {
            if (!plan_tensor(loader, &plan, &slot->buffer, &slot->layer.full.attention[i], t[source[i]], -1)) return 0;
        }
    } else {
        static const unsigned source[] = {2, 3, 5, 6, 8, 9, 10};
        static const int second[] = {-1, -1, 4, 7, -1, -1, -1};
        for (unsigned i = 0; i < sizeof(source) / sizeof(*source); ++i) {
            if (!plan_tensor(loader, &plan, &slot->buffer, &w[BW_MIX + i], t[source[i]],
                second[i] < 0 ? -1 : t[second[i]])) return 0;
        }
    }
    return materialize(m, loader, &slot->buffer, &plan);
}

static int load_eog(fg_gguf *file, fg_vk_bonsai *m) {
    const sl_i64 state = FG_GGUF_WORDS(file->state), meta = FG_GGUF_WORDS(file->metadata);
    int64_t control[3] = {-1, -1, -1};
    const char *names[] = {"eos", "eot", "eom"};
    for (unsigned i = 0; i < 3; ++i) {
        char name[48];
        snprintf(name, sizeof(name), "tokenizer.ggml.%s_token_id", names[i]);
        const int64_t key = fg_gguf_metadata(file, name);
        if (key >= 0 && gr_meta_int(file->data, state, meta, key, fg_gguf_words(control + i, 1))) return 0;
    }
    int64_t *tokens = malloc(2 * FG_BONSAI_VOCAB * sizeof(*tokens));
    if (!tokens) return 0;
    const sl_i64 offsets = fg_gguf_words(tokens, FG_BONSAI_VOCAB);
    const sl_i64 lengths = fg_gguf_words(tokens + FG_BONSAI_VOCAB, FG_BONSAI_VOCAB);
    const int64_t count = gr_arr_str_offsets(file->data, state, meta,
        fg_gguf_metadata(file, "tokenizer.ggml.tokens"), offsets, lengths);
    if (count == FG_BONSAI_VOCAB) {
        m->eog_count = bt_eog_ids(file->data, offsets, lengths, control[0], control[1], control[2], FG_GGUF_WORDS(m->eog));
    }
    free(tokens);
    return m->eog_count > 0;
}

static int model_meta(fg_gguf *file, fg_vk_bonsai *m, int32_t *signs) {
    const struct { const char *name; int64_t value; } checks[] = {
        {"qwen35.embedding_length", H}, {"qwen35.feed_forward_length", F},
        {"qwen35.attention.head_count", FG_BONSAI_QUERY_HEADS},
        {"qwen35.attention.head_count_kv", FG_BONSAI_KV_HEADS},
        {"qwen35.attention.key_length", FG_BONSAI_HEAD_WIDTH},
        {"qwen35.attention.value_length", FG_BONSAI_HEAD_WIDTH}, {"qwen35.rope.dimension_count", 64},
        {"qwen35.ssm.conv_kernel", 4}, {"qwen35.ssm.state_size", FG_BONSAI_DIM},
        {"qwen35.ssm.group_count", FG_BONSAI_KEY_HEADS}, {"qwen35.ssm.time_step_rank", FG_BONSAI_HEADS},
        {"qwen35.ssm.inner_size", V},
    };
    for (unsigned i = 0; i < sizeof(checks) / sizeof(*checks); ++i) {
        int64_t value;
        if (gr_meta_int(file->data, FG_GGUF_WORDS(file->state), FG_GGUF_WORDS(file->metadata),
            fg_gguf_metadata(file, checks[i].name), fg_gguf_words(&value, 1)) || value != checks[i].value) return 0;
    }
    int64_t eps, theta, maximum;
    if (gr_meta_f32_bits(file->data, FG_GGUF_WORDS(file->state), FG_GGUF_WORDS(file->metadata),
            fg_gguf_metadata(file, "qwen35.attention.layer_norm_rms_epsilon"), fg_gguf_words(&eps, 1)) ||
        gr_meta_f32_bits(file->data, FG_GGUF_WORDS(file->state), FG_GGUF_WORDS(file->metadata),
            fg_gguf_metadata(file, "qwen35.rope.freq_base"), fg_gguf_words(&theta, 1)) ||
        gr_meta_int(file->data, FG_GGUF_WORDS(file->state), FG_GGUF_WORDS(file->metadata),
            fg_gguf_metadata(file, "qwen35.context_length"), fg_gguf_words(&maximum, 1)) || maximum < m->capacity) return 0;
    const uint32_t eps_bits = (uint32_t)eps, theta_bits = (uint32_t)theta;
    float rope;
    memcpy(&m->epsilon, &eps_bits, sizeof(eps_bits));
    memcpy(&rope, &theta_bits, sizeof(theta_bits));
    if (m->epsilon != 1e-6f || rope != 1e7f) return 0;
    const int64_t source = fg_gguf_metadata(file, "prism.hadamard.sign_values");
    const int64_t widths = fg_gguf_metadata(file, "prism.hadamard.sign_widths");
    if (gr_arr_n(FG_GGUF_WORDS(file->state), FG_GGUF_WORDS(file->metadata), source) != H + V + F ||
        gr_arr_n(FG_GGUF_WORDS(file->state), FG_GGUF_WORDS(file->metadata), widths) != 3) return 0;
    const int64_t expected[3] = {H, V, F};
    for (int64_t i = 0; i < 3; ++i) {
        int64_t value;
        if (gr_array_int(file->data, FG_GGUF_WORDS(file->state), FG_GGUF_WORDS(file->metadata),
            widths, i, fg_gguf_words(&value, 1)) || value != expected[i]) return 0;
    }
    for (int64_t i = 0; i < H + V + F; ++i) {
        int64_t value;
        if (gr_array_int(file->data, FG_GGUF_WORDS(file->state), FG_GGUF_WORDS(file->metadata),
            source, i, fg_gguf_words(&value, 1)) || (value != -1 && value != 1)) return 0;
        signs[i] = (int32_t)value;
    }
    return load_eog(file, m);
}

static int load_weights(fg_vk_bonsai *m, FILE *file, fg_vk_upload *upload) {
    fg_gguf *header = calloc(1, sizeof(*header));
    unsigned char *chunk = malloc(CHUNK);
    int32_t signs[H + V + F];
    int64_t global[3], layers[(LAYERS + 1) * 15], mtp[4];
    int ok = header && chunk && fg_gguf_read_header(header, file) >= 0 &&
        bonsai_bind_layout(header->data, FG_GGUF_WORDS(header->state), FG_GGUF_WORDS(header->metadata),
            FG_GGUF_WORDS(header->tensors), FG_GGUF_WORDS(header->meta_slots), FG_GGUF_WORDS(header->tensor_slots),
            FG_GGUF_WORDS(global), FG_GGUF_WORDS(layers), FG_GGUF_WORDS(mtp)) == LAYERS &&
        model_meta(header, m, signs);
    if (!ok) fputs("Bonsai Vulkan: incompatible GGUF layout\n", stderr);
    fg_bonsai_loader loader = {file, header, upload, chunk, m->vk.properties.limits.minStorageBufferOffsetAlignment};
    for (unsigned i = 0; ok && i < G_SIGNS; ++i) {
        fg_bonsai_plan plan = {0};
        fg_vk_view v;
        ok = plan_tensor(&loader, &plan, &m->globals[i], &v, global[i], -1) &&
            materialize(m, &loader, &m->globals[i], &plan);
    }
    const unsigned counts[] = {H, V, F};
    VkDeviceSize sign_end = 0;
    for (unsigned i = 0; i < 3; ++i) {
        m->signs[i] = view_at(&m->globals[G_SIGNS], &sign_end, counts[i] * 4, loader.alignment);
    }
    if (ok) ok = alloc(m, &m->globals[G_SIGNS], sign_end, 0, 1);
    for (unsigned i = 0, at = 0; i < 3 && ok; ++i) {
        ok = fg_vk_upload_write(upload, &m->globals[G_SIGNS], m->signs[i].offset, signs + at, counts[i] * 4);
        at += counts[i];
    }
    for (unsigned i = 0; i < LAYERS && ok; ++i) ok = load_layer(m, &loader, &m->layers[i], layers + 15 * i);
    if (header) free(header->data.data);
    free(header);
    free(chunk);
    return ok;
}

static int allocate_work(fg_vk_bonsai *m, fg_vk_upload *upload) {
    const VkDeviceSize align = m->vk.properties.limits.minStorageBufferOffsetAlignment;
    VkDeviceSize scratch = 0, state = 0, host = 0;
    const unsigned widths[BS_COUNT] = {H, H, 2 * V, V, FG_BONSAI_KV, M, M, V, V, V, H, H, F, F, F, F};
    for (unsigned i = 0; i < BS_COUNT; ++i) m->s[i] = view_at(&m->scratch, &scratch, widths[i] * 4, align);
    for (unsigned i = GX_EMBED; i <= GX_ROTATED; ++i) m->extra[i] = view_at(&m->scratch, &scratch, H * 4, align);
    m->extra[GX_IDS] = view_at(&m->host, &host, 4, align);
    m->extra[GX_LOGITS] = view_at(&m->host, &host, 4 * FG_BONSAI_VOCAB, align);
    for (unsigned i = 0; i < LAYERS; ++i) {
        fg_bonsai_slot *slot = &m->layers[i];
        if (slot->attention) {
            VkDeviceSize bytes = (VkDeviceSize)m->capacity * FG_BONSAI_KV_BYTES;
            slot->layer.full.keys = view_at(&m->state, &state, bytes, align);
            slot->layer.full.values = view_at(&m->state, &state, bytes, align);
        } else {
            slot->layer.recurrent.history = view_at(&m->state, &state, M * 3 * 4, align);
            slot->layer.recurrent.state = view_at(&m->state, &state, V * FG_BONSAI_DIM * 4, align);
        }
    }
    return alloc(m, &m->scratch, scratch, 0, 0) && alloc(m, &m->state, state, 0, 1) &&
        alloc(m, &m->host, host, 1, 0) && fg_vk_upload_zero(upload, &m->state);
}

void fg_vk_bonsai_close(fg_vk_bonsai *m) {
    if (!m) return;
    fg_vk_graph_close(&m->graph);
    for (unsigned i = 0; i < LAYERS; ++i) fg_vk_free_buffer(&m->vk, &m->layers[i].buffer);
    for (unsigned i = 0; i < G_COUNT; ++i) fg_vk_free_buffer(&m->vk, &m->globals[i]);
    fg_vk_free_buffer(&m->vk, &m->state);
    fg_vk_free_buffer(&m->vk, &m->scratch);
    fg_vk_free_buffer(&m->vk, &m->host);
    fg_vk_close(&m->vk);
    free(m);
}

fg_vk_bonsai *fg_vk_bonsai_open(const char *path, uint32_t capacity, uint32_t device, int allow_cpu) {
    if (!capacity || capacity > 32768) return NULL;
    FILE *file = fopen(path, "rb");
    if (!file) { perror(path); return NULL; }
    fg_vk_bonsai *m = calloc(1, sizeof(*m));
    if (m) m->capacity = capacity;
    fg_vk_upload upload = {0};
    int ok = m && fg_vk_open(&m->vk, device, allow_cpu) &&
        fg_vk_upload_open(&upload, &m->vk, CHUNK) && load_weights(m, file, &upload) &&
        allocate_work(m, &upload) && fg_vk_graph_open(&m->graph, &m->vk, LAYERS * 26 + 32);
    fg_vk_upload_close(&upload);
    fclose(file);
    if (!ok) { fg_vk_bonsai_close(m); return NULL; }
    fprintf(stderr, "Vulkan Bonsai on %s, context=%u, MTP off\n", m->vk.properties.deviceName, m->capacity);
    return m;
}

int64_t fg_vk_bonsai_op(uint64_t owner, int64_t op, int64_t layer) {
    fg_vk_bonsai *m = (fg_vk_bonsai *)(uintptr_t)owner;
    fg_vk_graph *g = &m->graph;
    const fg_vk_view *x = m->extra;
    if (op == 0) {
        const unsigned first = m->token / EMBED_STRIPE * EMBED_STRIPE;
        const fg_vk_shape gather = {.width = H, .rows = EMBED_STRIPE, .batch = 1, .position = first};
        const fg_vk_view stripe = {&m->globals[G_EMBED], (VkDeviceSize)first * ROW_BYTES, EMBED_STRIPE * ROW_BYTES};
        const fg_vk_shape inverse = {.width = H, .batch = 1, .position = 1};
        return fg_vk_graph_record(g, FG_VK_GATHER_PQ2_0, gather, x[GX_IDS], stripe, x[GX_EMBED]) &&
            fg_vk_graph_record(g, FG_VK_FWHT, inverse, x[GX_EMBED], m->signs[0], x[GX_H0]) ? 0 : -1;
    }
    if (op == 1 && layer >= 0 && layer < LAYERS) {
        fg_bonsai_slot *slot = &m->layers[layer];
        const fg_vk_view in = x[GX_H0 + (layer & 1)], out = x[GX_H0 + (1 - (layer & 1))];
        int ok = slot->attention ?
            fg_vk_bonsai_attention_layer(g, &slot->layer.full, m->s, in, out, m->epsilon, m->position, m->capacity) :
            fg_vk_bonsai_recurrent_layer(g, &slot->layer.recurrent, m->s, in, out, m->epsilon);
        return ok ? 0 : -1;
    }
    if (op != 2) return -1;
    const fg_vk_shape shape = {.width = H, .batch = 1, .epsilon = m->epsilon};
    const fg_vk_view weight = {&m->globals[G_RMS], 0, H * 4};
    if (!fg_vk_graph_record(g, FG_VK_RMSNORM, shape, x[GX_H0], weight, x[GX_NORM]) ||
        !fg_vk_graph_record(g, FG_VK_FWHT, shape, x[GX_NORM], m->signs[0], x[GX_ROTATED])) return -1;
    for (unsigned first = 0; first < FG_BONSAI_VOCAB; first += VOCAB_STRIPE) {
        unsigned rows = FG_BONSAI_VOCAB - first;
        if (rows > VOCAB_STRIPE) rows = VOCAB_STRIPE;
        const fg_vk_view stripe = {&m->globals[G_OUTPUT], (VkDeviceSize)first * ROW_BYTES, (VkDeviceSize)rows * ROW_BYTES};
        const fg_vk_shape mm = {.width = H, .rows = rows, .batch = 1, .position = first, .context = FG_BONSAI_VOCAB};
        if (!fg_vk_graph_record(g, FG_VK_PQ2_0, mm, x[GX_ROTATED], stripe, x[GX_LOGITS])) return -1;
    }
    return 0;
}

int fg_vk_bonsai_step(fg_vk_bonsai *m, uint32_t token, float *logits) {
    if (m->failed || token >= FG_BONSAI_VOCAB || m->position >= m->capacity) return 0;
    m->token = token;
    const fg_vk_view id = m->extra[GX_IDS], output = m->extra[GX_LOGITS];
    int ok = fg_vk_access_buffer(&m->vk, &m->host, id.offset, &token, sizeof(token), 1) == VK_SUCCESS &&
        fg_vk_graph_begin(&m->graph);
    const int64_t status = ok ? fg_vk_bonsai_forward((uintptr_t)m, logits != NULL) : -1;
    if (status) fprintf(stderr, "Bonsai Vulkan: failed to record graph (%lld)\n", (long long)status);
    ok = ok && !status && fg_vk_graph_submit(&m->graph) &&
        (!logits || fg_vk_access_buffer(&m->vk, &m->host, output.offset, logits, (size_t)output.size, 0) == VK_SUCCESS);
    if (ok) ++m->position;
    else m->failed = 1;
    return ok;
}

int fg_vk_bonsai_is_eog(const fg_vk_bonsai *m, uint32_t token) {
    for (int64_t i = 0; i < m->eog_count; ++i) {
        if (m->eog[i] == token) return 1;
    }
    return 0;
}
