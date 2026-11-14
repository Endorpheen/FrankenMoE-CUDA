// Differential gate for P6-09 S3, control vectors (EXP578).
//
// Oracle strategy: the real llama_adapter_cvec methods are executed against
// 32 x 4096 F32 direction tensors allocated in a CPU backend buffer. No model
// weights and no GGUF are involved.
//
// llama_model is abstract in this tree (src/llama-model.h:708-920, seven pure
// virtuals: load_stats, load_hparams, load_vocab, load_tensors,
// load_arch_hparams, load_arch_tensors, build_arch_graph), so the oracle cannot
// hold one by value. TestModel below implements the seven virtuals as unused
// stubs; the native apply/apply_to/tensor_for never call them. The object is an
// hparams carrier only (n_layer, n_embd), the same role the concrete-model
// version played by writing those fields by hand.
//
// A vocab-only GGUF cannot replace the carrier either: llama_model::load_hparams
// returns before n_embd/n_layer are read when hparams.vocab_only is set
// (src/llama-model.cpp:1380-1382), and llama_model::select_buft needs
// pimpl->dev_layer, which only a real load populates. The fixture therefore
// builds the adapter's tensor table the way init() would and drives the native
// apply/apply_to/tensor_for directly. `private` is mapped to `public` only
// around llama-adapter.h so the test can install that table; this is the same
// class of white-box shim the native unit tests use for private state.
//
// For every layer the gate compares:
//   - presence: native tensor_for(il) != nullptr vs port direction_off(il) >= 0;
//   - the direction floats read back with ggml_backend_tensor_get;
//   - apply_to: pointer identity when absent, a new ADD node with the same
//     [n_embd, rows] shape when present, and the broadcasted numeric result.
//
// Output: FG578_CV cases=N values=M failures=K
// Exit: 0 ok, 1 failures, 3 native setup failed.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <exception>
#include <string>
#include <unordered_map>
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

#include "runtime-gen.h"

static int cases = 0;
static int values = 0;
static int fails = 0;

static int64_t N_LAYER = 32;
static int64_t N_EMBD  = 4096;

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

static void eq_bool(const std::string & c, const char * what, bool native, bool port) {
    ++values;
    if (native != port) {
        fail(c, std::string(what) + ": native=" + (native ? "true" : "false") +
                    " port=" + (port ? "true" : "false"));
    }
}

static void eq_bytes(const std::string & c, const char * what, const void * a, const void * b, size_t n) {
    ++values;
    if (memcmp(a, b, n) != 0) {
        fail(c, std::string(what) + ": " + std::to_string(n) + " bytes differ");
    }
}

static sl_i64 si(std::vector<int64_t> & v) {
    return sl_i64{ v.data(), (int64_t) v.size() };
}

static sl_f32 sf(std::vector<float> & v) {
    return sl_f32{ v.data(), (int64_t) v.size() };
}

static sl_f32 sfc(const std::vector<float> & v) {
    return sl_f32{ (float *) v.data(), (int64_t) v.size() };
}

// ---------------------------------------------------------------------------
// Native adapter with a CPU-backed direction table, no model load.

// llama_model is abstract in this tree (seven pure virtuals), so the oracle
// cannot hold one by value. This test-only subclass implements all seven as
// unused stubs: only hparams (n_embd/n_layer) is read by apply/apply_to/
// tensor_for, and select_buft/init are never reached because the tensor table
// is installed directly below. No weight, no GGUF and no network is involved.
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

struct NativeCv {
    TestModel         model;
    llama_adapter_cvec cv;
    bool              ok = false;

    NativeCv() : model(llama_model_default_params()) {
        model.hparams.n_layer_all  = (uint32_t) N_LAYER;
        model.hparams.n_layer_nextn = 0;
        model.hparams.n_embd       = (uint32_t) N_EMBD;
        model.hparams.vocab_only   = false;

        const size_t overhead = (size_t) N_LAYER * (size_t) ggml_tensor_overhead();
        ggml_init_params ip = { overhead + (1u << 20), nullptr, true };
        ggml_context * ctx = ggml_init(ip);
        if (ctx == nullptr) {
            return;
        }

        for (int64_t il = 0; il < N_LAYER; ++il) {
            cv.tensors.push_back(il == 0 ? nullptr : ggml_new_tensor_1d(ctx, GGML_TYPE_F32, N_EMBD));
        }

        ggml_backend_dev_t cpu = ggml_backend_dev_by_type(GGML_BACKEND_DEVICE_TYPE_CPU);
        ggml_backend_buffer_type_t buft = cpu == nullptr ? nullptr : ggml_backend_dev_buffer_type(cpu);
        if (buft == nullptr) {
            ggml_free(ctx);
            return;
        }
        ggml_backend_buffer_t buf = ggml_backend_alloc_ctx_tensors_from_buft(ctx, buft);
        if (buf == nullptr) {
            ggml_free(ctx);
            return;
        }
        ggml_backend_buffer_clear(buf, 0);

        cv.ctxs.emplace_back(ctx);
        cv.bufs.emplace_back(buf);
        cv.layer_start = -1;
        cv.layer_end   = -1;
        ok = true;
    }
};

// ---------------------------------------------------------------------------
// Port state

struct Port {
    std::vector<int64_t> S;
    std::vector<int64_t> T;
    std::vector<float>   F;

    Port() :
        S(8, 0),
        T((size_t) (3 * N_LAYER), 0),
        F((size_t) (N_LAYER * N_EMBD), 0.0f) {}
};

static void port_reset(Port & p) {
    std::fill(p.S.begin(), p.S.end(), 0);
    std::fill(p.T.begin(), p.T.end(), 0);

    // Seed the pool with garbage before cv_init. The native init() zeroes every
    // direction it owns, so a gate that starts from an already-zero pool cannot
    // tell a missing clear apart from a correct one (mutation A2).
    for (size_t i = 0; i < p.F.size(); ++i) {
        p.F[i] = (float) (i % 13) + 0.25f;
    }

    const int64_t rc = cv_init_state(si(p.S), si(p.T), N_LAYER, N_EMBD);
    eq_i64("setup", "cv_init_state", 0, rc);
}

static int64_t port_apply(Port & p, const std::vector<float> & data, int64_t n_embd, int64_t s, int64_t e) {
    return cv_apply(si(p.S), si(p.T), sf(p.F), sfc(data), n_embd, s, e);
}

// ---------------------------------------------------------------------------
// Native helpers

static bool native_apply(
    llama_adapter_cvec & cv,
    llama_model & model,
    const float * data,
    size_t len,
    int64_t n_embd,
    int64_t start,
    int64_t end,
    const std::string & c) {

    try {
        return cv.apply(model, data, len, (int32_t) n_embd, (int32_t) start, (int32_t) end);
    } catch (const std::exception & e) {
        fail(c, std::string("native apply threw: ") + e.what());
        return false;
    }
}

static bool native_disable(llama_adapter_cvec & cv, llama_model & model, const std::string & c) {
    return native_apply(cv, model, nullptr, 0, 0, 0, 0, c);
}

// ---------------------------------------------------------------------------
// Data

static std::vector<float> make_data(int64_t len, int64_t seed) {
    std::vector<float> d((size_t) len);

    for (int64_t i = 0; i < len; ++i) {
        d[(size_t) i] = (float) ((i * 7 + seed * 13) % 97) * 0.5f - 24.0f;
    }
    return d;
}

static std::vector<float> make_rows(int64_t rows) {
    std::vector<float> d((size_t) (rows * N_EMBD));

    for (int64_t r = 0; r < rows; ++r) {
        for (int64_t k = 0; k < N_EMBD; ++k) {
            d[(size_t) (r * N_EMBD + k)] = (float) ((r * 7 + k) % 31) * 0.25f - 2.0f;
        }
    }
    return d;
}

// ---------------------------------------------------------------------------
// Comparison

static void check_layer(
    const std::string & c,
    llama_adapter_cvec & cv,
    Port & p,
    int64_t il,
    int64_t rows) {

    ggml_tensor * dir = cv.tensor_for((int) il);
    const int64_t off = cv_direction_off(si(p.S), si(p.T), il);

    eq_bool(c, "present", dir != nullptr, off >= 0);

    std::vector<float> native_dir((size_t) N_EMBD, 0.0f);

    if (dir != nullptr && off >= 0) {
        ggml_backend_tensor_get(dir, native_dir.data(), 0, (size_t) (N_EMBD * sizeof(float)));
        eq_bytes(c, "direction", native_dir.data(), p.F.data() + off, (size_t) (N_EMBD * sizeof(float)));
    }

    const std::vector<float> host0 = make_rows(rows);

    ggml_init_params ip = { (size_t) (1u << 20), nullptr, false };
    ggml_context * ctx = ggml_init(ip);
    ggml_tensor * cur = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, N_EMBD, rows);
    memcpy(cur->data, host0.data(), host0.size() * sizeof(float));

    ggml_tensor * res = cv.apply_to(ctx, cur, (int) il);

    std::vector<float> host = host0;
    const int64_t rc = cv_apply_to(si(p.S), si(p.T), sfc(p.F), sf(host), il);

    if (dir == nullptr) {
        eq_bool(c, "apply_to.same_pointer", res == cur, true);
        eq_i64(c, "apply_to.rc", 0, rc);
        eq_bytes(c, "apply_to.values", host0.data(), host.data(), host0.size() * sizeof(float));
    } else {
        eq_bool(c, "apply_to.new_node", res != cur, true);
        eq_i64(c, "apply_to.op", GGML_OP_ADD, (int64_t) res->op);
        eq_i64(c, "apply_to.ne0", N_EMBD, (int64_t) res->ne[0]);
        eq_i64(c, "apply_to.ne1", rows, (int64_t) res->ne[1]);
        eq_i64(c, "apply_to.rc", 0, rc);

        std::vector<float> want = host0;

        for (int64_t r = 0; r < rows; ++r) {
            for (int64_t k = 0; k < N_EMBD; ++k) {
                want[(size_t) (r * N_EMBD + k)] += native_dir[(size_t) k];
            }
        }
        eq_bytes(c, "apply_to.broadcast", want.data(), host.data(), want.size() * sizeof(float));
    }
    ggml_free(ctx);
}

static void check_state(
    const std::string & c,
    llama_adapter_cvec & cv,
    Port & p,
    int64_t exp_start,
    int64_t exp_end,
    int64_t rows) {

    eq_i64(c, "layer_start", exp_start, p.S[2]);
    eq_i64(c, "layer_end", exp_end, p.S[3]);

    for (int64_t il = -1; il < N_LAYER; ++il) {
        check_layer(c, cv, p, il, rows);
    }
}

// ---------------------------------------------------------------------------
// Cases

// 1. Disabled range: apply_to is a no-op for every layer.
static void c01_default_state() {
    ++cases;
    const std::string c = "c01_default_state";

    NativeCv ncv;
    Port p;
    port_reset(p);
    if (!ncv.ok) {
        fail(c, "native setup failed");
        return;
    }

    check_state(c, ncv.cv, p, -1, -1, 2);
}

// 2. Full apply: all layers 1..n-1, three rows, broadcast.
static void c02_full_apply() {
    ++cases;
    const std::string c = "c02_full_apply";

    NativeCv ncv;
    Port p;
    port_reset(p);
    if (!ncv.ok) {
        fail(c, "native setup failed");
        return;
    }
    llama_adapter_cvec & cv = ncv.cv;

    const std::vector<float> data = make_data((N_LAYER - 1) * N_EMBD, 1);

    eq_bool(c, "native.apply", true, native_apply(cv, ncv.model, data.data(), data.size(), N_EMBD, 1, N_LAYER - 1, c));
    eq_i64(c, "port.apply", 0, port_apply(p, data, N_EMBD, 1, N_LAYER - 1));

    check_state(c, cv, p, 1, N_LAYER - 1, 3);
}

// 3. Data covers three layers; the rest stay zero but are still present.
static void c03_three_layers() {
    ++cases;
    const std::string c = "c03_three_layers";

    NativeCv ncv;
    Port p;
    port_reset(p);
    if (!ncv.ok) {
        fail(c, "native setup failed");
        return;
    }
    llama_adapter_cvec & cv = ncv.cv;

    const std::vector<float> data = make_data(3 * N_EMBD, 2);

    eq_bool(c, "native.apply", true, native_apply(cv, ncv.model, data.data(), data.size(), N_EMBD, 1, N_LAYER - 1, c));
    eq_i64(c, "port.apply", 0, port_apply(p, data, N_EMBD, 1, N_LAYER - 1));

    check_state(c, cv, p, 1, N_LAYER - 1, 2);
}

// 4. Length stops seven floats before the last layer: that layer is not copied.
static void c04_short_nonmultiple() {
    ++cases;
    const std::string c = "c04_short_nonmultiple";

    NativeCv ncv;
    Port p;
    port_reset(p);
    if (!ncv.ok) {
        fail(c, "native setup failed");
        return;
    }
    llama_adapter_cvec & cv = ncv.cv;

    const std::vector<float> data = make_data((N_LAYER - 1) * N_EMBD - 7, 3);

    eq_bool(c, "native.apply", true, native_apply(cv, ncv.model, data.data(), data.size(), N_EMBD, 1, N_LAYER - 1, c));
    eq_i64(c, "port.apply", 0, port_apply(p, data, N_EMBD, 1, N_LAYER - 1));

    check_state(c, cv, p, 1, N_LAYER - 1, 2);
}

// 5. Disable keeps the buffers but clears the range; re-enable a sub-range.
static void c05_disable_reenable() {
    ++cases;
    const std::string c = "c05_disable_reenable";

    NativeCv ncv;
    Port p;
    port_reset(p);
    if (!ncv.ok) {
        fail(c, "native setup failed");
        return;
    }
    llama_adapter_cvec & cv = ncv.cv;

    const std::vector<float> data = make_data((N_LAYER - 1) * N_EMBD, 4);

    eq_bool(c, "native.apply", true, native_apply(cv, ncv.model, data.data(), data.size(), N_EMBD, 1, N_LAYER - 1, c));
    eq_i64(c, "port.apply", 0, port_apply(p, data, N_EMBD, 1, N_LAYER - 1));

    eq_bool(c, "native.disable", true, native_disable(cv, ncv.model, c));
    eq_i64(c, "port.disable", 0, cv_disable(si(p.S)));

    check_state(c + ".disabled", cv, p, -1, -1, 2);

    eq_bool(c, "native.reenable", true, native_apply(cv, ncv.model, data.data(), data.size(), N_EMBD, 5, 7, c));
    eq_i64(c, "port.reenable", 0, port_apply(p, data, N_EMBD, 5, 7));

    check_state(c + ".reenabled", cv, p, 5, 7, 3);
}

// 6. n_embd mismatch: refused before any state change, with and without data.
static void c06_n_embd_mismatch() {
    ++cases;
    const std::string c = "c06_n_embd_mismatch";

    {
        NativeCv ncv;
        Port p;
        port_reset(p);
        if (!ncv.ok) {
            fail(c, "native setup failed");
            return;
        }
        llama_adapter_cvec & cv = ncv.cv;

        const std::vector<float> data = make_data((N_LAYER - 1) * N_EMBD, 5);

        eq_bool(c, "native.apply", true, native_apply(cv, ncv.model, data.data(), data.size(), N_EMBD, 1, 3, c));
        eq_i64(c, "port.apply", 0, port_apply(p, data, N_EMBD, 1, 3));

        const std::vector<float> bad((size_t) (N_EMBD - 1), 1.0f);

        eq_bool(c, "native.mismatch", false,
                native_apply(cv, ncv.model, bad.data(), bad.size(), N_EMBD - 1, 9, 9, c));
        eq_i64(c, "port.mismatch", -1, port_apply(p, bad, N_EMBD - 1, 9, 9));

        check_state(c, cv, p, 1, 3, 2);
    }

    {
        NativeCv ncv;
        Port p;
        port_reset(p);
        if (!ncv.ok) {
            fail(c, "native setup failed");
            return;
        }
        llama_adapter_cvec & cv = ncv.cv;

        const std::vector<float> bad((size_t) (N_EMBD - 1), 2.0f);

        eq_bool(c, "native.fresh_mismatch", false,
                native_apply(cv, ncv.model, bad.data(), bad.size(), N_EMBD - 1, 1, 1, c));
        eq_i64(c, "port.fresh_mismatch", -1, port_apply(p, bad, N_EMBD - 1, 1, 1));

        check_state(c + ".fresh", cv, p, -1, -1, 2);
    }
}

// 7. Single-layer range and reversed range; copies still happen for all layers.
static void c07_ranges() {
    ++cases;
    const std::string c = "c07_ranges";

    NativeCv ncv;
    Port p;
    port_reset(p);
    if (!ncv.ok) {
        fail(c, "native setup failed");
        return;
    }
    llama_adapter_cvec & cv = ncv.cv;

    const std::vector<float> data = make_data((N_LAYER - 1) * N_EMBD, 6);

    eq_bool(c, "native.single", true, native_apply(cv, ncv.model, data.data(), data.size(), N_EMBD, 10, 10, c));
    eq_i64(c, "port.single", 0, port_apply(p, data, N_EMBD, 10, 10));

    check_state(c + ".single", cv, p, 10, 10, 2);

    eq_bool(c, "native.reversed", true, native_apply(cv, ncv.model, data.data(), data.size(), N_EMBD, 20, 10, c));
    eq_i64(c, "port.reversed", 0, port_apply(p, data, N_EMBD, 20, 10));

    check_state(c + ".reversed", cv, p, 20, 10, 2);
}

// 8. Re-apply a shorter vector: the prefix updates, the tail keeps old data.
static void c08_reapply_shorter() {
    ++cases;
    const std::string c = "c08_reapply_shorter";

    NativeCv ncv;
    Port p;
    port_reset(p);
    if (!ncv.ok) {
        fail(c, "native setup failed");
        return;
    }
    llama_adapter_cvec & cv = ncv.cv;

    const std::vector<float> data_a = make_data((N_LAYER - 1) * N_EMBD, 7);
    const std::vector<float> data_b = make_data(2 * N_EMBD, 8);

    eq_bool(c, "native.first", true, native_apply(cv, ncv.model, data_a.data(), data_a.size(), N_EMBD, 1, N_LAYER - 1, c));
    eq_i64(c, "port.first", 0, port_apply(p, data_a, N_EMBD, 1, N_LAYER - 1));

    eq_bool(c, "native.second", true, native_apply(cv, ncv.model, data_b.data(), data_b.size(), N_EMBD, 1, N_LAYER - 1, c));
    eq_i64(c, "port.second", 0, port_apply(p, data_b, N_EMBD, 1, N_LAYER - 1));

    check_state(c, cv, p, 1, N_LAYER - 1, 3);
}

// 9. Non-null pointer with len 0: range is set, all directions stay zero.
static void c09_zero_len() {
    ++cases;
    const std::string c = "c09_zero_len";

    NativeCv ncv;
    Port p;
    port_reset(p);
    if (!ncv.ok) {
        fail(c, "native setup failed");
        return;
    }
    llama_adapter_cvec & cv = ncv.cv;

    float dummy = 0.0f;

    eq_bool(c, "native.apply", true, native_apply(cv, ncv.model, &dummy, 0, N_EMBD, 1, N_LAYER - 1, c));
    eq_i64(c, "port.apply", 0, port_apply(p, std::vector<float>(), N_EMBD, 1, N_LAYER - 1));

    check_state(c, cv, p, 1, N_LAYER - 1, 2);
}

// 10. Longer data with a negative start: all layers copied, only 1..2 selected.
static void c10_long_negative_start() {
    ++cases;
    const std::string c = "c10_long_negative_start";

    NativeCv ncv;
    Port p;
    port_reset(p);
    if (!ncv.ok) {
        fail(c, "native setup failed");
        return;
    }
    llama_adapter_cvec & cv = ncv.cv;

    const std::vector<float> data = make_data((N_LAYER - 1) * N_EMBD + N_EMBD, 9);

    eq_bool(c, "native.apply", true, native_apply(cv, ncv.model, data.data(), data.size(), N_EMBD, -5, 2, c));
    eq_i64(c, "port.apply", 0, port_apply(p, data, N_EMBD, -5, 2));

    check_state(c, cv, p, -5, 2, 2);
}

// Port-only contract checks, including inputs the native class cannot accept.
static void contract() {
    const std::string c = "contract";

    Port p;
    port_reset(p);

    eq_i64(c, "init_first", 0, cv_init(si(p.S), si(p.T), sf(p.F)));
    eq_i64(c, "init_twice", -3, cv_init(si(p.S), si(p.T), sf(p.F)));

    const std::vector<float> data = make_data((N_LAYER - 1) * N_EMBD, 10);
    eq_i64(c, "apply", 0, port_apply(p, data, N_EMBD, 1, N_LAYER - 1));

    std::vector<float> bad_cur((size_t) (N_EMBD + 1), 0.0f);
    eq_i64(c, "cur_shape", -1, cv_apply_to(si(p.S), si(p.T), sfc(p.F), sf(bad_cur), 1));
    eq_i64(c, "negative_il", -1, cv_direction_off(si(p.S), si(p.T), -1));

    {
        std::vector<int64_t> S2(8, 0);
        std::vector<int64_t> T2((size_t) (3 * N_LAYER - 1), 0);
        eq_i64(c, "store_small", -1, cv_init_state(si(S2), si(T2), N_LAYER, N_EMBD));
    }
    {
        std::vector<int64_t> S2(8, 0);
        std::vector<int64_t> T2((size_t) (3 * N_LAYER), 0);
        eq_i64(c, "n_embd_zero", -1, cv_init_state(si(S2), si(T2), N_LAYER, 0));
    }
}

int main(int argc, char ** argv) {
    if (argc > 1) {
        N_LAYER = atoll(argv[1]);
    }
    if (argc > 2) {
        N_EMBD = atoll(argv[2]);
    }

    llama_backend_init();

    ggml_backend_dev_t cpu = ggml_backend_dev_by_type(GGML_BACKEND_DEVICE_TYPE_CPU);
    if (cpu == nullptr) {
        fprintf(stderr, "FG578_CV no CPU backend\n");
        llama_backend_free();
        return 3;
    }

    goose_init(0, nullptr);

    c01_default_state();
    c02_full_apply();
    c03_three_layers();
    c04_short_nonmultiple();
    c05_disable_reenable();
    c06_n_embd_mismatch();
    c07_ranges();
    c08_reapply_shorter();
    c09_zero_len();
    c10_long_negative_start();
    contract();

    printf("FG578_CV cases=%d values=%d failures=%d\n", cases, values, fails);

    llama_backend_free();
    return fails == 0 ? 0 : 1;
}
