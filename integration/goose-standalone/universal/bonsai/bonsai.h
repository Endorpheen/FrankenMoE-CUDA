#pragma once
#include <stdint.h>
typedef struct gs_stack gs_stack;
typedef struct { void *data; int64_t len; } gg_slice;
#pragma pack(push, 1)
typedef struct { uint8_t *data; int64_t len; } sl_u8;
typedef struct { int64_t *data; int64_t len; } sl_i64;
typedef struct { float *data; int64_t len; } sl_f32;
typedef struct { int32_t *data; int64_t len; } sl_i32;
#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif
void goose_init(int argc, char **argv);
int64_t ggh_const_count(void);
int64_t ggh_const_name(int64_t gs_export_arg0, sl_u8 gs_export_arg1);
int64_t ggh_const_value(int64_t gs_export_arg0);
int64_t ggh_type_count(void);
int64_t ggh_type_value(int64_t gs_export_arg0);
int64_t ggh_type_name(int64_t gs_export_arg0, sl_u8 gs_export_arg1);
int64_t ggh_type_size(int64_t gs_export_arg0);
int64_t ggh_key_general_alignment(sl_u8 gs_export_arg0);
int64_t ggh_alignment_valid(int64_t gs_export_arg0);
int64_t ggh_data_offset(int64_t gs_export_arg0, int64_t gs_export_arg1, int64_t gs_export_arg2);
int64_t ggh_reader_calls(int64_t gs_export_arg0, int64_t gs_export_arg1);
int64_t ggh_reader_chunk(int64_t gs_export_arg0, int64_t gs_export_arg1, int64_t gs_export_arg2);
int64_t ggh_reader_continues(int64_t gs_export_arg0, int64_t gs_export_arg1);
int64_t s1_init(sl_i64 gs_export_arg0, sl_i64 gs_export_arg1, sl_i64 gs_export_arg2, sl_i64 gs_export_arg3, sl_i64 gs_export_arg4, sl_i64 gs_export_arg5, sl_i64 gs_export_arg6, sl_i64 gs_export_arg7);
int64_t s1_set_val(sl_i64 gs_export_arg0, sl_u8 gs_export_arg1, sl_i64 gs_export_arg2, sl_i64 gs_export_arg3, sl_i64 gs_export_arg4, sl_i64 gs_export_arg5, sl_i64 gs_export_arg6, sl_i64 gs_export_arg7, sl_i64 gs_export_arg8, sl_u8 gs_export_arg9, int64_t gs_export_arg10, int64_t gs_export_arg11, sl_i64 gs_export_arg12);
int64_t s1_set_str(sl_i64 gs_export_arg0, sl_u8 gs_export_arg1, sl_i64 gs_export_arg2, sl_i64 gs_export_arg3, sl_i64 gs_export_arg4, sl_i64 gs_export_arg5, sl_i64 gs_export_arg6, sl_i64 gs_export_arg7, sl_i64 gs_export_arg8, sl_u8 gs_export_arg9, sl_u8 gs_export_arg10, sl_i64 gs_export_arg11);
int64_t s1_set_arr(sl_i64 gs_export_arg0, sl_u8 gs_export_arg1, sl_i64 gs_export_arg2, sl_i64 gs_export_arg3, sl_i64 gs_export_arg4, sl_i64 gs_export_arg5, sl_i64 gs_export_arg6, sl_i64 gs_export_arg7, sl_i64 gs_export_arg8, sl_u8 gs_export_arg9, int64_t gs_export_arg10, sl_u8 gs_export_arg11, int64_t gs_export_arg12, sl_i64 gs_export_arg13);
int64_t s1_set_arr_str(sl_i64 gs_export_arg0, sl_u8 gs_export_arg1, sl_i64 gs_export_arg2, sl_i64 gs_export_arg3, sl_i64 gs_export_arg4, sl_i64 gs_export_arg5, sl_i64 gs_export_arg6, sl_i64 gs_export_arg7, sl_i64 gs_export_arg8, sl_u8 gs_export_arg9, sl_u8 gs_export_arg10, int64_t gs_export_arg11, sl_i64 gs_export_arg12);
int64_t s1_remove(sl_i64 gs_export_arg0, sl_u8 gs_export_arg1, sl_i64 gs_export_arg2, sl_i64 gs_export_arg3, sl_i64 gs_export_arg4, sl_i64 gs_export_arg5, sl_i64 gs_export_arg6, sl_i64 gs_export_arg7, sl_i64 gs_export_arg8, sl_u8 gs_export_arg9);
int64_t s1_set_kv(sl_i64 gs_export_arg0, sl_u8 gs_export_arg1, sl_i64 gs_export_arg2, sl_i64 gs_export_arg3, sl_i64 gs_export_arg4, sl_i64 gs_export_arg5, sl_i64 gs_export_arg6, sl_i64 gs_export_arg7, sl_i64 gs_export_arg8, sl_i64 gs_export_arg9, sl_u8 gs_export_arg10, sl_i64 gs_export_arg11, sl_i64 gs_export_arg12, sl_i64 gs_export_arg13, sl_i64 gs_export_arg14, sl_i64 gs_export_arg15, sl_i64 gs_export_arg16, sl_i64 gs_export_arg17, sl_i64 gs_export_arg18);
int64_t s2_write(sl_i64 gs_export_arg0, sl_u8 gs_export_arg1, sl_i64 gs_export_arg2, sl_i64 gs_export_arg3, sl_i64 gs_export_arg4, sl_i64 gs_export_arg5, sl_i64 gs_export_arg6, sl_i64 gs_export_arg7, sl_i64 gs_export_arg8, sl_u8 gs_export_arg9, int64_t gs_export_arg10, int64_t gs_export_arg11, int64_t gs_export_arg12);
int64_t s3_init_tensors(sl_i64 gs_export_arg0, int64_t gs_export_arg1);
int64_t s3_add_tensor(sl_i64 gs_export_arg0, sl_u8 gs_export_arg1, sl_i64 gs_export_arg2, sl_i64 gs_export_arg3, sl_u8 gs_export_arg4, int64_t gs_export_arg5, sl_i64 gs_export_arg6, int64_t gs_export_arg7, sl_u8 gs_export_arg8);
int64_t s3_set_tensor_type(sl_i64 gs_export_arg0, sl_u8 gs_export_arg1, sl_i64 gs_export_arg2, sl_i64 gs_export_arg3, sl_u8 gs_export_arg4, int64_t gs_export_arg5);
int64_t s3_set_tensor_data(sl_i64 gs_export_arg0, sl_u8 gs_export_arg1, sl_i64 gs_export_arg2, sl_i64 gs_export_arg3, sl_u8 gs_export_arg4, sl_u8 gs_export_arg5);
int64_t s3_find(sl_i64 gs_export_arg0, sl_u8 gs_export_arg1, sl_i64 gs_export_arg2, sl_u8 gs_export_arg3);
int64_t s3_tensor_size(sl_i64 gs_export_arg0, sl_i64 gs_export_arg1, int64_t gs_export_arg2);
int64_t s3_write(sl_i64 gs_export_arg0, sl_u8 gs_export_arg1, sl_i64 gs_export_arg2, sl_i64 gs_export_arg3, sl_i64 gs_export_arg4, sl_i64 gs_export_arg5, sl_i64 gs_export_arg6, sl_i64 gs_export_arg7, sl_i64 gs_export_arg8, sl_i64 gs_export_arg9, sl_u8 gs_export_arg10, sl_i64 gs_export_arg11, sl_i64 gs_export_arg12, sl_u8 gs_export_arg13, int64_t gs_export_arg14, int64_t gs_export_arg15);
int64_t gr_parse(sl_u8 gs_export_arg0, int64_t gs_export_arg1, sl_i64 gs_export_arg2, sl_i64 gs_export_arg3, sl_i64 gs_export_arg4, sl_i64 gs_export_arg5, sl_i64 gs_export_arg6);
int64_t gr_find_kv(sl_u8 gs_export_arg0, sl_i64 gs_export_arg1, sl_i64 gs_export_arg2, sl_i64 gs_export_arg3, sl_u8 gs_export_arg4);
int64_t gr_find_tensor(sl_u8 gs_export_arg0, sl_i64 gs_export_arg1, sl_i64 gs_export_arg2, sl_i64 gs_export_arg3, sl_u8 gs_export_arg4);
int64_t gr_array_raw_bits(sl_u8 gs_export_arg0, sl_i64 gs_export_arg1, sl_i64 gs_export_arg2, int64_t gs_export_arg3, int64_t gs_export_arg4, sl_i64 gs_export_arg5);
int64_t gr_meta_int(sl_u8 gs_export_arg0, sl_i64 gs_export_arg1, sl_i64 gs_export_arg2, int64_t gs_export_arg3, sl_i64 gs_export_arg4);
int64_t gr_array_int(sl_u8 gs_export_arg0, sl_i64 gs_export_arg1, sl_i64 gs_export_arg2, int64_t gs_export_arg3, int64_t gs_export_arg4, sl_i64 gs_export_arg5);
int64_t gr_meta_f32_bits(sl_u8 gs_export_arg0, sl_i64 gs_export_arg1, sl_i64 gs_export_arg2, int64_t gs_export_arg3, sl_i64 gs_export_arg4);
int64_t gr_string_array_index(sl_u8 gs_export_arg0, sl_i64 gs_export_arg1, sl_i64 gs_export_arg2, int64_t gs_export_arg3, sl_i64 gs_export_arg4);
int64_t gr_kv_type(sl_i64 gs_export_arg0, sl_i64 gs_export_arg1, int64_t gs_export_arg2);
int64_t gr_arr_type(sl_i64 gs_export_arg0, sl_i64 gs_export_arg1, int64_t gs_export_arg2);
int64_t gr_arr_n(sl_i64 gs_export_arg0, sl_i64 gs_export_arg1, int64_t gs_export_arg2);
int64_t gr_n_kv(sl_i64 gs_export_arg0);
int64_t gr_tensor_info(sl_i64 gs_export_arg0, sl_i64 gs_export_arg1, int64_t gs_export_arg2, sl_i64 gs_export_arg3);
int64_t gr_val_span(sl_i64 gs_export_arg0, sl_i64 gs_export_arg1, int64_t gs_export_arg2, sl_i64 gs_export_arg3);
int64_t gr_arr_str_span(sl_u8 gs_export_arg0, sl_i64 gs_export_arg1, sl_i64 gs_export_arg2, int64_t gs_export_arg3, int64_t gs_export_arg4, sl_i64 gs_export_arg5);
int64_t gr_arr_str_offsets(sl_u8 gs_export_arg0, sl_i64 gs_export_arg1, sl_i64 gs_export_arg2, int64_t gs_export_arg3, sl_i64 gs_export_arg4, sl_i64 gs_export_arg5);
int64_t gr_arr_str_table(sl_u8 gs_export_arg0, sl_i64 gs_export_arg1, sl_i64 gs_export_arg2, int64_t gs_export_arg3, sl_u8 gs_export_arg4, sl_i64 gs_export_arg5);
int64_t qd_dequant(int64_t gs_export_arg0, sl_u8 gs_export_arg1, int64_t gs_export_arg2, sl_f32 gs_export_arg3);
int64_t qd_dequant_block_size(int64_t gs_export_arg0);
int64_t qd_dequant_elems(int64_t gs_export_arg0);
int64_t qd_copy_to_f32(int64_t gs_export_arg0, sl_u8 gs_export_arg1, int64_t gs_export_arg2, sl_f32 gs_export_arg3);
int64_t qd_copy_from_f32(int64_t gs_export_arg0, sl_f32 gs_export_arg1, int64_t gs_export_arg2, sl_u8 gs_export_arg3);
int64_t bonsai_bind_layout(sl_u8 gs_export_arg0, sl_i64 gs_export_arg1, sl_i64 gs_export_arg2, sl_i64 gs_export_arg3, sl_i64 gs_export_arg4, sl_i64 gs_export_arg5, sl_i64 gs_export_arg6, sl_i64 gs_export_arg7, sl_i64 gs_export_arg8);
int64_t bonsai_signed_fwht(sl_f32 gs_export_arg0, sl_i32 gs_export_arg1, sl_f32 gs_export_arg2, uint8_t gs_export_arg3, int64_t gs_export_arg4, int64_t gs_export_arg5, int64_t gs_export_arg6);
int64_t bt_eog_ids(sl_u8 gs_export_arg0, sl_i64 gs_export_arg1, sl_i64 gs_export_arg2, int64_t gs_export_arg3, int64_t gs_export_arg4, int64_t gs_export_arg5, sl_i64 gs_export_arg6);
int64_t bonsai_graph(uint64_t gs_export_arg0, sl_i64 gs_export_arg1, sl_i64 gs_export_arg2, sl_i64 gs_export_arg3, sl_i64 gs_export_arg4, int64_t gs_export_arg5, int64_t gs_export_arg6, int64_t gs_export_arg7);
int64_t bonsai_mtp_stem(uint64_t gs_export_arg0, sl_i64 gs_export_arg1, sl_i64 gs_export_arg2, int64_t gs_export_arg3, int64_t gs_export_arg4);
int64_t bonsai_mtp_head(uint64_t gs_export_arg0, sl_i64 gs_export_arg1, sl_i64 gs_export_arg2, sl_i64 gs_export_arg3, sl_i64 gs_export_arg4, sl_i64 gs_export_arg5, int64_t gs_export_arg6, int64_t gs_export_arg7);
int64_t bonsai_mtp_kv(uint64_t gs_export_arg0, sl_i64 gs_export_arg1, sl_i64 gs_export_arg2, sl_i64 gs_export_arg3, sl_i64 gs_export_arg4, sl_i64 gs_export_arg5, int64_t gs_export_arg6, int64_t gs_export_arg7);
int64_t bonsai_mtp_kv_batch(uint64_t gs_export_arg0, sl_i64 gs_export_arg1, sl_i64 gs_export_arg2, sl_i64 gs_export_arg3, sl_i64 gs_export_arg4, sl_i64 gs_export_arg5, int64_t gs_export_arg6, int64_t gs_export_arg7);
#ifdef __cplusplus
}
#endif
