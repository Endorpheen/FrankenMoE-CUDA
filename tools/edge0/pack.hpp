// Edge0 pack reader: strata pack index v3 and native experts v6.
//
// This is the first piece of the loader: it turns the two text tables into
// structures the model loader can consume, and validates the layout against
// the pool size. It does not touch weights.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace edge0 {

struct TensorRow {
    std::string name;
    int file = 0;      // 0 = dense.bin pool
    int dtype = 0;     // 0 = kept in the skeleton GGUF, 2 = f32, 4 = bf16
    int64_t offset = 0;
    int64_t nbytes = 0;
    int64_t pool_off = 0;
    int64_t nelem = 0;
};

struct Index {
    int64_t align = 0;
    int64_t pool = 0;
    int64_t declared_tensors = 0;
    std::vector<TensorRow> rows;
};

struct ExpertLayer {
    int layer = 0;
    int gate_type = 0;
    int down_type = 0;
    int64_t offset = 0;
    int64_t size = 0;
};

struct NativeExperts {
    std::vector<ExpertLayer> layers;
    int64_t record = 0;
    int64_t stride = 0;
};

// Returns false and fills error on a malformed table.
bool parse_index(const std::string & text, Index & out, std::string & error);
bool parse_native_experts(const std::string & text, NativeExperts & out, std::string & error);

// Checks the parsed index against the actual pool size: tensor count, 64-byte
// alignment of every in-pool row, offsets inside the pool, and the declared
// header values. Returns the number of violations and describes the first ones.
struct PackStats {
    int64_t tensors = 0;
    int64_t in_pool = 0;
    int64_t pool_bytes = 0;
    int64_t max_end = 0;
    int64_t misaligned = 0;
    int64_t out_of_pool = 0;
    int64_t bad_rows = 0;
    int64_t native_layers = 0;
    int64_t expert_record = 0;
    int64_t expert_stride = 0;
    int64_t bad_expert_type = 0;
    int64_t bad_expert_offset = 0;
};

PackStats validate(const Index & index, const NativeExperts & native, int64_t dense_size);

// Human-readable one-line summary per problem, for tests and diagnostics.
std::vector<std::string> problems(const Index & index, const NativeExperts & native, int64_t dense_size);

}  // namespace edge0
