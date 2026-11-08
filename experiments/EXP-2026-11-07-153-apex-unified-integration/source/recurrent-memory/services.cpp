// services.cpp - native field/storage/tensor API for the recurrent Goose modules.
// Proposal for Codex; not compiled here.
//
// Contains ONLY typed accessors and raw primitives. No seq_rm/seq_cp/seq_keep/
// find_slot/init_batch/apply/state_* decision is implemented here: all ordered
// control lives in the Goose modules. Container ops (has/add/remove/clear/size/
// first) are generic std::set membership primitives, not sequence algorithms.
#include "bridge.h"
#include "fg153-projection.h"

#include "llama-memory-recurrent.h"
#include "llama-batch.h"
#include "llama-io.h"
#include "llama-impl.h"
#include "llama-model.h"
#include "ggml.h"
#include "ggml-backend.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <functional>
#include <iterator>
#include <set>
#include <unordered_map>
#include <vector>

#define FG153_PROJ_MAGIC 0x5245435552524E54ull /* "RECURRNT" */
#define FG153_BOX_MAGIC  0x524543424F585953ull /* "RECBOXXS" */

struct fg153_box {
    uint64_t magic;
    std::unordered_map<uint64_t, int64_t> kv;
    explicit fg153_box(size_t reserve) : magic(FG153_BOX_MAGIC) { kv.reserve(reserve); }
    ~fg153_box() {
        for (auto & e : kv)
            if ((uint64_t)(uint32_t)(e.first >> 32) == 14u)
                delete (std::set<llama_seq_id> *)(intptr_t) e.second;
    }
};

static inline uint64_t fg153_boxkey(int64_t k, int64_t i) {
    return (uint64_t)(uint32_t)k << 32 | (uint32_t)i;
}

// ---------------------------------------------------------------- projection
int64_t fg153_recurrent_projection::get(int64_t k, int64_t i) const {
    switch (k) {
        case 0:  return mem->size;
        case 1:  return mem->used;
        case 2:  return mem->head;
        case 3:  return mem->n;
        case 4:  return mem->n_seq_max;
        case 5:  return mem->n_rs_seq;
        case 6:  return mem->rs_z;
        // raw storage read; never call the owned is_empty() accessor back
        case 7:  return empty_override >= 0 ? empty_override : (mem->ctxs_bufs.empty() ? 1 : 0);
        case 10: return i >= 0 && (size_t) i < mem->cells.size() ? mem->cells[i].pos  : -1;
        case 11: return i >= 0 && (size_t) i < mem->cells.size() ? mem->cells[i].src  : -1;
        case 12: return i >= 0 && (size_t) i < mem->cells.size() ? mem->cells[i].src0 : -1;
        case 13: return i >= 0 && (size_t) i < mem->cells.size() ? mem->cells[i].tail : -1;
        case 14: return i >= 0 && (size_t) i < mem->cells.size()
                        ? (int64_t)(intptr_t)&mem->cells[i].seq_id : 0;
        case 15: return i >= 0 && (size_t) i < mem->rs_idx.size() ? mem->rs_idx[i] : 0;
        case 70: return i >= 0 && (size_t) i < mem->r_l.size() ? (int64_t)(intptr_t)mem->r_l[i] : 0;
        case 71: return i >= 0 && (size_t) i < mem->s_l.size() ? (int64_t)(intptr_t)mem->s_l[i] : 0;
        case 72: return i >= 0 && (size_t) i < mem->p_l.size() ? (int64_t)(intptr_t)mem->p_l[i] : 0;
        case 80: return buffer_at(i);
        case 81: return buffer_buft_at(i);
        case 83: return buffer_ctx_at(i);
        case 84: return buffer_count();
        // read-only hparam / hook projections (friend access)
        case 90: return mem->hparams.n_layer();
        case 91: return mem->hparams.n_embd_r();
        case 92: return mem->hparams.n_embd_s();
        case 93: return mem->hparams.ple_conv_state();
        case 94: return mem->hparams.is_ple((int32_t) i) ? 1 : 0;
        case 95: {
            if (!filter) return 1;
            auto * fn = static_cast<const std::function<bool(int32_t)> *>(filter);
            return (*fn)((int32_t) i) ? 1 : 0;
        }
        case 96: return model ? (int64_t)(intptr_t)((const llama_model *) model)->dev_layer((int32_t) i) : 0;
        case 97: return (int64_t) mem->hparams.n_pos_per_embd();
        default: GGML_ABORT("bad recurrent projection read");
    }
}

void fg153_recurrent_projection::put(int64_t k, int64_t i, int64_t v) {
    switch (k) {
        case 0:  mem->size = (uint32_t) v; return;
        case 1:  mem->used = (uint32_t) v; return;
        case 2:  mem->head = (uint32_t) v; return;
        case 3:  mem->n    = (uint32_t) v; return;
        // n_seq_max is const on the patched class; allow only the identical value
        case 4:  GGML_ASSERT((uint32_t) v == mem->n_seq_max); return;
        case 5:  mem->n_rs_seq = (uint32_t) v; return;
        case 6:  mem->rs_z = (int32_t) v; return;
        case 7:  empty_override = (int32_t) v; return;
        case 10: GGML_ASSERT(i >= 0 && (size_t) i < mem->cells.size()); mem->cells[i].pos  = (llama_pos) v; return;
        case 11: GGML_ASSERT(i >= 0 && (size_t) i < mem->cells.size()); mem->cells[i].src  = (int32_t) v; return;
        case 12: GGML_ASSERT(i >= 0 && (size_t) i < mem->cells.size()); mem->cells[i].src0 = (int32_t) v; return;
        case 13: GGML_ASSERT(i >= 0 && (size_t) i < mem->cells.size()); mem->cells[i].tail = (int32_t) v; return;
        case 15: GGML_ASSERT(i >= 0 && (size_t) i < mem->rs_idx.size()); mem->rs_idx[i] = (uint32_t) v; return;
        case 70: GGML_ASSERT(i >= 0 && (size_t) i < mem->r_l.size()); mem->r_l[i] = (ggml_tensor *)(intptr_t) v; return;
        case 71: GGML_ASSERT(i >= 0 && (size_t) i < mem->s_l.size()); mem->s_l[i] = (ggml_tensor *)(intptr_t) v; return;
        case 72: GGML_ASSERT(i >= 0 && (size_t) i < mem->p_l.size()); mem->p_l[i] = (ggml_tensor *)(intptr_t) v; return;
        case 80: case 81: case 83: {
            GGML_ASSERT(!adopted && "buffer table writes allowed only pre-adopt");
            if (i < 0) GGML_ABORT("bad buffer index");
            if ((size_t) i >= side.size()) side.resize((size_t) i + 1, bufent{nullptr, nullptr, nullptr});
            if (k == 80) side[i].buf  = (ggml_backend_buffer *)(intptr_t) v;
            if (k == 81) side[i].buft = (ggml_backend_buffer_type *)(intptr_t) v;
            if (k == 83) side[i].ctx  = (ggml_context *)(intptr_t) v;
            return;
        }
        case 84: GGML_ASSERT(!adopted && v >= 0); side_count = v; side.resize((size_t) v); return;
        default: GGML_ABORT("bad recurrent projection write");
    }
}

int64_t fg153_recurrent_projection::buffer_count() const {
    return adopted ? (int64_t) mem->ctxs_bufs.size() : (int64_t) side.size();
}
int64_t fg153_recurrent_projection::buffer_at(int64_t k) const {
    if (k < 0 || k >= buffer_count()) return 0;
    return adopted ? (int64_t)(intptr_t) mem->ctxs_bufs[k].second.get()
                   : (int64_t)(intptr_t) side[k].buf;
}
int64_t fg153_recurrent_projection::buffer_ctx_at(int64_t k) const {
    if (k < 0 || k >= buffer_count()) return 0;
    return adopted ? (int64_t)(intptr_t) mem->ctxs_bufs[k].first.get()
                   : (int64_t)(intptr_t) side[k].ctx;
}
int64_t fg153_recurrent_projection::buffer_buft_at(int64_t k) const {
    if (k < 0 || k >= buffer_count()) return 0;
    if (adopted) return (int64_t)(intptr_t) ggml_backend_buffer_get_type(mem->ctxs_bufs[k].second.get());
    return (int64_t)(intptr_t) side[k].buft;
}
// Boundary: size aggregation (r/s/p/total, breakdown) is Goose-owned in
// recurrent-lifecycle.goose (entries 20..24). services.cpp exposes only
// per-tensor/per-buffer primitives (ops 19/24, keys 70..72/80..84); it does not
// provide owned aggregation fallbacks.

int fg153_recurrent_projection::adopt() {
    if (adopted) return 0;
    // 1) preflight: every side entry must own a buffer (no partial commit).
    for (auto & e : side) if (!e.buf) return -1;
    // 2) reserve so the commit below cannot reallocate/throw mid-transfer.
    try {
        mem->ctxs_bufs.reserve(mem->ctxs_bufs.size() + side.size());
    } catch (...) {
        return -2; // nothing moved; side still owns everything
    }
    // 3) commit: after each successful move the side entry is nulled, so
    //    close()/cleanup can never free an already-transferred buffer twice.
    for (auto & e : side) {
        mem->ctxs_bufs.emplace_back(ggml_context_ptr(e.ctx), ggml_backend_buffer_ptr(e.buf));
        e.ctx = nullptr;
        e.buf = nullptr;
    }
    side.clear();
    side_count = 0;
    adopted = 1;
    empty_override = -1;
    return 0;
}

fg153_recurrent_projection * fg153_recurrent_projection::open(
        llama_memory_recurrent * m, const void * filter, void * model) {
    auto * p = new fg153_recurrent_projection();
    p->magic = FG153_PROJ_MAGIC;
    p->mem = m;
    p->filter = filter;
    p->model = model;
    p->empty_override = -1;
    p->adopted = m->ctxs_bufs.empty() ? 0 : 1;
    return p;
}
void fg153_recurrent_projection::close(fg153_recurrent_projection * p) {
    if (!p) return;
    if (!p->adopted) {
        for (auto & e : p->side) {
            if (e.buf) ggml_backend_buffer_free(e.buf);
            if (e.ctx) ggml_free(e.ctx);
        }
    }
    delete p;
}

// ---------------------------------------------------------------- dispatch
static int64_t fg153_read_any(int64_t p, int64_t k, int64_t i) {
    if (!p) return 0;
    uint64_t magic = *(const uint64_t *)(intptr_t) p;
    if (magic == FG153_PROJ_MAGIC) {
        auto * pr = (fg153_recurrent_projection *)(intptr_t) p;
        pr->reads++;
        return pr->get(k, i);
    }
    if (magic == FG153_BOX_MAGIC) {
        auto * b = (fg153_box *)(intptr_t) p;
        uint64_t key = fg153_boxkey(k, i);
        auto it = b->kv.find(key);
        if (it != b->kv.end()) return it->second;
        // lazily back key 14 (cell seq set) for harness boxes so container ops work
        if (k == 14) {
            auto * s = new std::set<llama_seq_id>();
            b->kv[key] = (int64_t)(intptr_t) s;
            return (int64_t)(intptr_t) s;
        }
        return 0;
    }
    GGML_ABORT("bad Goose recurrent handle read");
}
static void fg153_write_any(int64_t p, int64_t k, int64_t i, int64_t v) {
    if (!p) GGML_ABORT("null Goose recurrent handle write");
    uint64_t magic = *(const uint64_t *)(intptr_t) p;
    if (magic == FG153_PROJ_MAGIC) {
        auto * pr = (fg153_recurrent_projection *)(intptr_t) p;
        pr->writes++;
        pr->put(k, i, v);
        return;
    }
    if (magic == FG153_BOX_MAGIC) {
        auto * b = (fg153_box *)(intptr_t) p;
        b->kv[fg153_boxkey(k, i)] = v;
        return;
    }
    GGML_ABORT("bad Goose recurrent handle write");
}

int fg153_recurrent_enabled(void) {
    const char * v = getenv("GGML_GOOSE_RECURRENT");
    return !v || strcmp(v, "0") != 0;
}

// cells module externs
int64_t fg153_rg(int64_t p, int64_t k, int64_t i) { return fg153_read_any(p, k, i); }
void    fg153_rw(int64_t p, int64_t k, int64_t i, int64_t v) { fg153_write_any(p, k, i, v); }
// lifecycle module externs (same dispatcher, distinct symbols)
int64_t fg153_lg(int64_t p, int64_t k, int64_t i) { return fg153_read_any(p, k, i); }
void    fg153_lp(int64_t p, int64_t k, int64_t i, int64_t v) { fg153_write_any(p, k, i, v); }
// state module externs
int64_t fg153_sg(int64_t p, int64_t k, int64_t i) { return fg153_read_any(p, k, i); }
void    fg153_sp(int64_t p, int64_t k, int64_t i, int64_t v) { fg153_write_any(p, k, i, v); }

// Race-free liveness: atomic per-family/per-mode counters, readable by parent.
// family 0 = cells (fg153_rc), 1 = lifecycle (fg153_lw), 2 = state (fg153_sw).
static std::atomic<uint64_t> g_witness[3][64];
static void fg153_witness_hit(int family, int64_t mode) {
    if (mode < 0 || mode >= 64) return;
    g_witness[family][mode].fetch_add(1, std::memory_order_relaxed);
}
void fg153_rc(int64_t mode) { fg153_witness_hit(0, mode); }
void fg153_lw(int64_t mode) { fg153_witness_hit(1, mode); }
void fg153_sw(int64_t mode) { fg153_witness_hit(2, mode); }
extern "C" uint64_t fg153_recurrent_witness(int64_t family, int64_t mode) {
    if (family < 0 || family > 2 || mode < 0 || mode >= 64) return 0;
    return g_witness[family][mode].load(std::memory_order_relaxed);
}
extern "C" void fg153_recurrent_witness_reset(void) {
    for (int f = 0; f < 3; ++f)
        for (int m = 0; m < 64; ++m)
            g_witness[f][m].store(0, std::memory_order_relaxed);
}

// cells container primitives (opaque std::set<llama_seq_id>*)
int64_t fg153_rs(int64_t op, int64_t a, int64_t b, int64_t c) {
    auto * s = (std::set<llama_seq_id> *)(intptr_t) a;
    switch (op) {
        case 0: GGML_ABORT("Goose recurrent invariant failed");
        case 1: return s->count((llama_seq_id) b) ? 1 : 0;
        case 2: s->insert((llama_seq_id) b); return 0;
        case 3: s->erase((llama_seq_id) b); return 0;
        case 4: s->clear(); return 0;
        case 5: return (int64_t) s->size();
        case 6: { // data_clear: raw buffer clear only, never control state
            auto * pr = (fg153_recurrent_projection *)(intptr_t) a;
            for (int64_t k = 0; k < pr->buffer_count(); ++k)
                ggml_backend_buffer_clear((ggml_backend_buffer *)(intptr_t) pr->buffer_at(k), 0);
            return 0;
        }
        case 7: { auto * d = (std::set<llama_seq_id> *)(intptr_t) b; *s = *d; return 0; }
        case 8: LLAMA_LOG_WARN("recurrent: non-consecutive token position %lld after %lld for sequence %lld\n",
                               (long long) b, (long long) c, (long long) a); return 0;
        case 9: LLAMA_LOG_ERROR("recurrent: invalid seq_id (%lld)\n", (long long) a); return 0;
        case 10: return s->empty() ? -1 : (int64_t) *s->begin();
        default: GGML_ABORT("bad Goose recurrent container primitive");
    }
}

// lifecycle primitives
int64_t fg153_ls(int64_t op, int64_t a, int64_t b, int64_t c) {
    auto proj = [](int64_t h) -> fg153_recurrent_projection * {
        return (fg153_recurrent_projection *)(intptr_t) h;
    };
    switch (op) {
        case 0: LLAMA_LOG_ERROR("recurrent lifecycle fail reason=%lld\n", (long long) a); GGML_ABORT("Goose recurrent lifecycle fail");
        case 1: return (int64_t)(intptr_t) new fg153_box((size_t)(a > 0 ? a : 8));
        case 2: delete (fg153_box *)(intptr_t) a; return 0;
        case 3: return proj(b)->get(90, 0);
        case 4: return proj(b)->get(91, 0);
        case 5: return proj(b)->get(92, 0);
        case 6: return proj(b)->get(93, 0);
        case 7: return proj(b)->get(94, a);
        case 8: return proj(b)->get(95, a);
        case 9: return proj(b)->get(96, a);
        case 10: return (int64_t)(intptr_t) ggml_backend_dev_buffer_type((ggml_backend_dev_t)(intptr_t) a);
        case 11: { const char * n = ggml_backend_dev_name((ggml_backend_dev_t)(intptr_t) a); return (int64_t)(intptr_t) n; }
        case 12: return (int64_t)(intptr_t) ggml_backend_cpu_buffer_type();
        case 13: return (int64_t)(intptr_t) ggml_backend_buft_name((ggml_backend_buffer_type_t)(intptr_t) a);
        case 14: return strcmp((const char *)(intptr_t) a, (const char *)(intptr_t) b) == 0 ? 1 : 0;
        case 15: {
            if (getenv("FG153_FORCE_CTX_FAIL")) return 0; // failure injection (fixture)
            auto * p = proj(b);
            int32_t n_layer = (int32_t) p->get(90, 0);
            uint32_t ple = (uint32_t) p->get(93, 0);
            ggml_init_params params = {
                (size_t)((ple > 0 ? 3u : 2u) * (uint32_t) n_layer * ggml_tensor_overhead()),
                nullptr, true,
            };
            return (int64_t)(intptr_t) ggml_init(params);
        }
        case 16: ggml_free((ggml_context *)(intptr_t) a); return 0;
        case 17: {
            auto * ctx = (ggml_context *)(intptr_t) a;
            int64_t type = fg153_read_any(b, 0, 0);
            int64_t ne0  = fg153_read_any(b, 1, 0);
            int64_t nrow = fg153_read_any(b, 2, 0);
            return (int64_t)(intptr_t) ggml_new_tensor_2d(ctx, (ggml_type) type, (int64_t) ne0, (int64_t) nrow);
        }
        case 18: {
            auto * t = (ggml_tensor *)(intptr_t) a;
            if (b == 1) ggml_format_name(t, "cache_r_l%d", (int) c);
            else if (b == 2) ggml_format_name(t, "cache_s_l%d", (int) c);
            else ggml_format_name(t, "cache_ple_r_l%d", (int) c);
            return 0;
        }
        case 19: if (!a) return 0; return (int64_t) ggml_nbytes((ggml_tensor *)(intptr_t) a);
        case 20: return (int64_t) ((ggml_tensor *)(intptr_t) a)->type;
        case 21: return (int64_t) ggml_row_size((ggml_type) a, (int64_t) b);
        case 22: if (getenv("FG153_FORCE_BUFFER_FAIL")) return 0; // failure injection (fixture)
                 return (int64_t)(intptr_t) ggml_backend_alloc_ctx_tensors_from_buft(
                         (ggml_context *)(intptr_t) a, (ggml_backend_buffer_type_t)(intptr_t) b);
        case 23: ggml_backend_buffer_clear((ggml_backend_buffer_t)(intptr_t) a, 0); return 0;
        case 24: return (int64_t) ggml_backend_buffer_get_size((ggml_backend_buffer_t)(intptr_t) a);
        case 25: return (int64_t)(intptr_t) ggml_backend_buffer_get_type((ggml_backend_buffer_t)(intptr_t) a);
        case 26: return (int64_t)(intptr_t) ggml_backend_buffer_name((ggml_backend_buffer_t)(intptr_t) a);
        case 27: ggml_backend_buffer_free((ggml_backend_buffer_t)(intptr_t) a); return 0;
        case 28: /* log sink */ (void) a; (void) b; (void) c; return 0;
        case 29: /* debug sink */ (void) a; (void) b; return 0;
        case 30: ((llama_batch_allocr *)(intptr_t) a)->split_reset(); return 0;
        case 31: {
            auto * ba = (llama_batch_allocr *)(intptr_t) a;
            return (int64_t)(intptr_t) new llama_ubatch(std::move(ba->split_seq((uint32_t) b)));
        }
        case 32: {
            auto * ba = (llama_batch_allocr *)(intptr_t) a;
            int64_t n = fg153_read_any(b, 0, 0);
            int64_t eq = fg153_read_any(b, 1, 0);
            int64_t tail = fg153_read_any(b, 2, 0);
            return (int64_t)(intptr_t) new llama_ubatch(
                std::move(ba->split_equal((uint32_t) n, eq != 0, (uint32_t) tail)));
        }
        case 33: return (int64_t) ((llama_batch_allocr *)(intptr_t) a)->get_n_used();
        case 34: return (int64_t) ((llama_batch_allocr *)(intptr_t) a)->get_n_tokens();
        case 35: return (int64_t) ((llama_ubatch *)(intptr_t) a)->n_tokens;
        case 36: return (int64_t) ((llama_ubatch *)(intptr_t) a)->n_seq_tokens;
        case 37: return (int64_t) ((llama_ubatch *)(intptr_t) a)->n_seqs;
        case 38: { auto * u = (llama_ubatch *)(intptr_t) a;
                   return (int64_t) u->seq_id[(size_t) b * u->n_seq_tokens][0]; }
        case 39: { auto * u = (llama_ubatch *)(intptr_t) a;
                   return (int64_t) u->pos[(size_t) b * u->n_seq_tokens + u->n_seq_tokens - 1]; }
        case 40: delete (llama_ubatch *)(intptr_t) a; return 0;
        case 41: if (getenv("FG153_FORCE_ADOPT_FAIL")) return -2; // failure injection (fixture)
                 return proj(a)->adopt();
        case 42: return proj(a)->adopted ? 1 : 0;
        default: GGML_ABORT("bad Goose recurrent lifecycle primitive");
    }
}

// state primitives (raw IO / tensor byte transfer / hparams / logging)
int64_t fg153_ss(int64_t op, int64_t a, int64_t b, int64_t c) {
    auto proj = [](int64_t h) -> fg153_recurrent_projection * {
        return (fg153_recurrent_projection *)(intptr_t) h;
    };
    auto iocfg = [](int64_t cfg, int64_t * off, int64_t * size) {
        *off = fg153_read_any(cfg, 0, 0);
        *size = fg153_read_any(cfg, 1, 0);
    };
    switch (op) {
        case 0: GGML_ABORT("Goose recurrent state invariant failed");
        case 1: { uint32_t v = (uint32_t) b; ((llama_io_write_i *)(intptr_t) a)->write(&v, sizeof(v)); return 0; }
        case 2: { uint64_t v = (uint64_t) b; ((llama_io_write_i *)(intptr_t) a)->write(&v, sizeof(v)); return 0; }
        case 3: { int32_t  v = (int32_t)  b; ((llama_io_write_i *)(intptr_t) a)->write(&v, sizeof(v)); return 0; }
        case 4: { int64_t off, size; iocfg(c, &off, &size);
                  ((llama_io_write_i *)(intptr_t) a)->write_tensor((ggml_tensor *)(intptr_t) b, (size_t) off, (size_t) size); return 0; }
        case 5: { uint32_t v = 0; ((llama_io_read_i *)(intptr_t) a)->read(&v, sizeof(v)); return (int64_t) v; }
        case 6: { uint64_t v = 0; ((llama_io_read_i *)(intptr_t) a)->read(&v, sizeof(v)); return (int64_t) v; }
        case 7: { int32_t  v = 0; ((llama_io_read_i *)(intptr_t) a)->read(&v, sizeof(v)); return (int64_t) v; }
        case 8: { int64_t off, size; iocfg(c, &off, &size);
                  ((llama_io_read_i *)(intptr_t) a)->read_tensor((ggml_tensor *)(intptr_t) b, (size_t) off, (size_t) size); return 0; }
        case 9: { int64_t off, size; iocfg(b, &off, &size);
                  llama_clear_tensor_data((ggml_tensor *)(intptr_t) a, (size_t) off, (size_t) size); return 0; }
        case 10: return proj(a)->get(90, 0);
        case 11: return proj(a)->get(91, 0);
        case 12: return proj(a)->get(92, 0);
        case 13: return proj(a)->get(93, 0);
        case 14: return proj(a)->get(97, 0);
        case 15: return proj(a)->get(b == 0 ? 70 : (b == 1 ? 71 : 72), c);
        case 16: return (int64_t) ggml_row_size(((ggml_tensor *)(intptr_t) a)->type, (int64_t) b);
        case 17: return (int64_t) ((ggml_tensor *)(intptr_t) a)->type;
        case 18: return (int64_t) ggml_nbytes((ggml_tensor *)(intptr_t) a);
        // failure event sink: native state_read throws; Goose returns 0 and the
        // parent maps this logged event to an exception at the boundary.
        case 19: LLAMA_LOG_ERROR("recurrent state failure reason=%lld\n", (long long) a); return 0;
        case 20: LLAMA_LOG_ERROR("recurrent state error kind=%lld a=%lld b=%lld\n", (long long) a, (long long) b, (long long) c); return 0;
        case 21: LLAMA_LOG_WARN("recurrent state warn kind=%lld a=%lld b=%lld\n", (long long) a, (long long) b, (long long) c); return 0;
        case 22: return (a & LLAMA_STATE_SEQ_FLAGS_ON_DEVICE) ? 1 : 0;
        case 24: return (int64_t)(intptr_t) new fg153_box((size_t)(a > 0 ? a : 8));
        case 25: delete (fg153_box *)(intptr_t) a; return 0;
        // generic opaque seq-set primitives (std::set<llama_seq_id>*)
        case 26: return (int64_t) ((std::set<llama_seq_id> *)(intptr_t) a)->size();
        case 27: {
            auto * s = (std::set<llama_seq_id> *)(intptr_t) a;
            if (b < 0 || (size_t) b >= s->size()) return -1;
            auto it = s->begin(); std::advance(it, (size_t) b); return (int64_t) *it;
        }
        case 28: ((std::set<llama_seq_id> *)(intptr_t) a)->insert((llama_seq_id) b); return 0;
        case 29: ((std::set<llama_seq_id> *)(intptr_t) a)->clear(); return 0;
        default: GGML_ABORT("bad Goose recurrent state primitive");
    }
}

// Goose-to-Goose seams: bound by the integrator to the emitted cells entry.
// fg153_lc maps to cells entry modes {0 create,10 find_slot,11 prepare,12 s_copy,13 can_shift}.
// fg153_sc maps to cells entry modes {1 clear,2 seq_rm(fixed -1,-1),9 set_rs_idx,
//                                    10 find_slot,18 seq_has,19 seq_size(via mode 25)}.
extern "C" __attribute__((weak)) int64_t fg153_lc(int64_t, int64_t, int64_t, int64_t) {
    GGML_ABORT("fg153_lc must be bound to recurrent-cells entry");
    return 0;
}
extern "C" __attribute__((weak)) int64_t fg153_sc(int64_t, int64_t, int64_t, int64_t) {
    GGML_ABORT("fg153_sc must be bound to recurrent-cells entry");
    return 0;
}

// harness helpers
extern "C" int64_t fg153_box_new(int64_t bytes) {
    return (int64_t)(intptr_t) new fg153_box((size_t)(bytes > 0 ? bytes : 8));
}
extern "C" void fg153_box_free(int64_t h) { delete (fg153_box *)(intptr_t) h; }
// C++ linkage: bridge.h declares these without extern "C" (parent-reported fix).
fg153_recurrent_projection * fg153_projection_open(
        llama_memory_recurrent * m, const void * filter, void * model) {
    return fg153_recurrent_projection::open(m, filter, model);
}
void fg153_projection_close(fg153_recurrent_projection * p) {
    fg153_recurrent_projection::close(p);
}
