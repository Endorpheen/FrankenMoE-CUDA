// Goose bridge for the speculative-decoding decisions (staged S2.2 trial).
//
// Built only when FG_GOOSE_SPEC=ON. common/fg-spec-decide.cpp keeps the native bodies of the
// same decisions compiled and calls these wrappers instead when the flag is on.
//
// The seam is thin on purpose: the module's ABI passes scalars and slices by value, the
// carrier holds std::vector and small structs, so this file converts containers to slices,
// calls one sdp_* export per decision, and translates the flat out[] array back into a struct.
// No policy lives here. The one asymmetry worth naming is `accept`: the module returns counts
// (n_acc, confirmed length, eog hit, stop index) and the carrier needs the confirmed token
// vector, so it is rebuilt from `tok` plus the accepted drafts — correct because acceptance
// means the target's own token equals the draft.
#pragma once

#include "fg-spec-decide.h"

#include <cstdint>
#include <vector>

namespace fg_goose_spec {

int ngram_draft(const std::vector<int32_t> & ctx, int32_t last, int n_max, int min_match,
                int max_match, std::vector<int32_t> & out);

fg_spec::accept_t accept(const std::vector<int32_t> & drafts,
                         const std::vector<int32_t> & target,
                         const std::vector<int32_t> & eog, int32_t tok,
                         std::vector<int32_t> * confirmed);

long long emit_count(long long n_predict, long long n_gen, long long confirmed);

fg_spec::plan_t step_plan(long long n_predict, long long n_gen, long long draft_max,
                          long long spec_on, long long n_draft_in);

fg_spec::rollback_t rollback(long long n_past, long long n_draft, long long n_acc);

fg_spec::advance_t advance(long long n_past, long long n_acc, long long wide);

fg_spec::trim_t tail_trim(long long n_prompt, long long n_gen, long long kv_end);

// Decisions taken by the module in this process; the gate prints it as the witness that the
// dispatch really entered Goose.
long long calls();

} // namespace fg_goose_spec
