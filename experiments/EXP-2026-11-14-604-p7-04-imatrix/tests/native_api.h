// Shared types between tests/probe.cpp (generated) and the two native shims.
//
// The shims are separate translation units because quantize.cpp and
// llama-quant.cpp define colliding file-local names.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct n_entry {
    std::string          name;
    std::vector<float>   sums;
    std::vector<int64_t> counts;
};

struct n_imx {
    bool                     ok           = false;
    bool                     is_legacy    = false;
    bool                     has_metadata = false;
    int32_t                  chunk_count  = 0;
    int32_t                  chunk_size   = 0;
    std::vector<n_entry>     entries;    // std::map order
    std::vector<std::string> datasets;
};

// common_imatrix_load over a path the probe owns; 0 on success.
int n_raw(const std::string & path, n_imx & out);

// load_imatrix and prepare_imatrix from tools/quantize: both exit(1) on the
// native error paths, which the probe observes through a child process.
void n_norm(const std::string & path, std::vector<n_entry> & out, int & chunk_count);
void n_prep(const std::string & path, const std::vector<std::string> & excluded,
            const std::vector<std::string> & included,
            std::vector<std::string> & kept, int & last_call);

// remap_layer keeps `mapped` and `next_id` across calls; remap_imatrix
// GGML_ABORTs on a missing block mapping.
std::string n_remap_layer(const std::string & name, const std::vector<int> & prune,
                          std::map<int, std::string> & mapped, int & next_id);
std::string n_remap_imatrix(const std::string & name, const std::map<int, std::string> & mapped);
