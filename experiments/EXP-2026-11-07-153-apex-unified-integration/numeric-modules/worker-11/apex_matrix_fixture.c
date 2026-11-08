/* Host differential fixture for the Goose Apex CUDA matrix core.
 *
 * The generated Goose C is compiled as ordinary C11 on the host and compared
 * with independent references:
 *   - Q4_K/Q6_K row values use the qualified native dequantize_row_* from
 *     /runtime/libggml-base.so,
 *   - q8_1 activations use the qualified native quantize_row_q8_1 from
 *     /runtime/libggml-cpu.so,
 *   - q8_1 dot uses those native values as its reference,
 *   - F16/F32 GEMV/GEMM use a straight reference loop with the same
 *     deterministic order,
 *   - MMVQ/MMQ dispatch and tile sizes use the constants from the frozen
 *     ggml-cuda headers (restated here, not read back from the Goose code).
 * No CUDA device is touched.
 */
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdatomic.h>

#include "apex_matrix.h"

void dequantize_row_q4_K(const void * x, float * y, int64_t k);
void dequantize_row_q6_K(const void * x, float * y, int64_t k);
void quantize_row_q8_1(const float * x, void * y, int64_t k);

uint32_t fgm_bits(float x) { uint32_t v; memcpy(&v, &x, 4); return v; }
float fgm_from_bits(uint32_t v) { float x; memcpy(&x, &v, 4); return x; }
float fgm_half(uint16_t v) { return _cvtsh_ss(v); }
uint16_t fgm_to_half(float v) { return _cvtss_sh(v, _MM_FROUND_TO_NEAREST_INT); }

static atomic_uint_fast64_t fgm_calls[64];
void fgm_witness(int64_t op, int64_t value) {
    (void) value;
    if (op < 0 || op >= 64) __builtin_trap();
    atomic_fetch_add_explicit(&fgm_calls[op], 1, memory_order_relaxed);
}

#define main fgm_generated_unused_main
#include "apex_matrix_generated.c"
#undef main

static uint32_t rng_state;
static uint32_t next_u32(void) { rng_state = rng_state*1664525u + 1013904223u; return rng_state; }
static float next_f32(void) { return ((int)(next_u32()%2001u) - 1000) / 256.0f; }
static void reset(uint32_t seed) { rng_state = seed; }

static int failures;
static void report(const char * name, double got, double ref, double tol) {
    double diff = fabs(got-ref);
    double scale = fabs(ref) > 1.0 ? fabs(ref) : 1.0;
    if (diff > tol*scale) {
        printf("FAIL %s got=%.9g ref=%.9g diff=%.3g\n", name, got, ref, diff);
        failures++;
    }
}
static void report_exact(const char * name, double got, double ref) {
    if (memcmp(&got, &ref, sizeof(double)) != 0 && got != ref) {
        printf("FAIL %s got=%.9g ref=%.9g\n", name, got, ref);
        failures++;
    }
}

static uint8_t w4[144];
static uint8_t w6[210];
static uint8_t q8[8*36];
static float act[256];
static float ref[256];
static float out[256];

static void make_q4(void) {
    reset(0x4b344b51u);
    for (int i = 0; i < 128; i++) w4[16+i] = (uint8_t) next_u32();
    for (int i = 0; i < 12; i++) w4[4+i] = (uint8_t) next_u32();
    uint16_t d = fgm_to_half(0.5f), dmin = fgm_to_half(0.25f);
    memcpy(w4+0, &d, 2); memcpy(w4+2, &dmin, 2);
}
static void make_q6(void) {
    reset(0x4b364b51u);
    for (int i = 0; i < 128; i++) w6[i] = (uint8_t) next_u32();
    for (int i = 0; i < 64; i++) w6[128+i] = (uint8_t) next_u32();
    for (int i = 0; i < 16; i++) w6[192+i] = (uint8_t) next_u32();
    uint16_t d = fgm_to_half(0.125f);
    memcpy(w6+208, &d, 2);
}
static void make_act(void) {
    reset(0x71384b51u);
    for (int i = 0; i < 256; i++) act[i] = next_f32();
    quantize_row_q8_1(act, q8, 256);
}

static double ref_dot(const float * w, const uint8_t * q) {
    double s = 0.0;
    for (int b = 0; b < 8; b++) {
        uint16_t hd; memcpy(&hd, q+b*36, 2);
        float d8 = fgm_half(hd);
        const int8_t * qs = (const int8_t *)(q+b*36+4);
        for (int i = 0; i < 32; i++) s += (double)w[b*32+i] * d8 * (double)qs[i];
    }
    return s;
}

static void check_dequant(int kind, const uint8_t * w, size_t bytes) {
    memset(out, 0, sizeof(out));
    gg_slice ws = gg_view((void*)w, (int64_t)bytes);
    gg_slice xs = gg_view((void*)act, 256);
    gg_slice ys = gg_view(out, 256);
    int64_t ok = gg_function_1(kind == 4 ? 13 : 14, ws, xs, ys, 0,0,0,0, 0);
    if (ok != 1) { printf("FAIL dequant kind=%d status=%lld\n", kind, (long long)ok); failures++; return; }
    if (kind == 4) dequantize_row_q4_K(w, ref, 256); else dequantize_row_q6_K(w, ref, 256);
    for (int i = 0; i < 256; i++) {
        if (out[i] != ref[i]) {
            printf("FAIL dequant kind=%d lane=%d got=%.9g ref=%.9g\n", kind, i, out[i], ref[i]);
            failures++;
            return;
        }
    }
}

static void check_dot(int kind, const uint8_t * w, size_t bytes) {
    memset(out, 0, sizeof(out));
    gg_slice ws = gg_view((void*)w, (int64_t)bytes);
    gg_slice qs = gg_view((void*)q8, (int64_t)sizeof(q8));
    gg_slice ys = gg_view(out, 256);
    if (kind == 4) dequantize_row_q4_K(w, ref, 256); else dequantize_row_q6_K(w, ref, 256);
    int64_t ok = gg_function_1(kind == 4 ? 6 : 7, ws, qs, ys, 0,0,0,0, 0);
    if (ok != 1) { printf("FAIL dot kind=%d status=%lld\n", kind, (long long)ok); failures++; return; }
    report(kind == 4 ? "dot_q4k_q8_1" : "dot_q6k_q8_1", out[0], ref_dot(ref, q8), 2e-4);
}

static void check_gemv(int f16) {
    const int64_t nrows = 3, k = 40;
    static uint8_t wbuf[3*40*4];
    uint16_t * hw = (uint16_t*)wbuf;
    float * fw = (float*)wbuf;
    reset(f16 ? 0xf16f16u : 0xf32f32u);
    for (int r = 0; r < nrows; r++) {
        for (int i = 0; i < k; i++) {
            float v = next_f32();
            if (f16) hw[r*k+i] = fgm_to_half(v); else fw[r*k+i] = v;
        }
    }
    reset(0xabcdef01u);
    for (int i = 0; i < k; i++) act[i] = next_f32();
    for (int r = 0; r < nrows; r++) {
        out[0] = 0.0f;
        gg_slice ws = gg_view(wbuf, (int64_t)(f16 ? nrows*k*2 : nrows*k*4));
        gg_slice xs = gg_view(act, k*4);
        gg_slice ys = gg_view(out, 1);
        int64_t ok = gg_function_1(f16 ? 3 : 2, ws, xs, ys, nrows, k, r, 0, 0);
        if (ok != 1) { printf("FAIL gemv status=%lld\n", (long long)ok); failures++; return; }
        float s = 0.0f;
        for (int i = 0; i < k; i++) s = s + (f16 ? fgm_half(hw[r*k+i]) : fw[r*k+i]) * act[i];
        if (out[0] != s) { printf("FAIL %s row=%d got=%.9g ref=%.9g\n", f16?"gemv_f16":"gemv_f32", r, out[0], s); failures++; }
    }
}

static void check_gemm(int f16) {
    const int64_t m = 3, n = 4, k = 40;
    static uint8_t abuf[3*40*4];
    static float bbuf[40*4];
    uint16_t * ha = (uint16_t*)abuf;
    float * fa = (float*)abuf;
    reset(f16 ? 0x6e6d16u : 0x6e6d32u);
    for (int r = 0; r < m; r++) for (int i = 0; i < k; i++) {
        float v = next_f32();
        if (f16) ha[r*k+i] = fgm_to_half(v); else fa[r*k+i] = v;
    }
    reset(0xb00b1e5u);
    for (int i = 0; i < k*n; i++) bbuf[i] = next_f32();
    for (int r = 0; r < m; r++) {
        for (int j = 0; j < n; j++) out[j] = 0.0f;
        gg_slice as = gg_view(abuf, (int64_t)(f16 ? m*k*2 : m*k*4));
        gg_slice bs = gg_view(bbuf, k*n*4);
        gg_slice cs = gg_view(out, n);
        int64_t ok = gg_function_1(f16 ? 5 : 4, as, bs, cs, m, n, k, r, 0);
        if (ok != 1) { printf("FAIL gemm status=%lld\n", (long long)ok); failures++; return; }
        for (int j = 0; j < n; j++) {
            float s = 0.0f;
            for (int t = 0; t < k; t++) s = s + (f16 ? fgm_half(ha[r*k+t]) : fa[r*k+t]) * bbuf[t*n+j];
            if (out[j] != s) { printf("FAIL %s row=%d col=%d got=%.9g ref=%.9g\n", f16?"gemm_f16":"gemm_f32", r, j, out[j], s); failures++; }
        }
    }
}

static int64_t gemv_status(int64_t mode, uint8_t * w, int64_t wb, int64_t nrows, int64_t k, int64_t row) {
    gg_slice ws = gg_view(w, wb); gg_slice xs = gg_view(act, k*4); gg_slice ys = gg_view(out, 1);
    return gg_function_1(mode, ws, xs, ys, nrows, k, row, 0, 0);
}

static void check_dispatch(void) {
    struct { int kind, cc; long ne; long want; } cases[] = {
        {12, 800, 1, 1}, {12, 800, 8, 1}, {12, 800, 9, 0},
        {14, 890, 8, 1}, {14, 1200, 7, 1}, {14, 1200, 8, 0},
        {12, 1200, 5, 1}, {12, 1200, 6, 0}, {14, 870, 1, 1}, {14, 870, 2, 0},
        {12, 700, 5, 1}, {14, 700, 7, 1}, {12, 700, 6, 0},
    };
    for (unsigned i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        gg_slice z = gg_view(0, 0);
        int64_t got = gg_function_1(8, z, z, z, cases[i].kind, cases[i].cc, cases[i].ne, 0, 0);
        if (got != cases[i].want) { printf("FAIL mmvq_should_use kind=%d cc=%d ne=%ld got=%lld\n", cases[i].kind, cases[i].cc, cases[i].ne, (long long)got); failures++; }
    }
    int64_t nw = gg_function_1(9, gg_view(0,0), gg_view(0,0), gg_view(0,0), 12, 1, 0, 0, 0);
    if (nw != 4) { printf("FAIL mmvq_nwarps(1)=%lld\n", (long long)nw); failures++; }
    nw = gg_function_1(9, gg_view(0,0), gg_view(0,0), gg_view(0,0), 12, 8, 0, 0, 0);
    if (nw != 2) { printf("FAIL mmvq_nwarps(8)=%lld\n", (long long)nw); failures++; }

    for (int kind = 12; kind <= 14; kind += 2) {
        for (int J = 8; J <= 128; J += 8) {
            memset(out, 0, sizeof(out));
            gg_slice z = gg_view(0,0);
            int64_t ok = gg_function_1(10, z, z, gg_view(out, 10), kind, J, 0, 0, 0);
            if (ok != 1) { printf("FAIL mmq_config kind=%d J=%d\n", kind, J); failures++; continue; }
            long layout = kind == 12 ? 1 : 4;
            long stride = kind == 12 ? 84 : 76;
            long rpw = (J >= 48 && J % 16 == 0) ? 32 : 16;
            long want[10] = {256,1,128,J,layout,256,1,rpw,stride,0};
            for (int i = 0; i < 10; i++) if ((long)out[i] != want[i]) { printf("FAIL mmq_config kind=%d J=%d field=%d got=%ld want=%ld\n", kind, J, i, (long)out[i], want[i]); failures++; }
            memset(out, 0, sizeof(out));
            ok = gg_function_1(11, z, z, gg_view(out, 3), kind, 128, 0, 0, 0);
            long qs, df, sc;
            if (kind == 12) { qs = 128*32+128; df = 128; sc = 128*32/8+128/8; }
            else { qs = 128*64+128; df = 128+128/32; sc = 128*32/8+128/8; }
            if (ok != 1 || (long)out[0] != qs || (long)out[1] != df || (long)out[2] != sc) {
                printf("FAIL mmq_tile_x_sizes kind=%d got=%ld,%ld,%ld want=%ld,%ld,%ld\n", kind, (long)out[0], (long)out[1], (long)out[2], qs, df, sc); failures++;
            }
        }
    }
    if (gg_function_1(12, gg_view(0,0), gg_view(0,0), gg_view(0,0), 127, 0, 0, 0, 0) != 1) { printf("FAIL mmq_needs_fallback(127)\n"); failures++; }
    if (gg_function_1(12, gg_view(0,0), gg_view(0,0), gg_view(0,0), 128, 0, 0, 0, 0) != 0) { printf("FAIL mmq_needs_fallback(128)\n"); failures++; }
}

int main(void) {
    make_q4(); make_q6(); make_act();
    check_dequant(4, w4, sizeof(w4));
    check_dequant(6, w6, sizeof(w6));
    check_dot(4, w4, sizeof(w4));
    check_dot(6, w6, sizeof(w6));
    check_gemv(0); check_gemv(1);
    check_gemm(0); check_gemm(1);
    check_dispatch();

    uint64_t total = 0;
    for (int i = 0; i < 64; i++) total += atomic_load(&fgm_calls[i]);
    if (total < 20) { printf("FAIL witness total=%llu\n", (unsigned long long)total); failures++; }

    printf("{\"status\":\"%s\",\"failures\":%d,\"witness\":%llu,\"dequant\":\"exact\",\"dot_tol\":2e-4}\n",
           failures ? "FAIL" : "PASS", failures, (unsigned long long)total);
    return failures ? 1 : 0;
}
