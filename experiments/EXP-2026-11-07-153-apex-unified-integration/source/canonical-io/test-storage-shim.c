/* test-storage-shim.c - LOCAL TEST HARNESS ONLY for the worker8 canonical storage
 * contract. Not production: worker8 owns the real Goose implementation. */
#include "canonical-io.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAXN 1024
#define MAXK 16
struct rec { int used; int64_t kind; int64_t k[MAXK], v[MAXK]; int n; };
struct vec { int used; int64_t item; int64_t len, cap; unsigned char * d; };
struct map { int used; int64_t k[64], v[64], n; };
static struct rec R[MAXN]; static struct vec V[MAXN]; static struct map M[MAXN];
static int64_t live_r, live_v, live_m;
int64_t fg_test_live_records(void){return live_r;}
int64_t fg_test_live_vecs(void){return live_v;}
int64_t fg_test_live_maps(void){return live_m;}
static int64_t alloc_slot(void * arr, int64_t sz) {
    unsigned char * p = arr;
    for (int64_t i = 0; i < MAXN; ++i) if (!*(int *)(p + i * sz)) return i;
    return -1;
}
int64_t fgc_record_new(int64_t kind) {
    int64_t i = alloc_slot(R, sizeof(struct rec)); if (i < 0) return 0;
    memset(&R[i], 0, sizeof R[i]); R[i].used = 1; R[i].kind = kind; live_r++; return i + 1;
}
int64_t fgc_record_get(int64_t h, int64_t key) {
    struct rec * r = &R[h - 1]; if (!r->used) return 0;
    for (int j = 0; j < r->n; ++j) if (r->k[j] == key) return r->v[j];
    return 0;
}
int64_t fgc_record_set(int64_t h, int64_t key, int64_t val) {
    struct rec * r = &R[h - 1]; if (!r->used) return -1;
    for (int j = 0; j < r->n; ++j) if (r->k[j] == key) { r->v[j] = val; return 0; }
    if (r->n >= MAXK) return -1; r->k[r->n] = key; r->v[r->n] = val; r->n++; return 0;
}
int64_t fgc_i64_record_free(int64_t h) { if (h <= 0 || !R[h - 1].used) return -1; R[h-1].used = 0; live_r--; return 0; }
int64_t fgc_vec_new(int64_t ib) {
    int64_t i = alloc_slot(V, sizeof(struct vec)); if (i < 0) return 0;
    memset(&V[i], 0, sizeof V[i]); V[i].used = 1; V[i].item = ib; live_v++; return i + 1;
}
int64_t fgc_vec_len(int64_t h) { return V[h - 1].used ? V[h - 1].len : 0; }
int64_t fgc_i64_vec_data(int64_t h) { return V[h - 1].used ? (int64_t)(intptr_t) V[h - 1].d : 0; }
int64_t fgc_vec_resize(int64_t h, int64_t n) {
    struct vec * v = &V[h - 1]; if (!v->used || n < 0) return -1;
    if (n > v->cap) { int64_t nc = n < 8 ? 8 : n; unsigned char * p = realloc(v->d, (size_t)(nc * v->item));
        if (!p) return -1; v->d = p; v->cap = nc; }
    if (n > v->len) memset(v->d + v->len * v->item, 0, (size_t)((n - v->len) * v->item));
    v->len = n; return 0;
}
int64_t fgc_i64_vec_free(int64_t h) { struct vec * v = &V[h - 1]; if (!v->used) return -1; free(v->d); v->used = 0; live_v--; return 0; }
int64_t fgc_map_new(void) {
    int64_t i = alloc_slot(M, sizeof(struct map)); if (i < 0) return 0;
    memset(&M[i], 0, sizeof M[i]); M[i].used = 1; live_m++; return i + 1;
}
int64_t fgc_map_put(int64_t h, int64_t key, int64_t val) {
    struct map * m = &M[h - 1]; if (!m->used) return -1;
    for (int j = 0; j < m->n; ++j) if (m->k[j] == key) { m->v[j] = val; return 0; }
    if (m->n >= 64) return -1; m->k[m->n] = key; m->v[m->n] = val; m->n++; return 0;
}
int64_t fgc_i64_map_get(int64_t h, int64_t key, int64_t out) {
    struct map * m = &M[h - 1]; if (!m->used) return 0;
    for (int j = 0; j < m->n; ++j) if (m->k[j] == key) { if (out) *(int64_t *)(intptr_t) out = m->v[j]; return 1; }
    return 0;
}
int64_t fgc_map_erase(int64_t h, int64_t key) {
    struct map * m = &M[h - 1]; if (!m->used) return -1;
    for (int j = 0; j < m->n; ++j) if (m->k[j] == key) { m->k[j] = m->k[m->n-1]; m->v[j] = m->v[m->n-1]; m->n--; return 0; }
    return 0;
}
int64_t fgc_i64_map_free(int64_t h) { if (h <= 0 || !M[h - 1].used) return -1; M[h-1].used = 0; live_m--; return 0; }
