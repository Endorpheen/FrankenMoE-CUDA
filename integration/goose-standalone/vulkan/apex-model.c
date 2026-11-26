#include "apex-model.h"
#include "gguf.h"
#include "moe-bank.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

enum { LAYERS = 40, H = 2048, MIX = 8192, D = 128, VH = 32, KH = 16, V = D * VH,
       QH = 16, KVH = 2, DH = 256, Q = QH * DH, KV = KVH * DH,
       E = 256, K = 8, FF = 512, CHUNK = 4 << 20 };
enum { WM_PRE, WM_POST, WM_ROUTER, WM_SHARED_GATE, WM_SHARED_UP, WM_SHARED_DOWN, WM_SHARED_ROUTER,
       WM_Q, WM_K, WM_V, WM_QN, WM_KN, WM_AO, WM_MIX, WM_Z, WM_ALPHA, WM_BETA, WM_DT_A,
       WM_CONV, WM_GN, WM_GO, WM_COUNT };
enum { SX_H0, SX_H1, SX_NORM, SX_MIX, SX_Q, SX_K, SX_V, SX_QN, SX_KN, SX_QR, SX_KR,
       SX_GATE, SX_ATTN, SX_GATED, SX_ALPHA, SX_BETA, SX_GATES, SX_CONV, SX_POST,
       SX_ROUTER, SX_GATE_FF, SX_UP_FF, SX_SWIGLU, SX_EXPERTS, SX_ROUTED, SX_SHARED, SX_SCALAR,
       SX_SHARED_GATED, SX_FF, SX_RESIDUAL, SX_COUNT };
enum { GLOBAL_EMBED, GLOBAL_NORM, GLOBAL_OUTPUT, GLOBAL_COUNT };

typedef struct {
    fg_vk_view view;
    fg_vk_op op;
} apex_weight;

typedef struct {
    fg_vk_buffer weights;
    apex_weight w[WM_COUNT];
    fg_vk_expert_plane planes[3];
    fg_vk_op expert_ops[3];
    fg_vk_view first, second;
    int recurrent;
} apex_layer;

struct fg_vk_apex {
    fg_vk_device vk;
    fg_vk_graph graph;
    fg_vk_upload reset;
    fg_vk_expert_bank bank;
    fg_vk_buffer globals[GLOBAL_COUNT], work, host, state;
    apex_weight global[GLOBAL_COUNT];
    apex_layer layers[LAYERS];
    fg_vk_view s[SX_COUNT], ids, selected, logits;
    FILE *model;
    uint64_t file_bytes;
    uint32_t capacity, position, token;
    float scale;
    int failed;
};

typedef struct {
    const char *name;
    uint32_t columns, rows, experts;
    unsigned allowed;
} tensor_spec;

typedef struct {
    fg_gguf *header;
    fg_vk_upload *upload;
    unsigned char *chunk;
    uint64_t max_expert[3];
} apex_loader;

enum { F32 = 1, Q4 = 2, Q6 = 4, QK = Q4 | Q6 };

static fg_vk_view apex_view(fg_vk_apex *m, const fg_vk_buffer *buffer, VkDeviceSize *end, VkDeviceSize bytes) {
    const VkDeviceSize align = m->vk.properties.limits.minStorageBufferOffsetAlignment;
    const VkDeviceSize start = (*end + align - 1) & ~(align - 1);
    *end = start + bytes;
    return (fg_vk_view){buffer, start, bytes};
}

static int apex_alloc(fg_vk_apex *m, fg_vk_buffer *buffer, VkDeviceSize bytes, int host, int transfer) {
    VkResult result = fg_vk_alloc_buffer(&m->vk, buffer, bytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | (transfer ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : 0),
        host ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        host ? VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : 0);
    return result == VK_SUCCESS || fg_vk_error("allocate Vulkan Apex buffer", result);
}

static int apex_tensor(fg_gguf *h, const tensor_spec *spec, const char *name,
    int64_t data[9], fg_vk_op *op) {
    const sl_i64 state = FG_GGUF_WORDS(h->state), tensors = FG_GGUF_WORDS(h->tensors);
    int64_t index = gr_find_tensor(h->data, state, tensors, FG_GGUF_WORDS(h->tensor_slots), fg_gguf_key(name));
    if (gr_tensor_info(state, tensors, index, fg_gguf_words(data, 9)) ||
        data[2] != spec->columns || data[3] != spec->rows ||
        data[4] != spec->experts || data[5] != 1 || data[7] <= 0 ||
        data[7] % 4 || data[8] < 0) return 0;
    unsigned type = data[0] == 0 ? F32 : data[0] == 12 ? Q4 : data[0] == 14 ? Q6 : 0;
    *op = type == F32 ? FG_VK_F32_PROJECT : type == Q4 ? FG_VK_Q4_K : FG_VK_Q6_K;
    return (spec->allowed & type) != 0;
}

static int apex_metadata(fg_vk_apex *m, fg_gguf *h) {
    const sl_i64 state = FG_GGUF_WORDS(h->state), meta = FG_GGUF_WORDS(h->metadata);
    if (!fg_gguf_metadata_is(h, "general.architecture", "qwen35moe") ||
        !fg_gguf_metadata_is(h, "tokenizer.ggml.pre", "qwen35")) return 0;
    static const struct { const char *name; int64_t value; } checks[] = {
        {"block_count", LAYERS + 1}, {"nextn_predict_layers", 1}, {"embedding_length", H},
        {"attention.head_count", QH}, {"attention.head_count_kv", KVH},
        {"attention.key_length", DH}, {"attention.value_length", DH},
        {"rope.dimension_count", 64}, {"full_attention_interval", 4},
        {"ssm.conv_kernel", 4}, {"ssm.state_size", D}, {"ssm.group_count", KH},
        {"ssm.time_step_rank", VH}, {"ssm.inner_size", V},
        {"expert_count", E}, {"expert_used_count", K},
        {"expert_feed_forward_length", FF}, {"expert_shared_feed_forward_length", FF},
    };
    for (unsigned i = 0; i < sizeof(checks) / sizeof(*checks); ++i) {
        char key[96];
        int64_t value;
        snprintf(key, sizeof(key), "qwen35moe.%s", checks[i].name);
        if (gr_meta_int(h->data, state, meta, fg_gguf_metadata(h, key), fg_gguf_words(&value, 1)) ||
            value != checks[i].value) return 0;
    }
    const int64_t layer_types = fg_gguf_metadata(h, "qwen35moe.attention.recurrent_layers");
    const int64_t sections = fg_gguf_metadata(h, "qwen35moe.rope.dimension_sections");
    const int64_t expected_sections[] = {11, 11, 10, 0};
    for (int64_t i = 0; i < 4; ++i) {
        int64_t value;
        if (gr_array_int(h->data, state, meta, sections, i, fg_gguf_words(&value, 1)) ||
            value != expected_sections[i]) return 0;
    }
    for (int64_t i = 0; i < LAYERS; ++i) {
        int64_t flag;
        if (gr_array_int(h->data, state, meta, layer_types, i, fg_gguf_words(&flag, 1)) ||
            flag != ((i + 1) % 4 != 0)) return 0;
        m->layers[i].recurrent = flag != 0;
    }
    const char *floats[] = {"qwen35moe.attention.layer_norm_rms_epsilon", "qwen35moe.rope.freq_base"};
    const float expected[] = {1e-6f, 1e7f};
    for (unsigned i = 0; i < 2; ++i) {
        int64_t raw;
        if (gr_meta_f32_bits(h->data, state, meta, fg_gguf_metadata(h, floats[i]), fg_gguf_words(&raw, 1))) return 0;
        const uint32_t bits = (uint32_t)raw;
        float value;
        memcpy(&value, &bits, sizeof(value));
        if (value != expected[i]) return 0;
    }
    const int64_t scale = fg_gguf_metadata(h, "qwen35moe.expert_weights_scale");
    if (scale >= 0) {
        int64_t raw;
        if (gr_meta_f32_bits(h->data, state, meta, scale, fg_gguf_words(&raw, 1))) return 0;
        const uint32_t bits = (uint32_t)raw;
        memcpy(&m->scale, &bits, sizeof(bits));
    }
    return isfinite(m->scale) && m->scale >= 0.0f;
}

static int apex_load_globals(fg_vk_apex *m, apex_loader *l) {
    static const tensor_spec spec[] = {
        {"token_embd.weight", H, FG_APEX_VOCAB, 1, Q4},
        {"output_norm.weight", H, 1, 1, F32},
        {"output.weight", H, FG_APEX_VOCAB, 1, Q6},
    };
    for (unsigned i = 0; i < GLOBAL_COUNT; ++i) {
        int64_t t[9];
        if (!apex_tensor(l->header, &spec[i], spec[i].name, t, &m->global[i].op) ||
            !apex_alloc(m, &m->globals[i], t[7], 0, 1) ||
            !fg_gguf_upload(m->model, l->upload, &m->globals[i], 0, t[8], t[7], l->chunk)) return 0;
        m->global[i].view = (fg_vk_view){&m->globals[i], 0, t[7]};
    }
    return 1;
}

static int apex_load_layer(fg_vk_apex *m, apex_loader *l, unsigned index) {
    static const tensor_spec spec[WM_COUNT] = {
        [WM_PRE] = {"attn_norm.weight", H, 1, 1, F32},
        [WM_POST] = {"post_attention_norm.weight", H, 1, 1, F32},
        [WM_ROUTER] = {"ffn_gate_inp.weight", H, E, 1, F32},
        [WM_SHARED_GATE] = {"ffn_gate_shexp.weight", H, FF, 1, Q4},
        [WM_SHARED_UP] = {"ffn_up_shexp.weight", H, FF, 1, Q4},
        [WM_SHARED_DOWN] = {"ffn_down_shexp.weight", FF, H, 1, QK},
        [WM_SHARED_ROUTER] = {"ffn_gate_inp_shexp.weight", H, 1, 1, F32},
        [WM_Q] = {"attn_q.weight", H, 2 * Q, 1, Q4},
        [WM_K] = {"attn_k.weight", H, KV, 1, Q4},
        [WM_V] = {"attn_v.weight", H, KV, 1, QK},
        [WM_QN] = {"attn_q_norm.weight", DH, 1, 1, F32},
        [WM_KN] = {"attn_k_norm.weight", DH, 1, 1, F32},
        [WM_AO] = {"attn_output.weight", Q, H, 1, Q4},
        [WM_MIX] = {"attn_qkv.weight", H, MIX, 1, QK},
        [WM_Z] = {"attn_gate.weight", H, V, 1, Q4},
        [WM_ALPHA] = {"ssm_alpha.weight", H, VH, 1, Q4},
        [WM_BETA] = {"ssm_beta.weight", H, VH, 1, Q4},
        [WM_DT_A] = {"ssm_dt.bias", VH, 1, 1, F32},
        [WM_CONV] = {"ssm_conv1d.weight", 4, MIX, 1, F32},
        [WM_GN] = {"ssm_norm.weight", D, 1, 1, F32},
        [WM_GO] = {"ssm_out.weight", V, H, 1, Q4},
    };
    static const tensor_spec experts[] = {
        {"ffn_gate_exps.weight", H, FF, E, Q4},
        {"ffn_up_exps.weight", H, FF, E, Q4},
        {"ffn_down_exps.weight", FF, H, E, QK},
    };
    apex_layer *layer = &m->layers[index];
    int64_t offset[WM_COUNT] = {0}, ssm_a = 0;
    VkDeviceSize end = 0;
    char name[96];
    for (unsigned i = 0; i < WM_COUNT; ++i) {
        if ((i >= WM_Q && i <= WM_AO && layer->recurrent) || (i >= WM_MIX && !layer->recurrent)) continue;
        snprintf(name, sizeof(name), "blk.%u.%s", index, spec[i].name);
        int64_t t[9];
        if (!apex_tensor(l->header, &spec[i], name, t, &layer->w[i].op)) {
            fprintf(stderr, "Apex Vulkan: incompatible tensor %s\n", name);
            return 0;
        }
        offset[i] = t[8];
        layer->w[i].view = apex_view(m, &layer->weights, &end, i == WM_DT_A ? 2 * t[7] : t[7]);
    }
    if (layer->recurrent) {
        const tensor_spec a = {"ssm_a", VH, 1, 1, F32};
        int64_t t[9];
        fg_vk_op unused;
        snprintf(name, sizeof(name), "blk.%u.%s", index, a.name);
        if (!apex_tensor(l->header, &a, name, t, &unused)) return 0;
        ssm_a = t[8];
    }
    for (unsigned i = 0; i < 3; ++i) {
        int64_t t[9];
        snprintf(name, sizeof(name), "blk.%u.%s", index, experts[i].name);
        if (!apex_tensor(l->header, &experts[i], name, t, &layer->expert_ops[i]) || t[7] % E) return 0;
        layer->planes[i] = (fg_vk_expert_plane){t[8], t[7] / E};
        if (layer->planes[i].expert_bytes > l->max_expert[i]) l->max_expert[i] = layer->planes[i].expert_bytes;
    }
    if (!apex_alloc(m, &layer->weights, end, 0, 1)) return 0;
    for (unsigned i = 0; i < WM_COUNT; ++i) {
        const fg_vk_view v = layer->w[i].view;
        if (!v.size) continue;
        const size_t bytes = i == WM_DT_A ? v.size / 2 : v.size;
        if (!fg_gguf_upload(m->model, l->upload, &layer->weights, v.offset, offset[i], bytes, l->chunk) ||
            (i == WM_DT_A && !fg_gguf_upload(m->model, l->upload, &layer->weights,
                v.offset + bytes, ssm_a, bytes, l->chunk))) return 0;
    }
    return 1;
}

static int apex_allocate_work(fg_vk_apex *m, fg_vk_upload *upload) {
    static const unsigned widths[SX_COUNT] = {
        H, H, H, MIX, Q, KV, KV, Q, KV, Q, KV, V, Q, V, VH, VH, VH * 2, MIX, H,
        E, FF, FF, FF, K * H, H, H, 1, H, H, H,
    };
    VkDeviceSize work = 0, host = 0, state = 0;
    for (unsigned i = 0; i < SX_COUNT; ++i) m->s[i] = apex_view(m, &m->work, &work, widths[i] * 4);
    m->ids = apex_view(m, &m->host, &host, sizeof(uint32_t));
    m->selected = apex_view(m, &m->host, &host, K * 8);
    m->logits = apex_view(m, &m->host, &host, FG_APEX_VOCAB * 4);
    for (unsigned i = 0; i < LAYERS; ++i) {
        apex_layer *layer = &m->layers[i];
        const VkDeviceSize cache = (VkDeviceSize)m->capacity * KV / 32 * 18;
        layer->first = apex_view(m, &m->state, &state, layer->recurrent ? MIX * 3 * 4 : cache);
        layer->second = apex_view(m, &m->state, &state, layer->recurrent ? V * D * 4 : cache);
    }
    return apex_alloc(m, &m->work, work, 0, 0) && apex_alloc(m, &m->host, host, 1, 0) &&
        apex_alloc(m, &m->state, state, 0, 1) && fg_vk_upload_zero(upload, &m->state);
}

static int apex_load(fg_vk_apex *m) {
    fg_gguf *h = calloc(1, sizeof(*h));
    fg_vk_upload upload = {0};
    unsigned char *chunk = malloc(CHUNK);
    apex_loader loader = {.header = h, .upload = &upload, .chunk = chunk};
    const int64_t file_bytes = h ? fg_gguf_read_header(h, m->model) : -1;
    int ok = file_bytes > 0 && chunk && apex_metadata(m, h) && fg_vk_upload_open(&upload, &m->vk, CHUNK);
    if (ok) {
        m->file_bytes = (uint64_t)file_bytes;
        ok = apex_load_globals(m, &loader);
        for (unsigned i = 0; i < LAYERS && ok; ++i) ok = apex_load_layer(m, &loader, i);
        ok = ok && apex_allocate_work(m, &upload) && fg_vk_expert_bank_open(&m->bank, &m->vk, K, loader.max_expert);
    } else {
        fputs("Apex Vulkan: incompatible GGUF metadata or upload setup failed\n", stderr);
    }
    fg_vk_upload_close(&upload);
    if (h) free(h->data.data);
    free(h);
    free(chunk);
    return ok;
}

void fg_vk_apex_close(fg_vk_apex *m) {
    if (!m) return;
    if (m->model) fclose(m->model);
    fg_vk_graph_close(&m->graph);
    fg_vk_upload_close(&m->reset);
    fg_vk_expert_bank_close(&m->bank);
    for (unsigned i = 0; i < LAYERS; ++i) fg_vk_free_buffer(&m->vk, &m->layers[i].weights);
    for (unsigned i = 0; i < GLOBAL_COUNT; ++i) fg_vk_free_buffer(&m->vk, &m->globals[i]);
    fg_vk_free_buffer(&m->vk, &m->work);
    fg_vk_free_buffer(&m->vk, &m->host);
    fg_vk_free_buffer(&m->vk, &m->state);
    fg_vk_close(&m->vk);
    free(m);
}

fg_vk_apex *fg_vk_apex_open(const char *path, uint32_t capacity, uint32_t device, int allow_cpu) {
    if (!capacity || capacity > 32768) return NULL;
    fg_vk_apex *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->capacity = capacity;
    m->model = fopen(path, "rb");
    if (!m->model || !fg_vk_open(&m->vk, device, allow_cpu) || !apex_load(m) ||
        !fg_vk_graph_open(&m->graph, &m->vk, 48) || !fg_vk_upload_open(&m->reset, &m->vk, 4)) {
        fg_vk_apex_close(m);
        return NULL;
    }
    fprintf(stderr, "Vulkan Apex on %s, context=%u, MTP off\n", m->vk.properties.deviceName, m->capacity);
    return m;
}

static int apex_element(fg_vk_apex *m, fg_vk_op op, uint32_t width, fg_vk_view a, fg_vk_view b, fg_vk_view out) {
    return fg_vk_graph_record(&m->graph, op, (fg_vk_shape){.width = width, .batch = 1, .epsilon = 1e-6f}, a, b, out);
}

static int apex_project(fg_vk_apex *m, uint32_t width, uint32_t rows, fg_vk_view x, apex_weight weight, fg_vk_view out) {
    return fg_vk_graph_record(&m->graph, weight.op, (fg_vk_shape){.width = width, .rows = rows, .batch = 1},
        x, weight.view, out);
}

static int apex_recurrent(fg_vk_apex *m, apex_layer *layer) {
    const apex_weight *w = layer->w;
    const fg_vk_view *s = m->s;
    fg_vk_graph *g = &m->graph;
    const fg_vk_shape norm = {.width = D, .rows = 2 * KH + VH, .kv_heads = KH, .batch = 1, .epsilon = 1e-6f};
    const fg_vk_shape gdn = {.width = D, .rows = VH, .kv_heads = KH, .batch = 1};
    const fg_vk_shape gate = {.width = D, .batch = VH, .epsilon = 1e-6f};

    return apex_project(m, H, MIX, s[SX_NORM], w[WM_MIX], s[SX_MIX]) &&
        apex_project(m, H, V, s[SX_NORM], w[WM_Z], s[SX_GATE]) &&
        apex_project(m, H, VH, s[SX_NORM], w[WM_ALPHA], s[SX_ALPHA]) &&
        apex_project(m, H, VH, s[SX_NORM], w[WM_BETA], s[SX_BETA]) &&
        fg_vk_graph_gdn_apply(g, VH, s[SX_ALPHA], s[SX_BETA], w[WM_DT_A].view, s[SX_GATES]) &&
        fg_vk_graph_conv(g, MIX, s[SX_MIX], w[WM_CONV].view, layer->first, s[SX_CONV]) &&
        fg_vk_graph_record(g, FG_VK_APEX_QK_NORM, norm, s[SX_CONV], s[SX_CONV], s[SX_MIX]) &&
        fg_vk_graph_gdn(g, gdn, s[SX_MIX], layer->second, s[SX_GATES], s[SX_ATTN]) &&
        fg_vk_graph_gated_rms(g, gate, s[SX_ATTN], w[WM_GN].view, s[SX_GATE], s[SX_GATED]) &&
        apex_project(m, V, H, s[SX_GATED], w[WM_GO], s[SX_POST]);
}

static int apex_attention(fg_vk_apex *m, apex_layer *layer) {
    const apex_weight *w = layer->w;
    const fg_vk_view *s = m->s;
    fg_vk_graph *g = &m->graph;
    const fg_vk_shape q_split = {.width = DH, .rows = QH, .batch = 1};
    fg_vk_shape gate = q_split;
    gate.position = 1;
    const fg_vk_shape q_norm = {.width = DH, .batch = QH, .epsilon = 1e-6f};
    fg_vk_shape k_norm = q_norm;
    k_norm.batch = KVH;
    // Text positions are the same in all MRoPE sections.
    const fg_vk_shape q_rope = {.width = DH, .rows = QH, .batch = 1, .position = m->position,
        .rotary = 64, .theta = 1e7f};
    fg_vk_shape k_rope = q_rope;
    k_rope.rows = KVH;
    const fg_vk_shape store = {.width = DH, .rows = KVH, .batch = 1,
        .position = m->position, .context = m->capacity};
    const fg_vk_shape rotate_q = {.width = Q, .rows = 256, .batch = 1};
    const fg_vk_shape rotate_k = {.width = KV, .rows = 256, .batch = 1};
    const fg_vk_shape rotate_v = {.width = KV, .rows = 64, .batch = 1};
    const fg_vk_shape rotate_o = {.width = Q, .rows = 64, .batch = 1};
    fg_vk_shape attention = store;
    attention.rows = QH;
    attention.kv_heads = KVH;
    return apex_project(m, H, 2 * Q, s[SX_NORM], w[WM_Q], s[SX_MIX]) &&
        apex_project(m, H, KV, s[SX_NORM], w[WM_K], s[SX_K]) &&
        apex_project(m, H, KV, s[SX_NORM], w[WM_V], s[SX_V]) &&
        fg_vk_graph_record(g, FG_VK_HEAD_SLICE, q_split, s[SX_MIX], s[SX_MIX], s[SX_Q]) &&
        fg_vk_graph_record(g, FG_VK_HEAD_SLICE, gate, s[SX_MIX], s[SX_MIX], s[SX_GATE]) &&
        fg_vk_graph_record(g, FG_VK_RMSNORM, q_norm, s[SX_Q], w[WM_QN].view, s[SX_QN]) &&
        fg_vk_graph_record(g, FG_VK_RMSNORM, k_norm, s[SX_K], w[WM_KN].view, s[SX_KN]) &&
        fg_vk_graph_record(g, FG_VK_ROPE, q_rope, s[SX_QN], s[SX_QN], s[SX_QR]) &&
        fg_vk_graph_record(g, FG_VK_ROPE, k_rope, s[SX_KN], s[SX_KN], s[SX_KR]) &&
        fg_vk_graph_record(g, FG_VK_KV_FWHT, rotate_q, s[SX_QR], s[SX_QR], s[SX_Q]) &&
        fg_vk_graph_record(g, FG_VK_KV_FWHT, rotate_k, s[SX_KR], s[SX_KR], s[SX_K]) &&
        fg_vk_graph_record(g, FG_VK_KV_FWHT, rotate_v, s[SX_V], s[SX_V], s[SX_KN]) &&
        fg_vk_graph_record(g, FG_VK_KV_STORE_Q4, store, s[SX_K], s[SX_K], layer->first) &&
        fg_vk_graph_record(g, FG_VK_KV_STORE_Q4, store, s[SX_KN], s[SX_KN], layer->second) &&
        fg_vk_graph_attention_q4(g, attention, s[SX_Q], layer->first, layer->second, s[SX_ATTN]) &&
        fg_vk_graph_record(g, FG_VK_KV_FWHT, rotate_o, s[SX_ATTN], s[SX_ATTN], s[SX_QR]) &&
        apex_element(m, FG_VK_SIGMOID_GATE, Q, s[SX_QR], s[SX_GATE], s[SX_GATED]) &&
        apex_project(m, Q, H, s[SX_GATED], w[WM_AO], s[SX_POST]);
}

static int apex_layer_step(fg_vk_apex *m, unsigned index) {
    apex_layer *layer = &m->layers[index];
    const apex_weight *w = layer->w;
    const fg_vk_view *s = m->s;
    fg_vk_graph *g = &m->graph;
    const fg_vk_view in = s[SX_H0 + index % 2], out = s[SX_H0 + 1 - index % 2];
    const fg_vk_shape route = {.width = E, .rows = K, .batch = 1, .theta = m->scale};
    const fg_vk_shape weighted = {.width = H, .rows = K, .batch = 1};

    uint32_t selected[K * 2];
    return fg_vk_graph_begin(g) &&
        apex_element(m, FG_VK_RMSNORM, H, in, w[WM_PRE].view, s[SX_NORM]) &&
        (layer->recurrent ? apex_recurrent(m, layer) : apex_attention(m, layer)) &&
        apex_element(m, FG_VK_ADD, H, in, s[SX_POST], s[SX_RESIDUAL]) &&
        apex_element(m, FG_VK_RMSNORM, H, s[SX_RESIDUAL], w[WM_POST].view, s[SX_NORM]) &&
        apex_project(m, H, E, s[SX_NORM], w[WM_ROUTER], s[SX_ROUTER]) &&
        fg_vk_graph_record(g, FG_VK_MOE_ROUTE, route, s[SX_ROUTER], s[SX_ROUTER], m->selected) &&
        apex_project(m, H, FF, s[SX_NORM], w[WM_SHARED_GATE], s[SX_GATE_FF]) &&
        apex_project(m, H, FF, s[SX_NORM], w[WM_SHARED_UP], s[SX_UP_FF]) &&
        apex_element(m, FG_VK_SWIGLU, FF, s[SX_GATE_FF], s[SX_UP_FF], s[SX_SWIGLU]) &&
        apex_project(m, FF, H, s[SX_SWIGLU], w[WM_SHARED_DOWN], s[SX_SHARED]) &&
        apex_project(m, H, 1, s[SX_NORM], w[WM_SHARED_ROUTER], s[SX_SCALAR]) &&
        apex_element(m, FG_VK_SIGMOID_GATE_SCALAR, H, s[SX_SHARED], s[SX_SCALAR], s[SX_SHARED_GATED]) &&
        fg_vk_graph_submit(g) &&
        fg_vk_access_buffer(&m->vk, &m->host, m->selected.offset, selected, sizeof(selected), 0) == VK_SUCCESS &&
        fg_vk_expert_bank_stage(&m->bank, m->model, m->file_bytes, E, layer->planes, selected, K) &&
        fg_vk_graph_begin(g) &&
        fg_vk_expert_bank_record_split(g, &m->bank, H, FF, layer->expert_ops, s[SX_NORM], s + SX_GATE_FF, s[SX_EXPERTS]) &&
        fg_vk_graph_record(g, FG_VK_MOE_WEIGHTED, weighted, s[SX_EXPERTS], m->selected, s[SX_ROUTED]) &&
        apex_element(m, FG_VK_ADD, H, s[SX_ROUTED], s[SX_SHARED_GATED], s[SX_FF]) &&
        apex_element(m, FG_VK_ADD, H, s[SX_FF], s[SX_RESIDUAL], out) &&
        fg_vk_graph_submit(g);
}

int64_t fg_vk_apex_op(uint64_t owner, int64_t op, int64_t layer) {
    fg_vk_apex *m = (fg_vk_apex *)(uintptr_t)owner;
    fg_vk_graph *g = &m->graph;
    const fg_vk_view *s = m->s;
    if (op == 0) {
        const unsigned first = m->token / 32 * 32;
        const fg_vk_shape gather = {.width = H, .rows = 32, .batch = 1, .position = first};
        const fg_vk_view stripe = {&m->globals[GLOBAL_EMBED], (VkDeviceSize)first * (H / 256 * 144), 32 * (H / 256 * 144)};
        return fg_vk_graph_begin(g) &&
            fg_vk_graph_record(g, FG_VK_GATHER_Q4_K, gather, m->ids, stripe, s[SX_H0]) &&
            fg_vk_graph_submit(g) ? 0 : -1;
    }
    if (op == 1 && layer >= 0 && layer < LAYERS) return apex_layer_step(m, (unsigned)layer) ? 0 : -1;
    if (op != 2 || !fg_vk_graph_begin(g) ||
        !apex_element(m, FG_VK_RMSNORM, H, s[SX_H0], m->global[GLOBAL_NORM].view, s[SX_NORM])) return -1;
    for (unsigned first = 0; first < FG_APEX_VOCAB; first += 32768) {
        const unsigned left = FG_APEX_VOCAB - first;
        const unsigned rows = left < 32768 ? left : 32768;
        const fg_vk_view stripe = {&m->globals[GLOBAL_OUTPUT], (VkDeviceSize)first * (H / 256 * 210),
            (VkDeviceSize)rows * (H / 256 * 210)};
        const fg_vk_shape mm = {.width = H, .rows = rows, .batch = 1, .position = first, .context = FG_APEX_VOCAB};
        if (!fg_vk_graph_record(g, FG_VK_Q6_K, mm, s[SX_NORM], stripe, m->logits)) return -1;
    }
    return fg_vk_graph_submit(g) ? 0 : -1;
}

int fg_vk_apex_step(fg_vk_apex *m, uint32_t token, float *logits) {
    if (m->failed || token >= FG_APEX_VOCAB || m->position >= m->capacity) return 0;
    m->token = token;
    const int64_t status = fg_vk_access_buffer(&m->vk, &m->host, m->ids.offset, &token, sizeof(token), 1) == VK_SUCCESS
        ? fg_vk_apex_forward((uintptr_t)m, logits != NULL) : -1;
    if (status) fprintf(stderr, "Apex Vulkan: model step failed (%lld)\n", (long long)status);
    const int ok = !status && (!logits || fg_vk_access_buffer(&m->vk, &m->host, m->logits.offset,
        logits, (size_t)m->logits.size, 0) == VK_SUCCESS);
    if (ok) ++m->position;
    else m->failed = 1;
    return ok;
}

int fg_vk_apex_reset(fg_vk_apex *m) {
    if (!m->position && !m->failed) return 1;
    if (!fg_vk_upload_zero(&m->reset, &m->state)) return 0;
    m->graph.recording = 0;
    m->position = 0;
    m->failed = 0;
    return 1;
}
