// Goose bridge for the GGUF writer and the split plan (staged S1.2 trial).
//
// Built only when FG_GOOSE_GGUF=ON. The native writer in ggml/src/gguf.cpp
// stays compiled and keeps its original bodies under *_native(); this header
// is the only interface between that translation unit and the composed Goose
// module (goose/model/gguf-{metadata,writer,tensors,split}.goose).
//
// The native gguf_context is a private struct inside gguf.cpp (kv vector of
// std::string / std::vector<int8_t>, tensor infos with a ggml_tensor), so the
// bridge takes plain views that gguf.cpp fills: no C++ container or ggml type
// crosses this boundary.
//
// No model, no GPU, no server: this is a library-level seam.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace fg_goose {

// Which native call site asked for the bytes; only used for the witness count
// that proves the smoke gate dispatched into Goose.
enum entry {
    ENTRY_WRITE     = 0,  // gguf_write_to_buf
    ENTRY_META_SIZE = 1,  // gguf_get_meta_size
    ENTRY_META_DATA = 2,  // gguf_get_meta_data
    ENTRY_SPLIT     = 3,  // split_strategy::should_split
    ENTRY_LAYOUT    = 4,  // split_strategy::should_split, whole-plan form
    ENTRY_COUNT     = 5,
};

// One gguf_kv. Scalars carry their raw value bytes (1, 2, 4 or 8); numeric and
// bool arrays carry the element image; string arrays carry the C string list.
struct kv_view {
    const char *         key      = nullptr;
    int32_t              type     = 0;      // gguf_type of the scalar or element
    bool                 is_array = false;
    const void *         data     = nullptr;
    std::size_t          n_data   = 0;      // scalar: type size; array: element bytes
    std::size_t          n_elems  = 0;      // array length
    const char * const * strs     = nullptr;
    std::size_t          n_strs   = 0;
};

// One gguf_tensor_info. `ne` points at GGML_MAX_DIMS entries; `n_data` is 0 for
// a context that has no data image (only_meta writes accept that).
struct tensor_view {
    const char *    name    = nullptr;
    const int64_t * ne      = nullptr;
    int32_t         ndims   = 0;
    int32_t         type    = 0;
    const void *    data    = nullptr;
    std::size_t     n_data  = 0;
};

// Serialises the whole context (header, KV list, tensor descriptors, alignment
// padding and, unless only_meta, the tensor data section) through the Goose
// module. `out` is cleared first and left empty on refusal.
// Returns false when the module refuses the context (it mirrors the module's
// -1 contract: an invalid type, a violated reserved-key rule, a data image that
// does not match nbytes).
bool gguf_write(entry site, std::uint32_t version, std::size_t alignment,
                const kv_view * kv, std::size_t n_kv,
                const tensor_view * ts, std::size_t n_tensors,
                std::vector<std::int8_t> & out, bool only_meta);

// split_strategy::should_split. Returns 1 (split here), 0 (keep), or -1 for an
// invalid mode or an unusable tensor limit.
std::int64_t should_split(std::int32_t mode, std::int64_t i_tensor,
                          std::int64_t n_tensors, std::int64_t next_size,
                          std::int64_t n_split_tensors,
                          std::int64_t n_bytes_split);

// The split_strategy constructor's whole plan (gsp_split_layout): the shard
// count, with `t0[s]`/`t1[s]` receiving the tensor range of shard s. The split
// tool consumes the per-tensor predicate today; the whole-plan form is what the
// smoke gate checks the predicate against (layout/predicate consistency).
// Returns the shard count, or -1 when the module refuses the parameters.
std::int64_t split_layout(std::int64_t n_tensors, std::int32_t mode,
                          std::int64_t n_split_tensors, std::int64_t n_bytes_split,
                          bool no_tensor_first, const std::int64_t * nb,
                          std::int64_t * t0, std::int64_t * t1);

// Witness: how many times each entry point actually reached the Goose module.
std::uint64_t calls(entry site);

}  // namespace fg_goose
