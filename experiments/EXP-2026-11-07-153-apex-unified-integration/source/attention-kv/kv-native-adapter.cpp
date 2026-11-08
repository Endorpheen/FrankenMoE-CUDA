// worker-2/kv-native-adapter.cpp
//
// REAL-cache adapter. Implements:
//   * typed access to private llama_kv_cache / llama_kv_cells members declared
//     in kv-native-hooks.h (requires the documented friend patch),
//   * attached cells get/put backed by the real std::set indexes,
//   * metadata get/put backed by the real cache,
//   * a subclass of llama_kv_cache_context that calls the Goose entries and
//     keeps the base object valid for static_cast in hybrid/model graphs.
//
// No private offset arithmetic: only named members of the real classes.

#include "kv-native-adapter.h"
#include "kv-bridge.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "llama.h"
#include "llama-arch.h"
#include "ggml.h"
#include "ggml-backend.h"
#include "llama-batch.h"
#include "llama-graph.h"

#define private public
#include "llama-model.h"
#include "llama-kv-cache.h"
#undef private

#include "kv-native-hooks.h"

extern "C" {
int64_t fg_kv_attach(void * native, int64_t n_stream);
int64_t fg_kv_get(int64_t h, int64_t k, int64_t i);
void    fg_kv_put(int64_t h, int64_t k, int64_t i, int64_t v);
int64_t fg_kv_obj(int64_t kind);
void    fg_kv_vec_push(int64_t v, int64_t item);
void    fg_kv_slot_push(int64_t s, int64_t strm_idx, int64_t cell);
void    fg_kv_destroy(int64_t h);
int64_t fg_w2_attach_cells(void * opaque);
void    fg_w2_configure(int64_t max_seq, int64_t token_null);
void *  fg_w2_native_ptr(int64_t id);
int64_t fg_w2_set_wrap(int64_t kind, void * a);
void    fg_w2_set_destroy(int64_t h);
int64_t fg_goose_kv_lifecycle_entry(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
}

using llama_kv_goose::access;
namespace {

struct cell_wrap {
    llama_kv_cells * cells = nullptr;
    int64_t used = 0;
    std::vector<int64_t> seqpos;
};

std::map<int64_t, cell_wrap>            g_cell_wraps;     // cells handle -> wraps
std::map<int64_t, llama_kv_cache *>     g_kv_native;      // kv handle -> real cache
std::map<llama_kv_cache *, int64_t>     g_handle_of;      // real cache -> kv handle

int64_t used_wrap(cell_wrap & w) {
    if (w.used == 0) { w.used = fg_w2_set_wrap(1, &access::used(*w.cells)); }
    return w.used;
}
int64_t seq_wrap(cell_wrap & w, int64_t s) {
    if ((size_t) s >= w.seqpos.size()) { w.seqpos.resize((size_t) s + 1, 0); }
    if (w.seqpos[(size_t) s] == 0) {
        w.seqpos[(size_t) s] = fg_w2_set_wrap(2, &access::seq_pos(*w.cells)[s]);
    }
    return w.seqpos[(size_t) s];
}
cell_wrap & wrap_of(int64_t handle) {
    auto it = g_cell_wraps.find(handle);
    if (it == g_cell_wraps.end()) { std::abort(); }
    return it->second;
}
llama_kv_cache * kv_of(void * native) { return static_cast<llama_kv_cache *>(native); }

} // namespace

namespace llama_kv_goose {

llama_kv_cells & access::cells(llama_kv_cache & kv, uint32_t strm) { return kv.v_cells[strm]; }
uint32_t & access::head(llama_kv_cache & kv, uint32_t strm) { return kv.v_heads[strm]; }
bool access::has_other(llama_kv_cache & kv) { return kv.other != nullptr; }
uint32_t access::n_stream(const llama_kv_cache & kv) { return kv.n_stream; }
uint32_t access::n_seq_max(const llama_kv_cache & kv) { return kv.n_seq_max; }
uint32_t access::n_pad(const llama_kv_cache & kv) { return kv.n_pad; }
uint32_t access::n_swa(const llama_kv_cache & kv) { return kv.n_swa; }
llama_swa_type access::swa_type(const llama_kv_cache & kv) { return kv.swa_type; }
bool access::arch_step35(const llama_kv_cache & kv) { return kv.model.arch == LLM_ARCH_STEP35; }
int64_t access::n_pos_per_embd(const llama_kv_cache & kv) { return (int64_t) kv.hparams.n_pos_per_embd(); }
uint32_t access::n_layers(const llama_kv_cache & kv) { return (uint32_t) kv.layers.size(); }
uint32_t access::layer_il(const llama_kv_cache & kv, uint32_t il) { return kv.layers[il].il; }
bool access::layer_has_v(const llama_kv_cache & kv, uint32_t il) { return kv.layers[il].v != nullptr; }
bool access::layer_has_rope(const llama_kv_cache & kv, uint32_t il) { return kv.hparams.has_rope(kv.layers[il].il); }
int64_t access::ple_n_heads(const llama_kv_cache & kv) { return (int64_t) kv.hparams.ple_n_heads; }
int64_t access::ple_image_token(const llama_kv_cache & kv) { return (int64_t) kv.hparams.ple_image_token_id; }
int64_t access::ple_eos_token(const llama_kv_cache & kv) { return (int64_t) kv.hparams.ple_eos_token_id; }
bool access::rope_none(const llama_kv_cache & kv) { return kv.hparams.rope_type == LLAMA_ROPE_TYPE_NONE; }
bool access::v_trans(const llama_kv_cache & kv) { return kv.v_trans; }
uint32_t access::n_rot_k(const llama_kv_cache & kv) { return kv.n_rot_k; }
uint32_t access::n_rot_v(const llama_kv_cache & kv) { return kv.n_rot_v; }
bool access::has_cell_ext(const llama_kv_cache & kv) { return kv.hparams.n_pos_per_embd() > 1 || kv.hparams.ple_n_heads > 0; }
uint32_t access::n_embd_k(const llama_kv_cache & kv, uint32_t il) { return kv.hparams.n_embd_k_gqa(il); }
uint32_t access::n_embd_v(const llama_kv_cache & kv, uint32_t il) { return kv.hparams.n_embd_v_gqa(il); }
ggml_type access::type_k(const llama_kv_cache & kv) { return kv.layers.empty() ? GGML_TYPE_F32 : kv.layers[0].k->type; }
ggml_type access::type_v(const llama_kv_cache & kv) { return kv.layers.empty() ? GGML_TYPE_F32 : kv.layers[0].v->type; }
llama_kv_cache::stream_copy_info & access::sc_info(llama_kv_cache & kv) { return kv.sc_info; }
const std::vector<uint32_t> & access::seq_to_stream(const llama_kv_cache & kv) { return kv.seq_to_stream; }

std::vector<llama_pos> & access::pos(llama_kv_cells & c) { return c.pos; }
std::vector<llama_kv_cell_ext> & access::ext(llama_kv_cells & c) { return c.ext; }
std::vector<llama_pos> & access::shift(llama_kv_cells & c) { return c.shift; }
std::vector<std::bitset<LLAMA_MAX_SEQ>> & access::seq(llama_kv_cells & c) { return c.seq; }
std::set<uint32_t> & access::used(llama_kv_cells & c) { return c.used; }
std::set<std::pair<llama_pos, uint32_t>> (&access::seq_pos(llama_kv_cells & c))[LLAMA_MAX_SEQ] { return c.seq_pos; }
bool & access::has_shift(llama_kv_cells & c) { return c.has_shift; }

int32_t & access::n_kv(llama_kv_cache_context & ctx) { return ctx.n_kv; }
size_t & access::i_cur(llama_kv_cache_context & ctx) { return ctx.i_cur; }

} // namespace llama_kv_goose

extern "C" {

int64_t fg_kv_native_cells(void * native, int64_t strm) {
    llama_kv_cache * kv = kv_of(native);
    const int64_t h = fg_w2_attach_cells(&access::cells(*kv, (uint32_t) strm));
    cell_wrap w;
    w.cells = &access::cells(*kv, (uint32_t) strm);
    g_cell_wraps[h] = std::move(w);
    return h;
}

int64_t fg_w2_native_get(int64_t handle, int64_t k, int64_t i) {
    cell_wrap & w = wrap_of(handle);
    llama_kv_cells & c = *w.cells;
    if (k == 0)  { return (int64_t) c.size(); }
    if (k == 1)  { return access::has_shift(c) ? 1 : 0; }
    if (k == 10) { return (int64_t) access::pos(c).at((size_t) i); }
    if (k == 11) { return (int64_t) access::shift(c).at((size_t) i); }
    if (k == 12) { return (int64_t) access::ext(c).at((size_t) i).x; }
    if (k == 13) { return (int64_t) access::ext(c).at((size_t) i).y; }
    if (k == 14) { return (int64_t) access::ext(c).at((size_t) i).tok; }
    if (k == 20) { return used_wrap(w); }
    if (k == 25) { return seq_wrap(w, i); }
    if (k > 25 && k < 25 + LLAMA_MAX_SEQ) { return seq_wrap(w, k - 25); }
    if (k >= 1000) {
        const size_t s = (size_t) (k - 1000);
        return access::seq(c).at((size_t) i).test(s) ? 1 : 0;
    }
    std::abort();
}

void fg_w2_native_put(int64_t handle, int64_t k, int64_t i, int64_t v) {
    cell_wrap & w = wrap_of(handle);
    llama_kv_cells & c = *w.cells;
    if (k == 1) { access::has_shift(c) = (v != 0); return; }
    // Goose owns index maintenance through the wrapped real sets; these writes
    // are pure field/bitset updates so the real std::set indexes are touched
    // exactly once (by set_insert/set_erase on the wrappers).
    if (k == 10) { access::pos(c)[(size_t) i] = (llama_pos) v; return; }
    if (k == 11) { access::shift(c)[(size_t) i] = (llama_pos) v; return; }
    if (k == 12) { access::ext(c)[(size_t) i].x = (llama_pos) v; return; }
    if (k == 13) { access::ext(c)[(size_t) i].y = (llama_pos) v; return; }
    if (k == 14) { access::ext(c)[(size_t) i].tok = (llama_token) v; return; }
    if (k == 20) {
        access::used(c).clear();
        fg_w2_set_destroy(v); // the fresh set_create handle is unused
        return;
    }
    if (k == 25) {
        access::seq_pos(c)[i].clear();
        fg_w2_set_destroy(v);
        return;
    }
    if (k > 25 && k < 25 + LLAMA_MAX_SEQ) {
        access::seq_pos(c)[k - 25].clear();
        fg_w2_set_destroy(v);
        return;
    }
    if (k >= 1000) {
        const size_t s = (size_t) (k - 1000);
        auto & bs = access::seq(c).at((size_t) i);
        if (v != 0) { bs.set(s); } else { bs.reset(s); }
        return;
    }
    std::abort();
}

// ---- real metadata get/put ----
int64_t fg_kv_native_get(void * native, int64_t k, int64_t i) {
    llama_kv_cache * kv = kv_of(native);
    if (k == 0)  { return (int64_t) access::n_stream(*kv); }
    if (k == 1)  { return (int64_t) access::n_seq_max(*kv); }
    if (k == 2)  { return (int64_t) access::n_pad(*kv); }
    if (k == 3)  { return (int64_t) access::n_swa(*kv); }
    if (k == 4)  { return (int64_t) access::swa_type(*kv); }
    if (k == 5)  { return access::arch_step35(*kv) ? 1 : 0; }
    if (k == 6)  { return access::n_pos_per_embd(*kv); }
    if (k == 7)  { return (int64_t) access::n_layers(*kv); }
    if (k == 8)  { return access::ple_n_heads(*kv); }
    if (k == 9)  { return access::ple_image_token(*kv); }
    if (k == 60) { return access::ple_eos_token(*kv); }
    if (k == 61) { return access::rope_none(*kv) ? 1 : 0; }
    if (k == 62) { return kv->other != nullptr ? 1 : 0; }
    if (k == 63) { return (int64_t) access::type_k(*kv); }
    if (k == 64) { return (int64_t) access::type_v(*kv); }
    if (k == 65) { return (int64_t) access::sc_info(*kv).ssrc.size(); }
    if (k >= 200 && k < 300) { size_t x = (size_t)(k - 200); auto & v = access::sc_info(*kv).ssrc; return x < v.size() ? (int64_t) v[x] : 0; }
    if (k >= 300 && k < 400) { size_t x = (size_t)(k - 300); auto & v = access::sc_info(*kv).sdst; return x < v.size() ? (int64_t) v[x] : 0; }
    if (k >= 400 && k < 500) { size_t x = (size_t)(k - 400); return x < access::n_layers(*kv) && access::layer_has_v(*kv, (uint32_t) x) ? 1 : 0; }
    if (k >= 500 && k < 600) { size_t x = (size_t)(k - 500); return x < access::n_layers(*kv) && access::layer_has_rope(*kv, (uint32_t) x) ? 1 : 0; }
    if (k >= 100 && k < 200) { size_t s = (size_t)(k - 100); return s < (size_t) access::n_stream(*kv) ? (int64_t) access::head(*kv, (uint32_t) s) : 0; }
    std::abort();
}

void fg_kv_native_put(void * native, int64_t k, int64_t i, int64_t v) {
    llama_kv_cache * kv = kv_of(native);
    if (k == 100) { access::head(*kv, (uint32_t) i) = (uint32_t) v; return; }
    if (k >= 100 && k < 200) { access::head(*kv, (uint32_t)(k - 100)) = (uint32_t) v; return; }
    if (k == 65) { access::sc_info(*kv).ssrc.resize((size_t) v); access::sc_info(*kv).sdst.resize((size_t) v); return; }
    if (k >= 200 && k < 300) { size_t x = (size_t)(k - 200); auto & a = access::sc_info(*kv).ssrc; if (x >= a.size()) a.resize(x + 1); a[x] = (uint32_t) v; return; }
    if (k >= 300 && k < 400) { size_t x = (size_t)(k - 300); auto & a = access::sc_info(*kv).sdst; if (x >= a.size()) a.resize(x + 1); a[x] = (uint32_t) v; return; }
    if (k == 62) { std::abort(); } // other is fixed at construction
    std::abort();
}

void fg_kv_native_detach(void * native) { (void) native; }

void fg_w2_native_forget(int64_t handle) { g_cell_wraps.erase(handle); }

void fg_kv_native_forget(void * native) {
    for (auto it = g_kv_native.begin(); it != g_kv_native.end(); ) {
        if (it->second == native) { it = g_kv_native.erase(it); } else { ++it; }
    }
    g_handle_of.erase(static_cast<llama_kv_cache *>(native));
}

} // extern "C"

static void real_zero_cells(int64_t kvh, int64_t strm, int64_t sinfo, int64_t contiguous);
static void real_clear_data(int64_t kvh);

int64_t fg_kv_attach_real(llama_kv_cache * kv) {
    fg_w2_configure((int64_t) LLAMA_MAX_SEQ, (int64_t) LLAMA_TOKEN_NULL);
    const int64_t h = fg_kv_attach(kv, (int64_t) access::n_stream(*kv));
    g_kv_native[h] = kv;
    g_handle_of[kv] = h;
    fg_kv_bind_zero_cells(h, real_zero_cells);
    fg_kv_bind_clear_data(h, real_clear_data);
    return h;
}

static void real_zero_cells(int64_t kvh, int64_t strm, int64_t sinfo, int64_t contiguous) {
    (void) contiguous;
    auto it = g_kv_native.find(kvh);
    if (it == g_kv_native.end()) { return; }
    llama_kv_cache * kv = it->second;
    const int64_t n = fg_kv_get(sinfo, 2, 0);
    std::vector<int64_t> idx((size_t) n);
    for (int64_t j = 0; j < n; ++j) { idx[(size_t) j] = fg_kv_get(sinfo, 100, j); }
    for (auto & layer : kv->layers) {
        const uint32_t nk = kv->hparams.n_embd_k_gqa(layer.il);
        const uint32_t nv = kv->hparams.n_embd_v_gqa(layer.il);
        const size_t krow = ggml_row_size(layer.k->type, nk);
        const size_t vrow = layer.v ? ggml_row_size(layer.v->type, nv) : 0;
        std::vector<uint8_t> zk(krow, 0), zv(vrow, 0);
        ggml_tensor * kt = layer.k_stream[(size_t) strm];
        ggml_tensor * vt = layer.v ? layer.v_stream[(size_t) strm] : nullptr;
        for (int64_t j = 0; j < n; ++j) {
            ggml_backend_tensor_set(kt, zk.data(), (size_t) idx[(size_t) j] * krow, krow);
            if (vt) { ggml_backend_tensor_set(vt, zv.data(), (size_t) idx[(size_t) j] * vrow, vrow); }
        }
    }
}

static void real_clear_data(int64_t kvh) {
    auto it = g_kv_native.find(kvh);
    if (it == g_kv_native.end()) { return; }
    for (auto & cb : it->second->ctxs_bufs) {
        if (cb.second) { ggml_backend_buffer_clear(cb.second.get(), 0); }
    }
}

void fg_kv_configure_real(int64_t kvh, llama_kv_cache * kv) {
    fg_kv_configure(kvh,
            (int64_t) access::n_seq_max(*kv),
            (int64_t) access::n_pad(*kv),
            (int64_t) access::n_swa(*kv),
            (int64_t) access::swa_type(*kv),
            access::arch_step35(*kv) ? 1 : 0,
            access::n_pos_per_embd(*kv),
            (int64_t) access::n_layers(*kv),
            access::ple_n_heads(*kv),
            access::ple_image_token(*kv),
            access::ple_eos_token(*kv),
            access::rope_none(*kv) ? 1 : 0,
            (int64_t) access::type_k(*kv),
            (int64_t) access::type_v(*kv));
}

namespace {

int64_t life(int64_t mode, int64_t a, int64_t b = 0, int64_t c = 0,
             int64_t d = 0, int64_t e = 0, int64_t f = 0) {
    return fg_goose_kv_lifecycle_entry(mode, a, b, c, d, e, f);
}

llama_kv_cache::slot_info slot_from(int64_t h) {
    llama_kv_cache::slot_info s;
    s.s0 = (uint32_t) fg_kv_get(h, 0, 0);
    s.s1 = (uint32_t) fg_kv_get(h, 1, 0);
    const uint32_t ns = (uint32_t) fg_kv_get(h, 3, 0);
    s.resize(ns);
    for (uint32_t x = 0; x < ns; ++x) {
        s.strm[x] = (llama_seq_id) fg_kv_get(h, 10 + x, 0);
        const int64_t n = fg_kv_get(h, 2, 0);
        for (int64_t j = 0; j < n; ++j) {
            s.idxs[x].push_back((uint32_t) fg_kv_get(h, 100 + x, j));
        }
    }
    return s;
}

class goose_batch_ctx : public llama_kv_cache_context {
public:
    goose_batch_ctx(llama_kv_cache * kv, int64_t gkv, int64_t gctx,
                    std::vector<llama_kv_cache::slot_info> sinfos,
                    std::vector<llama_ubatch> ubs,
                    std::vector<int64_t> sh, std::vector<int64_t> uh)
        : llama_kv_cache_context(kv, std::move(sinfos), std::move(ubs)),
          gkv_(gkv), gctx_(gctx), sh_(std::move(sh)), uh_(std::move(uh)) {
        access::n_kv(*this) = 0;
    }
    ~goose_batch_ctx() override { life(15, gctx_); }
    bool next() override { return llama_kv_cache_context::next(); }
    bool apply() override {
        const size_t i = access::i_cur(*this);
        if (i >= uh_.size()) { return false; }
        life(4, gkv_, sh_[i], uh_[i]);
        const int64_t n = life(17, gkv_, sh_[i]);
        access::n_kv(*this) = (int32_t) n;
        return true;
    }
private:
    int64_t gkv_, gctx_;
    std::vector<int64_t> sh_, uh_;
};

class goose_full_ctx : public llama_kv_cache_context {
public:
    goose_full_ctx(llama_kv_cache * kv, int64_t gkv, int64_t gctx)
        : llama_kv_cache_context(kv), gkv_(gkv), gctx_(gctx) {}
    ~goose_full_ctx() override { life(15, gctx_); }
    bool apply() override { life(9, gctx_); return true; }
private:
    int64_t gkv_, gctx_;
};

class goose_update_ctx : public llama_kv_cache_context {
public:
    goose_update_ctx(llama_kv_cache * kv, llama_context * lctx, bool do_shift,
                     llama_kv_cache::stream_copy_info sc, int64_t gkv, int64_t gctx)
        : llama_kv_cache_context(kv, lctx, do_shift, std::move(sc)), gkv_(gkv), gctx_(gctx) {}
    ~goose_update_ctx() override { life(15, gctx_); }
    bool apply() override { life(9, gctx_); return true; }
private:
    int64_t gkv_, gctx_;
};

} // namespace

llama_memory_context_ptr fg_goose_context_word(llama_memory_status status) {
    return std::make_unique<llama_kv_cache_context>(status);
}

llama_memory_context_ptr fg_goose_context_full(llama_kv_cache * kv, int64_t gkv, int64_t gctx) {
    return std::make_unique<goose_full_ctx>(kv, gkv, gctx);
}

llama_memory_context_ptr fg_goose_context_batch(llama_kv_cache * kv, int64_t gkv, int64_t gctx) {
    const int64_t ns = fg_kv_get(gctx, 7, 0);
    const int64_t nu = fg_kv_get(gctx, 6, 0);
    std::vector<llama_kv_cache::slot_info> sinfos;
    std::vector<llama_ubatch> ubs;
    std::vector<int64_t> sh, uh;
    for (int64_t i = 0; i < ns; ++i) {
        const int64_t h = fg_kv_get(gctx, 200 + i, 0);
        sh.push_back(h);
        sinfos.push_back(slot_from(h));
    }
    for (int64_t i = 0; i < nu; ++i) {
        const int64_t h = fg_kv_get(gctx, 100 + i, 0);
        uh.push_back(h);
        ubs.push_back(*reinterpret_cast<llama_ubatch *>(static_cast<intptr_t>(h)));
    }
    return std::make_unique<goose_batch_ctx>(kv, gkv, gctx, std::move(sinfos),
            std::move(ubs), std::move(sh), std::move(uh));
}

llama_memory_context_ptr fg_goose_context_update(llama_kv_cache * kv, int64_t gkv, int64_t gctx,
        llama_context * lctx, bool do_shift, int64_t sc_count,
        const uint32_t * sc_src, const uint32_t * sc_dst) {
    llama_kv_cache::stream_copy_info sc;
    for (int64_t i = 0; i < sc_count; ++i) {
        sc.ssrc.push_back(sc_src[i]);
        sc.sdst.push_back(sc_dst[i]);
    }
    return std::make_unique<goose_update_ctx>(kv, lctx, do_shift, std::move(sc), gkv, gctx);
}


// ---------------------------------------------------------------------------
// State/IO primitives and public-API hooks for the patched real TU.
// ---------------------------------------------------------------------------
#include "llama-io.h"

extern "C" {
int64_t fg_goose_kv_lifecycle_entry(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg_goose_kv_seq_entry(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);

static int64_t handle_of(llama_kv_cache * kv) {
    auto it = g_handle_of.find(kv);
    return it == g_handle_of.end() ? 0 : it->second;
}
static int64_t life2(int64_t mode, int64_t a, int64_t b = 0, int64_t c = 0,
                     int64_t d = 0, int64_t e = 0, int64_t f = 0) {
    return fg_goose_kv_lifecycle_entry(mode, a, b, c, d, e, f);
}
static int64_t seq2(int64_t mode, int64_t a, int64_t b = 0, int64_t c = 0,
                    int64_t d = 0, int64_t e = 0) {
    return fg_goose_kv_seq_entry(mode, a, b, c, d, e);
}

void fg_state_write_u32(void * io, int64_t v) {
    uint32_t x = (uint32_t) v;
    static_cast<llama_io_write_i *>(io)->write(&x, sizeof(x));
}
int64_t fg_state_read_u32(void * io) {
    uint32_t x = 0;
    static_cast<llama_io_read_i *>(io)->read(&x, sizeof(x));
    return (int64_t) x;
}
void fg_state_write_i32(void * io, int64_t v) {
    int32_t x = (int32_t) v;
    static_cast<llama_io_write_i *>(io)->write(&x, sizeof(x));
}
int64_t fg_state_read_i32(void * io) {
    int32_t x = 0;
    static_cast<llama_io_read_i *>(io)->read(&x, sizeof(x));
    return (int64_t) x;
}
void fg_state_write_u64(void * io, int64_t v) {
    uint64_t x = (uint64_t) v;
    static_cast<llama_io_write_i *>(io)->write(&x, sizeof(x));
}
int64_t fg_state_read_u64(void * io) {
    uint64_t x = 0;
    static_cast<llama_io_read_i *>(io)->read(&x, sizeof(x));
    return (int64_t) x;
}

static llama_kv_cache * kv_of_handle(int64_t kvh) {
    auto it = g_kv_native.find(kvh);
    if (it == g_kv_native.end()) { std::abort(); }
    return it->second;
}
int64_t fg_state_v_trans(int64_t kvh) { return access::v_trans(*kv_of_handle(kvh)) ? 1 : 0; }
int64_t fg_state_n_rot(int64_t kvh, int64_t is_v) { return is_v ? access::n_rot_v(*kv_of_handle(kvh)) : access::n_rot_k(*kv_of_handle(kvh)); }
int64_t fg_state_has_cell_ext(int64_t kvh) { return access::has_cell_ext(*kv_of_handle(kvh)) ? 1 : 0; }
int64_t fg_state_layer_il(int64_t kvh, int64_t idx) { return (int64_t) access::layer_il(*kv_of_handle(kvh), (uint32_t) idx); }
int64_t fg_state_layer_has_v(int64_t kvh, int64_t idx) { return access::layer_has_v(*kv_of_handle(kvh), (uint32_t) idx) ? 1 : 0; }
int64_t fg_state_n_embd_k(int64_t kvh, int64_t il) { return (int64_t) access::n_embd_k(*kv_of_handle(kvh), (uint32_t) il); }
int64_t fg_state_n_embd_v(int64_t kvh, int64_t il) { return (int64_t) access::n_embd_v(*kv_of_handle(kvh), (uint32_t) il); }
int64_t fg_state_kv_size(int64_t kvh) { return (int64_t) access::cells(*kv_of_handle(kvh), 0).size(); }
static ggml_tensor * state_tensor(int64_t kvh, int64_t strm, int64_t idx, int64_t is_v) {
    llama_kv_cache * kv = kv_of_handle(kvh);
    auto & layer = kv->layers[(size_t) idx];
    return is_v ? layer.v_stream[(size_t) strm] : layer.k_stream[(size_t) strm];
}
int64_t fg_state_tensor_type(int64_t kvh, int64_t strm, int64_t idx, int64_t is_v) {
    ggml_tensor * t = state_tensor(kvh, strm, idx, is_v);
    return t ? (int64_t) t->type : -1;
}
int64_t fg_state_tensor_row_size(int64_t kvh, int64_t strm, int64_t idx, int64_t is_v) {
    llama_kv_cache * kv = kv_of_handle(kvh);
    auto & layer = kv->layers[(size_t) idx];
    ggml_tensor * t = state_tensor(kvh, strm, idx, is_v);
    if (!t) { return 0; }
    const uint32_t n = is_v ? access::n_embd_v(*kv, layer.il) : access::n_embd_k(*kv, layer.il);
    return (int64_t) ggml_row_size(t->type, n);
}
int64_t fg_state_tensor_size_el(int64_t kvh, int64_t strm, int64_t idx, int64_t is_v) {
    ggml_tensor * t = state_tensor(kvh, strm, idx, is_v);
    return t ? (int64_t) ggml_type_size(t->type) : 0;
}
void fg_state_write_rows(int64_t kvh, void * io, int64_t strm, int64_t idx, int64_t is_v, int64_t from, int64_t to) {
    ggml_tensor * t = state_tensor(kvh, strm, idx, is_v);
    const int64_t row = fg_state_tensor_row_size(kvh, strm, idx, is_v);
    static_cast<llama_io_write_i *>(io)->write_tensor(t, (size_t) from * row, (size_t) (to - from) * row);
}
void fg_state_read_rows(int64_t kvh, void * io, int64_t strm, int64_t idx, int64_t is_v, int64_t from, int64_t to) {
    ggml_tensor * t = state_tensor(kvh, strm, idx, is_v);
    const int64_t row = fg_state_tensor_row_size(kvh, strm, idx, is_v);
    static_cast<llama_io_read_i *>(io)->read_tensor(t, (size_t) from * row, (size_t) (to - from) * row);
}
void fg_state_write_trans(int64_t kvh, void * io, int64_t strm, int64_t idx, int64_t j, int64_t from, int64_t to) {
    ggml_tensor * t = state_tensor(kvh, strm, idx, 1);
    const int64_t el = fg_state_tensor_size_el(kvh, strm, idx, 1);
    const int64_t kv_size = fg_state_kv_size(kvh);
    const size_t off = (size_t) (from + j * kv_size) * el;
    static_cast<llama_io_write_i *>(io)->write_tensor(t, off, (size_t) (to - from) * el);
}
void fg_state_read_trans(int64_t kvh, void * io, int64_t strm, int64_t idx, int64_t j, int64_t from, int64_t to) {
    ggml_tensor * t = state_tensor(kvh, strm, idx, 1);
    const int64_t el = fg_state_tensor_size_el(kvh, strm, idx, 1);
    const int64_t kv_size = fg_state_kv_size(kvh);
    const size_t off = (size_t) (from + j * kv_size) * el;
    static_cast<llama_io_read_i *>(io)->read_tensor(t, off, (size_t) (to - from) * el);
}

int64_t fg_sinfos_count(void * p) {
    return p ? (int64_t) static_cast<llama_kv_cache::slot_info_vec_t *>(p)->size() : -1;
}
void fg_sinfos_init(void * p, int64_t n) {
    static_cast<llama_kv_cache::slot_info_vec_t *>(p)->assign((size_t) n, llama_kv_cache::slot_info{});
}
int64_t fg_sinfos_in_empty(void * p, int64_t s) {
    auto & v = *static_cast<llama_kv_cache::slot_info_vec_t *>(p);
    return v[(size_t) s].empty() ? 1 : 0;
}
void fg_sinfos_in_to_slot(void * p, int64_t s, int64_t slot) {
    auto & si = (*static_cast<llama_kv_cache::slot_info_vec_t *>(p))[(size_t) s];
    fg_kv_put(slot, 0, 0, si.s0);
    fg_kv_put(slot, 1, 0, si.s1);
    fg_kv_put(slot, 3, 0, (int64_t) si.n_stream());
    fg_kv_put(slot, 2, 0, si.idxs.empty() ? 0 : (int64_t) si.idxs[0].size());
    for (uint32_t x = 0; x < si.n_stream(); ++x) {
        fg_kv_put(slot, 10 + x, 0, si.strm[x]);
        for (size_t j = 0; j < si.idxs[x].size(); ++j) { fg_kv_slot_push(slot, x, si.idxs[x][j]); }
    }
}
void fg_sinfos_set_out(void * p, int64_t s, int64_t slot) {
    auto & v = *static_cast<llama_kv_cache::slot_info_vec_t *>(p);
    auto & si = v[(size_t) s];
    si.s0 = (uint32_t) fg_kv_get(slot, 0, 0);
    si.s1 = (uint32_t) fg_kv_get(slot, 1, 0);
    const uint32_t nstr = (uint32_t) fg_kv_get(slot, 3, 0);
    si.resize(nstr);
    for (uint32_t x = 0; x < nstr; ++x) {
        si.strm[x] = (llama_seq_id) fg_kv_get(slot, 10 + x, 0);
        const int64_t m = fg_kv_get(slot, 2, 0);
        for (int64_t j = 0; j < m; ++j) { si.idxs[x].push_back((uint32_t) fg_kv_get(slot, 100 + x, j)); }
    }
}

void fg_state_fail(int64_t code) {
    switch (code) {
        case 1: throw std::runtime_error("n_stream mismatch");
        case 2: throw std::runtime_error("failed to restore kv cache: mirrored slot layout has the wrong stream count");
        case 3: throw std::runtime_error("failed to restore kv cache: mirrored cache holds cells this one does not");
        case 4: throw std::runtime_error("failed to restore kv cache");
        case 5: throw std::runtime_error("invalid seq_id-agnostic kv cell");
        case 6: throw std::runtime_error("not enough cells in kv cache");
        case 7: throw std::runtime_error("mismatched layer count");
        case 8: throw std::runtime_error("incompatible V transposition");
        case 9: throw std::runtime_error("incompatible key rotation");
        case 10: throw std::runtime_error("incompatible value rotation");
        case 11: throw std::runtime_error("mismatched key type");
        case 12: throw std::runtime_error("mismatched key row size");
        case 13: throw std::runtime_error("mismatched value type");
        case 14: throw std::runtime_error("mismatched value row size");
        case 15: throw std::runtime_error("mismatched value element size");
        case 16: throw std::runtime_error("mismatched GQA embedding size");
        case 17: throw std::runtime_error("invalid seq_id");
        case 18: throw std::runtime_error("failed to find available cells in kv cache");
        default: throw std::runtime_error("failed to restore kv cache");
    }
}

void * fg_goose_init_batch(void * kvp, void * balloc, uint32_t n, int embd) {
    llama_kv_cache * kv = static_cast<llama_kv_cache *>(kvp);
    const int64_t gkv = handle_of(kv);
    if (gkv == 0) { return nullptr; }
    const int64_t gctx = life2(5, gkv, (int64_t) (intptr_t) balloc, (int64_t) n, (int64_t) embd);
    const int64_t st = life2(10, gctx);
    if (st != 0) { life2(15, gctx); return new llama_kv_cache_context((llama_memory_status) st); }
    return fg_goose_context_batch(kv, gkv, gctx).release();
}
void * fg_goose_init_full(void * kvp) {
    llama_kv_cache * kv = static_cast<llama_kv_cache *>(kvp);
    const int64_t gkv = handle_of(kv);
    if (gkv == 0) { return nullptr; }
    return fg_goose_context_full(kv, gkv, life2(6, gkv)).release();
}
void * fg_goose_init_update(void * kvp, void * lctx_ptr) {
    llama_kv_cache * kv = static_cast<llama_kv_cache *>(kvp);
    const int64_t gkv = handle_of(kv);
    if (gkv == 0) { return nullptr; }
    auto & sc = access::sc_info(*kv);
    std::vector<uint32_t> src = sc.ssrc, dst = sc.sdst;
    const bool do_shift = access::has_shift(access::cells(*kv, 0));
    const int64_t gctx = life2(7, gkv, (int64_t) (intptr_t) lctx_ptr);
    return fg_goose_context_update(kv, gkv, gctx, static_cast<llama_context *>(lctx_ptr),
            do_shift, (int64_t) src.size(), src.data(), dst.data()).release();
}
int fg_goose_update(void * kvp, void * lctx_ptr, int do_shift, const void * scp, int * updated) {
    llama_kv_cache * kv = static_cast<llama_kv_cache *>(kvp);
    const int64_t gkv = handle_of(kv);
    if (gkv == 0) { return 0; }
    const auto * sc = static_cast<const llama_kv_cache::stream_copy_info *>(scp);
    const int64_t ctx = life2(20, 0);
    const int64_t n = (int64_t) sc->ssrc.size();
    fg_kv_put(ctx, 8, 0, n);
    for (int64_t i = 0; i < n; ++i) {
        fg_kv_put(ctx, 300 + i, 0, sc->ssrc[(size_t) i]);
        fg_kv_put(ctx, 400 + i, 0, sc->sdst[(size_t) i]);
    }
    const int64_t r = life2(21, gkv, (int64_t) (intptr_t) lctx_ptr, do_shift, n, ctx);
    life2(15, ctx);
    *updated = (int) r;
    return 1;
}

int fg_goose_seq_rm(void * kvp, int seq, int p0, int p1) {
    const int64_t gkv = handle_of(static_cast<llama_kv_cache *>(kvp));
    if (gkv == 0) { return 0; }
    seq2(3, gkv, seq, p0, p1);
    return 1;
}
int fg_goose_seq_cp(void * kvp, int src, int dst, int p0, int p1) {
    const int64_t gkv = handle_of(static_cast<llama_kv_cache *>(kvp));
    if (gkv == 0) { return 0; }
    seq2(4, gkv, src, dst, p0, p1);
    return 1;
}
int fg_goose_seq_keep(void * kvp, int seq) {
    const int64_t gkv = handle_of(static_cast<llama_kv_cache *>(kvp));
    if (gkv == 0) { return 0; }
    seq2(5, gkv, seq);
    return 1;
}
int fg_goose_seq_add(void * kvp, int seq, int p0, int p1, int shift) {
    const int64_t gkv = handle_of(static_cast<llama_kv_cache *>(kvp));
    if (gkv == 0) { return 0; }
    seq2(6, gkv, seq, p0, p1, shift);
    return 1;
}
int fg_goose_seq_div(void * kvp, int seq, int p0, int p1, int d) {
    const int64_t gkv = handle_of(static_cast<llama_kv_cache *>(kvp));
    if (gkv == 0) { return 0; }
    seq2(7, gkv, seq, p0, p1, d);
    return 1;
}
int fg_goose_seq_pos_min(void * kvp, int seq, int * out) {
    const int64_t gkv = handle_of(static_cast<llama_kv_cache *>(kvp));
    if (gkv == 0) { return 0; }
    *out = (int) seq2(8, gkv, seq);
    return 1;
}
int fg_goose_seq_pos_max(void * kvp, int seq, int * out) {
    const int64_t gkv = handle_of(static_cast<llama_kv_cache *>(kvp));
    if (gkv == 0) { return 0; }
    *out = (int) seq2(9, gkv, seq);
    return 1;
}
int fg_goose_clear(void * kvp, int data) {
    const int64_t gkv = handle_of(static_cast<llama_kv_cache *>(kvp));
    if (gkv == 0) { return 0; }
    seq2(2, gkv, data);
    return 1;
}
int fg_goose_state_clear(void * kvp, int seq) {
    const int64_t gkv = handle_of(static_cast<llama_kv_cache *>(kvp));
    if (gkv == 0) { return 0; }
    seq2(10, gkv, seq);
    return 1;
}
int fg_goose_state_write(void * kvp, void * io, int seq, int flags) {
    const int64_t gkv = handle_of(static_cast<llama_kv_cache *>(kvp));
    if (gkv == 0) { return 0; }
    life2(22, gkv, (int64_t) (intptr_t) io, seq, flags);
    return 1;
}
int fg_goose_state_read(void * kvp, void * io, int seq, int flags) {
    const int64_t gkv = handle_of(static_cast<llama_kv_cache *>(kvp));
    if (gkv == 0) { return 0; }
    life2(23, gkv, (int64_t) (intptr_t) io, seq, flags, 0, 0);
    return 1;
}
int fg_goose_state_read_sinfo(void * kvp, void * io, int seq, int flags, void * sinfos_out, const void * sinfos_in) {
    const int64_t gkv = handle_of(static_cast<llama_kv_cache *>(kvp));
    if (gkv == 0) { return 0; }
    life2(23, gkv, (int64_t) (intptr_t) io, seq, flags,
          (int64_t) (intptr_t) sinfos_out, (int64_t) (intptr_t) sinfos_in);
    return 1;
}
void * fg_goose_prepare(void * kvp, const void * ub_array, size_t n) {
    llama_kv_cache * kv = static_cast<llama_kv_cache *>(kvp);
    const int64_t gkv = handle_of(kv);
    if (gkv == 0) { return nullptr; }
    const int64_t v = fg_kv_obj(4);
    const llama_ubatch * u = static_cast<const llama_ubatch *>(ub_array);
    for (size_t i = 0; i < n; ++i) { fg_kv_vec_push(v, (int64_t) (intptr_t) &u[i]); }
    const int64_t vec = life2(3, gkv, v);
    if (vec == 0) { return nullptr; }
    auto * out = new llama_kv_cache::slot_info_vec_t();
    const int64_t ns = fg_kv_get(vec, 0, 0);
    (void) ns;
    for (int64_t i = 0; i < ns; ++i) {
        const int64_t sh = fg_kv_get(vec, 10 + i, 0);
        llama_kv_cache::slot_info si;
        si.s0 = (uint32_t) fg_kv_get(sh, 0, 0);
        si.s1 = (uint32_t) fg_kv_get(sh, 1, 0);
        const uint32_t nstr = (uint32_t) fg_kv_get(sh, 3, 0);
        si.resize(nstr);
        for (uint32_t x = 0; x < nstr; ++x) {
            si.strm[x] = (llama_seq_id) fg_kv_get(sh, 10 + x, 0);
            const int64_t m = fg_kv_get(sh, 2, 0);
            for (int64_t j = 0; j < m; ++j) { si.idxs[x].push_back((uint32_t) fg_kv_get(sh, 100 + x, j)); }
        }
        out->push_back(std::move(si));
    }
    return out;
}
void * fg_goose_find_slot(void * kvp, const void * ub, int cont) {
    llama_kv_cache * kv = static_cast<llama_kv_cache *>(kvp);
    const int64_t gkv = handle_of(kv);
    if (gkv == 0) { return nullptr; }
    const int64_t sh = life2(2, gkv, (int64_t) (intptr_t) ub, cont);
    if (sh == 0) { return nullptr; }
    auto * out = new llama_kv_cache::slot_info();
    out->s0 = (uint32_t) fg_kv_get(sh, 0, 0);
    out->s1 = (uint32_t) fg_kv_get(sh, 1, 0);
    const uint32_t nstr = (uint32_t) fg_kv_get(sh, 3, 0);
    out->resize(nstr);
    for (uint32_t x = 0; x < nstr; ++x) {
        out->strm[x] = (llama_seq_id) fg_kv_get(sh, 10 + x, 0);
        const int64_t m = fg_kv_get(sh, 2, 0);
        for (int64_t j = 0; j < m; ++j) { out->idxs[x].push_back((uint32_t) fg_kv_get(sh, 100 + x, j)); }
    }
    return out;
}
int fg_goose_apply_ubatch(void * kvp, const void * sinfop, const void * ubp) {
    llama_kv_cache * kv = static_cast<llama_kv_cache *>(kvp);
    const int64_t gkv = handle_of(kv);
    if (gkv == 0) { return 0; }
    const auto * si = static_cast<const llama_kv_cache::slot_info *>(sinfop);
    const int64_t sh = fg_kv_obj(3);
    fg_kv_put(sh, 0, 0, si->s0);
    fg_kv_put(sh, 1, 0, si->s1);
    fg_kv_put(sh, 3, 0, si->n_stream());
    for (uint32_t x = 0; x < si->n_stream(); ++x) {
        fg_kv_put(sh, 10 + x, 0, si->strm[x]);
        for (size_t j = 0; j < si->idxs[x].size(); ++j) {
            fg_kv_slot_push(sh, x, si->idxs[x][j]);
        }
    }
    life2(4, gkv, sh, (int64_t) (intptr_t) ubp);
    fg_kv_destroy(sh);
    return 1;
}
}
