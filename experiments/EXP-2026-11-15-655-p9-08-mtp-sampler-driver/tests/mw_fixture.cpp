// Differential gate for P9-08 G4: the MTP capture warm-up and its harvest.
//
// The oracle is the carrier's text. tests/native_extract.py copies four regions
// of session.cpp verbatim:
//
//   native_warm_seq.inc    992-1008   begin_capture .. end_capture
//   native_warm_walk.inc   1011-1037  the captured-layer harvest
//   native_warm_clear.inc  1129-1130  the KV wipe after the warm-up
//   native_dense_filter.inc 1056-1080 the dense/row split handed to the streamer
//
// and this fixture supplies the llama API, the hook, the gguf offset maps and
// the tensor leaves as stubs that record what the carrier does instead of doing
// it. Both arms see the same synthetic session state; the gate compares the
// event sequences, the failure code that closes the open, the walked counts and
// the dense/row verdict per weight.
//
// Nothing here touches a model, a GPU or a file: the tensors are byte-less
// descriptors and the offsets are integers.
//
// Checks are explicit (no <cassert>): NDEBUG is defined in Release builds.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "bmoe/config.h" // the real RunConfig / SpecConfig
#include "mw.h"          // mw_*, goose_init

using namespace bmoe; // main.cpp:40 — the carrier's own using-directive

// ── stub llama surface ─────────────────────────────────────────────────────
// One recorded carrier action: op, first argument, second argument.
struct Ev {
    int64_t op;
    int64_t a0;
    int64_t a1;
};

std::vector<Ev> g_warm;

using llama_token = int32_t;
using llama_pos = int32_t;

struct llama_vocab {
    int tag;
};
struct llama_context {
    int tag;
};
struct llama_memory_t {
    void * p;
};

struct llama_batch {
    int kind; // 0 empty, 1 verify batch, 2 one-shot
    llama_token tok;
    int n;
    int pos0;
    bool logits;
};

void batch_fill(llama_batch & b, const llama_token * toks, int n, llama_pos pos0, bool all_logits) {
    g_warm.push_back(Ev{2, (int64_t) pos0, all_logits ? 1 : 0});
    b.kind = 1;
    b.tok = n > 0 ? toks[0] : -1;
    b.n = n;
    b.pos0 = pos0;
    b.logits = all_logits;
}

llama_batch llama_batch_get_one(llama_token * tok, int n) {
    llama_batch b{};
    b.kind = 2;
    b.tok = n > 0 ? tok[0] : -1;
    b.n = n;
    b.pos0 = 0;
    b.logits = true;
    return b;
}

namespace {

int g_bos = 1;
int g_decode_rc = 0;
bool g_process_ok = true;
int g_fail_code = 0;
int g_clear_target = 0, g_clear_draft = 0;
std::string g_fail_msg;

// Fail codes: the message class the carrier returns. 1..7 are the seven fail()
// sites of these three regions; a message outside the table is 99 so a fixture
// drift cannot pass silently.
int classify(const std::string & m) {
    if (m.rfind("capture warm-up decode failed", 0) == 0) return 1;
    if (m.rfind("MTP capture warm-up failed", 0) == 0) return 2;
    if (m.rfind("cannot read gguf offsets", 0) == 0) return 3;
    if (m.rfind("captured MoE layer is missing expert tensor", 0) == 0) return 4;
    if (m.rfind("no gguf offset for tensor", 0) == 0) return 5;
    if (m.rfind("inconsistent expert count", 0) == 0) return 6;
    if (m.rfind("no MoE expert tensors captured", 0) == 0) return 7;
    return 99;
}

struct FailSink {
    int operator()(const std::string & m) const {
        g_fail_msg = m;
        g_fail_code = classify(m);
        return g_fail_code;
    }
};

struct Hook;
Hook * g_hook_self = nullptr;

// ── the captured leaves ────────────────────────────────────────────────────
struct Backend {
    int dev_type; // 0 CPU, 1 non-CPU
};
struct Buffer {
    Backend dev;
};
struct ggml_backend_buffer_type {
    Backend dev;
};
using ggml_backend_buffer_type_t = ggml_backend_buffer_type *;
using ggml_backend_dev_t = Backend *;
enum { GGML_BACKEND_DEVICE_TYPE_CPU = 0 };

ggml_backend_buffer_type_t ggml_backend_buffer_get_type(Buffer * b) {
    static thread_local ggml_backend_buffer_type bt;
    bt.dev = b->dev;
    return &bt;
}
ggml_backend_dev_t ggml_backend_buft_get_device(ggml_backend_buffer_type_t bt) { return bt ? &bt->dev : nullptr; }
int ggml_backend_dev_type(ggml_backend_dev_t d) { return d->dev_type; }
struct ggml_tensor {
    std::string name;
    int ne[4] = {0, 0, 0, 0};
    Buffer * buffer = nullptr;
};

struct Proj {
    ggml_tensor * tensor = nullptr;
    uint64_t file_off = 0;
    int file_idx = -1;
};

struct LayerExperts {
    bool bound = false;
    Proj proj[4];
};

struct MoeRecipe {
    static constexpr int max_exps = 4;
    const char * exps_suffix[4] = {"ffn_gate_exps.weight", "ffn_up_exps.weight",
                                   "ffn_down_exps.weight", nullptr};
};

struct DenseTensorRef {
    ggml_tensor * tensor = nullptr;
    uint64_t file_off = 0;
    uint64_t size = 0;
    int file_idx = -1;
};

// The gguf offset maps, with the lookups recorded: the native walk visits a slot
// exactly when it asks the map for the tensor's offset, which is what makes the
// module's VISIT events comparable.
struct Touch {
    int layer;
    int slot;
    int found;
};

struct OffMap {
    std::map<std::string, uint64_t> m;
    std::map<std::string, std::pair<int, int>> * where = nullptr;
    std::vector<Touch> * log = nullptr;

    using iterator = std::map<std::string, uint64_t>::iterator;
    using const_iterator = std::map<std::string, uint64_t>::const_iterator;

    // find/end are logically const: the lookup may record what was asked for.
    // The carrier calls them through a const reference, as the native code does.

    iterator find(const std::string & k) const {
        std::map<std::string, uint64_t>::iterator it =
            const_cast<std::map<std::string, uint64_t> &>(m).find(k);
        if (log && where) {
            auto w = where->find(k);
            if (w != where->end()) log->push_back(Touch{w->second.first, w->second.second, it == m.end() ? 0 : 1});
        }
        return it;
    }
    iterator end() const {
        return const_cast<std::map<std::string, uint64_t> &>(m).end();
    }
    uint64_t & operator[](const std::string & k) { return m[k]; }
};

struct GgufOffsets {
    bool ok = false;
    OffMap off_by_name;
    std::map<std::string, uint64_t> size_by_name;
    std::map<std::string, int> file_by_name;
    std::vector<std::string> shard_paths;
};

// A context handle with the two things the carrier asks of a unique_ptr: a
// truth value and get(). The draft context exists only for MTP sessions.
struct CtxPtr {
    bool present = false;
    llama_context * p = nullptr;
    llama_context * get() const { return p; }
    explicit operator bool() const { return present; }
};

struct Im {
    llama_vocab * vocab = nullptr;
    Hook * hook = nullptr;
    llama_batch mtp_batch{};
    struct Mtp {
        int * p = nullptr;
        int * get() const { return p; }
    } mtp;
    CtxPtr ctx_dft;
};

Im im;
RunConfig cfg;
llama_context * ctx = nullptr;
const MoeRecipe * recipe = nullptr;
GgufOffsets g_offs;
std::map<std::string, std::pair<int, int>> g_where;

GgufOffsets * g_meta_offsets = nullptr;

struct Meta {
    GgufOffsets & offsets;
};
Meta meta() { return Meta{*g_meta_offsets}; }

int fail(const std::string & m) { return classify(m); }

struct Hook {
    std::vector<LayerExperts> layers;
    std::vector<std::pair<std::string, ggml_tensor *>> weights;
    bool capturing = false;

    void begin_capture() {
        capturing = true;
        g_warm.push_back(Ev{1, 0, 0});
    }
    void end_capture() {
        capturing = false;
        g_warm.push_back(Ev{6, 0, 0});
    }
    std::vector<LayerExperts> captured() { return layers; }
    std::vector<std::pair<std::string, ggml_tensor *>> captured_weights() { return weights; }
};

Hook g_hook;

std::vector<llama_memory_t> g_memories;

} // namespace

llama_token llama_vocab_bos(llama_vocab *) { return (llama_token) g_bos; }

int llama_decode(llama_context *, llama_batch b) {
    // a0 carries how many positions the decode was given: one for the dense
    // warm-up, one for the verify batch (pos0 0, all_logits on).
    g_warm.push_back(Ev{b.kind == 1 ? 3 : 5, (int64_t) b.n, 0});
    return g_decode_rc;
}

bool common_speculative_process(void *, llama_batch &) {
    g_warm.push_back(Ev{4, 0, 0});
    return g_process_ok;
}

llama_memory_t llama_get_memory(llama_context * c) { return llama_memory_t{c}; }

void llama_memory_clear(llama_memory_t m, bool) {
    g_memories.push_back(m);
    if (m.p == (void *) ctx)
        g_clear_target = 1;
    else
        g_clear_draft = 1;
    g_warm.push_back(Ev{m.p == (void *) ctx ? 7 : 8, 0, 0});
}

namespace {

// ── the harness ────────────────────────────────────────────────────────────
int g_cases = 0, g_values = 0, g_fails = 0;

void new_case() { ++g_cases; }
void note(bool ok, const std::string & what) {
    ++g_values;
    if (!ok) {
        ++g_fails;
        std::printf("  FAIL [G4] %s\n", what.c_str());
    }
}

struct WarmState {
    int moe = 1;
    int mtp = 0;
    int has_draft = 0;
    int bos = 1;
    int decode_rc = 0;
    int process_ok = 1;
};

struct WarmOut {
    std::vector<Ev> ev;
    int fail_code = 0;
    int warm_token = 0;
};

// The carrier text, compiled as its own function. `ctx`, `im`, `cfg`, `recipe`
// and `fail` are the fixture's; everything else is the carrier's.
int native_warm_body() {
    {
#include "native_warm_seq.inc"
    }
    {
#include "native_warm_clear.inc"
    }
    return 0;
}

WarmOut native_warm(const WarmState & st) {
    WarmOut out;
    g_warm.clear();
    g_decode_rc = st.decode_rc;
    g_process_ok = st.process_ok != 0;
    g_bos = st.bos;
    g_clear_target = g_clear_draft = 0;
    im.vocab = (llama_vocab *) (intptr_t) 1;
    im.hook = &g_hook;
    im.mtp.p = (int *) (intptr_t) 2;
    im.ctx_dft.present = st.has_draft != 0;
    static int dft_object = 0;
    im.ctx_dft.p = (llama_context *) &dft_object;
    static int ctx_object = 0;
    ctx = (llama_context *) &ctx_object;
    cfg.spec.source = st.mtp ? DraftSource::mtp : DraftSource::none;
    cfg.moe.enabled = st.moe != 0;
    im.mtp_batch = llama_batch{};
    // The carrier wraps the whole block in `if (cfg.moe.enabled)`, outside the
    // extracted region; the plan models that gate as "no events at all".
    const int rc = st.moe == 0 ? 0 : native_warm_body();
    out.ev = g_warm;
    out.fail_code = rc;
    out.warm_token = st.bos;
    return out;
}

struct WarmCase {
    const char * tag;
    WarmState st;
};

void run_warm(const WarmCase & c) {
    new_case();
    const WarmOut nat = native_warm(c.st);
    const std::string t = c.tag;

    int64_t plan[64];
    std::memset(plan, 0, sizeof plan);
    sl_i64 pv{plan, 64};
    const int64_t n = mw_warm_plan(c.st.moe, c.st.mtp, c.st.has_draft, pv);
    const int64_t mfail = mw_warm_fail(c.st.decode_rc, c.st.process_ok, c.st.mtp);

    note(mw_warm_token(c.st.bos) == (int64_t) (c.st.bos < 0 ? 0 : c.st.bos), t + " warm token");

    // A failure closes the open where it happens: the plan is a whole sequence,
    // the carrier stops at the failing call. Truncate at the same point before
    // comparing, so the comparison is "the same events happened before it".
    int64_t plan_len = n;
    if (mfail == 1) {
        for (int64_t k = 0; k < n; ++k) {
            if (plan[3 * k] == 3 || plan[3 * k] == 5) { plan_len = k + 1; break; }
        }
    } else if (mfail == 2) {
        for (int64_t k = 0; k < n; ++k) {
            if (plan[3 * k] == 4) { plan_len = k + 1; break; }
        }
    }
    note(plan_len == (int64_t) nat.ev.size(), t + " event count");
    const size_t common = (size_t) (plan_len < (int64_t) nat.ev.size() ? plan_len : (int64_t) nat.ev.size());
    for (size_t k = 0; k < common; ++k) {
        note(plan[3 * k] == nat.ev[k].op, t + " op[" + std::to_string(k) + "]");
        note(plan[3 * k + 1] == nat.ev[k].a0, t + " a0[" + std::to_string(k) + "]");
        note(plan[3 * k + 2] == nat.ev[k].a1, t + " a1[" + std::to_string(k) + "]");
    }
    // The failure that closes the open, and that it closes it BEFORE the clears.
    note(mfail == (int64_t) nat.fail_code, t + " failure code");
    if (nat.fail_code != 0) {
        bool saw_clear = false;
        for (const Ev & e : nat.ev) saw_clear = saw_clear || e.op >= 7;
        note(!saw_clear, t + " no KV wipe after a failed warm-up");
    }
}

// ── the harvest ────────────────────────────────────────────────────────────
struct LayerSpec {
    int bound;
    int has_tensor[4];
    int offset[4];
    int ne2[4];
};

struct WalkOut {
    std::vector<Touch> touches;
    int fail_code = 0;
    int n_bound = 0;
    int n_expert = 0;
};

// The carrier's harvest text, compiled as its own function with fixture locals.
int native_walk_body(std::vector<Ev> *, int * n_bound_out, int * n_expert_out) {
#include "native_warm_walk.inc"
    *n_bound_out = n_bound;
    *n_expert_out = n_expert;
    return 0;
}

WalkOut native_walk(const std::vector<LayerSpec> & spec, bool offsets_ok, bool no_layers, int nslot = 4) {
    WalkOut out;
    g_fail_code = 0;
    g_fail_msg.clear();
    g_hook.layers.clear();
    g_offs = GgufOffsets{};
    g_offs.ok = offsets_ok;
    g_offs.off_by_name.where = &g_where;
    g_offs.off_by_name.log = &out.touches;
    g_where.clear();
    cfg.model_path = "/models/stub.gguf";
    static MoeRecipe rec;
    // An architecture that uses nslot projection slots leaves the rest null, and
    // the carrier skips a null slot before it ever looks at a tensor.
    static const char * names[4] = {"ffn_gate_exps.weight", "ffn_up_exps.weight",
                                    "ffn_down_exps.weight", nullptr};
    for (int i = 0; i < 4; ++i) rec.exps_suffix[i] = i < nslot ? names[i] : nullptr;
    recipe = &rec;
    g_meta_offsets = &g_offs;

    for (size_t l = 0; l < spec.size(); ++l) {
        LayerExperts L;
        L.bound = spec[l].bound != 0;
        for (int p = 0; p < 4; ++p) {
            if (rec.exps_suffix[p] == nullptr) continue;
            const std::string name = "blk." + std::to_string(l) + "." + rec.exps_suffix[p];
            g_where[name] = std::make_pair((int) l, p);
            if (L.bound && spec[l].has_tensor[p]) {
                static std::vector<std::unique_ptr<ggml_tensor>> pool;
                pool.emplace_back(new ggml_tensor());
                ggml_tensor * t = pool.back().get();
                t->name = name;
                t->ne[2] = spec[l].ne2[p];
                L.proj[p].tensor = t;
                L.proj[p].file_off = 0;
                if (spec[l].offset[p]) g_offs.off_by_name[name] = (uint64_t) spec[l].offset[p];
                g_offs.file_by_name[name] = (int) l;
            }
        }
        g_hook.layers.push_back(L);
    }
    if (no_layers) g_hook.layers.clear();

    int nb = 0, ne = 0;
    const int rc = native_walk_body(nullptr, &nb, &ne);
    out.fail_code = rc;
    out.n_bound = nb;
    out.n_expert = ne;
    return out;
}

struct WalkCase {
    const char * tag;
    std::vector<LayerSpec> spec;
    bool offsets_ok = true;
    bool no_layers = false;
};

// The module's harvest plan over the same state.
struct WalkPlan {
    std::vector<Ev> ev; // op, layer, slot, x, y packed as five i64
    std::vector<int64_t> flat;
};

WalkPlan run_plan(const std::vector<LayerSpec> & spec, bool offsets_ok, int nslot) {
    WalkPlan out;
    const int nl = (int) spec.size();
    std::vector<int64_t> bound(nl + 1, 0), used(nslot + 1, 0);
    std::vector<int64_t> has_t((size_t) (nl + 1) * nslot, 0), off((size_t) (nl + 1) * nslot, 0),
        ne2((size_t) (nl + 1) * nslot, 0);
    for (int l = 0; l < nl; ++l) {
        bound[l] = spec[l].bound;
        for (int p = 0; p < nslot; ++p) {
            const size_t idx = (size_t) l * nslot + p;
            has_t[idx] = spec[l].has_tensor[p];
            off[idx] = spec[l].offset[p];
            ne2[idx] = spec[l].ne2[p];
        }
    }
    for (int p = 0; p < nslot; ++p) used[p] = 1;

    std::vector<int64_t> outv((size_t) 5 * (nl * nslot + 8), 0);
    sl_i64 sv{outv.data(), (int64_t) outv.size()};
    const int64_t n = mw_walk(nl, nslot, sl_i64{bound.data(), (int64_t) bound.size()},
                              sl_i64{used.data(), (int64_t) used.size()},
                              sl_i64{has_t.data(), (int64_t) has_t.size()},
                              sl_i64{off.data(), (int64_t) off.size()},
                              sl_i64{ne2.data(), (int64_t) ne2.size()}, sv);
    out.flat.assign(outv.begin(), outv.begin() + std::min<size_t>(outv.size(), (size_t) n * 5));
    // The offsets-table gate is a separate decision in the carrier.
    if (!offsets_ok) {
        out.flat.clear();
    }
    return out;
}

void run_walk(const WalkCase & c, int nslot) {
    new_case();
    const WalkOut nat = native_walk(c.spec, c.offsets_ok, c.no_layers, nslot);
    const std::string t = c.tag;

    // The offsets gate comes before any lookup in the carrier.
    const int64_t gate = mw_offsets_gate(c.offsets_ok ? 1 : 0);
    note(gate == (nat.fail_code == 3 ? 0 : 1), t + " offsets gate");
    if (gate == 0) {
        // The carrier returns before the walk; the module's plan is not entered.
        note(nat.touches.empty(), t + " no lookup without offsets");
        return;
    }

    const WalkPlan pl = run_plan(c.spec, c.offsets_ok, nslot);

    // The touch sequence: the carrier visits a slot when it asks for the
    // tensor's offset; the plan calls that VISIT, or FAIL_NO_OFFSET when the
    // map has no entry. Same slots, same order, same found-ness.
    std::vector<Touch> plan_touch;
    int64_t total_bound = -1, total_experts = -1;
    int64_t fail_op = 0;
    for (size_t k = 0; k + 4 < pl.flat.size(); k += 5) {
        const int64_t op = pl.flat[k];
        if (op == 0) continue; // unbound layer: nothing to touch
        if (op == 1 || op == 4 || op == 5) {
            plan_touch.push_back(Touch{(int) pl.flat[k + 1], (int) pl.flat[k + 2], 1});
        } else if (op == 3) {
            plan_touch.push_back(Touch{(int) pl.flat[k + 1], (int) pl.flat[k + 2], 0});
            fail_op = op;
        } else if (op == 7) {
            total_bound = pl.flat[k + 1];
        } else if (op == 8) {
            total_experts = pl.flat[k + 1];
        } else if (op == 6) {
            // The lookup happened and succeeded; the count check is what failed.
            plan_touch.push_back(Touch{(int) pl.flat[k + 1], (int) pl.flat[k + 2], 1});
            fail_op = op;
        } else if (op == 2) {
            // A missing tensor fails before any offset lookup.
            fail_op = op;
        }
    }

    note(plan_touch.size() == nat.touches.size(), t + " touch count");
    const size_t common = std::min(plan_touch.size(), nat.touches.size());
    for (size_t k = 0; k < common; ++k) {
        note(plan_touch[k].layer == nat.touches[k].layer, t + " touch layer[" + std::to_string(k) + "]");
        note(plan_touch[k].slot == nat.touches[k].slot, t + " touch slot[" + std::to_string(k) + "]");
        note(plan_touch[k].found == nat.touches[k].found, t + " touch found[" + std::to_string(k) + "]");
    }

    // The failure that closes the open: 0, a missing tensor, a missing offset or
    // an inconsistent expert count. Inconsistency is compared by the class,
    // because the module also reports the expected value.
    const int nat_class = nat.fail_code;
    if (nat_class == 7) {
        // A capture that bound nothing is its own refusal, issued after the
        // loops; the plan does not diagnose it, the bound gate does.
        note(mw_bound_gate(nat.n_bound) == 0, t + " bound gate refuses");
        note(plan_touch.empty(), t + " nothing touched");
        return;
    }
    int plan_class = 0;
    if (fail_op == 2) plan_class = 4;
    else if (fail_op == 3) plan_class = 5;
    else if (fail_op == 6) plan_class = 6;
    note(plan_class == nat_class, t + " failure class");

    if (nat_class == 0) {
        note(total_bound == (int64_t) nat.n_bound, t + " total bound");
        note(total_experts == (int64_t) nat.n_expert, t + " total experts");
        note(mw_bound_gate(nat.n_bound) == 1, t + " bound gate accepts");
    }
}

// ── the dense/row split ────────────────────────────────────────────────────
struct WeightSpec {
    const char * name;
    bool is_expert;
    bool has_tensor;
    bool has_buffer;
    int dev_type; // 0 CPU, 1 else
    bool is_row;
    bool in_off;
    bool in_size;
};

struct DenseOut {
    std::vector<std::string> dense;
    std::vector<std::string> rows;
};

DenseOut native_dense(const std::vector<WeightSpec> & w) {
    DenseOut out;
    g_hook.weights.clear();
    std::vector<std::unique_ptr<ggml_tensor>> pool;
    std::vector<std::unique_ptr<Buffer>> bufs;
    std::unordered_set<std::string> expert_names, row_names;
    std::vector<DenseTensorRef> dense, rows;
    g_offs = GgufOffsets{};
    g_offs.ok = true;
    g_offs.off_by_name.where = nullptr;
    g_offs.off_by_name.log = nullptr;
    for (const WeightSpec & s : w) {
        ggml_tensor * t = nullptr;
        if (s.has_tensor) {
            pool.emplace_back(new ggml_tensor());
            t = pool.back().get();
            t->name = s.name;
            if (s.has_buffer) {
                bufs.emplace_back(new Buffer());
                bufs.back()->dev.dev_type = s.dev_type;
                t->buffer = bufs.back().get();
            }
        }
        g_hook.weights.emplace_back(std::string(s.name), t);
        if (s.is_expert) expert_names.insert(s.name);
        if (s.is_row) row_names.insert(s.name);
        if (s.in_off) g_offs.off_by_name[std::string(s.name)] = 4096u;
        if (s.in_size) g_offs.size_by_name[std::string(s.name)] = 4096u;
        g_offs.file_by_name[std::string(s.name)] = 0;
    }
    const GgufOffsets & offs = g_offs;
#include "native_dense_filter.inc"
    for (const DenseTensorRef & d : dense) out.dense.push_back(d.tensor->name);
    for (const DenseTensorRef & d : rows) out.rows.push_back(d.tensor->name);
    return out;
}

struct RowBudget {
    int64_t bytes;
};

int64_t native_row_budget(int mb) {
    cfg.moe.row_stream_mb = mb;
#include "native_row_budget.inc"
    return (int64_t) row_budget;
}

void run_dense(const char * tag, const std::vector<WeightSpec> & w) {
    new_case();
    const DenseOut nat = native_dense(w);
    const std::string t = tag;
    std::vector<std::string> pdense, prows;
    for (const WeightSpec & s : w) {
        const int64_t b = mw_dense_bucket(s.is_expert ? 1 : 0, s.has_tensor ? 1 : 0, s.has_buffer ? 1 : 0,
                                          s.dev_type != 0 ? 1 : 0, s.in_off ? 1 : 0, s.in_size ? 1 : 0,
                                          s.is_row ? 1 : 0);
        if (b >= 0) pdense.push_back(s.name);
        if (b == 1) prows.push_back(s.name);
    }
    note(pdense.size() == nat.dense.size(), t + " dense count");
    const size_t common = std::min(pdense.size(), nat.dense.size());
    for (size_t k = 0; k < common; ++k) note(pdense[k] == nat.dense[k], t + " dense[" + std::to_string(k) + "]");
    note(prows.size() == nat.rows.size(), t + " rows count");
    const size_t cr = std::min(prows.size(), nat.rows.size());
    for (size_t k = 0; k < cr; ++k) note(prows[k] == nat.rows[k], t + " rows[" + std::to_string(k) + "]");
}

} // namespace

int main(int argc, char ** argv) {
    goose_init(argc, argv);

    // ── warm-up sequences ──────────────────────────────────────────────────
    {
        WarmCase c{"no-stream", WarmState{}};
        c.st.moe = 0;
        run_warm(c);
    }
    {
        WarmCase c{"dense", WarmState{}};
        run_warm(c);
    }
    {
        WarmCase c{"mtp", WarmState{}};
        c.st.mtp = 1;
        c.st.has_draft = 1;
        run_warm(c);
    }
    {
        WarmCase c{"draft-no-mtp", WarmState{}};
        c.st.has_draft = 1;
        run_warm(c);
    }
    {
        WarmCase c{"bos-missing", WarmState{}};
        c.st.bos = -1;
        run_warm(c);
    }
    {
        WarmCase c{"bos-zero", WarmState{}};
        c.st.bos = 0;
        run_warm(c);
    }
    {
        WarmCase c{"bos-large", WarmState{}};
        c.st.bos = 151643;
        run_warm(c);
    }
    {
        WarmCase c{"decode-fail", WarmState{}};
        c.st.decode_rc = 1;
        run_warm(c);
    }
    {
        WarmCase c{"decode-fail-negative", WarmState{}};
        c.st.decode_rc = -1; // llama_decode reports a failure as a negative code
        run_warm(c);
    }
    {
        WarmCase c{"mtp-decode-fail", WarmState{}};
        c.st.mtp = 1;
        c.st.has_draft = 1;
        c.st.decode_rc = 1;
        run_warm(c);
    }
    {
        WarmCase c{"process-fail", WarmState{}};
        c.st.mtp = 1;
        c.st.has_draft = 1;
        c.st.process_ok = 0;
        run_warm(c);
    }
    {
        WarmCase c{"process-fail-dense", WarmState{}};
        c.st.process_ok = 0; // no draft context: process is not on the path
        run_warm(c);
    }

    // ── the harvest ────────────────────────────────────────────────────────
    auto ls = [](int bound, std::vector<int> has, std::vector<int> off, std::vector<int> ne) {
        LayerSpec s{};
        s.bound = bound;
        for (int p = 0; p < 4; ++p) {
            s.has_tensor[p] = p < (int) has.size() ? has[p] : 0;
            s.offset[p] = p < (int) off.size() ? off[p] : 0;
            s.ne2[p] = p < (int) ne.size() ? ne[p] : 0;
        }
        return s;
    };
    {
        WalkCase c{"one-layer", {ls(1, {1, 1, 1}, {4096, 8192, 12288}, {128, 128, 128})}};
        run_walk(c, 3);
    }
    {
        WalkCase c{"two-layers", {ls(1, {1, 1, 1}, {4096, 8192, 12288}, {128, 128, 128}),
                                  ls(1, {1, 1, 1}, {5096, 9192, 13288}, {128, 128, 128})}};
        run_walk(c, 3);
    }
    {
        WalkCase c{"unbound-middle", {ls(1, {1, 1, 1}, {4096, 8192, 12288}, {128, 128, 128}),
                                      ls(0, {1, 1, 1}, {4096, 8192, 12288}, {64, 64, 64}),
                                      ls(1, {1, 1, 1}, {5096, 9192, 13288}, {128, 128, 128})}};
        run_walk(c, 3);
    }
    {
        WalkCase c{"no-layers", {}};
        c.no_layers = true;
        run_walk(c, 3);
    }
    {
        WalkCase c{"missing-tensor", {ls(1, {1, 0, 1}, {4096, 8192, 12288}, {128, 128, 128})}};
        run_walk(c, 3);
    }
    {
        WalkCase c{"missing-offset", {ls(1, {1, 1, 1}, {4096, 0, 12288}, {128, 128, 128})}};
        run_walk(c, 3);
    }
    {
        WalkCase c{"inconsistent", {ls(1, {1, 1, 1}, {4096, 8192, 12288}, {128, 64, 128})}};
        run_walk(c, 3);
    }
    {
        WalkCase c{"inconsistent-across-layers", {ls(1, {1, 1, 1}, {4096, 8192, 12288}, {128, 128, 128}),
                                                  ls(1, {1, 1, 1}, {4096, 8192, 12288}, {128, 64, 128})}};
        run_walk(c, 3);
    }
    {
        WalkCase c{"no-offsets", {ls(1, {1, 1, 1}, {4096, 8192, 12288}, {128, 128, 128})}};
        c.offsets_ok = false;
        run_walk(c, 3);
    }
    {
        // An architecture that uses only two projection slots: the unused one
        // must be skipped whole, even though it has no tensor.
        WalkCase c{"two-slot-arch", {ls(1, {1, 1}, {4096, 8192}, {256, 256})}};
        run_walk(c, 2);
    }

    // The slot ladder itself, over the whole 16-combination grid, against the
    // carrier's own walk on a single-layer/single-slot state.
    for (int bound = 0; bound < 2; ++bound)
        for (int has_t = 0; has_t < 2; ++has_t)
            for (int has_off = 0; has_off < 2; ++has_off) {
                new_case();
                LayerSpec s = ls(bound, {has_t}, {has_off}, {128});
                const WalkOut nat = native_walk({s}, true, false, 1);
                const int64_t code = mw_slot_step(bound, 1, has_t, has_off);
                char tag[64];
                std::snprintf(tag, sizeof tag, "slot(b=%d,t=%d,o=%d)", bound, has_t, has_off);
                const std::string t = tag;
                if (bound == 0) note(code == 0, t + " unbound layer skipped");
                else if (has_t == 0) note(code == 2 && nat.fail_code == 4, t + " missing tensor");
                else if (has_off == 0) note(code == 3 && nat.fail_code == 5, t + " missing offset");
                else note(code == 1 && nat.fail_code == 0, t + " visited");
                // An unused slot is skipped even on a bound layer.
                note(mw_slot_step(1, 0, 1, 1) == -2, t + " unused slot code");
            }

    // ── the dense/row split ────────────────────────────────────────────────
    run_dense("plain", {
        {"token_embd.weight", false, true, true, 0, false, true, true},
        {"blk.0.attn_q.weight", false, true, true, 0, false, true, true},
        {"blk.0.ffn_gate_exps.weight", true, true, true, 0, false, true, true},
    });
    run_dense("rows", {
        {"token_embd.weight", false, true, true, 0, true, true, true},
        {"output.weight", false, true, true, 0, false, true, true},
    });
    run_dense("skips", {
        {"graph_input", false, false, false, 0, false, false, false},
        {"kv_cache", false, true, false, 0, false, false, false},
        {"cuda_weight", false, true, true, 1, false, true, true},
        {"no_off", false, true, true, 0, false, false, true},
        {"no_size", false, true, true, 0, false, true, false},
        {"expert", true, true, true, 0, true, true, true},
    });

    // ── the row window ─────────────────────────────────────────────────────
    for (int mb : {0, 1, 64, 512, -1}) {
        new_case();
        char tag[48];
        std::snprintf(tag, sizeof tag, "row-budget(%d)", mb);
        note(mw_row_budget(mb) == native_row_budget(mb), std::string(tag));
    }

    std::printf("FG654_MW cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails;
}
