// P4-13 host unit for the Goose allocator primitives (fg161) with the
// generation guard from P4-13-CONTRACT.md. Owns buffers only; block tables
// stay with the stream source (option B).
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ggml.h"
#include "ggml-backend.h"
#include <stdio.h>
static unsigned long g_bulk_calls, g_alloc_calls;
__attribute__((destructor)) static void fg161_report(void) {
    if (g_bulk_calls) fprintf(stderr, "GOOSE_ALLOC bulk=%lu buffers=%lu\n", g_bulk_calls, g_alloc_calls);
}

#define FG161_MAX 4096
typedef struct { ggml_backend_buffer_t buf; uint64_t gen; int live; } slot;
static slot g_slots[FG161_MAX];
static uint64_t g_next = 1;

int64_t fg161_alloc_buffer(int64_t buft, uint64_t size) {
    ggml_backend_buffer_t b = ggml_backend_buft_alloc_buffer((ggml_backend_buffer_type_t) (uintptr_t) buft, size);
    if (!b) return 0;
    for (size_t i = 1; i < FG161_MAX; ++i) {
        if (!g_slots[i].live) {
            g_slots[i].buf = b; g_slots[i].gen = 0; g_slots[i].live = 1;
            return (int64_t) i;
        }
    }
    ggml_backend_buffer_free(b);
    return 0;
}

void fg161_free_buffer(int64_t h) {
    if (h <= 0 || h >= FG161_MAX || !g_slots[h].live) return;
    ggml_backend_buffer_free(g_slots[h].buf);
    g_slots[h].buf = NULL; g_slots[h].live = 0; g_slots[h].gen++;
}

void fg161_reset_buffer(int64_t h) {
    if (h <= 0 || h >= FG161_MAX || !g_slots[h].live) return;
    ggml_backend_buffer_reset(g_slots[h].buf);
    g_slots[h].gen++;
}

void fg161_set_usage(int64_t h, int64_t usage) {
    if (h <= 0 || h >= FG161_MAX || !g_slots[h].live) return;
    ggml_backend_buffer_set_usage(g_slots[h].buf, (enum ggml_backend_buffer_usage) usage);
}

int64_t fg161_tensor_alloc(int64_t h, int64_t t, uint64_t address) {
    if (h <= 0 || h >= FG161_MAX || !g_slots[h].live) return -1;
    ggml_backend_tensor_alloc(g_slots[h].buf, (struct ggml_tensor *) (uintptr_t) t, (void *) (uintptr_t) address);
    return 0;
}

uint64_t fg161_generation(int64_t h) {
    if (h <= 0 || h >= FG161_MAX || !g_slots[h].live) return 0;
    return g_slots[h].gen;
}

int fg161_live_count(void) {
    int n = 0;
    for (size_t i = 1; i < FG161_MAX; ++i) n += g_slots[i].live;
    return n;
}

/* the Goose module imports this exact name */
void fg161_usage(int64_t h, int64_t usage) { fg161_set_usage(h, usage); }

void * fg161_buffer_base(int64_t h) {
    if (h <= 0 || h >= FG161_MAX || !g_slots[h].live) return NULL;
    return ggml_backend_buffer_get_base(g_slots[h].buf);
}

size_t fg161_buffer_size(int64_t h) {
    if (h <= 0 || h >= FG161_MAX || !g_slots[h].live) return 0;
    return ggml_backend_buffer_get_size(g_slots[h].buf);
}

/* bulk: place every tensor of a context into one buffer, packing exactly as the
   native allocator does (alloc size from the buft, alignment from the buft). */
int64_t fg161_bulk(int64_t buft, int64_t ctx, int64_t size_only) {
    g_bulk_calls++;
    ggml_backend_buffer_type_t bt = (ggml_backend_buffer_type_t)(uintptr_t) buft;
    struct ggml_context * c = (struct ggml_context *)(uintptr_t) ctx;
    size_t align = ggml_backend_buft_get_alignment(bt);
    size_t total = 0;
    for (struct ggml_tensor * t = ggml_get_first_tensor(c); t; t = ggml_get_next_tensor(c, t))
        if (t->view_src == NULL) total += ggml_backend_buft_get_alloc_size(bt, t);
    total = (total + align - 1) / align * align;
    if (size_only) return (int64_t) total;
    int64_t h = fg161_alloc_buffer(buft, total);
    if (h) g_alloc_calls++;
    if (!h) return 0;
    char * base = (char *) fg161_buffer_base(h);
    size_t off = 0;
    for (struct ggml_tensor * t = ggml_get_first_tensor(c); t; t = ggml_get_next_tensor(c, t)) {
        if (t->view_src != NULL) {
            ggml_backend_view_init(t);
            continue;
        }
        off = (off + align - 1) / align * align;
        ggml_backend_tensor_alloc(g_slots[h].buf, t, base + off);
        off += ggml_backend_buft_get_alloc_size(bt, t);
    }
    return h;
}

void * fg161_buffer_handle(int64_t h) {
    if (h <= 0 || h >= FG161_MAX || !g_slots[h].live) return NULL;
    return (void *) g_slots[h].buf;
}
