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
    if (check(ffn_driver.create(&m->capture.stream, CU_STREAM_NON_BLOCKING),
              "Bonsai FFN graph stream")) return;
    m->capture.enabled = 1;
}

_Static_assert(offsetof(struct bonsai_ffn_capture, stream) == 0 &&
    offsetof(struct bonsai_ffn_capture, graphs) == 8 && offsetof(struct bonsai_ffn_capture, norm) == 520 &&
    offsetof(struct bonsai_ffn_capture, phase) == 1032 && offsetof(struct bonsai_ffn_capture, layer) == 1036 &&
    offsetof(struct bonsai_ffn_capture, next) == 1040 && offsetof(struct bonsai_ffn_capture, enabled) == 1044 &&
    LAYERS == 64, "Bonsai FFN capture ABI");

extern void bonsai_capture_abort(uint64_t capture);
extern int32_t bonsai_capture_begin(uint64_t capture, uint64_t norm);
extern int32_t bonsai_capture_finish(uint64_t capture);

int32_t bonsai_capture_driver(uint32_t op, uint64_t first, uint64_t second) {
    switch (op) {
    case 0: return check(ffn_driver.begin((CUstream)(uintptr_t)first, CU_STREAM_CAPTURE_MODE_THREAD_LOCAL),
                         "Bonsai FFN graph capture start");
    case 1: return check(ffn_driver.end((CUstream)(uintptr_t)first, (CUgraph *)(uintptr_t)second),
                         "Bonsai FFN graph capture end");
    case 2: return check(ffn_driver.nodes((CUgraph)(uintptr_t)first, NULL, (size_t *)(uintptr_t)second),
                         "Bonsai FFN graph nodes");
    case 3: return check(ffn_driver.instantiate((CUgraphExec *)(uintptr_t)second, (CUgraph)(uintptr_t)first, 0),
                         "Bonsai FFN graph instantiate");
    case 4: return ffn_driver.graph_destroy((CUgraph)(uintptr_t)first);
    case 5: return check(ffn_driver.launch((CUgraphExec)(uintptr_t)first, NULL), "Bonsai FFN graph launch");
    case 6: return ffn_driver.end((CUstream)(uintptr_t)first, (CUgraph *)(uintptr_t)second);
    default: return -1;
    }
}

static void ffn_graph_abort(model *m) { bonsai_capture_abort((uintptr_t)&m->capture); }

static void ffn_graph_close(model *m) {
    if (!m->capture.stream) return;
    CUcontext current;
    if (bonsai_cuda.fn_cuCtxPushCurrent(m->cuda) != CUDA_SUCCESS) return;
    ffn_graph_abort(m);
    for (unsigned i = 0; i < LAYERS; ++i) {
        if (m->capture.graphs[i]) (void)ffn_driver.exec_destroy(m->capture.graphs[i]);
    }
    (void)ffn_driver.destroy(m->capture.stream);
    (void)bonsai_cuda.fn_cuCtxPopCurrent(&current);
}

static int ffn_graph_begin(model *m, CUdeviceptr norm_weight) {
    return bonsai_capture_begin((uintptr_t)&m->capture, norm_weight);
}

static int ffn_graph_finish(model *m) {
    return bonsai_capture_finish((uintptr_t)&m->capture);
}
