#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200809L
#include "qwen2.h"
#include "qwen2_unit.h"
#include "tokenizer.h"
#include "upload.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { HIDDEN = 896, FFN = 4864, HEAD_DIM = 64, HEADS = 14, KV_HEADS = 2,
       KV_WIDTH = KV_HEADS * HEAD_DIM, PROJECTION_ROWS = 32768, STAGING_BYTES = 4 << 20 };
enum { GGML_F32 = 0, GGML_Q4_0 = 2, GGML_Q8_0 = 8 };

typedef struct {
    sl_u8 data;
    int64_t state[8], metadata[8 * 256], tensors[12 * 512], meta_slots[512], tensor_slots[1024];
} fg_gguf;

typedef struct {
    uint64_t file_offset, bytes;
    VkDeviceSize device_offset;
} fg_weight;

static sl_u8 key(const char *text) {
    return (sl_u8){(uint8_t *)text, (int64_t)strlen(text)};
}

static sl_i64 words(int64_t *data, size_t count) {
    return (sl_i64){data, (int64_t)count};
}

#define WORDS(array) words((array), sizeof(array) / sizeof(*(array)))

static int64_t metadata(fg_gguf *file, const char *name) {
    return gr_find_kv(file->data, WORDS(file->state), WORDS(file->metadata), WORDS(file->meta_slots), key(name));
}

static int metadata_is(fg_gguf *file, const char *name, const char *value) {
    int64_t span[4];
    return !gr_val_span(WORDS(file->state), WORDS(file->metadata), metadata(file, name), WORDS(span)) &&
        span[0] == 8 && span[2] == (int64_t)strlen(value) &&
        !memcmp(file->data.data + span[1], value, (size_t)span[2]);
}

static int64_t read_header(fg_gguf *file, FILE *model) {
    if (fseeko(model, 0, SEEK_END)) return -1;
    const int64_t size = ftello(model);
    if (size < 24) return -1;
    const size_t prefix = size > (64 << 20) ? 64 << 20 : (size_t)size;
    file->data = (sl_u8){malloc(prefix), (int64_t)prefix};
    if (!file->data.data || fseeko(model, 0, SEEK_SET) || fread(file->data.data, 1, prefix, model) != prefix ||
        gr_parse(file->data, size, WORDS(file->state), WORDS(file->metadata), WORDS(file->tensors),
                 WORDS(file->meta_slots), WORDS(file->tensor_slots))) return -1;
    return size;
}

extern fg_vk_tokenizer *fg_vk_tokenizer_from_gguf(sl_u8 data, sl_i64 state, sl_i64 meta,
    int64_t tokens, int64_t merges, int64_t types, int64_t eos);

fg_vk_tokenizer *fg_vk_qwen2_tokenizer_open(const char *path) {
    FILE *model = fopen(path, "rb");
    if (!model) {
        perror(path);
        return NULL;
    }
    fg_gguf file = {0};
    fg_vk_tokenizer *tokenizer = NULL;
    int64_t eos;
    if (read_header(&file, model) >= 0 && metadata_is(&file, "general.architecture", "qwen2") &&
        metadata_is(&file, "tokenizer.ggml.model", "gpt2") && metadata_is(&file, "tokenizer.ggml.pre", "qwen2") &&
        !gr_meta_int(file.data, WORDS(file.state), WORDS(file.metadata),
                     metadata(&file, "tokenizer.ggml.eos_token_id"), words(&eos, 1)) &&
        eos >= 0 && eos < FG_QWEN_VOCAB) {
        tokenizer = fg_vk_tokenizer_from_gguf(file.data, WORDS(file.state), WORDS(file.metadata),
            metadata(&file, "tokenizer.ggml.tokens"), metadata(&file, "tokenizer.ggml.merges"),
            metadata(&file, "tokenizer.ggml.token_type"), eos);
    }
    free(file.data.data);
    fclose(model);
    return tokenizer;
}

static int validate_model(fg_gguf *file, fg_vk_qwen2 *model) {
    if (!metadata_is(file, "general.architecture", "qwen2")) return 0;
    const struct { const char *name; int64_t expected; } dims[] = {
        {"qwen2.block_count", FG_QWEN_LAYERS}, {"qwen2.embedding_length", HIDDEN},
        {"qwen2.feed_forward_length", FFN}, {"qwen2.attention.head_count", HEADS},
        {"qwen2.attention.head_count_kv", KV_HEADS},
    };
    for (size_t i = 0; i < sizeof(dims) / sizeof(*dims); ++i) {
        int64_t value;
        if (gr_meta_int(file->data, WORDS(file->state), WORDS(file->metadata),
            metadata(file, dims[i].name), words(&value, 1)) || value != dims[i].expected) return 0;
    }
    int64_t maximum, eps, theta;
    if (gr_meta_int(file->data, WORDS(file->state), WORDS(file->metadata),
            metadata(file, "qwen2.context_length"), words(&maximum, 1)) ||
        maximum < model->context ||
        gr_meta_f32_bits(file->data, WORDS(file->state), WORDS(file->metadata),
            metadata(file, "qwen2.attention.layer_norm_rms_epsilon"), words(&eps, 1)) ||
        gr_meta_f32_bits(file->data, WORDS(file->state), WORDS(file->metadata),
            metadata(file, "qwen2.rope.freq_base"), words(&theta, 1))) return 0;
    const uint32_t eps_bits = (uint32_t)eps, theta_bits = (uint32_t)theta;
    memcpy(&model->epsilon, &eps_bits, sizeof(eps_bits));
    memcpy(&model->theta, &theta_bits, sizeof(theta_bits));
    return model->epsilon == 1e-6f && model->theta == 1000000.0f;
}

static VkDeviceSize align_size(VkDeviceSize v, VkDeviceSize alignment) {
    return (v + alignment - 1) & ~(alignment - 1);
}

static int bind_weight(fg_gguf *file, const fg_vk_device *vk, fg_weight *weight, VkDeviceSize *end,
    const char *name, int64_t type, int64_t width, int64_t rows, uint64_t file_size) {
    int64_t info[9];
    const int64_t index = gr_find_tensor(file->data, WORDS(file->state), WORDS(file->tensors),
        WORDS(file->tensor_slots), key(name));
    if (gr_tensor_info(WORDS(file->state), WORDS(file->tensors), index, WORDS(info)) ||
        info[0] != type || info[1] != (rows == 1 ? 1 : 2) ||
        info[2] != width || info[3] != rows || info[7] <= 0 || info[8] < 0 ||
        (uint64_t)info[8] > file_size || (uint64_t)info[7] > file_size - (uint64_t)info[8]) {
        fprintf(stderr, "Qwen2 Vulkan: unsupported or missing tensor %s\n", name);
        return 0;
    }
    const VkDeviceSize offset = align_size(*end, vk->properties.limits.minStorageBufferOffsetAlignment);
    *weight = (fg_weight){(uint64_t)info[8], (uint64_t)info[7], offset};
    *end = offset + align_size(weight->bytes, 4);
    return 1;
}

static int layout(fg_gguf *file, fg_vk_qwen2 *model, fg_weight *weights, uint64_t size, VkDeviceSize *end) {
    const struct { const char *name; int64_t type, width, rows; } globals[] = {
        [WT_EMBED] = {"token_embd.weight", GGML_Q4_0, HIDDEN, FG_QWEN_VOCAB},
        [WT_NORM] = {"output_norm.weight", GGML_F32, HIDDEN, 1},
        [WT_LOGITS] = {"output.weight", GGML_Q8_0, HIDDEN, FG_QWEN_VOCAB},
    }, per_layer[] = {
        [WT_AN] = {"attn_norm.weight", GGML_F32, HIDDEN, 1},
        [WT_Q] = {"attn_q.weight", GGML_Q4_0, HIDDEN, HIDDEN},
        [WT_K] = {"attn_k.weight", GGML_Q4_0, HIDDEN, KV_WIDTH},
        [WT_V] = {"attn_v.weight", GGML_Q4_0, HIDDEN, KV_WIDTH},
        [WT_QB] = {"attn_q.bias", GGML_F32, HIDDEN, 1},
        [WT_KB] = {"attn_k.bias", GGML_F32, KV_WIDTH, 1},
        [WT_VB] = {"attn_v.bias", GGML_F32, KV_WIDTH, 1},
        [WT_O] = {"attn_output.weight", GGML_Q4_0, HIDDEN, HIDDEN},
        [WT_FN] = {"ffn_norm.weight", GGML_F32, HIDDEN, 1},
        [WT_GATE] = {"ffn_gate.weight", GGML_Q4_0, HIDDEN, FFN},
        [WT_UP] = {"ffn_up.weight", GGML_Q4_0, HIDDEN, FFN},
        [WT_DOWN] = {"ffn_down.weight", GGML_Q4_0, FFN, HIDDEN},
    };
    for (unsigned i = 0; i < WT_GLOBAL_COUNT; ++i) {
        if (!bind_weight(file, &model->vk, &weights[i], end, globals[i].name,
            globals[i].type, globals[i].width, globals[i].rows, size)) return 0;
    }
    for (unsigned layer = 0; layer < FG_QWEN_LAYERS; ++layer) {
        for (unsigned i = 0; i < WT_COUNT; ++i) {
            char name[64];
            snprintf(name, sizeof(name), "blk.%u.%s", layer, per_layer[i].name);
            if (!bind_weight(file, &model->vk, &weights[WT_GLOBAL_COUNT + layer * WT_COUNT + i],
                end, name, per_layer[i].type, per_layer[i].width, per_layer[i].rows, size)) return 0;
        }
    }
    return 1;
}

static int upload_model(FILE *file, fg_vk_qwen2 *model, const fg_weight *weights, unsigned count) {
    fg_vk_upload upload = {0};
    unsigned char *chunk = malloc(STAGING_BYTES);
    int ok = chunk && fg_vk_upload_open(&upload, &model->vk, STAGING_BYTES);
    for (unsigned i = 0; ok && i < count; ++i) {
        const fg_weight *w = &weights[i];
        ok = fseeko(file, w->file_offset, SEEK_SET) == 0;
        for (uint64_t at = 0; ok && at < w->bytes;) {
            const size_t bytes = w->bytes - at > STAGING_BYTES ? STAGING_BYTES : (size_t)(w->bytes - at);
            const size_t padded = (size_t)align_size(bytes, 4);
            ok = fread(chunk, 1, bytes, file) == bytes;
            if (ok) {
                memset(chunk + bytes, 0, padded - bytes);
                ok = fg_vk_upload_write(&upload, &model->weights, w->device_offset + at, chunk, padded);
            }
            at += bytes;
        }
    }
    fg_vk_upload_close(&upload);
    free(chunk);
    return ok;
}

static fg_vk_view view_at(const fg_vk_buffer *buffer, VkDeviceSize *end, VkDeviceSize bytes, VkDeviceSize alignment) {
    const VkDeviceSize offset = align_size(*end, alignment);
    *end = offset + bytes;
    return (fg_vk_view){buffer, offset, bytes};
}

static int allocate_work(fg_vk_qwen2 *m) {
    const VkDeviceSize alignment = m->vk.properties.limits.minStorageBufferOffsetAlignment;
    VkDeviceSize device_end = 0, host_end = 0;
    const unsigned scratch_width[T_COUNT] = {
        [T_IDS] = 1, [T_HIDDEN] = HIDDEN, [T_NORM] = HIDDEN, [T_Q] = HIDDEN, [T_K] = KV_WIDTH, [T_V] = KV_WIDTH,
        [T_Q_BIAS] = HIDDEN, [T_K_BIAS] = KV_WIDTH, [T_V_BIAS] = KV_WIDTH, [T_ATTN] = HIDDEN,
        [T_PROJECTED] = HIDDEN, [T_RESIDUAL] = HIDDEN, [T_GATE] = FFN, [T_UP] = FFN, [T_PRODUCT] = FFN,
        [T_LOGITS] = FG_QWEN_VOCAB,
    };
    for (unsigned i = 0; i < T_COUNT; ++i) {
        const int host = i == T_IDS || i == T_LOGITS;
        m->scratch[i] = view_at(host ? &m->host : &m->work, host ? &host_end : &device_end,
                                scratch_width[i] * 4, alignment);
    }
    for (unsigned i = 0; i < FG_QWEN_LAYERS; ++i) {
        m->keys[i] = view_at(&m->work, &device_end, (VkDeviceSize)m->context * KV_WIDTH * 4, alignment);
        m->values[i] = view_at(&m->work, &device_end, (VkDeviceSize)m->context * KV_WIDTH * 4, alignment);
    }
    VkResult result = fg_vk_alloc_buffer(&m->vk, &m->work, device_end, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
    if (result == VK_SUCCESS) result = fg_vk_alloc_buffer(&m->vk, &m->host, host_end, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    return result == VK_SUCCESS || fg_vk_error("allocate Vulkan Qwen2 workspace", result);
}

void fg_vk_qwen2_close(fg_vk_qwen2 *m) {
    fg_vk_graph_close(&m->graph);
    fg_vk_free_buffer(&m->vk, &m->weights);
    fg_vk_free_buffer(&m->vk, &m->work);
    fg_vk_free_buffer(&m->vk, &m->host);
    fg_vk_close(&m->vk);
}

int fg_vk_qwen2_open(fg_vk_qwen2 *m, const char *path, uint32_t context, uint32_t index, int allow_cpu) {
    *m = (fg_vk_qwen2){.context = context};
    if (!context || context > 32768) return 0;
    FILE *model = fopen(path, "rb");
    if (!model) {
        perror(path);
        return 0;
    }
    fg_gguf file = {0};
    const int64_t file_size = read_header(&file, model);
    int ok = file_size >= 0 && validate_model(&file, m);
    if (!ok) fputs("Qwen2 Vulkan: invalid or incompatible GGUF metadata\n", stderr);
    VkDeviceSize size = 0;
    fg_weight weights[WT_GLOBAL_COUNT + FG_QWEN_LAYERS * WT_COUNT];
    if (ok) ok = fg_vk_open(&m->vk, index, allow_cpu) && layout(&file, m, weights, (uint64_t)file_size, &size);
    free(file.data.data);
    if (ok) {
        VkResult result = fg_vk_alloc_buffer(&m->vk, &m->weights, size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
        ok = result == VK_SUCCESS || fg_vk_error("allocate Vulkan model weights", result);
    }
    if (ok) ok = upload_model(model, m, weights, sizeof(weights) / sizeof(*weights));
    fclose(model);
    if (ok) ok = allocate_work(m) && fg_vk_graph_open(&m->graph, &m->vk, 512);
    if (!ok) {
        fg_vk_qwen2_close(m);
        return 0;
    }
    for (unsigned i = 0; i < WT_GLOBAL_COUNT; ++i) {
        m->globals[i] = (fg_vk_view){&m->weights, weights[i].device_offset, align_size(weights[i].bytes, 4)};
    }
    for (unsigned l = 0; l < FG_QWEN_LAYERS; ++l) {
        for (unsigned i = 0; i < WT_COUNT; ++i) {
            const fg_weight *w = &weights[WT_GLOBAL_COUNT + l * WT_COUNT + i];
            m->layers[l][i] = (fg_vk_view){&m->weights, w->device_offset, align_size(w->bytes, 4)};
        }
    }
    return 1;
}

static int record(fg_vk_qwen2 *m, fg_vk_op op, unsigned width, unsigned rows,
    unsigned a, fg_vk_view b, unsigned out) {
    const fg_vk_shape shape = {.width = width, .rows = rows, .batch = 1, .epsilon = m->epsilon,
        .position = op == FG_VK_ROPE || op == FG_VK_KV_STORE ? m->position : 0,
        .context = op == FG_VK_KV_STORE ? m->context : 0, .theta = m->theta};
    return fg_vk_graph_record(&m->graph, op, shape, m->scratch[a], b, m->scratch[out]);
}

static int write_kv(fg_vk_qwen2 *m, unsigned input, fg_vk_view dest) {
    const fg_vk_shape shape = {.width = HEAD_DIM, .rows = KV_HEADS, .batch = 1,
        .position = m->position, .context = m->context};
    return fg_vk_graph_record(&m->graph, FG_VK_KV_STORE, shape, m->scratch[input], m->scratch[input], dest);
}

static int project_logits(fg_vk_qwen2 *m) {
    const unsigned row_bytes = HIDDEN / 32 * 34;
    for (unsigned start = 0; start < FG_QWEN_VOCAB; start += PROJECTION_ROWS) {
        const unsigned remaining = FG_QWEN_VOCAB - start;
        const unsigned count = remaining < PROJECTION_ROWS ? remaining : PROJECTION_ROWS;
        const fg_vk_shape shape = {.width = HIDDEN, .rows = count, .batch = 1,
            .position = start, .context = FG_QWEN_VOCAB};
        fg_vk_view stripe = m->globals[WT_LOGITS];
        stripe.offset += (VkDeviceSize)start * row_bytes;
        stripe.size = align_size((VkDeviceSize)count * row_bytes, 4);
        if (!fg_vk_graph_record(&m->graph, FG_VK_Q8_0, shape, m->scratch[T_NORM], stripe, m->scratch[T_LOGITS])) return 0;
    }
    return 1;
}

int64_t fg_vk_qwen_op(uint64_t context, int64_t op, int64_t layer) {
    fg_vk_qwen2 *m = (fg_vk_qwen2 *)(uintptr_t)context;
    if (!m || layer < 0 || layer >= FG_QWEN_LAYERS) return -1;
    const fg_vk_view *w = m->layers[layer], *s = m->scratch;
    const fg_vk_shape attn = {.width = HEAD_DIM, .rows = HEADS, .batch = 1,
        .position = m->position, .kv_heads = KV_HEADS, .context = m->context};
    int ok = 0;
    switch (op) {
    case 0:
        ok = record(m, FG_VK_GATHER_Q4_0, HIDDEN, FG_QWEN_VOCAB, T_IDS, m->globals[WT_EMBED], T_HIDDEN);
        break;
    case 1:
        ok = record(m, FG_VK_RMSNORM, HIDDEN, 0, T_HIDDEN, w[WT_AN], T_NORM);
        break;
    case 2:
        ok = record(m, FG_VK_Q4_0, HIDDEN, HIDDEN, T_NORM, w[WT_Q], T_Q) &&
             record(m, FG_VK_Q4_0, HIDDEN, KV_WIDTH, T_NORM, w[WT_K], T_K) &&
             record(m, FG_VK_Q4_0, HIDDEN, KV_WIDTH, T_NORM, w[WT_V], T_V) &&
             record(m, FG_VK_BIAS, HIDDEN, 0, T_Q, w[WT_QB], T_Q_BIAS) &&
             record(m, FG_VK_BIAS, KV_WIDTH, 0, T_K, w[WT_KB], T_K_BIAS) &&
             record(m, FG_VK_BIAS, KV_WIDTH, 0, T_V, w[WT_VB], T_V_BIAS);
        break;
    case 3:
        ok = record(m, FG_VK_ROPE, HEAD_DIM, HEADS, T_Q_BIAS, s[T_Q_BIAS], T_Q) &&
             record(m, FG_VK_ROPE, HEAD_DIM, KV_HEADS, T_K_BIAS, s[T_K_BIAS], T_K) &&
             write_kv(m, T_K, m->keys[layer]) && write_kv(m, T_V_BIAS, m->values[layer]);
        break;
    case 4:
        ok = fg_vk_graph_attention(&m->graph, attn, s[T_Q], m->keys[layer], m->values[layer], s[T_ATTN]) &&
             record(m, FG_VK_Q4_0, HIDDEN, HIDDEN, T_ATTN, w[WT_O], T_PROJECTED) &&
             record(m, FG_VK_ADD, HIDDEN, 0, T_HIDDEN, s[T_PROJECTED], T_RESIDUAL);
        break;
    case 5:
        ok = record(m, FG_VK_RMSNORM, HIDDEN, 0, T_RESIDUAL, w[WT_FN], T_NORM);
        break;
    case 6:
        ok = record(m, FG_VK_Q4_0, HIDDEN, FFN, T_NORM, w[WT_GATE], T_GATE) &&
             record(m, FG_VK_Q4_0, HIDDEN, FFN, T_NORM, w[WT_UP], T_UP) &&
             record(m, FG_VK_SWIGLU, FFN, 0, T_GATE, s[T_UP], T_PRODUCT);
        break;
    case 7:
        ok = record(m, FG_VK_Q4_0, FFN, HIDDEN, T_PRODUCT, w[WT_DOWN], T_PROJECTED) &&
             record(m, FG_VK_ADD, HIDDEN, 0, T_RESIDUAL, s[T_PROJECTED], T_HIDDEN);
        break;
    case 8:
        ok = record(m, FG_VK_RMSNORM, HIDDEN, 0, T_HIDDEN, m->globals[WT_NORM], T_NORM) && project_logits(m);
        break;
    }
    return ok ? 0 : -1;
}

int fg_vk_qwen2_step(fg_vk_qwen2 *m, uint32_t id, float *logits) {
    if (id >= FG_QWEN_VOCAB || m->position >= m->context) return 0;
    const fg_vk_view ids = m->scratch[T_IDS], output = m->scratch[T_LOGITS];
    if (fg_vk_access_buffer(&m->vk, &m->host, ids.offset, &id, sizeof(id), 1) != VK_SUCCESS ||
        !fg_vk_graph_begin(&m->graph)) return 0;
    const int64_t rc = fg_vk_qwen2_forward((uintptr_t)m, FG_QWEN_LAYERS, logits != NULL);
    if (rc != 0) {
        fprintf(stderr, "Qwen2 Vulkan: graph record failed (%lld)\n", (long long)rc);
        return 0;
    }
    if (!fg_vk_graph_submit(&m->graph) ||
        (logits && fg_vk_access_buffer(&m->vk, &m->host, output.offset, logits, (size_t)output.size, 0) != VK_SUCCESS)) return 0;
    ++m->position;
    return 1;
}

int fg_vk_qwen2_choose(const float *logits, const fg_vk_tokenizer *ignore_eog, uint32_t *chosen) {
    uint32_t best = UINT32_MAX;
    for (uint32_t i = 0; i < FG_QWEN_VOCAB; ++i) {
        if (!isfinite(logits[i])) return 0;
        if ((best == UINT32_MAX || logits[i] > logits[best]) && !fg_vk_tokenizer_is_eog(ignore_eog, i)) best = i;
    }
    if (best == UINT32_MAX) return 0;
    *chosen = best;
    return 1;
}
