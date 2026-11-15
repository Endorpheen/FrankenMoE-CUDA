// Goose bridge for the model-management decisions (staged S3.1 trial).
//
// Built only when FG_GOOSE_MODELS=ON. common/fg-model-decide.cpp keeps the native
// transcriptions compiled and calls these wrappers instead when the flag is on.
//
// The seam is thin on purpose: the module's ABI passes scalars and slices by value,
// the carrier holds std::vector and small structs, so this file flattens containers
// into the name-table shape the modules take (one name buffer plus off/ln), calls one
// export per decision, and translates the flat out[] array back into a struct. No
// policy lives here.
#pragma once

#include "fg-model-decide.h"

#include <cstdint>
#include <vector>

namespace fg_goose_model {

fg_model::pred_t pred(int status, long long exit_code);

fg_model::unload_t unload_action(int mapped, int status);

long long victim(const std::vector<fg_model::row_t> & rows);

std::string preset_norm(const std::string & options);

fg_model::unload_set_t unload_set(const std::vector<fg_model::model_t> & cur,
                                  const std::vector<fg_model::model_t> & next);

fg_model::connector_t connector_target(int sibling, const std::vector<int> & dir_exists,
                                       const std::vector<std::string> & hosts);

// Decisions taken by the module in this process: the gate prints it as the witness
// that the dispatch really entered Goose.
long long calls();

}  // namespace fg_goose_model
