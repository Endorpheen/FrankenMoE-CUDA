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
int64_t peg_parse_ast(int64_t gs_export_arg0, sl_u8 gs_export_arg1, int64_t gs_export_arg2, int64_t gs_export_arg3, sl_i64 gs_export_arg4, sl_i64 gs_export_arg5, sl_i64 gs_export_arg6, sl_i64 gs_export_arg7, sl_i64 gs_export_arg8, sl_i64 gs_export_arg9, sl_i64 gs_export_arg10, sl_u8 gs_export_arg11, sl_i64 gs_export_arg12, sl_i64 gs_export_arg13, sl_i64 gs_export_arg14, sl_i64 gs_export_arg15, sl_i64 gs_export_arg16, sl_i64 gs_export_arg17, sl_i64 gs_export_arg18, sl_i64 gs_export_arg19, sl_i64 gs_export_arg20, sl_i64 gs_export_arg21, sl_i64 gs_export_arg22, sl_i64 gs_export_arg23, sl_i64 gs_export_arg24, sl_i64 gs_export_arg25);
int64_t ast_dump(sl_u8 gs_export_arg0_2, sl_u8 gs_export_arg1_2, sl_i64 gs_export_arg2_2, sl_i64 gs_export_arg3_2, sl_i64 gs_export_arg4_2, sl_i64 gs_export_arg5_2, sl_i64 gs_export_arg6_2, sl_i64 gs_export_arg7_2, sl_i64 gs_export_arg8_2, sl_i64 gs_export_arg9_2, sl_i64 gs_export_arg10_2, sl_i64 gs_export_arg11_2, sl_i64 gs_export_arg12_2, int64_t gs_export_arg13_2, sl_u8 gs_export_arg14_2, sl_i64 gs_export_arg15_2);
#ifdef __cplusplus
}
#endif
