// worker-2/kv-bridge.cpp
//
// Opaque metadata/collection store + typed access to native llama_batch_allocr
// and llama_ubatch. This file contains no llama_kv_cache control policy: it
// only stores Goose handles and reads public ubatch/balloc fields/methods.
//
// Numeric cache values absent from the snapshot (max_seq, token_null, hparams
// flags) are injected via fg_kv_configure by the parent. No private offsets and
// no emitter function IDs are fabricated.

#include "kv-bridge.h"
#include "llama-batch.h"
#include "llama.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unordered_set>
#include <vector>
#include <utility>

extern "C" {
// cells bridge, linked separately (worker-2/storage-bridge.cpp)
int64_t fg_w2_get(int64_t p, int64_t k, int64_t i);
int64_t fg_w2_service(int64_t op, int64_t a, int64_t b);
void    fg_w2_configure(int64_t max_seq, int64_t token_null);
void    fg_w2_set_destroy(int64_t h);

// Implemented by kv-native-adapter.cpp when the real cache build is linked.
// Weak so the standalone store still compiles and runs.
int64_t fg_kv_native_get(void * native, int64_t k, int64_t i) __attribute__((weak));
void    fg_kv_native_put(void * native, int64_t k, int64_t i, int64_t v) __attribute__((weak));
int64_t fg_kv_native_cells(void * native, int64_t strm) __attribute__((weak));
void    fg_kv_native_detach(void * native) __attribute__((weak));
void    fg_kv_native_forget(void * native) __attribute__((weak));
}

namespace {

constexpr uint32_t MAGIC = 0x57b2c001u;
enum kind_t : uint32_t { KIND_KV = 1, KIND_CTX = 2, KIND_SLOT = 3, KIND_VEC = 4, KIND_STATE = 5 };

struct base {
    uint32_t magic = MAGIC;
    uint32_t kind = 0;
    virtual ~base() = default;
};

struct cfg_t {
    int64_t n_seq_max = 1, n_pad = 1, n_swa = 0, swa_type = 0, arch_step35 = 0;
    int64_t n_pos_per_embd = 1, n_layers = 0, ple_n_heads = 0, ple_img = 0, ple_eos = 0;
    int64_t rope_none = 1, type_k = 0, type_v = 0;
};

struct cb_t {
    fg_kv_swa_masked_fn  swa = nullptr;
    fg_kv_sync_fn        sync = nullptr;
    fg_kv_tensor_copy_fn copy = nullptr;
    fg_kv_shift_fn       shift = nullptr;
    fg_kv_zero_cells_fn  zero = nullptr;
    fg_kv_clear_data_fn  clear = nullptr;
    fg_kv_other_pos_fn   other = nullptr;
};

struct kv_obj : base {
    cfg_t cfg;
    cb_t  cb;
    void * native = nullptr; // set by fg_kv_attach(); NULL = standalone store

    int64_t n_stream = 0;
    int64_t n_seq_max = 1;
    int64_t n_pad = 1;
    int64_t n_swa = 0;
    int64_t swa_type = 0;
    int64_t arch_step35 = 0;
    int64_t n_pos_per_embd = 1;
    int64_t n_layers = 0;
    int64_t ple_n_heads = 0;
    int64_t ple_image_tok = 0;
    int64_t ple_eos_tok = 0;
    int64_t rope_none = 1;
    int64_t type_k = 0;
    int64_t type_v = 0;
    int64_t has_other = 0;

    std::vector<int64_t> cells;
    std::vector<int64_t> heads;
    std::vector<int64_t> sc_src;
    std::vector<int64_t> sc_dst;
    std::vector<char>    layer_has_v;
    std::vector<char>    layer_has_rope;
};

struct slot_obj : base {
    int64_t s0 = 0;
    int64_t s1 = 0;
    int64_t size_hint = 0;
    int64_t n_stream = 0;
    std::vector<int64_t> strm;
    std::vector<std::vector<int64_t>> idxs;
};

struct vec_obj : base { std::vector<int64_t> items; };

struct state_obj : base {
    int64_t sinfo = 0;
    std::vector<int64_t> heads;
    std::vector<int64_t> snaps;
};

struct ctx_obj : base {
    int64_t status = 0;
    int64_t kv = 0;
    int64_t lctx = 0;
    int64_t i_cur = 0;
    int64_t n_kv = 0;
    int64_t do_shift = 0;
    int64_t sc_count = 0;
    std::vector<int64_t> ubatches;
    std::vector<int64_t> sinfos;
    std::vector<int64_t> sc_src;
    std::vector<int64_t> sc_dst;
};

// Compile-time limits now come from the real headers, not from unknown inputs.
// They are identical for every cache instance, so configuring the cells bridge
// once is safe; mutable configuration and callbacks are per kv_obj below.

inline base * as_obj(int64_t h) {
    base * b = reinterpret_cast<base *>(static_cast<intptr_t>(h));
    if (b == nullptr || b->magic != MAGIC) {
        std::fprintf(stderr, "fg_kv: bad handle\n");
        std::abort();
    }
    return b;
}
inline int64_t to_h(void * p) { return static_cast<int64_t>(reinterpret_cast<intptr_t>(p)); }

void destroy_cell_bundle(int64_t c) {
    if (c == 0) { return; }
    fg_w2_set_destroy(fg_w2_get(c, 20, 0));
    for (int64_t s = 0; s < LLAMA_MAX_SEQ; ++s) {
        fg_w2_set_destroy(fg_w2_get(c, 25 + s, 0));
    }
    fg_w2_service(2, c, 0);
}

// ---- synthetic ubatch/balloc for the in-module selftest and replay ----
struct fake_ub {
    int64_t n_tokens = 0;
    std::vector<int64_t> pos;
    std::vector<std::vector<int64_t>> seq_id;
    std::vector<int64_t> n_seq_id;
    std::vector<int64_t> token;
    std::vector<int64_t> seq_id_unq;
    std::vector<int64_t> type;
    bool has_token = false;
    int is2d = 0;
    int mixed = 0;
};
struct fake_balloc { std::vector<int64_t> ubs; size_t idx = 0; int64_t n_tokens = 0; int64_t n_used = 0; };

std::unordered_set<uintptr_t> g_fake_ub;
std::unordered_set<uintptr_t> g_fake_balloc;

bool is_fake_ub(int64_t u) { return g_fake_ub.count(static_cast<uintptr_t>(u)) != 0; }
bool is_fake_balloc(int64_t b) { return g_fake_balloc.count(static_cast<uintptr_t>(b)) != 0; }

fake_ub * fake_of(int64_t u) { return reinterpret_cast<fake_ub *>(static_cast<intptr_t>(u)); }
llama_ubatch * real_ub(int64_t u) { return reinterpret_cast<llama_ubatch *>(static_cast<intptr_t>(u)); }

} // namespace

extern "C" {

void fg_kv_configure(int64_t kvh, int64_t n_seq_max, int64_t n_pad,
                     int64_t n_swa, int64_t swa_type, int64_t arch_step35, int64_t n_pos_per_embd,
                     int64_t n_layers, int64_t ple_n_heads, int64_t ple_image_token_id,
                     int64_t ple_eos_token_id, int64_t rope_type_none, int64_t type_k, int64_t type_v) {
    base * b = as_obj(kvh);
    if (b->kind != KIND_KV) { std::abort(); }
    kv_obj * kv = static_cast<kv_obj *>(b);
    kv->cfg.n_seq_max = n_seq_max;
    kv->cfg.n_pad = n_pad;
    kv->cfg.n_swa = n_swa;
    kv->cfg.swa_type = swa_type;
    kv->cfg.arch_step35 = arch_step35;
    kv->cfg.n_pos_per_embd = n_pos_per_embd;
    kv->cfg.n_layers = n_layers;
    kv->cfg.ple_n_heads = ple_n_heads;
    kv->cfg.ple_img = ple_image_token_id;
    kv->cfg.ple_eos = ple_eos_token_id;
    kv->cfg.rope_none = rope_type_none;
    kv->cfg.type_k = type_k;
    kv->cfg.type_v = type_v;
    kv->n_seq_max = n_seq_max;
    kv->n_pad = n_pad;
    kv->n_swa = n_swa;
    kv->swa_type = swa_type;
    kv->arch_step35 = arch_step35;
    kv->n_pos_per_embd = n_pos_per_embd;
    kv->n_layers = n_layers;
    kv->ple_n_heads = ple_n_heads;
    kv->ple_image_tok = ple_image_token_id;
    kv->ple_eos_tok = ple_eos_token_id;
    kv->rope_none = rope_type_none;
    kv->type_k = type_k;
    kv->type_v = type_v;
    if ((int64_t) kv->layer_has_v.size() < n_layers) {
        kv->layer_has_v.resize((size_t) n_layers, 0);
        kv->layer_has_rope.resize((size_t) n_layers, 0);
    }
}

void fg_kv_bind_swa(int64_t kvh, fg_kv_swa_masked_fn fn) { static_cast<kv_obj *>(as_obj(kvh))->cb.swa = fn; }
void fg_kv_bind_sync(int64_t kvh, fg_kv_sync_fn fn) { static_cast<kv_obj *>(as_obj(kvh))->cb.sync = fn; }
void fg_kv_bind_tensor_copy(int64_t kvh, fg_kv_tensor_copy_fn fn) { static_cast<kv_obj *>(as_obj(kvh))->cb.copy = fn; }
void fg_kv_bind_shift(int64_t kvh, fg_kv_shift_fn fn) { static_cast<kv_obj *>(as_obj(kvh))->cb.shift = fn; }
void fg_kv_bind_zero_cells(int64_t kvh, fg_kv_zero_cells_fn fn) { static_cast<kv_obj *>(as_obj(kvh))->cb.zero = fn; }
void fg_kv_bind_clear_data(int64_t kvh, fg_kv_clear_data_fn fn) { static_cast<kv_obj *>(as_obj(kvh))->cb.clear = fn; }
void fg_kv_bind_other_pos(int64_t kvh, fg_kv_other_pos_fn fn) { static_cast<kv_obj *>(as_obj(kvh))->cb.other = fn; }

void fg_kv_set_layer(int64_t kvh, int64_t il, int64_t has_v, int64_t has_rope) {
    base * b = as_obj(kvh);
    if (b->kind != KIND_KV) { std::abort(); }
    kv_obj * kv = static_cast<kv_obj *>(b);
    if (il < 0 || il >= (int64_t) kv->layer_has_v.size()) { std::abort(); }
    kv->layer_has_v[(size_t) il] = has_v != 0;
    kv->layer_has_rope[(size_t) il] = has_rope != 0;
}

int64_t fg_kv_create(int64_t n_stream, int64_t kv_size) {
    static const bool cells_ready = [] {
        fg_w2_configure((int64_t) LLAMA_MAX_SEQ, (int64_t) LLAMA_TOKEN_NULL);
        return true;
    }();
    (void) cells_ready;
    kv_obj * kv = new kv_obj();
    kv->kind = KIND_KV;
    kv->n_stream = n_stream;
    kv->heads.assign((size_t) n_stream, 0);
    kv->cells.resize((size_t) n_stream, 0);
    for (int64_t s = 0; s < n_stream; ++s) {
        kv->cells[(size_t) s] = fg_w2_service(1, kv_size, 0);
        if (kv->cells[(size_t) s] == 0) { std::abort(); }
    }
    kv->layer_has_v.assign((size_t) kv->cfg.n_layers, 0);
    kv->layer_has_rope.assign((size_t) kv->cfg.n_layers, 0);
    return to_h(kv);
}

// Wrap a REAL llama_kv_cache instance. Metadata/cells/heads/sc_info are served
// by kv-native-adapter.cpp through typed hooks; this bridge only stores the
// opaque pointer and the attached cells handles.
int64_t fg_kv_attach(void * native, int64_t n_stream) {
    if (native == nullptr || n_stream <= 0) { std::abort(); }
    if (fg_kv_native_cells == nullptr) {
        std::fprintf(stderr, "fg_kv: native adapter is not linked\n");
        std::abort();
    }
    kv_obj * kv = new kv_obj();
    kv->kind = KIND_KV;
    kv->native = native;
    kv->n_stream = n_stream;
    kv->heads.assign((size_t) n_stream, 0);
    kv->cells.assign((size_t) n_stream, 0);
    for (int64_t s = 0; s < n_stream; ++s) {
        kv->cells[(size_t) s] = fg_kv_native_cells(native, s);
        if (kv->cells[(size_t) s] == 0) { std::abort(); }
    }
    return to_h(kv);
}

int64_t fg_kv_obj(int64_t kind) {
    base * b = nullptr;
    switch (kind) {
        case 1: b = new ctx_obj();  b->kind = KIND_CTX;   break;
        case 3: b = new slot_obj(); b->kind = KIND_SLOT;  break;
        case 4: b = new vec_obj();  b->kind = KIND_VEC;   break;
        case 5: b = new state_obj();b->kind = KIND_STATE; break;
        default: std::abort();
    }
    return to_h(b);
}

void fg_kv_destroy(int64_t h) {
    if (h == 0) { return; }
    base * b = as_obj(h);
    if (b->kind == KIND_KV) {
        kv_obj * kv = static_cast<kv_obj *>(b);
        if (kv->native != nullptr && fg_kv_native_forget != nullptr) { fg_kv_native_forget(kv->native); }
    }
    if (b->kind == KIND_STATE) {
        state_obj * st = static_cast<state_obj *>(b);
        for (int64_t snap : st->snaps) { destroy_cell_bundle(snap); }
        delete st;
        return;
    }
    delete b;
}

int64_t fg_kv_get(int64_t h, int64_t k, int64_t i) {
    base * b = as_obj(h);
    switch (b->kind) {
        case KIND_KV: {
            kv_obj * kv = static_cast<kv_obj *>(b);
            if (kv->native != nullptr && !(k >= 10 && k < 20)) {
                if (fg_kv_native_get == nullptr) { std::abort(); }
                return fg_kv_native_get(kv->native, k, i);
            }
            if (k == 0)  return kv->n_stream;
            if (k == 1)  return kv->n_seq_max;
            if (k == 2)  return kv->n_pad;
            if (k == 3)  return kv->n_swa;
            if (k == 4)  return kv->swa_type;
            if (k == 5)  return kv->arch_step35;
            if (k == 6)  return kv->n_pos_per_embd;
            if (k == 7)  return kv->n_layers;
            if (k == 8)  return kv->ple_n_heads;
            if (k == 9)  return kv->ple_image_tok;
            if (k == 60) return kv->ple_eos_tok;
            if (k == 61) return kv->rope_none;
            if (k == 62) return kv->has_other;
            if (k == 63) return kv->type_k;
            if (k == 64) return kv->type_v;
            if (k == 65) return (int64_t) kv->sc_src.size();
            if (k >= 200 && k < 300) { size_t x = (size_t)(k - 200); return x < kv->sc_src.size() ? kv->sc_src[x] : 0; }
            if (k >= 300 && k < 400) { size_t x = (size_t)(k - 300); return x < kv->sc_dst.size() ? kv->sc_dst[x] : 0; }
            if (k >= 400 && k < 500) { size_t x = (size_t)(k - 400); return x < kv->layer_has_v.size() ? kv->layer_has_v[x] : 0; }
            if (k >= 500 && k < 600) { size_t x = (size_t)(k - 500); return x < kv->layer_has_rope.size() ? kv->layer_has_rope[x] : 0; }
            if (k >= 10 && k < 20)  { size_t s = (size_t)(k - 10); return s < kv->cells.size() ? kv->cells[s] : 0; }
            if (k >= 100 && k < 200) { size_t s = (size_t)(k - 100); return s < kv->heads.size() ? kv->heads[s] : 0; }
            break;
        }
        case KIND_CTX: {
            ctx_obj * c = static_cast<ctx_obj *>(b);
            if (k == 0) return c->status;
            if (k == 1) return c->kv;
            if (k == 2) return c->lctx;
            if (k == 3) return c->i_cur;
            if (k == 4) return c->n_kv;
            if (k == 5) return c->do_shift;
            if (k == 6) return (int64_t) c->ubatches.size();
            if (k == 7) return (int64_t) c->sinfos.size();
            if (k == 8) return (int64_t) c->sc_src.size();
            if (k >= 100 && k < 200) { size_t x = (size_t)(k - 100); return x < c->ubatches.size() ? c->ubatches[x] : 0; }
            if (k >= 200 && k < 300) { size_t x = (size_t)(k - 200); return x < c->sinfos.size() ? c->sinfos[x] : 0; }
            if (k >= 300 && k < 400) { size_t x = (size_t)(k - 300); return x < c->sc_src.size() ? c->sc_src[x] : 0; }
            if (k >= 400 && k < 500) { size_t x = (size_t)(k - 400); return x < c->sc_dst.size() ? c->sc_dst[x] : 0; }
            break;
        }
        case KIND_SLOT: {
            slot_obj * s = static_cast<slot_obj *>(b);
            if (k == 0) return s->s0;
            if (k == 1) return s->s1;
            if (k == 2) return s->size_hint;
            if (k == 3) return s->n_stream;
            if (k >= 10 && k < 100) { size_t x = (size_t)(k - 10); return x < s->strm.size() ? s->strm[x] : 0; }
            if (k >= 100 && k < 200) {
                size_t x = (size_t)(k - 100);
                if (x < s->idxs.size() && i >= 0 && (size_t) i < s->idxs[x].size()) { return s->idxs[x][(size_t) i]; }
                return 0;
            }
            break;
        }
        case KIND_VEC: {
            vec_obj * v = static_cast<vec_obj *>(b);
            if (k == 0) return (int64_t) v->items.size();
            if (k >= 10) { size_t x = (size_t)(k - 10); return x < v->items.size() ? v->items[x] : 0; }
            break;
        }
        case KIND_STATE: {
            state_obj * st = static_cast<state_obj *>(b);
            if (k == 0) return st->sinfo;
            if (k >= 10 && k < 100) { size_t x = (size_t)(k - 10); return x < st->heads.size() ? st->heads[x] : 0; }
            if (k >= 100 && k < 200) { size_t x = (size_t)(k - 100); return x < st->snaps.size() ? st->snaps[x] : 0; }
            break;
        }
        default: break;
    }
    std::fprintf(stderr, "fg_kv_get: bad key kind=%u k=%lld\n", b->kind, (long long) k);
    std::abort();
}

void fg_kv_put(int64_t h, int64_t k, int64_t i, int64_t v) {
    base * b = as_obj(h);
    switch (b->kind) {
        case KIND_KV: {
            kv_obj * kv = static_cast<kv_obj *>(b);
            if (kv->native != nullptr && !(k >= 10 && k < 20)) {
                if (fg_kv_native_put == nullptr) { std::abort(); }
                fg_kv_native_put(kv->native, k, i, v);
                return;
            }
            if (k == 62) { kv->has_other = v; return; }
            if (k == 65) { kv->sc_src.resize((size_t) v); kv->sc_dst.resize((size_t) v); return; }
            if (k >= 200 && k < 300) { size_t x = (size_t)(k - 200); if (x >= kv->sc_src.size()) kv->sc_src.resize(x + 1); kv->sc_src[x] = v; return; }
            if (k >= 300 && k < 400) { size_t x = (size_t)(k - 300); if (x >= kv->sc_dst.size()) kv->sc_dst.resize(x + 1); kv->sc_dst[x] = v; return; }
            if (k >= 10 && k < 20)  { size_t s = (size_t)(k - 10); if (s >= kv->cells.size()) kv->cells.resize(s + 1); kv->cells[s] = v; return; }
            if (k >= 100 && k < 200) { size_t s = (size_t)(k - 100); if (s >= kv->heads.size()) kv->heads.resize(s + 1); kv->heads[s] = v; return; }
            if (k >= 400 && k < 500) { size_t x = (size_t)(k - 400); if (x >= kv->layer_has_v.size()) kv->layer_has_v.resize(x + 1); kv->layer_has_v[x] = (char) v; return; }
            if (k >= 500 && k < 600) { size_t x = (size_t)(k - 500); if (x >= kv->layer_has_rope.size()) kv->layer_has_rope.resize(x + 1); kv->layer_has_rope[x] = (char) v; return; }
            break;
        }
        case KIND_CTX: {
            ctx_obj * c = static_cast<ctx_obj *>(b);
            if (k == 0) { c->status = v; return; }
            if (k == 1) { c->kv = v; return; }
            if (k == 2) { c->lctx = v; return; }
            if (k == 3) { c->i_cur = v; return; }
            if (k == 4) { c->n_kv = v; return; }
            if (k == 5) { c->do_shift = v; return; }
            if (k == 6) { c->ubatches.resize((size_t) v); return; }
            if (k == 7) { c->sinfos.resize((size_t) v); return; }
            if (k == 8) { c->sc_src.resize((size_t) v); c->sc_dst.resize((size_t) v); return; }
            if (k >= 100 && k < 200) { size_t x = (size_t)(k - 100); if (x >= c->ubatches.size()) c->ubatches.resize(x + 1); c->ubatches[x] = v; return; }
            if (k >= 200 && k < 300) { size_t x = (size_t)(k - 200); if (x >= c->sinfos.size()) c->sinfos.resize(x + 1); c->sinfos[x] = v; return; }
            if (k >= 300 && k < 400) { size_t x = (size_t)(k - 300); if (x >= c->sc_src.size()) c->sc_src.resize(x + 1); c->sc_src[x] = v; return; }
            if (k >= 400 && k < 500) { size_t x = (size_t)(k - 400); if (x >= c->sc_dst.size()) c->sc_dst.resize(x + 1); c->sc_dst[x] = v; return; }
            break;
        }
        case KIND_SLOT: {
            slot_obj * s = static_cast<slot_obj *>(b);
            if (k == 0) { s->s0 = v; return; }
            if (k == 1) { s->s1 = v; return; }
            if (k == 2) { s->size_hint = v; return; }
            if (k == 3) { s->n_stream = v; if (s->strm.size() < (size_t) v) s->strm.resize((size_t) v, 0); if (s->idxs.size() < (size_t) v) s->idxs.resize((size_t) v); return; }
            if (k >= 10 && k < 100) { size_t x = (size_t)(k - 10); if (x >= s->strm.size()) s->strm.resize(x + 1); s->strm[x] = v; return; }
            if (k >= 100 && k < 200) { size_t x = (size_t)(k - 100); if (x >= s->idxs.size()) s->idxs.resize(x + 1); if (i >= 0) { if ((size_t) i >= s->idxs[x].size()) s->idxs[x].resize((size_t) i + 1); s->idxs[x][(size_t) i] = v; } return; }
            break;
        }
        case KIND_VEC: {
            vec_obj * vv = static_cast<vec_obj *>(b);
            if (k >= 10) { size_t x = (size_t)(k - 10); if (x >= vv->items.size()) vv->items.resize(x + 1); vv->items[x] = v; return; }
            break;
        }
        case KIND_STATE: {
            state_obj * st = static_cast<state_obj *>(b);
            if (k == 0) { st->sinfo = v; return; }
            if (k >= 10 && k < 100) { size_t x = (size_t)(k - 10); if (x >= st->heads.size()) st->heads.resize(x + 1); st->heads[x] = v; return; }
            if (k >= 100 && k < 200) { size_t x = (size_t)(k - 100); if (x >= st->snaps.size()) st->snaps.resize(x + 1); st->snaps[x] = v; return; }
            break;
        }
        default: break;
    }
    std::fprintf(stderr, "fg_kv_put: bad write kind=%u k=%lld\n", b->kind, (long long) k);
    std::abort();
}

int64_t fg_kv_service(int64_t op, int64_t a, int64_t b) {
    (void) a; (void) b;
    if (op == 0) { std::fprintf(stderr, "fg_kv: Goose invariant trap\n"); std::abort(); }
    if (op == 100) { return 0; }
    std::fprintf(stderr, "fg_kv_service: bad op %lld\n", (long long) op);
    std::abort();
}

void fg_kv_vec_push(int64_t vh, int64_t item) {
    base * b = as_obj(vh);
    if (b->kind != KIND_VEC) { std::abort(); }
    static_cast<vec_obj *>(b)->items.push_back(item);
}

void fg_kv_slot_push(int64_t sh, int64_t strm_idx, int64_t cell) {
    base * b = as_obj(sh);
    if (b->kind != KIND_SLOT) { std::abort(); }
    slot_obj * s = static_cast<slot_obj *>(b);
    if (strm_idx < 0) { std::abort(); }
    if ((size_t) strm_idx >= s->idxs.size()) { s->idxs.resize((size_t) strm_idx + 1); }
    s->idxs[(size_t) strm_idx].push_back(cell);
}

int64_t fg_kv_slot_len(int64_t sh, int64_t strm_idx) {
    base * b = as_obj(sh);
    if (b->kind != KIND_SLOT) { std::abort(); }
    slot_obj * s = static_cast<slot_obj *>(b);
    if (strm_idx < 0 || (size_t) strm_idx >= s->idxs.size()) { return 0; }
    return (int64_t) s->idxs[(size_t) strm_idx].size();
}

void fg_kv_slot_clear(int64_t sh, int64_t strm_idx) {
    base * b = as_obj(sh);
    if (b->kind != KIND_SLOT) { std::abort(); }
    slot_obj * s = static_cast<slot_obj *>(b);
    if (strm_idx >= 0 && (size_t) strm_idx < s->idxs.size()) { s->idxs[(size_t) strm_idx].clear(); }
}

// ---- ubatch access ----
int64_t fg_kv_ub_n_tokens(int64_t u) { return is_fake_ub(u) ? fake_of(u)->n_tokens : (int64_t) real_ub(u)->n_tokens; }
int64_t fg_kv_ub_n_seqs_unq(int64_t u) { return is_fake_ub(u) ? (int64_t) fake_of(u)->seq_id_unq.size() : (int64_t) real_ub(u)->n_seqs_unq; }
int64_t fg_kv_ub_seq_id_unq(int64_t u, int64_t s) {
    if (is_fake_ub(u)) { fake_ub * f = fake_of(u); return (s >= 0 && (size_t) s < f->seq_id_unq.size()) ? f->seq_id_unq[(size_t) s] : 0; }
    return real_ub(u)->seq_id_unq[s];
}
int64_t fg_kv_ub_pos(int64_t u, int64_t i) {
    if (is_fake_ub(u)) { fake_ub * f = fake_of(u); return (i >= 0 && (size_t) i < f->pos.size()) ? f->pos[(size_t) i] : 0; }
    return real_ub(u)->pos[i];
}
int64_t fg_kv_ub_n_seq_id(int64_t u, int64_t i) {
    if (is_fake_ub(u)) { fake_ub * f = fake_of(u); return (i >= 0 && (size_t) i < f->n_seq_id.size()) ? f->n_seq_id[(size_t) i] : 0; }
    return real_ub(u)->n_seq_id[i];
}
int64_t fg_kv_ub_seq_id(int64_t u, int64_t i, int64_t j) {
    if (is_fake_ub(u)) {
        fake_ub * f = fake_of(u);
        if (i < 0 || (size_t) i >= f->seq_id.size()) { return 0; }
        return (j >= 0 && (size_t) j < f->seq_id[(size_t) i].size()) ? f->seq_id[(size_t) i][(size_t) j] : 0;
    }
    return real_ub(u)->seq_id[i][j];
}
int64_t fg_kv_ub_token(int64_t u, int64_t i) {
    if (is_fake_ub(u)) { fake_ub * f = fake_of(u); return (i >= 0 && (size_t) i < f->token.size()) ? f->token[(size_t) i] : 0; }
    return real_ub(u)->token[i];
}
int64_t fg_kv_ub_has_token(int64_t u) { return is_fake_ub(u) ? (fake_of(u)->has_token ? 1 : 0) : (real_ub(u)->token != nullptr ? 1 : 0); }
int64_t fg_kv_ub_is_pos_2d(int64_t u) { return is_fake_ub(u) ? fake_of(u)->is2d : (real_ub(u)->is_pos_2d() ? 1 : 0); }
int64_t fg_kv_ub_is_mixed(int64_t u) { return is_fake_ub(u) ? fake_of(u)->mixed : (real_ub(u)->is_mixed() ? 1 : 0); }
int64_t fg_kv_ub_type(int64_t u, int64_t i) {
    if (is_fake_ub(u)) { fake_ub * f = fake_of(u); return (i >= 0 && (size_t) i < f->type.size()) ? f->type[(size_t) i] : 0; }
    return real_ub(u)->type[i];
}
void fg_kv_ub_destroy(int64_t u) {
    if (u == 0) { return; }
    if (is_fake_ub(u)) { g_fake_ub.erase((uintptr_t) u); delete fake_of(u); return; }
    delete real_ub(u);
}
void fg_kv_test_ubatch_free(int64_t u) { fg_kv_ub_destroy(u); }

// ---- balloc access ----
int64_t fg_kv_balloc_split(int64_t bh, int64_t n_ubatch, int64_t kind) {
    if (is_fake_balloc(bh)) {
        fake_balloc * b = reinterpret_cast<fake_balloc *>(static_cast<intptr_t>(bh));
        if (b->idx < b->ubs.size()) { return b->ubs[b->idx++]; }
        fake_ub * z = new fake_ub();
        g_fake_ub.insert((uintptr_t) z);
        return to_h(z);
    }
    llama_batch_allocr * b = reinterpret_cast<llama_batch_allocr *>(static_cast<intptr_t>(bh));
    if (kind == 0) { return to_h(new llama_ubatch(b->split_simple((uint32_t) n_ubatch))); }
    if (kind == 1) { return to_h(new llama_ubatch(b->split_seq((uint32_t) n_ubatch))); }
    return to_h(new llama_ubatch(b->split_equal((uint32_t) n_ubatch, true, 0)));
}
void fg_kv_balloc_reset(int64_t bh) { if (!is_fake_balloc(bh)) { reinterpret_cast<llama_batch_allocr *>(static_cast<intptr_t>(bh))->split_reset(); } }
int64_t fg_kv_balloc_n_used(int64_t bh) {
    if (is_fake_balloc(bh)) { return reinterpret_cast<fake_balloc *>(static_cast<intptr_t>(bh))->n_used; }
    return (int64_t) reinterpret_cast<llama_batch_allocr *>(static_cast<intptr_t>(bh))->get_n_used();
}
int64_t fg_kv_balloc_n_tokens(int64_t bh) {
    if (is_fake_balloc(bh)) { return reinterpret_cast<fake_balloc *>(static_cast<intptr_t>(bh))->n_tokens; }
    return (int64_t) reinterpret_cast<llama_batch_allocr *>(static_cast<intptr_t>(bh))->get_n_tokens();
}
void fg_kv_balloc_destroy(int64_t bh) {
    if (bh == 0) { return; }
    if (is_fake_balloc(bh)) { g_fake_balloc.erase((uintptr_t) bh); delete reinterpret_cast<fake_balloc *>(static_cast<intptr_t>(bh)); return; }
    delete reinterpret_cast<llama_batch_allocr *>(static_cast<intptr_t>(bh));
}

// ---- synthetic builders (selftest/replay only) ----
int64_t fg_kv_test_ubatch(int64_t n_tokens) {
    fake_ub * f = new fake_ub();
    f->n_tokens = n_tokens;
    f->pos.assign((size_t) n_tokens, 0);
    f->seq_id.resize((size_t) n_tokens);
    f->n_seq_id.assign((size_t) n_tokens, 0);
    f->token.assign((size_t) n_tokens, 0);
    f->type.assign((size_t) n_tokens, 0);
    g_fake_ub.insert((uintptr_t) f);
    return to_h(f);
}
void fg_kv_test_ub_set_pos(int64_t u, int64_t i, int64_t v) {
    if (!is_fake_ub(u) || i < 0) { std::abort(); }
    fake_ub * f = fake_of(u);
    if ((size_t) i >= f->pos.size()) { f->pos.resize((size_t) i + 1); }
    f->pos[(size_t) i] = v;
}
void fg_kv_test_ub_set_seq(int64_t u, int64_t i, int64_t j, int64_t seq) {
    if (!is_fake_ub(u) || i < 0 || j < 0) { std::abort(); }
    fake_ub * f = fake_of(u);
    if ((size_t) i >= f->seq_id.size()) { f->seq_id.resize((size_t) i + 1); f->n_seq_id.resize((size_t) i + 1, 0); }
    if ((size_t) j >= f->seq_id[(size_t) i].size()) { f->seq_id[(size_t) i].resize((size_t) j + 1); }
    f->seq_id[(size_t) i][(size_t) j] = seq;
    f->n_seq_id[(size_t) i] = j + 1;
}
void fg_kv_test_ub_set_seq_unq(int64_t u, int64_t s, int64_t seq) {
    if (!is_fake_ub(u) || s < 0) { std::abort(); }
    fake_ub * f = fake_of(u);
    if ((size_t) s >= f->seq_id_unq.size()) { f->seq_id_unq.resize((size_t) s + 1); }
    f->seq_id_unq[(size_t) s] = seq;
}
void fg_kv_test_ub_set_token(int64_t u, int64_t i, int64_t v) {
    if (!is_fake_ub(u) || i < 0) { std::abort(); }
    fake_ub * f = fake_of(u);
    if ((size_t) i >= f->token.size()) { f->token.resize((size_t) i + 1); }
    f->token[(size_t) i] = v;
    f->has_token = true;
}
int64_t fg_kv_test_balloc(int64_t u) {
    fake_balloc * b = new fake_balloc();
    b->ubs.push_back(u);
    b->n_tokens = fg_kv_ub_n_tokens(u);
    b->n_used = b->n_tokens;
    g_fake_balloc.insert((uintptr_t) b);
    return to_h(b);
}

// ---- backend/tensor callbacks, per cache instance ----
int64_t fg_kv_swa_masked(int64_t kvh, int64_t n_swa, int64_t swa_type, int64_t p0, int64_t p1) {
    cb_t & cb = static_cast<kv_obj *>(as_obj(kvh))->cb;
    if (cb.swa == nullptr) { std::fprintf(stderr, "fg_kv: swa callback not bound\n"); std::abort(); }
    return cb.swa(n_swa, swa_type, p0, p1);
}
void fg_kv_sync(int64_t kvh, int64_t lctx) {
    cb_t & cb = static_cast<kv_obj *>(as_obj(kvh))->cb;
    if (!cb.sync) { std::abort(); } cb.sync(lctx);
}
void fg_kv_tensor_copy(int64_t kv, int64_t src, int64_t dst, int64_t il, int64_t is_v) {
    cb_t & cb = static_cast<kv_obj *>(as_obj(kv))->cb;
    if (!cb.copy) { std::abort(); } cb.copy(kv, src, dst, il, is_v);
}
int64_t fg_kv_shift_compute(int64_t kv, int64_t lctx) {
    cb_t & cb = static_cast<kv_obj *>(as_obj(kv))->cb;
    if (!cb.shift) { std::abort(); } return cb.shift(kv, lctx);
}
void fg_kv_zero_cells(int64_t kv, int64_t strm, int64_t sinfo, int64_t contiguous) {
    cb_t & cb = static_cast<kv_obj *>(as_obj(kv))->cb;
    if (!cb.zero) { std::abort(); } cb.zero(kv, strm, sinfo, contiguous);
}
void fg_kv_clear_data(int64_t kv) {
    cb_t & cb = static_cast<kv_obj *>(as_obj(kv))->cb;
    if (!cb.clear) { std::abort(); } cb.clear(kv);
}
int64_t fg_kv_other_pos(int64_t kv, int64_t seq, int64_t is_max) {
    cb_t & cb = static_cast<kv_obj *>(as_obj(kv))->cb;
    if (!cb.other) { std::abort(); } return cb.other(kv, seq, is_max);
}

} // extern "C"
