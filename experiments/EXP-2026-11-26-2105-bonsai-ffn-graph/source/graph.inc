static const enum kernel ffn_graph_nodes[] = {
    NORM_WIDE_SMALL, ROTATE, MV_PACK_Q8, MV_Q8_2, MV_Q8_2,
    SWIGLU, ROTATE, MV_PACK_Q8, MV_Q8_2, ADD,
};

static struct {
    __typeof__(cuStreamCreate) *create;
    __typeof__(cuStreamDestroy) *destroy;
    __typeof__(cuStreamBeginCapture) *begin;
    __typeof__(cuStreamEndCapture) *end;
    __typeof__(cuGraphGetNodes) *nodes;
    __typeof__(cuGraphInstantiate) *instantiate;
    __typeof__(cuGraphLaunch) *launch;
    __typeof__(cuGraphDestroy) *graph_destroy;
    __typeof__(cuGraphExecDestroy) *exec_destroy;
    int ready;
} ffn_driver;

static void bind_ffn_graph(void) {
#define LOAD_GRAPH(field, symbol) do { \
    ffn_driver.field = dlsym(bonsai_cuda.library, symbol); \
    if (!ffn_driver.field) return; \
} while (0)
    LOAD_GRAPH(create, "cuStreamCreate");
    LOAD_GRAPH(destroy, "cuStreamDestroy_v2");
    LOAD_GRAPH(begin, "cuStreamBeginCapture_v2");
    LOAD_GRAPH(end, "cuStreamEndCapture");
    LOAD_GRAPH(nodes, "cuGraphGetNodes");
    LOAD_GRAPH(instantiate, "cuGraphInstantiateWithFlags");
    LOAD_GRAPH(launch, "cuGraphLaunch");
    LOAD_GRAPH(graph_destroy, "cuGraphDestroy");
    LOAD_GRAPH(exec_destroy, "cuGraphExecDestroy");
#undef LOAD_GRAPH
    ffn_driver.ready = 1;
}

static void ffn_graph_open(model *m) {
    const char *enabled = getenv("BONSAI_HTTP_FFN_GRAPH");
    if (!enabled || strcmp(enabled, "1") || m->q8_mode != 1 || !m->norm_wide) return;
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    if (pthread_once(&once, bind_ffn_graph) != 0 || !ffn_driver.ready) return;
    if (check(ffn_driver.create(&m->ffn_capture_stream, CU_STREAM_NON_BLOCKING),
              "Bonsai FFN graph stream")) return;
    m->ffn_graph_enabled = 1;
}

static void ffn_graph_abort(model *m) {
    if (m->ffn_graph_phase == 1) {
        CUgraph graph = NULL;
        (void)ffn_driver.end(m->ffn_capture_stream, &graph);
        if (graph) (void)ffn_driver.graph_destroy(graph);
    }
    m->ffn_graph_phase = 0;
    m->ffn_graph_enabled = 0;
}

static void ffn_graph_close(model *m) {
    if (!m->ffn_capture_stream) return;
    CUcontext current;
    if (bonsai_cuda.fn_cuCtxPushCurrent(m->cuda) != CUDA_SUCCESS) return;
    ffn_graph_abort(m);
    for (unsigned i = 0; i < LAYERS; ++i) {
        if (m->ffn_graphs[i]) (void)ffn_driver.exec_destroy(m->ffn_graphs[i]);
    }
    (void)ffn_driver.destroy(m->ffn_capture_stream);
    (void)bonsai_cuda.fn_cuCtxPopCurrent(&current);
}

static int ffn_graph_begin(model *m, CUdeviceptr norm_weight) {
    if (m->ffn_graph_phase || m->ffn_graph_layer >= LAYERS) return -1;
    const unsigned scope = m->ffn_graph_layer;
    m->ffn_graph_next = 0;
    if (m->ffn_graphs[scope]) {
        if (m->ffn_graph_norm[scope] != norm_weight) return -1;
        m->ffn_graph_phase = 2;
    } else {
        const CUresult status = ffn_driver.begin(m->ffn_capture_stream, CU_STREAM_CAPTURE_MODE_THREAD_LOCAL);
        if (status != CUDA_SUCCESS) {
            (void)check(status, "Bonsai FFN graph capture start");
            m->ffn_graph_enabled = 0;
            return 0;
        }
        m->ffn_graph_norm[scope] = norm_weight;
        m->ffn_graph_phase = 1;
    }
    return 0;
}

static int ffn_graph_finish(model *m) {
    if (!m->ffn_graph_phase || m->ffn_graph_next != sizeof(ffn_graph_nodes)/sizeof(*ffn_graph_nodes)) return -1;
    const unsigned scope = m->ffn_graph_layer++;
    if (m->ffn_graph_phase == 1) {
        CUgraph graph = NULL;
        const CUresult captured = ffn_driver.end(m->ffn_capture_stream, &graph);
        m->ffn_graph_phase = 0;
        if (check(captured, "Bonsai FFN graph capture end") || !graph) {
            if (graph) (void)ffn_driver.graph_destroy(graph);
            return -1;
        }
        size_t count = 0;
        const int status = check(ffn_driver.nodes(graph, NULL, &count), "Bonsai FFN graph nodes") ||
            count != sizeof(ffn_graph_nodes)/sizeof(*ffn_graph_nodes) ||
            check(ffn_driver.instantiate(&m->ffn_graphs[scope], graph, 0), "Bonsai FFN graph instantiate");
        (void)ffn_driver.graph_destroy(graph);
        if (status) return -1;
    }
    m->ffn_graph_phase = 0;
    return check(ffn_driver.launch(m->ffn_graphs[scope], NULL), "Bonsai FFN graph launch");
}
