#include "graph-execute.h"

#include "cpu.h"
#include "ggml-impl.h"
#include "goose-host.h"
#define GGML_COMMON_DECL_C
#define GGML_COMMON_IMPL_C
#include "ggml-common.h"

#include <pthread.h>
#include <inttypes.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void fg143_cpu_init(int argc, char ** argv);
extern uint64_t fg_cuda_take_matrices(void);
extern uint64_t fg_cuda_take_f32_matrices(void);
extern void fg_cuda_take_matrix_launches(uint64_t * launches, uint64_t * cached);
extern uint64_t fg_cuda_take_mmq(void);
extern uint64_t fg_cuda_take_dense_mma(void);
extern uint64_t fg_cuda_take_attentions(void);
extern uint64_t fg_cuda_take_causal_attentions(void);
extern uint64_t fg_cuda_take_gdns(void);
extern int fg_cuda_state_graph_done(void);
extern void fg_cuda_take_resident_state(uint64_t * kv, uint64_t * gdn);
extern uint64_t fg_cuda_take_q4_writes(void);
extern uint64_t fg_cuda_take_vectors(void);
extern uint64_t fg_cuda_take_fwhts(void);
extern uint64_t fg_cuda_take_moe_weighted(void);
extern void fg_cuda_take_position(uint64_t * ropes, uint64_t * convs);
extern void fg_cuda_take_routes(uint64_t * routes);
extern void fg_cuda_take_blits(uint64_t * blits);
extern int fg_cuda_activation_graph_begin(void);
extern int fg_cuda_activation_graph_end(bool success, bool all, uint64_t host, int64_t bytes, uint64_t * hits,
                                        uint64_t * skipped_download_bytes);

static pthread_once_t cpu_init = PTHREAD_ONCE_INIT;
static atomic_uint_fast64_t cpu_values;

static void init_cpu(void) {
    fg_goose_host_ensure_init();
    fg143_cpu_init(0, NULL);
}

uint32_t fg143_bits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}

float fg143_from_bits(uint32_t bits) {
    float value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

float fg143_codec_from_bits(uint32_t bits) {
    return fg143_from_bits(bits);
}

float fg143_half(uint16_t bits) {
    return ggml_fp16_to_fp32(bits);
}

float fg141_half(uint16_t bits) {
    return fg143_half(bits);
}

uint16_t fg143_to_half(float value) {
    return ggml_fp32_to_fp16(value);
}

void fg143_witness(int64_t op, int64_t values) {
    (void) op;
    atomic_fetch_add_explicit(&cpu_values, (uint64_t) values, memory_order_relaxed);
}

void fg141_witness(int64_t stage, int64_t kind, int64_t values) {
    (void) stage;
    fg143_witness(kind, values);
}

void fg143_copy_bytes(uint64_t dst, uint64_t src, int64_t length) {
    if (length > 0) memmove((void *) (uintptr_t) dst, (const void *) (uintptr_t) src, (size_t) length);
}

int64_t fg143_storage_count(uint64_t address) {
    const struct ggml_cgraph * graph = (const void *) (uintptr_t) address;
    return (int64_t) graph->n_leafs + graph->n_nodes;
}

uint64_t fg143_storage_tensor(uint64_t address, int64_t index) {
    const struct ggml_cgraph * graph = (const void *) (uintptr_t) address;
    const int64_t leaves = graph->n_leafs;
    return (uint64_t) (uintptr_t) (index < leaves ? graph->leafs[index] : graph->nodes[index - leaves]);
}

int64_t fg143_storage_contains(uint64_t address, uint64_t tensor) {
    const struct ggml_cgraph * graph = (const void *) (uintptr_t) address;
    if (graph->size == 0 || graph->visited_hash_set.size == 0) return -1;
    return ggml_hash_contains(&graph->visited_hash_set, (void *) (uintptr_t) tensor);
}

uint64_t fg143_storage_alignment(void) {
    return GGML_MEM_ALIGN;
}

uint64_t fg143_storage_alloc(uint64_t bytes) {
    if (bytes > SIZE_MAX) return 0;
    return (uint64_t) (uintptr_t) aligned_alloc(GGML_MEM_ALIGN, (size_t) bytes);
}

void * fg_standalone_graph_alloc(struct ggml_cgraph * graph) {
    pthread_once(&cpu_init, init_cpu);
    return (void *) (uintptr_t) fg143_storage_allocate((uint64_t) (uintptr_t) graph);
}

int64_t fg143_storage_hash_size(uint64_t address) {
    const struct ggml_cgraph * graph = (const void *) (uintptr_t) address;
    return graph->size > 0 ? (int64_t) graph->visited_hash_set.size : 0;
}

int64_t fg143_storage_slot(uint64_t address, uint64_t tensor) {
    const struct ggml_cgraph * graph = (const void *) (uintptr_t) address;
    const struct ggml_hash_set * table = &graph->visited_hash_set;
    const size_t slot = ggml_hash_find(table, (void *) (uintptr_t) tensor);
    return slot != GGML_HASHSET_FULL && ggml_bitset_get(table->used, slot) ? (int64_t) slot : -1;
}

uint64_t fg143_storage_ram_available(void) {
    FILE * file = fopen("/proc/meminfo", "r");
    if (!file) return 0;
    uint64_t kib = 0;
    char line[128];
    while (fgets(line, sizeof(line), file)) {
        if (sscanf(line, "MemAvailable: %" SCNu64 " kB", &kib) == 1) break;
    }
    fclose(file);
    return kib <= UINT64_MAX / 1024 ? kib * 1024 : 0;
}

void fg143_storage_ram_error(uint64_t required, uint64_t available) {
    fprintf(stderr, "Goose graph allocation refused: need %" PRIu64 " bytes, RAM available %" PRIu64 " bytes\n",
        required, available);
}

void fg143_storage_report(uint64_t bytes, int64_t blocks, int64_t reused) {
    if (getenv("GOOSE_ARENA_LOG")) {
        fprintf(stderr, "Goose tensor arena: %" PRIu64 " bytes, %" PRId64 " blocks, %" PRId64 " reuses\n",
            bytes, blocks, reused);
    }
}

void * fg_standalone_graph_alloc_output(struct ggml_cgraph * graph, const struct ggml_tensor * output) {
    pthread_once(&cpu_init, init_cpu);
    return (void *) (uintptr_t) fg143_storage_allocate_output((uint64_t) (uintptr_t) graph,
        (uint64_t) (uintptr_t) output);
}

uint64_t fg143_graph_resize(uint64_t address, int64_t bytes) {
    void * work = (void *) (uintptr_t) address;
    if (bytes == 0) {
        free(work);
        return 0;
    }
    if (bytes < 0 || (uint64_t) bytes > SIZE_MAX) return 0;
    return (uint64_t) (uintptr_t) realloc(work, (size_t) bytes);
}

static sl_u8 tensor_slice(const struct ggml_tensor * tensor, int64_t * metadata) {
    if (!tensor) return (sl_u8) {0};
    for (int axis = 0; axis < GGML_MAX_DIMS; ++axis) {
        metadata[axis] = tensor->ne[axis];
        metadata[4 + axis] = (int64_t) tensor->nb[axis];
    }
    metadata[8] = tensor->type;
    metadata[9] = (int64_t) ggml_nbytes(tensor);
    return (sl_u8) { tensor->data, metadata[9] };
}

static bool float_slice(sl_u8 bytes, sl_f32 * out) {
    if (bytes.len < 0 || bytes.len % sizeof(float) || (uintptr_t) bytes.data % _Alignof(float)) {
        return false;
    }
    *out = (sl_f32) { (float *) bytes.data, bytes.len / (int64_t) sizeof(float) };
    return true;
}

static uint8_t run_node(sl_i64 packet, int64_t mode, const sl_u8 inputs[5],
                        sl_f32 scratch, sl_i64 indices) {
    return fg143_node_run(packet, mode, inputs[0], inputs[1], inputs[2], inputs[4], inputs[3], scratch, indices,
        (sl_a8_u8) { (void *) iq2xxs_grid, 256 },
        (sl_a8_u8) { (void *) iq2xs_grid, 512 },
        (sl_a8_u8) { (void *) iq2s_grid, 1024 },
        (sl_a4_u8) { (void *) iq3xxs_grid, 256 },
        (sl_a4_u8) { (void *) iq3s_grid, 512 },
        (sl_a8_i8) { (void *) iq1s_grid, NGRID_IQ1S },
        (sl_u8) { (void *) ksigns_iq2xs, 128 },
        (sl_i8) { (void *) kvalues_iq4nl, 16 });
}

int64_t fg143_graph_nodes(uint64_t graph) {
    return ggml_graph_n_nodes((struct ggml_cgraph *) (uintptr_t) graph);
}

uint64_t fg143_graph_node(uint64_t graph, int64_t index) {
    return (uint64_t) (uintptr_t) ggml_graph_node((struct ggml_cgraph *) (uintptr_t) graph, (int) index);
}

void fg143_graph_inputs(uint64_t address, sl_u64 sources) {
    const struct ggml_tensor * tensor = (const void *) (uintptr_t) address;
    _Static_assert(GGML_MAX_SRC == 10, "Goose graph operand count");
    for (int i = 0; i < GGML_MAX_SRC; ++i) {
        sources.data[i] = (uint64_t) (uintptr_t) tensor->src[i];
    }
}

void fg143_graph_describe(uint64_t address, sl_i64 metadata) {
    const struct ggml_tensor * t = (const void *) (uintptr_t) address;
    int64_t * packet = metadata.data;
    const sl_u8 x = tensor_slice(t->src[0], packet);
    const sl_u8 y = tensor_slice(t->src[1], packet + 10);
    tensor_slice(t->src[2], packet + 20);
    const sl_u8 dst = tensor_slice(t, packet + 30);
    tensor_slice(t->src[3], packet + 59);
    packet[40] = t->op;
    for (int i = 0; i < 16; ++i) packet[41 + i] = t->op_params[i];
    packet[57] = (int64_t) (uintptr_t) x.data;
    packet[58] = (int64_t) (uintptr_t) dst.data;
    packet[69] = t->src[4] != NULL;
    packet[70] = (int64_t) (uintptr_t) y.data;
}

static sl_u8 borrowed_bytes(const struct ggml_tensor * t, int64_t bytes) {
    return (sl_u8) {t ? t->data : NULL, bytes};
}

uint8_t fg143_graph_regular(uint64_t address, sl_i64 packet, int64_t mode, sl_i64 sizes, uint64_t workspace) {
    const struct ggml_tensor * t = (const void *) (uintptr_t) address;
    const sl_u8 inputs[] = {
        borrowed_bytes(t->src[0], packet.data[9]), borrowed_bytes(t->src[1], packet.data[19]),
        borrowed_bytes(t->src[2], packet.data[29]), borrowed_bytes(t, packet.data[39]),
        borrowed_bytes(t->src[3], packet.data[68]),
    };
    if (mode == 1) return run_node(packet, mode, inputs, (sl_f32) {0}, sizes);
    return run_node(packet, mode, inputs, (sl_f32) {(void *) (uintptr_t) workspace, sizes.data[0]},
        (sl_i64) {(void *) (uintptr_t) (workspace + (uint64_t) sizes.data[2]), sizes.data[1]});
}

static sl_u8 state_slices(const struct ggml_tensor * t, int64_t * packet, sl_u8 * inputs, int count) {
    for (int i = 0; i < count; ++i) inputs[i] = tensor_slice(t->src[i], packet + 10 * i);
    packet[80] = t->op;
    packet[81] = t->op_params[0];
    return tensor_slice(t, packet + 70);
}

uint8_t fg143_graph_gdn(uint64_t address) {
    const struct ggml_tensor * t = (const void *) (uintptr_t) address;
    int64_t packet[83] = {0};
    sl_u8 inputs[6];
    const sl_u8 dst = state_slices(t, packet, inputs, 6);
    packet[82] = t->op_params[4];
    sl_f32 args[6], output;
    for (int i = 0; i < 6; ++i) {
        if (!float_slice(inputs[i], &args[i])) return 0;
    }
    if (!float_slice(dst, &output)) return 0;
    return fg143_gdn_run((sl_i64) {packet, 83}, args[0], args[1], args[2], args[3], args[4], args[5], output);
}

uint8_t fg143_graph_scan(uint64_t address) {
    const struct ggml_tensor * t = (const void *) (uintptr_t) address;
    int64_t packet[82] = {0};
    sl_u8 inputs[7];
    const sl_u8 dst = state_slices(t, packet, inputs, 7);
    return fg143_state_run((sl_i64) {packet, 82}, inputs[0], inputs[1], inputs[2], inputs[3],
        inputs[4], inputs[5], inputs[6], dst);
}

static enum ggml_status compute_graph(struct ggml_cgraph * graph, bool all, const struct ggml_tensor * output) {
    pthread_once(&cpu_init, init_cpu);
    if (fg_cuda_activation_graph_begin() != 0) return GGML_STATUS_FAILED;
    const int64_t failed = fg143_graph_run((uint64_t) (uintptr_t) graph, !all);
    if (failed > 0) {
        const struct ggml_tensor * node = ggml_graph_node(graph, (int) (failed - 1));
        fprintf(stderr, "Goose graph: node %lld (%s) failed\n", (long long) failed - 1, ggml_op_name(node->op));
    }
    const uint64_t matrices = fg_cuda_take_matrices();
    const uint64_t f32_matrices = fg_cuda_take_f32_matrices();
    uint64_t matrix_launches, cached_projections;
    fg_cuda_take_matrix_launches(&matrix_launches, &cached_projections);
    const uint64_t mmq = fg_cuda_take_mmq();
    const uint64_t dense_mma = fg_cuda_take_dense_mma();
    const uint64_t attentions = fg_cuda_take_attentions();
    const uint64_t causal = fg_cuda_take_causal_attentions();
    const uint64_t gdns = fg_cuda_take_gdns();
    uint64_t resident_kv, resident_gdn;
    fg_cuda_take_resident_state(&resident_kv, &resident_gdn);
    const uint64_t writes = fg_cuda_take_q4_writes();
    const uint64_t vectors = fg_cuda_take_vectors();
    const uint64_t fwhts = fg_cuda_take_fwhts();
    const uint64_t weighted = fg_cuda_take_moe_weighted();
    uint64_t ropes, convs;
    fg_cuda_take_position(&ropes, &convs);
    uint64_t routes[4];
    fg_cuda_take_routes(routes);
    uint64_t blits[3];
    fg_cuda_take_blits(blits);
    const bool valid = failed == 0 && fg_cuda_state_graph_done() == 0;
    uint64_t hits = 0, skipped_download_bytes = 0;
    const int published = fg_cuda_activation_graph_end(valid, all,
        output ? (uint64_t) (uintptr_t) output->data : 0,
        output ? (int64_t) ggml_nbytes(output) : 0, &hits, &skipped_download_bytes);
    if (getenv("GOOSE_GRAPH_LOG")) {
        const uint64_t values = (uint64_t) atomic_exchange_explicit(&cpu_values, 0, memory_order_relaxed);
        fprintf(stderr, "Goose CPU graph: %llu values\n", (unsigned long long) values);
        if (matrices) fprintf(stderr, "Goose CUDA graph: %llu matrices\n", (unsigned long long) matrices);
        if (f32_matrices) fprintf(stderr, "Goose CUDA F32 matrix: %llu nodes\n", (unsigned long long) f32_matrices);
        if (matrix_launches) fprintf(stderr, "Goose CUDA matrix launches: %llu\n", (unsigned long long) matrix_launches);
        if (cached_projections) fprintf(stderr, "Goose CUDA cached expert projections: %llu\n", (unsigned long long) cached_projections);
        if (mmq) fprintf(stderr, "Goose CUDA MMQ projections: %llu nodes\n", (unsigned long long) mmq);
        if (dense_mma) fprintf(stderr, "Goose CUDA dense MMA: %llu nodes\n", (unsigned long long) dense_mma);
        if (attentions) fprintf(stderr, "Goose CUDA attention: %llu nodes\n", (unsigned long long) attentions);
        if (causal) fprintf(stderr, "Goose CUDA causal attention: %llu nodes\n", (unsigned long long) causal);
        if (gdns) fprintf(stderr, "Goose CUDA GDN: %llu nodes\n", (unsigned long long) gdns);
        if (resident_kv) fprintf(stderr, "Goose CUDA resident KV: %llu nodes\n", (unsigned long long) resident_kv);
        if (resident_gdn) fprintf(stderr, "Goose CUDA resident GDN: %llu nodes\n", (unsigned long long) resident_gdn);
        if (writes) fprintf(stderr, "Goose CUDA resident Q4 writes: %llu nodes\n", (unsigned long long) writes);
        if (vectors) fprintf(stderr, "Goose CUDA vector: %llu nodes\n", (unsigned long long) vectors);
        if (fwhts) fprintf(stderr, "Goose CUDA KV FWHT: %llu nodes\n", (unsigned long long) fwhts);
        if (weighted) fprintf(stderr, "Goose CUDA MoE weighted reduction: %llu tiles\n", (unsigned long long) weighted);
        if (skipped_download_bytes) fprintf(stderr, "Goose CUDA overwritten downloads skipped: %" PRIu64 " bytes\n",
            skipped_download_bytes);
        if (ropes) fprintf(stderr, "Goose CUDA RoPE: %llu nodes\n", (unsigned long long) ropes);
        if (convs) fprintf(stderr, "Goose CUDA SSM convolution: %llu nodes\n", (unsigned long long) convs);
        for (int i = 0; i < 4; ++i) {
            const char * names[] = {"softmax", "sort", "gather", "row sum"};
            if (routes[i]) fprintf(stderr, "Goose CUDA %s: %llu nodes\n", names[i], (unsigned long long) routes[i]);
        }
        const char * blit_names[] = {"concat", "contiguous copy", "resident SSM writes"};
        for (int i = 0; i < 3; ++i) {
            if (blits[i]) fprintf(stderr, "Goose CUDA %s: %llu nodes\n", blit_names[i], (unsigned long long) blits[i]);
        }
        if (hits) fprintf(stderr, "Goose CUDA activation reuse: %llu inputs\n", (unsigned long long) hits);
    }
    return valid && published == 0 ? GGML_STATUS_SUCCESS : GGML_STATUS_FAILED;
}

enum ggml_status fg_standalone_graph_compute(struct ggml_cgraph * graph) {
    return compute_graph(graph, true, NULL);
}

enum ggml_status fg_standalone_graph_compute_output(struct ggml_cgraph * graph, const struct ggml_tensor * output) {
    return compute_graph(graph, false, output);
}
