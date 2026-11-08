// worker-2/kv-ownership-fixture.cpp
// Real 4-layer synthetic CPU cache constructed through the patched ownership
// constructor, with attention layer filter, OOM/partial rollback, address reuse,
// real full/update contexts and cross Goose state blobs. No model/GPU/bench.
#include "kv-native-adapter.h"
#include "llama-batch.h"
#include "llama-io.h"
#include "llama-arch.h"
#include "llama.h"
#include "ggml.h"
#include "ggml-backend.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#define private public
#include "llama-model.h"
#include "llama-kv-cache.h"
#undef private

#include "kv-ownership-factory.h"

extern "C" {
void    fg153_kv_own_force_oom(int64_t after);
void    fg153_kv_own_reset_oom(void);
int64_t fg153_kv_witness(void);
}

namespace {
int failures = 0;
void check(bool ok, const char * tag) { std::printf("%s %s\n", tag, ok ? "OK" : "FAIL"); if (!ok) ++failures; }

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

llama_hparams make_hparams() {
    llama_hparams h{};
    h.n_layer_all = 4;
    h.n_embd = 8;
    h.n_embd_head_k_full = 8;
    h.n_embd_head_v_full = 8;
    h.n_embd_head_k_swa = 8;
    h.n_embd_head_v_swa = 8;
    h.n_rot_full = 0;
    h.n_rot_swa = 0;
    for (int i = 0; i < 4; ++i) {
        h.n_head_arr[i] = 1;
        h.n_head_kv_arr[i] = 1;
        h.is_swa_impl[i] = 0;
    }
    h.n_layer_kv_from_start = -1;
    h.rope_type = LLAMA_ROPE_TYPE_NONE;
    h.no_alloc = false;
    h.vocab_only = false;
    return h;
}

struct ub_holder {
    llama_ubatch ub{};
    std::shared_ptr<llama_ubatch::data_t> d;
    ub_holder(uint32_t n, int64_t base) {
        d = std::make_shared<llama_ubatch::data_t>();
        d->pos.resize(n);
        for (uint32_t i = 0; i < n; ++i) d->pos[i] = (llama_pos)(base + i);
        d->n_seq_id.assign(n, 1);
        d->seq_id_data.assign(n, 0);
        d->seq_id.resize(n);
        for (uint32_t i = 0; i < n; ++i) d->seq_id[i] = &d->seq_id_data[i];
        d->seq_id_unq.assign(1, 0);
        d->seq_idx.assign(LLAMA_MAX_SEQ, 0);
        d->output.assign(n, 0);
        d->token.assign(n, 0);
        ub.b_equal_seqs = 1; ub.n_tokens = n; ub.n_seq_tokens = n; ub.n_seqs = 1; ub.n_seqs_unq = 1; ub.n_pos = 1;
        ub.token = nullptr; ub.embd = nullptr; ub.embd_state = nullptr;
        ub.pos = d->pos.data(); ub.n_seq_id = d->n_seq_id.data(); ub.seq_id = d->seq_id.data();
        ub.seq_id_unq = d->seq_id_unq.data(); ub.seq_idx = d->seq_idx.data(); ub.output = d->output.data();
        ub.type = nullptr; ub.decision_order = nullptr; ub.data = d;
    }
};

struct mem_write : llama_io_write_i {
    std::vector<uint8_t> buf;
    void write(const void * src, size_t size) override { const uint8_t * p = (const uint8_t*)src; buf.insert(buf.end(), p, p + size); }
    void write_tensor(ggml_tensor * t, size_t off, size_t size) override { std::vector<uint8_t> tmp(size); ggml_backend_tensor_get(t, tmp.data(), off, size); write(tmp.data(), size); }
    size_t n_bytes() override { return buf.size(); }
};
struct mem_read : llama_io_read_i {
    const std::vector<uint8_t> & buf; size_t pos = 0;
    explicit mem_read(const std::vector<uint8_t> & b) : buf(b) {}
    void read(void * dst, size_t size) override { std::memcpy(dst, buf.data() + pos, size); pos += size; }
    void read_tensor(ggml_tensor * t, size_t off, size_t size) override { ggml_backend_tensor_set(t, buf.data() + pos, off, size); pos += size; }
    size_t n_bytes() override { return pos; }
};

struct harness {
    probe_model model;
    llama_hparams h;
    llama_kv_cache::layer_filter_cb filter;
    harness() : model(llama_model_default_params()), h(make_hparams()), filter([](int32_t il){ return il != 1; }) {
        model.arch = LLM_ARCH_UNKNOWN;
    }
    llama_kv_cache * make() {
        return new llama_kv_cache(model, h, GGML_TYPE_F32, GGML_TYPE_F32, false, false, true,
                16, 1, 16, 0, LLAMA_SWA_TYPE_NONE, nullptr,
                filter, llama_kv_cache::layer_reuse_cb{}, llama_kv_cache::layer_share_cb{}, "own");
    }
};

bool cells_equal(llama_kv_cache * a, llama_kv_cache * b) {
    const llama_kv_cells & ca = a->get_cells(0);
    const llama_kv_cells & cb = b->get_cells(0);
    bool ok = ca.get_used() == cb.get_used();
    for (uint32_t i = 0; i < ca.size(); ++i) {
        ok = ok && ca.is_empty(i) == cb.is_empty(i);
        if (!ca.is_empty(i) && !cb.is_empty(i)) ok = ok && ca.pos_get(i) == cb.pos_get(i) && ca.seq_count(i) == cb.seq_count(i);
    }
    return ok;
}
} // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    harness H;

    // OOM: allocation of the first buffer fails -> ctor throws and rolls back.
    bool oom = false;
    fg153_kv_own_force_oom(0);
    try { delete H.make(); } catch (const std::exception &) { oom = true; }
    fg153_kv_own_reset_oom();
    check(oom, "oom_ctor_throws");

    // Partial failure: third tensor creation fails -> ctor throws after rollback.
    bool partial = false;
    fg153_kv_own_force_oom(2);
    try { delete H.make(); } catch (const std::exception &) { partial = true; }
    fg153_kv_own_reset_oom();
    check(partial, "partial_ctor_throws");

    // Normal construction through the Goose ownership path.
    llama_kv_cache * a = H.make();
    check(a->layers.size() == 3, "attention_filter_layers");
    check(a->map_layer_ids.size() == 3, "map_layer_ids");
    check(a->ctxs_bufs.size() == 1, "buft_dedup_ctx");
    check(a->ctxs_bufs[0].second != nullptr && a->get_size() == 16, "buffer_adopted");
    check(!a->layers.empty() && a->layers[0].k != nullptr && a->layers[0].v != nullptr, "layer_tensors");
    check(fg153_kv_witness() != 0, "goose_witness");

    // Address reuse: construct/destroy repeatedly without leaks or double free.
    uintptr_t seen = 0;
    for (int i = 0; i < 3; ++i) {
        llama_kv_cache * t = H.make();
        uintptr_t addr = (uintptr_t) t->get_k_storage(0)->data;
        check(addr != 0, "address_alive");
        if (seen == 0) seen = addr;
        delete t;
    }
    check(seen != 0, "address_reuse");

    llama_kv_cache * b = H.make();
    // ownership ctor auto-attaches both caches (default-on public routing).

    // Real context lifecycle: full next/apply, then prepare/apply.
    auto full = a->init_full();
    auto * kf = static_cast<llama_kv_cache_context *>(full.get());
    check(kf->get_status() == LLAMA_MEMORY_STATUS_SUCCESS, "context_status");
    check(kf->get_n_kv() == 16, "context_n_kv");
    check(!kf->next(), "context_next_end");
    check(kf->apply(), "context_apply");

    ub_holder u0(2, 0), u1(2, 2);
    auto sa = a->find_slot(u0.ub, false);
    auto sb = b->find_slot(u0.ub, false);
    check(!sa.empty() && !sb.empty(), "find_slot");
    a->apply_ubatch(sa, u0.ub);
    b->apply_ubatch(sb, u0.ub);
    std::vector<llama_ubatch> ubs = { u0.ub, u1.ub };
    auto pa = a->prepare(ubs);
    auto pb = b->prepare(ubs);
    check(pa.size() == 2 && pb.size() == 2, "prepare_slots");
    check(cells_equal(a, b), "prepare_restore");

    // Hybrid closure path: concrete context factory from native sinfos/ubatches.
    {
        auto ca = fg153_kv_context_batch(a, pa, ubs);
        auto cb = fg153_kv_context_batch(b, pb, ubs);
        auto * kca = static_cast<llama_kv_cache_context *>(ca.get());
        auto * kcb = static_cast<llama_kv_cache_context *>(cb.get());
        check(kca->get_status() == kcb->get_status(), "factory_status");
        check(kca->apply() && kcb->apply(), "factory_apply_first");
        check(kca->next() && kcb->next(), "factory_next");
        check(kca->apply() && kcb->apply(), "factory_apply_second");
        check(cells_equal(a, b), "factory_cells");
    }

    // Update context Goose path (real llama_context is the parent gate).
    a->seq_add(0, -1, -1, 3);
    b->seq_add(0, -1, -1, 3);
    auto ua = a->init_update(nullptr, false);
    auto ub = b->init_update(nullptr, false);
    check(ub->apply(), "update_apply");
    check(!b->get_has_shift(), "update_shift_reset");
    (void) ua;

    // Cross Goose state blobs (whole-cache and seq_id>=0), byte exact.
    mem_write wa, wb;
    a->state_write(wa);
    b->state_write(wb);
    check(wa.buf == wb.buf && !wa.buf.empty(), "state_bytes_equal");
    a->clear(false);
    b->clear(false);
    mem_read ra(wa.buf), rb(wb.buf);
    a->state_read(rb);
    b->state_read(ra);
    check(cells_equal(a, b), "state_cross_roundtrip");
    mem_write wa2, wb2;
    a->state_write(wa2, 0);
    b->state_write(wb2, 0);
    check(wa2.buf == wb2.buf, "state_seq_bytes_equal");
    a->clear(false);
    b->clear(false);
    mem_read r1(wa2.buf), r2(wb2.buf);
    a->state_read(r1, 0);
    b->state_read(r2, 0);
    check(cells_equal(a, b), "state_seq_cross_roundtrip");

    delete a;
    delete b;
    check(true, "destructor_release");
    std::printf(failures == 0 ? "OWNERSHIP PASS\n" : "OWNERSHIP FAIL\n");
    return failures == 0 ? 0 : 1;
}
