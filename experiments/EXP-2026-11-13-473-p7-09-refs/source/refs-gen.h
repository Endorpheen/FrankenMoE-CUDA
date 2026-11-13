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
int64_t sanitize_into(sl_u8 gs_export_arg0, int64_t gs_export_arg1, sl_u8 gs_export_arg2, int64_t gs_export_arg3);
int64_t sanitize(sl_u8 gs_export_arg0_2, sl_u8 gs_export_arg1_2);
int64_t ref_cut(sl_u8 gs_export_arg0_3);
int64_t ref_name(sl_u8 gs_export_arg0_4, sl_u8 gs_export_arg1_3);
int64_t rule_name(sl_u8 gs_export_arg0_5, sl_u8 gs_export_arg1_4);
int64_t sub_name(sl_u8 gs_export_arg0_6, sl_u8 gs_export_arg1_5);
int64_t add_rule(sl_u8 gs_export_arg0_7, int64_t gs_export_arg1_6, sl_u8 gs_export_arg2_2, sl_i64 gs_export_arg3_2, sl_i64 gs_export_arg4, sl_u8 gs_export_arg5);
int64_t resolve_action(int64_t gs_export_arg0_8, int64_t gs_export_arg1_7, int64_t gs_export_arg2_3, sl_u8 gs_export_arg3_3);
#ifdef __cplusplus
}
#endif
