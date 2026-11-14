// Differential gate for the special-token partitioning slice (S2).
//
// The oracle is the native text of llama_vocab::impl::tokenizer_st_partition
// extracted from src/llama-vocab.cpp by tests/make-native.py; the port is
// source/special-partition.goose. Both return a flattened fragment list and the
// gate compares entry by entry.
#include "native-partition.h"
#include "special-partition.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct one_token {
    int         id;
    int         attr;
    std::string text;
};

struct row {
    int                    parse_special;
    std::string            text;
    std::vector<one_token> tokens;
};

std::string from_hex(const std::string & h) {
    std::string r;
    r.reserve(h.size() / 2);
    for (size_t i = 0; i + 1 < h.size(); i += 2) {
        r.push_back((char) std::stoi(h.substr(i, 2), nullptr, 16));
    }
    return r;
}

std::vector<row> load(const char * path) {
    std::ifstream in(path);
    if (!in) {
        fprintf(stderr, "cannot open %s\n", path);
        exit(2);
    }
    std::vector<row> rows;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        size_t t1 = line.find('\t');
        size_t t2 = line.find('\t', t1 + 1);
        row r;
        r.parse_special = std::stoi(line.substr(0, t1));
        r.text = from_hex(line.substr(t1 + 1, t2 - t1 - 1));

        std::string rest = line.substr(t2 + 1);
        size_t pos = 0;
        while (pos < rest.size()) {
            size_t comma = rest.find(',', pos);
            if (comma == std::string::npos) {
                comma = rest.size();
            }
            std::string item = rest.substr(pos, comma - pos);
            size_t c1 = item.find(':');
            size_t c2 = item.find(':', c1 + 1);
            one_token t;
            t.id = std::stoi(item.substr(0, c1));
            t.attr = std::stoi(item.substr(c1 + 1, c2 - c1 - 1));
            t.text = from_hex(item.substr(c2 + 1));
            r.tokens.push_back(t);
            pos = comma + 1;
        }
        rows.push_back(r);
    }
    return rows;
}

std::string flat(const std::vector<int> & kind, const std::vector<int> & tok,
                 const std::vector<unsigned> & off, const std::vector<unsigned> & len) {
    std::string s;
    char buf[64];
    for (size_t i = 0; i < kind.size(); i++) {
        if (kind[i] == 0) {
            snprintf(buf, sizeof(buf), "T%d ", tok[i]);
        } else {
            snprintf(buf, sizeof(buf), "R%u+%u ", off[i], len[i]);
        }
        s += buf;
    }
    return s;
}

} // namespace

int main(int argc, char ** argv) {
    goose_init(0, nullptr);

    const char * case_file = argc > 1 ? argv[1] : "build/partition-cases.tsv";
    std::vector<row> rows = load(case_file);

    size_t cases = 0, fragments = 0, failures = 0;
    for (size_t c = 0; c < rows.size(); c++) {
        const row & r = rows[c];

        stub_vocab vocab;
        int max_id = 0;
        for (const auto & t : r.tokens) {
            if (t.id > max_id) {
                max_id = t.id;
            }
        }
        vocab.tokens.resize((size_t) max_id + 1);
        for (const auto & t : r.tokens) {
            vocab.tokens[(size_t) t.id].text = t.text;
            vocab.tokens[(size_t) t.id].score = 0.0f;
            vocab.tokens[(size_t) t.id].attr = t.attr;
        }

        std::vector<llama_token> order;
        for (const auto & t : r.tokens) {
            order.push_back(t.id);
        }
        // The loader sorts by text length, longest first; ties cannot matter
        // because two different texts of the same length are never a prefix of
        // each other, so the order is fixed by id here for reproducibility.
        std::stable_sort(order.begin(), order.end(), [&](llama_token a, llama_token b) {
            const auto & ta = vocab.tokens[(size_t) a].text;
            const auto & tb = vocab.tokens[(size_t) b].text;
            if (ta.size() != tb.size()) {
                return ta.size() > tb.size();
            }
            return a < b;
        });

        partition_result want = native_partition(r.text, order, vocab, r.parse_special != 0);

        // Port inputs: per-id token text, concatenated, and the order.
        std::string blob;
        std::vector<int64_t> t_off(vocab.tokens.size(), 0), t_len(vocab.tokens.size(), 0),
                             t_attr(vocab.tokens.size(), 0), ord;
        for (size_t i = 0; i < vocab.tokens.size(); i++) {
            t_off[i] = (int64_t) blob.size();
            t_len[i] = (int64_t) vocab.tokens[i].text.size();
            t_attr[i] = vocab.tokens[i].attr;
            blob += vocab.tokens[i].text;
        }
        for (auto id : order) {
            ord.push_back(id);
        }

        const size_t cap = 4 * (r.text.size() + 2) + 16 * ord.size() + 16;
        std::vector<int64_t> o_kind(cap, -1), o_tok(cap, -1), o_off(cap, -1), o_len(cap, -1);

        sl_u8 in{(uint8_t *) r.text.data(), (int64_t) r.text.size()};
        sl_u8 ttext{(uint8_t *) blob.data(), (int64_t) blob.size()};
        sl_i64 so{(int64_t *) t_off.data(), (int64_t) t_off.size()};
        sl_i64 sl{(int64_t *) t_len.data(), (int64_t) t_len.size()};
        sl_i64 sa{(int64_t *) t_attr.data(), (int64_t) t_attr.size()};
        sl_i64 sord{(int64_t *) ord.data(), (int64_t) ord.size()};
        sl_i64 k{(int64_t *) o_kind.data(), (int64_t) o_kind.size()};
        sl_i64 tk{(int64_t *) o_tok.data(), (int64_t) o_tok.size()};
        sl_i64 of{(int64_t *) o_off.data(), (int64_t) o_off.size()};
        sl_i64 ln{(int64_t *) o_len.data(), (int64_t) o_len.size()};

        int64_t n = stp_partition(in, (int64_t) vocab.tokens.size(), so, sl, ttext, sa,
                                 sord, r.parse_special, k, tk, of, ln);

        std::vector<int> g_kind, g_tok;
        std::vector<unsigned> g_off, g_len;
        for (int64_t i = 0; i < n; i++) {
            g_kind.push_back((int) o_kind[(size_t) i]);
            g_tok.push_back((int) o_tok[(size_t) i]);
            g_off.push_back((unsigned) o_off[(size_t) i]);
            g_len.push_back((unsigned) o_len[(size_t) i]);
        }

        const std::string a = flat(want.type, want.token, want.offset, want.length);
        const std::string b = flat(g_kind, g_tok, g_off, g_len);

        cases += 1;
        fragments += want.type.size();
        if (a != b) {
            failures += 1;
            if (failures <= 10) {
                printf("case %zu parse_special=%d text=%zu bytes tokens=%zu\n  want %s\n  got  %s\n",
                       c, r.parse_special, r.text.size(), r.tokens.size(), a.c_str(), b.c_str());
            }
        }
    }

    printf("cases %zu, fragments %zu, failures %zu\n", cases, fragments, failures);
    return failures == 0 ? 0 : 1;
}
