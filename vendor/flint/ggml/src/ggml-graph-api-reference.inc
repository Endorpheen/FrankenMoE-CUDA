#ifdef GOOSEINFER_TYPE_METADATA_ONLY
extern uint64_t fg143_graph_hash_size(uint64_t minimum);
size_t ggml_hash_size(size_t minimum) {
    fg143_cpu_init(0, NULL);
    return fg143_graph_hash_size(minimum);
}
#else
#include "ggml-hash-size-reference.inc"
#endif

#ifdef GOOSEINFER_TYPE_METADATA_ONLY
extern void fg143_graph_expand_forward(uint64_t graph, uint64_t tensor);

void ggml_build_forward_expand(struct ggml_cgraph *graph, struct ggml_tensor *tensor) {
    fg143_graph_expand_forward((uintptr_t)graph, (uintptr_t)tensor);
}
#else
#include "ggml-graph-reference.inc"
#endif
