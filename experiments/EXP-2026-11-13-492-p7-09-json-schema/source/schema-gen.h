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

typedef struct { uint8_t e[1024]; } a1024_u8;

typedef struct { uint8_t e[256]; } a256_u8;

typedef struct { int64_t e[128]; } a128_i64;

typedef struct { int64_t e[256]; } a256_i64;

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
int64_t schema_build(int64_t gs_export_arg0, int64_t gs_export_arg1, sl_i64 gs_export_arg2, sl_i64 gs_export_arg3, sl_f64 gs_export_arg4, sl_i64 gs_export_arg5, sl_i64 gs_export_arg6, sl_i64 gs_export_arg7, sl_i64 gs_export_arg8, sl_u8 gs_export_arg9, sl_i64 gs_export_arg10, sl_i64 gs_export_arg11, sl_i64 gs_export_arg12, sl_i64 gs_export_arg13, sl_i64 gs_export_arg14, sl_i64 gs_export_arg15, sl_i64 gs_export_arg16, sl_i64 gs_export_arg17, sl_i64 gs_export_arg18, sl_i64 gs_export_arg19, sl_i64 gs_export_arg20, sl_i64 gs_export_arg21, sl_i64 gs_export_arg22, sl_i64 gs_export_arg23, sl_i64 gs_export_arg24, sl_i64 gs_export_arg25, sl_i64 gs_export_arg26, sl_i64 gs_export_arg27, sl_i64 gs_export_arg28, sl_i64 gs_export_arg29, sl_i64 gs_export_arg30, sl_i64 gs_export_arg31, sl_i64 gs_export_arg32, sl_i64 gs_export_arg33, sl_i64 gs_export_arg34, sl_i64 gs_export_arg35, sl_u8 gs_export_arg36, sl_u8 gs_export_arg37, sl_u8 gs_export_arg38, sl_i64 gs_export_arg39);
#ifdef __cplusplus
}
#endif
