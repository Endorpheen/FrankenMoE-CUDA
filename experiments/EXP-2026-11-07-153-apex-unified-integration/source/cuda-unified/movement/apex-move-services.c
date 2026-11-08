/* worker-10: host implementations of the typed Goose primitives.
 *
 * These are raw byte/scalar loads and stores plus IEEE half conversion.  No
 * index decomposition, loop, bound policy or quantization lives here.
 */
#include "apex-move-bridge.h"
#include "ggml.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int64_t g_witness[8];

static uint8_t * B(int64_t h) { return (uint8_t *) (uintptr_t) h; }

int64_t fg153_am_param(int64_t p, int64_t index) {
    if (p == 0 || index < 0 || index >= 32) {
        abort();
    }
    return ((const int64_t *) (uintptr_t) p)[index];
}

int64_t fg153_am_load_u8(int64_t base, int64_t off) {
    if (base == 0 || off < 0) abort();
    return B(base)[off];
}
int64_t fg153_am_load_u16(int64_t base, int64_t off) {
    if (base == 0 || off < 0) abort();
    uint16_t v;
    memcpy(&v, B(base) + off, sizeof(v));
    return v;
}
int64_t fg153_am_load_i32(int64_t base, int64_t off) {
    if (base == 0 || off < 0) abort();
    int32_t v;
    memcpy(&v, B(base) + off, sizeof(v));
    return v;
}
int64_t fg153_am_load_i64(int64_t base, int64_t off) {
    if (base == 0 || off < 0) abort();
    int64_t v;
    memcpy(&v, B(base) + off, sizeof(v));
    return v;
}
void fg153_am_store_u8(int64_t base, int64_t off, int64_t v) {
    if (base == 0 || off < 0) abort();
    B(base)[off] = (uint8_t) v;
}
void fg153_am_store_u16(int64_t base, int64_t off, int64_t v) {
    if (base == 0 || off < 0) abort();
    uint16_t x = (uint16_t) v;
    memcpy(B(base) + off, &x, sizeof(x));
}
void fg153_am_store_i32(int64_t base, int64_t off, int64_t v) {
    if (base == 0 || off < 0) abort();
    int32_t x = (int32_t) v;
    memcpy(B(base) + off, &x, sizeof(x));
}
void fg153_am_store_f32(int64_t base, int64_t off, float v) {
    if (base == 0 || off < 0) abort();
    memcpy(B(base) + off, &v, sizeof(v));
}
float fg153_am_load_f32(int64_t base, int64_t off) {
    if (base == 0 || off < 0) abort();
    float v;
    memcpy(&v, B(base) + off, sizeof(v));
    return v;
}
int64_t fg153_am_f16_bits(float v) {
    return (int64_t) ggml_fp32_to_fp16(v);
}
float fg153_am_f16_value(int64_t bits) {
    return ggml_fp16_to_fp32((ggml_fp16_t) (uint16_t) bits);
}
float fg153_am_fabs(float v) {
    return v < 0.0f ? -v : v;
}
int64_t fg153_am_trunc_i64(float v) {
    return (int64_t) v;
}

/* Host execution has no CUDA grid. Serial entries never call these. */
int64_t fg153_am_gid_x(void) { return 0; }
int64_t fg153_am_bid_x(void) { return 0; }
int64_t fg153_am_bid_y(void) { return 0; }
int64_t fg153_am_bid_z(void) { return 0; }
int64_t fg153_am_tid_x(void) { return 0; }
int64_t fg153_am_tid_y(void) { return 0; }
int64_t fg153_am_bdim_x(void) { return 1; }
int64_t fg153_am_gdim_y(void) { return 1; }
int64_t fg153_am_gdim_z(void) { return 1; }

void fg153_am_enter_copy_scalar(void) { g_witness[0]++; }
void fg153_am_enter_copy_contig(void) { g_witness[1]++; }
void fg153_am_enter_copy_blocks(void) { g_witness[2]++; }
void fg153_am_enter_transpose(void) { g_witness[3]++; }
void fg153_am_enter_getrows(void) { g_witness[4]++; }
void fg153_am_enter_setrows(void) { g_witness[5]++; }
void fg153_am_enter_q4_dequant(void) { g_witness[6]++; }

int64_t fg153_am_witness_count(int64_t kind) {
    if (kind < 0 || kind > 6) return 0;
    return g_witness[kind];
}

int fg153_apex_move_enabled(void) {
    static int cached = -1;
    if (cached < 0) {
        const char * v = getenv("FG153_APEX_MOVE");
        cached = !(v && v[0] == '0' && v[1] == '\0');
    }
    return cached;
}

static int p_ok(const fg153_am_params * p) {
    if (!p) return 0;
    if (p->v[FG153_AM_P_SRC] == 0 || p->v[FG153_AM_P_DST] == 0) return 0;
    for (int i = FG153_AM_P_NE00; i <= FG153_AM_P_NE03; ++i) if (p->v[i] < 0) return 0;
    for (int i = FG153_AM_P_NB00; i <= FG153_AM_P_NB03; ++i) if (p->v[i] < 0) return 0;
    for (int i = FG153_AM_P_NE10; i <= FG153_AM_P_NE13; ++i) if (p->v[i] < 0) return 0;
    for (int i = FG153_AM_P_NB10; i <= FG153_AM_P_NB13; ++i) if (p->v[i] < 0) return 0;
    if (p->v[FG153_AM_P_N] < 0) return 0;
    return 1;
}

int fg153_am_can_run(int64_t op, const fg153_am_params * p) {
    if (!p_ok(p)) return 0;
    const int64_t st = p->v[FG153_AM_P_ST];
    const int64_t dt = p->v[FG153_AM_P_DT];
    const int64_t ne00 = p->v[FG153_AM_P_NE00];
    const int64_t ne01 = p->v[FG153_AM_P_NE01];
    const int64_t ne02 = p->v[FG153_AM_P_NE02];
    const int64_t ne03 = p->v[FG153_AM_P_NE03];
    switch (op) {
        case FG153_AM_OP_COPY_SCALAR:
            if (!((st == GGML_TYPE_F32 && dt == GGML_TYPE_F32) ||
                  (st == GGML_TYPE_F16 && dt == GGML_TYPE_F16) ||
                  (st == GGML_TYPE_F32 && dt == GGML_TYPE_F16) ||
                  (st == GGML_TYPE_F16 && dt == GGML_TYPE_F32))) return 0;
            if (ne00 <= 0 || ne01 <= 0 || ne02 <= 0 || ne03 <= 0) return 0;
            if (p->v[FG153_AM_P_NE10] != ne00 || p->v[FG153_AM_P_NE11] != ne01 ||
                p->v[FG153_AM_P_NE12] != ne02 || p->v[FG153_AM_P_NE13] != ne03) return 0;
            return p->v[FG153_AM_P_N] == ne00 * ne01 * ne02 * ne03;
        case FG153_AM_OP_TRANSPOSE:
            if (ne00 != ne01) return 0;
            if (!((st == GGML_TYPE_F32 && dt == GGML_TYPE_F32) ||
                  (st == GGML_TYPE_F16 && dt == GGML_TYPE_F16) ||
                  (st == GGML_TYPE_F32 && dt == GGML_TYPE_F16) ||
                  (st == GGML_TYPE_F16 && dt == GGML_TYPE_F32))) return 0;
            return p->v[FG153_AM_P_N] == ne00 * ne01 * ne02 * ne03 &&
                   p->v[FG153_AM_P_NE13] == 1;
        case FG153_AM_OP_COPY_CONTIG:
            return ((st == GGML_TYPE_F32 && dt == GGML_TYPE_F32) ||
                    (st == GGML_TYPE_F16 && dt == GGML_TYPE_F16) ||
                    (st == GGML_TYPE_F32 && dt == GGML_TYPE_F16) ||
                    (st == GGML_TYPE_F16 && dt == GGML_TYPE_F32)) && p->v[FG153_AM_P_N] >= 0;
        case FG153_AM_OP_COPY_BLOCKS:
            if (ne00 % 32 != 0 || p->v[FG153_AM_P_N] % 32 != 0) return 0;
            if (!((st == GGML_TYPE_F32 && dt == GGML_TYPE_Q4_0) ||
                  (st == GGML_TYPE_Q4_0 && dt == GGML_TYPE_F32))) return 0;
            return p->v[FG153_AM_P_NE10] == ne00 && p->v[FG153_AM_P_NE11] == ne01 &&
                   p->v[FG153_AM_P_NE12] == ne02 && p->v[FG153_AM_P_NE13] == ne03;
        case FG153_AM_OP_GETROWS:
            if (ne00 <= 0 || ne01 <= 0 || ne02 <= 0 || ne03 <= 0) return 0;
            if (p->v[FG153_AM_P_NE13] != 1) return 0;
            if (!((st == GGML_TYPE_F32 || st == GGML_TYPE_F16 || st == GGML_TYPE_Q4_0) &&
                  (dt == GGML_TYPE_F32 || dt == GGML_TYPE_F16))) return 0;
            if (p->v[FG153_AM_P_IT] != GGML_TYPE_I32) return 0;
            if (st == GGML_TYPE_Q4_0 && ne00 % 32 != 0) return 0;
            if (p->v[FG153_AM_P_NE11] <= 0 || p->v[FG153_AM_P_NE12] <= 0) return 0;
            return p->v[FG153_AM_P_AUX0] == p->v[FG153_AM_P_NE10];
        case FG153_AM_OP_SETROWS:
            if (ne00 <= 0 || ne01 <= 0 || ne02 <= 0 || ne03 <= 0) return 0;
            if (!((st == GGML_TYPE_F32 && (dt == GGML_TYPE_F32 || dt == GGML_TYPE_F16 || dt == GGML_TYPE_Q4_0)) ||
                  (st == GGML_TYPE_F16 && dt == GGML_TYPE_F16))) return 0;
            if (p->v[FG153_AM_P_IT] != GGML_TYPE_I32 && p->v[FG153_AM_P_IT] != GGML_TYPE_I64) return 0;
            if (p->v[FG153_AM_P_NE11] <= 0 || p->v[FG153_AM_P_NE12] <= 0) return 0;
            if (dt == GGML_TYPE_Q4_0) {
                if (st != GGML_TYPE_F32 || ne00 % 32 != 0) return 0;
                if (p->v[FG153_AM_P_N] != ne00 * ne01 * ne02 * ne03 / 32) return 0;
            } else {
                if (p->v[FG153_AM_P_N] != ne00 * ne01 * ne02 * ne03) return 0;
            }
            return 1;
        case FG153_AM_OP_Q4_DEQUANT:
            return p->v[FG153_AM_P_N] >= 0;
        default:
            return 0;
    }
}
