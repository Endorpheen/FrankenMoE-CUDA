#ifndef APEX_HOST_RUNTIME_H
#define APEX_HOST_RUNTIME_H
// Host-side SIMT emulation of the permitted device primitives. It runs the
// exact Goose-emitted kernel text on the CPU so differential tests exercise
// the same instruction order as the CUDA build. No compute lives here.
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

typedef struct { void *data; int64_t len; } gg_slice;

extern _Thread_local int64_t apex_h_tid_x;
extern _Thread_local int64_t apex_h_tid_y;
extern _Thread_local int64_t apex_h_tid_z;
extern _Thread_local int64_t apex_h_bid_x;
extern _Thread_local int64_t apex_h_bid_y;
extern _Thread_local int64_t apex_h_bid_z;
extern int64_t apex_h_bdim_x;
extern int64_t apex_h_bdim_y;
extern int64_t apex_h_bdim_z;
extern int64_t apex_h_gdim_x;
extern int64_t apex_h_gdim_y;
extern int64_t apex_h_gdim_z;
extern pthread_barrier_t apex_h_barrier;
extern float apex_h_warp[1024];

static inline void gg_fail(const char * message) {
    fprintf(stderr, "apex host kernel trap: %s\n", message);
    abort();
}
static inline int64_t gg_index(int64_t i, int64_t len) {
    if ((uint64_t) i >= (uint64_t) len) { gg_fail("array index out of bounds"); }
    return i;
}
static inline gg_slice gg_view(void * p, int64_t n) {
    gg_slice v; v.data = p; v.len = n; return v;
}

static inline int64_t gg_div(int64_t a, int64_t b) {
    if (!b) { gg_fail("division by zero"); }
    if (a == INT64_MIN && b == -1) { gg_fail("division overflow"); }
    return a / b;
}
static inline int64_t gg_mod(int64_t a, int64_t b) {
    int64_t r;
    if (!b) { gg_fail("division by zero"); }
    if (b == -1) { return 0; }
    r = a % b;
    if (r < 0) { return (int64_t)((uint64_t)r + (b < 0 ? (uint64_t)0 - (uint64_t)b : (uint64_t)b)); }
    return r;
}
static inline uint64_t gg_udiv(uint64_t a, uint64_t b) {
    if (!b) { gg_fail("division by zero"); }
    return a / b;
}
static inline uint64_t gg_umod(uint64_t a, uint64_t b) {
    if (!b) { gg_fail("division by zero"); }
    return a % b;
}

static inline double gg_double_bits(uint64_t u) {
    union { uint64_t u; double d; } x; x.u = u; return x.d;
}

static inline int64_t apex_tid_x()  { return apex_h_tid_x; }
static inline int64_t apex_tid_y()  { return apex_h_tid_y; }
static inline int64_t apex_tid_z()  { return apex_h_tid_z; }
static inline int64_t apex_bid_x()  { return apex_h_bid_x; }
static inline int64_t apex_bid_y()  { return apex_h_bid_y; }
static inline int64_t apex_bid_z()  { return apex_h_bid_z; }
static inline int64_t apex_bdim_x() { return apex_h_bdim_x; }
static inline int64_t apex_bdim_y() { return apex_h_bdim_y; }
static inline int64_t apex_bdim_z() { return apex_h_bdim_z; }
static inline int64_t apex_gdim_x() { return apex_h_gdim_x; }
static inline int64_t apex_gdim_y() { return apex_h_gdim_y; }
static inline int64_t apex_gdim_z() { return apex_h_gdim_z; }

static inline void apex_sync() { pthread_barrier_wait(&apex_h_barrier); }

static inline float apex_shfl_xor_f32(float v, int64_t off) {
    const int64_t base = (apex_h_tid_x / 32) * 32;
    const int64_t lane = apex_h_tid_x - base;
    apex_h_warp[apex_h_tid_x] = v;
    pthread_barrier_wait(&apex_h_barrier);
    const float r = apex_h_warp[base + (lane ^ off)];
    pthread_barrier_wait(&apex_h_barrier);
    return r;
}
static inline float apex_shfl_down_f32(float v, int64_t off) {
    const int64_t base = (apex_h_tid_x / 32) * 32;
    const int64_t lane = apex_h_tid_x - base;
    apex_h_warp[apex_h_tid_x] = v;
    pthread_barrier_wait(&apex_h_barrier);
    const float r = (lane + off < 32) ? apex_h_warp[base + lane + off] : v;
    pthread_barrier_wait(&apex_h_barrier);
    return r;
}

static inline float apex_expf(float x)   { return expf(x); }
static inline float apex_logf(float x)   { return logf(x); }
static inline float apex_rsqrtf(float x) { return 1.0f / sqrtf(x); }
static inline float apex_sqrtf(float x)  { return sqrtf(x); }
static inline float apex_powf(float a, float b) { return powf(a, b); }
static inline float apex_floorf(float x) { return floorf(x); }
static inline float apex_ceilf(float x)  { return ceilf(x); }
static inline float apex_cosf(float x)   { return cosf(x); }
static inline float apex_sinf(float x)   { return sinf(x); }
static inline float apex_tanhf(float x)  { return tanhf(x); }
static inline float apex_fabsf(float x)  { return fabsf(x); }
static inline float apex_fmaxf(float a, float b) { return fmaxf(a, b); }
static inline float apex_fminf(float a, float b) { return fminf(a, b); }
static inline float apex_neg_inf()       { return -INFINITY; }
static inline float apex_from_bits(uint32_t u) {
    union { uint32_t u; float f; } x; x.u = u; return x.f;
}

static inline uint16_t apex_f32_to_f16(float x) {
    union { float f; uint32_t u; } v; v.f = x;
    uint32_t sign = (v.u >> 16) & 0x8000u;
    int32_t exp = (int32_t)((v.u >> 23) & 0xffu) - 127 + 15;
    uint32_t mant = v.u & 0x7fffffu;
    if (exp <= 0) {
        if (exp < -10) { return (uint16_t)sign; }
        mant |= 0x800000u;
        uint32_t shift = (uint32_t)(14 - exp);
        uint32_t half = mant >> shift;
        if ((mant >> (shift - 1)) & 1u) { half++; }
        return (uint16_t)(sign | half);
    }
    if (exp >= 31) { return (uint16_t)(sign | 0x7c00u); }
    uint16_t half = (uint16_t)(sign | ((uint32_t)exp << 10) | (mant >> 13));
    if (mant & 0x1000u) { half++; }
    return half;
}
static inline float apex_f16_to_f32(uint16_t h) {
    uint32_t sign = (uint32_t)(h & 0x8000u) << 16;
    uint32_t exp = (h >> 10) & 0x1fu;
    uint32_t mant = h & 0x3ffu;
    uint32_t u;
    if (exp == 0) {
        if (mant == 0) { u = sign; }
        else {
            exp = 127 - 15 + 1;
            while ((mant & 0x400u) == 0) { mant <<= 1; exp--; }
            mant &= 0x3ffu;
            u = sign | (exp << 23) | (mant << 13);
        }
    } else if (exp == 31) {
        u = sign | 0x7f800000u | (mant << 13);
    } else {
        u = sign | ((exp + 127 - 15) << 23) | (mant << 13);
    }
    union { uint32_t u; float f; } v; v.u = u; return v.f;
}

#endif
