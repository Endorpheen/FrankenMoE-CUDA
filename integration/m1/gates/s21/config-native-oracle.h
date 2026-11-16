// P11-04 D1 (EXP670): the frozen copy of bmoe::validate() as it stood before the
// Goose port. The production body in runtime/core/src/config.cpp is deleted by
// the carrier patch; this file is the only remaining copy and exists so the S2.1
// gate keeps an independent oracle.
//
// Native source: runtime/core/src/config.cpp, bmoe::validate() (whole function),
// taken verbatim. Nothing here is called by shipped code.
#pragma once

#include "bmoe/config.h"

namespace bmoe {

ValidationResult validate_native_oracle(const RunConfig & cfg);

} // namespace bmoe
