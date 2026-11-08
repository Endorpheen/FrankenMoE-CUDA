// fixture.cpp - REAL native-vs-Goose differential trace.
// Two concrete llama_memory_recurrent instances are built via the additive
// tagged constructor (metadata only, no weights):
//   * `nat` is never registered, so the injected early-return lines fall through
//     to the original, unchanged native bodies (captured native reference);
//   * `gs` is registered, so the same public methods route into the emitted Goose
//     entries through fg153-hooks.
// The original constructor and both original bodies remain in the translation
// unit, so rollback is `FG153_GOOSE_HOOKS=0` (or simply not registering).
// Proposal for Codex; not run here.
#include "llama-memory-recurrent.h"
#include "llama-model.h"
#include "llama-io.h"
#include "ggml.h"
#include "fg153-hooks.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <new>
#include <set>
#include <vector>

// Minimal in-memory IO for the state control test (n_layer==0 here, so no tensor
// row transfer happens; tensor IO remains covered by the parent corpus).
struct fg153_vec_writer : llama_io_write_i {
    std::vector<uint8_t> buf;
    void write(const void * src, size_t n) override {
        auto * p = (const uint8_t *) src;
        buf.insert(buf.end(), p, p + n);
    }
    void write_tensor(ggml_tensor *, size_t, size_t) override { std::abort(); }
    size_t n_bytes() override { return buf.size(); }
};
struct fg153_vec_reader : llama_io_read_i {
    const std::vector<uint8_t> & buf;
    size_t off = 0;
    explicit fg153_vec_reader(const std::vector<uint8_t> & b) : buf(b) {}
    void read(void * dst, size_t n) override {
        if (off + n > buf.size()) std::abort();
        std::memcpy(dst, buf.data() + off, n); off += n;
    }
    void read_tensor(ggml_tensor *, size_t, size_t) override { std::abort(); }
    size_t n_bytes() override { return off; }
};

static int g_fail = 0;
static void stage(const char * s) { std::printf("FG153_STAGE %s\n", s); std::fflush(stdout); }
static void num(const char * tc, const char * field, int64_t goose, int64_t native) {
    int ok = goose == native;
    if (!ok) g_fail++;
    std::printf("FG153_TRACE case=%s field=%s goose=%lld native=%lld %s\n",
                tc, field, (long long) goose, (long long) native, ok ? "PASS" : "FAIL");
}
static void setcmp(const char * tc, const char * field,
                   const std::set<llama_seq_id> & goose, const std::set<llama_seq_id> & native) {
    int ok = goose == native;
    if (!ok) g_fail++;
    std::printf("FG153_TRACE case=%s field=%s goose_set=%zu native_set=%zu %s\n",
                tc, field, goose.size(), native.size(), ok ? "PASS" : "FAIL");
}

static void reset_state(llama_memory_recurrent & m) {
    for (auto & c : m.cells) {
        c.pos = -1; c.src = -1; c.src0 = -1; c.tail = -1; c.seq_id.clear();
    }
    m.head = 0; m.used = 0; m.n = 0; m.rs_z = -1;
    for (auto & x : m.rs_idx) x = 0;
}

static void compare_all(const char * tc, const llama_memory_recurrent & nat,
                        const llama_memory_recurrent & gs) {
    num(tc, "head", gs.head, nat.head);
    num(tc, "used", gs.used, nat.used);
    num(tc, "n", gs.n, nat.n);
    num(tc, "rs_z", gs.rs_z, nat.rs_z);
    num(tc, "size", gs.size, nat.size);
    for (size_t i = 0; i < nat.cells.size(); ++i) {
        num(tc, "pos", gs.cells[i].pos, nat.cells[i].pos);
        num(tc, "src", gs.cells[i].src, nat.cells[i].src);
        num(tc, "src0", gs.cells[i].src0, nat.cells[i].src0);
        num(tc, "tail", gs.cells[i].tail, nat.cells[i].tail);
        setcmp(tc, "seq", gs.cells[i].seq_id, nat.cells[i].seq_id);
    }
    for (size_t s = 0; s < nat.rs_idx.size() && s < gs.rs_idx.size(); ++s)
        num(tc, "rs_idx", gs.rs_idx[s], nat.rs_idx[s]);
}

static void cell(llama_memory_recurrent & m, int i, llama_pos pos, llama_seq_id seq) {
    m.cells[i].pos = pos;
    m.cells[i].seq_id.insert(seq);
    m.used++;
}

int main() {
    llama_hparams hp{}; // synthetic: seq/clear/pos paths do not read it
    llama_memory_recurrent nat(fg153_native_init_tag{}, hp, 8, 4, 4);
    llama_memory_recurrent gs(fg153_native_init_tag{}, hp, 8, 4, 4);
    fg153_fixture_seed(&nat);
    fg153_fixture_seed(&gs);
    fg153_hooks_register(&gs);
    stage("seeded+registered");

    // --- seq_rm partial rollback (module non-empty via seeded buffer)
    reset_state(nat); reset_state(gs);
    for (auto * m : {&nat, &gs}) {
        cell(*m, 5, 10, 0);
        m->cells[0].tail = 5;
        m->rs_idx[0] = 0;
    }
    {
        bool rn = nat.seq_rm(0, 8, 11);
        bool rg = gs.seq_rm(0, 8, 11);
        num("rm-rollback", "rc", rg ? 1 : 0, rn ? 1 : 0);
        compare_all("rm-rollback", nat, gs);
    }
    stage("rm-rollback");

    // --- shared-cell reject, unshare, retry
    reset_state(nat); reset_state(gs);
    for (auto * m : {&nat, &gs}) {
        cell(*m, 6, 10, 0);
        m->cells[6].seq_id.insert(1);
        m->cells[0].tail = 6;
        m->rs_idx[0] = 0;
    }
    {
        bool rn0 = nat.seq_rm(0, 8, 11);
        bool rg0 = gs.seq_rm(0, 8, 11);
        num("rm-retry", "rc_reject", rg0 ? 1 : 0, rn0 ? 1 : 0);
        compare_all("rm-retry", nat, gs);
        nat.cells[6].seq_id.erase(1);
        gs.cells[6].seq_id.erase(1);
        bool rn1 = nat.seq_rm(0, 8, 11);
        bool rg1 = gs.seq_rm(0, 8, 11);
        num("rm-retry", "rc_retry", rg1 ? 1 : 0, rn1 ? 1 : 0);
        compare_all("rm-retry", nat, gs);
    }
    stage("rm-retry");

    // --- seq_cp
    reset_state(nat); reset_state(gs);
    for (auto * m : {&nat, &gs}) {
        cell(*m, 2, 4, 1); cell(*m, 4, 9, 0);
        m->cells[1].tail = 2; m->cells[0].tail = 4;
    }
    {
        nat.seq_cp(0, 1, -1, -1);
        gs.seq_cp(0, 1, -1, -1);
        compare_all("cp", nat, gs);
    }
    stage("cp");

    // --- seq_keep
    reset_state(nat); reset_state(gs);
    for (auto * m : {&nat, &gs}) {
        cell(*m, 0, 1, 0); m->cells[0].seq_id.insert(1);
        cell(*m, 1, 2, 0); cell(*m, 3, 7, 1);
        m->cells[0].tail = 1; m->cells[1].tail = 3;
    }
    {
        nat.seq_keep(1);
        gs.seq_keep(1);
        compare_all("keep", nat, gs);
    }
    stage("keep");

    // --- clear(false)
    {
        nat.clear(false);
        gs.clear(false);
        compare_all("clear", nat, gs);
    }
    stage("clear");

    // --- position queries + can_shift
    reset_state(nat); reset_state(gs);
    for (auto * m : {&nat, &gs}) { cell(*m, 2, 4, 0); cell(*m, 5, 9, 0); }
    num("pos", "min", gs.seq_pos_min(0), nat.seq_pos_min(0));
    num("pos", "max", gs.seq_pos_max(0), nat.seq_pos_max(0));
    num("shift", "can_shift", gs.get_can_shift() ? 1 : 0, nat.get_can_shift() ? 1 : 0);
    stage("pos+shift");

    // --- prepare / find_slot (hybrid::init_batch bypass) with a synthetic ubatch
    reset_state(nat); reset_state(gs);
    {
        llama_seq_id s0 = 0;
        llama_seq_id * seqids[1] = { &s0 };
        int32_t nseq[1] = { 1 };
        llama_pos pos[1] = { 2 };
        llama_ubatch ub{};
        ub.b_equal_seqs = 1;
        ub.n_tokens = 1; ub.n_seq_tokens = 1; ub.n_seqs = 1; ub.n_seqs_unq = 1; ub.n_pos = 1;
        ub.pos = pos; ub.n_seq_id = nseq; ub.seq_id = seqids;

        std::vector<llama_ubatch> ubs{ub};
        bool pn = nat.prepare(ubs);
        bool pg = gs.prepare(ubs);
        num("prepare", "rc", pg ? 1 : 0, pn ? 1 : 0);
        compare_all("prepare", nat, gs);

        reset_state(nat); reset_state(gs);
        bool fn = nat.find_slot(ub);
        bool fg = gs.find_slot(ub);
        num("find-slot", "rc", fg ? 1 : 0, fn ? 1 : 0);
        compare_all("find-slot", nat, gs);
    }
    stage("prepare+find_slot");

    // --- state_write/state_read control (no tensor rows: hparams.n_layer()==0)
    reset_state(nat); reset_state(gs);
    for (auto * m : {&nat, &gs}) {
        cell(*m, 0, 1, 0); cell(*m, 3, 5, 0);
        m->cells[0].tail = 3;
    }
    {
        fg153_vec_writer wn, wg;
        nat.state_write(wn);
        gs.state_write(wg);
        num("state", "bytes", (int64_t) wg.buf.size(), (int64_t) wn.buf.size());
        num("state", "equal", wg.buf == wn.buf ? 1 : 0, 1);
        reset_state(nat); reset_state(gs);
        fg153_vec_reader rn(wn.buf), rg(wg.buf);
        int okn = 1, okg = 1;
        try { nat.state_read(rn); } catch (...) { okn = 0; }
        try { gs.state_read(rg); } catch (...) { okg = 0; }
        num("state", "read_rc", okg, okn);
        compare_all("state", nat, gs);
    }
    stage("state-io");

    // --- lifecycle contexts (update/full) via real concrete context hooks
    {
        auto ncu = nat.init_update(nullptr, false);
        auto gcu = gs.init_update(nullptr, false);
        num("init-update", "status", (int64_t) gcu->get_status(), (int64_t) ncu->get_status());
        num("init-update", "apply", gcu->apply() ? 1 : 0, ncu->apply() ? 1 : 0);
        // parent fix: NO_UPDATE does not permit next(); do not call it here.
        auto ncf = nat.init_full();
        auto gcf = gs.init_full();
        num("init-full", "status", (int64_t) gcf->get_status(), (int64_t) ncf->get_status());
        num("init-full", "next", gcf->next() ? 1 : 0, ncf->next() ? 1 : 0);
        // Concrete-ABI check: exactly what hybrid::get_recr + non-virtual getters do.
        auto * nfc = static_cast<llama_memory_recurrent_context *>(ncf.get());
        auto * gfc = static_cast<llama_memory_recurrent_context *>(gcf.get());
        num("typed", "n_rs", (int64_t) gfc->get_n_rs(), (int64_t) nfc->get_n_rs());
        num("typed", "head", (int64_t) gfc->get_head(), (int64_t) nfc->get_head());
        num("typed", "rs_z", (int64_t) gfc->get_rs_z(), (int64_t) nfc->get_rs_z());
        num("typed", "size", (int64_t) gfc->get_size(), (int64_t) nfc->get_size());
        stage("lifecycle");
        if (!nat.r_l.empty()) {
            num("typed", "r_l0", (int64_t)(intptr_t) gfc->get_r_l(0),
                (int64_t)(intptr_t) nfc->get_r_l(0));
        }
    }

    // --- memory_breakdown (both read the seeded ctxs_bufs)
    {
        auto nb = nat.memory_breakdown();
        auto gb = gs.memory_breakdown();
        size_t nsum = 0, gsum = 0;
        for (auto & kv : nb) nsum += kv.second;
        for (auto & kv : gb) gsum += kv.second;
        num("breakdown", "entries", (int64_t) gb.size(), (int64_t) nb.size());
        num("breakdown", "bytes", (int64_t) gsum, (int64_t) nsum);
    }
    stage("breakdown");

    fg153_hooks_forget(&gs);
    stage("forget");

    // --- production ctor allocation: 4 real layers, recurrent filter, PLE==0.
    // Layer 3 is an attention layer (filtered out) so r_l/s_l[3] stay NULL, and
    // PLE==0 keeps p_l NULL; native skips those in size accounting.
    llama_hparams h4{};
    h4.n_layer_all = 4;
    h4.n_embd = 128;
    h4.ssm_d_conv = 4;
    h4.ssm_d_inner = 64;
    h4.ssm_d_state = 16;
    h4.ssm_n_group = 2;
    h4.ssm_dt_rank = 4;
    for (int i = 0; i < 4; ++i) h4.is_recr_impl[i] = ((i + 1) % 4 != 0);
    std::function<bool(int32_t)> recr_filter = [](int32_t il) { return (il + 1) % 4 != 0; };
    stage("ctor-start");
    std::printf("FG153_HP n_layer=%u r=%u s=%u ple=%u\n",
                (unsigned) h4.n_layer(), (unsigned) h4.n_embd_r(),
                (unsigned) h4.n_embd_s(), (unsigned) h4.ple_conv_state());
    std::fflush(stdout);
    {
        llama_memory_recurrent t(fg153_native_init_tag{}, h4, 8, 4, 2);
        int64_t rc = fg153_hooks_ctor_alloc(&t, &recr_filter, nullptr,
                                            (int) GGML_TYPE_F32, (int) GGML_TYPE_F32, 0, 8, 4, 2);
        num("ctor", "rc", rc, 0);
        num("ctor", "r_l0_present", (t.r_l.size() > 0 && t.r_l[0] != nullptr) ? 1 : 0, 1);
        num("ctor", "r_l3_null", (t.r_l.size() > 3 && t.r_l[3] == nullptr) ? 1 : 0, 1);
        num("ctor", "p_l0_null", (t.p_l.size() > 0 && t.p_l[0] == nullptr) ? 1 : 0, 1);
        num("ctor", "buffers", t.memory_breakdown().empty() ? 0 : 1, 1);
        auto c = t.init_full();
        num("ctor", "size", (int64_t) static_cast<llama_memory_recurrent_context *>(c.get())->get_size(), 8);
        c.reset();
    }
    num("ctor", "live_after_dtor", fg153_hooks_live_count(), 0);
    stage("ctor-alloc");

    // --- forced allocation/adopt failures: cleanup must free only owned data
    {
        auto run_fail = [&](const char * name, const char * env, int64_t want) {
            setenv(env, "1", 1);
            int64_t rc = 0;
            {
                llama_memory_recurrent t(fg153_native_init_tag{}, h4, 8, 4, 2);
                rc = fg153_hooks_ctor_alloc(&t, &recr_filter, nullptr,
                                            (int) GGML_TYPE_F32, (int) GGML_TYPE_F32, 0, 8, 4, 2);
            }
            unsetenv(env);
            num(name, "rc", rc, want);
            num(name, "live_after_dtor", fg153_hooks_live_count(), 0);
        };
        run_fail("ctor-ctx-fail", "FG153_FORCE_CTX_FAIL", -3);
        run_fail("ctor-buf-fail", "FG153_FORCE_BUFFER_FAIL", -4);
        run_fail("ctor-adopt-fail", "FG153_FORCE_ADOPT_FAIL", -5);
    }
    stage("ctor-failures");

    // --- ownership: ctor -> context -> typed getters -> reset/free -> address reuse
    {
        using T = llama_memory_recurrent;
        num("ownership", "live_isolated", fg153_hooks_live_count(), 0);
        alignas(T) unsigned char storage[sizeof(T)];

        T * a = new (storage) T(fg153_native_init_tag{}, hp, 8, 4, 4);
        fg153_hooks_register(a);
        num("ownership", "live_after_register", fg153_hooks_live_count(), 1);
        {
            auto ca = a->init_full();
            num("ownership", "live_with_ctx", fg153_hooks_live_count(), 2);
            auto * tca = static_cast<llama_memory_recurrent_context *>(ca.get());
            num("ownership", "typed_size", (int64_t) tca->get_size(), 8);
            num("ownership", "typed_n_rs", (int64_t) tca->get_n_rs(), 8);
            ca.reset(); // guaranteed context unregister + Goose ctx free
            num("ownership", "live_after_ctx_reset", fg153_hooks_live_count(), 1);
        }
        a->~T(); // out-of-line dtor unregisters unconditionally
        num("ownership", "live_after_dtor", fg153_hooks_live_count(), 0);

        // address reuse: a stale registry entry here would be UAF
        T * b = new (storage) T(fg153_native_init_tag{}, hp, 8, 4, 4);
        num("reuse", "active_before_register", fg153_hooks_active(b) ? 1 : 0, 0);
        fg153_hooks_register(b);
        {
            auto cb = b->init_full();
            num("reuse", "status", (int64_t) cb->get_status(), 0);
            num("reuse", "typed_size", (int64_t) static_cast<llama_memory_recurrent_context *>(cb.get())->get_size(), 8);
            cb.reset();
        }
        b->~T();
        num("reuse", "live_after_dtor", fg153_hooks_live_count(), 0);
    }
    stage("ownership");

    std::printf("FG153_RESULT failures=%d\n", g_fail);
    return g_fail ? 1 : 0;
}
