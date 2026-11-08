// fg153-hooks.h - actual Goose hooks for the recurrent class + context.
// Proposal for Codex; the parent injects early-return hooks into the real
// llama-memory-recurrent.cpp (replay.py). Native original bodies stay in place
// and run whenever an object/context is not Goose-managed (rollback).
#ifndef FG153_RECURRENT_HOOKS_H
#define FG153_RECURRENT_HOOKS_H

#include "llama-memory-recurrent.h"
#include "llama-batch.h"

#include <map>
#include <memory>
#include <vector>
#include <cstdint>

class llama_memory_context_i;
struct llama_memory_recurrent;
class llama_memory_recurrent_context;

// Goose entry points emitted by the frozen self-hosted stage3 (six args each).
extern "C" int64_t fg153_goose_cells_entry(int64_t mode, int64_t a, int64_t b,
                                           int64_t c, int64_t d, int64_t e);
extern "C" int64_t fg153_goose_lifecycle_entry(int64_t mode, int64_t a, int64_t b,
                                               int64_t c, int64_t d, int64_t e);
extern "C" int64_t fg153_goose_state_entry(int64_t mode, int64_t a, int64_t b,
                                           int64_t c, int64_t d, int64_t e);

// Memory-instance registration (projection lifetime is shared, never raw).
bool fg153_hooks_active(const llama_memory_recurrent * m);
void fg153_hooks_register(llama_memory_recurrent * m);
void fg153_hooks_forget(const llama_memory_recurrent * m);
int64_t fg153_hooks_live_count();

// Fixture helper: install one real small buffer so is_empty()/ctxs_bufs paths
// are non-empty for both reference instances (no model/weights involved).
void fg153_fixture_seed(llama_memory_recurrent * m);

// Production constructor: Goose allocation/resource ownership. Returns 0 on
// success, negative on failure after full cleanup (caller throws).
bool fg153_hooks_ctor_enabled();
int64_t fg153_hooks_ctor_alloc(llama_memory_recurrent * m, const void * filter, void * model,
                               int type_r, int type_s, int offload,
                               uint32_t mem_size, uint32_t n_seq_max, uint32_t n_rs_seq);

// Per-method memory hooks.
void fg153_hook_clear(llama_memory_recurrent * m, bool data);
bool fg153_hook_seq_rm(llama_memory_recurrent * m, llama_seq_id seq_id, llama_pos p0, llama_pos p1);
void fg153_hook_seq_cp(llama_memory_recurrent * m, llama_seq_id src, llama_seq_id dst, llama_pos p0, llama_pos p1);
void fg153_hook_seq_keep(llama_memory_recurrent * m, llama_seq_id seq_id);
void fg153_hook_seq_add(llama_memory_recurrent * m, llama_seq_id seq_id, llama_pos p0, llama_pos p1, llama_pos shift);
void fg153_hook_seq_div(llama_memory_recurrent * m, llama_seq_id seq_id, llama_pos p0, llama_pos p1, int d);
llama_pos fg153_hook_seq_pos_min(const llama_memory_recurrent * m, llama_seq_id seq_id);
llama_pos fg153_hook_seq_pos_max(const llama_memory_recurrent * m, llama_seq_id seq_id);
bool fg153_hook_get_can_shift(const llama_memory_recurrent * m);
// Public methods called directly by hybrid::init_batch (bypassing init_batch hook).
bool fg153_hook_prepare(llama_memory_recurrent * m, const std::vector<llama_ubatch> & ubatches);
bool fg153_hook_find_slot(llama_memory_recurrent * m, const llama_ubatch & ubatch);
std::unique_ptr<llama_memory_context_i> fg153_hook_init_batch(
        llama_memory_recurrent * m, llama_batch_allocr & balloc, uint32_t n_ubatch, bool embd_all);
std::unique_ptr<llama_memory_context_i> fg153_hook_init_full(llama_memory_recurrent * m);
std::unique_ptr<llama_memory_context_i> fg153_hook_init_update(
        llama_memory_recurrent * m, llama_context * lctx, bool optimize);
void fg153_hook_state_write(const llama_memory_recurrent * m, llama_io_write_i & io,
                            llama_seq_id seq_id, llama_state_seq_flags flags);
void fg153_hook_state_read(llama_memory_recurrent * m, llama_io_read_i & io,
                           llama_seq_id seq_id, llama_state_seq_flags flags);
std::map<ggml_backend_buffer_type_t, size_t> fg153_hook_memory_breakdown(
        const llama_memory_recurrent * m);

// Per-method context hooks (the REAL concrete llama_memory_recurrent_context is
// preserved; non-virtual getters are patched, not replaced by an unrelated type).
bool fg153_context_active(const llama_memory_recurrent_context * c);
int64_t fg153_context_hook_next(const llama_memory_recurrent_context * c);
int64_t fg153_context_hook_apply(const llama_memory_recurrent_context * c);
int64_t fg153_context_hook_status(const llama_memory_recurrent_context * c);
int64_t fg153_context_hook_ubatch(const llama_memory_recurrent_context * c);
int64_t fg153_context_hook_n_rs(const llama_memory_recurrent_context * c);
int64_t fg153_context_hook_head(const llama_memory_recurrent_context * c);
int64_t fg153_context_hook_rs_z(const llama_memory_recurrent_context * c);
int64_t fg153_context_hook_size(const llama_memory_recurrent_context * c);
int64_t fg153_context_hook_tensor(const llama_memory_recurrent_context * c, int64_t which, int64_t il);
int64_t fg153_context_hook_s_copy(const llama_memory_recurrent_context * c, int64_t i);
// Called from the out-of-line concrete context destructor.
void fg153_context_release(const llama_memory_recurrent_context * c);

#endif /* FG153_RECURRENT_HOOKS_H */
