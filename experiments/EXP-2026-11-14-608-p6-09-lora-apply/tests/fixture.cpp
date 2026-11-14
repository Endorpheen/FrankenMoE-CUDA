// Differential gate for P6-09 S2, LoRA application, scale and cleanup (EXP-2026-11-14-605).
//
// Oracle strategy: the real llm_graph_context::build_lora_mm() is executed for
// one weight tensor against hand-built llama_adapter_lora objects whose a/b
// tensors live in a CPU backend buffer, and the resulting four-node sequence
// is inspected and evaluated. The port only returns data (which adapters
// apply, the scale, the delta shape and the node count); the native graph is
// what produces numbers, so the port cannot agree with itself.
//
// llama_model is abstract in this tree (seven pure virtuals), so the oracle
// cannot hold one by value. TestModel below implements the seven virtuals as
// unused stubs; build_lora_mm never calls them. The adapter objects are
// constructed field by field instead of being loaded from a GGUF file: S1
// already gates the loader, and this slice starts from the pair table it
// produces.
//
// Floating point: every payload value is a multiple of 0.25 in [-2, 2] with
// n_embd <= 8 and rank <= 3, so every dot product is an exact float sum and a
// different accumulation order (or FMA) cannot change a single bit. The scale
// is applied as a single float multiply on both sides. The gate therefore
// compares values with exact equality, not with a tolerance.
//
// Output: FG605_LORA cases=N values=M failures=K
// Exit: 0 ok, 1 failures, 2 native setup failed.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <array>
#include <vector>
#include <algorithm>

#include "ggml.h"
#include "ggml-alloc.h"
#include "ggml-backend.h"
#include "ggml-cpp.h"
#include "llama.h"

#define private public
#include "llama-adapter.h"
#undef private

#include "llama-model.h"
#include "llama-graph.h"

#include "runtime-gen.h"

static int cases  = 0;
static int values = 0;
static int fails  = 0;

static ggml_backend_dev_t DEV = nullptr;
static ggml_backend_t     BE  = nullptr;

static void fail(const std::string & c, const std::string & what) {
    ++fails;
    if (fails <= 200) {
        printf("FAIL %s: %s\n", c.c_str(), what.c_str());
    }
}

static void eq_i64(const std::string & c, const char * what, int64_t native, int64_t port) {
    ++values;
    if (native != port) {
        fail(c, std::string(what) + ": native=" + std::to_string(native) + " port=" + std::to_string(port));
    }
}

static void eq_f32_bits(const std::string & c, const char * what, float native, float port) {
    ++values;
    uint32_t a = 0;
    uint32_t b = 0;
    memcpy(&a, &native, sizeof(a));
    memcpy(&b, &port,  sizeof(b));
    if (a != b) {
        fail(c, std::string(what) + ": native=" + std::to_string(native) + " (0x" +
                    [&] { char s[16]; snprintf(s, sizeof(s), "%08x", a); return std::string(s); }() +
                    ") port=" + std::to_string(port) + " (0x" +
                    [&] { char s[16]; snprintf(s, sizeof(s), "%08x", b); return std::string(s); }() + ")");
    }
}

static sl_i64 si(std::vector<int64_t> & v) {
    return sl_i64{ v.data(), (int64_t) v.size() };
}

static sl_f32 sf(std::vector<float> & v) {
    return sl_f32{ v.data(), (int64_t) v.size() };
}

static sl_u8 su(std::vector<uint8_t> & v) {
    return sl_u8{ v.data(), (int64_t) v.size() };
}

// ---------------------------------------------------------------------------
// Case description

struct TS {
    std::string name;
    int64_t     ne0 = 8;   // rows of the matrix as ggml sees it: ne0 columns
    int64_t     ne1 = 8;
};

struct PSpec {
    int         ad = 0;
    std::string name;      // base name, no .lora_a/.lora_b suffix
    int         a  = 0;    // tensor indices in Case::tensors
    int         b  = 0;
};

struct Case {
    std::string name;

    std::string wname  = "blk.0.attn_q.weight";
    int64_t     w_ne0  = 8;
    int64_t     w_ne1  = 8;
    int64_t     cur_ne1 = 1;
    int64_t     cur_ne2 = 1;
    int64_t     cur_ne3 = 1;

    std::vector<TS>    tensors;
    std::vector<PSpec> pairs;
    std::vector<float> alpha;
    std::vector<float> caller;
    std::vector<bool>  registered;   // adapter is in model.loras

    bool     port_only       = false; // native would assert, contract block
    int      cleanup_target  = -1;    // adapter index freed before the graph
    bool     cleanup_null    = false; // llama_adapter_lora_free(nullptr)
};

// Test-only subclass: llama_model is abstract, build_lora_mm needs no virtual.
struct TestModel : llama_model {
    explicit TestModel(const llama_model_params & params) : llama_model(params) {}

    void load_stats  (llama_model_loader &) override {}
    void load_hparams(llama_model_loader &) override {}
    void load_vocab  (llama_model_loader &) override {}
    bool load_tensors(llama_model_loader &) override { return false; }

    void load_arch_hparams(llama_model_loader &) override {}
    void load_arch_tensors(llama_model_loader &) override {}
    std::unique_ptr<llm_graph_context> build_arch_graph(const llm_graph_params &) const override {
        return nullptr;
    }
};

// ---------------------------------------------------------------------------
// Port side

struct PortRes {
    int64_t code   = 0;
    int64_t n_app  = 0;
    int64_t n_nodes = 0;
    int64_t dne2   = 0;
    int64_t dne3   = 0;
    int64_t npairs = 0;

    std::vector<int>     active;    // original adapter indices in port order
    std::vector<int>     pair_src;  // port pair index -> Case::pairs index
    std::vector<int64_t> st;        // AP_SLOTS per active adapter
    std::vector<float>   fs;
};

static const int64_t AP_SLOTS = 5;

static PortRes run_port(const Case & cs, const std::vector<int> & active) {
    PortRes r;
    r.active = active;

    const int64_t n_ad = (int64_t) active.size();

    std::vector<float> alpha((size_t) n_ad, 0.0f);
    std::vector<float> caller((size_t) n_ad, 0.0f);
    for (int64_t i = 0; i < n_ad; ++i) {
        alpha[(size_t) i]  = cs.alpha[(size_t) active[(size_t) i]];
        caller[(size_t) i] = cs.caller[(size_t) active[(size_t) i]];
    }

    // One name arena for the weight and every pair name.
    std::vector<uint8_t> names;
    const auto add_name = [&names](const std::string & s) -> int64_t {
        const int64_t off = (int64_t) names.size();
        names.insert(names.end(), s.begin(), s.end());
        return off;
    };
    const int64_t w_off = add_name(cs.wname);
    const int64_t w_len = (int64_t) cs.wname.size();

    std::vector<int64_t> pairs;
    std::vector<int>     pair_src; // original case pair index
    for (size_t k = 0; k < cs.pairs.size(); ++k) {
        const PSpec & p = cs.pairs[k];
        int port_ad = -1;
        for (size_t i = 0; i < active.size(); ++i) {
            if (active[i] == p.ad) {
                port_ad = (int) i;
            }
        }
        if (port_ad < 0) {
            continue;
        }
        const int64_t off = add_name(p.name);
        pairs.push_back((int64_t) port_ad);
        pairs.push_back(off);
        pairs.push_back((int64_t) p.name.size());
        pairs.push_back(p.a);
        pairs.push_back(p.b);
        pair_src.push_back((int) k);
    }

    std::vector<int64_t> t_ne0, t_ne1;
    for (const TS & t : cs.tensors) {
        t_ne0.push_back(t.ne0);
        t_ne1.push_back(t.ne1);
    }

    r.pair_src = pair_src;
    r.st.assign((size_t) (AP_SLOTS * n_ad), 0);
    r.fs.assign((size_t) n_ad, 0.0f);
    std::vector<int64_t> out(6, 0);

    const int64_t rc = lora_apply_dense(
        n_ad,
        sf(alpha), sf(caller),
        (int64_t) pair_src.size(), si(pairs),
        su(names),
        w_off, w_len,
        si(t_ne0), si(t_ne1),
        cs.w_ne1,
        cs.w_ne0, cs.cur_ne1, cs.cur_ne2, cs.cur_ne3,
        si(r.st), sf(r.fs), si(out));

    r.code    = out[0];
    r.n_app   = out[1];
    r.n_nodes = out[2];
    r.dne2    = out[3];
    r.dne3    = out[4];
    r.npairs  = out[5];

    if (rc != r.code) {
        fail(cs.name, "port return code does not match out[0]");
    }
    return r;
}

// ---------------------------------------------------------------------------
// Native side

static float pattern(int64_t k, int64_t seed) {
    const int64_t v = (k * 37 + seed * 11) % 17;
    return (float) (v - 8) * 0.25f;
}

struct Native {
    TestModel model { llama_model_default_params() };
    ggml_context * ctx = nullptr;
    ggml_backend_buffer_t buf = nullptr;

    ggml_tensor * w   = nullptr;
    ggml_tensor * cur = nullptr;

    std::vector<ggml_tensor *>     tensors;  // Case::tensors, by index
    std::vector<llama_adapter_lora *> ads;     // by adapter index
    std::vector<bool>               owned;     // registered in model.loras
    std::vector<bool>               deleted;   // released by llama_adapter_lora_free

    bool ok = false;

    void teardown() {
        for (size_t i = 0; i < ads.size(); ++i) {
            if (ads[i] != nullptr && !owned[i] && !deleted[i]) {
                delete ads[i];   // never registered: the model will not free it
                ads[i] = nullptr;
            }
        }
        if (buf != nullptr) {
            ggml_backend_buffer_free(buf);
            buf = nullptr;
        }
        if (ctx != nullptr) {
            ggml_free(ctx);
            ctx = nullptr;
        }
        // registered adapters are deleted by ~llama_model
    }
};

static bool setup_native(const Case & cs, Native & n) {
    n.model.hparams = llama_hparams{};
    n.model.hparams.n_embd          = (uint32_t) cs.w_ne0;
    n.model.hparams.n_layer_all     = 1;
    n.model.hparams.n_layer_nextn   = 0;
    n.model.hparams.n_head_arr[0]    = 1;
    n.model.hparams.n_head_kv_arr[0] = 1;
    n.model.hparams.n_ff_arr[0]      = 1;
    n.model.hparams.n_embd_head_k_full = 1;
    n.model.hparams.n_embd_head_v_full = 1;
    n.model.hparams.n_rot_full         = 1;
    n.model.hparams.rope_type          = LLAMA_ROPE_TYPE_NORM;

    const size_t overhead = (cs.tensors.size() + 8) * ggml_tensor_overhead() + (1u << 20);
    ggml_init_params ip = { overhead, nullptr, true };   // no_alloc: the backend buffer holds the data
    n.ctx = ggml_init(ip);
    if (n.ctx == nullptr) {
        return false;
    }

    n.w = ggml_new_tensor_2d(n.ctx, GGML_TYPE_F32, cs.w_ne0, cs.w_ne1);
    ggml_set_name(n.w, cs.wname.c_str());

    for (const TS & t : cs.tensors) {
        ggml_tensor * gt = ggml_new_tensor_2d(n.ctx, GGML_TYPE_F32, t.ne0, t.ne1);
        ggml_set_name(gt, t.name.c_str());
        n.tensors.push_back(gt);
    }

    n.cur = ggml_new_tensor_4d(n.ctx, GGML_TYPE_F32,
            cs.w_ne0, cs.cur_ne1, cs.cur_ne2, cs.cur_ne3);

    ggml_backend_buffer_type_t buft = ggml_backend_dev_buffer_type(DEV);
    n.buf = ggml_backend_alloc_ctx_tensors_from_buft(n.ctx, buft);
    if (n.buf == nullptr) {
        return false;
    }
    ggml_backend_buffer_clear(n.buf, 0);

    // Payloads: a multiple of 0.25 in [-2, 2], deterministic per tensor.
    std::vector<float> host;
    host.resize((size_t) (cs.w_ne0 * cs.w_ne1));
    for (size_t k = 0; k < host.size(); ++k) {
        host[k] = pattern((int64_t) k, 1);
    }
    ggml_backend_tensor_set(n.w, host.data(), 0, host.size() * sizeof(float));

    for (size_t i = 0; i < cs.tensors.size(); ++i) {
        host.resize((size_t) (cs.tensors[i].ne0 * cs.tensors[i].ne1));
        for (size_t k = 0; k < host.size(); ++k) {
            host[k] = pattern((int64_t) k, (int64_t) i + 2);
        }
        ggml_backend_tensor_set(n.tensors[i], host.data(), 0, host.size() * sizeof(float));
    }

    const int64_t n_cur = cs.w_ne0 * cs.cur_ne1 * cs.cur_ne2 * cs.cur_ne3;
    host.resize((size_t) n_cur);
    for (int64_t k = 0; k < n_cur; ++k) {
        host[(size_t) k] = pattern(k, 99);
    }
    ggml_backend_tensor_set(n.cur, host.data(), 0, host.size() * sizeof(float));

    // Adapters, field by field: S1 owns the loader.
    n.ads.assign(cs.alpha.size(), nullptr);
    n.owned.assign(cs.alpha.size(), false);
    n.deleted.assign(cs.alpha.size(), false);
    for (size_t i = 0; i < cs.alpha.size(); ++i) {
        auto * ad = new llama_adapter_lora(&n.model);
        ad->alpha = cs.alpha[i];
        for (const PSpec & p : cs.pairs) {
            if (p.ad != (int) i) {
                continue;
            }
            ad->ab_map[p.name] = llama_adapter_lora_weight(n.tensors[(size_t) p.a], n.tensors[(size_t) p.b]);
        }
        n.ads[i] = ad;
        if (i < cs.registered.size() && cs.registered[i]) {
            n.model.loras.insert(ad);
            n.owned[i] = true;
        }
    }
    n.ok = true;
    return true;
}

struct GraphOut {
    int final_op = 0;   // ggml_op of the tensor build_lora_mm returned
    int n_mm = 0, n_scale = 0, n_add = 0;
    std::vector<float> scales;
    std::vector<std::array<int64_t, 4>> delta_ne;
    std::vector<float> result;
};

// One build_lora_mm() call in a fresh graph context, expanded and evaluated.
static bool run_graph(
    Native & n,
    const std::vector<float> & caller,
    const std::vector<bool> & active,
    GraphOut & go,
    const std::string & c) {

    llama_adapter_loras loras;
    for (size_t i = 0; i < n.ads.size(); ++i) {
        if (n.ads[i] != nullptr && active[i]) {
            loras[n.ads[i]] = caller[i];
        }
    }

    llm_graph_result res(4096);

    llm_graph_params params{};
    params.arch     = LLM_ARCH_LLAMA;
    params.hparams  = n.model.hparams;
    params.cparams  = llama_cparams{};
    params.ubatch   = llama_ubatch{};
    params.n_outputs = 1;
    params.loras    = &loras;
    params.res      = &res;

    llm_graph_context gctx(params);

    ggml_tensor * out = gctx.build_lora_mm(n.w, n.cur);
    go.final_op = (int) out->op;

    ggml_cgraph * gf = res.get_gf();
    ggml_build_forward_expand(gf, out);

    const int n_nodes = ggml_graph_n_nodes(gf);
    for (int i = 0; i < n_nodes; ++i) {
        ggml_tensor * nd = ggml_graph_node(gf, i);
        switch (nd->op) {
            case GGML_OP_MUL_MAT: ++go.n_mm; break;
            case GGML_OP_SCALE:
                {
                    ++go.n_scale;
                    float s = 0.0f;
                    memcpy(&s, nd->op_params, sizeof(s));
                    go.scales.push_back(s);
                    go.delta_ne.push_back({ nd->ne[0], nd->ne[1], nd->ne[2], nd->ne[3] });
                } break;
            case GGML_OP_ADD: ++go.n_add; break;
            default: break;
        }
    }

    ggml_backend_buffer_t gbuf = ggml_backend_alloc_ctx_tensors(res.get_ctx(), BE);
    if (gbuf == nullptr) {
        fail(c, "graph buffer allocation failed");
        return false;
    }

    if (ggml_backend_graph_compute(BE, gf) != GGML_STATUS_SUCCESS) {
        ggml_backend_buffer_free(gbuf);
        fail(c, "graph compute failed");
        return false;
    }

    const size_t nbytes = ggml_nbytes(out);
    go.result.resize(nbytes / sizeof(float));
    ggml_backend_tensor_get(out, go.result.data(), 0, nbytes);

    ggml_backend_buffer_free(gbuf);
    return true;
}

// ---------------------------------------------------------------------------
// Comparison

static std::vector<float> host_of(ggml_tensor * t) {
    std::vector<float> v(ggml_nbytes(t) / sizeof(float));
    ggml_backend_tensor_get(t, v.data(), 0, ggml_nbytes(t));
    return v;
}

// Reference value of one build_lora_mm call: base matmul, then one delta per
// applied adapter in port order, each delta scaled by the port scale.
static std::vector<float> reference(
    const Case & cs,
    Native & n,
    const PortRes & pr,
    const std::vector<float> & fs) {

    const int64_t n_in  = cs.w_ne0;
    const int64_t n_out = cs.w_ne1;
    const int64_t rows  = cs.cur_ne1 * cs.cur_ne2 * cs.cur_ne3;

    const std::vector<float> wh = host_of(n.w);
    const std::vector<float> ch = host_of(n.cur);

    std::vector<float> res((size_t) (n_out * rows), 0.0f);
    for (int64_t r = 0; r < rows; ++r) {
        for (int64_t o = 0; o < n_out; ++o) {
            float acc = 0.0f;
            for (int64_t k = 0; k < n_in; ++k) {
                acc += wh[(size_t) (k + n_in * o)] * ch[(size_t) (k + n_in * r)];
            }
            res[(size_t) (o + n_out * r)] = acc;
        }
    }

    for (size_t i = 0; i < pr.active.size(); ++i) {
        if (pr.st[(size_t) (AP_SLOTS * (int64_t) i) + 0] == 0) {
            continue;
        }
        const int ad  = pr.active[i];
        const int64_t pi = pr.st[(size_t) (AP_SLOTS * (int64_t) i) + 2];
        if (pi < 0 || pi >= (int64_t) pr.pair_src.size()) {
            fail(cs.name, "port pair index out of range");
            continue;
        }
        {
            const PSpec & p = cs.pairs[(size_t) pr.pair_src[(size_t) pi]];
            if (p.ad != ad) {
                fail(cs.name, "port pair belongs to another adapter");
                continue;
            }
            const int64_t rank = cs.tensors[(size_t) p.b].ne0;
            const std::vector<float> ah = host_of(n.tensors[(size_t) p.a]);
            const std::vector<float> bh = host_of(n.tensors[(size_t) p.b]);
            std::vector<float> tmp((size_t) (rank * rows), 0.0f);
            for (int64_t r = 0; r < rows; ++r) {
                for (int64_t q = 0; q < rank; ++q) {
                    float acc = 0.0f;
                    for (int64_t k = 0; k < n_in; ++k) {
                        acc += ah[(size_t) (k + n_in * q)] * ch[(size_t) (k + n_in * r)];
                    }
                    tmp[(size_t) (q + rank * r)] = acc;
                }
            }
            const float scale = fs[i];
            for (int64_t r = 0; r < rows; ++r) {
                for (int64_t o = 0; o < n_out; ++o) {
                    float acc = 0.0f;
                    for (int64_t q = 0; q < rank; ++q) {
                        acc += bh[(size_t) (q + rank * o)] * tmp[(size_t) (q + rank * r)];
                    }
                    res[(size_t) (o + n_out * r)] += acc * scale;
                }
            }
        }
    }
    return res;
}

static void compare_case(Case & cs) {
    ++cases;

    std::vector<int> active;
    for (size_t i = 0; i < cs.alpha.size(); ++i) {
        active.push_back((int) i);
    }

    if (cs.port_only) {
        const PortRes pr = run_port(cs, active);
        eq_i64(cs.name, "port.code", cs.alpha.empty() ? -1 : 0, pr.code);
        // The guard is checked before any adapter is applied.
        eq_i64(cs.name, "port.applied", 0, pr.n_app);
        return;
    }

    Native n;
    if (!setup_native(cs, n)) {
        fail(cs.name, "native setup failed");
        n.teardown();
        return;
    }

    // Registry snapshot before any free: model.loras as the port sees it.
    std::vector<int64_t> reg_ids;
    for (size_t i = 0; i < n.ads.size(); ++i) {
        if (n.ads[i] != nullptr && n.owned[i]) {
            reg_ids.push_back(100 + (int64_t) i);
        }
    }

    // Cleanup before the graph, when the case asks for it.
    int64_t cleanup_found_native = -1;
    int64_t cleanup_size_after    = (int64_t) n.model.loras.size();

    if (cs.cleanup_null || cs.cleanup_target >= 0) {
        const size_t before = n.model.loras.size();

        if (cs.cleanup_null) {
            llama_adapter_lora_free(nullptr);
        } else {
            llama_adapter_lora_free(n.ads[(size_t) cs.cleanup_target]);
            n.deleted[(size_t) cs.cleanup_target] = true;
            n.owned[(size_t) cs.cleanup_target]   = false;
            n.ads[(size_t) cs.cleanup_target]     = nullptr;  // object is gone
        }
        const size_t after = n.model.loras.size();
        cleanup_found_native = (int64_t) before - (int64_t) after;
        cleanup_size_after   = (int64_t) after;

        if (cs.cleanup_target >= 0) {
            active.erase(std::remove(active.begin(), active.end(), cs.cleanup_target), active.end());
        }
    }

    const PortRes pr = run_port(cs, active);

    eq_i64(cs.name, "code", 0, pr.code);

    int64_t pairs_seen = 0;
    for (const PSpec & p : cs.pairs) {
        if (std::find(active.begin(), active.end(), p.ad) != active.end()) {
            ++pairs_seen;
        }
    }
    eq_i64(cs.name, "pairs_seen", pairs_seen, pr.npairs);

    // Cleanup contract, compared on the same registry.
    if (cs.cleanup_null || cs.cleanup_target >= 0) {
        std::vector<int64_t> ids = reg_ids;
        std::vector<int64_t> st(3, 0);
        const int64_t target = cs.cleanup_null ? 999 : 100 + (int64_t) cs.cleanup_target;
        const int64_t rc = lora_cleanup(si(ids), (int64_t) ids.size(), target, si(st));
        eq_i64(cs.name, "cleanup.rc", 0, rc);
        eq_i64(cs.name, "cleanup.found", cleanup_found_native, st[0]);
        eq_i64(cs.name, "cleanup.new_n", cleanup_size_after, st[1]);
        values += 1;
        if (st[1] != cleanup_size_after) {
            fail(cs.name, "cleanup size drift");
        }
        // The registry must have lost exactly the target.
        ++values;
        int64_t still = 0;
        for (int64_t id : ids) {
            if (id == target) {
                ++still;
            }
        }
        if (still != 0 || (int64_t) ids.size() < st[1]) {
            fail(cs.name, "cleanup left the target registered");
        }
    }

    // Node budget of every adapter, native get_n_nodes() against the port.
    for (size_t i = 0; i < n.ads.size(); ++i) {
        if (n.ads[i] == nullptr) {
            continue;
        }
        int64_t pairs_native = 0;
        for (const PSpec & p : cs.pairs) {
            if (p.ad == (int) i) {
                ++pairs_native;
            }
        }
        values += 2;
        if (n.ads[i]->get_n_nodes() != (uint32_t) lora_budget(pairs_native)) {
            fail(cs.name, "budget mismatch for adapter " + std::to_string(i) +
                          ": native=" + std::to_string(n.ads[i]->get_n_nodes()) +
                          " port=" + std::to_string(lora_budget(pairs_native)));
        }
        if ((int64_t) n.ads[i]->get_n_nodes() != pairs_native * 6) {
            fail(cs.name, "native budget is not 6 per pair");
        }
    }

    std::vector<bool> act(n.ads.size(), false);
    for (int ad : active) {
        act[(size_t) ad] = true;
    }
    std::vector<float> caller = cs.caller;

    GraphOut go;
    if (!run_graph(n, caller, act, go, cs.name)) {
        n.teardown();
        return;
    }

    eq_i64(cs.name, "applied", pr.n_app, go.n_add);
    eq_i64(cs.name, "lora_nodes", (int64_t) go.n_mm - 1 + go.n_scale + go.n_add, pr.n_nodes);
    eq_i64(cs.name, "mm_nodes", 1 + 2 * pr.n_app, go.n_mm);
    eq_i64(cs.name, "scale_nodes", pr.n_app, go.n_scale);
    eq_i64(cs.name, "final_op", pr.n_app > 0 ? (int64_t) GGML_OP_ADD : (int64_t) GGML_OP_MUL_MAT,
           (int64_t) go.final_op);

    // Per applied adapter: scale bits and delta geometry.
    std::vector<float> port_scales;
    std::vector<std::array<int64_t, 4>> port_delta;
    for (size_t i = 0; i < pr.active.size(); ++i) {
        const int64_t applied = pr.st[(size_t) (AP_SLOTS * (int64_t) i) + 0];
        const int64_t pair    = pr.st[(size_t) (AP_SLOTS * (int64_t) i) + 2];
        if (applied == 0) {
            eq_i64(cs.name, "skip.pair", -1, pair);
            eq_i64(cs.name, "skip.reason", 1, pr.st[(size_t) (AP_SLOTS * (int64_t) i) + 1]);
            eq_i64(cs.name, "skip.ne0", 0, pr.st[(size_t) (AP_SLOTS * (int64_t) i) + 3]);
            eq_i64(cs.name, "skip.ne1", 0, pr.st[(size_t) (AP_SLOTS * (int64_t) i) + 4]);
            continue;
        }
        eq_i64(cs.name, "apply.reason", 0, pr.st[(size_t) (AP_SLOTS * (int64_t) i) + 1]);
        if (pair < 0) {
            fail(cs.name, "applied adapter without a pair index");
            continue;
        }
        port_scales.push_back(pr.fs[i]);
        port_delta.push_back({ pr.st[(size_t) (AP_SLOTS * (int64_t) i) + 3],
                               pr.st[(size_t) (AP_SLOTS * (int64_t) i) + 4],
                               pr.dne2, pr.dne3 });
    }

    // The port order and the native unordered_map order need not agree, so the
    // scale multiset and the delta shape set are compared, not their order.
    eq_i64(cs.name, "scales.count", (int64_t) port_scales.size(), (int64_t) go.scales.size());
    if (port_scales.size() == 1 && go.scales.size() == 1) {
        // Single adapter: the native SCALE constant and the port scale must be
        // the same float, bit for bit.
        eq_f32_bits(cs.name, "scale", go.scales[0], port_scales[0]);
    }
    std::vector<char> used(go.scales.size(), 0);
    for (float s : port_scales) {
        bool hit = false;
        for (size_t j = 0; j < go.scales.size(); ++j) {
            if (used[j] == 0 && memcmp(&s, &go.scales[j], sizeof(float)) == 0) {
                used[j] = 1;
                hit = true;
                break;
            }
        }
        ++values;
        if (!hit) {
            fail(cs.name, "no native scale node with the port value " + std::to_string(s));
        }
    }

    for (const auto & d : port_delta) {
        bool hit = false;
        for (const auto & nd : go.delta_ne) {
            if (nd[0] == d[0] && nd[1] == d[1] && nd[2] == d[2] && nd[3] == d[3]) {
                hit = true;
                break;
            }
        }
        ++values;
        if (!hit) {
            fail(cs.name, "no native delta with shape " + std::to_string(d[0]) + "x" +
                          std::to_string(d[1]) + "x" + std::to_string(d[2]) + "x" + std::to_string(d[3]));
        }
    }

    // Arithmetic: the port's scales and selection, the native's numbers.
    const std::vector<float> want = reference(cs, n, pr, pr.fs);
    eq_i64(cs.name, "tensor.size", (int64_t) want.size(), (int64_t) go.result.size());
    if (want.size() == go.result.size()) {
        int shown = 0;
        for (size_t i = 0; i < want.size(); ++i) {
            ++values;
            if (memcmp(&want[i], &go.result[i], sizeof(float)) != 0) {
                if (shown < 4) {
                    fail(cs.name, "value[" + std::to_string(i) + "] native=" +
                                  std::to_string(go.result[i]) + " port=" + std::to_string(want[i]));
                    ++shown;
                } else {
                    ++fails;
                }
            }
        }
    }

    n.teardown();
}

// ---------------------------------------------------------------------------
// Cases

static Case make_case(const std::string & name, int64_t n_in, int64_t n_out, int64_t rank) {
    Case c;
    c.name  = name;
    c.w_ne0 = n_in;
    c.w_ne1 = n_out;
    return c;
}

// One adapter tensor pair under a base name.
static void add_pair(Case & c, int ad, const std::string & base, int64_t n_in, int64_t n_out, int64_t rank) {
    TS a;
    a.name = base + ".lora_a";
    a.ne0  = n_in;
    a.ne1  = rank;
    TS b;
    b.name = base + ".lora_b";
    b.ne0  = rank;
    b.ne1  = n_out;

    PSpec p;
    p.ad   = ad;
    p.name = base;
    p.a    = (int) c.tensors.size();
    c.tensors.push_back(a);
    p.b = (int) c.tensors.size();
    c.tensors.push_back(b);
    c.pairs.push_back(p);
}

static void add_adapter(Case & c, float alpha, float caller, bool registered) {
    c.alpha.push_back(alpha);
    c.caller.push_back(caller);
    c.registered.push_back(registered);
}

int main() {
    goose_init(0, nullptr);
    ggml_backend_load_all();

    DEV = ggml_backend_dev_by_type(GGML_BACKEND_DEVICE_TYPE_CPU);
    if (DEV == nullptr) {
        printf("FG605_LORA no CPU backend\n");
        return 2;
    }
    BE = ggml_backend_dev_init(DEV, nullptr);
    if (BE == nullptr) {
        printf("FG605_LORA no CPU backend\n");
        return 2;
    }

    // 1. One adapter, alpha 2 over rank 2: scale is the caller scale.
    {
        Case c = make_case("c01_single", 4, 3, 2);
        add_adapter(c, 2.0f, 1.0f, true);
        add_pair(c, 0, c.wname, 4, 3, 2);
        compare_case(c);
    }

    // 2. alpha == 0 keeps the caller scale (0.5).
    {
        Case c = make_case("c02_alpha_zero", 4, 3, 2);
        add_adapter(c, 0.0f, 0.5f, true);
        add_pair(c, 0, c.wname, 4, 3, 2);
        compare_case(c);
    }

    // 3. Non-representable scale: alpha 0.1, caller 1.7, rank 3.
    {
        Case c = make_case("c03_scale_bits", 4, 2, 3);
        add_adapter(c, 0.1f, 1.7f, true);
        add_pair(c, 0, c.wname, 4, 2, 3);
        compare_case(c);
    }

    // 4. The adapter has no pair for this weight: base matmul only.
    {
        Case c = make_case("c04_no_weight", 4, 3, 2);
        add_adapter(c, 2.0f, 1.0f, true);
        add_pair(c, 0, "blk.0.attn_k.weight", 4, 3, 2);
        compare_case(c);
    }

    // 5. Two pairs in one adapter, one of them for this weight.
    {
        Case c = make_case("c05_pair_select", 4, 3, 2);
        add_adapter(c, 2.0f, 1.0f, true);
        add_pair(c, 0, "blk.0.attn_k.weight", 4, 3, 2);
        add_pair(c, 0, c.wname, 4, 3, 2);
        compare_case(c);
    }

    // 6. Two adapters on the same weight: two deltas, two scales.
    {
        Case c = make_case("c06_two_adapters", 4, 3, 2);
        add_adapter(c, 2.0f, 1.0f, true);
        add_adapter(c, 4.0f, 0.5f, true);
        add_pair(c, 0, c.wname, 4, 3, 2);
        add_pair(c, 1, c.wname, 4, 3, 2);
        compare_case(c);
    }

    // 7. Batch of four tokens.
    {
        Case c = make_case("c07_batch", 4, 3, 2);
        c.cur_ne1 = 4;
        add_adapter(c, 2.0f, 1.0f, true);
        add_pair(c, 0, c.wname, 4, 3, 2);
        compare_case(c);
    }

    // 8. 4D activation: [4, 2, 3, 2].
    {
        Case c = make_case("c08_batch_4d", 4, 3, 2);
        c.cur_ne1 = 2;
        c.cur_ne2 = 3;
        c.cur_ne3 = 2;
        add_adapter(c, 2.0f, 1.0f, true);
        add_pair(c, 0, c.wname, 4, 3, 2);
        compare_case(c);
    }

    // 9. Rank 1.
    {
        Case c = make_case("c09_rank1", 4, 3, 1);
        add_adapter(c, 3.0f, 1.0f, true);
        add_pair(c, 0, c.wname, 4, 3, 1);
        compare_case(c);
    }

    // 10. Negative caller scale and negative alpha.
    {
        Case c = make_case("c10_negative", 4, 3, 2);
        add_adapter(c, -2.0f, -0.75f, true);
        add_pair(c, 0, c.wname, 4, 3, 2);
        compare_case(c);
    }

    // 11. Rank 3 with a 2-token batch and one skipped adapter.
    {
        Case c = make_case("c11_mixed", 5, 4, 3);
        c.cur_ne1 = 2;
        add_adapter(c, 1.5f, 0.25f, true);
        add_adapter(c, 2.0f, 1.0f, true);
        add_pair(c, 0, c.wname, 5, 4, 3);
        add_pair(c, 1, "output.weight", 5, 4, 3);
        compare_case(c);
    }

    // 12. Scalar geometry: n_embd 1, one output row, rank 1.
    {
        Case c = make_case("c12_scalar", 1, 1, 1);
        add_adapter(c, 1.0f, 2.0f, true);
        add_pair(c, 0, c.wname, 1, 1, 1);
        compare_case(c);
    }

    // 13. Rank 0: native divides by zero into an inf scale, the port refuses.
    {
        Case c = make_case("c13_rank0", 4, 3, 0);
        c.port_only = true;
        add_adapter(c, 2.0f, 1.0f, true);
        add_pair(c, 0, c.wname, 4, 3, 0);
        const PortRes pr = run_port(c, { 0 });
        eq_i64(c.name, "rank0.code", -3, pr.code);
        eq_i64(c.name, "rank0.applied", 0, pr.n_app);
        ++cases;
    }

    // 14. b.ne1 differs from the weight output: native would broadcast or
    //     assert inside ggml_add, the port refuses (loader shape rule).
    {
        Case c = make_case("c14_bad_out", 4, 6, 2);
        c.port_only = true;
        add_adapter(c, 2.0f, 1.0f, true);
        add_pair(c, 0, c.wname, 4, 3, 2);
        const PortRes pr = run_port(c, { 0 });
        eq_i64(c.name, "bad_out.code", -2, pr.code);
        eq_i64(c.name, "bad_out.applied", 0, pr.n_app);
        ++cases;
    }

    // 15. Cleanup of a registered adapter: it is unregistered and the graph
    //     then applies only the survivor.
    {
        Case c = make_case("c15_cleanup", 4, 3, 2);
        add_adapter(c, 2.0f, 1.0f, true);
        add_adapter(c, 4.0f, 0.5f, true);
        add_pair(c, 0, c.wname, 4, 3, 2);
        add_pair(c, 1, c.wname, 4, 3, 2);
        c.cleanup_target = 0;
        compare_case(c);
    }

    // 16. Cleanup of an adapter that was never registered: the registry does
    //     not change, but the object is still released by native free.
    {
        Case c = make_case("c16_cleanup_unregistered", 4, 3, 2);
        add_adapter(c, 2.0f, 1.0f, true);
        add_adapter(c, 4.0f, 0.5f, false);
        add_pair(c, 0, c.wname, 4, 3, 2);
        add_pair(c, 1, c.wname, 4, 3, 2);
        c.cleanup_target = 1;
        compare_case(c);
    }

    // 17. free(nullptr) is a no-op for the registry.
    {
        Case c = make_case("c17_cleanup_null", 4, 3, 2);
        add_adapter(c, 2.0f, 1.0f, true);
        add_pair(c, 0, c.wname, 4, 3, 2);
        c.cleanup_null = true;
        compare_case(c);
    }

    // 18. Cleanup of an unknown id: nothing is removed.
    {
        Case c = make_case("c18_cleanup_unknown", 4, 3, 2);
        add_adapter(c, 2.0f, 1.0f, true);
        add_pair(c, 0, c.wname, 4, 3, 2);
        c.cleanup_target = 0;
        // The adapter is registered, so free removes it; the unknown-id path is
        // exercised separately on the same registry below.
        compare_case(c);

        Case d = make_case("c18b_cleanup_unknown_id", 4, 3, 2);
        add_adapter(d, 2.0f, 1.0f, true);
        add_pair(d, 0, d.wname, 4, 3, 2);
        Native nd;
        if (!setup_native(d, nd)) {
            fail(d.name, "native setup failed");
        } else {
            ++cases;
            std::vector<int64_t> ids = { 100 };
            std::vector<int64_t> st(3, 0);
            const int64_t rc = lora_cleanup(si(ids), 1, 555, si(st));
            eq_i64(d.name, "unknown.rc", 0, rc);
            eq_i64(d.name, "unknown.found", 0, st[0]);
            eq_i64(d.name, "unknown.size", 1, st[1]);
            eq_i64(d.name, "unknown.kept", 100, ids[0]);
            eq_i64(d.name, "native.size", (int64_t) nd.model.loras.size(), 1);
            nd.teardown();
        }
    }

    // 19. Two adapters, 4D activation [4, 3, 2, 2]; the third dimension is the
    //     one a shape mutation is most likely to drop.
    {
        Case c = make_case("c19_batch_4d_two", 4, 3, 2);
        c.cur_ne1 = 3;
        c.cur_ne2 = 2;
        c.cur_ne3 = 2;
        add_adapter(c, 2.0f, 1.0f, true);
        add_adapter(c, 3.0f, 0.5f, true);
        add_pair(c, 0, c.wname, 4, 3, 2);
        add_pair(c, 1, c.wname, 4, 3, 2);
        compare_case(c);
    }

    // 20. No adapter at all: build_lora_mm is the base matmul.
    {
        Case c = make_case("c20_no_adapter", 4, 3, 2);
        c.cur_ne1 = 2;
        compare_case(c);
    }

    printf("FG605_LORA cases=%d values=%d failures=%d\n", cases, values, fails);

    ggml_backend_free(BE);
    return fails == 0 ? 0 : 1;
}
