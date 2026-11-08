// worker-2/kv-replay.cpp
//
// Differential reference replay over the REAL llama_kv_cache public API.
// This file is for comparison only: it is never called from the Goose module.
// It constructs no model and loads no weights; the parent test harness supplies
// an already-constructed llama_kv_cache*. No private members are read.
//
// Targets the ordinary Apex path (unified cache, n_stream == 1) for the
// differential journal; multi-stream slot_info is present in the class but
// flattening here is single-stream on purpose.

#include "llama-kv-cache.h"
#include "llama-batch.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {
llama_kv_cache * as_kv(void * p) { return reinterpret_cast<llama_kv_cache *>(p); }
llama_ubatch * as_ub(void * p) { return reinterpret_cast<llama_ubatch *>(p); }
} // namespace

extern "C" {

int64_t fg_kv_replay_size(void * kv) {
    return (int64_t) as_kv(kv)->get_size();
}

int64_t fg_kv_replay_stream(void * kv, int64_t seq) {
    return (int64_t) as_kv(kv)->get_stream((llama_seq_id) seq);
}

// Returns the number of target cells for the (single-stream) slot, or -1 on
// failure. Writes up to cap cell indices into out.
int64_t fg_kv_replay_find_slot(void * kv, void * ub, int64_t cont, int64_t * out, int64_t cap,
                               int64_t * s0, int64_t * s1) {
    auto sinfo = as_kv(kv)->find_slot(*as_ub(ub), cont != 0);
    if (sinfo.empty()) { return -1; }
    if (s0) { *s0 = (int64_t) sinfo.s0; }
    if (s1) { *s1 = (int64_t) sinfo.s1; }
    if (sinfo.idxs.empty()) { return 0; }
    const auto & idx = sinfo.idxs[0];
    if ((int64_t) idx.size() > cap) { return -2; }
    for (size_t i = 0; i < idx.size(); ++i) { out[i] = (int64_t) idx[i]; }
    return (int64_t) idx.size();
}

// Returns number of prepared slots, or -1 on failure. prepare() leaves the
// cache unchanged (it snapshots/applies/restores internally).
int64_t fg_kv_replay_prepare(void * kv, void * const * ubs, int64_t n) {
    std::vector<llama_ubatch> ubatches;
    ubatches.reserve((size_t) n);
    for (int64_t i = 0; i < n; ++i) { ubatches.push_back(*as_ub(ubs[i])); }
    auto sinfos = as_kv(kv)->prepare(ubatches);
    if (sinfos.empty()) { return -1; }
    return (int64_t) sinfos.size();
}

// find_slot(cont=false) + apply_ubatch. Returns 1 on success, 0 if no slot.
int64_t fg_kv_replay_apply(void * kv, void * ub) {
    auto sinfo = as_kv(kv)->find_slot(*as_ub(ub), false);
    if (sinfo.empty()) { return 0; }
    as_kv(kv)->apply_ubatch(sinfo, *as_ub(ub));
    return 1;
}

void fg_kv_replay_clear(void * kv) {
    as_kv(kv)->clear(false);
}

// Cell observables for the stream holding seq: used, used_min, used_max_p1,
// seq_pos_min, seq_pos_max, seq_pos_count.
int64_t fg_kv_replay_cell_stats(void * kv, int64_t seq, int64_t * out6) {
    const auto & cells = as_kv(kv)->get_cells((llama_seq_id) seq);
    out6[0] = (int64_t) cells.get_used();
    out6[1] = (int64_t) cells.used_min();
    out6[2] = (int64_t) cells.used_max_p1();
    out6[3] = (int64_t) cells.seq_pos_min((llama_seq_id) seq);
    out6[4] = (int64_t) cells.seq_pos_max((llama_seq_id) seq);
    out6[5] = (int64_t) cells.seq_pos_get((llama_seq_id) seq).size();
    return 0;
}

int64_t fg_kv_replay_is_empty(void * kv, int64_t seq, int64_t cell) {
    return as_kv(kv)->get_cells((llama_seq_id) seq).is_empty((uint32_t) cell) ? 1 : 0;
}

int64_t fg_kv_replay_pos(void * kv, int64_t seq, int64_t cell) {
    const auto & cells = as_kv(kv)->get_cells((llama_seq_id) seq);
    if (cells.is_empty((uint32_t) cell)) { return -1; }
    return (int64_t) cells.pos_get((uint32_t) cell);
}

int64_t fg_kv_replay_has_shift(void * kv) {
    return as_kv(kv)->get_has_shift() ? 1 : 0;
}

int64_t fg_kv_replay_can_shift(void * kv) {
    return as_kv(kv)->get_can_shift() ? 1 : 0;
}

// Native control methods used only as the differential reference.
int64_t fg_kv_replay_seq_rm(void * kv, int64_t seq, int64_t p0, int64_t p1) {
    return as_kv(kv)->seq_rm((llama_seq_id) seq, (llama_pos) p0, (llama_pos) p1) ? 1 : 0;
}
void fg_kv_replay_seq_keep(void * kv, int64_t seq) { as_kv(kv)->seq_keep((llama_seq_id) seq); }
void fg_kv_replay_seq_add(void * kv, int64_t seq, int64_t p0, int64_t p1, int64_t shift) {
    as_kv(kv)->seq_add((llama_seq_id) seq, (llama_pos) p0, (llama_pos) p1, (llama_pos) shift);
}
void fg_kv_replay_seq_div(void * kv, int64_t seq, int64_t p0, int64_t p1, int64_t d) {
    as_kv(kv)->seq_div((llama_seq_id) seq, (llama_pos) p0, (llama_pos) p1, (int) d);
}
void fg_kv_replay_seq_cp(void * kv, int64_t src, int64_t dst, int64_t p0, int64_t p1) {
    as_kv(kv)->seq_cp((llama_seq_id) src, (llama_seq_id) dst, (llama_pos) p0, (llama_pos) p1);
}
int64_t fg_kv_replay_seq_pos_min(void * kv, int64_t seq) {
    return (int64_t) as_kv(kv)->seq_pos_min((llama_seq_id) seq);
}
int64_t fg_kv_replay_seq_pos_max(void * kv, int64_t seq) {
    return (int64_t) as_kv(kv)->seq_pos_max((llama_seq_id) seq);
}
void fg_kv_replay_state_clear(void * kv, int64_t seq) {
    as_kv(kv)->state_clear((llama_seq_id) seq);
}

} // extern "C"
