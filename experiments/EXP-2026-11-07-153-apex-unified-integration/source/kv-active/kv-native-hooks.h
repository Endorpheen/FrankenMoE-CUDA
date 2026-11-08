// worker-2/kv-native-hooks.h
//
// Typed access hooks for the REAL llama_kv_cache / llama_kv_cells /
// llama_kv_cache_context. No offsets are invented: every member below is
// named exactly as in inputs/llama-kv-cache.h and inputs/llama-kv-cells.h.
//
// Access is granted TU-locally in kv-native-adapter.cpp with
//   #define private public
// around the native model/kv-cache includes. This changes no layout and no
// ABI (the access specifier is not part of the object), so the real headers
// stay untouched. All member names below are the actual native names.

#pragma once

#include "llama-kv-cache.h"
#include "llama-kv-cells.h"

#include <bitset>
#include <cstdint>
#include <set>
#include <utility>
#include <vector>

namespace llama_kv_goose {

struct access {
    // llama_kv_cache
    static llama_kv_cells & cells(llama_kv_cache & kv, uint32_t strm);
    static uint32_t & head(llama_kv_cache & kv, uint32_t strm);
    static bool has_other(llama_kv_cache & kv);
    static uint32_t n_stream(const llama_kv_cache & kv);
    static uint32_t n_seq_max(const llama_kv_cache & kv);
    static uint32_t n_pad(const llama_kv_cache & kv);
    static uint32_t n_swa(const llama_kv_cache & kv);
    static llama_swa_type swa_type(const llama_kv_cache & kv);
    static bool arch_step35(const llama_kv_cache & kv);
    static int64_t n_pos_per_embd(const llama_kv_cache & kv);
    static uint32_t n_layers(const llama_kv_cache & kv);
    static uint32_t layer_il(const llama_kv_cache & kv, uint32_t il);
    static bool layer_has_v(const llama_kv_cache & kv, uint32_t il);
    static bool layer_has_rope(const llama_kv_cache & kv, uint32_t il);
    static int64_t ple_n_heads(const llama_kv_cache & kv);
    static int64_t ple_image_token(const llama_kv_cache & kv);
    static int64_t ple_eos_token(const llama_kv_cache & kv);
    static bool rope_none(const llama_kv_cache & kv);
    static bool v_trans(const llama_kv_cache & kv);
    static uint32_t n_rot_k(const llama_kv_cache & kv);
    static uint32_t n_rot_v(const llama_kv_cache & kv);
    static bool has_cell_ext(const llama_kv_cache & kv);
    static uint32_t n_embd_k(const llama_kv_cache & kv, uint32_t il);
    static uint32_t n_embd_v(const llama_kv_cache & kv, uint32_t il);
    static ggml_type type_k(const llama_kv_cache & kv);
    static ggml_type type_v(const llama_kv_cache & kv);
    static llama_kv_cache::stream_copy_info & sc_info(llama_kv_cache & kv);
    static const std::vector<uint32_t> & seq_to_stream(const llama_kv_cache & kv);

    // llama_kv_cells
    static std::vector<llama_pos> & pos(llama_kv_cells & c);
    static std::vector<llama_kv_cell_ext> & ext(llama_kv_cells & c);
    static std::vector<llama_pos> & shift(llama_kv_cells & c);
    static std::vector<std::bitset<LLAMA_MAX_SEQ>> & seq(llama_kv_cells & c);
    static std::set<uint32_t> & used(llama_kv_cells & c);
    static std::set<std::pair<llama_pos, uint32_t>> (&seq_pos(llama_kv_cells & c))[LLAMA_MAX_SEQ];
    static bool & has_shift(llama_kv_cells & c);

    // llama_kv_cache_context
    static int32_t & n_kv(llama_kv_cache_context & ctx);
    static size_t & i_cur(llama_kv_cache_context & ctx);
};

} // namespace llama_kv_goose
