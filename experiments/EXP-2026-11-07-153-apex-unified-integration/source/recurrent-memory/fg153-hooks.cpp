// fg153-hooks.cpp - actual Goose hooks + rollback gate for recurrent memory and
// its REAL concrete context. Proposal for Codex; compiled by the parent only.
//
// Registration model:
//   * fg153_hooks_register(m) opens a projection and stores a shared_ptr. The
//     normal llama_memory_recurrent constructor calls it at the end (production
//     path is Goose-controlled by default; rollback = FG153_GOOSE_HOOKS=0).
//   * The out-of-line destructor calls fg153_hooks_forget(m), so unregister is
//     guaranteed (no dangling registry / UAF, no manual caller burden).
//   * Projection lifetime is held by shared_ptr through every hook call and by
//     every live context, so concurrent forget cannot free it mid-call.
//   * Contexts are the real llama_memory_recurrent_context (concrete ABI kept);
//     only its methods are hooked. hybrid::get_recr static_cast and the
//     non-virtual getters therefore remain valid.
#include "fg153-hooks.h"
#include "fg153-projection.h"
#include "bridge.h"

#include "ggml.h"
#include "ggml-backend.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>
#include <vector>

using proj_ptr = std::shared_ptr<fg153_recurrent_projection>;

// Friend projection into the private concrete-context state (declared friend in
// the patched header). Used only to read real ctor state; no layout change.
struct fg153_context_access {
    using ctx_t = llama_memory_recurrent_context;
    static llama_memory_recurrent * mem(const ctx_t * c) { return c->mem; }
    static llama_memory_status status(const ctx_t * c) { return c->status; }
    static bool is_full(const ctx_t * c) { return c->is_full; }
    static const std::vector<llama_ubatch> & ubs(const ctx_t * c) { return c->ubatches; }
};

struct fg153_ctx_entry {
    int64_t goose = 0;
    proj_ptr proj;
};

static std::mutex g_mu;
static std::unordered_map<const llama_memory_recurrent *, proj_ptr> g_mem;
static std::unordered_map<const llama_memory_recurrent_context *, fg153_ctx_entry> g_ctx;

static bool fg153_hooks_enabled() {
    const char * v = getenv("FG153_GOOSE_HOOKS");
    return !v || strcmp(v, "0") != 0; // rollback: FG153_GOOSE_HOOKS=0
}

static proj_ptr fg153_make_projection(llama_memory_recurrent * m,
                                      const void * filter = nullptr, void * model = nullptr) {
    return proj_ptr(fg153_recurrent_projection::open(m, filter, model),
                    [](fg153_recurrent_projection * p) { fg153_recurrent_projection::close(p); });
}

static proj_ptr acquire_mem(const llama_memory_recurrent * m) {
    if (!fg153_hooks_enabled()) return nullptr;
    std::lock_guard<std::mutex> lk(g_mu);
    auto it = g_mem.find(m);
    return it == g_mem.end() ? nullptr : it->second;
}

bool fg153_hooks_active(const llama_memory_recurrent * m) {
    return acquire_mem(m) != nullptr;
}

static void register_with(llama_memory_recurrent * m, const void * filter, void * model) {
    if (!fg153_hooks_enabled() || !m) return;
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_mem.count(m)) return;
    g_mem.emplace(m, fg153_make_projection(m, filter, model));
}

void fg153_hooks_register(llama_memory_recurrent * m) { register_with(m, nullptr, nullptr); }

bool fg153_hooks_ctor_enabled() { return fg153_hooks_enabled(); }

// Production constructor path: register with real filter/model, run Goose
// create_owned (allocation ownership), then detach ctor-only metadata so the
// projection never keeps dangling pointers to a temporary filter.
int64_t fg153_hooks_ctor_alloc(llama_memory_recurrent * m, const void * filter, void * model,
                               int type_r, int type_s, int offload,
                               uint32_t mem_size, uint32_t n_seq_max, uint32_t n_rs_seq) {
    register_with(m, filter, model);
    auto pr = acquire_mem(m);
    if (!pr) return -1;
    int64_t h = (int64_t)(intptr_t) pr.get();
    int64_t cfg = fg153_box_new(64);
    if (!cfg) return -2;
    fg153_lp(cfg, 0, 0, mem_size);
    fg153_lp(cfg, 1, 0, n_seq_max);
    fg153_lp(cfg, 2, 0, n_rs_seq);
    fg153_lp(cfg, 3, 0, offload ? 1 : 0);
    fg153_lp(cfg, 4, 0, type_r);
    fg153_lp(cfg, 5, 0, type_s);
    int64_t rc = fg153_goose_lifecycle_entry(1, h, cfg, 0, 0, 0);
    fg153_box_free(cfg);
    {
        std::lock_guard<std::mutex> lk(g_mu);
        auto it = g_mem.find(m);
        if (it != g_mem.end()) { it->second->filter = nullptr; it->second->model = nullptr; }
    }
    return rc;
}

void fg153_hooks_forget(const llama_memory_recurrent * m) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_mem.erase(m); // shared_ptr kept alive by every live context
}

int64_t fg153_hooks_live_count() {
    std::lock_guard<std::mutex> lk(g_mu);
    return (int64_t)(g_mem.size() + g_ctx.size());
}

void fg153_fixture_seed(llama_memory_recurrent * m) {
    if (!m) return;
    ggml_init_params params = { (size_t) 1 << 20, nullptr, true }; // parent fix: no_alloc
    ggml_context * ctx = ggml_init(params);
    if (!ctx) return;
    ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 1);
    // ggml_backend_alloc_ctx_tensors takes a backend; we want the buft API
    ggml_backend_buffer_t buf = ggml_backend_alloc_ctx_tensors_from_buft(
            ctx, ggml_backend_cpu_buffer_type());
    if (!buf) { ggml_free(ctx); return; }
    auto * pr = fg153_recurrent_projection::open(m, nullptr, nullptr);
    int64_t h = (int64_t)(intptr_t) pr;
    fg153_lp(h, 80, 0, (int64_t)(intptr_t) buf);
    fg153_lp(h, 81, 0, (int64_t)(intptr_t) ggml_backend_buffer_get_type(buf));
    fg153_lp(h, 83, 0, (int64_t)(intptr_t) ctx);
    fg153_lp(h, 84, 0, 1);
    if (pr->adopt() != 0) fg153_recurrent_projection::close(pr);
    else fg153_recurrent_projection::close(pr); // adopted: buffers now owned by the class
}

// ---- memory hooks ----------------------------------------------------------
void fg153_hook_clear(llama_memory_recurrent * m, bool data) {
    if (auto pr = acquire_mem(m))
        fg153_goose_cells_entry(1, (int64_t)(intptr_t) pr.get(), data ? 1 : 0, 0, 0, 0);
}
bool fg153_hook_seq_rm(llama_memory_recurrent * m, llama_seq_id seq_id, llama_pos p0, llama_pos p1) {
    auto pr = acquire_mem(m);
    if (!pr) return false;
    return fg153_goose_cells_entry(2, (int64_t)(intptr_t) pr.get(), seq_id, p0, p1, 0) != 0;
}
void fg153_hook_seq_cp(llama_memory_recurrent * m, llama_seq_id src, llama_seq_id dst, llama_pos p0, llama_pos p1) {
    if (auto pr = acquire_mem(m))
        fg153_goose_cells_entry(3, (int64_t)(intptr_t) pr.get(), src, dst, p0, p1);
}
void fg153_hook_seq_keep(llama_memory_recurrent * m, llama_seq_id seq_id) {
    if (auto pr = acquire_mem(m))
        fg153_goose_cells_entry(4, (int64_t)(intptr_t) pr.get(), seq_id, 0, 0, 0);
}
void fg153_hook_seq_add(llama_memory_recurrent * m, llama_seq_id seq_id, llama_pos p0, llama_pos p1, llama_pos shift) {
    if (auto pr = acquire_mem(m))
        fg153_goose_cells_entry(5, (int64_t)(intptr_t) pr.get(), seq_id, p0, p1, shift);
}
void fg153_hook_seq_div(llama_memory_recurrent * m, llama_seq_id seq_id, llama_pos p0, llama_pos p1, int d) {
    if (auto pr = acquire_mem(m))
        fg153_goose_cells_entry(6, (int64_t)(intptr_t) pr.get(), seq_id, p0, p1, d);
}
llama_pos fg153_hook_seq_pos_min(const llama_memory_recurrent * m, llama_seq_id seq_id) {
    auto pr = acquire_mem(m);
    return pr ? (llama_pos) fg153_goose_cells_entry(7, (int64_t)(intptr_t) pr.get(), seq_id, 0, 0, 0) : -1;
}
llama_pos fg153_hook_seq_pos_max(const llama_memory_recurrent * m, llama_seq_id seq_id) {
    auto pr = acquire_mem(m);
    return pr ? (llama_pos) fg153_goose_cells_entry(8, (int64_t)(intptr_t) pr.get(), seq_id, 0, 0, 0) : -1;
}
bool fg153_hook_get_can_shift(const llama_memory_recurrent * m) {
    auto pr = acquire_mem(m);
    return pr && fg153_goose_cells_entry(13, (int64_t)(intptr_t) pr.get(), 0, 0, 0, 0) != 0;
}

// Borrowed descriptor from a borrowed ubatch: cells expects keys 20..23.
static int64_t fg153_desc_from_ubatch(const llama_ubatch & u) {
    int64_t d = fg153_box_new(64);
    if (!d) return 0;
    int64_t ub = (int64_t)(intptr_t) &u;
    int64_t n_seqs = fg153_ls(37, ub, 0, 0);
    int64_t n_tok  = fg153_ls(36, ub, 0, 0);
    fg153_lp(d, 20, 0, n_seqs);
    fg153_lp(d, 21, 0, n_tok);
    for (int64_t s = 0; s < n_seqs; ++s) {
        fg153_lp(d, 22, s, fg153_ls(38, ub, s, 0));
        fg153_lp(d, 23, s, fg153_ls(39, ub, s, 0));
    }
    return d;
}

// hybrid::init_batch calls mem->prepare(ubatches) directly; route to the cells
// prepare entry (snapshot + find_slot per ubatch + restore). Borrowed inputs.
bool fg153_hook_prepare(llama_memory_recurrent * m, const std::vector<llama_ubatch> & ubatches) {
    auto pr = acquire_mem(m);
    if (!pr) return false;
    int64_t h = (int64_t)(intptr_t) pr.get();
    int64_t n = (int64_t) ubatches.size();
    int64_t batch = fg153_box_new(128);
    if (!batch) return false;
    fg153_lp(batch, 24, 0, n);
    for (int64_t k = 0; k < n; ++k) {
        int64_t d = fg153_desc_from_ubatch(ubatches[(size_t) k]);
        if (!d) {
            for (int64_t j = 0; j < k; ++j) fg153_box_free(fg153_lg(batch, 25, j));
            fg153_box_free(batch);
            return false;
        }
        fg153_lp(batch, 25, k, d);
    }
    int64_t rc = fg153_goose_cells_entry(11, h, batch, 0, 0, 0);
    for (int64_t k = 0; k < n; ++k) fg153_box_free(fg153_lg(batch, 25, k));
    fg153_box_free(batch);
    return rc != 0;
}

// Public find_slot(const llama_ubatch&) can also be called directly.
bool fg153_hook_find_slot(llama_memory_recurrent * m, const llama_ubatch & ubatch) {
    auto pr = acquire_mem(m);
    if (!pr) return false;
    int64_t d = fg153_desc_from_ubatch(ubatch);
    if (!d) return false;
    int64_t rc = fg153_goose_cells_entry(10, (int64_t)(intptr_t) pr.get(), d, 0, 0, 0);
    fg153_box_free(d);
    return rc != 0;
}

// ---- real concrete context construction -----------------------------------
static void ctx_register(llama_memory_recurrent_context * c, int64_t goose, const proj_ptr & pr) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_ctx[c] = fg153_ctx_entry{goose, pr};
}

static std::unique_ptr<llama_memory_context_i> wrap_ctx(llama_memory_recurrent_context * c,
                                                        int64_t goose, const proj_ptr & pr) {
    if (!c) return nullptr;
    if (!goose) { delete c; return nullptr; }
    ctx_register(c, goose, pr);
    return std::unique_ptr<llama_memory_context_i>(c);
}

std::unique_ptr<llama_memory_context_i> fg153_hook_init_batch(
        llama_memory_recurrent * m, llama_batch_allocr & balloc, uint32_t n_ubatch, bool embd_all) {
    auto pr = acquire_mem(m);
    if (!pr) return nullptr;
    int64_t g = fg153_goose_lifecycle_entry(3, (int64_t)(intptr_t) pr.get(),
                                            (int64_t)(intptr_t) &balloc, n_ubatch, embd_all ? 1 : 0, 0);
    return wrap_ctx(new llama_memory_recurrent_context(m, std::vector<llama_ubatch>{}), g, pr);
}
std::unique_ptr<llama_memory_context_i> fg153_hook_init_full(llama_memory_recurrent * m) {
    auto pr = acquire_mem(m);
    if (!pr) return nullptr;
    int64_t g = fg153_goose_lifecycle_entry(4, (int64_t)(intptr_t) pr.get(), 0, 0, 0, 0);
    return wrap_ctx(new llama_memory_recurrent_context(m), g, pr);
}
std::unique_ptr<llama_memory_context_i> fg153_hook_init_update(
        llama_memory_recurrent * m, llama_context *, bool) {
    auto pr = acquire_mem(m);
    if (!pr) return nullptr;
    int64_t g = fg153_goose_lifecycle_entry(5, (int64_t)(intptr_t) pr.get(), 0, 0, 0, 0);
    return wrap_ctx(new llama_memory_recurrent_context(LLAMA_MEMORY_STATUS_NO_UPDATE), g, pr);
}

// ---- context hooks ---------------------------------------------------------
// Lazily connect ANY real concrete ctor path (mem, mem+ubatches, status) to a
// Goose control handle exactly once. init_* hooks register explicitly, so this
// only runs for direct `new llama_memory_recurrent_context(...)` (hybrid path).
static int64_t ensure_ctx(const llama_memory_recurrent_context * c) {
    {
        std::lock_guard<std::mutex> lk(g_mu);
        auto it = g_ctx.find(c);
        if (it != g_ctx.end()) return it->second.goose;
    }
    if (!fg153_hooks_enabled()) return 0;

    const llama_memory_status st = fg153_context_access::status(c);
    int64_t g = 0;
    proj_ptr pr = nullptr;
    if (st == LLAMA_MEMORY_STATUS_SUCCESS) {
        llama_memory_recurrent * m = fg153_context_access::mem(c); // valid for SUCCESS
        pr = acquire_mem(m);
        if (!pr) return 0; // memory not Goose-managed -> stay native (rollback)
        const int64_t mh = (int64_t)(intptr_t) pr.get();
        if (fg153_context_access::is_full(c)) {
            g = fg153_goose_lifecycle_entry(4, mh, 0, 0, 0, 0);
        } else {
            const auto & ubs = fg153_context_access::ubs(c);
            int64_t box = fg153_box_new(64);
            if (!box) return 0;
            int64_t idx = 0;
            for (const auto & u : ubs) {
                auto * cp = new llama_ubatch(u);
                fg153_lp(box, 10, idx, (int64_t)(intptr_t) cp);
                idx++;
            }
            fg153_lp(box, 11, 0, idx);
            g = fg153_goose_lifecycle_entry(32, mh, box, 0, 0, 0);
            if (!g) {
                for (int64_t k = 0; k < idx; ++k)
                    delete (llama_ubatch *)(intptr_t) fg153_lg(box, 10, k);
                fg153_box_free(box);
                return 0;
            }
        }
    } else {
        // NO_UPDATE / FAILED_*: native leaves mem uninitialised; never read it.
        g = fg153_goose_lifecycle_entry(33, (int64_t) st, 0, 0, 0, 0);
    }
    if (!g) return 0;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        auto it = g_ctx.find(c);
        if (it != g_ctx.end()) { // lost a race: keep the first registration
            fg153_goose_lifecycle_entry(29, g, 0, 0, 0, 0);
            return it->second.goose;
        }
        g_ctx[c] = fg153_ctx_entry{g, pr};
    }
    return g;
}

static bool ctx_acquire(const llama_memory_recurrent_context * c, int64_t * goose, proj_ptr * pr) {
    int64_t g = ensure_ctx(c);
    if (!g) return false;
    *goose = g;
    std::lock_guard<std::mutex> lk(g_mu);
    auto it = g_ctx.find(c);
    if (it != g_ctx.end()) *pr = it->second.proj;
    return true;
}

bool fg153_context_active(const llama_memory_recurrent_context * c) {
    return ensure_ctx(c) != 0;
}
int64_t fg153_context_hook_next(const llama_memory_recurrent_context * c) {
    int64_t g; proj_ptr pr;
    if (!ctx_acquire(c, &g, &pr)) return 0;
    return fg153_goose_lifecycle_entry(6, g, 0, 0, 0, 0);
}
int64_t fg153_context_hook_apply(const llama_memory_recurrent_context * c) {
    int64_t g; proj_ptr pr;
    if (!ctx_acquire(c, &g, &pr)) return 0;
    return fg153_goose_lifecycle_entry(7, g, 0, 0, 0, 0);
}
int64_t fg153_context_hook_status(const llama_memory_recurrent_context * c) {
    int64_t g; proj_ptr pr;
    if (!ctx_acquire(c, &g, &pr)) return -1;
    return fg153_goose_lifecycle_entry(8, g, 0, 0, 0, 0);
}
int64_t fg153_context_hook_ubatch(const llama_memory_recurrent_context * c) {
    int64_t g; proj_ptr pr;
    if (!ctx_acquire(c, &g, &pr)) return 0;
    return fg153_goose_lifecycle_entry(9, g, 0, 0, 0, 0);
}
int64_t fg153_context_hook_n_rs(const llama_memory_recurrent_context * c) {
    int64_t g; proj_ptr pr;
    if (!ctx_acquire(c, &g, &pr)) return 0;
    return fg153_goose_lifecycle_entry(10, g, 0, 0, 0, 0);
}
int64_t fg153_context_hook_head(const llama_memory_recurrent_context * c) {
    int64_t g; proj_ptr pr;
    if (!ctx_acquire(c, &g, &pr)) return 0;
    return fg153_goose_lifecycle_entry(11, g, 0, 0, 0, 0);
}
int64_t fg153_context_hook_rs_z(const llama_memory_recurrent_context * c) {
    int64_t g; proj_ptr pr;
    if (!ctx_acquire(c, &g, &pr)) return -1;
    return fg153_goose_lifecycle_entry(12, g, 0, 0, 0, 0);
}
int64_t fg153_context_hook_size(const llama_memory_recurrent_context * c) {
    int64_t g; proj_ptr pr;
    if (!ctx_acquire(c, &g, &pr)) return 0;
    return fg153_goose_lifecycle_entry(13, g, 0, 0, 0, 0);
}
int64_t fg153_context_hook_tensor(const llama_memory_recurrent_context * c, int64_t which, int64_t il) {
    int64_t g; proj_ptr pr;
    if (!ctx_acquire(c, &g, &pr)) return 0;
    return fg153_goose_lifecycle_entry(14 + which, g, il, 0, 0, 0);
}
int64_t fg153_context_hook_s_copy(const llama_memory_recurrent_context * c, int64_t i) {
    int64_t g; proj_ptr pr;
    if (!ctx_acquire(c, &g, &pr)) return 0;
    return fg153_goose_cells_entry(12, (int64_t)(intptr_t) pr.get(), i, 0, 0, 0);
}

void fg153_context_release(const llama_memory_recurrent_context * c) {
    int64_t g = 0;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        auto it = g_ctx.find(c);
        if (it != g_ctx.end()) { g = it->second.goose; g_ctx.erase(it); }
    }
    if (g) fg153_goose_lifecycle_entry(29, g, 0, 0, 0, 0);
}

void fg153_hook_state_write(const llama_memory_recurrent * m, llama_io_write_i & io,
                            llama_seq_id seq_id, llama_state_seq_flags flags) {
    if (auto pr = acquire_mem(m))
        fg153_goose_state_entry(0, (int64_t)(intptr_t) pr.get(),
                                (int64_t)(intptr_t) &io, seq_id, (int64_t) flags, 0);
}
void fg153_hook_state_read(llama_memory_recurrent * m, llama_io_read_i & io,
                           llama_seq_id seq_id, llama_state_seq_flags flags) {
    auto pr = acquire_mem(m);
    if (!pr) return;
    int64_t ok = fg153_goose_state_entry(1, (int64_t)(intptr_t) pr.get(),
                                         (int64_t)(intptr_t) &io, seq_id, (int64_t) flags, 0);
    if (!ok) throw std::runtime_error("failed to restore kv cache");
}
std::map<ggml_backend_buffer_type_t, size_t> fg153_hook_memory_breakdown(
        const llama_memory_recurrent * m) {
    std::map<ggml_backend_buffer_type_t, size_t> out;
    auto pr = acquire_mem(m);
    if (!pr) return out;
    int64_t h = fg153_goose_lifecycle_entry(24, (int64_t)(intptr_t) pr.get(), 0, 0, 0, 0);
    int64_t n = fg153_goose_lifecycle_entry(25, h, 0, 0, 0, 0);
    for (int64_t i = 0; i < n; ++i) {
        auto buft = (ggml_backend_buffer_type_t)(intptr_t) fg153_goose_lifecycle_entry(26, h, i, 0, 0, 0);
        size_t bytes = (size_t) fg153_goose_lifecycle_entry(27, h, i, 0, 0, 0);
        out[buft] += bytes;
    }
    fg153_box_free(h);
    return out;
}
