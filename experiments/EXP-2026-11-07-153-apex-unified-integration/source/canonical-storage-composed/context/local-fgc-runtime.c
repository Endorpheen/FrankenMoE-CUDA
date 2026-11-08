/* Local reference implementation of the worker8 canonical storage primitives.
 * This file is NOT part of the canonical production path: production links
 * worker8's real strong exports. It exists so worker5's local checks execute
 * real record/vector/map/set behaviour instead of success stubs.
 *
 * Policy here is limited to raw container effects; the canonical module still
 * owns its schema and all row/seq/ownership loops.
 */
#include "canonical-storage-imports.h"
#include <stdlib.h>
#include <string.h>

typedef struct rec_s {
    int64_t kind;
    size_t n, cap;
    int64_t * keys;
    int64_t * vals;
} rec_t;

typedef struct vec_s {
    size_t item, n, cap;
    unsigned char * data;
} vec_t;

typedef struct map_s {
    size_t n, cap;
    int64_t * keys;
    int64_t * vals;
} map_t;

typedef struct set_s {
    size_t n, cap;
    int64_t * keys;
} set_t;

static int grow(void ** p, size_t * cap, size_t need, size_t item) {
    if (need <= *cap) return 0;
    size_t nc = *cap ? *cap : 8;
    while (nc < need) nc *= 2;
    void * q = realloc(*p, nc * item);
    if (!q) return -1;
    *p = q;
    *cap = nc;
    return 0;
}

int64_t fgc_record_new(int64_t kind) {
    rec_t * r = calloc(1, sizeof *r);
    if (!r) return -1;
    r->kind = kind;
    return (int64_t) (intptr_t) r;
}
int64_t fgc_record_get(int64_t h, int64_t key) {
    rec_t * r = (rec_t *) (intptr_t) h;
    if (!r) return 0;
    for (size_t i = 0; i < r->n; ++i) if (r->keys[i] == key) return r->vals[i];
    return 0;
}
int64_t fgc_record_set(int64_t h, int64_t key, int64_t value) {
    rec_t * r = (rec_t *) (intptr_t) h;
    if (!r) return -1;
    for (size_t i = 0; i < r->n; ++i) if (r->keys[i] == key) { r->vals[i] = value; return 0; }
    if (grow((void **) &r->keys, &r->cap, r->n + 1, sizeof(int64_t))) return -1;
    r->vals = realloc(r->vals, r->cap * sizeof(int64_t));
    if (!r->vals) return -1;
    r->keys[r->n] = key;
    r->vals[r->n] = value;
    r->n++;
    return 0;
}
int64_t fgc_record_kind(int64_t h) { rec_t * r = (rec_t *) (intptr_t) h; return r ? r->kind : -1; }
int64_t fgc_i64_record_free(int64_t h) { rec_t * r = (rec_t *) (intptr_t) h; if (!r) return -1; free(r->keys); free(r->vals); free(r); return 0; }

int64_t fgc_vec_new(int64_t item_bytes) {
    if (item_bytes <= 0) return -1;
    vec_t * v = calloc(1, sizeof *v);
    if (!v) return -1;
    v->item = (size_t) item_bytes;
    return (int64_t) (intptr_t) v;
}
int64_t fgc_vec_len(int64_t h) { vec_t * v = (vec_t *) (intptr_t) h; return v ? (int64_t) v->n : 0; }
int64_t fgc_i64_vec_data(int64_t h) { vec_t * v = (vec_t *) (intptr_t) h; return v ? (int64_t) (intptr_t) v->data : 0; }
int64_t fgc_vec_reserve(int64_t h, int64_t count) {
    vec_t * v = (vec_t *) (intptr_t) h;
    if (!v || count < 0) return -1;
    return grow((void **) &v->data, &v->cap, (size_t) count, v->item) ? -1 : 0;
}
int64_t fgc_vec_resize(int64_t h, int64_t count) {
    vec_t * v = (vec_t *) (intptr_t) h;
    if (!v || count < 0) return -1;
    if (fgc_vec_reserve(h, count)) return -1;
    if ((size_t) count > v->n) memset(v->data + v->n * v->item, 0, ((size_t) count - v->n) * v->item);
    v->n = (size_t) count;
    return 0;
}
int64_t fgc_i64_vec_free(int64_t h) { vec_t * v = (vec_t *) (intptr_t) h; if (!v) return -1; free(v->data); free(v); return 0; }

static int map_find(map_t * m, int64_t key) {
    size_t lo = 0, hi = m->n;
    while (lo < hi) { size_t mid = (lo + hi) / 2; if (m->keys[mid] < key) lo = mid + 1; else hi = mid; }
    return (int) lo;
}
int64_t fgc_map_new(void) { map_t * m = calloc(1, sizeof *m); return m ? (int64_t) (intptr_t) m : -1; }
int64_t fgc_map_put(int64_t h, int64_t key, int64_t value) {
    map_t * m = (map_t *) (intptr_t) h;
    if (!m) return -1;
    int i = map_find(m, key);
    if ((size_t) i < m->n && m->keys[i] == key) { m->vals[i] = value; return 0; }
    if (grow((void **) &m->keys, &m->cap, m->n + 1, sizeof(int64_t))) return -1;
    m->vals = realloc(m->vals, m->cap * sizeof(int64_t));
    if (!m->vals) return -1;
    memmove(m->keys + i + 1, m->keys + i, (m->n - (size_t) i) * sizeof(int64_t));
    memmove(m->vals + i + 1, m->vals + i, (m->n - (size_t) i) * sizeof(int64_t));
    m->keys[i] = key; m->vals[i] = value; m->n++;
    return 0;
}
int64_t fgc_i64_map_get(int64_t h, int64_t key, int64_t out_ptr) {
    map_t * m = (map_t *) (intptr_t) h;
    if (!m || !out_ptr) return 0;
    int i = map_find(m, key);
    if ((size_t) i >= m->n || m->keys[i] != key) return 0;
    *(int64_t *) (intptr_t) out_ptr = m->vals[i];
    return 1;
}
int64_t fgc_map_erase(int64_t h, int64_t key) {
    map_t * m = (map_t *) (intptr_t) h;
    if (!m) return -1;
    int i = map_find(m, key);
    if ((size_t) i >= m->n || m->keys[i] != key) return 0;
    memmove(m->keys + i, m->keys + i + 1, (m->n - (size_t) i - 1) * sizeof(int64_t));
    memmove(m->vals + i, m->vals + i + 1, (m->n - (size_t) i - 1) * sizeof(int64_t));
    m->n--;
    return 0;
}
int64_t fgc_i64_map_free(int64_t h) { map_t * m = (map_t *) (intptr_t) h; if (!m) return -1; free(m->keys); free(m->vals); free(m); return 0; }

static int set_find(set_t * s, int64_t key) {
    size_t lo = 0, hi = s->n;
    while (lo < hi) { size_t mid = (lo + hi) / 2; if (s->keys[mid] < key) lo = mid + 1; else hi = mid; }
    return (int) lo;
}
int64_t fgc_set_new(void) { set_t * s = calloc(1, sizeof *s); return s ? (int64_t) (intptr_t) s : -1; }
int64_t fgc_set_insert(int64_t h, int64_t key) {
    set_t * s = (set_t *) (intptr_t) h;
    if (!s) return -1;
    int i = set_find(s, key);
    if ((size_t) i < s->n && s->keys[i] == key) return 0;
    if (grow((void **) &s->keys, &s->cap, s->n + 1, sizeof(int64_t))) return -1;
    memmove(s->keys + i + 1, s->keys + i, (s->n - (size_t) i) * sizeof(int64_t));
    s->keys[i] = key; s->n++;
    return 0;
}
int64_t fgc_set_erase(int64_t h, int64_t key) {
    set_t * s = (set_t *) (intptr_t) h;
    if (!s) return -1;
    int i = set_find(s, key);
    if ((size_t) i >= s->n || s->keys[i] != key) return 0;
    memmove(s->keys + i, s->keys + i + 1, (s->n - (size_t) i - 1) * sizeof(int64_t));
    s->n--;
    return 0;
}
int64_t fgc_set_contains(int64_t h, int64_t key) {
    set_t * s = (set_t *) (intptr_t) h;
    if (!s) return 0;
    int i = set_find(s, key);
    return ((size_t) i < s->n && s->keys[i] == key) ? 1 : 0;
}
int64_t fgc_set_lower_bound(int64_t h, int64_t key) {
    set_t * s = (set_t *) (intptr_t) h;
    if (!s) return 0;
    int i = set_find(s, key);
    return (size_t) i < s->n ? s->keys[i] : 0;
}
int64_t fgc_set_first(int64_t h) { set_t * s = (set_t *) (intptr_t) h; return (s && s->n) ? s->keys[0] : 0; }
int64_t fgc_set_next(int64_t h, int64_t key) {
    set_t * s = (set_t *) (intptr_t) h;
    if (!s) return 0;
    int i = set_find(s, key);
    if ((size_t) i < s->n && s->keys[i] == key) i++;
    return (size_t) i < s->n ? s->keys[i] : 0;
}
int64_t fgc_set_free(int64_t h) { set_t * s = (set_t *) (intptr_t) h; if (!s) return -1; free(s->keys); free(s); return 0; }

int64_t fgc_raw_calloc(int64_t nbytes) { return nbytes > 0 ? (int64_t) (intptr_t) calloc(1, (size_t) nbytes) : 0; }
void fgc_raw_free(int64_t p) { free((void *) (intptr_t) p); }
void fgc_raw_memcpy(int64_t dst, int64_t src, int64_t n) { if (dst && src && n > 0) memcpy((void *) (intptr_t) dst, (void *) (intptr_t) src, (size_t) n); }
void fgc_raw_memset(int64_t dst, int64_t v, int64_t n) { if (dst && n > 0) memset((void *) (intptr_t) dst, (int) v, (size_t) n); }
int64_t fgc_raw_ptr_add(int64_t base, int64_t off) { return base ? (int64_t) (intptr_t) ((unsigned char *) (intptr_t) base + off) : 0; }
int64_t fgc_raw_load_i64(int64_t a) { return a ? *(int64_t *) (intptr_t) a : 0; }
int64_t fgc_raw_load_i32(int64_t a) { return a ? *(int32_t *) (intptr_t) a : 0; }
int64_t fgc_raw_load_i8(int64_t a) { return a ? *(int8_t *) (intptr_t) a : 0; }
void fgc_raw_store_i64(int64_t a, int64_t v) { if (a) *(int64_t *) (intptr_t) a = v; }
void fgc_raw_store_i32(int64_t a, int64_t v) { if (a) *(int32_t *) (intptr_t) a = (int32_t) v; }
void fgc_raw_store_i8(int64_t a, int64_t v) { if (a) *(int8_t *) (intptr_t) a = (int8_t) v; }
void fgc_raw_store_f32(int64_t a, int64_t bits) { if (a) *(int32_t *) (intptr_t) a = (int32_t) bits; }
