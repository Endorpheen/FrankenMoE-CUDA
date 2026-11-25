#define _POSIX_C_SOURCE 200809L

#include "bonsai.h"
#include <cuda.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

enum { LAYERS = 64, POSITIONS_MAX = 262144, VOCAB = 248320, KV_BYTES = 576,
       GDN_STATE = 48 * 128 * 128, GDN_HISTORY = 3 * 10240, CONTEXT_MIN = 1 };

enum kernel {
    NORM, ROTATE, MATRIX, ADD, DECAY, CONV, GDN, SPLIT, ROPE, Q4, TILES, GATE,
    EMBED, SWIGLU, MERGE, KERNEL_COUNT
};

typedef struct {
    CUcontext cuda;
    CUmodule module, frozen;
    CUfunction kernel[KERNEL_COUNT];
    CUdeviceptr weights, memory, positions, rope_q, rope_k, attention, partial;
    int64_t bound[LAYERS * 15], states[LAYERS * 2], globals[3], b[24];
    unsigned context, position;
    int64_t eog[64], eog_count;
    float epsilon;
} model;

static int check(CUresult status, const char *where) {
    if (status == CUDA_SUCCESS) return 0;
    const char *message = NULL;
    (void) cuGetErrorString(status, &message);
    fprintf(stderr, "%s: CUDA %d (%s)\n", where, status, message ? message : "unknown");
    return -1;
}

static int launch(model *m, enum kernel id, unsigned x, unsigned y, unsigned z,
                  unsigned threads, void **args) {
    return check(cuLaunchKernel(m->kernel[id], x, y, z, threads, 1, 1, 0, NULL, args, NULL), "kernel launch");
}

static uint32_t f32_bits(float x) {
    uint32_t bits;
    memcpy(&bits, &x, sizeof(bits));
    return bits;
}

static int gdn(model *m, const CUdeviceptr p[8]) {
    uint64_t packet[57];
    memcpy(packet, p, 8 * sizeof(*p));
    const uint64_t config[49] = {
        1, 128, 2048, 2048, 1, 128, 2048, 2048, 1, 128, 6144, 6144,
        1, 1, 48, 48, 1, 1, 48, 48, 1, 128, 16384, GDN_STATE,
        128, 48, 1, 1, 0, 0, 1, 0, 0, 0, 8, 32, 4, 32, 1, GDN_STATE,
        0, f32_bits(1.0f / sqrtf(128.0f)), 1, 4, 16, 1, 0, 1, 6144,
    };
    memcpy(packet + 8, config, sizeof(config));
    return launch(m, GDN, 48, 1, 8, 128, (void *[]){ packet });
}

int64_t bonsai_launch(uint64_t context, int64_t op, int64_t a, int64_t b, int64_t c,
                      int64_t d, int64_t e, int64_t f, int64_t g, int64_t h) {
    model *m = (model *)(uintptr_t)context;
    CUdeviceptr p[] = { a, b, c, d, e, f, g, h };
    unsigned n = (unsigned)e, rows = (unsigned)f, mode = (unsigned)g;
    unsigned out_width = 0, in_width = 0, heads = 0;
    float scale = 1.0f / 16.0f;
    int start = 0;
    switch (op) {
        case NORM:
            return launch(m, NORM, rows, 1, 1, 32, (void *[]){ &p[0], &p[1], &p[2], &p[3], &n, &m->epsilon, &mode });
        case ROTATE: {
            const unsigned width = (unsigned)d, inverse = (unsigned)e;
            const unsigned head = f ? 128 : 0, keys = f ? 16 : 0, repeat = f ? 3 : 1;
            return launch(m, ROTATE, width/1024, 1, 1, 256,
                (void *[]){ &p[0], &p[1], &p[2], (void *)&inverse, (void *)&head, (void *)&keys, (void *)&repeat });
        }
        case MATRIX:
            in_width = (unsigned)d; out_width = (unsigned)e;
            return launch(m, MATRIX, (out_width+3)/4, 1, 1, 128, (void *[]){ &p[0], &p[1], &p[2], &in_width, &out_width });
        case ADD:
            return launch(m, ADD, 20, 1, 1, 256, (void *[]){ &p[0], &p[1], &p[2] });
        case DECAY:
            return launch(m, DECAY, 48, 1, 1, 32, (void *[]){ &p[0], &p[1], &p[2], &p[3], &p[4], &p[5], &p[6] });
        case CONV:
            return launch(m, CONV, 40, 1, 1, 256, (void *[]){ &p[0], &p[1], &p[2], &p[3] });
        case GDN:
            return gdn(m, p);
        case SPLIT:
            return launch(m, SPLIT, 24, 1, 1, 256, (void *[]){ &p[0], &p[1], &p[2] });
        case ROPE: {
            heads = (unsigned)c;
            CUdeviceptr config = heads == 24 ? m->rope_q : m->rope_k, no_factors = 0;
            return launch(m, ROPE, heads, 1, 1, 128, (void *[]){ &p[0], &m->positions, &no_factors, &p[1], &config });
        }
        case Q4: {
            uint64_t stride = 4096, tasks = 32;
            unsigned blocks = 32;
            return launch(m, Q4, 1, 1, 1, 128, (void *[]){ &p[0], &p[1], &stride, &blocks, &tasks });
        }
        case TILES: {
            CUdeviceptr no_mask = 0;
            const unsigned tiles = ((unsigned)e + 255)/256;
            if (launch(m, TILES, 24, tiles, 1, 32, (void *[]){
                &p[0], &p[1], &p[2], &no_mask, &m->partial, &m->attention, &scale, &start }) != 0) return -1;
            return launch(m, MERGE, 24, 1, 1, 32, (void *[]){ &m->partial, &p[3], &m->attention, &start });
        }
        case GATE:
            return launch(m, GATE, 24, 1, 1, 256, (void *[]){ &p[0], &p[1], &p[2] });
        case EMBED: {
            unsigned id = (unsigned)c, width = 5120;
            return launch(m, EMBED, 20, 1, 1, 256, (void *[]){ &p[0], &p[1], &width, &id });
        }
        case SWIGLU:
            out_width = 17408;
            return launch(m, SWIGLU, 68, 1, 1, 256, (void *[]){ &p[0], &p[1], &p[2], &out_width });
        default:
            return -1;
    }
}

static sl_i64 slice(int64_t *data, int64_t len) {
    return (sl_i64){ data, len };
}

static int64_t key(sl_u8 file, sl_i64 st, sl_i64 kv, sl_i64 slots, const char *name) {
    return gr_find_kv(file, st, kv, slots, (sl_u8){ (uint8_t *)name, (int64_t)strlen(name) });
}

static int load_eog(model *m, sl_u8 file, sl_i64 st, sl_i64 meta, sl_i64 slots) {
    const int64_t vocabulary = key(file, st, meta, slots, "tokenizer.ggml.tokens");
    int64_t *tokens = malloc(2 * VOCAB * sizeof(*tokens));
    if (!tokens) return -1;
    const sl_i64 offsets = slice(tokens, VOCAB), lengths = slice(tokens + VOCAB, VOCAB);
    const int64_t count = gr_arr_str_offsets(file, st, meta, vocabulary, offsets, lengths);
    const char *names[] = {"eos", "eot", "eom"};
    int64_t control[3] = {-1, -1, -1};
    for (unsigned i = 0; i < 3; ++i) {
        char name[48];
        snprintf(name, sizeof(name), "tokenizer.ggml.%s_token_id", names[i]);
        const int64_t index = key(file, st, meta, slots, name);
        if (index >= 0 && gr_meta_int(file, st, meta, index, slice(control + i, 1))) {
            free(tokens);
            return -1;
        }
    }
    if (count == VOCAB) {
        m->eog_count = bt_eog_ids(file, offsets, lengths, control[0], control[1], control[2],
                                 slice(m->eog, sizeof(m->eog) / sizeof(*m->eog)));
    }
    free(tokens);
    return m->eog_count > 0 ? 0 : -1;
}

static CUdeviceptr reserve(CUdeviceptr *ptr, size_t length) {
    const CUdeviceptr start = *ptr;
    *ptr += (length + 255) & ~(size_t)255;
    return start;
}

static int rope_config(CUdeviceptr dest, int64_t heads) {
    const int64_t conf[27] = {
        2, heads, 1, 1, 1024, 256, heads, 1, 1, 64, 40, 0, 11, 11, 10, 0,
        4, 1024, heads * 1024, heads * 1024, 1,
        f32_bits(powf(1e7f, -1.0f / 32)), f32_bits(1), 0, f32_bits(1), 0, 0,
    };
    return check(cuMemcpyHtoD(dest, conf, sizeof(conf)), "RoPE configuration");
}

static int workspace(model *m, const int32_t *sign) {
    const unsigned words[23] = {
        5120, 5120, 5120, 12288, 6144, 1024, 6144, 10240, 6144, 6144, 6144, 6144,
        5120, 5120, 17408, 17408, 17408, 17408, 5120, VOCAB, 5120, 6144, 17408,
    };
    const size_t cache = m->context * (size_t)KV_BYTES;
    const size_t partial = 24 * ((m->context + 255)/256) * (256 + 2) * sizeof(float);
    CUdeviceptr ptr = 0;
    for (unsigned layer = 0; layer < LAYERS; ++layer) {
        const int attention = m->bound[layer * 15] == 0;
        m->states[2 * layer] = reserve(&ptr, attention ? cache : 4 * GDN_HISTORY);
        m->states[2 * layer + 1] = reserve(&ptr, attention ? cache : 4 * GDN_STATE);
    }
    for (unsigned i = 0; i < 23; ++i) m->b[i] = reserve(&ptr, words[i] * sizeof(float));
    m->b[23] = reserve(&ptr, 4 * GDN_STATE);
    m->positions = reserve(&ptr, 16);
    m->rope_q = reserve(&ptr, 27 * 8);
    m->rope_k = reserve(&ptr, 27 * 8);
    m->attention = reserve(&ptr, 22 * 8);
    m->partial = reserve(&ptr, partial);
    if (check(cuMemAlloc(&m->memory, ptr), "state and scratch") ||
        check(cuMemsetD8(m->memory, 0, ptr), "state initialization")) return -1;
    for (unsigned i = 0; i < LAYERS * 2; ++i) m->states[i] += m->memory;
    for (unsigned i = 0; i < 24; ++i) m->b[i] += m->memory;
    m->positions += m->memory; m->rope_q += m->memory; m->rope_k += m->memory;
    m->attention += m->memory; m->partial += m->memory;
    for (unsigned i = 20; i < 23; ++i) {
        if (check(cuMemcpyHtoD(m->b[i], sign, words[i] * sizeof(*sign)), "Hadamard signs")) return -1;
        sign += words[i];
    }
    if (rope_config(m->rope_q, 24) || rope_config(m->rope_k, 4)) return -1;
    fprintf(stderr, "Device workspace: %llu bytes (Q4_0 K/V, GDN, activations)\n", (unsigned long long)ptr);
    return 0;
}

static int load_weights(model *m, uint8_t *mapping, size_t length) {
    int64_t header[8], kv[8*512], tensors[12*1024], kv_slots[1024], tensor_slots[2048];
    int64_t bound[65*15], draft[4], widths[3] = {5120, 6144, 17408};
    int32_t signs[28672];
    const sl_u8 file = { mapping, length > 67108864 ? 67108864 : (int64_t)length };
    const sl_i64 st = slice(header, 8), meta = slice(kv, 8*512), ten = slice(tensors, 12*1024);
    const sl_i64 ks = slice(kv_slots, 1024), ts = slice(tensor_slots, 2048);
    const int64_t parse = gr_parse(file, (int64_t)length, st, meta, ten, ks, ts);
    if (parse != 0) {
        fprintf(stderr, "GGUF parse: %lld\n", (long long)parse);
        return -1;
    }
    if (bonsai_bind_layout(file, st, meta, ten, ks, ts, slice(m->globals, 3),
                           slice(bound, 65*15), slice(draft, 4)) != LAYERS) return -1;
    int64_t index = key(file, st, meta, ks, "prism.hadamard.sign_widths");
    if (gr_arr_n(st, meta, index) != 3) return -1;
    int64_t value;
    for (int i = 0; i < 3; ++i) {
        if (gr_array_int(file, st, meta, index, i, slice(&value, 1)) || value != widths[i]) return -1;
    }
    index = key(file, st, meta, ks, "prism.hadamard.sign_values");
    if (gr_arr_n(st, meta, index) != 28672) return -1;
    for (unsigned i = 0; i < 28672; ++i) {
        if (gr_array_int(file, st, meta, index, i, slice(&value, 1)) || (value != -1 && value != 1)) return -1;
        signs[i] = (int32_t)value;
    }
    index = key(file, st, meta, ks, "qwen35.attention.layer_norm_rms_epsilon");
    if (gr_meta_f32_bits(file, st, meta, index, slice(&value, 1))) return -1;
    const uint32_t raw_eps = (uint32_t)value;
    memcpy(&m->epsilon, &raw_eps, sizeof(raw_eps));
    if (!isfinite(m->epsilon) || m->epsilon <= 0.0f) return -1;
    if (load_eog(m, file, st, meta, ks)) return -1;
    const size_t bytes = (size_t)header[6];
    if (check(cuMemAlloc(&m->weights, bytes), "resident GGUF weights") ||
        check(cuMemcpyHtoD(m->weights, mapping + header[5], bytes), "resident GGUF upload")) return -1;
    for (unsigned i = 0; i < 3; ++i) m->globals[i] = m->weights + tensors[m->globals[i]*12+8];
    for (unsigned i = 0; i < LAYERS*15; ++i) {
        m->bound[i] = i % 15 == 0 ? bound[i] : (bound[i] < 0 ? 0 : (int64_t)(m->weights + tensors[bound[i]*12+8]));
    }
    fprintf(stderr, "Device GGUF payload: %zu bytes (includes inactive MTP tensors)\n", bytes);
    return workspace(m, signs);
}

static model *bonsai_open(const char *gguf, const char *cubin, const char *frozen, unsigned context) {
    if (context < CONTEXT_MIN || context > POSITIONS_MAX) return NULL;
    model *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->context = context;
    CUdevice device;
    const char *names[KERNEL_COUNT] = {
        "bonsai_norm", "bonsai_fwht1024", "bonsai_pq2_matvec", "bonsai_residual", "bonsai_decay",
        "bonsai_conv_step", "goose_gdn_decode", "bonsai_split_q_gate", "goose_rope_decode",
        "goose_set_q4_rows", "goose_q4_attention_tiles", "bonsai_attention_gate", "bonsai_pq2_row",
        "bonsai_swiglu", "goose_q4_attention_merge",
    };
    if (check(cuInit(0), "driver") || check(cuDeviceGet(&device, 0), "device") ||
        check(cuCtxCreate(&m->cuda, 0, device), "context") ||
        check(cuModuleLoad(&m->module, cubin), "Bonsai CUDA image") ||
        check(cuModuleLoad(&m->frozen, frozen), "frozen Goose CUDA image")) goto error;
    for (unsigned i = 0; i < KERNEL_COUNT; ++i) {
        CUmodule module = strncmp(names[i], "goose_", 6) == 0 ? m->frozen : m->module;
        if (check(cuModuleGetFunction(&m->kernel[i], module, names[i]), names[i])) goto error;
    }
    int fd = open(gguf, O_RDONLY);
    if (fd < 0) { perror(gguf); goto error; }
    struct stat info;
    if (fstat(fd, &info) != 0 || info.st_size < 24) { close(fd); goto error; }
    uint8_t *mapping = mmap(NULL, info.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (mapping == MAP_FAILED) { perror("mmap"); goto error; }
    const int loaded = load_weights(m, mapping, (size_t)info.st_size);
    munmap(mapping, info.st_size);
    if (loaded != 0) goto error;
    size_t available = 0, total = 0;
    if (!check(cuMemGetInfo(&available, &total), "free VRAM")) {
        fprintf(stderr, "Device free: %zu / %zu bytes\n", available, total);
    }
    return m;
error:
    if (m->cuda) cuCtxDestroy(m->cuda);
    free(m);
    return NULL;
}

static int evaluate(model *m, unsigned token, float *logits) {
    if (m->position >= m->context || token >= VOCAB) return -1;
    const int pos = (int)m->position, keys = pos + 1;
    const int64_t tiles = (keys + 255)/256;
    const int32_t positions[4] = {pos, pos, pos, 0};
    const int64_t cfg[22] = {
        256, 256, 1, 24, 4, 4, keys, 24576, 1024, KV_BYTES, 144, KV_BYTES, 144,
        0, 1024, 24576, 256, tiles, 24, 24, 24 * tiles * 258 * 4, keys,
    };
    if (check(cuMemcpyHtoD(m->positions, positions, sizeof(positions)), "position") ||
        check(cuMemcpyHtoD(m->attention, cfg, sizeof(cfg)), "attention configuration")) return -1;
    const int64_t result = bonsai_graph((uintptr_t)m, slice(m->bound, LAYERS*15), slice(m->states, LAYERS*2),
        slice(m->globals, 3), slice(m->b, 24), token, pos, logits != NULL);
    if (result != 0) { fprintf(stderr, "Goose graph failed: %lld\n", (long long)result); return -1; }
    ++m->position;
    return logits ? check(cuMemcpyDtoH(logits, m->b[19], VOCAB * sizeof(float)), "logits") : 0;
}

int main(int argc, char **argv) {
    if (argc < 7) {
        fprintf(stderr, "usage: %s model.gguf bonsai.cubin frozen.ptx context generate [--ignore-eos] input_id ...\n", argv[0]);
        return 2;
    }
    const unsigned context = (unsigned)strtoul(argv[4], NULL, 10);
    const unsigned generate = (unsigned)strtoul(argv[5], NULL, 10);
    const int ignore_eos = strcmp(argv[6], "--ignore-eos") == 0;
    const int first = 6 + ignore_eos;
    if (argc <= first || !generate || (uint64_t)(argc - first) + generate - 1 > context) return 2;
    goose_init(0, NULL);
    model *m = bonsai_open(argv[1], argv[2], argv[3], context);
    if (!m) return 1;
    float *logits = malloc(VOCAB * sizeof(*logits));
    int status = logits ? 0 : 1;
    for (int i = first; i < argc && !status; ++i) {
        char *end;
        const unsigned long token = strtoul(argv[i], &end, 10);
        if (*end || end == argv[i] || token >= VOCAB || evaluate(m, token, i+1 == argc ? logits : NULL)) status = 1;
    }
    for (unsigned i = 0; i < generate && !status; ++i) {
        for (unsigned id = 0; id < VOCAB; ++id) if (!isfinite(logits[id])) status = 1;
        if (status) break;
        if (ignore_eos) {
            for (int64_t j = 0; j < m->eog_count; ++j) logits[m->eog[j]] = -INFINITY;
        }
        unsigned best = 0;
        for (unsigned id = 1; id < VOCAB; ++id) if (logits[id] > logits[best]) best = id;
        int stop = 0;
        for (int64_t j = 0; j < m->eog_count; ++j) if (m->eog[j] == best) stop = 1;
        printf("{\"step\":%u,\"token\":%u,\"logit\":%.9g}\n", i, best, logits[best]);
        if (stop) break;
        if (i+1 < generate && evaluate(m, best, logits)) status = 1;
    }
    free(logits);
    cuCtxDestroy(m->cuda);
    free(m);
    return status;
}
