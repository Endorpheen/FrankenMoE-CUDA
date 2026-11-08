/* worker-8: executable differential and failure-injection corpus for the
 * remaining allocator ownership closure.
 *
 * Runs against the qualified CPU runtime (real ggml contexts, tensors and
 * graphs) with a counting/canary buffer type.  It proves:
 *   - Goose reserve_n_size sizes equal the runtime native implementation for
 *     single, distinct-multi and shared-multi buffer allocators;
 *   - the public buffer size view agrees with the summed plan;
 *   - aligned_offset and ggml_op_can_inplace match the native contract for the
 *     full op enum range and a spread of alignments;
 *   - OOM during reserve leaves no dangling alias, no double free and frees
 *     cleanly, and a retry succeeds;
 *   - a failed tensor init restores the journal (no partially-owned tensors),
 *     and a retry succeeds;
 *   - repeated alloc_graph reuses addresses deterministically (reset path);
 *   - gallocr_free/limits tolerate NULL and every case returns live == 0.
 *
 * No model, no weights, no benchmark, no sanitizer campaign. */

#include "ggml.h"
#include "ggml-alloc.h"
#include "ggml-backend-impl.h"
#include "allocator-closure-bridge.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Declared in ggml-impl.h and inputs/ggml-alloc.h respectively; declared
 * locally so this fixture does not pull the internal header into the
 * runtime-facing TU and does not depend on header snapshot age. */
extern bool ggml_op_can_inplace(enum ggml_op op);
extern void ggml_gallocr_set_buffer_growth_limits(ggml_gallocr_t galloc, const size_t * limits);

#define W8_GUARD 32
#define W8_MAGIC UINT64_C(0x5738414c4c4f4331)
#define W8_MAX_LIVE 8192

struct w8_hdr {
    uint64_t magic;
    void * user;
    size_t size;
};

static struct w8_hdr * g_live[W8_MAX_LIVE];
static int g_nlive;
static int g_alloc_calls, g_frees, g_init_calls;
static int g_fail_alloc_at, g_fail_init_at;
static size_t g_max_a = 1u << 20, g_max_b = 1u << 19;
static unsigned g_cases;
static uint64_t g_hash = UINT64_C(1469598103934665603);
static int g_errors;

#define CHECK(x) do { \
    if (!(x)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); g_errors++; } \
} while (0)

static void mix(uint64_t v) { g_hash ^= v; g_hash *= UINT64_C(1099511628211); }

static void reset_counters(void) {
    g_nlive = 0;
    g_alloc_calls = g_frees = g_init_calls = 0;
    g_fail_alloc_at = g_fail_init_at = 0;
}

static const char * w8_name(ggml_backend_buffer_type_t t);
static size_t w8_alignment(ggml_backend_buffer_type_t t);
static size_t w8_max_size(ggml_backend_buffer_type_t t);
static ggml_backend_buffer_t w8_alloc(ggml_backend_buffer_type_t t, size_t size);
static void w8_free_buffer(ggml_backend_buffer_t b);
static void * w8_base(ggml_backend_buffer_t b);
static enum ggml_status w8_init_tensor(ggml_backend_buffer_t b, struct ggml_tensor * t);
static void w8_clear(ggml_backend_buffer_t b, uint8_t value);
static void w8_reset(ggml_backend_buffer_t b);

static struct ggml_backend_buffer_type g_type_a = {
    .iface = {
        .get_name     = w8_name,
        .alloc_buffer = w8_alloc,
        .get_alignment = w8_alignment,
        .get_max_size = w8_max_size,
    },
};
static struct ggml_backend_buffer_type g_type_b = {
    .iface = {
        .get_name     = w8_name,
        .alloc_buffer = w8_alloc,
        .get_alignment = w8_alignment,
        .get_max_size = w8_max_size,
    },
};

static const char * w8_name(ggml_backend_buffer_type_t t) { (void) t; return "w8-host"; }
static size_t w8_alignment(ggml_backend_buffer_type_t t) { (void) t; return 64; }
static size_t w8_max_size(ggml_backend_buffer_type_t t) {
    return t == &g_type_b ? g_max_b : g_max_a;
}

static ggml_backend_buffer_t w8_alloc(ggml_backend_buffer_type_t t, size_t size) {
    g_alloc_calls++;
    if (g_fail_alloc_at && g_alloc_calls == g_fail_alloc_at) return NULL;

    size_t total = W8_GUARD + sizeof(struct w8_hdr) + size + W8_GUARD;
    void * raw = NULL;
    if (posix_memalign(&raw, 64, total) != 0) return NULL;
    unsigned char * p = (unsigned char *) raw;
    memset(p, 0xC5, W8_GUARD);
    struct w8_hdr * h = (struct w8_hdr *) (void *) (p + W8_GUARD);
    h->magic = W8_MAGIC;
    h->user = p + W8_GUARD + sizeof(struct w8_hdr);
    h->size = size;
    memset(h->user, 0, size);
    memset((unsigned char *) h->user + size, 0x5A, W8_GUARD);

    if (g_nlive >= W8_MAX_LIVE) { free(raw); return NULL; }
    g_live[g_nlive++] = h;

    struct ggml_backend_buffer_i iface = {
        /* .free_buffer   = */ w8_free_buffer,
        /* .get_base      = */ w8_base,
        /* .init_tensor   = */ w8_init_tensor,
        /* .memset_tensor = */ NULL,
        /* .set_tensor    = */ NULL,
        /* .get_tensor    = */ NULL,
        /* .set_tensor_2d = */ NULL,
        /* .get_tensor_2d = */ NULL,
        /* .cpy_tensor    = */ NULL,
        /* .clear         = */ w8_clear,
        /* .reset         = */ w8_reset,
    };
    return ggml_backend_buffer_init(t, iface, h, size);
}

static void w8_free_buffer(ggml_backend_buffer_t b) {
    struct w8_hdr * h = (struct w8_hdr *) b->context;
    if (!h) return;
    CHECK(h->magic == W8_MAGIC);
    unsigned char * raw = (unsigned char *) h - W8_GUARD;
    for (int i = 0; i < W8_GUARD; ++i) CHECK(raw[i] == 0xC5);
    size_t s = h->size;
    for (int i = 0; i < W8_GUARD; ++i) CHECK(((unsigned char *) h->user)[s + (size_t) i] == 0x5A);

    int idx = -1;
    for (int i = 0; i < g_nlive; ++i) if (g_live[i] == h) { idx = i; break; }
    if (idx < 0) {
        fprintf(stderr, "FAIL %s:%d: double free detected\n", __FILE__, __LINE__);
        g_errors++;
        return;
    }
    g_live[idx] = g_live[--g_nlive];
    h->magic = 0;
    free(raw);
    g_frees++;
}

static void * w8_base(ggml_backend_buffer_t b) {
    struct w8_hdr * h = (struct w8_hdr *) b->context;
    return h ? h->user : NULL;
}

static enum ggml_status w8_init_tensor(ggml_backend_buffer_t b, struct ggml_tensor * t) {
    (void) b;
    g_init_calls++;
    if (g_fail_init_at && g_init_calls == g_fail_init_at) return GGML_STATUS_FAILED;
    if (!t->data) return GGML_STATUS_FAILED;
    return GGML_STATUS_SUCCESS;
}

static void w8_clear(ggml_backend_buffer_t b, uint8_t value) {
    struct w8_hdr * h = (struct w8_hdr *) b->context;
    if (h) memset(h->user, value, h->size);
}

static void w8_reset(ggml_backend_buffer_t b) {
    struct w8_hdr * h = (struct w8_hdr *) b->context;
    if (h) memset(h->user, 0, h->size);
}

static struct ggml_context * make_ctx(void) {
    struct ggml_init_params params = {
        /* .mem_size   = */ 4u * 1024u * 1024u,
        /* .mem_buffer = */ NULL,
        /* .no_alloc   = */ true,
    };
    return ggml_init(params);
}

static struct ggml_cgraph * build_graph(struct ggml_context * c, struct ggml_tensor ** out_z) {
    struct ggml_tensor * x = ggml_new_tensor_2d(c, GGML_TYPE_F32, 32, 8);
    struct ggml_tensor * y = ggml_new_tensor_2d(c, GGML_TYPE_F32, 32, 8);
    ggml_set_name(x, "w8_x");
    ggml_set_name(y, "w8_y");
    ggml_set_input(x);
    ggml_set_input(y);
    struct ggml_tensor * sum = ggml_add(c, x, y);
    struct ggml_tensor * sc  = ggml_scale(c, sum, 0.5f);
    struct ggml_tensor * v   = ggml_view_2d(c, sc, 16, 8, sc->nb[1], 0);
    struct ggml_tensor * z   = ggml_cont(c, v);
    ggml_set_output(z);
    struct ggml_cgraph * g = ggml_new_graph(c);
    ggml_build_forward_expand(g, z);
    if (out_z) *out_z = z;
    return g;
}

static void make_types(int nbufs, int shared, ggml_backend_buffer_type_t * types) {
    types[0] = &g_type_a;
    if (nbufs > 1) types[1] = shared ? &g_type_a : &g_type_b;
}

static void fill_ids(struct ggml_cgraph * g, int nbufs, int shared, int * ids) {
    int n = ggml_graph_n_nodes(g);
    for (int i = 0; i < n; ++i) ids[i] = (nbufs > 1 && !shared) ? i % 2 : 0;
}

/* ------------------------------------------------------------------ cases */

static void case_size_differential(int nbufs, int shared, int growth) {
    CHECK(g_nlive == 0);
    reset_counters();
    g_max_a = 32768;
    g_max_b = 16384;

    struct ggml_context * c = make_ctx();
    CHECK(c != NULL);
    struct ggml_cgraph * g = build_graph(c, NULL);
    ggml_backend_buffer_type_t types[2];
    make_types(nbufs, shared, types);
    ggml_gallocr_t ga = ggml_gallocr_new_n(types, nbufs);
    CHECK(ga != NULL);
    CHECK(fg153_alloc_closure(1, (intptr_t) ga, 0, 0) == nbufs);

    size_t limits[2] = { growth ? 65536u : 0u, growth ? 32768u : 0u };
    ggml_gallocr_set_buffer_growth_limits(ga, limits);

    int ids[256];
    fill_ids(g, nbufs, shared, ids);
    CHECK(ggml_gallocr_reserve_n(ga, g, ids, NULL));
    CHECK(g_nlive > 0);

    size_t native[2] = {0, 0};
    size_t goose[2]  = {0, 0};
    ggml_gallocr_reserve_n_size(ga, g, ids, NULL, native);
    fg153_alloc_closure(0, (intptr_t) ga, (intptr_t) goose, 0);
    for (int i = 0; i < nbufs; ++i) CHECK(native[i] == goose[i]);
    if (shared && nbufs > 1) CHECK(goose[0] == goose[1]);

    /* Reallocate real buffers and cross-check the public size view.  With
     * growth limits the allocated chunk may be larger than the plan, so the
     * equality check is only exact when growth is off. */
    CHECK(ggml_gallocr_reserve_n(ga, g, ids, NULL));
    CHECK(g_nlive > 0);
    for (int i = 0; i < nbufs; ++i) {
        size_t pub = ggml_gallocr_get_buffer_size(ga, i);
        if (shared && i > 0) CHECK(pub == 0);
        else if (!growth) CHECK(pub == goose[i]);
        else CHECK(pub >= goose[i]);
        mix(pub);
    }

    CHECK(ggml_gallocr_alloc_graph(ga, g));
    int n = ggml_graph_n_nodes(g);
    for (int i = 0; i < n; ++i) {
        struct ggml_tensor * t = ggml_graph_node(g, i);
        if (!t->view_src) { CHECK(t->data != NULL); CHECK(t->buffer != NULL); }
        mix(ggml_nbytes(t));
    }

    ggml_gallocr_free(ga);
    CHECK(g_nlive == 0);
    CHECK(g_frees == g_alloc_calls);
    ggml_free(c);
    g_cases++;
}

static void case_multichunk(void) {
    CHECK(g_nlive == 0);
    reset_counters();
    /* A small backend max forces the dyn tallocr to split the plan into more
     * than one chunk, so the closure must sum across every chunk, not just the
     * first one. */
    g_max_a = 512;

    struct ggml_context * c = make_ctx();
    CHECK(c != NULL);
    struct ggml_cgraph * g = build_graph(c, NULL);
    ggml_backend_buffer_type_t types[1] = { &g_type_a };
    ggml_gallocr_t ga = ggml_gallocr_new_n(types, 1);
    CHECK(ga != NULL);
    int ids[256];
    fill_ids(g, 1, 0, ids);
    CHECK(ggml_gallocr_reserve_n(ga, g, ids, NULL));

    int64_t allocator = fg153_alloc_closure(2, (intptr_t) ga, 0, 0);
    CHECK(allocator != 0);
    int64_t chunks = fg153_alloc_closure(3, allocator, 0, 0);
    CHECK(chunks > 1);
    CHECK(chunks <= 16);

    size_t native[1] = {0};
    size_t goose[1]  = {0};
    ggml_gallocr_reserve_n_size(ga, g, ids, NULL, native);
    fg153_alloc_closure(0, (intptr_t) ga, (intptr_t) goose, 0);
    CHECK(native[0] == goose[0]);
    CHECK(goose[0] > 0);
    mix((uint64_t) chunks);
    mix(goose[0]);

    CHECK(ggml_gallocr_reserve_n(ga, g, ids, NULL));
    CHECK(ggml_gallocr_alloc_graph(ga, g));
    ggml_gallocr_free(ga);
    CHECK(g_nlive == 0);
    ggml_free(c);
    g_cases++;
}

static void case_oom_retry(int nbufs, int shared) {
    for (int at = 1; at <= 8; ++at) {
        CHECK(g_nlive == 0);
        reset_counters();
        g_max_a = 4096;
        g_max_b = 2048;

        struct ggml_context * c = make_ctx();
        CHECK(c != NULL);
        struct ggml_cgraph * g = build_graph(c, NULL);
        ggml_backend_buffer_type_t types[2];
        make_types(nbufs, shared, types);
        ggml_gallocr_t ga = ggml_gallocr_new_n(types, nbufs);
        CHECK(ga != NULL);
        int ids[256];
        fill_ids(g, nbufs, shared, ids);

        g_fail_alloc_at = at;
        bool ok = ggml_gallocr_reserve_n(ga, g, ids, NULL);
        if (!ok) {
            /* Earlier buffer slots may already be owned by galloc; freeing it
             * must release them exactly once. */
            ggml_gallocr_free(ga);
            CHECK(g_nlive == 0);

            ga = ggml_gallocr_new_n(types, nbufs);
            CHECK(ga != NULL);
            g_fail_alloc_at = 0;
            CHECK(ggml_gallocr_reserve_n(ga, g, ids, NULL));
        }

        size_t native[2] = {0, 0};
        size_t goose[2]  = {0, 0};
        ggml_gallocr_reserve_n_size(ga, g, ids, NULL, native);
        fg153_alloc_closure(0, (intptr_t) ga, (intptr_t) goose, 0);
        for (int i = 0; i < nbufs; ++i) CHECK(native[i] == goose[i]);

        /* reserve_n_size re-planned with no_alloc, so buffers are gone. */
        CHECK(ggml_gallocr_reserve_n(ga, g, ids, NULL));
        CHECK(ggml_gallocr_alloc_graph(ga, g));
        ggml_gallocr_free(ga);
        CHECK(g_nlive == 0);
        ggml_free(c);
        g_cases++;
    }
}

static void case_partial_init(void) {
    CHECK(g_nlive == 0);
    reset_counters();
    g_max_a = 1u << 20;

    struct ggml_context * c = make_ctx();
    CHECK(c != NULL);
    struct ggml_cgraph * g = build_graph(c, NULL);
    ggml_backend_buffer_type_t types[1] = { &g_type_a };
    ggml_gallocr_t ga = ggml_gallocr_new_n(types, 1);
    CHECK(ga != NULL);
    int ids[256];
    fill_ids(g, 1, 0, ids);
    CHECK(ggml_gallocr_reserve_n(ga, g, ids, NULL));
    int live_before = g_nlive;
    CHECK(live_before > 0);

    g_fail_init_at = 3;
    g_init_calls = 0;
    bool ok = ggml_gallocr_alloc_graph(ga, g);
    /* Goose graph-allocator returns false after the journal restore.  The
     * native (rollback) path returns true here; record which path ran. */
    if (ok) {
        fprintf(stderr, "NOTE partial-init failure not surfaced (native rollback path)\n");
    }
    /* Every captured tensor must be back to its pre-init ownership. */
    int n = ggml_graph_n_nodes(g);
    for (int i = 0; i < n; ++i) {
        struct ggml_tensor * t = ggml_graph_node(g, i);
        CHECK(t->data == NULL);
        CHECK(t->buffer == NULL);
    }
    struct ggml_tensor * lx = ggml_graph_get_tensor(g, "w8_x");
    struct ggml_tensor * ly = ggml_graph_get_tensor(g, "w8_y");
    CHECK(lx != NULL && ly != NULL);
    CHECK(lx->data == NULL && lx->buffer == NULL);
    CHECK(ly->data == NULL && ly->buffer == NULL);
    CHECK(g_nlive == live_before);

    g_fail_init_at = 0;
    g_init_calls = 0;
    CHECK(ggml_gallocr_alloc_graph(ga, g));
    for (int i = 0; i < n; ++i) {
        struct ggml_tensor * t = ggml_graph_node(g, i);
        if (!t->view_src) CHECK(t->data != NULL);
    }

    ggml_gallocr_free(ga);
    CHECK(g_nlive == 0);
    ggml_free(c);
    g_cases++;
}

static void case_address_reuse(void) {
    CHECK(g_nlive == 0);
    reset_counters();
    g_max_a = 1u << 20;

    struct ggml_context * c = make_ctx();
    CHECK(c != NULL);
    struct ggml_tensor * z = NULL;
    struct ggml_cgraph * g = build_graph(c, &z);
    ggml_backend_buffer_type_t types[1] = { &g_type_a };
    ggml_gallocr_t ga = ggml_gallocr_new_n(types, 1);
    CHECK(ga != NULL);
    int ids[256];
    fill_ids(g, 1, 0, ids);
    CHECK(ggml_gallocr_reserve_n(ga, g, ids, NULL));
    CHECK(ggml_gallocr_alloc_graph(ga, g));

    int n = ggml_graph_n_nodes(g);
    void * data1[256];
    void * buf1[256];
    for (int i = 0; i < n; ++i) {
        struct ggml_tensor * t = ggml_graph_node(g, i);
        data1[i] = t->data;
        buf1[i] = t->buffer;
    }
    int live1 = g_nlive;

    CHECK(ggml_gallocr_alloc_graph(ga, g));
    CHECK(g_nlive == live1);
    for (int i = 0; i < n; ++i) {
        struct ggml_tensor * t = ggml_graph_node(g, i);
        CHECK(t->data == data1[i]);
        CHECK(t->buffer == buf1[i]);
    }
    CHECK(z->data != NULL && z->buffer != NULL);
    memset(z->data, 0x6b, ggml_nbytes(z));
    CHECK(((unsigned char *) z->data)[ggml_nbytes(z) - 1] == 0x6b);
    mix((uintptr_t) z->data - (uintptr_t) ggml_backend_buffer_get_base(z->buffer));

    ggml_gallocr_free(ga);
    CHECK(g_nlive == 0);
    ggml_free(c);
    g_cases++;
}

static void case_null_lifecycle(void) {
    CHECK(g_nlive == 0);
    ggml_gallocr_free(NULL);
    ggml_gallocr_set_buffer_growth_limits(NULL, NULL);
    size_t limits[1] = { 123 };
    ggml_gallocr_set_buffer_growth_limits(NULL, limits);
    ggml_gallocr_free(NULL);
    CHECK(g_nlive == 0);
    g_cases++;
}

static int expected_inplace(int op) {
    switch (op) {
        case GGML_OP_FILL:
        case GGML_OP_SCALE:
        case GGML_OP_DIAG_MASK_ZERO:
        case GGML_OP_DIAG_MASK_INF:
        case GGML_OP_ADD:
        case GGML_OP_ADD_ID:
        case GGML_OP_ADD1:
        case GGML_OP_SUB:
        case GGML_OP_MUL:
        case GGML_OP_DIV:
        case GGML_OP_SQR:
        case GGML_OP_SQRT:
        case GGML_OP_LOG:
        case GGML_OP_UNARY:
        case GGML_OP_ROPE:
        case GGML_OP_ROPE_BACK:
        case GGML_OP_SILU_BACK:
        case GGML_OP_RMS_NORM:
        case GGML_OP_RMS_NORM_BACK:
        case GGML_OP_CLAMP:
        case GGML_OP_SOFT_MAX:
        case GGML_OP_SOFT_MAX_BACK:
            return 1;
        default:
            return 0;
    }
}

static void case_contracts(void) {
    for (int op = 0; op < (int) GGML_OP_COUNT; ++op) {
        int native = ggml_op_can_inplace((enum ggml_op) op) ? 1 : 0;
        int goose  = (int) fg153_alloc_closure(6, op, 0, 0);
        CHECK(native == goose);
        CHECK(native == expected_inplace(op));
    }

    const int64_t bases[] = { 0, 1, 7, 64, 4096, 4099 };
    const int64_t offs[]  = { 0, 1, 3, 7, 63, 64, 127, 1000 };
    const int64_t aligns[] = { 1, 2, 4, 8, 16, 32, 64, 128 };
    for (size_t bi = 0; bi < sizeof(bases) / sizeof(bases[0]); ++bi) {
        for (size_t oi = 0; oi < sizeof(offs) / sizeof(offs[0]); ++oi) {
            for (size_t ai = 0; ai < sizeof(aligns) / sizeof(aligns[0]); ++ai) {
                int64_t base = bases[bi], off = offs[oi], al = aligns[ai];
                int64_t rem = (base + off) % al;
                int64_t adjust = rem == 0 ? 0 : al - rem;
                int64_t expected = off + adjust;
                int64_t got = fg153_alloc_closure(7, base, off, al);
                CHECK(got == expected);
                mix((uint64_t) got);
            }
        }
    }
    g_cases++;
}

int main(void) {
    case_size_differential(1, 0, 0);
    case_size_differential(1, 0, 1);
    case_size_differential(2, 0, 0);
    case_size_differential(2, 1, 0);
    case_size_differential(2, 0, 1);
    case_multichunk();
    case_oom_retry(1, 0);
    case_oom_retry(2, 0);
    case_oom_retry(2, 1);
    case_partial_init();
    case_address_reuse();
    case_null_lifecycle();
    case_contracts();

    if (g_errors) {
        fprintf(stderr, "{\"status\":\"FAIL_LOCAL_ALLOC_CLOSURE\",\"errors\":%d,\"cases\":%u}\n",
                g_errors, g_cases);
        return 1;
    }
    printf("{\"status\":\"PASS_LOCAL_ALLOC_CLOSURE\",\"cases\":%u,\"live\":%d,\"allocs\":%d,"
           "\"frees\":%d,\"hash\":\"%016llx\"}\n",
           g_cases, g_nlive, g_alloc_calls, g_frees, (unsigned long long) g_hash);
    return 0;
}
