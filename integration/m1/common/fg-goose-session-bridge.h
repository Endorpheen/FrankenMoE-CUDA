// Goose bridge for the P9-05 session decisions (staged S3.2 trial).
//
// Built only when FG_GOOSE_SESSION=ON. common/fg-session-decide.cpp keeps the native bodies
// compiled and calls these wrappers instead when the flag is on.
//
// The seam is thin on purpose: the composed module (goose-gen/session.goose, one unit for the
// three slices) takes int64 scalars and sl_i64 slices by value, the carrier holds int64
// fields and a small struct, so each wrapper is one export call and, where the export writes
// a flat out[] array, one translation back into the carrier's struct. No policy lives here.
#pragma once

#include "fg-session-decide.h"

#include <cstdint>

namespace fg_goose_session {

int64_t sentinels(int64_t top_k);
int64_t hot_slots(int64_t budget_mb, int64_t meta_ok, int64_t n_expert, int64_t bytes_per_slot,
                  int64_t top_k);
int64_t hot_reason(int64_t budget_mb, int64_t meta_ok, int64_t n_expert, int64_t bytes_per_slot,
                   int64_t top_k);
int64_t sentinel_mib(int64_t bytes_per_slot, int64_t top_k);
int64_t vram_clamp(int64_t free_mib, int64_t budget_mb, int64_t n_gpu_layers);
int64_t probe_margin(int64_t is_mtp);
int64_t probe_budget(int64_t free_mib, int64_t is_mtp);

fg_session::ResizePlan resize_plan(int64_t plan_slots, int64_t plan_mib, int64_t n_gpu_layers,
                                   int64_t free_mib, int64_t is_mtp, int64_t meta_ok,
                                   int64_t n_expert, int64_t bytes_per_slot, int64_t top_k);

int64_t n_batch(int64_t n_ctx);
int64_t ubatch(int64_t n_ubatch, int64_t n_batch);

// rts_plan writes the executed step codes into out[] and returns their count.
int64_t run_plan(int64_t validate_ok, int64_t open_ok, int64_t * out, int64_t cap);

int64_t draft_ubatch(int64_t draft_max, int64_t target_ubatch);
int64_t verify_batch(int64_t n_batch, int64_t draft_max);

// Decisions the module took in this process; the gate prints it as the witness that the
// dispatch really entered Goose (0 in an OFF build, where this file is not compiled).
long long calls();

} // namespace fg_goose_session
