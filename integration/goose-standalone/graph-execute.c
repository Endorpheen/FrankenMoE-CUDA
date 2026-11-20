#include "graph-execute.h"

#include "cpu.h"
#include "ggml-impl.h"
#include "goose-host.h"
#define GGML_COMMON_DECL_C
#define GGML_COMMON_IMPL_C
#include "ggml-common.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void fg143_cpu_init(int argc, char ** argv);
extern uint64_t fg_cuda_take_matrices(void);

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

static bool needs_storage(const struct ggml_tensor * tensor) {
    return tensor->data == NULL && tensor->view_src == NULL;
}

static struct ggml_tensor * graph_tensor(struct ggml_cgraph * graph, size_t index) {
    const size_t leaves = (size_t) graph->n_leafs;
    return index < leaves ? graph->leafs[index] : graph->nodes[index - leaves];
}

void * fg_standalone_graph_alloc(struct ggml_cgraph * graph) {
    if (!graph) return NULL;
    const size_t n = (size_t) graph->n_leafs + (size_t) graph->n_nodes;
    size_t bytes = 0;
    for (size_t i = 0; i < n; ++i) {
        const struct ggml_tensor * tensor = graph_tensor(graph, i);
        if (tensor->view_src) {
            const size_t source_bytes = ggml_nbytes(tensor->view_src);
            if (tensor->view_offs > source_bytes || ggml_nbytes(tensor) > source_bytes - tensor->view_offs) {
                return NULL;
            }
        } else if (needs_storage(tensor)) {
            const size_t size = ggml_nbytes(tensor);
            if (size > SIZE_MAX - bytes - (GGML_MEM_ALIGN - 1)) {
                return NULL;
            }
            bytes += GGML_PAD(size, GGML_MEM_ALIGN);
        }
    }

    // Keep storage independent of the metadata context and any mapped weights.
    uint8_t * storage = aligned_alloc(GGML_MEM_ALIGN, bytes ? bytes : GGML_MEM_ALIGN);
    if (!storage) return NULL;

    size_t at = 0;
    for (size_t i = 0; i < n; ++i) {
        struct ggml_tensor * tensor = graph_tensor(graph, i);
        if (needs_storage(tensor)) {
            tensor->data = storage + at;
            at += GGML_PAD(ggml_nbytes(tensor), GGML_MEM_ALIGN);
        }
    }
    for (size_t i = 0; i < n; ++i) {
        struct ggml_tensor * tensor = graph_tensor(graph, i);
        if (tensor->data == NULL && tensor->view_src != NULL) {
            if (tensor->view_src->data == NULL) {
                free(storage);
                return NULL;
            }
            tensor->data = (uint8_t *) tensor->view_src->data + tensor->view_offs;
        }
    }
    return storage;
}

struct node_workspace {
    float * scratch;
    int64_t * indices;
    int64_t scratch_len;
    int64_t indices_len;
};

static bool reserve_workspace(struct node_workspace * work, const int64_t sizes[4]) {
    if (sizes[0] < 0 || sizes[1] < 0 ||
        (uint64_t) sizes[0] > SIZE_MAX / sizeof(*work->scratch) ||
        (uint64_t) sizes[1] > SIZE_MAX / sizeof(*work->indices)) return false;

    if (sizes[0] > work->scratch_len) {
        float * buffer = realloc(work->scratch, (size_t) sizes[0] * sizeof(*buffer));
        if (!buffer) return false;
        work->scratch = buffer;
        work->scratch_len = sizes[0];
    }
    if (sizes[1] > work->indices_len) {
        int64_t * buffer = realloc(work->indices, (size_t) sizes[1] * sizeof(*buffer));
        if (!buffer) return false;
        work->indices = buffer;
        work->indices_len = sizes[1];
    }
    return true;
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

uint8_t fg143_graph_exec_node(uint64_t graph, uint64_t workspace, int64_t index) {
    struct ggml_tensor * tensor = ggml_graph_node((struct ggml_cgraph *) (uintptr_t) graph, (int) index);
    switch (tensor->op) {
        case GGML_OP_NONE:
        case GGML_OP_RESHAPE:
        case GGML_OP_VIEW:
        case GGML_OP_PERMUTE:
        case GGML_OP_TRANSPOSE:
            return 1;
        default:
            break;
    }
    if (!tensor->data) return 0;
    for (int i = 0; i < GGML_MAX_SRC; ++i) {
        if (tensor->src[i] && !tensor->src[i]->data && !ggml_is_empty(tensor->src[i])) return 0;
    }

    if (tensor->op == GGML_OP_GATED_DELTA_NET || tensor->op == GGML_OP_SSM_SCAN) {
        int64_t packet[82] = {0};
        sl_u8 inputs[7];
        for (int i = 0; i < 7; ++i) inputs[i] = tensor_slice(tensor->src[i], packet + 10 * i);
        const sl_u8 dst = tensor_slice(tensor, packet + 70);
        packet[80] = tensor->op;
        packet[81] = tensor->op_params[0];
        return fg143_state_run((sl_i64) { packet, 82 }, inputs[0], inputs[1], inputs[2],
            inputs[3], inputs[4], inputs[5], inputs[6], dst);
    }

    int64_t packet[71] = {0};
    const sl_u8 inputs[5] = {
        tensor_slice(tensor->src[0], packet), tensor_slice(tensor->src[1], packet + 10),
        tensor_slice(tensor->src[2], packet + 20), tensor_slice(tensor, packet + 30),
        tensor_slice(tensor->src[3], packet + 59),
    };
    packet[40] = tensor->op;
    for (int i = 0; i < 16; ++i) packet[41 + i] = tensor->op_params[i];
    int64_t sizes[4];
    packet[57] = (int64_t) (uintptr_t) inputs[0].data;
    packet[58] = (int64_t) (uintptr_t) inputs[3].data;
    packet[69] = tensor->src[4] != NULL;
    packet[70] = (int64_t) (uintptr_t) inputs[1].data;
    const sl_i64 args = { packet, 71 };
    struct node_workspace * work = (void *) (uintptr_t) workspace;
    if (!run_node(args, 1, inputs, (sl_f32) {0}, (sl_i64) { sizes, 4 }) ||
        !reserve_workspace(work, sizes)) return 0;
    return run_node(args, 0, inputs, (sl_f32) { work->scratch, sizes[0] },
        (sl_i64) { work->indices, sizes[1] });
}

enum ggml_status fg_standalone_graph_compute(struct ggml_cgraph * graph) {
    pthread_once(&cpu_init, init_cpu);
    struct node_workspace work = {0};
    const int64_t failed = fg143_graph_run((uint64_t) (uintptr_t) graph, (uint64_t) (uintptr_t) &work);
    free(work.scratch);
    free(work.indices);
    if (failed > 0) {
        const struct ggml_tensor * node = ggml_graph_node(graph, (int) (failed - 1));
        fprintf(stderr, "Goose graph: node %lld (%s) failed\n", (long long) failed - 1, ggml_op_name(node->op));
    }
    const uint64_t matrices = fg_cuda_take_matrices();
    if (getenv("GOOSE_GRAPH_LOG")) {
        const uint64_t values = (uint64_t) atomic_exchange_explicit(&cpu_values, 0, memory_order_relaxed);
        fprintf(stderr, "Goose CPU graph: %llu values\n", (unsigned long long) values);
        if (matrices) fprintf(stderr, "Goose CUDA graph: %llu matrices\n", (unsigned long long) matrices);
    }
    return failed == 0 ? GGML_STATUS_SUCCESS : GGML_STATUS_FAILED;
}
