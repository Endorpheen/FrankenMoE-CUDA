// Native oracle, part 1: common_imatrix_load, load_imatrix, prepare_imatrix.
#include "native_api.h"

#include "imatrix-loader.cpp"

// quantize.cpp owns main(); rename it so this translation unit can use the
// two file-local functions it defines.
#define main quantize_cli_main
#include "quantize.cpp"
#undef main

int n_raw(const std::string & path, n_imx & out) {
    common_imatrix im;
    if (!common_imatrix_load(path, im)) {
        return 1;
    }
    out.ok           = true;
    out.is_legacy    = im.is_legacy;
    out.has_metadata = im.has_metadata;
    out.chunk_count  = im.chunk_count;
    out.chunk_size   = im.chunk_size;
    for (const auto & [name, e] : im.entries) {
        out.entries.push_back({ name, e.sums, e.counts });
    }
    out.datasets = im.datasets;
    return 0;
}

void n_norm(const std::string & path, std::vector<n_entry> & out, int & chunk_count) {
    std::vector<std::string> datasets;
    std::unordered_map<std::string, std::vector<float>> data;
    chunk_count = load_imatrix(path, datasets, data);
    // The native container is unordered; sort so the diff is stable.
    std::map<std::string, std::vector<float>> sorted(data.begin(), data.end());
    for (const auto & [name, sums] : sorted) {
        out.push_back({ name, sums, {} });
    }
}

void n_prep(const std::string & path, const std::vector<std::string> & excluded,
            const std::vector<std::string> & included,
            std::vector<std::string> & kept, int & last_call) {
    std::vector<std::string> datasets;
    std::unordered_map<std::string, std::vector<float>> data;
    last_call = prepare_imatrix(path, datasets, included, excluded, data);
    for (const auto & e : data) {
        kept.push_back(e.first);
    }
    std::sort(kept.begin(), kept.end());
}
