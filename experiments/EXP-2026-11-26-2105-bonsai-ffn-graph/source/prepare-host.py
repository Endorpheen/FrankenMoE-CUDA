"""Insert guarded per-layer capture into the existing Bonsai launch routing."""

from pathlib import Path
import sys


def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f"Expected one host anchor: {old!r}")
    return text.replace(old, new)


backend, model_host, out = map(Path, sys.argv[1:])
native = backend.read_text()
native = replace(native, '    int64_t eog[64], eog_count;', '''    int64_t eog[64], eog_count;
    CUstream ffn_capture_stream;
    CUgraphExec ffn_graphs[LAYERS];
    CUdeviceptr ffn_graph_norm[LAYERS];
    unsigned ffn_graph_phase, ffn_graph_layer, ffn_graph_next;
    int ffn_graph_enabled;''')
native = replace(native, 'static int launch(model *m, enum kernel id,', '''#include "graph.inc"

static int launch(model *m, enum kernel id,''')
native = replace(native, '''    const unsigned shared = id == FLASH_KV_PANEL ? FLASH_PANEL_SHARED : 0;
    return check(bonsai_cuda.fn_cuLaunchKernel(m->kernel[id], x, y, z, threads, 1, 1, shared, NULL, args, NULL), "kernel launch");''',
                         '''    CUstream stream = NULL;
    if (m->ffn_graph_phase) {
        if (m->ffn_graph_next >= sizeof(ffn_graph_nodes)/sizeof(*ffn_graph_nodes) ||
            ffn_graph_nodes[m->ffn_graph_next++] != id) return -1;
        if (m->ffn_graph_phase == 2) return 0;
        stream = m->ffn_capture_stream;
    }
    const unsigned shared = id == FLASH_KV_PANEL ? FLASH_PANEL_SHARED : 0;
    return check(bonsai_cuda.fn_cuLaunchKernel(m->kernel[id], x, y, z, threads, 1, 1, shared, stream, args, NULL), "kernel launch");''')
native = replace(native, '''        case NORM:
            if (m->norm_wide''',
                         '''        case NORM:
            if (m->ffn_graph_enabled && m->in_decode && count == 1 &&
                    p[0] == (CUdeviceptr)m->b[13] && p[3] == (CUdeviceptr)m->b[1] &&
                    ffn_graph_begin(m, p[1])) return -1;
            if (m->norm_wide''')
native = replace(native, '''        case ADD:
            return launch(m, ADD, 20 * count, 1, 1, 256, (void *[]){ &p[0], &p[1], &p[2] });''',
                         '''        case ADD: {
            const int status = launch(m, ADD, 20 * count, 1, 1, 256,
                (void *[]){ &p[0], &p[1], &p[2] });
            if (status || !m->ffn_graph_phase) return status;
            if (p[0] != (CUdeviceptr)m->b[13] || p[1] != (CUdeviceptr)m->b[18] ||
                    p[2] != (CUdeviceptr)m->b[0]) return -1;
            return ffn_graph_finish(m);
        }''')
native = replace(native, '''    size_t available = 0, total = 0;''',
                         '''    ffn_graph_open(m);
    size_t available = 0, total = 0;''')
native = replace(native, '''    const int64_t result = bonsai_graph((uintptr_t)m,''',
                         '''    m->ffn_graph_layer = 0;
    const int64_t result = bonsai_graph((uintptr_t)m,''')
native = replace(native, '''    if (result != 0) { fprintf(stderr, "Goose graph failed: %lld\\n", (long long)result); return -1; }''',
                         '''    if (result != 0 || m->ffn_graph_phase ||
            (m->ffn_graph_enabled && m->in_decode && count == 1 && m->ffn_graph_layer != LAYERS)) {
        if (m->ffn_capture_stream) ffn_graph_abort(m);
        fprintf(stderr, "Goose graph failed: %lld\\n", (long long)result);
        return -1;
    }''')
native = replace(native, '''    (void) check(bonsai_cuda.fn_cuCtxDestroy(session->runtime->cuda), "destroy Bonsai context");''',
                         '''    ffn_graph_close(session->runtime);
    (void) check(bonsai_cuda.fn_cuCtxDestroy(session->runtime->cuda), "destroy Bonsai context");''')
(out / "backend.c").write_text(native)
(out / "model-host.c").write_bytes(model_host.read_bytes())
