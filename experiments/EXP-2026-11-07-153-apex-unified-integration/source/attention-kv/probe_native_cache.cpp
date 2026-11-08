// Probe: can we construct a real llama_kv_cache with synthetic hparams?
#include <cstdio>
#include <cstdint>
#include <array>
#include <bitset>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <list>
#include <deque>
#include <algorithm>
#include <stdexcept>

#include "llama.h"
#include "llama-arch.h"
#include "ggml.h"
#include "ggml-backend.h"
#include "llama-batch.h"
#include "llama-graph.h"
#include "llama-memory.h"

#define private public
#include "llama-model.h"
#include "llama-kv-cache.h"
#undef private

struct probe_model : llama_model {
    using llama_model::llama_model;
    void load_stats(llama_model_loader &) override {}
    void load_hparams(llama_model_loader &) override {}
    void load_vocab(llama_model_loader &) override {}
    bool load_tensors(llama_model_loader &) override { return true; }
    void load_arch_hparams(llama_model_loader &) override {}
    void load_arch_tensors(llama_model_loader &) override {}
    std::unique_ptr<llm_graph_context> build_arch_graph(const llm_graph_params &) const override { return nullptr; }
};

int main() {
    llama_hparams h{};
    h.n_layer_all = 1;
    h.n_embd = 8;
    h.n_embd_head_k_full = 8;
    h.n_embd_head_v_full = 8;
    h.n_embd_head_k_swa = 8;
    h.n_embd_head_v_swa = 8;
    h.n_rot_full = 0;
    h.n_rot_swa = 0;
    h.n_head_arr[0] = 1;
    h.n_head_kv_arr[0] = 1;
    h.is_swa_impl[0] = 0;
    h.n_layer_kv_from_start = -1;
    h.rope_type = LLAMA_ROPE_TYPE_NONE;
    h.no_alloc = false;
    h.vocab_only = false;

    std::printf("hparams: layer_all=%u head_k=%u k_gqa=%u v_gqa=%u n_pos=%u has_kv=%d has_rope=%d\n",
            h.n_layer_all, h.n_embd_head_k(0), h.n_embd_k_gqa(0), h.n_embd_v_gqa(0),
            h.n_pos_per_embd(), h.has_kv(0) ? 1 : 0, h.has_rope(0) ? 1 : 0);

    llama_model_params mp = llama_model_default_params();
    probe_model model(mp);
    model.arch = LLM_ARCH_UNKNOWN;
    std::printf("model constructed arch=%d\n", (int) model.arch);

    using kv_t = llama_kv_cache;
    kv_t * kv = new kv_t(model, h, GGML_TYPE_F32, GGML_TYPE_F32,
                         /*v_trans*/ false, /*offload*/ false, /*unified*/ true,
                         /*kv_size*/ 16, /*n_seq_max*/ 1, /*n_pad*/ 16,
                         /*n_swa*/ 0, LLAMA_SWA_TYPE_NONE, /*mem_other*/ nullptr,
                         kv_t::layer_filter_cb{}, kv_t::layer_reuse_cb{}, kv_t::layer_share_cb{}, "probe");
    std::printf("cache constructed size=%u streams=%u layers=%zu\n",
            kv->get_size(), kv->get_n_stream(), kv->layers.size());
    kv->seq_add(0, -1, -1, 0);
    delete kv;
    std::printf("ok\n");
    return 0;
}
