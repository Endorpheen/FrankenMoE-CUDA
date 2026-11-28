extern uint64_t fg143_graph_overhead_custom(uint64_t size, uint8_t grads);

size_t ggml_graph_overhead_custom(size_t size, bool grads) {
    fg143_cpu_init(0, NULL);
    return fg143_graph_overhead_custom(size, grads);
}
