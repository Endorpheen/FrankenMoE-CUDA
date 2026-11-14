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

typedef struct { uint8_t e[2048]; } a2048_u8;

typedef struct { int64_t e[16]; } a16_i64;

typedef struct { uint8_t e[8192]; } a8192_u8;

typedef struct { int64_t e[4]; } a4_i64;

typedef struct { int64_t e[1]; } a1_i64;

typedef struct { uint8_t e[1024]; } a1024_u8;

typedef struct { int64_t e[8]; } a8_i64;

typedef struct { int64_t e[9]; } a9_i64;

typedef struct { int64_t e[17]; } a17_i64;

typedef struct { int64_t e[2]; } a2_i64;

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
int64_t chat_probe_used(sl_i64 gs_export_arg0, sl_i64 gs_export_arg1, int64_t gs_export_arg2);
int64_t builtin_names(int64_t gs_export_arg0_2, sl_u8 gs_export_arg1_2);
int64_t builtin_call(int64_t gs_export_arg0_3, sl_u8 gs_export_arg1_3, int64_t gs_export_arg2_2, sl_i64 gs_export_arg3, int64_t gs_export_arg4, sl_i64 gs_export_arg5, sl_i64 gs_export_arg6, sl_i64 gs_export_arg7, int64_t gs_export_arg8, sl_i64 gs_export_arg9, sl_f64 gs_export_arg10, sl_u8 gs_export_arg11, sl_u8 gs_export_arg12, sl_u8 gs_export_arg13, sl_i64 gs_export_arg14);
int64_t eval_expr(int64_t gs_export_arg0_4, int64_t gs_export_arg1_4, sl_i64 gs_export_arg2_3, sl_i64 gs_export_arg3_2, sl_i64 gs_export_arg4_2, int64_t gs_export_arg5_2, sl_i64 gs_export_arg6_2, sl_f64 gs_export_arg7_2, sl_u8 gs_export_arg8_2, sl_u8 gs_export_arg9_2, sl_u8 gs_export_arg10_2, sl_i64 gs_export_arg11_2);
int64_t exec_program(int64_t gs_export_arg0_5, int64_t gs_export_arg1_5, sl_i64 gs_export_arg2_4, sl_i64 gs_export_arg3_3, sl_i64 gs_export_arg4_3, sl_i64 gs_export_arg5_3, sl_f64 gs_export_arg6_3, sl_u8 gs_export_arg7_3, sl_u8 gs_export_arg8_3, sl_u8 gs_export_arg9_3, sl_i64 gs_export_arg10_3);
int64_t chat_message_json(sl_i64 gs_export_arg0_6, sl_f64 gs_export_arg1_6, sl_u8 gs_export_arg2_5, sl_u8 gs_export_arg3_4, sl_i64 gs_export_arg4_4, sl_u8 gs_export_arg5_4, sl_u8 gs_export_arg6_4, sl_u8 gs_export_arg7_4);
int64_t chat_normalize(sl_i64 gs_export_arg0_7, sl_f64 gs_export_arg1_7, sl_u8 gs_export_arg2_6, sl_u8 gs_export_arg3_5, sl_i64 gs_export_arg4_5, sl_u8 gs_export_arg5_5, sl_i64 gs_export_arg6_5, sl_i64 gs_export_arg7_5, sl_i64 gs_export_arg8_4, sl_i64 gs_export_arg9_4, sl_i64 gs_export_arg10_4, sl_i64 gs_export_arg11_3, int64_t gs_export_arg12_2, int64_t gs_export_arg13_2, sl_u8 gs_export_arg14_2);
int64_t chat_probe_content_used(sl_i64 gs_export_arg0_8, sl_u8 gs_export_arg1_8, sl_i64 gs_export_arg2_7, int64_t gs_export_arg3_6);
int64_t chat_probe_fresh(sl_i64 gs_export_arg0_9, sl_f64 gs_export_arg1_9, sl_u8 gs_export_arg2_8, sl_i64 gs_export_arg3_7);
int64_t chat_probe_mask(sl_i64 gs_export_arg0_10, sl_i64 gs_export_arg1_10, int64_t gs_export_arg2_9);
int64_t chat_probe_content_mask(sl_i64 gs_export_arg0_11, sl_u8 gs_export_arg1_11, sl_i64 gs_export_arg2_10, int64_t gs_export_arg3_8);
int64_t chat_probe_tool_name_mask(sl_i64 gs_export_arg0_12, sl_u8 gs_export_arg1_12, sl_i64 gs_export_arg2_11, int64_t gs_export_arg3_9);
int64_t chat_probe_tool_calls_mask(sl_i64 gs_export_arg0_13, sl_u8 gs_export_arg1_13, sl_i64 gs_export_arg2_12, int64_t gs_export_arg3_10);
int64_t chat_probe_tool_arg_mask(sl_i64 gs_export_arg0_14, sl_u8 gs_export_arg1_14, sl_i64 gs_export_arg2_13, int64_t gs_export_arg3_11);
int64_t chat_probe_second_call_mask(sl_i64 gs_export_arg0_15, sl_u8 gs_export_arg1_15, sl_i64 gs_export_arg2_14, int64_t gs_export_arg3_12);
int64_t chat_concat_parts(sl_u8 gs_export_arg0_16, sl_i64 gs_export_arg1_16, sl_i64 gs_export_arg2_15, sl_i64 gs_export_arg3_13, sl_i64 gs_export_arg4_6, int64_t gs_export_arg5_6, sl_u8 gs_export_arg6_6);
int64_t chat_content_json(sl_u8 gs_export_arg0_17, sl_i64 gs_export_arg1_17, sl_i64 gs_export_arg2_16, sl_i64 gs_export_arg3_14, sl_i64 gs_export_arg4_7, int64_t gs_export_arg5_7, sl_u8 gs_export_arg6_7);
int64_t chat_message_full_json(sl_u8 gs_export_arg0_18, int64_t gs_export_arg1_18, int64_t gs_export_arg2_17, int64_t gs_export_arg3_15, int64_t gs_export_arg4_8, int64_t gs_export_arg5_8, int64_t gs_export_arg6_8, int64_t gs_export_arg7_6, int64_t gs_export_arg8_5, int64_t gs_export_arg9_5, int64_t gs_export_arg10_5, sl_i64 gs_export_arg11_4, sl_i64 gs_export_arg12_3, sl_i64 gs_export_arg13_3, sl_i64 gs_export_arg14_3, sl_i64 gs_export_arg15, sl_i64 gs_export_arg16, int64_t gs_export_arg17, sl_u8 gs_export_arg18);
int64_t chat_tools_json(sl_u8 gs_export_arg0_19, sl_i64 gs_export_arg1_19, sl_i64 gs_export_arg2_18, sl_i64 gs_export_arg3_16, sl_i64 gs_export_arg4_9, sl_i64 gs_export_arg5_9, sl_i64 gs_export_arg6_9, int64_t gs_export_arg7_7, sl_u8 gs_export_arg8_6);
int64_t chat_tool_parameters_json(sl_u8 gs_export_arg0_20, int64_t gs_export_arg1_20, int64_t gs_export_arg2_19, sl_u8 gs_export_arg3_17);
int64_t chat_tools_parse(sl_u8 gs_export_arg0_21, int64_t gs_export_arg1_21, sl_u8 gs_export_arg2_20, sl_i64 gs_export_arg3_18, sl_i64 gs_export_arg4_10, sl_i64 gs_export_arg5_10);
int64_t chat_tools_error(sl_u8 gs_export_arg0_22, int64_t gs_export_arg1_22, int64_t gs_export_arg2_21, int64_t gs_export_arg3_19, sl_u8 gs_export_arg4_11);
int64_t chat_contains_media(sl_u8 gs_export_arg0_23, sl_i64 gs_export_arg1_23, sl_i64 gs_export_arg2_22, int64_t gs_export_arg3_20);
int64_t chat_format_single(sl_u8 gs_export_arg0_24, sl_u8 gs_export_arg1_24, int64_t gs_export_arg2_23, sl_u8 gs_export_arg3_21);
int64_t chat_mark_value(sl_i64 gs_export_arg0_25, sl_i64 gs_export_arg1_25, int64_t gs_export_arg2_24, int64_t gs_export_arg3_22);
int64_t chat_input_parts(sl_i64 gs_export_arg0_26);
int64_t chat_strip_affix(sl_u8 gs_export_arg0_27, sl_u8 gs_export_arg1_26, sl_u8 gs_export_arg2_25, int64_t gs_export_arg3_23, int64_t gs_export_arg4_12, int64_t gs_export_arg5_11, sl_u8 gs_export_arg6_10);
int64_t chat_bases(sl_i64 gs_export_arg0_28, int64_t gs_export_arg1_27);
int64_t chat_parse_content_only(sl_u8 gs_export_arg0_29, sl_u8 gs_export_arg1_28, sl_u8 gs_export_arg2_26);
int64_t peg_rest(sl_i64 gs_export_arg0_30, sl_i64 gs_export_arg1_29, sl_i64 gs_export_arg2_27, sl_i64 gs_export_arg3_24, sl_i64 gs_export_arg4_13);
int64_t peg_end_node(sl_i64 gs_export_arg0_31, sl_i64 gs_export_arg1_30, sl_i64 gs_export_arg2_28, sl_i64 gs_export_arg3_25, sl_i64 gs_export_arg4_14);
int64_t peg_eps_node(sl_i64 gs_export_arg0_32, sl_i64 gs_export_arg1_31, sl_i64 gs_export_arg2_29, sl_i64 gs_export_arg3_26, sl_i64 gs_export_arg4_15);
int64_t peg_literal(sl_i64 gs_export_arg0_33, sl_i64 gs_export_arg1_32, sl_i64 gs_export_arg2_30, sl_i64 gs_export_arg3_27, sl_i64 gs_export_arg4_16, sl_i64 gs_export_arg5_12, sl_u8 gs_export_arg6_11, sl_i64 gs_export_arg7_8, sl_u8 gs_export_arg8_7);
int64_t peg_child_one(sl_i64 gs_export_arg0_34, sl_i64 gs_export_arg1_33, sl_i64 gs_export_arg2_31, sl_i64 gs_export_arg3_28, sl_i64 gs_export_arg4_17, sl_i64 gs_export_arg5_13, int64_t gs_export_arg6_12, int64_t gs_export_arg7_9, int64_t gs_export_arg8_8);
int64_t peg_tag(sl_i64 gs_export_arg0_35, sl_i64 gs_export_arg1_34, sl_i64 gs_export_arg2_32, sl_i64 gs_export_arg3_29, sl_i64 gs_export_arg4_18, sl_i64 gs_export_arg5_14, sl_i64 gs_export_arg6_13, sl_u8 gs_export_arg7_10, sl_i64 gs_export_arg8_9, sl_u8 gs_export_arg9_6, int64_t gs_export_arg10_6);
int64_t peg_atomic(sl_i64 gs_export_arg0_36, sl_i64 gs_export_arg1_35, sl_i64 gs_export_arg2_33, sl_i64 gs_export_arg3_30, sl_i64 gs_export_arg4_19, sl_i64 gs_export_arg5_15, int64_t gs_export_arg6_14);
int64_t peg_seq(sl_i64 gs_export_arg0_37, sl_i64 gs_export_arg1_36, sl_i64 gs_export_arg2_34, sl_i64 gs_export_arg3_31, sl_i64 gs_export_arg4_20, sl_i64 gs_export_arg5_16, sl_i64 gs_export_arg6_15);
int64_t peg_choice(sl_i64 gs_export_arg0_38, sl_i64 gs_export_arg1_37, sl_i64 gs_export_arg2_35, sl_i64 gs_export_arg3_32, sl_i64 gs_export_arg4_21, sl_i64 gs_export_arg5_17, sl_i64 gs_export_arg6_16);
int64_t peg_repeat(sl_i64 gs_export_arg0_39, sl_i64 gs_export_arg1_38, sl_i64 gs_export_arg2_36, sl_i64 gs_export_arg3_33, sl_i64 gs_export_arg4_22, sl_i64 gs_export_arg5_18, sl_i64 gs_export_arg6_17, sl_i64 gs_export_arg7_11, int64_t gs_export_arg8_10, int64_t gs_export_arg9_7, int64_t gs_export_arg10_7);
int64_t peg_dump_basic(int64_t gs_export_arg0_40, sl_i64 gs_export_arg1_39, sl_i64 gs_export_arg2_37, sl_i64 gs_export_arg3_34, sl_i64 gs_export_arg4_23, sl_i64 gs_export_arg5_19, sl_i64 gs_export_arg6_18, sl_i64 gs_export_arg7_12, sl_i64 gs_export_arg8_11, sl_i64 gs_export_arg9_8, sl_i64 gs_export_arg10_8, sl_i64 gs_export_arg11_5, sl_u8 gs_export_arg12_4, sl_u8 gs_export_arg13_4, sl_i64 gs_export_arg14_4);
int64_t peg_any_node(sl_i64 gs_export_arg0_41, sl_i64 gs_export_arg1_40, sl_i64 gs_export_arg2_38, sl_i64 gs_export_arg3_35, sl_i64 gs_export_arg4_24);
int64_t peg_not_node(sl_i64 gs_export_arg0_42, sl_i64 gs_export_arg1_41, sl_i64 gs_export_arg2_39, sl_i64 gs_export_arg3_36, sl_i64 gs_export_arg4_25, sl_i64 gs_export_arg5_20, int64_t gs_export_arg6_19);
int64_t peg_optspace(sl_i64 gs_export_arg0_43, sl_i64 gs_export_arg1_42, sl_i64 gs_export_arg2_40, sl_i64 gs_export_arg3_37, sl_i64 gs_export_arg4_26, sl_i64 gs_export_arg5_21, sl_i64 gs_export_arg6_20, sl_i64 gs_export_arg7_13, sl_i64 gs_export_arg8_12, sl_u8 gs_export_arg9_9, sl_i64 gs_export_arg10_9, sl_u8 gs_export_arg11_6);
int64_t peg_prefix(sl_i64 gs_export_arg0_44, sl_i64 gs_export_arg1_43, sl_i64 gs_export_arg2_41, sl_i64 gs_export_arg3_38, sl_i64 gs_export_arg4_27, sl_i64 gs_export_arg5_22, sl_u8 gs_export_arg6_21, sl_i64 gs_export_arg7_14, sl_u8 gs_export_arg8_13, sl_u8 gs_export_arg9_10);
int64_t peg_tag_safe(sl_i64 gs_export_arg0_45, sl_i64 gs_export_arg1_44, sl_i64 gs_export_arg2_42, sl_i64 gs_export_arg3_39, sl_i64 gs_export_arg4_28, sl_i64 gs_export_arg5_23, sl_i64 gs_export_arg6_22, sl_i64 gs_export_arg7_15, sl_i64 gs_export_arg8_14, sl_u8 gs_export_arg9_11, sl_i64 gs_export_arg10_10, sl_u8 gs_export_arg11_7, sl_u8 gs_export_arg12_5, int64_t gs_export_arg13_5);
int64_t peg_permute(sl_u8 gs_export_arg0_46, sl_i64 gs_export_arg1_45, int64_t gs_export_arg2_43, sl_i64 gs_export_arg3_40, sl_u8 gs_export_arg4_29, sl_i64 gs_export_arg5_24, sl_i64 gs_export_arg6_23, sl_i64 gs_export_arg7_16, sl_i64 gs_export_arg8_15, sl_i64 gs_export_arg9_12, sl_i64 gs_export_arg10_11, sl_i64 gs_export_arg11_8, sl_u8 gs_export_arg12_6, sl_i64 gs_export_arg13_6);
int64_t peg_map_msg(sl_i64 gs_export_arg0_47, int64_t gs_export_arg1_46, sl_u8 gs_export_arg2_44, sl_i64 gs_export_arg3_41, sl_i64 gs_export_arg4_30, sl_i64 gs_export_arg5_25, sl_i64 gs_export_arg6_24, sl_i64 gs_export_arg7_17, sl_i64 gs_export_arg8_16, sl_i64 gs_export_arg9_13, sl_i64 gs_export_arg10_12, sl_i64 gs_export_arg11_9, sl_i64 gs_export_arg12_7, sl_u8 gs_export_arg13_7, sl_u8 gs_export_arg14_5, sl_u8 gs_export_arg15_2, sl_i64 gs_export_arg16_2, sl_i64 gs_export_arg17_2);
int64_t peg_map_gemma4(sl_i64 gs_export_arg0_48, int64_t gs_export_arg1_47, sl_u8 gs_export_arg2_45, sl_i64 gs_export_arg3_42, sl_i64 gs_export_arg4_31, sl_i64 gs_export_arg5_26, sl_i64 gs_export_arg6_25, sl_i64 gs_export_arg7_18, sl_i64 gs_export_arg8_17, sl_i64 gs_export_arg9_14, sl_i64 gs_export_arg10_13, sl_i64 gs_export_arg11_10, sl_i64 gs_export_arg12_8, sl_i64 gs_export_arg13_8, sl_i64 gs_export_arg14_6, sl_i64 gs_export_arg15_3, sl_u8 gs_export_arg16_3, sl_u8 gs_export_arg17_3, sl_u8 gs_export_arg18_2, sl_i64 gs_export_arg19, sl_i64 gs_export_arg20);
int64_t peg_map_minimax(sl_i64 gs_export_arg0_49, int64_t gs_export_arg1_48, sl_u8 gs_export_arg2_46, sl_i64 gs_export_arg3_43, sl_i64 gs_export_arg4_32, sl_i64 gs_export_arg5_27, sl_i64 gs_export_arg6_26, sl_i64 gs_export_arg7_19, sl_i64 gs_export_arg8_18, sl_i64 gs_export_arg9_15, sl_i64 gs_export_arg10_14, sl_i64 gs_export_arg11_11, sl_i64 gs_export_arg12_9, sl_i64 gs_export_arg13_9, sl_u8 gs_export_arg14_7, sl_u8 gs_export_arg15_4, sl_u8 gs_export_arg16_4, sl_i64 gs_export_arg17_4, sl_i64 gs_export_arg18_3);
int64_t legacy_merge(sl_u8 gs_export_arg0_50, sl_u8 gs_export_arg1_49, sl_i64 gs_export_arg2_47, sl_i64 gs_export_arg3_44, int64_t gs_export_arg4_33, sl_u8 gs_export_arg5_28);
int64_t legacy_detect(sl_u8 gs_export_arg0_51);
int64_t legacy_apply(sl_u8 gs_export_arg0_52, sl_u8 gs_export_arg1_50, sl_i64 gs_export_arg2_48, sl_i64 gs_export_arg3_45, sl_u8 gs_export_arg4_34, sl_i64 gs_export_arg5_29, sl_i64 gs_export_arg6_27, int64_t gs_export_arg7_20, int64_t gs_export_arg8_19, sl_u8 gs_export_arg9_16, sl_i64 gs_export_arg10_15);
#ifdef __cplusplus
}
#endif
