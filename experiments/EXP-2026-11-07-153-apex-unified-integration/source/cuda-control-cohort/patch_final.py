#!/usr/bin/env python3
"""worker-14/APX025 hash-bound patch for the active CUDA control path."""
import argparse, hashlib, json, sys

GGML_CUDA_SHA = "92aeb2d95f6f12e7a56c55ddba2a14d07a9fcb8accfe35571024f5f1f1a60350"
COMMON_SHA = "c593d21286a4b4845039408e225c4e61accafcd6053aee1dae918d3f8396b216"
VERSION = "fg14-apx025-1"

GGML_REPL = {
"static bool ggml_cuda_can_fuse(const struct ggml_cgraph *                cgraph,\n                               int                                       node_idx,": """
#ifdef FG14_NATIVE_ROLLBACK
{original}
#else
static bool ggml_cuda_can_fuse(const struct ggml_cgraph *                cgraph,
                               int                                       node_idx,
                               std::initializer_list<enum ggml_op>       ops,
                               std::initializer_list<enum ggml_unary_op> unary_ops) {
    int64_t oarr[32];
    int64_t uarr[32];
    int n = 0;
    int nu = 0;
    for (enum ggml_op op : ops) { oarr[n++] = (int64_t) op; }
    for (enum ggml_unary_op u : unary_ops) { uarr[nu++] = (int64_t) u; }
    return fg14_can_fuse((int64_t) (intptr_t) cgraph, node_idx,
                         (int64_t) (intptr_t) oarr, n,
                         (int64_t) (intptr_t) uarr, nu) != 0;
}
#endif
""",
"static int ggml_cuda_try_fuse(ggml_backend_cuda_context * cuda_ctx, ggml_cgraph * cgraph, int i)": """
#ifdef FG14_NATIVE_ROLLBACK
{original}
#else
static int ggml_cuda_try_fuse(ggml_backend_cuda_context * cuda_ctx, ggml_cgraph * cgraph, int i) {
    return (int) fg14_try_fuse(cuda_ctx->fg14_rec, (int64_t) (intptr_t) cgraph, (int64_t) i);
}
#endif
""",
"static void ggml_backend_cuda_graph_optimize(ggml_backend_t backend, ggml_cgraph * cgraph, ggml_backend_graph_optimize_params * params)": """
#ifdef FG14_NATIVE_ROLLBACK
{original}
#else
static void ggml_backend_cuda_graph_optimize(ggml_backend_t backend, ggml_cgraph * cgraph, ggml_backend_graph_optimize_params * params) {
    (void) fg14_graph_optimize((int64_t) (intptr_t) backend->context,
                               (int64_t) (intptr_t) cgraph,
                               (int64_t) (intptr_t) params);
}
#endif
""",
"static enum ggml_status ggml_backend_cuda_graph_compute(ggml_backend_t backend, ggml_cgraph * cgraph)": """
#ifdef FG14_NATIVE_ROLLBACK
{original}
#else
static enum ggml_status ggml_backend_cuda_graph_compute(ggml_backend_t backend, ggml_cgraph * cgraph) {
    (void) fg14_goose_launch(0, (int64_t) (intptr_t) backend->context,
                             (int64_t) (intptr_t) cgraph, 0, 0, 0, 0);
    return GGML_STATUS_SUCCESS;
}
#endif
""",
}

COMMON_REPL = {
"explicit ggml_backend_cuda_context(int device) :": """
#ifdef FG14_NATIVE_ROLLBACK
{original}
#else
    explicit ggml_backend_cuda_context(int device) :
        device(device),
        name(GGML_CUDA_NAME + std::to_string(device)) {
        fg14_rec = fg14_res_ctx_attach((int64_t) (intptr_t) this, (int64_t) device);
    }
#endif
""",
"cudaStream_t stream(int device, int stream)": """
#ifdef FG14_NATIVE_ROLLBACK
{original}
#else
    cudaStream_t stream(int device, int stream) {
        return (cudaStream_t) (intptr_t) fg14_res_stream(fg14_rec, (int64_t) device, (int64_t) stream);
    }
#endif
""",
"cublasHandle_t cublas_handle()": """
#ifdef FG14_NATIVE_ROLLBACK
{original}
#else
    cublasHandle_t cublas_handle() {
        return (cublasHandle_t) (intptr_t) fg14_res_cublas(fg14_rec, (int64_t) device, (int64_t) curr_stream_no);
    }
#endif
""",
"ggml_cuda_pool & pool(int device)": """
#ifdef FG14_NATIVE_ROLLBACK
{original}
#else
    ggml_cuda_pool & pool(int device) {
        return *(ggml_cuda_pool *) (intptr_t) fg14_res_pool(fg14_rec, (int64_t) device, (int64_t) curr_stream_no);
    }
#endif
""",
"bool any_cuda_graph_enabled() const": """
#ifdef FG14_NATIVE_ROLLBACK
{original}
#else
    bool any_cuda_graph_enabled() const {
        return fg14_res_graph_any_enabled(fg14_rec) != 0;
    }
#endif
""",
"bool any_cuda_graph_has_instance() const": """
#ifdef FG14_NATIVE_ROLLBACK
{original}
#else
    bool any_cuda_graph_has_instance() const {
        return fg14_res_graph_any_instance(fg14_rec) != 0;
    }
#endif
""",
}


def skip_string(s, i):
    q = s[i]; i += 1
    while i < len(s):
        if s[i] == '\\': i += 2; continue
        if s[i] == q: return i + 1
        i += 1
    raise ValueError('unterminated string')


def find_body(s, start):
    i = s.find('{', start)
    depth = 0
    while i < len(s):
        c = s[i]
        if c == '/' and i+1 < len(s) and s[i+1] == '/':
            j = s.find('\n', i+2); i = len(s) if j < 0 else j+1; continue
        if c == '/' and i+1 < len(s) and s[i+1] == '*':
            j = s.find('*/', i+2); i = j+2; continue
        if c in ('"', "'"): i = skip_string(s, i); continue
        if c == '{': depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0: return i+1
        i += 1
    raise ValueError('unterminated body')


def apply(text, anchors, insert, dtor=False):
    missing = []
    for anchor, tmpl in anchors.items():
        i = text.find(anchor)
        if i < 0: missing.append(anchor); continue
        if text.find(anchor, i+1) >= 0: raise SystemExit('ambiguous anchor: '+anchor)
        end = find_body(text, i)
        text = text[:i] + tmpl.replace('{original}', text[i:end]) + text[end:]
    if missing: raise SystemExit('missing anchors: '+json.dumps(missing, indent=1))
    if insert is not None:
        marker, payload = insert
        if marker not in text: raise SystemExit('insertion marker missing: '+marker)
        text = text.replace(marker, marker + payload, 1)
    if dtor:
        anchor = "ggml_backend_cuda_context::~ggml_backend_cuda_context()"
        i = text.find(anchor)
        if i < 0: raise SystemExit('destructor anchor missing')
        end = find_body(text, i)
        body = text[i:end]
        if body.rstrip().endswith('}'):
            body = body.rstrip()[:-1] + "    if (fg14_rec) { (void) fg14_res_ctx_free(fg14_rec); }\n}\n"
        text = text[:i] + "#ifdef FG14_NATIVE_ROLLBACK\n" + text[i:end] + "\n#else\n" + body + "#endif\n" + text[end:]
    return text


def main():
    a = argparse.ArgumentParser()
    a.add_argument('--ggml-cuda', required=True)
    a.add_argument('--common', required=True)
    a.add_argument('--out-cuda', required=True)
    a.add_argument('--out-common', required=True)
    a.add_argument('--stats', required=True)
    a.add_argument('--native-rollback', action='store_true')
    args = a.parse_args()
    raw_cuda = open(args.ggml_cuda, 'rb').read()
    raw_common = open(args.common, 'rb').read()
    h1 = hashlib.sha256(raw_cuda).hexdigest()
    h2 = hashlib.sha256(raw_common).hexdigest()
    if h1 != GGML_CUDA_SHA or h2 != COMMON_SHA:
        raise SystemExit('source hash mismatch: %s %s' % (h1, h2))
    if args.native_rollback:
        open(args.out_cuda, 'wb').write(raw_cuda)
        open(args.out_common, 'wb').write(raw_common)
        stats = {'status': 'native-rollback', 'ggml_cuda_sha256': h1, 'common_sha256': h2}
    else:
        cuda = raw_cuda.decode()
        common = raw_common.decode()
        cuda = apply(cuda, GGML_REPL,
                     ('#include <vector>\n', '#include "cuda-resource-schema.h"\n#include "fg14_abi.h"\n'),
                     dtor=True)
        common = apply(common, COMMON_REPL,
                       ('#include <utility>\n', '#include "cuda-resource-schema.h"\n#include "fg14_abi.h"\n'))
        # add canonical record field to the context
        tail = "    ggml_cuda_pool & pool() {\n        return pool(device);\n    }\n};"
        if tail not in common: raise SystemExit('context tail marker missing')
        common = common.replace(tail, "    ggml_cuda_pool & pool() {\n        return pool(device);\n    }\n\n    int64_t fg14_rec = 0;\n};", 1)
        if not args.native_rollback:
            cuda = cuda.rstrip('\n') + '\n\n#include "fg14_services_real.cpp"\n'
        open(args.out_cuda, 'w').write(cuda)
        open(args.out_common, 'w').write(common)
        stats = {'status': 'patched', 'patch_version': VERSION,
                 'ggml_cuda_sha256': h1, 'common_sha256': h2,
                 'rewritten_functions': 5}
    open(args.stats, 'w').write(json.dumps(stats, indent=2) + '\n')
    print(json.dumps(stats))


if __name__ == '__main__':
    main()
