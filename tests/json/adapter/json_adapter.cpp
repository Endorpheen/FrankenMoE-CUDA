// Adapter: the Goose parser's parallel arrays into a common_json tree.
//
// The parser (goose/json/json-parser.goose) answers in flat arrays:
//
//   jk[node]  kind: 0 null, 1 bool, 2 integer, 3 unsigned, 4 float, 5 string, 6 array, 7 object
//   jv[node]  integer or bool value
//   js[jl]    string offset and length in the text buffer
//   jn[node]  float value, bit-cast
//   jh[node]  head of the edge chain for an object or array, -1 when empty
//   edges     e_val[e], e_koff[e], e_klen[e], e_next[e] - the object key chain, in document order
//
// The tree is built here and nowhere else: common_json's own accessors are left alone, which is the
// smaller change and the one a shadow comparison can check. Order is preserved because the edge
// chain is in document order and the array children are stored in order.
//
// See docs/goose-migration/P10-07-JSON-ADAPTER-STATE.md for what this does and does not prove.
#include "json.h"

#include <cstring>
#include <string>

namespace fg_json_adapter {

struct arrays {
    const int64_t * jk;
    const int64_t * jv;
    const int64_t * js;
    const int64_t * jl;
    const double *  jd;      // float value for kind 4
    const int64_t * jh;
    const int64_t * e_val;
    const int64_t * e_koff;
    const int64_t * e_klen;
    const int64_t * e_next;
    const char *    sbuf;
    int64_t         root_kind_unused; // kept for signature symmetry with the parser's state
};

namespace {

std::string slice(const arrays & a, int64_t off, int64_t len) {
    if (off < 0 || len < 0) { return {}; }
    return std::string(a.sbuf + off, (size_t) len);
}

common_json_value value_of(const arrays & a, int64_t node, int depth);

common_json build_container(const arrays & a, int64_t node, int depth) {
    const int64_t kind = a.jk[node];
    if (kind == 6) {
        common_json arr = common_json::array();
        // children are the edge chain's values in order
        for (int64_t e = a.jh[node]; e >= 0; e = a.e_next[e]) {
            arr.push_back(value_of(a, a.e_val[e], depth + 1));
        }
        return arr;
    }
    common_json obj = common_json::object();
    for (int64_t e = a.jh[node]; e >= 0; e = a.e_next[e]) {
        obj[std::string(a.sbuf + a.e_koff[e], (size_t) a.e_klen[e])] = value_of(a, a.e_val[e], depth + 1);
    }
    return obj;
}

common_json_value value_of(const arrays & a, int64_t node, int depth) {
    if (depth > 512 || node < 0) { return common_json_value(); }
    switch (a.jk[node]) {
        case 0: return common_json_value();
        case 1: return common_json_value(a.jv[node] != 0);
        case 2: return common_json_value((int64_t) a.jv[node]);
        case 3: return common_json_value((uint64_t) a.jv[node]);
        case 4: return common_json_value(a.jd[node]);
        case 5: return common_json_value(slice(a, a.js[node], a.jl[node]));
        case 6:
        case 7: return common_json_value(build_container(a, node, depth));
        default: return common_json_value();
    }
}

} // namespace

// Build the whole tree. `root` is the node index the parser reports as its root.
common_json to_tree(const arrays & a, int64_t root) {
    const int64_t kind = a.jk[root];
    if (kind == 6 || kind == 7) { return build_container(a, root, 0); }
    // a scalar document still needs a common_json, so it is wrapped the way the library does
    common_json doc = common_json::object();
    doc[""] = value_of(a, root, 0);
    return doc;
}

} // namespace fg_json_adapter
