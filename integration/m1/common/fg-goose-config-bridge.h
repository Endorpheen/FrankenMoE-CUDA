// Bridge between bmoe::RunConfig and the Goose config validator (cfg_validate).
//
// The Goose export takes scalars and slices only, so the carrier flattens the
// config into the fixed layout the module documents: iv[32] for the integer
// half, fv[5] for the float half, every index naming the native field.
//
// This header and its .cpp exist only when FG_GOOSE_CONFIG is ON.
#pragma once

#include <cstdint>

#include "bmoe/config.h"

namespace bmoe {

// RunConfig -> the flat layout cfg_validate() reads. Float fields are narrowed
// to single precision first and widened afterwards, because the native
// comparisons are single-precision: a raw double can disagree at [0, 1].
void fg_config_to_flat(const RunConfig & cfg, int64_t iv[32], double fv[5]);

// One call into the composed Goose unit; returns the rule number (0 = valid,
// -1 = the flat layout was not filled). Counts the dispatch for the gate.
long long fg_config_validate_flat(const int64_t iv[32], const double fv[5]);

// Number of dispatches that actually entered the Goose module since start.
long long fg_config_goose_calls();

} // namespace bmoe
