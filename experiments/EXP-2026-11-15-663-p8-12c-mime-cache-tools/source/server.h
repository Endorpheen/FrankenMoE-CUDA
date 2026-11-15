#pragma once
#include <stdint.h>
typedef struct gs_stack gs_stack;
#pragma pack(push, 1)
typedef struct { uint8_t *base; int64_t len; } gs_rhdr;
typedef struct { gs_rhdr *hdr; gs_stack *stk; } gs_rref;
typedef struct { gs_rhdr *hdr; gs_stack *stk; gs_rhdr *fl; gs_stack *flstk; } gs_pref;

typedef struct { double e[23]; } a23_f64;

typedef struct { uint8_t *data; int64_t len; } sl_u8;

typedef struct { sl_u8 e[12]; } a12_sl_u8;

typedef struct { int64_t e[12]; } a12_i64;

typedef struct { int64_t *data; int64_t len; } sl_i64;

typedef struct { double *data; int64_t len; } sl_f64;

typedef struct TPNode_g TPNode_g;
struct TPNode_g {
    sl_i64 jk_g;
    sl_i64 jv_g;
    sl_i64 js_g;
    sl_i64 jl_g;
    sl_i64 jf_g;
    sl_i64 jn_g;
    sl_i64 jkey_g;
    sl_i64 jkl_g;
    sl_i64 jchild_g;
    sl_u8 sbuf_g;
};

typedef struct { int64_t e[64]; } a64_i64;

typedef struct { uint8_t e[32]; } a32_u8;

typedef struct { uint8_t e[24]; } a24_u8;

typedef struct { double e[64]; } a64_f64;

typedef struct { uint8_t e[8192]; } a8192_u8;

typedef struct { int64_t e[16]; } a16_i64;

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
int64_t mg_status_str(int64_t gs_export_arg0, sl_u8 gs_export_arg1);
int64_t mg_source_str(int64_t gs_export_arg0_2, sl_u8 gs_export_arg1_2);
int64_t mg_pred(int64_t gs_export_arg0_3, int64_t gs_export_arg1_3, sl_i64 gs_export_arg2);
int64_t mg_unload_action(int64_t gs_export_arg0_4, int64_t gs_export_arg1_4, sl_i64 gs_export_arg2_2);
int64_t mg_remove_gate(int64_t gs_export_arg0_5, int64_t gs_export_arg1_5, sl_i64 gs_export_arg2_3);
int64_t mg_stop_gate(int64_t gs_export_arg0_6, int64_t gs_export_arg1_6);
int64_t mg_wait_pred(int64_t gs_export_arg0_7, int64_t gs_export_arg1_7);
int64_t mg_update_fields(int64_t gs_export_arg0_8, int64_t gs_export_arg1_8, int64_t gs_export_arg2_4, int64_t gs_export_arg3, int64_t gs_export_arg4, int64_t gs_export_arg5, sl_i64 gs_export_arg6);
int64_t mg_arch_take(int64_t gs_export_arg0_9, int64_t gs_export_arg1_9, int64_t gs_export_arg2_5);
int64_t mg_capacity(int64_t gs_export_arg0_10, int64_t gs_export_arg1_10);
int64_t mg_victim(int64_t gs_export_arg0_11, sl_u8 gs_export_arg1_11, sl_i64 gs_export_arg2_6, sl_i64 gs_export_arg3_2, sl_i64 gs_export_arg4_2, sl_i64 gs_export_arg5_2, sl_i64 gs_export_arg6_2, sl_i64 gs_export_arg7, sl_i64 gs_export_arg8);
int64_t mg_tick(int64_t gs_export_arg0_12, sl_u8 gs_export_arg1_12, sl_i64 gs_export_arg2_7, sl_i64 gs_export_arg3_3, sl_i64 gs_export_arg4_3, sl_i64 gs_export_arg5_3, sl_i64 gs_export_arg6_3, sl_i64 gs_export_arg7_2, int64_t gs_export_arg8_2, int64_t gs_export_arg9, sl_i64 gs_export_arg10, sl_i64 gs_export_arg11, sl_i64 gs_export_arg12);
int64_t mg_lru_log(int64_t gs_export_arg0_13, int64_t gs_export_arg1_13, sl_u8 gs_export_arg2_8, sl_u8 gs_export_arg3_4);
int64_t json_parse(sl_u8 gs_export_arg0_14, sl_i64 gs_export_arg1_14, sl_i64 gs_export_arg2_9, sl_f64 gs_export_arg3_5, sl_i64 gs_export_arg4_4, sl_i64 gs_export_arg5_4, sl_i64 gs_export_arg6_4, sl_i64 gs_export_arg7_3, sl_i64 gs_export_arg8_3, sl_i64 gs_export_arg9_2, sl_i64 gs_export_arg10_2, sl_i64 gs_export_arg11_2, sl_i64 gs_export_arg12_2, sl_u8 gs_export_arg13, sl_i64 gs_export_arg14, sl_i64 gs_export_arg15, sl_i64 gs_export_arg16, sl_i64 gs_export_arg17);
int64_t tool_name(int64_t gs_export_arg0_15, sl_u8 gs_export_arg1_15);
int64_t tool_index(sl_u8 gs_export_arg0_16);
int64_t tool_flags(int64_t gs_export_arg0_17);
int64_t tool_validate(int64_t gs_export_arg0_18, sl_u8 gs_export_arg1_16, sl_i64 gs_export_arg2_10, sl_u8 gs_export_arg3_6, sl_u8 gs_export_arg4_5);
int64_t tool_edit_check(sl_i64 gs_export_arg0_19, sl_u8 gs_export_arg1_17, sl_u8 gs_export_arg2_11, sl_u8 gs_export_arg3_7);
int64_t tool_read_text(sl_u8 gs_export_arg0_20, int64_t gs_export_arg1_18, int64_t gs_export_arg2_12, int64_t gs_export_arg3_8, sl_u8 gs_export_arg4_6);
int64_t tool_read_guard(int64_t gs_export_arg0_21, int64_t gs_export_arg1_19, int64_t gs_export_arg2_13, sl_u8 gs_export_arg3_9, int64_t gs_export_arg4_7, sl_u8 gs_export_arg5_5);
int64_t tool_literal_regex(sl_u8 gs_export_arg0_22, sl_u8 gs_export_arg1_20);
int64_t tool_exec_tail(sl_u8 gs_export_arg0_23, int64_t gs_export_arg1_21, int64_t gs_export_arg2_14, sl_u8 gs_export_arg3_10);
int64_t tool_json_error(sl_u8 gs_export_arg0_24, sl_u8 gs_export_arg1_22);
int64_t tool_json_plain(sl_u8 gs_export_arg0_25, sl_u8 gs_export_arg1_23);
int64_t tool_json_write(sl_u8 gs_export_arg0_26, int64_t gs_export_arg1_24, sl_u8 gs_export_arg2_15);
int64_t tool_json_edit(sl_u8 gs_export_arg0_27, int64_t gs_export_arg1_25, sl_u8 gs_export_arg2_16);
int64_t tool_json_info(sl_u8 gs_export_arg0_28, sl_u8 gs_export_arg1_26, sl_u8 gs_export_arg2_17);
int64_t tool_json_base64(int64_t gs_export_arg0_29, sl_u8 gs_export_arg1_27);
int64_t tool_glob_tail(int64_t gs_export_arg0_30, int64_t gs_export_arg1_28, int64_t gs_export_arg2_18, sl_u8 gs_export_arg3_11);
int64_t web_route(sl_u8 gs_export_arg0_31, sl_u8 gs_export_arg1_29, sl_u8 gs_export_arg2_19, sl_i64 gs_export_arg3_12, int64_t gs_export_arg4_8);
int64_t web_serve(sl_u8 gs_export_arg0_32, sl_u8 gs_export_arg1_30, sl_u8 gs_export_arg2_20, sl_i64 gs_export_arg3_13, sl_u8 gs_export_arg4_9, sl_i64 gs_export_arg5_6, int64_t gs_export_arg6_5, sl_u8 gs_export_arg7_4, sl_u8 gs_export_arg8_4, sl_i64 gs_export_arg9_3);
int64_t web_cache_text(int64_t gs_export_arg0_33, sl_u8 gs_export_arg1_31);
int64_t web_isolation_of(sl_u8 gs_export_arg0_34, sl_i64 gs_export_arg1_32, int64_t gs_export_arg2_21);
int64_t mime_of(sl_u8 gs_export_arg0_35);
int64_t mime_text(int64_t gs_export_arg0_36, sl_u8 gs_export_arg1_33);
int64_t mime_base(sl_u8 gs_export_arg0_37, sl_u8 gs_export_arg1_34);
int64_t mime_required(sl_u8 gs_export_arg0_38);
int64_t mime_required_label(int64_t gs_export_arg0_39, sl_u8 gs_export_arg1_35);
int64_t cors_is_localhost(sl_u8 gs_export_arg0_40, int64_t gs_export_arg1_36);
int64_t cors_allow_origin(sl_u8 gs_export_arg0_41, int64_t gs_export_arg1_37, sl_u8 gs_export_arg2_22, int64_t gs_export_arg3_14, sl_u8 gs_export_arg4_10, sl_i64 gs_export_arg5_7);
int64_t cors_is_preflight(sl_u8 gs_export_arg0_42);
int64_t cors_credentials_text(int64_t gs_export_arg0_43, sl_u8 gs_export_arg1_38);
int64_t cors_preflight_type(sl_u8 gs_export_arg0_44);
#ifdef __cplusplus
}
#endif
