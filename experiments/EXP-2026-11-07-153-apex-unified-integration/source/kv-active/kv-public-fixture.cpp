// worker-2/kv-public-fixture.cpp
//
// Normal public-API liveness fixture for the patched real llama-kv-cache TU.
// cache A is unattached -> native bodies. cache B is Goose-attached -> the
// patched public methods route into the Goose implementation. Synthetic
// 1-layer/1-head hparams and real CPU tensors/buffers; no weights.

#include "kv-native-adapter.h"
#include "llama-batch.h"
#include "llama-io.h"
#include "llama-kv-cache.h"
#include "llama-model.h"
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

namespace {

int failures = 0;
void check(bool ok, const char * tag) {
    std::printf("%s %s\n", tag, ok ? "OK" : "FAIL");
    if (!ok) { ++failures; }
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
    std::shared_ptr<llama_ubatch::data_t> d;
    ub_holder(uint32_t n, int64_t base, llama_token tok = 0, bool with_tok = false) {
        d = std::make_shared<llama_ubatch::data_t>();
        d->token.assign(n, tok);
        d->pos.resize(n);
        for (uint32_t i = 0; i < n; ++i) { d->pos[i] = (llama_pos) (base + i); }
        d->n_seq_id.assign(n, 1);
        d->seq_id_data.assign(n, 0);
        d->seq_id.resize(n);
        for (uint32_t i = 0; i < n; ++i) { d->seq_id[i] = &d->seq_id_data[i]; }
        d->seq_id_unq.assign(1, 0);
        d->seq_idx.assign(LLAMA_MAX_SEQ, 0);
        d->output.assign(n, 0);
        ub.b_equal_seqs = 1;
        ub.n_tokens = n;
        ub.n_seq_tokens = n;
        ub.n_seqs = 1;
        ub.n_seqs_unq = 1;
        ub.n_pos = 1;
        ub.token = with_tok ? d->token.data() : nullptr;
        ub.pos = d->pos.data();
        ub.n_seq_id = d->n_seq_id.data();
        ub.seq_id = d->seq_id.data();
        ub.seq_id_unq = d->seq_id_unq.data();
        ub.seq_idx = d->seq_idx.data();
        ub.output = d->output.data();
        ub.type = nullptr;
        ub.decision_order = nullptr;
        ub.data = d;
    }
};

struct mem_write : llama_io_write_i {
    std::vector<uint8_t> buf;
    void write(const void * src, size_t size) override {
        const uint8_t * p = static_cast<const uint8_t *>(src);
        buf.insert(buf.end(), p, p + size);
    }
    void write_tensor(ggml_tensor * t, size_t off, size_t size) override {
        std::vector<uint8_t> tmp(size);
        ggml_backend_tensor_get(t, tmp.data(), off, size);
        write(tmp.data(), size);
    }
    size_t n_bytes() override { return buf.size(); }
};

struct mem_read : llama_io_read_i {
    const std::vector<uint8_t> & buf;
    size_t pos = 0;
    explicit mem_read(const std::vector<uint8_t> & b) : buf(b) {}
    void read(void * dst, size_t size) override {
        if (pos + size > buf.size()) { throw std::runtime_error("truncated state"); }
        std::memcpy(dst, buf.data() + pos, size);
        pos += size;
    }
    void read_tensor(ggml_tensor * t, size_t off, size_t size) override {
        if (pos + size > buf.size()) { throw std::runtime_error("truncated state"); }
        ggml_backend_tensor_set(t, buf.data() + pos, off, size);
        pos += size;
    }
    size_t n_bytes() override { return pos; }
};

bool seq_equiv(llama_kv_cache * a, llama_kv_cache * b) {
    const llama_kv_cells & ca = a->get_cells(0);
    const llama_kv_cells & cb = b->get_cells(0);
    if (ca.get_used() != cb.get_used()) { return false; }
    if (ca.seq_pos_min(0) != cb.seq_pos_min(0)) { return false; }
    if (ca.seq_pos_max(0) != cb.seq_pos_max(0)) { return false; }
    std::vector<int> va, vb;
    for (uint32_t i = 0; i < ca.size(); ++i) {
        if (!ca.is_empty(i) && ca.seq_has(i, 0)) { va.push_back((int) ca.pos_get(i)); }
        if (!cb.is_empty(i) && cb.seq_has(i, 0)) { vb.push_back((int) cb.pos_get(i)); }
    }
    std::sort(va.begin(), va.end());
    std::sort(vb.begin(), vb.end());
    if (va != vb || ca.get_used() != cb.get_used()) {
        std::fprintf(stderr, "[seq] used A=%d B=%d min A=%d B=%d max A=%d B=%d nA=%zu nB=%zu\n",
                ca.get_used(), cb.get_used(), ca.seq_pos_min(0), cb.seq_pos_min(0),
                ca.seq_pos_max(0), cb.seq_pos_max(0), va.size(), vb.size());
        for (auto & p : va) std::fprintf(stderr, " A(%d)", p);
        for (auto & p : vb) std::fprintf(stderr, " B(%d)", p);
        std::fprintf(stderr, "\n");
    }
    return va == vb;
}

bool cells_equal(llama_kv_cache * a, llama_kv_cache * b) {
    const llama_kv_cells & ca = a->get_cells(0);
    const llama_kv_cells & cb = b->get_cells(0);
    bool ok = ca.size() == cb.size() && ca.get_used() == cb.get_used();
    for (uint32_t i = 0; i < ca.size(); ++i) {
        const bool ea_empty = ca.is_empty(i);
        const bool eb_empty = cb.is_empty(i);
        if (ea_empty != eb_empty) { std::fprintf(stderr, "[mismatch] cell %u empty A=%d B=%d\n", i, (int) ea_empty, (int) eb_empty); }
        ok = ok && ea_empty == eb_empty;
        if (!ea_empty && !eb_empty) {
            if (ca.pos_get(i) != cb.pos_get(i)) { std::fprintf(stderr, "[mismatch] pos cell %u A=%d B=%d\n", i, ca.pos_get(i), cb.pos_get(i)); }
            if (ca.seq_count(i) != cb.seq_count(i)) { std::fprintf(stderr, "[mismatch] seqcount cell %u A=%d B=%d\n", i, ca.seq_count(i), cb.seq_count(i)); }
            ok = ok && ca.pos_get(i) == cb.pos_get(i);
            ok = ok && ca.seq_count(i) == cb.seq_count(i);
            const auto & xa = ca.ext_get(i);
            const auto & xb = cb.ext_get(i);
            ok = ok && xa.x == xb.x && xa.y == xb.y && xa.tok == xb.tok;
        }
    }
    return ok;
}

struct two {
    probe_model model;
    llama_hparams h;
    llama_kv_cache * a;
    llama_kv_cache * b;
    two() : model(llama_model_default_params()), h(make_hparams()) {
        model.arch = LLM_ARCH_UNKNOWN;
        a = mk();
        b = mk();
    }
    ~two() { delete a; delete b; }
    llama_kv_cache * mk() {
        return new llama_kv_cache(model, h, GGML_TYPE_F32, GGML_TYPE_F32, false, false, true,
                16, 1, 16, 0, LLAMA_SWA_TYPE_NONE, nullptr,
                llama_kv_cache::layer_filter_cb{}, llama_kv_cache::layer_reuse_cb{},
                llama_kv_cache::layer_share_cb{}, "public");
    }
};

void apply_both(two & t, llama_ubatch & u) {
    auto sa = t.a->find_slot(u, false);
    auto sb = t.b->find_slot(u, false);
    check(!sa.empty() && !sb.empty(), "find_slot");
    t.a->apply_ubatch(sa, u);
    t.b->apply_ubatch(sb, u);
}

} // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    two t;
    const int64_t gkv = fg_kv_attach_real(t.b);
    fg_kv_configure_real(gkv, t.b);
    check(gkv != 0, "attach_real");

    ub_holder u0(2, 0, 5, true), u1(2, 2), u2(12, 0);
    apply_both(t, u0.ub);
    apply_both(t, u1.ub);
    check(cells_equal(t.a, t.b), "public_apply");

    // prepare success and failure rollback through the patched public path.
    std::vector<llama_ubatch> ok_ubs = { u0.ub, u1.ub };
    t.a->clear(false);
    t.b->clear(false);
    auto pa = t.a->prepare(ok_ubs);
    auto pb = t.b->prepare(ok_ubs);
    check(pa.size() == pb.size() && pa.size() == 2, "public_prepare_success");
    check(cells_equal(t.a, t.b), "prepare_restore");

    std::vector<llama_ubatch> fail_ubs = { u2.ub, u0.ub, u1.ub, u1.ub, u1.ub };
    t.a->clear(false);
    t.b->clear(false);
    auto fa = t.a->prepare(fail_ubs);
    auto fb = t.b->prepare(fail_ubs);
    check(fa.empty() && fb.empty(), "public_prepare_failure");
    check(cells_equal(t.a, t.b), "prepare_failure_rollback");

    // seq operations.
    t.a->clear(false);
    t.b->clear(false);
    apply_both(t, u0.ub);
    t.a->seq_add(0, -1, -1, 3);
    t.b->seq_add(0, -1, -1, 3);
    check(cells_equal(t.a, t.b) && t.a->get_has_shift() == t.b->get_has_shift(), "public_seq_add");
    t.a->seq_div(0, -1, -1, 2);
    t.b->seq_div(0, -1, -1, 2);
    check(cells_equal(t.a, t.b), "public_seq_div");
    check(t.a->seq_rm(0, 0, 2) == t.b->seq_rm(0, 0, 2), "public_seq_rm");
    check(cells_equal(t.a, t.b), "public_seq_rm_state");

    // state roundtrip through the patched public state_read/state_write.
    mem_write wa, wb;
    t.a->state_write(wa);
    t.b->state_write(wb);
    check(wa.buf.size() > 0 && wb.buf.size() > 0, "state_write_nonempty");
    check(wa.buf == wb.buf, "state_bytes_equal");
    const std::vector<uint8_t> save_a = wa.buf, save_b = wb.buf;

    // cross-native: native blob -> Goose read; Goose blob -> native read.
    t.a->clear(false);
    t.b->clear(false);
    mem_read ra(save_a), rb(save_b);
    t.a->state_read(rb);   // native reads the Goose blob
    t.b->state_read(ra);   // Goose reads the native blob
    check(cells_equal(t.a, t.b), "state_cross_roundtrip");

    // seq_id >= 0 wire path. Native seq *read* in this runtime calls
    // llama_batch_allocr::ubatch_reserve -> fg153_import_state and aborts with
    // std::length_error before touching the cache, so only native-write ->
    // Goose-read is exercised here; the native-read side is a runtime boundary.
    {
        mem_write wa2, wb2;
        t.a->state_write(wa2, 0);
        t.b->state_write(wb2, 0);
        check(wa2.buf == wb2.buf, "state_seq_bytes_equal");
        t.b->clear(false);
        mem_read r1(wa2.buf);
        t.b->state_read(r1, 0); // Goose reads native seq blob
        check(cells_equal(t.a, t.b), "state_seq_cross_roundtrip");
    }

    // state_read_sinfo out/in.
    {
        t.a->clear(false);
        t.b->clear(false);
        llama_kv_cache::slot_info_vec_t sa, sb;
        mem_read ra1(save_a), rb1(save_a);
        t.a->state_read_sinfo(ra1, -1, 0, &sa, nullptr);
        t.b->state_read_sinfo(rb1, -1, 0, &sb, nullptr);
        bool same = sa.size() == sb.size();
        for (size_t i = 0; same && i < sa.size(); ++i) {
            same = sa[i].s0 == sb[i].s0 && sa[i].s1 == sb[i].s1;
            same = same && sa[i].n_stream() == sb[i].n_stream();
            same = same && sa[i].size() == sb[i].size();
        }
        check(same, "state_sinfo_out");
        t.b->clear(false);
        mem_read rb2(save_a);
        t.b->state_read_sinfo(rb2, -1, 0, nullptr, &sa);
        check(cells_equal(t.a, t.b), "state_sinfo_in_mirror");
    }

    // corrupted key type: same failure effect on both (clear + throw).
    {
        std::vector<uint8_t> bad = save_a;
        const size_t off = 4 + 4 + 12 + 4 + 4 + 4 + 4; // n_stream,count,meta,v_trans,n_layer,n_rot_k,n_rot_v
        if (off + 4 <= bad.size()) { bad[off] = 0x7f; bad[off+1] = 0x7f; bad[off+2] = 0; bad[off+3] = 0; }
        t.a->clear(false);
        t.b->clear(false);
        bool ta = false, tb = false;
        try { mem_read r(bad); t.a->state_read(r); } catch (const std::exception &) { ta = true; }
        try { mem_read r(bad); t.b->state_read(r); } catch (const std::exception &) { tb = true; }
        check(ta && tb, "state_bad_type_throws");
        check(t.a->get_cells(0).get_used() == 0 && t.b->get_cells(0).get_used() == 0, "state_bad_type_clears");

        std::vector<uint8_t> trunc(save_a.begin(), save_a.begin() + save_a.size() / 2);
        ta = tb = false;
        try { mem_read r(trunc); t.a->state_read(r); } catch (const std::exception &) { ta = true; }
        try { mem_read r(trunc); t.b->state_read(r); } catch (const std::exception &) { tb = true; }
        check(ta && tb, "state_truncated_throws");
    }

    // full context through the public patched init_full.
    auto cfa = t.a->init_full();
    auto cfb = t.b->init_full();
    auto * ka = static_cast<llama_kv_cache_context *>(cfa.get());
    auto * kb = static_cast<llama_kv_cache_context *>(cfb.get());
    check(ka->get_status() == kb->get_status(), "full_status");
    check(ka->get_n_kv() == kb->get_n_kv(), "full_n_kv");
    // Native full-context apply dereferences an uninitialized lctx in
    // llama_kv_cache_context(kv); only the Goose side is applied here.
    check(kb->apply(), "full_apply_goose");

    // update context: native update needs a real llama_context; only the Goose
    // side is exercised here (honest boundary, not a success stub).
    t.a->clear(false);
    t.b->clear(false);
    apply_both(t, u0.ub);
    t.a->seq_add(0, -1, -1, 3);
    t.b->seq_add(0, -1, -1, 3);
    auto cua = t.a->init_update(nullptr, false);
    auto cub = t.b->init_update(nullptr, false);
    check(cub->apply(), "update_goose_apply");
    check(t.b->get_has_shift() == false, "update_goose_reset");
    (void) cua;

    check(cells_equal(t.a, t.b), "final_cells");
    std::printf(failures == 0 ? "PUBLIC PASS\n" : "PUBLIC FAIL\n");
    return failures == 0 ? 0 : 1;
}
