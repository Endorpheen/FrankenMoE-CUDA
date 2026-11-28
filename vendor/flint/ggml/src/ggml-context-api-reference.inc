#ifdef GOOSEINFER_TYPE_METADATA_ONLY
extern uint64_t fg143_tensor_overhead(void);

size_t ggml_tensor_overhead(void) {
    fg143_cpu_init(0, NULL);
    return fg143_tensor_overhead();
}
#endif

#ifdef GOOSEINFER_TYPE_METADATA_ONLY
extern uint64_t fg143_arena_init(uint64_t capacity, uint64_t buffer, uint8_t no_alloc);
#endif

struct ggml_context * ggml_init(struct ggml_init_params params) {
    static bool is_first_call = true;

    ggml_critical_section_start();

    if (is_first_call) {
#ifdef GOOSEINFER_TYPE_METADATA_ONLY
        fg143_cpu_init(0, NULL);
#else
        ggml_time_init();
#endif

        is_first_call = false;
    }

    ggml_critical_section_end();

#ifdef GOOSEINFER_TYPE_METADATA_ONLY
    return (void *)(uintptr_t)fg143_arena_init(params.mem_size, (uintptr_t)params.mem_buffer, params.no_alloc);
#else
    struct ggml_context * ctx = GGML_MALLOC(sizeof(struct ggml_context));

    // allow to call ggml_init with 0 size
    if (params.mem_size == 0) {
        params.mem_size = GGML_MEM_ALIGN;
    }

    const size_t mem_size = params.mem_buffer ? params.mem_size : GGML_PAD(params.mem_size, GGML_MEM_ALIGN);

    *ctx = (struct ggml_context) {
        /*.mem_size           =*/ mem_size,
        /*.mem_buffer         =*/ params.mem_buffer ? params.mem_buffer : ggml_aligned_malloc(mem_size),
        /*.mem_buffer_owned   =*/ params.mem_buffer ? false : true,
        /*.no_alloc           =*/ params.no_alloc,
        /*.n_objects          =*/ 0,
        /*.objects_begin      =*/ NULL,
        /*.objects_end        =*/ NULL,
    };

    GGML_ASSERT(ctx->mem_buffer != NULL);

    GGML_ASSERT_ALIGNED(ctx->mem_buffer);

    GGML_PRINT_DEBUG("%s: context initialized\n", __func__);

    return ctx;
#endif
}
