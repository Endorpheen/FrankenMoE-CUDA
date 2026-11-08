// worker-2/kv-q4-final/q4-fixture.cpp
// Real Q4_0 K/V synthetic CPU cache: Goose ownership + n_rot/R_K/R_V,
// normal init_batch from a real llama_batch_allocr, Q4 tensor layout and
// native wire/state diff. No model, GPU or benchmark.
#include "kv-native-adapter.h"
#include "llama-batch.h"
#include "llama-io.h"
#include "llama-arch.h"
#include "llama-vocab.h"
#include "llama.h"
#include "ggml.h"
#include "ggml-backend.h"

#include <cmath>
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

#include "q4-factory.h"

extern "C" {
void    fg153_q4_force_oom(int64_t after);
void    fg153_q4_reset_oom(void);
int64_t fg153_q4_witness(void);
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
    h.n_embd = 64;
    h.n_embd_head_k_full = 64;
    h.n_embd_head_v_full = 64;
    h.n_embd_head_k_swa = 64;
    h.n_embd_head_v_swa = 64;
    h.n_rot_full = 0;
    h.n_rot_swa = 0;
    for (int i = 0; i < 4; ++i) { h.n_head_arr[i] = 1; h.n_head_kv_arr[i] = 1; h.is_swa_impl[i] = 0; }
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
        model.attn_rot = true;
    }
    llama_kv_cache * make() {
        return new llama_kv_cache(model, h, GGML_TYPE_Q4_0, GGML_TYPE_Q4_0, false, false, true,
                16, 1, 16, 0, LLAMA_SWA_TYPE_NONE, nullptr,
                filter, llama_kv_cache::layer_reuse_cb{}, llama_kv_cache::layer_share_cb{}, "q4");
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
    llama_kv_cache * a = H.make();
    llama_kv_cache * b = H.make();

    check(a->layers.size() == 3, "q4_attention_filter");
    check(a->ctxs_bufs.size() == 1 && a->ctxs_bufs[0].second != nullptr, "q4_cpu_buffer");
    check(a->layers[0].k->type == GGML_TYPE_Q4_0 && a->layers[0].v->type == GGML_TYPE_Q4_0, "q4_tensor_type");
    check(a->n_rot_k == 64 && a->n_rot_v == 64, "q4_n_rot");
    check(a->attn_rot_hadamard.count(64) == 1, "q4_hadamard_present");
    const auto & hm = a->attn_rot_hadamard[64];
    check(hm.size() == 64 * 64, "q4_hadamard_size");
    check(std::fabs(hm[0] - 1.0f / 8.0f) < 1e-6f, "q4_hadamard_scale");
    check(std::fabs(hm[64 + 0] - 0.125f) < 1e-6f && std::fabs(hm[64 + 1] + 0.125f) < 1e-6f, "q4_hadamard_pattern");

    // Normal init_batch from a real llama_batch_allocr.
    llama_vocab vocab;
    llama_batch_ext be(4, 64, 64, 1, nullptr, 128, 1);
    const int32_t t0 = be.add_token(0);
    const int32_t t1 = be.add_token(0);
    bool be_ok = t0 >= 0 && t1 >= 0;
    be_ok = be_ok && be.set_token_id(t0, 1) && be.set_token_id(t1, 2);
    be_ok = be_ok && be.add_seq(t0, 0) && be.add_seq(t1, 0);
    llama_pos p0[4] = { 0, 0, 0, 0 }, p1[4] = { 1, 0, 0, 0 };
    be_ok = be_ok && be.set_token_pos(t0, p0) && be.set_token_pos(t1, p1);
    check(be_ok, "batch_ext_built");
    llama_batch_allocr balloc(1);
    const bool init_ok = balloc.init(be, vocab, false);
    check(init_ok, "allocr_init");
    auto ctx = init_ok ? a->init_batch(balloc, 2, false) : llama_memory_context_ptr{};
    auto * kctx = ctx ? static_cast<llama_kv_cache_context *>(ctx.get()) : nullptr;
    check(kctx && kctx->get_status() == LLAMA_MEMORY_STATUS_SUCCESS, "init_batch_status");
    check(kctx && kctx->get_n_kv() >= 0, "init_batch_n_kv");
    check(kctx && kctx->apply(), "init_batch_apply");
    check(!kctx || !kctx->next(), "init_batch_next_end");
    check(!a->get_cells(0).is_empty(0) && a->get_cells(0).pos_get(0) == 0, "init_batch_cell0");
    check(a->get_cells(0).pos_get(1) == 1 && a->get_cells(0).seq_has(1, 0), "init_batch_cell1");

    // Same two cells on the second cache via the normal slot/apply path.
    ub_holder u0(2, 0);
    auto sb = b->find_slot(u0.ub, false);
    check(!sb.empty(), "b_find_slot");
    b->apply_ubatch(sb, u0.ub);
    check(cells_equal(a, b), "cells_equal_after_init_batch");

    // Q4 tensor row bytes: write the same pattern into both caches.
    ggml_tensor * ka = a->get_k_storage(0);
    ggml_tensor * kb = b->get_k_storage(0);
    std::vector<uint8_t> pat(ggml_row_size(ka->type, 64));
    for (size_t i = 0; i < pat.size(); ++i) pat[i] = (uint8_t) (0x40 + (i % 16));
    ggml_backend_tensor_set(ka, pat.data(), 0, pat.size());
    ggml_backend_tensor_set(kb, pat.data(), 0, pat.size());

    mem_write wa, wb;
    a->state_write(wa);
    b->state_write(wb);
    check(wa.buf == wb.buf && !wa.buf.empty(), "q4_state_bytes_equal");
    a->clear(false);
    b->clear(false);
    mem_read ra(wa.buf), rb(wb.buf);
    a->state_read(rb);
    b->state_read(ra);
    check(cells_equal(a, b), "q4_state_cross_roundtrip");

    // Concrete hybrid context factory with Q4 sinfos/ubatches.
    {
        ub_holder u1(1, 5);
        auto s1 = a->find_slot(u1.ub, false);
        auto s2 = b->find_slot(u1.ub, false);
        std::vector<llama_ubatch> ubs = { u1.ub };
        auto ca = fg153_q4_context_batch(a, { s1 }, ubs);
        auto cb = fg153_q4_context_batch(b, { s2 }, ubs);
        auto * kca = static_cast<llama_kv_cache_context *>(ca.get());
        auto * kcb = static_cast<llama_kv_cache_context *>(cb.get());
        check(kca->get_status() == kcb->get_status(), "q4_factory_status");
        check(kca->apply() && kcb->apply(), "q4_factory_apply");
        check(cells_equal(a, b), "q4_factory_cells");
    }

    check(fg153_q4_witness() != 0, "q4_witness");

    // Rollback: native body path (FG153_KV_Q4=0 is exercised by the parent env).
    delete a;
    delete b;
    // Address reuse: construct/destroy with the Q4 rotation tail.
    for (int i = 0; i < 2; ++i) {
        llama_kv_cache * t = H.make();
        check(t->n_rot_k == 64 && t->attn_rot_hadamard.count(64) == 1, "q4_reuse_rotation");
        delete t;
    }
    std::printf(failures == 0 ? "Q4 PASS\n" : "Q4 FAIL\n");
    return failures == 0 ? 0 : 1;
}
