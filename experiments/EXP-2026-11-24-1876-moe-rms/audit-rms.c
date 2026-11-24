#include "main.h"
#include "ggml.h"
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

extern int64_t __real_fg143_graph_run(uint64_t, uint8_t, sl_i64, uint8_t);
extern uint8_t fg143_cpu_prefill(void);

static bool dense_matrix(const struct ggml_tensor *tensor, int64_t width, int64_t rows) {
    return tensor && tensor->type == GGML_TYPE_F32 && tensor->ne[0] == width &&
        tensor->ne[1] == rows && tensor->ne[2] == 1 && tensor->ne[3] == 1 &&
        tensor->data && ggml_is_contiguous(tensor);
}

static bool reads(const struct ggml_tensor *node, const struct ggml_tensor *target) {
    if (node->view_src == target) return true;
    for (unsigned i = 0; i < GGML_MAX_SRC; ++i)
        if (node->src[i] == target) return true;
    return false;
}

static bool overlaps(const struct ggml_tensor *left, const struct ggml_tensor *right) {
    const uintptr_t a = (uintptr_t)left->data;
    const uintptr_t b = (uintptr_t)right->data;
    return a <= b ? b - a < ggml_nbytes(left) : a - b < ggml_nbytes(right);
}

static int producer_index(struct ggml_cgraph *graph, const struct ggml_tensor *input, int count) {
    for (int i = 0; i < count; ++i)
        if (ggml_graph_node(graph, i) == input) return i;
    return -1;
}

static void report(struct ggml_cgraph *graph, sl_i64 deadlines) {
    const int count = ggml_graph_n_nodes(graph);
    unsigned broadcasts = 0, same_sources = 0, consecutive = 0, eligible = 0;
    unsigned lifetime = 0, overlap = 0, disjoint = 0, ready = 0, unique = 0, observable = 0;
    unsigned next_rms = 0, rms_dense = 0, shared_expires = 0, norm_disjoint = 0, weighted = 0;
    unsigned exact_alias[4] = {0};
    int64_t tokens = 0;
    for (int i = 0; i < count; ++i) {
        const struct ggml_tensor *mul = ggml_graph_node(graph, i);
        const int64_t rows = mul->ne[1];
        if (mul->op != GGML_OP_MUL || rows < 2 || !dense_matrix(mul, 2048, rows) ||
            !dense_matrix(mul->src[0], 2048, rows) || !dense_matrix(mul->src[1], 1, rows))
            continue;
        ++broadcasts;
        tokens = rows;
        int next = i + 1;
        while (next < count && !reads(ggml_graph_node(graph, next), mul)) ++next;
        if (next >= count) continue;
        const struct ggml_tensor *add = ggml_graph_node(graph, next);
        if (add->op != GGML_OP_ADD || add->src[1] != mul || !dense_matrix(add, 2048, rows) ||
            !dense_matrix(add->src[0], 2048, rows)) continue;
        int last = next + 1;
        while (last < count && !reads(ggml_graph_node(graph, last), add)) ++last;
        if (last >= count) continue;
        const struct ggml_tensor *out = ggml_graph_node(graph, last);
        if (out->op != GGML_OP_ADD || out->src[0] != add || !dense_matrix(out, 2048, rows) ||
            !dense_matrix(out->src[1], 2048, rows)) continue;
        ++same_sources;
        if (next != i + 1 || last != i + 2) continue;
        ++consecutive;
        const struct ggml_tensor *inputs[] = {add->src[0], mul->src[0], mul->src[1], out->src[1]};
        bool safe = true;
        bool direct = true;
        bool aligned = (uintptr_t)out->data % 16 == 0;
        bool input_ready = true;
        for (unsigned input = 0; input < sizeof inputs / sizeof *inputs; ++input) {
            input_ready &= producer_index(graph, inputs[input], count) < i;
            if (input != 2) aligned &= (uintptr_t)inputs[input]->data % 16 == 0;
            if (!overlaps(out, inputs[input])) continue;
            direct = false;
            if (input == 2 || out->data != inputs[input]->data || ggml_nbytes(out) != ggml_nbytes(inputs[input]))
                safe = false;
            else ++exact_alias[input];
        }
        ready += input_ready;
        overlap += safe;
        disjoint += direct;
        const bool deadline_ok = deadlines.len == count && deadlines.data[i] == next && deadlines.data[next] == last;
        lifetime += deadline_ok;
        bool single_consumer = true;
        for (int j = last; j < count; ++j) {
            const struct ggml_tensor *user = ggml_graph_node(graph, j);
            if (reads(user, mul) || (j != last && reads(user, add))) single_consumer = false;
        }
        unique += single_consumer;
        const bool normal = !(mul->flags & GGML_TENSOR_FLAG_OUTPUT) && !(add->flags & GGML_TENSOR_FLAG_OUTPUT) &&
            !mul->view_src && !add->view_src && !out->view_src;
        observable += normal;
        eligible += safe && aligned && input_ready && deadline_ok && single_consumer && normal;
        if (last + 1 >= count) continue;
        const struct ggml_tensor *rms = ggml_graph_node(graph, last + 1);
        if (rms->op != GGML_OP_RMS_NORM || rms->src[0] != out) continue;
        ++next_rms;
        if (rms->view_src || !dense_matrix(rms, 2048, rows) || (uintptr_t)rms->data % 16 ||
            rms->src[2] || (rms->src[1] && !dense_matrix(rms->src[1], 2048, 1))) continue;
        ++rms_dense;
        weighted += rms->src[1] != NULL;
        const int producer = producer_index(graph, mul->src[0], count);
        if (producer >= 0 && deadlines.len == count && deadlines.data[producer] == i) ++shared_expires;
        bool separate = !overlaps(rms, out) && (!rms->src[1] || !overlaps(rms, rms->src[1]));
        for (unsigned input = 0; input < sizeof inputs / sizeof *inputs; ++input)
            separate &= !overlaps(rms, inputs[input]);
        norm_disjoint += separate;

    }
    fprintf(stderr, "Goose epilogue audit tokens=%" PRId64 " broadcasts=%u chains=%u consecutive=%u"
        " ready=%u lifetime=%u unique=%u normal=%u alias_safe=%u disjoint=%u"
        " alias_routed=%u alias_shared=%u alias_gate=%u alias_residual=%u eligible=%u\n",
        tokens, broadcasts, same_sources, consecutive, ready, lifetime, unique, observable, overlap, disjoint,
        exact_alias[0], exact_alias[1], exact_alias[2], exact_alias[3], eligible);
    fprintf(stderr, "Goose MoE+RMS audit tokens=%" PRId64 " adjacent=%u dense=%u weighted=%u shared_last_use=%u disjoint=%u\n",
        tokens, next_rms, rms_dense, weighted, shared_expires, norm_disjoint);

}

int64_t __wrap_fg143_graph_run(uint64_t address, uint8_t selective, sl_i64 deadlines, uint8_t observe) {
    if (address && fg143_cpu_prefill() && selective && !observe)
        report((struct ggml_cgraph *)(uintptr_t)address, deadlines);
    return __real_fg143_graph_run(address, selective, deadlines, observe);
}
