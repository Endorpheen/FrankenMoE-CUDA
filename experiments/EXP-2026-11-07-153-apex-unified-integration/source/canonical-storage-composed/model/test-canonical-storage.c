// worker-4/canonical-model: TEST-ONLY conformance implementation of the
// canonical-storage contract v1 subset used by the model schema, plus synthetic
// VOCAB/loader providers. This is not production storage and is not shipped as
// worker8; it exists so the local corpus exercises the Goose module with real
// semantics (not success stubs).
#include "canonical-storage-required.h"

#include <stdlib.h>
#include <string.h>

#define REC_KEYS 512
#define MAP_PAIRS 4096

typedef struct { int64_t kind; int64_t n; int64_t keys[REC_KEYS]; int64_t vals[REC_KEYS]; } trec;
typedef struct { int64_t item; int64_t len; int64_t cap; unsigned char * data; } tvec;
typedef struct { int64_t n; int64_t keys[MAP_PAIRS]; int64_t vals[MAP_PAIRS]; } tmap;

static int64_t g_record_frees = 0;
static int64_t g_vec_frees = 0;
static int64_t g_map_frees = 0;
int64_t fgc_test_record_frees(void) { return g_record_frees; }
int64_t fgc_test_vec_frees(void) { return g_vec_frees; }
int64_t fgc_test_map_frees(void) { return g_map_frees; }

int64_t fgc_record_new(int64_t kind) {
    trec * r = (trec *) calloc(1, sizeof *r);
    if (!r) return 0;
    r->kind = kind;
    return (int64_t) (intptr_t) r;
}
int64_t fgc_record_get(int64_t handle, int64_t key) {
    trec * r = (trec *) (intptr_t) handle;
    if (!r) return 0;
    for (int64_t i = 0; i < r->n; ++i) if (r->keys[i] == key) return r->vals[i];
    return 0;
}
int64_t fgc_record_set(int64_t handle, int64_t key, int64_t value) {
    trec * r = (trec *) (intptr_t) handle;
    if (!r) return -1;
    for (int64_t i = 0; i < r->n; ++i) if (r->keys[i] == key) { r->vals[i] = value; return 0; }
    if (r->n >= REC_KEYS) return -1;
    r->keys[r->n] = key; r->vals[r->n] = value; r->n++;
    return 0;
}
int64_t fgc_record_kind(int64_t handle) {
    trec * r = (trec *) (intptr_t) handle;
    return r ? r->kind : -1;
}
int64_t fgc_i64_record_free(int64_t handle) {
    trec * r = (trec *) (intptr_t) handle;
    if (!r) return 0;
    free(r); g_record_frees++;
    return 0;
}

int64_t fgc_vec_new(int64_t item_bytes) {
    tvec * v = (tvec *) calloc(1, sizeof *v);
    if (!v || item_bytes <= 0) { free(v); return 0; }
    v->item = item_bytes;
    return (int64_t) (intptr_t) v;
}
int64_t fgc_vec_len(int64_t handle) {
    tvec * v = (tvec *) (intptr_t) handle;
    return v ? v->len : 0;
}
int64_t fgc_i64_vec_data(int64_t handle) {
    tvec * v = (tvec *) (intptr_t) handle;
    return v ? (int64_t) (intptr_t) v->data : 0;
}
int64_t fgc_vec_reserve(int64_t handle, int64_t count) {
    tvec * v = (tvec *) (intptr_t) handle;
    if (!v || count < 0) return -1;
    if (count <= v->cap) return 0;
    int64_t cap = v->cap ? v->cap : 8;
    while (cap < count) cap *= 2;
    unsigned char * p = (unsigned char *) realloc(v->data, (size_t) cap * (size_t) v->item);
    if (!p) return -1;
    memset(p + (size_t) v->len * (size_t) v->item, 0, (size_t) (cap - v->len) * (size_t) v->item);
    v->data = p; v->cap = cap;
    return 0;
}
int64_t fgc_vec_resize(int64_t handle, int64_t count) {
    tvec * v = (tvec *) (intptr_t) handle;
    if (!v || count < 0) return -1;
    if (fgc_vec_reserve(handle, count) != 0) return -1;
    if (count > v->len) memset(v->data + (size_t) v->len * (size_t) v->item, 0, (size_t) (count - v->len) * (size_t) v->item);
    v->len = count;
    return 0;
}
int64_t fgc_i64_vec_free(int64_t handle) {
    tvec * v = (tvec *) (intptr_t) handle;
    if (!v) return 0;
    free(v->data); free(v); g_vec_frees++;
    return 0;
}

int64_t fgc_map_new(void) {
    tmap * m = (tmap *) calloc(1, sizeof *m);
    return m ? (int64_t) (intptr_t) m : 0;
}
int64_t fgc_map_put(int64_t handle, int64_t key, int64_t value) {
    tmap * m = (tmap *) (intptr_t) handle;
    if (!m) return -1;
    for (int64_t i = 0; i < m->n; ++i) if (m->keys[i] == key) { m->vals[i] = value; return 0; }
    if (m->n >= MAP_PAIRS) return -1;
    m->keys[m->n] = key; m->vals[m->n] = value; m->n++;
    return 0;
}
int64_t fgc_i64_map_get(int64_t handle, int64_t key, int64_t out_ptr) {
    tmap * m = (tmap *) (intptr_t) handle;
    if (!m) return 0;
    for (int64_t i = 0; i < m->n; ++i) if (m->keys[i] == key) {
        if (out_ptr) *(int64_t *) (intptr_t) out_ptr = m->vals[i];
        return 1;
    }
    return 0;
}
int64_t fgc_map_erase(int64_t handle, int64_t key) {
    tmap * m = (tmap *) (intptr_t) handle;
    if (!m) return -1;
    for (int64_t i = 0; i < m->n; ++i) if (m->keys[i] == key) {
        m->keys[i] = m->keys[m->n - 1]; m->vals[i] = m->vals[m->n - 1]; m->n--;
        return 0;
    }
    return 0;
}
int64_t fgc_i64_map_free(int64_t handle) {
    tmap * m = (tmap *) (intptr_t) handle;
    if (!m) return 0;
    free(m); g_map_frees++;
    return 0;
}

// ---- synthetic cross-schema providers (test only) -------------------------
typedef struct { int64_t n_tokens; } t_vocab;
typedef struct { int64_t layer, kind, tensor; } t_bind;
typedef struct { int64_t n; t_bind b[64]; } t_loader;

int64_t fgc_test_vocab_new(int64_t n_tokens) {
    t_vocab * v = (t_vocab *) calloc(1, sizeof *v);
    if (!v) return 0;
    v->n_tokens = n_tokens;
    return (int64_t) (intptr_t) v;
}
void fgc_test_vocab_free(int64_t vocab) { free((void *) (intptr_t) vocab); }
int64_t fgc_vocab_n_tokens(int64_t vocab) {
    t_vocab * v = (t_vocab *) (intptr_t) vocab;
    return v ? v->n_tokens : -1;
}
int64_t fgc_test_loader_new(void) { t_loader * l = (t_loader *) calloc(1, sizeof *l); return l ? (int64_t) (intptr_t) l : 0; }
void fgc_test_loader_free(int64_t loader) { free((void *) (intptr_t) loader); }
int64_t fgc_test_loader_add(int64_t loader, int64_t layer, int64_t kind, int64_t tensor) {
    t_loader * l = (t_loader *) (intptr_t) loader;
    if (!l || l->n >= 64) return -1;
    l->b[l->n].layer = layer; l->b[l->n].kind = kind; l->b[l->n].tensor = tensor; l->n++;
    return 0;
}
int64_t fgc_loader_get_tensor(int64_t loader, int64_t layer, int64_t kind) {
    t_loader * l = (t_loader *) (intptr_t) loader;
    if (!l) return 0;
    for (int64_t i = 0; i < l->n; ++i) if (l->b[i].layer == layer && l->b[i].kind == kind) return l->b[i].tensor;
    return 0;
}
int64_t fgc_loader_handover(int64_t loader, int64_t model) { (void) loader; (void) model; return 0; }
