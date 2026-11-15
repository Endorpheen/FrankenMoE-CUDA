// Adapter implementation: json_parse buffers -> the flat document, and the
// nlohmann-subset accessors on top of it. Host code, not a Goose module.

#include "goose-json-adapter.h"

#include "json-parser-gen.h"

#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace goose_json {

namespace {

// json_parse borrows strtod, the same primitive the accepted parser documents.
extern "C" double gs_json_strtod(sl_u8 s) {
    std::string tmp;
    tmp.assign(reinterpret_cast<const char *>(s.data), (std::size_t) s.len);
    return std::strtod(tmp.c_str(), nullptr);
}

void init_once() {
    static std::once_flag once;
    std::call_once(once, [] { goose_init(0, nullptr); });
}

sl_u8 slice_u8(const void * p, std::size_t n) {
    sl_u8 s;
    s.data = const_cast<uint8_t *>(reinterpret_cast<const uint8_t *>(p));
    s.len  = (int64_t) n;
    return s;
}

template <class T>
sl_i64 slice_i64(T & v) {
    sl_i64 s;
    s.data = reinterpret_cast<int64_t *>(v.data());
    s.len  = (int64_t) v.size();
    return s;
}

sl_f64 slice_f64(std::vector<double> & v) {
    sl_f64 s;
    s.data = v.data();
    s.len  = (int64_t) v.size();
    return s;
}

const char * kind_name(int k) {
    switch (k) {
        case detail::K_NULL:   return "null";
        case detail::K_BOOL:   return "boolean";
        case detail::K_INT:    return "number_integer";
        case detail::K_UINT:   return "number_unsigned";
        case detail::K_FLOAT:  return "number_float";
        case detail::K_STRING: return "string";
        case detail::K_ARRAY:  return "array";
        case detail::K_OBJECT: return "object";
        default:               return "unknown";
    }
}

[[noreturn]] void type_error(const char * expected, const char * actual) {
    throw std::runtime_error(std::string("goose_json: type_error, expected ") + expected +
                             ", got " + actual);
}

} // namespace

json json::parse(const std::string & text) {
    init_once();

    // Upper bounds of the flat table, see json-parser.goose: a node consumes at
    // least one byte of text, an object member at least four.
    const std::size_t n = text.size() + 2;

    std::vector<int64_t> jk(n), jv(n), js(n), jl(n), jf(n), jn(n), jh(n);
    std::vector<double>  jd(n);
    std::vector<int64_t> e_val(n), e_koff(n), e_klen(n), e_next(n);
    std::vector<int64_t> jkey(n), jkl(n), jchild(n);
    std::vector<int64_t> st(12, 0);
    std::vector<uint8_t> sbuf(text.size() * 2 + 16, 0);

    const int64_t root = json_parse(slice_u8(text.data(), text.size()),
                                    slice_i64(jk), slice_i64(jv), slice_f64(jd),
                                    slice_i64(js), slice_i64(jl), slice_i64(jf),
                                    slice_i64(jn), slice_i64(jh),
                                    slice_i64(e_val), slice_i64(e_koff),
                                    slice_i64(e_klen), slice_i64(e_next),
                                    slice_u8(sbuf.data(), sbuf.size()),
                                    slice_i64(jkey), slice_i64(jkl),
                                    slice_i64(jchild), slice_i64(st));

    if (root < 0) {
        throw parse_error((int) st[1], st[2], st[3]);
    }

    auto doc = std::make_shared<detail::document>();
    doc->nodes.resize((std::size_t) st[0]);
    for (std::size_t i = 0; i < doc->nodes.size(); ++i) {
        detail::node & nd = doc->nodes[i];
        nd.kind   = (int) jk[i];
        nd.ival   = jv[i];
        nd.dval   = jd[i];
        nd.soff   = (int) js[i];
        nd.slen   = (int) jl[i];
        nd.cbegin = (int) jf[i];
        nd.ccount = (int) jn[i];
    }
    doc->entries.resize((std::size_t) st[11]);
    for (std::size_t i = 0; i < doc->entries.size(); ++i) {
        detail::entry & e = doc->entries[i];
        e.cnode = (int) jchild[i];
        e.koff  = (int) jkey[i];
        e.klen  = (int) jkl[i];
    }
    doc->sbuf.assign(reinterpret_cast<const char *>(sbuf.data()), (std::size_t) st[10]);

    return json(std::move(doc), (int) root);
}

const detail::node & json::node_ref() const {
    static const detail::node null_node;
    if (!doc_ || idx_ < 0) {
        return null_node;
    }
    return doc_->nodes[(std::size_t) idx_];
}

bool json::is_null() const   { return node_ref().kind == detail::K_NULL; }
bool json::is_boolean() const { return node_ref().kind == detail::K_BOOL; }
bool json::is_number() const {
    const int k = node_ref().kind;
    return k == detail::K_INT || k == detail::K_UINT || k == detail::K_FLOAT;
}
bool json::is_string() const { return node_ref().kind == detail::K_STRING; }
bool json::is_array() const  { return node_ref().kind == detail::K_ARRAY; }
bool json::is_object() const { return node_ref().kind == detail::K_OBJECT; }

bool json::key_eq(const detail::entry & e, const std::string & key) const {
    if ((std::size_t) e.klen != key.size()) {
        return false;
    }
    return std::memcmp(doc_->sbuf.data() + e.koff, key.data(), key.size()) == 0;
}

bool json::contains(const std::string & key) const {
    if (!doc_ || idx_ < 0 || node_ref().kind != detail::K_OBJECT) {
        return false;
    }
    const detail::node & nd = node_ref();
    for (int i = 0; i < nd.ccount; ++i) {
        if (key_eq(doc_->entries[(std::size_t) (nd.cbegin + i)], key)) {
            return true;
        }
    }
    return false;
}

json json::operator[](const std::string & key) const {
    if (!doc_ || idx_ < 0 || node_ref().kind != detail::K_OBJECT) {
        return json();
    }
    const detail::node & nd = node_ref();
    for (int i = 0; i < nd.ccount; ++i) {
        const detail::entry & e = doc_->entries[(std::size_t) (nd.cbegin + i)];
        if (key_eq(e, key)) {
            return json(doc_, e.cnode);
        }
    }
    return json();
}

json json::operator[](std::size_t index) const {
    if (!doc_ || idx_ < 0 || node_ref().kind != detail::K_ARRAY) {
        return json();
    }
    const detail::node & nd = node_ref();
    if (index >= (std::size_t) nd.ccount) {
        return json();
    }
    return json(doc_, doc_->entries[(std::size_t) (nd.cbegin + (int) index)].cnode);
}

std::size_t json::size() const {
    const int k = node_ref().kind;
    if (k == detail::K_ARRAY || k == detail::K_OBJECT) {
        return (std::size_t) node_ref().ccount;
    }
    return 0;
}

json::const_iterator json::begin() const {
    if (!doc_ || idx_ < 0) {
        return const_iterator();
    }
    const detail::node & nd = node_ref();
    return const_iterator(doc_, nd.cbegin, nd.cbegin + nd.ccount);
}

json::const_iterator json::end() const {
    if (!doc_ || idx_ < 0) {
        return const_iterator();
    }
    const detail::node & nd = node_ref();
    return const_iterator(doc_, nd.cbegin + nd.ccount, nd.cbegin + nd.ccount);
}

template <>
std::string json::get<std::string>() const {
    if (node_ref().kind != detail::K_STRING) {
        type_error("string", kind_name(node_ref().kind));
    }
    const detail::node & nd = node_ref();
    return doc_->sbuf.substr((std::size_t) nd.soff, (std::size_t) nd.slen);
}

template <>
int json::get<int>() const {
    const detail::node & nd = node_ref();
    if (nd.kind == detail::K_INT) {
        return (int) nd.ival;
    }
    if (nd.kind == detail::K_UINT) {
        return (int) (uint64_t) nd.ival;
    }
    type_error("int", kind_name(nd.kind));
}

template <>
double json::get<double>() const {
    const detail::node & nd = node_ref();
    if (nd.kind == detail::K_FLOAT) {
        return nd.dval;
    }
    if (nd.kind == detail::K_INT) {
        return (double) nd.ival;
    }
    if (nd.kind == detail::K_UINT) {
        return (double) (uint64_t) nd.ival;
    }
    type_error("double", kind_name(nd.kind));
}

template <>
bool json::get<bool>() const {
    if (node_ref().kind != detail::K_BOOL) {
        type_error("boolean", kind_name(node_ref().kind));
    }
    return node_ref().ival != 0;
}

} // namespace goose_json
