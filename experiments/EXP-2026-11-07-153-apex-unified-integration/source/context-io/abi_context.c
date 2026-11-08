/* abi_context.c - typed C ABI: bind + context-aware device handles.
 * Raw mutex serializes the lazily-created canonical registry (no std container). */
#include "canonical-io.h"
#include <pthread.h>
#include <stdint.h>

extern int64_t fgcio_context_entry(int64_t mode, int64_t a, int64_t b, int64_t c, int64_t d, int64_t e);
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static int64_t g_reg = 0;

int64_t fgcio_bind_buffer(int64_t h, int64_t ptr, int64_t size) {
    pthread_mutex_lock(&g_mu);
    int64_t r = fgcio_context_entry(0, h, ptr, size, 0, 0);
    pthread_mutex_unlock(&g_mu);
    return r;
}
int64_t fgcio_context_write_device(int64_t ctx, int64_t seq, int64_t dst, int64_t size) {
    pthread_mutex_lock(&g_mu);
    if (!g_reg) g_reg = fgcio_context_entry(4, 0, 0, 0, 0, 0);
    int64_t h = g_reg ? fgcio_context_entry(1, g_reg, ctx, seq, dst, size) : 0;
    pthread_mutex_unlock(&g_mu);
    return h;
}
int64_t fgcio_context_read_device(int64_t ctx, int64_t seq, int64_t src, int64_t size) {
    pthread_mutex_lock(&g_mu);
    int64_t h = g_reg ? fgcio_context_entry(2, g_reg, ctx, seq, src, size) : 0;
    pthread_mutex_unlock(&g_mu);
    return h;
}
int64_t fgcio_context_drop(int64_t ctx) {
    pthread_mutex_lock(&g_mu);
    int64_t r = g_reg ? fgcio_context_entry(3, g_reg, ctx, 0, 0, 0) : 0;
    if (r == 1) g_reg = 0;
    pthread_mutex_unlock(&g_mu);
    return r;
}

extern int64_t fgc_map_get(int64_t,int64_t,int64_t *);
int64_t fgcio_map_value(int64_t h,int64_t key) {
 int64_t value=0;fgc_map_get(h,key,&value);return value;
}
int64_t fgcio_context_has(int64_t ctx,int64_t seq) {
 pthread_mutex_lock(&g_mu);
 int64_t r=g_reg?fgcio_context_entry(5,g_reg,ctx,seq,0,0):0;
 pthread_mutex_unlock(&g_mu);return r;
}
