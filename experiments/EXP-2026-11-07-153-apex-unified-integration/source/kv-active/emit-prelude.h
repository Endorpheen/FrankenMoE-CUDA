#pragma once
#include <stdint.h>

int64_t fg_w2_get(int64_t, int64_t, int64_t);
void    fg_w2_put(int64_t, int64_t, int64_t, int64_t);
int64_t fg_w2_service(int64_t, int64_t, int64_t);
void    fg_w2_witness(int64_t);
int64_t fg_w2_set_create(void);
void    fg_w2_set_destroy(int64_t);
int64_t fg_w2_set_insert(int64_t, int64_t);
int64_t fg_w2_set_erase(int64_t, int64_t);
int64_t fg_w2_set_lower_bound(int64_t, int64_t);
int64_t fg_w2_set_count(int64_t, int64_t);
int64_t fg_w2_set_first(int64_t);
int64_t fg_w2_set_last(int64_t);
int64_t fg_w2_set_next(int64_t, int64_t);
int64_t fg_w2_set_prev(int64_t, int64_t);
int64_t fg_w2_set_size(int64_t);
int64_t fg_w2_set_key(int64_t, int64_t);

int64_t fg_kv_get(int64_t, int64_t, int64_t);
void    fg_kv_put(int64_t, int64_t, int64_t, int64_t);
int64_t fg_kv_service(int64_t, int64_t, int64_t);
int64_t fg_kv_obj(int64_t);
int64_t fg_kv_create(int64_t, int64_t);
void    fg_kv_destroy(int64_t);
void    fg_kv_vec_push(int64_t, int64_t);
void    fg_kv_slot_push(int64_t, int64_t, int64_t);
int64_t fg_kv_slot_len(int64_t, int64_t);
void    fg_kv_slot_clear(int64_t, int64_t);
int64_t fg_kv_swa_masked(int64_t, int64_t, int64_t, int64_t, int64_t);
void    fg_kv_sync(int64_t, int64_t);
void    fg_kv_tensor_copy(int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg_kv_shift_compute(int64_t, int64_t);
void    fg_kv_zero_cells(int64_t, int64_t, int64_t, int64_t);
void    fg_kv_clear_data(int64_t);
int64_t fg_kv_other_pos(int64_t, int64_t, int64_t);

int64_t fg_kv_ub_n_tokens(int64_t);
int64_t fg_kv_ub_n_seqs_unq(int64_t);
int64_t fg_kv_ub_seq_id_unq(int64_t, int64_t);
int64_t fg_kv_ub_pos(int64_t, int64_t);
int64_t fg_kv_ub_n_seq_id(int64_t, int64_t);
int64_t fg_kv_ub_seq_id(int64_t, int64_t, int64_t);
int64_t fg_kv_ub_token(int64_t, int64_t);
int64_t fg_kv_ub_has_token(int64_t);
int64_t fg_kv_ub_is_pos_2d(int64_t);
int64_t fg_kv_ub_is_mixed(int64_t);
int64_t fg_kv_ub_type(int64_t, int64_t);
void    fg_kv_ub_destroy(int64_t);
void    fg_kv_balloc_reset(int64_t);
int64_t fg_kv_balloc_split(int64_t, int64_t, int64_t);
int64_t fg_kv_balloc_n_used(int64_t);
int64_t fg_kv_balloc_n_tokens(int64_t);
void    fg_kv_balloc_destroy(int64_t);
int64_t fg_kv_test_ubatch(int64_t);
void    fg_kv_test_ub_set_pos(int64_t, int64_t, int64_t);
void    fg_kv_test_ub_set_seq(int64_t, int64_t, int64_t, int64_t);
void    fg_kv_test_ub_set_seq_unq(int64_t, int64_t, int64_t);
void    fg_kv_test_ub_set_token(int64_t, int64_t, int64_t);
int64_t fg_kv_test_balloc(int64_t);
void    fg_kv_test_ubatch_free(int64_t);

void    fg_state_write_u32(void *, int64_t);
int64_t fg_state_read_u32(void *);
void    fg_state_write_i32(void *, int64_t);
int64_t fg_state_read_i32(void *);
void    fg_state_write_u64(void *, int64_t);
int64_t fg_state_read_u64(void *);
int64_t fg_state_v_trans(int64_t);
int64_t fg_state_n_rot(int64_t, int64_t);
int64_t fg_state_has_cell_ext(int64_t);
int64_t fg_state_layer_il(int64_t, int64_t);
int64_t fg_state_layer_has_v(int64_t, int64_t);
int64_t fg_state_n_embd_k(int64_t, int64_t);
int64_t fg_state_n_embd_v(int64_t, int64_t);
int64_t fg_state_tensor_type(int64_t, int64_t, int64_t, int64_t);
int64_t fg_state_tensor_row_size(int64_t, int64_t, int64_t, int64_t);
int64_t fg_state_tensor_size_el(int64_t, int64_t, int64_t, int64_t);
void    fg_state_write_rows(int64_t, void *, int64_t, int64_t, int64_t, int64_t, int64_t);
void    fg_state_read_rows(int64_t, void *, int64_t, int64_t, int64_t, int64_t, int64_t);
void    fg_state_write_trans(int64_t, void *, int64_t, int64_t, int64_t, int64_t, int64_t);
void    fg_state_read_trans(int64_t, void *, int64_t, int64_t, int64_t, int64_t, int64_t);
void    fg_state_fail(int64_t);
int64_t fg_sinfos_count(void *);
void    fg_sinfos_init(void *, int64_t);
int64_t fg_sinfos_in_empty(void *, int64_t);
void    fg_sinfos_in_to_slot(void *, int64_t, int64_t);
void    fg_sinfos_set_out(void *, int64_t, int64_t);

#ifndef FG153_KV_I64_IO
#define FG153_KV_I64_IO
void fg153_i64_fg_state_write_u32(int64_t,int64_t);
int64_t fg153_i64_fg_state_read_u32(int64_t);
void fg153_i64_fg_state_write_i32(int64_t,int64_t);
int64_t fg153_i64_fg_state_read_i32(int64_t);
void fg153_i64_fg_state_write_u64(int64_t,int64_t);
int64_t fg153_i64_fg_state_read_u64(int64_t);
void fg153_i64_fg_state_write_rows(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
void fg153_i64_fg_state_read_rows(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
void fg153_i64_fg_state_write_trans(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
void fg153_i64_fg_state_read_trans(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
int64_t fg153_i64_fg_sinfos_count(int64_t);
void fg153_i64_fg_sinfos_init(int64_t,int64_t);
int64_t fg153_i64_fg_sinfos_in_empty(int64_t,int64_t);
void fg153_i64_fg_sinfos_in_to_slot(int64_t,int64_t,int64_t);
void fg153_i64_fg_sinfos_set_out(int64_t,int64_t,int64_t);
#endif
