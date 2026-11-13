#include "pack.hpp"

#include <sstream>

namespace edge0 {

namespace {

std::vector<std::string> split(const std::string & line) {
    std::vector<std::string> out;
    std::istringstream stream(line);
    std::string word;
    while (stream >> word) {
        out.push_back(word);
    }
    return out;
}

bool to_i64(const std::string & word, int64_t & value) {
    try {
        size_t used = 0;
        value = std::stoll(word, &used, 10);
        return used == word.size();
    } catch (...) {
        return false;
    }
}

}  // namespace

bool parse_index(const std::string & text, Index & out, std::string & error) {
    out = Index{};
    std::istringstream stream(text);
    std::string line;
    int line_no = 0;
    while (std::getline(stream, line)) {
        ++line_no;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        if (line[0] == '#') {
            const auto words = split(line);
            for (size_t i = 0; i + 1 < words.size(); ++i) {
                int64_t value = 0;
                if (!to_i64(words[i + 1], value)) {
                    continue;
                }
                if (words[i] == "align") {
                    out.align = value;
                } else if (words[i] == "pool") {
                    out.pool = value;
                } else if (words[i] == "tensors") {
                    out.declared_tensors = value;
                }
            }
            continue;
        }
        const auto words = split(line);
        if (words.size() < 5) {
            error = "short row at line " + std::to_string(line_no);
            return false;
        }
        TensorRow row;
        row.name = words[0];
        int64_t value = 0;
        if (!to_i64(words[1], value)) { error = "file at line " + std::to_string(line_no); return false; }
        row.file = (int) value;
        if (!to_i64(words[2], value)) { error = "dtype at line " + std::to_string(line_no); return false; }
        row.dtype = (int) value;
        if (!to_i64(words[3], row.offset)) { error = "offset at line " + std::to_string(line_no); return false; }
        if (!to_i64(words[4], row.nbytes)) { error = "nbytes at line " + std::to_string(line_no); return false; }
        if (words.size() > 5 && to_i64(words[5], row.pool_off)) {
            // pool offset is informative; a zero nbytes row is not in the pool
        }
        if (words.size() > 6) {
            to_i64(words[6], row.nelem);
        }
        out.rows.push_back(std::move(row));
    }
    return true;
}

bool parse_native_experts(const std::string & text, NativeExperts & out, std::string & error) {
    out = NativeExperts{};
    std::istringstream stream(text);
    std::string line;
    int line_no = 0;
    while (std::getline(stream, line)) {
        ++line_no;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto words = split(line);
        if (words.size() != 5) {
            error = "expected 5 columns at line " + std::to_string(line_no);
            return false;
        }
        int64_t values[5];
        for (int i = 0; i < 5; ++i) {
            if (!to_i64(words[i], values[i])) {
                error = "bad number at line " + std::to_string(line_no);
                return false;
            }
        }
        ExpertLayer layer;
        layer.layer = (int) values[0];
        layer.gate_type = (int) values[1];
        layer.down_type = (int) values[2];
        layer.offset = values[3];
        layer.size = values[4];
        out.layers.push_back(layer);
    }
    if (!out.layers.empty()) {
        out.record = out.layers.front().size;
        out.stride = 512 * out.record;
    }
    return true;
}

PackStats validate(const Index & index, const NativeExperts & native, int64_t dense_size) {
    PackStats stats;
    stats.tensors = (int64_t) index.rows.size();
    stats.pool_bytes = index.pool;
    for (const auto & row : index.rows) {
        if (row.nbytes <= 0) {
            continue;
        }
        ++stats.in_pool;
        const int64_t end = row.offset + row.nbytes;
        if (end > stats.max_end) {
            stats.max_end = end;
        }
        if (index.align > 0 && row.offset % index.align != 0) {
            ++stats.misaligned;
        }
        if (end > dense_size) {
            ++stats.out_of_pool;
        }
    }
    stats.native_layers = (int64_t) native.layers.size();
    stats.expert_record = native.record;
    stats.expert_stride = native.stride;
    for (const auto & layer : native.layers) {
        if (layer.gate_type != 11 || layer.down_type != 20) {
            ++stats.bad_expert_type;
        }
        if (layer.offset != layer.layer * native.stride) {
            ++stats.bad_expert_offset;
        }
    }
    return stats;
}

std::vector<std::string> problems(const Index & index, const NativeExperts & native, int64_t dense_size) {
    std::vector<std::string> out;
    if (index.declared_tensors != (int64_t) index.rows.size()) {
        out.push_back("declared tensors " + std::to_string(index.declared_tensors) +
                      " != rows " + std::to_string(index.rows.size()));
    }
    if (index.pool != dense_size) {
        out.push_back("pool " + std::to_string(index.pool) +
                      " != dense.bin " + std::to_string(dense_size));
    }
    for (const auto & row : index.rows) {
        if (row.nbytes <= 0) {
            continue;
        }
        if (index.align > 0 && row.offset % index.align != 0) {
            out.push_back("misaligned tensor " + row.name);
        }
        if (row.offset + row.nbytes > dense_size) {
            out.push_back("tensor outside the pool " + row.name);
        }
        if (out.size() > 32) {
            break;
        }
    }
    for (const auto & layer : native.layers) {
        if (layer.gate_type != 11 || layer.down_type != 20) {
            out.push_back("expert types at layer " + std::to_string(layer.layer));
        }
        if (layer.offset != layer.layer * native.stride) {
            out.push_back("expert offset at layer " + std::to_string(layer.layer));
        }
        if (out.size() > 32) {
            break;
        }
    }
    return out;
}

}  // namespace edge0
