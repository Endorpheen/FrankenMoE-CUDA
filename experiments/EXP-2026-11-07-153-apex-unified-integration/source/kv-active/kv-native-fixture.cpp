// worker-2/kv-native-fixture.cpp
//
// Real CPU corpus: two REAL llama_kv_cache instances (native vs Goose-attached)
// with a synthetic 1-layer/1-head KV. Drives find_slot/apply/prepare/seq/state
// and the context subclass. No weights, no inference, no benchmark.

#include "kv-native-adapter.h"
#include "kv-bridge.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "llama.h"
#include "llama-arch.h"
#include "llama-batch.h"
#include "llama-graph.h"
#include "llama-memory.h"
#include "ggml.h"

#define private public
#include "llama-model.h"
#include "llama-kv-cache.h"
#undef private

extern "C" {
int64_t fg_goose_kv_lifecycle_entry(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg_goose_kv_seq_entry(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg_goose_cells_entry(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg_kv_get(int64_t, int64_t, int64_t);
int64_t fg_kv_obj(int64_t);
void    fg_kv_vec_push(int64_t, int64_t);
void    fg_kv_destroy(int64_t);
}

namespace {

int failures = 0;

void check(bool ok, const char * tag) {
    std::printf("%s %s\n", tag, ok ? "OK" : "FAIL");
    if (!ok) { ++failures; }
}

int64_t life(int64_t mode, int64_t a, int64_t b = 0, int64_t c = 0,
             int64_t d = 0, int64_t e = 0, int64_t f = 0) {
    return fg_goose_kv_lifecycle_entry(mode, a, b, c, d, e, f);
}

int64_t cell(int64_t h, int64_t mode, int64_t a = 0, int64_t b = 0, int64_t c = 0,
             int64_t d = 0, int64_t e = 0) {
    return fg_goose_cells_entry(mode, h, a, b, c, d, e);
}

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
    return h;
}

struct ub_holder {
    llama_ubatch ub{};
    llama_ubatch::data_t d;
    std::vector<llama_seq_id> sid;

    ub_holder(uint32_t n, int64_t base_pos, llama_token tok = 0, bool with_token = false) {
        d.token.assign(n, tok);
        d.pos.resize(n);
        for (uint32_t i = 0; i < n; ++i) { d.pos[i] = (llama_pos) (base_pos + i); }
        d.n_seq_id.assign(n, 1);
        d.seq_id_data.assign(n, 0);
        d.seq_id.resize(n);
        for (uint32_t i = 0; i < n; ++i) { d.seq_id[i] = &d.seq_id_data[i]; }
        d.seq_id_unq.assign(1, 0);
        d.seq_idx.assign(LLAMA_MAX_SEQ, 0);
        d.output.assign(n, 0);

        ub.b_equal_seqs = 1;
        ub.n_tokens = n;
        ub.n_seq_tokens = n;
        ub.n_seqs = 1;
        ub.n_seqs_unq = 1;
        ub.n_pos = 1;
        ub.token = with_token ? d.token.data() : nullptr;
        ub.embd = nullptr;
        ub.embd_state = nullptr;
        ub.pos = d.pos.data();
        ub.n_seq_id = d.n_seq_id.data();
        ub.seq_id = d.seq_id.data();
        ub.seq_id_unq = d.seq_id_unq.data();
        ub.seq_idx = d.seq_idx.data();
        ub.output = d.output.data();
        ub.type = nullptr;
        ub.decision_order = nullptr;
        ub.data = std::make_shared<llama_ubatch::data_t>(d);
        // re-point at the shared copy: the ubatch may outlive this holder's d
        llama_ubatch::data_t & s = *ub.data;
        ub.token = with_token ? s.token.data() : nullptr;
        ub.pos = s.pos.data();
        ub.n_seq_id = s.n_seq_id.data();
        for (uint32_t i = 0; i < n; ++i) { s.seq_id[i] = &s.seq_id_data[i]; }
        ub.seq_id = s.seq_id.data();
        ub.seq_id_unq = s.seq_id_unq.data();
        ub.seq_idx = s.seq_idx.data();
        ub.output = s.output.data();
    }
};

struct two_caches {
    probe_model model;
    llama_hparams h;
    llama_kv_cache * a = nullptr;
    llama_kv_cache * b = nullptr;

    two_caches() : model(llama_model_default_params()), h(make_hparams()) {
        model.arch = LLM_ARCH_UNKNOWN;
        std::fprintf(stderr, "[corpus] make cache A\n");
        a = make_cache();
        std::fprintf(stderr, "[corpus] make cache B\n");
        b = make_cache();
        std::fprintf(stderr, "[corpus] caches ready\n");
    }
    ~two_caches() {
        delete a;
        delete b;
    }
    llama_kv_cache * make_cache() {
        return new llama_kv_cache(model, h, GGML_TYPE_F32, GGML_TYPE_F32,
                false, false, true, 16, 1, 16, 0, LLAMA_SWA_TYPE_NONE, nullptr,
                llama_kv_cache::layer_filter_cb{}, llama_kv_cache::layer_reuse_cb{},
                llama_kv_cache::layer_share_cb{}, "corpus");
    }
};

bool cells_equal(llama_kv_cache * a, int64_t gkv) {
    const llama_kv_cells & c = a->get_cells(0);
    const int64_t ch = fg_kv_get(gkv, 10, 0);
    const int64_t n = (int64_t) c.size();
    bool ok = true; ok = ok && (int64_t) c.get_used() == cell(ch, 6); ok = ok && (int64_t) c.used_min() == cell(ch, 7); ok = ok && (int64_t) c.used_max_p1() == cell(ch, 8); ok = ok && (int64_t) c.seq_pos_min(0) == cell(ch, 22, 0); ok = ok && (int64_t) c.seq_pos_max(0) == cell(ch, 23, 0);
    for (int64_t i = 0; i < n; ++i) {
        const bool emp = c.is_empty((uint32_t) i);
        ok = ok && (emp ? 1 : 0) == cell(ch, 5, i);
        ok = ok && (c.seq_has((uint32_t) i, 0) ? 1 : 0) == cell(ch, 17, i, 0);
        if (!emp) {
            ok = ok && (int64_t) c.pos_get((uint32_t) i) == cell(ch, 10, i);
            ok = ok && (int64_t) c.seq_count((uint32_t) i) == cell(ch, 16, i);
            const auto & e = c.ext_get((uint32_t) i);
            ok = ok && (int64_t) e.x == cell(ch, 28, i);
            ok = ok && (int64_t) e.y == cell(ch, 29, i);
            ok = ok && (int64_t) e.tok == cell(ch, 30, i);
        }
    }
    return ok;
}

int64_t make_ub_vec(std::initializer_list<llama_ubatch *> ubs) {
    const int64_t v = fg_kv_obj(4);
    for (llama_ubatch * u : ubs) { fg_kv_vec_push(v, (int64_t) (intptr_t) u); }
    return v;
}

} // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::fprintf(stderr, "[corpus] main enter\n");
    two_caches tc;
    std::printf("stage caches\n");
    const int64_t gkv = fg_kv_attach_real(tc.b);
    std::printf("stage attached\n");
    fg_kv_configure_real(gkv, tc.b);
    std::printf("stage configured\n");

    ub_holder H0(2, 0, 7, true), H1(2, 2), H2(1, 0), H3(2, 0), H4(3, 0), H5(12, 0);
    std::printf("stage ubatches\n");
    std::printf("stage cleared (fresh cache, no clear call)\n");

    // J1/J2: find_slot + apply.
    std::printf("stage J1\n");
    int64_t out[8] = {0};
    for (llama_ubatch * u : { &H0.ub, &H1.ub }) {
        auto s = tc.a->find_slot(*u, false);
        const int64_t gs = life(2, gkv, (int64_t) (intptr_t) u);
        check(!s.empty() && gs != 0, "find_slot");
        std::printf("native apply begin\n");
        if (!s.empty()) { tc.a->apply_ubatch(s, *u); }
        std::printf("native apply done, goose apply begin\n");
        if (gs != 0) { life(4, gkv, gs, (int64_t) (intptr_t) u); }
        std::printf("goose apply done\n");
    }
    check(cells_equal(tc.a, gkv), "cells_after_apply");

    // J3: fill the rest, then a one-token ubatch must fail on both.
    {
        auto s5 = tc.a->find_slot(H5.ub, false);
        const int64_t g5 = life(2, gkv, (int64_t) (intptr_t) &H5.ub);
        check(!s5.empty() && g5 != 0, "fill_rest");
        tc.a->apply_ubatch(s5, H5.ub);
        life(4, gkv, g5, (int64_t) (intptr_t) &H5.ub);
        check(cells_equal(tc.a, gkv), "cells_after_fill");
    }
    auto sf = tc.a->find_slot(H2.ub, false);
    const int64_t gf = life(2, gkv, (int64_t) (intptr_t) &H2.ub);
    check(sf.empty() && gf == 0, "full_cache_failure");

    // J4: prepare failure rolls back both.
    std::printf("stage J4\n");
    tc.a->clear(false);
    std::printf("J4 native cleared\n");
    fg_goose_kv_seq_entry(2, gkv, 0, 0, 0, 0);
    std::printf("J4 goose cleared\n");
    std::vector<llama_ubatch> ubs_fail = { H5.ub, H4.ub, H3.ub };
    std::printf("J4 native prepare begin\n");
    auto p_fail = tc.a->prepare(ubs_fail);
    std::printf("J4 native prepare done\n");
    const int64_t vf = make_ub_vec({ &H5.ub, &H4.ub, &H3.ub });
    std::printf("J4 goose prepare begin\n");
    const int64_t gf_prep = life(3, gkv, vf);
    std::printf("J4 goose prepare done\n");
    check(p_fail.empty() && gf_prep == 0, "prepare_failure");
    check(cells_equal(tc.a, gkv), "prepare_failure_rollback");
    fg_kv_destroy(vf);

    // J5: prepare success restores both, returns two slots.
    std::printf("stage J5\n");
    std::vector<llama_ubatch> ubs_ok = { H3.ub, H1.ub };
    auto p_ok = tc.a->prepare(ubs_ok);
    const int64_t vo = make_ub_vec({ &H3.ub, &H1.ub });
    const int64_t go_prep = life(3, gkv, vo);
    check(!p_ok.empty() && go_prep != 0, "prepare_success");
    check(cells_equal(tc.a, gkv), "prepare_success_restore");

    // J6: batch context subclass applies through the real base object.
    std::printf("stage J6\n");
    {
        llama_ubatch * c3 = new llama_ubatch(H3.ub);
        llama_ubatch * c1 = new llama_ubatch(H1.ub);
        const int64_t vc = make_ub_vec({ c3, c1 });
        const int64_t gctx = life(19, gkv, go_prep, vc);
        auto ctx = fg_goose_context_batch(tc.b, gkv, gctx);
        auto * kctx = static_cast<llama_kv_cache_context *>(ctx.get());
        check(kctx->get_status() == LLAMA_MEMORY_STATUS_SUCCESS, "ctx_status");
        check(kctx->get_n_kv() >= 0, "ctx_n_kv_initial");
        check(kctx->apply(), "ctx_apply_first");
        if (kctx->next()) {
            check(kctx->apply(), "ctx_apply_second");
        }
        // native side: same slots applied manually (single stream)
        for (size_t i = 0; i < p_ok.size(); ++i) {
            tc.a->apply_ubatch(p_ok[i], ubs_ok[i]);
        }
        check(cells_equal(tc.a, gkv), "ctx_apply_matches_native");
        check(kctx->get_n_kv() >= 0, "ctx_n_kv_after");
    }

    // J7: full context on both; concrete getters must work.
    std::printf("stage J7\n");
    {
        auto nctx = tc.a->init_full();
        auto * nk = static_cast<const llama_kv_cache_context *>(nctx.get());
        const int64_t n_nkv = nk->get_n_kv();
        const int64_t gctx = life(6, gkv);
        auto g = fg_goose_context_full(tc.b, gkv, gctx);
        auto * gk = static_cast<llama_kv_cache_context *>(g.get());
        check(gk->get_status() == LLAMA_MEMORY_STATUS_SUCCESS, "full_status");
        check((int64_t) gk->get_n_kv() == n_nkv, "full_n_kv");
        check(gk->apply(), "full_apply");
        check(cells_equal(tc.a, gkv), "full_apply_state");
    }

    // J8: seq_add / seq_div / seq_rm / state_clear against native.
    std::printf("stage J8\n");
    {
        tc.a->clear(false);
        fg_goose_kv_seq_entry(2, gkv, 0, 0, 0, 0);
        auto s = tc.a->find_slot(H3.ub, false);
        const int64_t gs = life(2, gkv, (int64_t) (intptr_t) &H3.ub);
        tc.a->apply_ubatch(s, H3.ub);
        life(4, gkv, gs, (int64_t) (intptr_t) &H3.ub);

        tc.a->seq_add(0, -1, -1, 3);
        fg_goose_kv_seq_entry(6, gkv, 0, -1, -1, 3);
        check(cells_equal(tc.a, gkv), "seq_add");

        tc.a->seq_div(0, -1, -1, 2);
        fg_goose_kv_seq_entry(7, gkv, 0, -1, -1, 2);
        check(cells_equal(tc.a, gkv), "seq_div");

        check((tc.a->seq_rm(0, 0, 2) ? 1 : 0) == fg_goose_kv_seq_entry(3, gkv, 0, 0, 2, 0), "seq_rm_ret");
        check(cells_equal(tc.a, gkv), "seq_rm");

        tc.a->state_clear(0);
        fg_goose_kv_seq_entry(10, gkv, 0, 0, 0, 0);
        check(cells_equal(tc.a, gkv), "state_clear");
    }

    // J9: update context (shift reset path, rope NONE -> no tensor callbacks).
    std::printf("stage J9\n");
    {
        auto s = tc.a->find_slot(H3.ub, false);
        const int64_t gs = life(2, gkv, (int64_t) (intptr_t) &H3.ub);
        tc.a->apply_ubatch(s, H3.ub);
        life(4, gkv, gs, (int64_t) (intptr_t) &H3.ub);
        tc.a->seq_add(0, -1, -1, 3);
        fg_goose_kv_seq_entry(6, gkv, 0, -1, -1, 3);
        { const int64_t ch = fg_kv_get(gkv, 10, 0); check((tc.a->get_has_shift() ? 1 : 0) == cell(ch, 9), "shift_flag_seen"); }

        const int64_t gctx = life(7, gkv, 0);
        auto g = fg_goose_context_update(tc.b, gkv, gctx, nullptr, true, 0, nullptr, nullptr);
        auto * gk = static_cast<llama_kv_cache_context *>(g.get());
        check(gk->apply(), "update_apply");
        // Native update calls lctx->get_sched() before checking sc/shift, so a
        // real llama_context is required; this corpus does not fabricate one.
        const int64_t ch9 = fg_kv_get(gkv, 10, 0);
        check(cell(ch9, 9) == 0, "goose_shift_reset");
        check(cells_equal(tc.a, gkv), "update_cells");
    }

    fg_goose_kv_seq_entry(1, gkv, 0, 0, 0, 0); // destroy attached handle
    std::printf(failures == 0 ? "CORPUS PASS\n" : "CORPUS FAIL\n");
    return failures == 0 ? 0 : 1;
}
