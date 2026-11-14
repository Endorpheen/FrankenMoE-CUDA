// Differential gate for the vocab-type / pre-tokenizer dispatch slice (S1).
//
// The oracle is the native text of llama_vocab::impl::load, extracted from
// src/llama-vocab.cpp by tests/make-native.py; the port is
// source/vocab-type.goose. Both print one canonical record per case and the
// gate compares the bytes.
#include "native-dispatch.h"
#include "vocab-type.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct row {
    std::string model;
    std::string pre;
    int has_merges;
    int merges_kv_array;
    int merges_arr_string;
    int n_merges;
    int charsmap_present;
    int charsmap_kv_array;
    int charsmap_i8u8;
    int charsmap_len;
    int charsmap_blob;
};

std::string unescape(const std::string & s) {
    // "none" stands for the empty string so that a tab-separated line stays
    // unambiguous; anything else is taken verbatim.
    return s == "none" ? std::string() : s;
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
        std::istringstream ls(line);
        std::string f[11];
        for (int i = 0; i < 11; i++) {
            std::getline(ls, f[i], '\t');
        }
        row r;
        r.model             = unescape(f[0]);
        r.pre               = unescape(f[1]);
        r.has_merges        = std::stoi(f[2]);
        r.merges_kv_array   = std::stoi(f[3]);
        r.merges_arr_string = std::stoi(f[4]);
        r.n_merges          = std::stoi(f[5]);
        r.charsmap_present  = std::stoi(f[6]);
        r.charsmap_kv_array = std::stoi(f[7]);
        r.charsmap_i8u8     = std::stoi(f[8]);
        r.charsmap_len      = std::stoi(f[9]);
        r.charsmap_blob     = std::stoi(f[10]);
        rows.push_back(r);
    }
    return rows;
}

} // namespace

int main(int argc, char ** argv) {
    goose_init(0, nullptr);

    const char * case_file = argc > 1 && strcmp(argv[1], "--quick") != 0
                             ? argv[1]
                             : "build/dispatch-cases.tsv";
    const bool quick = argc > 1 && strcmp(argv[1], "--quick") == 0;

    std::vector<row> rows = load(case_file);
    size_t limit = quick ? 240 : rows.size();

    size_t cases = 0, bytes = 0, failures = 0;
    for (size_t i = 0; i < rows.size() && i < limit; i++) {
        const row & r = rows[i];

        case_cfg cfg;
        cfg.has_merges        = r.has_merges != 0;
        cfg.merges_kv_type    = r.merges_kv_array   ? GGUF_TYPE_ARRAY : GGUF_TYPE_INT32;
        cfg.merges_arr_type   = r.merges_arr_string ? GGUF_TYPE_STRING : GGUF_TYPE_INT32;
        cfg.n_merges          = r.n_merges;
        cfg.has_charsmap      = r.charsmap_present != 0;
        cfg.charsmap_kv_type  = r.charsmap_kv_array ? GGUF_TYPE_ARRAY : GGUF_TYPE_INT32;
        cfg.charsmap_arr_type = r.charsmap_i8u8 ? GGUF_TYPE_INT8 : GGUF_TYPE_FLOAT32;
        cfg.charsmap_len      = r.charsmap_len;
        cfg.charsmap_blob     = (uint32_t) r.charsmap_blob;

        const std::string want = format_record(native_dispatch(r.model, r.pre, cfg));

        char buf[1024];
        sl_u8 model{(uint8_t *) r.model.data(), (int64_t) r.model.size()};
        sl_u8 pre{(uint8_t *) r.pre.data(), (int64_t) r.pre.size()};
        sl_u8 out{(uint8_t *) buf, (int64_t) sizeof(buf)};
        int64_t n = vt_dispatch(model, pre,
                                r.has_merges,
                                r.merges_kv_array,
                                r.merges_arr_string,
                                r.n_merges,
                                r.charsmap_present,
                                r.charsmap_kv_array,
                                r.charsmap_i8u8,
                                r.charsmap_len,
                                r.charsmap_blob,
                                out);
        std::string got(buf, (size_t) n);

        cases += 1;
        bytes += want.size();
        if (want != got) {
            failures += 1;
            if (failures <= 10) {
                printf("case %zu model='%s' pre='%s' cfg=(%d,%d,%d,%d|%d,%d,%d,%d,%d)\n"
                       "  want %s\n  got  %s\n",
                       i, r.model.c_str(), r.pre.c_str(),
                       r.has_merges, r.merges_kv_array, r.merges_arr_string, r.n_merges,
                       r.charsmap_present, r.charsmap_kv_array, r.charsmap_i8u8,
                       r.charsmap_len, r.charsmap_blob,
                       want.c_str(), got.c_str());
            }
        }
    }

    printf("cases %zu, record bytes %zu, failures %zu\n", cases, bytes, failures);
    return failures == 0 ? 0 : 1;
}
