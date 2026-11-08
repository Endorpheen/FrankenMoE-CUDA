#ifndef FG153_GDN_ABI_H
#define FG153_GDN_ABI_H
#include <stdint.h>

/* Field indices for fg153_gdn_cfg.v.  Keep in sync with gdn_recurrent.goose. */
enum {
    FG153_GDN_Q = 0,
    FG153_GDN_K = 1,
    FG153_GDN_V = 2,
    FG153_GDN_G = 3,
    FG153_GDN_BETA = 4,
    FG153_GDN_STATE_IN = 5,
    FG153_GDN_DST = 6,
    FG153_GDN_STATE_OUT = 7,
    FG153_GDN_H = 8,
    FG153_GDN_NTOK = 9,
    FG153_GDN_NSEQ = 10,
    FG153_GDN_SQ1 = 11,
    FG153_GDN_SQ2 = 12,
    FG153_GDN_SQ3 = 13,
    FG153_GDN_SV1 = 14,
    FG153_GDN_SV2 = 15,
    FG153_GDN_SV3 = 16,
    FG153_GDN_SB1 = 17,
    FG153_GDN_SB2 = 18,
    FG153_GDN_SB3 = 19,
    FG153_GDN_NEQK1 = 20,
    FG153_GDN_RQ3 = 21,
    FG153_GDN_STATE_SLOT_STRIDE = 22,
    FG153_GDN_K_SLOTS = 23,
    FG153_GDN_KDA = 24,
    FG153_GDN_KEEP_RS = 25,
    FG153_GDN_COLS_PER_CTA = 26,
    FG153_GDN_ERROR = 27,
    FG153_GDN_SV = 28,
    FG153_GDN_NI64 = 32
};

typedef struct {
    int64_t v[FG153_GDN_NI64];
    float   f[8];
} fg153_gdn_cfg;

/* Field indices for fg153_conv_cfg.v.  Keep in sync with ssm_conv.goose. */
enum {
    FG153_CONV_X = 0,
    FG153_CONV_W = 1,
    FG153_CONV_BIAS = 2,
    FG153_CONV_Y = 3,
    FG153_CONV_NC = 4,
    FG153_CONV_NR = 5,
    FG153_CONV_NT = 6,
    FG153_CONV_NS = 7,
    FG153_CONV_XSC = 8,
    FG153_CONV_XSS = 9,
    FG153_CONV_WSC = 10,
    FG153_CONV_YSC = 11,
    FG153_CONV_YST = 12,
    FG153_CONV_YSS = 13,
    FG153_CONV_SILU = 14,
    FG153_CONV_SPLIT = 15,
    FG153_CONV_MODE = 16,
    FG153_CONV_NTHREADS = 17,
    FG153_CONV_SMEM_CAP = 18,
    FG153_CONV_ERROR = 19,
    FG153_CONV_NI64 = 24
};

typedef struct {
    int64_t v[FG153_CONV_NI64];
} fg153_conv_cfg;

#endif
