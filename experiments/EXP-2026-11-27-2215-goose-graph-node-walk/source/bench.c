#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>
#include "ggml-impl.h"
#include "walk.h"

enum { Nodes = 4096, Iterations = 500 };
static struct ggml_tensor tensors[Nodes];
static struct ggml_tensor *nodes[Nodes];
static volatile uint64_t sink;

static double now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}

static double measure(uint64_t (*walk)(uint64_t), const struct ggml_cgraph *graph) {
    const double start = now();
    for (int i = 0; i < Iterations; ++i) sink = walk((uintptr_t)graph);
    return (now() - start) * 1e9 / (Iterations * Nodes);
}

int main(void) {
    goose_init(0, NULL);
    struct ggml_cgraph graph = {.nodes = nodes};
    if (native_walk((uintptr_t)&graph) || goose_walk((uintptr_t)&graph)) return 1;
    graph.n_nodes = Nodes;
    for (int i = 0; i < Nodes; ++i) nodes[i] = &tensors[(i * 41) % Nodes];
    for (int i = -Nodes; i < Nodes; ++i) {
        if (goose_node_at((uintptr_t)&graph, i) != (uintptr_t)ggml_graph_node(&graph, i)) return 2;
    }
    if (native_walk((uintptr_t)&graph) != goose_walk((uintptr_t)&graph)) return 3;
    graph.n_nodes = Nodes - 3;
    if (native_walk((uintptr_t)&graph) != goose_walk((uintptr_t)&graph)) return 4;
    graph.n_nodes = Nodes;
    puts("8192 indexed checks and empty graph matched");
    for (int i = 0; i < 6; ++i) {
        double c, goose;
        if (i % 2) { goose = measure(goose_walk, &graph); c = measure(native_walk, &graph); }
        else { c = measure(native_walk, &graph); goose = measure(goose_walk, &graph); }
        printf("GGML %.2f ns/node, Goose %.2f ns/node\n", c, goose);
    }
}
