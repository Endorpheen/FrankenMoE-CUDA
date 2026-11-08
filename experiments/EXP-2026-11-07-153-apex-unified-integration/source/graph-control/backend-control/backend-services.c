// APX016/015 backend-control raw services.
// ABI: inputs/ggml-backend-impl.h:50-225, inputs/ggml.h tensor fields.
// This file has no branch/loop/lifetime policy: no tensor-copy algorithm,
// no graph-copy/compare, no scheduler or buffer lifecycle. Scheduler and
// buffer hooks remain parent-owned.

#include "backend-bridge.h"
#include "ggml.h"
#include "ggml-impl.h"
#include "ggml-backend-impl.h"

#include <stdio.h>
#include <stdlib.h>

static struct ggml_backend * as_backend(int64_t p) { return (struct ggml_backend *) (intptr_t) p; }
static struct ggml_backend_device * as_device(int64_t p) { return (struct ggml_backend_device *) (intptr_t) p; }
static struct ggml_backend_buffer * as_buffer(int64_t p) { return (struct ggml_backend_buffer *) (intptr_t) p; }
static struct ggml_tensor * as_tensor(int64_t p) { return (struct ggml_tensor *) (intptr_t) p; }
static struct ggml_backend_event * as_event(int64_t p) { return (struct ggml_backend_event *) (intptr_t) p; }

void fg153_bc_fail(int64_t code) {
    fprintf(stderr, "fg153 backend-control failure: code=%lld\n", (long long) code);
    abort();
}

int64_t fg153_bc_backend_device(int64_t backend) {
    struct ggml_backend * b = as_backend(backend);
    return b ? (int64_t) (intptr_t) b->device : 0;
}

int64_t fg153_bc_backend_has(int64_t backend, int64_t code) {
    struct ggml_backend * b = as_backend(backend);
    if (!b) {
        return 0;
    }
    switch (code) {
        case 2: return b->iface.set_tensor_async != NULL;
        case 3: return b->iface.get_tensor_async != NULL;
        case 9: return b->iface.graph_compute != NULL;
        case 10: return b->iface.synchronize != NULL;
        case 12: return b->iface.event_record != NULL;
        case 13: return b->iface.event_wait != NULL;
        case 14: return b->iface.graph_optimize != NULL;
        default: return 0;
    }
}

int64_t fg153_bc_backend_call(int64_t backend, int64_t code,
                              int64_t a, int64_t b, int64_t c, int64_t d, int64_t e) {
    struct ggml_backend * be = as_backend(backend);
    if (!be) {
        return 0;
    }
    switch (code) {
        case 2:
            be->iface.set_tensor_async(be, as_tensor(a), (const void *) (intptr_t) b, (size_t) c, (size_t) d);
            return 0;
        case 3:
            be->iface.get_tensor_async(be, as_tensor(a), (void *) (intptr_t) b, (size_t) c, (size_t) d);
            return 0;
        case 9:
            return (int64_t) be->iface.graph_compute(be, (struct ggml_cgraph *) (intptr_t) a);
        case 10:
            be->iface.synchronize(be);
            return 0;
        case 12:
            be->iface.event_record(be, as_event(a));
            return 0;
        case 13:
            be->iface.event_wait(be, as_event(a));
            return 0;
        case 14:
            be->iface.graph_optimize(be, (struct ggml_cgraph *) (intptr_t) a,
                                     (struct ggml_backend_graph_optimize_params *) (intptr_t) b);
            return 0;
        default:
            (void) e;
            return 0;
    }
}

int64_t fg153_bc_backend_name(int64_t backend) {
    struct ggml_backend * b = as_backend(backend);
    if (!b) {
        return (int64_t) (intptr_t) "NULL";
    }
    return (int64_t) (intptr_t) b->iface.get_name(b);
}

int64_t fg153_bc_dev_has(int64_t device, int64_t code) {
    struct ggml_backend_device * d = as_device(device);
    if (!d) {
        return 0;
    }
    switch (code) {
        case 0: return d->iface.supports_op != NULL;
        case 1: return d->iface.offload_op != NULL;
        case 2: return d->iface.event_new != NULL;
        case 3: return d->iface.event_free != NULL;
        case 4: return d->iface.event_synchronize != NULL;
        case 5: return d->iface.supports_buft != NULL;
        default: return 0;
    }
}

int64_t fg153_bc_dev_call(int64_t device, int64_t code,
                          int64_t a, int64_t b, int64_t c, int64_t d, int64_t e) {
    struct ggml_backend_device * dev = as_device(device);
    if (!dev) {
        return 0;
    }
    switch (code) {
        case 0: return dev->iface.supports_op(dev, as_tensor(a)) ? 1 : 0;
        case 1: return dev->iface.offload_op(dev, as_tensor(a)) ? 1 : 0;
        case 2: return (int64_t) (intptr_t) dev->iface.event_new(dev);
        case 3: dev->iface.event_free(dev, as_event(a)); return 0;
        case 4: dev->iface.event_synchronize(dev, as_event(a)); return 0;
        case 5: return dev->iface.supports_buft(dev, (ggml_backend_buffer_type_t) (intptr_t) a) ? 1 : 0;
        default: (void) b; (void) c; (void) d; (void) e; return 0;
    }
}

int64_t fg153_bc_buffer_has(int64_t buffer, int64_t code) {
    struct ggml_backend_buffer * buf = as_buffer(buffer);
    if (!buf) {
        return 0;
    }
    switch (code) {
        case 0: return buf->iface.cpy_tensor != NULL;
        case 1: return buf->iface.set_tensor != NULL;
        case 2: return buf->iface.get_tensor != NULL;
        case 5: return buf->iface.memset_tensor != NULL;
        default: return 0;
    }
}

int64_t fg153_bc_buffer_call(int64_t buffer, int64_t code,
                             int64_t a, int64_t b, int64_t c, int64_t d, int64_t e) {
    struct ggml_backend_buffer * buf = as_buffer(buffer);
    if (!buf) {
        return 0;
    }
    switch (code) {
        case 0:
            return buf->iface.cpy_tensor(buf, as_tensor(a), as_tensor(b)) ? 1 : 0;
        case 1:
            buf->iface.set_tensor(buf, as_tensor(a), (const void *) (intptr_t) b, (size_t) c, (size_t) d);
            return 0;
        case 2:
            buf->iface.get_tensor(buf, as_tensor(a), (void *) (intptr_t) b, (size_t) c, (size_t) d);
            return 0;
        case 5:
            buf->iface.memset_tensor(buf, as_tensor(a), (uint8_t) b, (size_t) c, (size_t) d);
            return 0;
        default:
            (void) e;
            return 0;
    }
}

int64_t fg153_bc_tensor_get(int64_t tp, int64_t code, int64_t i) {
    struct ggml_tensor * t = as_tensor(tp);
    if (!t) {
        return 0;
    }
    switch (code) {
        case 0: return (int64_t) (intptr_t) t->data;
        case 1: return (int64_t) (intptr_t) t->buffer;
        case 2: return (int64_t) (intptr_t) t->view_src;
        case 3: return (int64_t) t->view_offs;
        case 4: return t->op;
        case 5: return t->flags;
        case 6: return (int64_t) (intptr_t) t->name;
        case 7: return (i >= 0 && i < GGML_MAX_SRC) ? (int64_t) (intptr_t) t->src[i] : 0;
        case 8: return (int64_t) (intptr_t) t->op_params;
        case 9: return t->type;
        default: return 0;
    }
}

int64_t fg153_bc_tensor_buffer(int64_t tp) {
    struct ggml_tensor * t = as_tensor(tp);
    if (!t) {
        return 0;
    }
    struct ggml_backend_buffer * buf = t->view_src ? t->view_src->buffer : t->buffer;
    return (int64_t) (intptr_t) buf;
}

int64_t fg153_bc_tensor_nbytes(int64_t tp) {
    struct ggml_tensor * t = as_tensor(tp);
    return t ? (int64_t) ggml_nbytes(t) : 0;
}

int64_t fg153_bc_same_layout(int64_t a, int64_t b) {
    return ggml_are_same_layout(as_tensor(a), as_tensor(b)) ? 1 : 0;
}

int64_t fg153_bc_buffer_is_host(int64_t buffer) {
    return ggml_backend_buffer_is_host(as_buffer(buffer)) ? 1 : 0;
}

int64_t fg153_bc_event_get(int64_t event, int64_t code) {
    struct ggml_backend_event * ev = as_event(event);
    if (!ev) {
        return 0;
    }
    if (code == 0) {
        return (int64_t) (intptr_t) ev->device;
    }
    return 0;
}

int64_t fg153_bc_alloc(int64_t bytes) {
    if (bytes <= 0) {
        return 0;
    }
    return (int64_t) (intptr_t) malloc((size_t) bytes);
}

void fg153_bc_free_mem(int64_t ptr) {
    free((void *) (intptr_t) ptr);
}
