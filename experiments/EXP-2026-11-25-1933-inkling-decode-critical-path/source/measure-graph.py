from pathlib import Path

p = Path("inkling-backend.c")
s = p.read_text()

def replace(old, new):
    global s
    assert s.count(old) == 1, old
    s = s.replace(old, new)

replace("static _Thread_local bool time_decode;", """static _Thread_local bool time_decode;
static _Thread_local double graph_build, graph_alloc, graph_exec, graph_finish, context_alloc;
static _Thread_local double graph_cpu_build, graph_cpu_alloc, graph_cpu_exec, graph_cpu_finish, context_cpu_alloc;

static double thread_clock(void) {
    struct timespec ts;
    assert(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0);
    return (double)ts.tv_sec + ts.tv_nsec * 1e-9;
}""")
replace("    struct ggml_cgraph *graph=ggml_new_graph_custom(ctx,256,false);", """    double start = time_decode ? now() : 0.0;
    double cpu = time_decode ? thread_clock() : 0.0;
    struct ggml_cgraph *graph=ggml_new_graph_custom(ctx,256,false);""")
replace("    void *storage = NULL;", """    if (time_decode) {
        graph_build += now() - start;
        graph_cpu_build += thread_clock() - cpu;
        start = now();
        cpu = thread_clock();
    }
    void *storage = NULL;""")
replace("        assert(plan && MEASURE(8, fg_standalone_graph_plan_compute(plan)) == GGML_STATUS_SUCCESS);",
        """        if (time_decode) {
            graph_alloc += now() - start;
            graph_cpu_alloc += thread_clock() - cpu;
            start = now();
            cpu = thread_clock();
        }
        assert(plan && MEASURE(8, fg_standalone_graph_plan_compute(plan)) == GGML_STATUS_SUCCESS);""")
replace("        assert(storage && fg_standalone_graph_compute(graph) == GGML_STATUS_SUCCESS);",
        """        if (time_decode) {
            graph_alloc += now() - start;
            graph_cpu_alloc += thread_clock() - cpu;
            start = now();
            cpu = thread_clock();
        }
        assert(storage && fg_standalone_graph_compute(graph) == GGML_STATUS_SUCCESS);""")
replace("    int at=0;\n    for(int i=0;i<count;i++) {", """    if (time_decode) {
        graph_exec += now() - start;
        graph_cpu_exec += thread_clock() - cpu;
        start = now();
        cpu = thread_clock();
    }
    int at=0;
    for(int i=0;i<count;i++) {""")
replace("    free(storage);\n}", """    free(storage);
    if (time_decode) {
        graph_finish += now() - start;
        graph_cpu_finish += thread_clock() - cpu;
    }
}""")
replace("    struct ggml_context *ctx=ggml_init", """    const double start = time_decode ? now() : 0.0;
    const double cpu = time_decode ? thread_clock() : 0.0;
    struct ggml_context *ctx=ggml_init""")
replace("    assert(ctx);\n    return ctx;\n}", """    assert(ctx);
    if (time_decode) {
        context_alloc += now() - start;
        context_cpu_alloc += thread_clock() - cpu;
    }
    return ctx;
}""")
replace("        decode_attention = decode_bridge = decode_moe = decode_head = 0.0;", """        decode_attention = decode_bridge = decode_moe = decode_head = 0.0;
        graph_build = graph_alloc = graph_exec = graph_finish = context_alloc = 0.0;
        graph_cpu_build = graph_cpu_alloc = graph_cpu_exec = graph_cpu_finish = context_cpu_alloc = 0.0;""")
replace("        time_decode = false;", """        fprintf(stderr, "I2 graph costs step=%d context=%.4f build=%.4f plan=%.4f exec=%.4f finish=%.4f"
                " cpu_context=%.4f cpu_build=%.4f cpu_plan=%.4f cpu_exec=%.4f cpu_finish=%.4f\\n",
                d->position, context_alloc*1000, graph_build*1000, graph_alloc*1000, graph_exec*1000, graph_finish*1000,
                context_cpu_alloc*1000, graph_cpu_build*1000, graph_cpu_alloc*1000, graph_cpu_exec*1000, graph_cpu_finish*1000);
        time_decode = false;""")
p.write_text(s)
