#define GGML_COMMON_DECL_CPP
#include "ggml-cpu.h"
#include "ggml-cpu/repack.h"
#include "ggml-cpu/quants.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cassert>

static uint32_t rng = 20261107u;
static uint32_t next32() { rng = rng * 1664525u + 1013904223u; return rng; }
static float nextf() { return ((int32_t)(next32() >> 8) - 8388608) * (1.0f / 8388608.0f); }
static int bit_eq(float a, float b) { uint32_t x, y; memcpy(&x, &a, 4); memcpy(&y, &b, 4); return x == y; }

static block_q4_Kx8 make_block(const block_q4_K * in8, int interleave) {
    block_q4_Kx8 out;
    for (int i = 0; i < 8; i++) out.d[i] = in8[i].GGML_COMMON_AGGR_U.GGML_COMMON_AGGR_S.d;
    for (int i = 0; i < 8; i++) out.dmin[i] = in8[i].GGML_COMMON_AGGR_U.GGML_COMMON_AGGR_S.dmin;
    const int end = QK_K * 4 / interleave;
    for (int i = 0; i < end; ++i) {
        int src_id = i % 8;
        int src_offset = (i / 8) * interleave;
        int dst_offset = i * interleave;
        uint64_t elems = 0;
        memcpy(&elems, &in8[src_id].qs[src_offset], interleave);
        memcpy(&out.qs[dst_offset], &elems, interleave);
    }
    uint8_t s[8], m[8];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 8; j++) { s[j] = in8[j].scales[i] & 63; m[j] = in8[j].scales[i + 4] & 63; }
        out.scales[i*12+0]=(s[0]&63)+((s[4]&48)<<2); out.scales[i*12+1]=(s[1]&63)+((s[5]&48)<<2);
        out.scales[i*12+2]=(s[2]&63)+((s[6]&48)<<2); out.scales[i*12+3]=(s[3]&63)+((s[7]&48)<<2);
        out.scales[i*12+4]=(m[0]&63)+((m[4]&48)<<2); out.scales[i*12+5]=(m[1]&63)+((m[5]&48)<<2);
        out.scales[i*12+6]=(m[2]&63)+((m[6]&48)<<2); out.scales[i*12+7]=(m[3]&63)+((m[7]&48)<<2);
        out.scales[i*12+8]=(s[4]&15)+((m[4]&15)<<4); out.scales[i*12+9]=(s[5]&15)+((m[5]&15)<<4);
        out.scales[i*12+10]=(s[6]&15)+((m[6]&15)<<4); out.scales[i*12+11]=(s[7]&15)+((m[7]&15)<<4);
    }
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 8; j++) { s[j]=((in8[j].scales[i]&192)>>2)|(in8[j].scales[i+8]&15); m[j]=((in8[j].scales[i+4]&192)>>2)|((in8[j].scales[i+8]&240)>>4); }
        out.scales[i*12+48]=(s[0]&63)+((s[4]&48)<<2); out.scales[i*12+49]=(s[1]&63)+((s[5]&48)<<2);
        out.scales[i*12+50]=(s[2]&63)+((s[6]&48)<<2); out.scales[i*12+51]=(s[3]&63)+((s[7]&48)<<2);
        out.scales[i*12+52]=(m[0]&63)+((m[4]&48)<<2); out.scales[i*12+53]=(m[1]&63)+((m[5]&48)<<2);
        out.scales[i*12+54]=(m[2]&63)+((m[6]&48)<<2); out.scales[i*12+55]=(m[3]&63)+((m[7]&48)<<2);
        out.scales[i*12+56]=(s[4]&15)+((m[4]&15)<<4); out.scales[i*12+57]=(s[5]&15)+((m[5]&15)<<4);
        out.scales[i*12+58]=(s[6]&15)+((m[6]&15)<<4); out.scales[i*12+59]=(s[7]&15)+((m[7]&15)<<4);
    }
    return out;
}

int main(int argc, char ** argv) {
    ggml_cpu_init();
    const int K = 512, M = 8, nb = K / 256;
    float wrow[8][K];
    block_q4_K raw[8][2];
    block_q4_Kx8 rep[2];
    for (int r = 0; r < 8; ++r) {
        for (int i = 0; i < K; ++i) wrow[r][i] = nextf() * (1.0f + (i % 7));
        quantize_row_q4_K(wrow[r], raw[r], K);
    }
    for (int b = 0; b < nb; ++b) {
        block_q4_K tmp[8];
        for (int r = 0; r < 8; ++r) tmp[r] = raw[r][b];
        rep[b] = make_block(tmp, 8);
    }
    float act[K];
    for (int i = 0; i < K; ++i) act[i] = nextf() * 3.0f;
    block_q8_K q8k[2];
    quantize_row_q8_K(act, q8k, K);
    float a[8], b[8];
    ggml_gemv_q4_K_8x8_q8_K(K, a, 8, rep, q8k, 1, 8);
    ggml_gemv_q4_K_8x8_q8_K_generic(K, b, 8, rep, q8k, 1, 8);
    int gemv_diff = 0;
    for (int i = 0; i < 8; ++i) if (!bit_eq(a[i], b[i])) { gemv_diff++; fprintf(stderr, "gemv[%d] %a %a\n", i, a[i], b[i]); }

    float act4[4][K];
    for (int r = 0; r < 4; ++r) for (int i = 0; i < K; ++i) act4[r][i] = nextf() * 3.0f;
    block_q8_Kx4 q8k4[2];
    ggml_quantize_mat_q8_K_4x8(&act4[0][0], q8k4, K);
    float c[4][8], d[4][8];
    ggml_gemm_q4_K_8x8_q8_K(K, &c[0][0], 8, rep, q8k4, 4, 8);
    ggml_gemm_q4_K_8x8_q8_K_generic(K, &d[0][0], 8, rep, q8k4, 4, 8);
    int gemm_diff = 0;
    for (int m = 0; m < 4; ++m) for (int j = 0; j < 8; ++j)
        if (!bit_eq(c[m][j], d[m][j])) { gemm_diff++; if (gemm_diff < 9) fprintf(stderr, "gemm[%d][%d] %a %a\n", m, j, c[m][j], d[m][j]); }
    FILE * out = fopen(argc > 1 ? argv[1] : "probe-repack.json", "wb");
    if (out) {
        fprintf(out, "{\"profile\":\"Q4_K_M+5950X\",\"gemv_diff\":%d,\"gemv_total\":8,"
                "\"gemm_diff\":%d,\"gemm_total\":32,\"native_avx2_vs_generic\":%s,"
                "\"status\":\"ACTIVE_GAP\"}\n",
                gemv_diff, gemm_diff, (gemv_diff || gemm_diff) ? "false" : "true");
        fclose(out);
    }
    printf("{\"gemv_diff\":%d,\"gemm_diff\":%d,\"status\":\"ACTIVE_GAP\"}\n", gemv_diff, gemm_diff);
    return 0;
}
