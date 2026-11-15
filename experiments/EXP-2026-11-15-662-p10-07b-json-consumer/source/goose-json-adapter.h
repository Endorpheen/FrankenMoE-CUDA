// Adapter: the nlohmann::json subset actually used by one consumer, implemented
// over the flat node table that goose/json/json-parser.goose emits.
//
// Consumer: tests/gguf-model-data.cpp, detect_gguf_filename() (HuggingFace
// /api/models response -> sibling file names). The subset is exactly:
//   json::parse(first, last)            parse a byte range, throw on error
//   j.contains(key)                     object key presence
//   j[key]                              object member, null view when absent
//   j[index]                            array element
//   v.is_array()                        kind test
//   v.get<std::string>()                typed read, throws on a wrong type
//   for (const auto & e : v)            array iteration
//   v.size()                            element count
//
// The tree stays flat, exactly as json_parse emits it: node kind, integer /
// double payload, string range into the shared string buffer, and a child
// block (child node id + key range per entry). This header is the seam of the
// stage: it is host code, not a Goose module, and it makes no JSON decision.
//
// Node kinds and the array layout are the parser's contract, see the header
// comment of source/json-parser.goose.

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace goose_json {

namespace detail {

// The parser's node kinds (source/json-parser.goose).
enum kind : int {
    K_NULL   = 0,
    K_BOOL   = 1,
    K_INT    = 2,
    K_UINT   = 3,
    K_FLOAT  = 4,
    K_STRING = 5,
    K_ARRAY  = 6,
    K_OBJECT = 7,
};

struct node {
    int     kind   = K_NULL;
    int64_t ival   = 0;   // bool, K_INT, K_UINT (as the bit pattern of a u64)
    double  dval   = 0.0; // K_FLOAT
    int     soff   = 0;   // K_STRING: offset and length in sbuf
    int     slen   = 0;
    int     cbegin = 0;   // first entry of the child block
    int     ccount = 0;   // number of entries
};

struct entry {
    int cnode = 0; // child node id
    int koff  = 0; // key offset and length in sbuf
    int klen  = 0;
};

struct document {
    std::vector<node>  nodes;
    std::vector<entry> entries;
    std::string        sbuf;
};

} // namespace detail

struct parse_error : std::runtime_error {
    parse_error(int code, int64_t line, int64_t column)
        : std::runtime_error("goose_json: parse error " + std::to_string(code) +
                             " at line " + std::to_string(line) + ", column " +
                             std::to_string(column)),
          code(code), line(line), column(column) {}
    int     code;
    int64_t line;
    int64_t column;
};

class json {
public:
    json() = default;

    json(std::shared_ptr<const detail::document> doc, int idx)
        : doc_(std::move(doc)), idx_(idx) {}

    // nlohmann::json::parse(InputIt first, InputIt last)
    static json parse(const std::string & text);

    template <class It>
    static json parse(It first, It last) {
        return parse(std::string(first, last));
    }

    bool is_null() const;
    bool is_boolean() const;
    bool is_number() const;
    bool is_string() const;
    bool is_array() const;
    bool is_object() const;

    bool contains(const std::string & key) const;
    bool contains(const char * key) const { return contains(std::string(key)); }

    // A missing member reads as a null view, like nlohmann's null insert, and
    // every consumer here only ever tests it for presence or type.
    json operator[](const std::string & key) const;
    json operator[](const char * key) const { return (*this)[std::string(key)]; }
    json operator[](std::size_t index) const;

    std::size_t size() const;

    // Explicit specialisations only: the consumer reads strings, and a wrong
    // type throws like nlohmann's get<T>().
    template <class T>
    T get() const;

    class const_iterator {
    public:
        const_iterator() = default;
        const_iterator(std::shared_ptr<const detail::document> doc, int pos, int end)
            : doc_(std::move(doc)), pos_(pos), end_(end) {}

        json operator*() const {
            const detail::entry & e = doc_->entries[(std::size_t) pos_];
            return json(doc_, e.cnode);
        }

        const_iterator & operator++() {
            pos_ = pos_ + 1;
            return *this;
        }

        bool operator==(const const_iterator & other) const { return pos_ == other.pos_; }
        bool operator!=(const const_iterator & other) const { return pos_ != other.pos_; }

    private:
        std::shared_ptr<const detail::document> doc_;
        int pos_ = 0;
        int end_ = 0;
    };

    const_iterator begin() const;
    const_iterator end() const;

    const detail::document & document_ref() const { return *doc_; }

private:
    const detail::node & node_ref() const;
    bool key_eq(const detail::entry & e, const std::string & key) const;

    std::shared_ptr<const detail::document> doc_;
    int idx_ = -1; // -1: the null view
};

template <>
std::string json::get<std::string>() const;
template <>
int json::get<int>() const;
template <>
double json::get<double>() const;
template <>
bool json::get<bool>() const;

} // namespace goose_json
