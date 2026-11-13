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

typedef struct { int64_t e[1]; } a1_i64;

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
int64_t int_rule(int64_t gs_export_arg0, int64_t gs_export_arg1, sl_u8 gs_export_arg2, sl_u8 gs_export_arg3);
int64_t const_rule(sl_u8 gs_export_arg0_2, sl_u8 gs_export_arg1_2);
int64_t enum_rule(sl_u8 gs_export_arg0_3, sl_i64 gs_export_arg1_3, int64_t gs_export_arg2_2, sl_u8 gs_export_arg3_2);
#ifdef __cplusplus
}
#endif
