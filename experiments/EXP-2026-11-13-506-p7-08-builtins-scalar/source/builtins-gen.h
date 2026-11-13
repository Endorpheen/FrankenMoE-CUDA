#pragma once
#include <stdint.h>
typedef struct gs_stack gs_stack;
#pragma pack(push, 1)
typedef struct { uint8_t *base; int64_t len; } gs_rhdr;
typedef struct { gs_rhdr *hdr; gs_stack *stk; } gs_rref;
typedef struct { gs_rhdr *hdr; gs_stack *stk; gs_rhdr *fl; gs_stack *flstk; } gs_pref;

typedef struct { double e[23]; } a23_f64;

typedef struct { uint8_t *data; int64_t len; } sl_u8;

typedef struct { int64_t *data; int64_t len; } sl_i64;

typedef struct { double *data; int64_t len; } sl_f64;

typedef struct { uint8_t e[24]; } a24_u8;

typedef struct { uint8_t e[64]; } a64_u8;

typedef struct { uint8_t e[96]; } a96_u8;

typedef struct { uint8_t e[4096]; } a4096_u8;

typedef struct { uint8_t e[256]; } a256_u8;

typedef struct { uint8_t e[65536]; } a65536_u8;

typedef struct { uint8_t e[131072]; } a131072_u8;

typedef struct { uint8_t e[512]; } a512_u8;

typedef struct ByteSet_g ByteSet_g;
typedef struct { uint8_t e[256]; } a256_b;

typedef struct { uint8_t e[3]; } a3_u8;

typedef struct { uint8_t e[16]; } a16_u8;

struct ByteSet_g {
    a256_b members_g;
    uint8_t kind_g;
    uint8_t invert_g;
    a3_u8 bytes_g;
    a16_u8 lo_g;
    a16_u8 hi_g;
};

#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif
void goose_init(int argc, char **argv);
int64_t builtin_names(int64_t gs_export_arg0, sl_u8 gs_export_arg1);
int64_t builtin_call(int64_t gs_export_arg0_2, sl_u8 gs_export_arg1_2, int64_t gs_export_arg2, sl_i64 gs_export_arg3, int64_t gs_export_arg4, sl_i64 gs_export_arg5, sl_i64 gs_export_arg6, sl_i64 gs_export_arg7, int64_t gs_export_arg8, sl_i64 gs_export_arg9, sl_f64 gs_export_arg10, sl_u8 gs_export_arg11, sl_u8 gs_export_arg12, sl_u8 gs_export_arg13, sl_i64 gs_export_arg14);
#ifdef __cplusplus
}
#endif
