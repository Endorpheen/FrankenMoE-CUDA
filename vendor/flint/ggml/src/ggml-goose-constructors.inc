static_assert(offsetof(struct ggml_tensor, ne) == 16 &&
              offsetof(struct ggml_tensor, nb) == 48 &&
              sizeof(((struct ggml_tensor *)0)->nb) == 32 &&
              offsetof(struct ggml_tensor, view_src) == 232 &&
              offsetof(struct ggml_tensor, view_offs) == 240 &&
              offsetof(struct ggml_tensor, data) == 248 &&
              sizeof(struct ggml_tensor) == 336, "Goose tensor geometry ABI");

void fg143_ctor_caption(int64_t address, int64_t source, int64_t kind) {
    const char *suffix[] = {"reshaped", "view", "permuted", "transposed"};
    const struct ggml_tensor *input = (const void *)(intptr_t)source;
    ggml_format_name((void *)(intptr_t)address, "%s (%s)", input->name, suffix[kind]);
}
