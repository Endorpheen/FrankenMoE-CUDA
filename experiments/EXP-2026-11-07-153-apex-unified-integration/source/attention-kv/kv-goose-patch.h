// worker-2/kv-goose-patch.h
// C hooks used by the patched real llama-kv-cache.cpp TU. Each returns 0 when
// the cache is not Goose-attached, in which case the original native body runs
// (native-disable rollback). Non-zero means the Goose path handled the call.
#pragma once
#include <cstddef>
#include <cstdint>

extern "C" {
void * fg_goose_init_batch(void * kv, void * balloc, uint32_t n_ubatch, int embd_all);
void * fg_goose_init_full(void * kv);
void * fg_goose_init_update(void * kv, void * lctx);
int    fg_goose_update(void * kv, void * lctx, int do_shift, const void * sc_info, int * updated);

int fg_goose_seq_rm(void * kv, int seq, int p0, int p1);
int fg_goose_seq_cp(void * kv, int src, int dst, int p0, int p1);
int fg_goose_seq_keep(void * kv, int seq);
int fg_goose_seq_add(void * kv, int seq, int p0, int p1, int shift);
int fg_goose_seq_div(void * kv, int seq, int p0, int p1, int d);
int fg_goose_seq_pos_min(void * kv, int seq, int * out);
int fg_goose_seq_pos_max(void * kv, int seq, int * out);
int fg_goose_clear(void * kv, int data);

int fg_goose_state_write(void * kv, void * io, int seq, int flags);
int fg_goose_state_read(void * kv, void * io, int seq, int flags);
int fg_goose_state_read_sinfo(void * kv, void * io, int seq, int flags, void * sinfos_out, const void * sinfos_in);
int fg_goose_state_clear(void * kv, int seq);

void * fg_goose_prepare(void * kv, const void * ub_array, size_t n);
void * fg_goose_find_slot(void * kv, const void * ub, int cont);
int    fg_goose_apply_ubatch(void * kv, const void * sinfo, const void * ub);
}
