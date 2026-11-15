// The pre-integration body of bmoe::validate(), kept under its own name.
//
// Native source: runtime/core/src/config.cpp, bmoe::validate() (whole function).
// This staged build renames that body to validate_native() so that:
//   * the gate has a native oracle to compare the Goose path against;
//   * switching FG_GOOSE_CONFIG off is a rebuild, not a revert — the native
//     decision chain stays compiled in both configurations.
//
// In the production integration this declaration is not needed: validate() is
// the only symbol the engine calls, and the native body is called from inside
// the same translation unit.
#pragma once

#include "bmoe/config.h"

namespace bmoe {

ValidationResult validate_native(const RunConfig & cfg);

} // namespace bmoe
