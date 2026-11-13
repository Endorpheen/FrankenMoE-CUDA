static void persistent_pool_cases(int threads) {
    struct ggml_threadpool_params config = ggml_threadpool_params_default(threads);
    config.poll = 0;
    struct ggml_threadpool *pool = ggml_threadpool_new(&config);
    if (!pool) abort();
    for (int cycle = 0; cycle < 6; ++cycle) {
        begin();
        struct ggml_tensor *x = make(GGML_TYPE_F32, 64, 3, 1, 1);
        struct ggml_tensor *weight = make(GGML_TYPE_F32, 64, 1, 1, 1);
        struct ggml_tensor *gate = make(GGML_TYPE_F32, 64, 3, 1, 1);
        fill(x, cycle%2); fill(weight, 1); fill(gate, 0);
        struct ggml_tensor *norm = ggml_rms_norm(ctx, x, 1e-5f);
        if (cycle == 4) ggml_set_output(norm);
        struct ggml_tensor *scaled = ggml_mul(ctx, norm, weight);
        struct ggml_tensor *t = ggml_swiglu_split(ctx, scaled, gate);
        memset(t->data, 0, ggml_nbytes(t));
        struct ggml_cgraph *graph = ggml_new_graph(ctx);
        ggml_build_forward_expand(graph, t);
        struct ggml_cplan plan = ggml_graph_plan(graph, threads, pool);
        plan.work_data = malloc(plan.work_size);
        if (plan.work_size && !plan.work_data) abort();
        int64_t calls = 0;
        if (cycle == 2) {plan.abort_callback = abort_once; plan.abort_callback_data = &calls;}
        ggml_threadpool_pause(pool);
        ggml_threadpool_resume(pool);
        enum ggml_status status = ggml_graph_compute(graph, &plan);
        if (status != (cycle == 2 ? GGML_STATUS_ABORTED : GGML_STATUS_SUCCESS)) abort();
        int64_t control[2] = {status, calls};
        if (fwrite(control, sizeof(control), 1, out) != 1) abort();
        printf("{\"name\":\"pool-status-cycle%d-t%d\",\"type\":%d,\"elements\":2,\"bytes\":16,\"threads\":%d}\n", cycle, threads, GGML_TYPE_I64, threads);
        size_t bytes = ggml_nbytes(t);
        if (fwrite(t->data, 1, bytes, out) != bytes) abort();
        printf("{\"name\":\"pool-output-cycle%d-t%d\",\"type\":0,\"elements\":%lld,\"bytes\":%zu,\"threads\":%d}\n", cycle, threads, (long long)ggml_nelements(t), bytes, threads);
        fflush(stdout);
        free(plan.work_data);
        end();
    }
    ggml_threadpool_free(pool);
}
