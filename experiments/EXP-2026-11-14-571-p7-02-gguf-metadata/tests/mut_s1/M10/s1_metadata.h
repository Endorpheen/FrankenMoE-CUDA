#pragma once
#include <stdint.h>
typedef struct gs_stack gs_stack;
#pragma pack(push, 1)
typedef struct { uint8_t *base; int64_t len; } gs_rhdr;
typedef struct { gs_rhdr *hdr; gs_stack *stk; } gs_rref;
typedef struct { gs_rhdr *hdr; gs_stack *stk; gs_rhdr *fl; gs_stack *flstk; } gs_pref;

typedef struct { int64_t *data; int64_t len; } sl_i64;

typedef struct { uint8_t *data; int64_t len; } sl_u8;

#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif
void goose_init(int argc, char **argv);
int64_t s1_init(sl_i64 gs_export_arg0, sl_i64 gs_export_arg1, sl_i64 gs_export_arg2, sl_i64 gs_export_arg3, sl_i64 gs_export_arg4, sl_i64 gs_export_arg5, sl_i64 gs_export_arg6, sl_i64 gs_export_arg7);
int64_t s1_set_val(sl_i64 gs_export_arg0_2, sl_u8 gs_export_arg1_2, sl_i64 gs_export_arg2_2, sl_i64 gs_export_arg3_2, sl_i64 gs_export_arg4_2, sl_i64 gs_export_arg5_2, sl_i64 gs_export_arg6_2, sl_i64 gs_export_arg7_2, sl_i64 gs_export_arg8, sl_u8 gs_export_arg9, int64_t gs_export_arg10, int64_t gs_export_arg11, sl_i64 gs_export_arg12);
int64_t s1_set_str(sl_i64 gs_export_arg0_3, sl_u8 gs_export_arg1_3, sl_i64 gs_export_arg2_3, sl_i64 gs_export_arg3_3, sl_i64 gs_export_arg4_3, sl_i64 gs_export_arg5_3, sl_i64 gs_export_arg6_3, sl_i64 gs_export_arg7_3, sl_i64 gs_export_arg8_2, sl_u8 gs_export_arg9_2, sl_u8 gs_export_arg10_2, sl_i64 gs_export_arg11_2);
int64_t s1_set_arr(sl_i64 gs_export_arg0_4, sl_u8 gs_export_arg1_4, sl_i64 gs_export_arg2_4, sl_i64 gs_export_arg3_4, sl_i64 gs_export_arg4_4, sl_i64 gs_export_arg5_4, sl_i64 gs_export_arg6_4, sl_i64 gs_export_arg7_4, sl_i64 gs_export_arg8_3, sl_u8 gs_export_arg9_3, int64_t gs_export_arg10_3, sl_u8 gs_export_arg11_3, int64_t gs_export_arg12_2, sl_i64 gs_export_arg13);
int64_t s1_set_arr_str(sl_i64 gs_export_arg0_5, sl_u8 gs_export_arg1_5, sl_i64 gs_export_arg2_5, sl_i64 gs_export_arg3_5, sl_i64 gs_export_arg4_5, sl_i64 gs_export_arg5_5, sl_i64 gs_export_arg6_5, sl_i64 gs_export_arg7_5, sl_i64 gs_export_arg8_4, sl_u8 gs_export_arg9_4, sl_u8 gs_export_arg10_4, int64_t gs_export_arg11_4, sl_i64 gs_export_arg12_3);
int64_t s1_remove(sl_i64 gs_export_arg0_6, sl_u8 gs_export_arg1_6, sl_i64 gs_export_arg2_6, sl_i64 gs_export_arg3_6, sl_i64 gs_export_arg4_6, sl_i64 gs_export_arg5_6, sl_i64 gs_export_arg6_6, sl_i64 gs_export_arg7_6, sl_i64 gs_export_arg8_5, sl_u8 gs_export_arg9_5);
int64_t s1_set_kv(sl_i64 gs_export_arg0_7, sl_u8 gs_export_arg1_7, sl_i64 gs_export_arg2_7, sl_i64 gs_export_arg3_7, sl_i64 gs_export_arg4_7, sl_i64 gs_export_arg5_7, sl_i64 gs_export_arg6_7, sl_i64 gs_export_arg7_7, sl_i64 gs_export_arg8_6, sl_i64 gs_export_arg9_6, sl_u8 gs_export_arg10_5, sl_i64 gs_export_arg11_5, sl_i64 gs_export_arg12_4, sl_i64 gs_export_arg13_2, sl_i64 gs_export_arg14, sl_i64 gs_export_arg15, sl_i64 gs_export_arg16, sl_i64 gs_export_arg17, sl_i64 gs_export_arg18);
#ifdef __cplusplus
}
#endif
