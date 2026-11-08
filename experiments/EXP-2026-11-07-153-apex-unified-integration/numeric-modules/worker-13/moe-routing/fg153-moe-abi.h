#ifndef FG153_MOE_ABI_H
#define FG153_MOE_ABI_H
#include <stdint.h>

/* argsort / top-k phase config.  Indices are shared with argsort_moe.goose. */
enum {
    FG153_MOE_X = 0,
    FG153_MOE_DST = 1,
    FG153_MOE_NCOLS = 2,
    FG153_MOE_NPAD = 3,
    FG153_MOE_ROWS = 4,
    FG153_MOE_OUTCOLS = 5,
    FG153_MOE_ORDER = 6,
    FG153_MOE_PHASE = 7,
    FG153_MOE_KK = 8,
    FG153_MOE_JJ = 9,
    FG153_MOE_ERROR = 10,
    FG153_MOE_KIND = 11,
    FG153_MOE_DSTSTRIDE = 12,
    FG153_MOE_NI64 = 16
};

typedef struct {
    int64_t v[FG153_MOE_NI64];
} fg153_moe_sort_cfg;

/* weighted expert-output reduction config. */
enum {
    FG153_MR_EXPERTS = 0,
    FG153_MR_SCALE = 1,
    FG153_MR_WEIGHTS = 2,
    FG153_MR_DST = 3,
    FG153_MR_NEMBD = 4,
    FG153_MR_NTOK = 5,
    FG153_MR_NUSED = 6,
    FG153_MR_ERROR = 7,
    FG153_MR_NI64 = 8
};

typedef struct {
    int64_t v[FG153_MR_NI64];
} fg153_moe_reduce_cfg;

#endif
