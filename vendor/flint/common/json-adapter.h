// Adapter: the Goose JSON parser's parallel arrays into nlohmann's ordered_json.
//
// common_json stores ordered_json internally, so the tree this builds is the one the library would
// hold. Kept separate from json.cpp so the swap there is one call, and so the test tree can link the
// same translation unit the runtime would use.
#pragma once

#include <cstdint>

#include "nlohmann/json.hpp"

namespace fg_json_adapter {

struct arrays {
    const int64_t * jk;
    const int64_t * jv;
    const int64_t * js;
    const int64_t * jl;
    const double *  jd;
    const int64_t * jh;
    const int64_t * e_val;
    const int64_t * e_koff;
    const int64_t * e_klen;
    const int64_t * e_next;
    const char *    sbuf;
    int64_t         root_kind_unused;
};

nlohmann::ordered_json to_tree(const arrays & a, int64_t root);

} // namespace fg_json_adapter
