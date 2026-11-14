// Differential gate for P6-09 S4, the llama_model_saver writer path.
//
// One binary, two implementations of the same scenario:
//   - native: llama_model_saver (src/llama-model-saver.cpp) over libllama,
//     constructed either with an arch and an empty GGUF context or with a
//     TestModel that only carries arch and hparams.n_layer_all;
//   - port:   sav_* from source/model-saver.goose over the accepted GGUF
//     writer/tensors port (S1 + S2 + S3).
//
// Both sides receive the same overload calls with the same values and the same
// resolved metadata key name (LLM_KV naming stays native). Every compared case
// checks:
//   - the canonical KV dump (key, type, array element type, count, bytes);
//   - the tensor descriptor dump (name, type, size, offset, ne[0..3]);
//   - the full serialized file from save(FILE *);
//   - the port bytes reloaded through gguf_init_from_file, comparing counts,
//     KV and descriptor dumps again.
//
// Per-layer calls need a model natively (GGML_ASSERT), so those cases use the
// model constructor. Cases that native answers with an abort (char overload,
// duplicate non-rope tensor, per-layer without a model or with a short
// container) are checked on the port side only and marked port_only.
//
// Output: FG_S4 cases=N checks=M failures=K
// Exit:   0 ok, 1 failures, 3 native setup failed.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ggml.h"
#include "gguf.h"
#include "llama.h"
#include "llama-arch.h"
#include "llama-model.h"
#include "llama-model-saver.h"

#include "model_saver.h"

// ---------------------------------------------------------------- test model

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

// ---------------------------------------------------------------- case model

enum OpKind { OP_U32, OP_I32, OP_U64, OP_F32, OP_BOOL, OP_STR, OP_ARR, OP_STRS };

struct Op {
    OpKind        kind  = OP_U32;
    llm_kv        kv    = LLM_KV_GENERAL_NAME;
    uint64_t      u     = 0;
    int64_t       i     = 0;
    float         f     = 0.0f;
    std::string   s;
    int           etype = 0;      // gguf_type for OP_ARR
    bool          per_layer = false;
    std::vector<uint8_t>       data;  // OP_ARR element bytes, OP_STRS NUL blob
    int64_t       n     = 0;
    std::vector<std::string>   strs;
};

struct TensorSpec {
    std::string          name;
    int                  type = 0;          // ggml_type
    std::vector<int64_t> ne4;               // GGML_MAX_DIMS entries
    std::vector<uint8_t> data;              // ggml_nbytes bytes
};

struct Case {
    std::string              name;
    int                      n_layer = 0;   // 0 = arch constructor (no model)
    std::vector<Op>          ops;
    std::vector<TensorSpec>  tensors;
    bool                     port_only = false;
    int64_t                  expect_rc = 0;
    int64_t                  expect_n_kv = 0;
    int64_t                  expect_n_tensors = 0;
};

static std::vector<Case> g_cases;

static Case & add_case(const char * name, int n_layer = 0) {
    Case c;
    c.name = name;
    c.n_layer = n_layer;
    g_cases.push_back(std::move(c));
    return g_cases.back();
}

static Op op_u32(llm_kv kv, uint32_t v)  { Op o; o.kind = OP_U32;  o.kv = kv; o.u = v; return o; }
static Op op_i32(llm_kv kv, int32_t v)   { Op o; o.kind = OP_I32;  o.kv = kv; o.i = v; return o; }
static Op op_u64(llm_kv kv, uint64_t v)  { Op o; o.kind = OP_U64;  o.kv = kv; o.u = v; return o; }
static Op op_bool(llm_kv kv, bool v)     { Op o; o.kind = OP_BOOL; o.kv = kv; o.u = v ? 1u : 0u; return o; }
static Op op_bool_raw(llm_kv kv, uint64_t v) { Op o; o.kind = OP_BOOL; o.kv = kv; o.u = v; return o; }
static Op op_str(llm_kv kv, const char * v) { Op o; o.kind = OP_STR; o.kv = kv; o.s = v; return o; }

static Op op_f32(llm_kv kv, float v) {
    Op o; o.kind = OP_F32; o.kv = kv; o.f = v;
    return o;
}

static Op op_arr_u32(llm_kv kv, const std::vector<uint32_t> & v, bool per_layer = false) {
    Op o; o.kind = OP_ARR; o.kv = kv; o.etype = GGUF_TYPE_UINT32; o.n = (int64_t) v.size();
    o.per_layer = per_layer;
    o.data.resize(v.size() * 4);
    if (!v.empty()) memcpy(o.data.data(), v.data(), o.data.size());
    return o;
}
static Op op_arr_f32(llm_kv kv, const std::vector<float> & v, bool per_layer = false) {
    Op o; o.kind = OP_ARR; o.kv = kv; o.etype = GGUF_TYPE_FLOAT32; o.n = (int64_t) v.size();
    o.per_layer = per_layer;
    o.data.resize(v.size() * 4);
    if (!v.empty()) memcpy(o.data.data(), v.data(), o.data.size());
    return o;
}
static Op op_arr_u64(llm_kv kv, const std::vector<uint64_t> & v, bool per_layer = false) {
    Op o; o.kind = OP_ARR; o.kv = kv; o.etype = GGUF_TYPE_UINT64; o.n = (int64_t) v.size();
    o.per_layer = per_layer;
    o.data.resize(v.size() * 8);
    if (!v.empty()) memcpy(o.data.data(), v.data(), o.data.size());
    return o;
}
static Op op_strs(llm_kv kv, const std::vector<std::string> & v) {
    Op o; o.kind = OP_STRS; o.kv = kv; o.strs = v; o.n = (int64_t) v.size();
    for (const std::string & s : v) {
        o.data.insert(o.data.end(), s.begin(), s.end());
        o.data.push_back(0);
    }
    return o;
}

// ------------------------------------------------------------ native helpers

static std::string hex(const void * p, size_t n) {
    static const char * d = "0123456789abcdef";
    const unsigned char * b = (const unsigned char *) p;
    std::string s;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; i++) { s += d[b[i] >> 4]; s += d[b[i] & 15]; }
    return s;
}

static size_t type_size(int t) {
    switch (t) {
        case GGUF_TYPE_UINT8: case GGUF_TYPE_INT8: case GGUF_TYPE_BOOL: return 1;
        case GGUF_TYPE_UINT16: case GGUF_TYPE_INT16: return 2;
        case GGUF_TYPE_UINT32: case GGUF_TYPE_INT32: case GGUF_TYPE_FLOAT32: return 4;
        case GGUF_TYPE_UINT64: case GGUF_TYPE_INT64: case GGUF_TYPE_FLOAT64: return 8;
        default: return 0;
    }
}

static std::string native_kv_dump(const gguf_context * ctx) {
    std::string s;
    char line[256];
    const int64_t n_kv = gguf_get_n_kv(ctx);
    snprintf(line, sizeof line, "N\t%lld\n", (long long) n_kv);
    s += line;
    for (int64_t i = 0; i < n_kv; i++) {
        const char * key = gguf_get_key(ctx, i);
        const int t = (int) gguf_get_kv_type(ctx, i);
        if (t == GGUF_TYPE_ARRAY) {
            const int at = (int) gguf_get_arr_type(ctx, i);
            const size_t n = gguf_get_arr_n(ctx, i);
            snprintf(line, sizeof line, "KV\t%s\t%d\t1\t%d\t%lld\t", key, at, at, (long long) n);
            s += line;
            if (at == GGUF_TYPE_STRING) {
                for (size_t j = 0; j < n; j++) {
                    const char * e = gguf_get_arr_str(ctx, i, j);
                    if (j) s += ",";
                    s += hex(e, strlen(e));
                }
            } else {
                s += hex(gguf_get_arr_data(ctx, i), n * type_size(at));
            }
            s += "\n";
        } else if (t == GGUF_TYPE_STRING) {
            const char * v = gguf_get_val_str(ctx, i);
            snprintf(line, sizeof line, "KV\t%s\t%d\t0\t8\t%lld\t", key, t, (long long) strlen(v));
            s += line;
            s += hex(v, strlen(v));
            s += "\n";
        } else {
            unsigned char buf[8] = {0};
            switch (t) {
                case GGUF_TYPE_UINT8:   { uint8_t  v = gguf_get_val_u8 (ctx, i); memcpy(buf, &v, 1); break; }
                case GGUF_TYPE_INT8:    { int8_t   v = gguf_get_val_i8 (ctx, i); memcpy(buf, &v, 1); break; }
                case GGUF_TYPE_UINT32:  { uint32_t v = gguf_get_val_u32(ctx, i); memcpy(buf, &v, 4); break; }
                case GGUF_TYPE_INT32:   { int32_t  v = gguf_get_val_i32(ctx, i); memcpy(buf, &v, 4); break; }
                case GGUF_TYPE_FLOAT32: { float    v = gguf_get_val_f32(ctx, i); memcpy(buf, &v, 4); break; }
                case GGUF_TYPE_UINT64:  { uint64_t v = gguf_get_val_u64(ctx, i); memcpy(buf, &v, 8); break; }
                case GGUF_TYPE_INT64:   { int64_t  v = gguf_get_val_i64(ctx, i); memcpy(buf, &v, 8); break; }
                case GGUF_TYPE_FLOAT64: { double   v = gguf_get_val_f64(ctx, i); memcpy(buf, &v, 8); break; }
                case GGUF_TYPE_BOOL:    { bool     v = gguf_get_val_bool(ctx, i); buf[0] = v ? 1 : 0; break; }
                default: break;
            }
            snprintf(line, sizeof line, "KV\t%s\t%d\t0\t%d\t%lld\t", key, t, t,
                     (long long) type_size(t));
            s += line;
            s += hex(buf, type_size(t));
            s += "\n";
        }
    }
    return s;
}

static std::string native_tensor_dump(const gguf_context * ctx) {
    std::string s;
    char line[256];
    snprintf(line, sizeof line, "T\t%lld\n", (long long) gguf_get_n_tensors(ctx));
    s += line;
    for (int64_t i = 0; i < gguf_get_n_tensors(ctx); i++) {
        const int64_t * ne = gguf_get_tensor_ne(ctx, i);
        snprintf(line, sizeof line, "T\t%s\t%d\t%lld\t%lld\t%lld\t%lld\t%lld\t%lld\n",
                 gguf_get_tensor_name(ctx, i), (int) gguf_get_tensor_type(ctx, i),
                 (long long) gguf_get_tensor_size(ctx, i),
                 (long long) gguf_get_tensor_offset(ctx, i),
                 (long long) ne[0], (long long) ne[1], (long long) ne[2], (long long) ne[3]);
        s += line;
    }
    return s;
}

static std::vector<int8_t> native_save(llama_model_saver & s, bool & ok) {
    std::vector<int8_t> b;
    ok = false;
    FILE * f = tmpfile();
    if (!f) return b;
    s.save(f);
    fflush(f);
    const long n = ftell(f);
    if (n < 0) { fclose(f); return b; }
    fseek(f, 0, SEEK_SET);
    b.resize((size_t) n);
    if (!b.empty() && fread(b.data(), 1, b.size(), f) != b.size()) { fclose(f); return b; }
    fclose(f);
    ok = true;
    return b;
}

// ---------------------------------------------------------------- port state

struct Port {
    static const int MAXK = 64;
    static const int MAXT = 32;

    int64_t st[8];
    unsigned char pool[65536];
    int64_t ko[MAXK], kl[MAXK], ty[MAXK], ar[MAXK], v0[MAXK], v1[MAXK], v2[MAXK];

    int64_t tst[8];
    unsigned char tpool[65536];
    int64_t tne[MAXT * 4], trec[MAXT * 8];

    int64_t out[2];

    sl_i64 A(int64_t * p, int64_t n) { return sl_i64{ p, n }; }
    sl_u8  S(void * p, int64_t n)     { return sl_u8{ (uint8_t *) p, n }; }

    void init(int64_t n_layer) {
        int64_t rc = sav_init(A(st, 8), A(ko, MAXK), A(kl, MAXK), A(ty, MAXK), A(ar, MAXK),
                              A(v0, MAXK), A(v1, MAXK), A(v2, MAXK),
                              A(tst, 8), 32, n_layer < 0 ? -1 : n_layer);
        if (rc != 0) { printf("FAIL init rc=%lld\n", (long long) rc); }
    }
};

static int64_t port_op(Port & p, const Op & o, const std::string & key) {
    const sl_u8 k{ (uint8_t *) key.data(), (int64_t) key.size() };
    switch (o.kind) {
        case OP_U32:
            return sav_add_u32(p.A(p.st, 8), p.S(p.pool, sizeof p.pool), p.A(p.ko, Port::MAXK),
                               p.A(p.kl, Port::MAXK), p.A(p.ty, Port::MAXK), p.A(p.ar, Port::MAXK),
                               p.A(p.v0, Port::MAXK), p.A(p.v1, Port::MAXK), p.A(p.v2, Port::MAXK),
                               k, (int64_t) o.u, p.A(p.out, 2));
        case OP_I32:
            return sav_add_i32(p.A(p.st, 8), p.S(p.pool, sizeof p.pool), p.A(p.ko, Port::MAXK),
                               p.A(p.kl, Port::MAXK), p.A(p.ty, Port::MAXK), p.A(p.ar, Port::MAXK),
                               p.A(p.v0, Port::MAXK), p.A(p.v1, Port::MAXK), p.A(p.v2, Port::MAXK),
                               k, (int64_t) o.i, p.A(p.out, 2));
        case OP_U64:
            return sav_add_u64(p.A(p.st, 8), p.S(p.pool, sizeof p.pool), p.A(p.ko, Port::MAXK),
                               p.A(p.kl, Port::MAXK), p.A(p.ty, Port::MAXK), p.A(p.ar, Port::MAXK),
                               p.A(p.v0, Port::MAXK), p.A(p.v1, Port::MAXK), p.A(p.v2, Port::MAXK),
                               k, (int64_t) o.u, p.A(p.out, 2));
        case OP_F32: {
            uint32_t bits = 0;
            memcpy(&bits, &o.f, 4);
            return sav_add_f32(p.A(p.st, 8), p.S(p.pool, sizeof p.pool), p.A(p.ko, Port::MAXK),
                               p.A(p.kl, Port::MAXK), p.A(p.ty, Port::MAXK), p.A(p.ar, Port::MAXK),
                               p.A(p.v0, Port::MAXK), p.A(p.v1, Port::MAXK), p.A(p.v2, Port::MAXK),
                               k, (int64_t) bits, p.A(p.out, 2));
        }
        case OP_BOOL:
            // o.u is passed raw on purpose: native takes a bool, so any non-zero
            // value must become 1 on the wire.
            return sav_add_bool(p.A(p.st, 8), p.S(p.pool, sizeof p.pool), p.A(p.ko, Port::MAXK),
                                p.A(p.kl, Port::MAXK), p.A(p.ty, Port::MAXK), p.A(p.ar, Port::MAXK),
                                p.A(p.v0, Port::MAXK), p.A(p.v1, Port::MAXK), p.A(p.v2, Port::MAXK),
                                k, (int64_t) o.u, p.A(p.out, 2));
        case OP_STR:
            return sav_add_str(p.A(p.st, 8), p.S(p.pool, sizeof p.pool), p.A(p.ko, Port::MAXK),
                               p.A(p.kl, Port::MAXK), p.A(p.ty, Port::MAXK), p.A(p.ar, Port::MAXK),
                               p.A(p.v0, Port::MAXK), p.A(p.v1, Port::MAXK), p.A(p.v2, Port::MAXK),
                               k, p.S((void *) o.s.data(), (int64_t) o.s.size()), p.A(p.out, 2));
        case OP_ARR:
            return sav_add_arr(p.A(p.st, 8), p.S(p.pool, sizeof p.pool), p.A(p.ko, Port::MAXK),
                               p.A(p.kl, Port::MAXK), p.A(p.ty, Port::MAXK), p.A(p.ar, Port::MAXK),
                               p.A(p.v0, Port::MAXK), p.A(p.v1, Port::MAXK), p.A(p.v2, Port::MAXK),
                               p.A(p.tst, 8), k, o.etype,
                               p.S((void *) o.data.data(), (int64_t) o.data.size()),
                               o.n, o.per_layer ? 1 : 0, p.A(p.out, 2));
        case OP_STRS:
            return sav_add_strs(p.A(p.st, 8), p.S(p.pool, sizeof p.pool), p.A(p.ko, Port::MAXK),
                                p.A(p.kl, Port::MAXK), p.A(p.ty, Port::MAXK), p.A(p.ar, Port::MAXK),
                                p.A(p.v0, Port::MAXK), p.A(p.v1, Port::MAXK), p.A(p.v2, Port::MAXK),
                                k, p.S((void *) o.data.data(), (int64_t) o.data.size()),
                                o.n, p.A(p.out, 2));
    }
    return -100;
}

static int64_t port_tensor_add(Port & p, const TensorSpec & t) {
    const sl_u8 name{ (uint8_t *) t.name.data(), (int64_t) t.name.size() };
    int64_t ne[4] = { t.ne4[0], t.ne4[1], t.ne4[2], t.ne4[3] };
    return sav_add_tensor(p.A(p.tst, 8), p.S(p.tpool, sizeof p.tpool), p.A(p.tne, Port::MAXT * 4),
                          p.A(p.trec, Port::MAXT * 8), name, 4, sl_i64{ ne, 4 }, t.type,
                          p.S((void *) t.data.data(), (int64_t) t.data.size()));
}

static std::string port_kv_dump(const Port & p) {
    std::string s;
    char line[256];
    snprintf(line, sizeof line, "N\t%lld\n", (long long) p.st[0]);
    s += line;
    for (int64_t i = 0; i < p.st[0]; i++) {
        std::string key((const char *) p.pool + p.ko[i], (size_t) p.kl[i]);
        const int t = (int) p.ty[i];
        if (p.ar[i] != 0) {
            const int64_t n = p.v1[i];
            snprintf(line, sizeof line, "KV\t%s\t%d\t1\t%d\t%lld\t", key.c_str(), t, t, (long long) n);
            s += line;
            if (p.ar[i] == 2) {
                int64_t q = p.v0[i], end = p.v0[i] + p.v2[i], k = 0;
                for (; q <= end && k < n; k++) {
                    int64_t b = q;
                    while (q < end && p.pool[q] != 0) q++;
                    if (k) s += ",";
                    s += hex(p.pool + b, (size_t) (q - b));
                    q++;
                }
            } else {
                s += hex(p.pool + p.v0[i], (size_t) p.v2[i]);
            }
            s += "\n";
        } else if (t == 8) {
            snprintf(line, sizeof line, "KV\t%s\t%d\t0\t8\t%lld\t", key.c_str(), t, (long long) p.v2[i]);
            s += line;
            s += hex(p.pool + p.v0[i], (size_t) p.v2[i]);
            s += "\n";
        } else {
            const uint64_t bits = (uint64_t) p.v0[i];
            const size_t n = type_size(t);
            snprintf(line, sizeof line, "KV\t%s\t%d\t0\t%d\t%lld\t", key.c_str(), t, t, (long long) n);
            s += line;
            unsigned char b[8];
            for (size_t k = 0; k < n; k++) b[k] = (unsigned char) (bits >> (8 * k));
            s += hex(b, n);
            s += "\n";
        }
    }
    return s;
}

static std::string port_tensor_dump(const Port & p) {
    std::string s;
    char line[256];
    snprintf(line, sizeof line, "T\t%lld\n", (long long) p.tst[0]);
    s += line;
    for (int64_t i = 0; i < p.tst[0]; i++) {
        std::string name((const char *) p.tpool + p.trec[i * 8 + 0], (size_t) p.trec[i * 8 + 1]);
        const int64_t nb = s3_tensor_size(sl_i64{ (int64_t *) p.tne, Port::MAXT * 4 },
                                          sl_i64{ (int64_t *) p.trec, Port::MAXT * 8 }, i);
        snprintf(line, sizeof line, "T\t%s\t%lld\t%lld\t%lld\t%lld\t%lld\t%lld\t%lld\n",
                 name.c_str(), (long long) p.trec[i * 8 + 2], (long long) nb,
                 (long long) p.trec[i * 8 + 6],
                 (long long) p.tne[i * 4 + 0], (long long) p.tne[i * 4 + 1],
                 (long long) p.tne[i * 4 + 2], (long long) p.tne[i * 4 + 3]);
        s += line;
    }
    return s;
}

// ------------------------------------------------------------------- harness

static int g_checks = 0;
static int g_fail = 0;

static void fail(const std::string & c, const char * what, const std::string & detail) {
    g_fail++;
    printf("FAIL\t%s\t%s\t%s\n", c.c_str(), what, detail.c_str());
}

static void check_str(const std::string & c, const char * what,
                      const std::string & native, const std::string & port) {
    g_checks++;
    if (native != port) {
        std::string d = "native loos=" + std::to_string(native.size()) +
                        " port len=" + std::to_string(port.size());
        size_t i = 0;
        while (i < native.size() && i < port.size() && native[i] == port[i]) i++;
        d += " first_diff=" + std::to_string(i);
        if (i < native.size()) {
            const size_t lo = i > 20 ? i - 20 : 0;
            d += " native=" + hex(native.data() + lo, std::min<size_t>(40, native.size() - lo));
        }
        fail(c, what, d);
    } else {
        printf("OK\t%s\t%s\t%zu\n", c.c_str(), what, native.size());
    }
}

static void check_bytes(const std::string & c, const std::vector<int8_t> & n,
                        const std::vector<uint8_t> & p) {
    g_checks++;
    if (n.size() != p.size()) {
        fail(c, "bytes", "native len=" + std::to_string(n.size()) +
                         " port len=" + std::to_string(p.size()));
        return;
    }
    for (size_t i = 0; i < n.size(); i++) {
        if ((uint8_t) n[i] != p[i]) {
            fail(c, "bytes", "first_diff=" + std::to_string(i) + " port=" + hex(p.data() + i,
                 std::min<size_t>(8, p.size() - i)));
            return;
        }
    }
    printf("OK\t%s\tbytes\t%zu\n", c.c_str(), n.size());
}

// Build one native ggml tensor that mirrors TensorSpec.
static ggml_tensor * make_tensor(ggml_context * ctx, const TensorSpec & t) {
    int64_t ne[4] = { t.ne4[0], t.ne4[1], t.ne4[2], t.ne4[3] };
    ggml_tensor * g = ggml_new_tensor(ctx, (ggml_type) t.type, 4, ne);
    ggml_set_name(g, t.name.c_str());
    if ((size_t) ggml_nbytes(g) != t.data.size()) {
        printf("FAIL\t%s\tfixture\tdata %zu != nbytes %zu\n", t.name.c_str(),
               t.data.size(), (size_t) ggml_nbytes(g));
        return g;
    }
    memcpy(g->data, t.data.data(), t.data.size());
    return g;
}

static bool run_native(const Case & c, std::string & kv_dump, std::string & t_dump,
                       std::vector<int8_t> & bytes) {
    TestModel model(llama_model_default_params());
    model.arch = LLM_ARCH_LLAMA;
    model.hparams.n_layer_all = c.n_layer > 0 ? (uint32_t) c.n_layer : 1;
    model.hparams.n_layer_nextn = 0;

    std::unique_ptr<llama_model_saver> saver;
    if (c.n_layer > 0) {
        saver.reset(new llama_model_saver(&model));
    } else {
        saver.reset(new llama_model_saver(LLM_ARCH_LLAMA, nullptr));
    }

    for (const Op & o : c.ops) {
        switch (o.kind) {
            case OP_U32:  saver->add_kv(o.kv, (uint32_t) o.u); break;
            case OP_I32:  saver->add_kv(o.kv, (int32_t)  o.i); break;
            case OP_U64:  saver->add_kv(o.kv, (uint64_t) o.u); break;
            case OP_F32:  saver->add_kv(o.kv, o.f); break;
            case OP_BOOL: saver->add_kv(o.kv, o.u != 0); break;
            case OP_STR:  saver->add_kv(o.kv, o.s.c_str()); break;
            case OP_ARR:
                if (o.etype == GGUF_TYPE_UINT32) {
                    std::vector<uint32_t> v((size_t) o.n);
                    if (!v.empty()) memcpy(v.data(), o.data.data(), v.size() * 4);
                    saver->add_kv(o.kv, v, o.per_layer);
                } else if (o.etype == GGUF_TYPE_FLOAT32) {
                    std::vector<float> v((size_t) o.n);
                    if (!v.empty()) memcpy(v.data(), o.data.data(), v.size() * 4);
                    saver->add_kv(o.kv, v, o.per_layer);
                } else if (o.etype == GGUF_TYPE_UINT64) {
                    std::vector<uint64_t> v((size_t) o.n);
                    if (!v.empty()) memcpy(v.data(), o.data.data(), v.size() * 8);
                    saver->add_kv(o.kv, v, o.per_layer);
                } else {
                    printf("FAIL\t%s\tfixture\tunsupported native element type %d\n",
                           c.name.c_str(), o.etype);
                    return false;
                }
                break;
            case OP_STRS: saver->add_kv(o.kv, o.strs); break;
        }
    }

    if (!c.tensors.empty()) {
        ggml_init_params ip;
        ip.mem_size = 1 << 20;
        ip.mem_buffer = nullptr;
        ip.no_alloc = false;
        ggml_context * gctx = ggml_init(ip);
        if (!gctx) { printf("FAIL\t%s\tfixture\tggml_init\n", c.name.c_str()); return false; }
        for (const TensorSpec & t : c.tensors) {
            ggml_tensor * g = make_tensor(gctx, t);
            saver->add_tensor(g);
        }
        kv_dump = native_kv_dump(saver->gguf_ctx);
        t_dump = native_tensor_dump(saver->gguf_ctx);
        bool ok = false;
        bytes = native_save(*saver, ok);
        ggml_free(gctx);
        if (!ok) { printf("FAIL\t%s\tfixture\tnative save\n", c.name.c_str()); return false; }
    } else {
        kv_dump = native_kv_dump(saver->gguf_ctx);
        t_dump = native_tensor_dump(saver->gguf_ctx);
        bool ok = false;
        bytes = native_save(*saver, ok);
        if (!ok) { printf("FAIL\t%s\tfixture\tnative save\n", c.name.c_str()); return false; }
    }

    return true;
}

static void run_port(const Case & c, Port & p, std::vector<uint8_t> & bytes) {
    p.init(c.n_layer > 0 ? c.n_layer : -1);
    const LLM_KV kv(LLM_ARCH_LLAMA);
    for (const Op & o : c.ops) {
        const std::string key = kv(o.kv);
        int64_t rc = port_op(p, o, key);
        if (rc != 0) {
            printf("FAIL\t%s\tport_op\tkey=%s rc=%lld\n", c.name.c_str(), key.c_str(),
                   (long long) rc);
            g_fail++;
        }
    }
    for (const TensorSpec & t : c.tensors) {
        int64_t rc = port_tensor_add(p, t);
        if (rc < 0) {
            printf("FAIL\t%s\tport_tensor\t%s rc=%lld\n", c.name.c_str(), t.name.c_str(),
                   (long long) rc);
            g_fail++;
        }
    }
    bytes.assign(1 << 20, 0);
    const int64_t n = sav_save(p.A(p.st, 8), sl_u8{ p.pool, (int64_t) sizeof p.pool },
                               p.A(p.ko, Port::MAXK), p.A(p.kl, Port::MAXK),
                               p.A(p.ty, Port::MAXK), p.A(p.ar, Port::MAXK),
                               p.A(p.v0, Port::MAXK), p.A(p.v1, Port::MAXK), p.A(p.v2, Port::MAXK),
                               p.A(p.tst, 8), sl_u8{ p.tpool, (int64_t) sizeof p.tpool },
                               p.A(p.tne, Port::MAXT * 4), p.A(p.trec, Port::MAXT * 8),
                               sl_u8{ bytes.data(), (int64_t) bytes.size() }, 3);
    if (n < 0) {
        printf("FAIL\t%s\tport_save\trc=%lld\n", c.name.c_str(), (long long) n);
        g_fail++;
        bytes.clear();
    } else {
        bytes.resize((size_t) n);
    }
}

// Reload the port bytes natively and require the same counts and dumps.
static void check_reload(const Case & c, const std::string & path,
                         const std::string & kv_dump, const std::string & t_dump) {
    // the caller writes the port bytes to `path` before this call
    gguf_init_params params;
    params.no_alloc = true;
    params.ctx = nullptr;
    gguf_context * ctx = gguf_init_from_file(path.c_str(), params);
    remove(path.c_str());
    g_checks++;
    if (!ctx) { fail(c.name, "reload", "gguf_init_from_file failed"); return; }
    check_str(c.name, "reload_kv", kv_dump, native_kv_dump(ctx));
    check_str(c.name, "reload_tensors", t_dump, native_tensor_dump(ctx));
    gguf_free(ctx);
}

int main(int argc, char ** argv) {
    goose_init(argc, argv);
    const std::string tmp = argc > 1 ? argv[1] : "s4_tmp.gguf";

    // ---- scalar overloads, arch constructor ------------------------------
    {
        Case & c = add_case("u32_block");
        c.ops = { op_u32(LLM_KV_BLOCK_COUNT, 32), op_u32(LLM_KV_VOCAB_SIZE, 32000) };
    }
    {
        Case & c = add_case("i32_negative");
        c.ops = { op_i32(LLM_KV_DECODER_START_TOKEN_ID, -1),
                  op_i32(LLM_KV_EXPERT_USED_COUNT, -7),
                  op_i32(LLM_KV_EXPERT_GROUP_COUNT, 2147483647) };
    }
    {
        Case & c = add_case("u64_large");
        c.ops = { op_u64(LLM_KV_GENERAL_FILE_TYPE, 0x123456789abcdef0ull),
                  op_u64(LLM_KV_TIME_MIX_EXTRA_DIM, 1ull) };
    }
    {
        Case & c = add_case("f32_scalars");
        c.ops = { op_f32(LLM_KV_ATTENTION_LAYERNORM_EPS, 1e-5f),
                  op_f32(LLM_KV_LOGIT_SCALE, 0.5f),
                  op_f32(LLM_KV_EXPERT_WEIGHTS_SCALE, -1.25f) };
    }
    {
        Case & c = add_case("bool_values");
        c.ops = { op_bool(LLM_KV_USE_PARALLEL_RESIDUAL, true),
                  op_bool(LLM_KV_ATTENTION_CAUSAL, false),
                  op_bool_raw(LLM_KV_SWIN_NORM, 7) };
    }
    {
        Case & c = add_case("str_utf8");
        c.ops = { op_str(LLM_KV_GENERAL_NAME, "\xd0\xbf\xd0\xbe\xd1\x80\xd1\x82/\xe6\xa8\xa1\xe5\x9e\x8b"),
                  op_str(LLM_KV_GENERAL_AUTHOR, ""),
                  op_str(LLM_KV_GENERAL_LICENSE, "MIT") };
    }
    {
        Case & c = add_case("overwrite_key");
        c.ops = { op_u32(LLM_KV_BLOCK_COUNT, 5), op_str(LLM_KV_GENERAL_NAME, "a"),
                  op_u32(LLM_KV_VOCAB_SIZE, 1), op_u32(LLM_KV_BLOCK_COUNT, 7),
                  op_str(LLM_KV_GENERAL_NAME, "b") };
    }

    // ---- container overloads ---------------------------------------------
    {
        Case & c = add_case("arr_u32_mixed");
        c.ops = { op_arr_u32(LLM_KV_ATTENTION_HEAD_COUNT, {4, 4, 8, 8}) };
    }
    {
        Case & c = add_case("arr_f32");
        c.ops = { op_arr_f32(LLM_KV_SWIGLU_CLAMP_EXP, {0.5f, -1.25f, 3.0f, 0.0f}) };
    }
    {
        Case & c = add_case("arr_u64");
        c.ops = { op_arr_u64(LLM_KV_PLE_HEAD_OFFSETS, {1ull, 100000ull, 0x7fffffffull}) };
    }
    {
        Case & c = add_case("arr_empty");
        c.ops = { op_arr_u32(LLM_KV_FEED_FORWARD_LENGTH, {}),
                  op_u32(LLM_KV_BLOCK_COUNT, 1) };
    }
    {
        Case & c = add_case("strs_tokens");
        c.ops = { op_strs(LLM_KV_TOKENIZER_LIST, {"a", "", "bc",
                  "\xd0\xba\xd0\xbb\xd1\x8e\xd1\x87", "\xe6\xa8\xa1\xe5\x9e\x8b"}) };
    }
    {
        Case & c = add_case("strs_long");
        std::vector<std::string> v;
        for (int i = 0; i < 40; i++) v.push_back(std::string((size_t) (i * 3), (char) ('a' + i % 26)));
        c.ops = { op_strs(LLM_KV_TOKENIZER_MERGES, v) };
    }

    // ---- per-layer reduction (model constructor) -------------------------
    {
        Case & c = add_case("perlayer_same_u32", 4);
        c.ops = { op_arr_u32(LLM_KV_FEED_FORWARD_LENGTH, {10, 10, 10, 10}, true) };
    }
    {
        Case & c = add_case("perlayer_diff_u32", 4);
        c.ops = { op_arr_u32(LLM_KV_ATTENTION_HEAD_COUNT, {4, 4, 8, 8}, true) };
    }
    {
        Case & c = add_case("perlayer_diff_f32", 6);
        c.ops = { op_arr_f32(LLM_KV_SWIGLU_CLAMP_EXP,
                             {0.5f, -1.25f, 3.0f, 0.0f, 27.5f, -0.125f}, true) };
    }
    {
        Case & c = add_case("perlayer_first_n", 3);
        c.ops = { op_arr_u32(LLM_KV_ATTENTION_HEAD_COUNT_KV, {1, 2, 3, 4, 5}, true) };
    }
    {
        Case & c = add_case("perlayer_same_u64", 2);
        c.ops = { op_arr_u64(LLM_KV_PLE_HEAD_OFFSETS, {7, 7, 9}, true) };
    }
    {
        Case & c = add_case("perlayer_prefix_same", 3);
        c.ops = { op_arr_u32(LLM_KV_ATTENTION_HEAD_COUNT, {5, 5, 5, 9}, true) };
    }
    {
        Case & c = add_case("empty_file", 0);
    }

    // ---- tensors and save -------------------------------------------------
    {
        Case & c = add_case("tensor_one");
        TensorSpec t;
        t.name = "token_embd.weight";
        t.type = GGML_TYPE_F32;
        t.ne4 = {4, 1, 1, 1};
        t.data.resize(16);
        for (int i = 0; i < 16; i++) t.data[i] = (uint8_t) (i * 7 + 1);
        c.tensors = { t };
        c.ops = { op_u32(LLM_KV_BLOCK_COUNT, 1) };
    }
    {
        Case & c = add_case("tensor_many");
        c.ops = { op_u32(LLM_KV_BLOCK_COUNT, 3),
                  op_f32(LLM_KV_ATTENTION_LAYERNORM_RMS_EPS, 1e-6f),
                  op_str(LLM_KV_GENERAL_ARCHITECTURE, "llama") };
        TensorSpec a; a.name = "blk.0.attn_q.weight"; a.type = GGML_TYPE_F16; a.ne4 = {4, 2, 1, 1};
        a.data.resize(16); for (int i = 0; i < 16; i++) a.data[i] = (uint8_t) (0x40 + i);
        TensorSpec b; b.name = "blk.0.attn_k.weight"; b.type = GGML_TYPE_Q8_0; b.ne4 = {32, 1, 1, 1};
        b.data.resize(34); for (int i = 0; i < 34; i++) b.data[i] = (uint8_t) (0xa0 + i);
        TensorSpec d; d.name = "blk.0.ffn_gate.weight"; d.type = GGML_TYPE_I32; d.ne4 = {3, 1, 1, 1};
        d.data.resize(12); for (int i = 0; i < 12; i++) d.data[i] = (uint8_t) (0x10 + i);
        TensorSpec e; e.name = "output_norm.weight"; e.type = GGML_TYPE_F32; e.ne4 = {2, 2, 1, 1};
        e.data.resize(16); for (int i = 0; i < 16; i++) e.data[i] = (uint8_t) (0xf0 - i);
        c.tensors = { a, b, d, e };
    }
    {
        Case & c = add_case("tensor_long_name");
        TensorSpec a;
        a.name = std::string(63, 'n');
        a.type = GGML_TYPE_F32;
        a.ne4 = {8, 1, 1, 1};
        a.data.resize(32);
        for (int i = 0; i < 32; i++) a.data[i] = (uint8_t) i;
        TensorSpec b; b.name = "blk.0.attn_v.weight"; b.type = GGML_TYPE_I32; b.ne4 = {1, 3, 1, 1};
        b.data.resize(12); for (int i = 0; i < 12; i++) b.data[i] = (uint8_t) (0x30 + i);
        c.tensors = { a, b };
    }
    {
        Case & c = add_case("tensor_rope_dup");
        TensorSpec a; a.name = "rope_freqs.weight"; a.type = GGML_TYPE_F32; a.ne4 = {4, 1, 1, 1};
        a.data.resize(16); for (int i = 0; i < 16; i++) a.data[i] = (uint8_t) i;
        TensorSpec b = a;
        b.data.assign(16, 0xEE);  // ignored: native skips the duplicate
        TensorSpec d; d.name = "blk.0.attn_norm.weight"; d.type = GGML_TYPE_F32; d.ne4 = {2, 1, 1, 1};
        d.data = {1, 2, 3, 4, 5, 6, 7, 8};
        c.tensors = { a, b, d };
    }
    {
        Case & c = add_case("combined");
        c.n_layer = 4;
        c.ops = { op_u32(LLM_KV_BLOCK_COUNT, 4),
                  op_arr_u32(LLM_KV_FEED_FORWARD_LENGTH, {16, 16, 32, 32}, true),
                  op_bool(LLM_KV_USE_PARALLEL_RESIDUAL, true),
                  op_str(LLM_KV_GENERAL_NAME, "combined") };
        TensorSpec a; a.name = "token_embd.weight"; a.type = GGML_TYPE_F16; a.ne4 = {8, 2, 1, 1};
        a.data.resize(32); for (int i = 0; i < 32; i++) a.data[i] = (uint8_t) (i * 3);
        TensorSpec b; b.name = "output.weight"; b.type = GGML_TYPE_Q8_0; b.ne4 = {32, 2, 1, 1};
        b.data.resize(68); for (int i = 0; i < 68; i++) b.data[i] = (uint8_t) (0x80 + i);
        c.tensors = { a, b };
    }

    // ---- port-only refusals (native aborts) -------------------------------
    {
        Case & c = add_case("char_overload", 0);
        c.port_only = true;
        c.expect_rc = -1;
        c.expect_n_kv = 0;
        c.expect_n_tensors = 0;
    }
    {
        Case & c = add_case("tensor_dup_refused", 0);
        c.port_only = true;
        c.expect_rc = -1;
        c.expect_n_tensors = 1;
        TensorSpec a; a.name = "blk.0.attn_q.weight"; a.type = GGML_TYPE_F32; a.ne4 = {2, 1, 1, 1};
        a.data = {1, 2, 3, 4, 5, 6, 7, 8};
        c.tensors = { a, a };
    }
    {
        Case & c = add_case("perlayer_no_model", 0);
        c.port_only = true;
        c.expect_rc = -1;
        c.expect_n_kv = 0;
        c.ops = { op_arr_u32(LLM_KV_FEED_FORWARD_LENGTH, {1, 2, 3, 4}, true) };
    }
    {
        Case & c = add_case("perlayer_short", 8);
        c.port_only = true;
        c.expect_rc = -1;
        c.expect_n_kv = 0;
        c.ops = { op_arr_u32(LLM_KV_FEED_FORWARD_LENGTH, {1, 2, 3, 4}, true) };
    }

    for (const Case & c : g_cases) {
        Port p;
        if (c.port_only) {
            p.init(c.n_layer > 0 ? c.n_layer : -1);
            const LLM_KV kv(LLM_ARCH_LLAMA);
            int64_t rc = 0;
            if (c.name == "char_overload") {
                const std::string key = kv(LLM_KV_GENERAL_NAME);
                rc = sav_add_char(p.A(p.st, 8), p.S(p.pool, sizeof p.pool), p.A(p.ko, Port::MAXK),
                                  p.A(p.kl, Port::MAXK), p.A(p.ty, Port::MAXK), p.A(p.ar, Port::MAXK),
                                  p.A(p.v0, Port::MAXK), p.A(p.v1, Port::MAXK), p.A(p.v2, Port::MAXK),
                                  sl_u8{ (uint8_t *) key.data(), (int64_t) key.size() }, 65,
                                  p.A(p.out, 2));
            } else if (!c.tensors.empty()) {
                for (const TensorSpec & t : c.tensors) rc = port_tensor_add(p, t);
            } else {
                for (const Op & o : c.ops) rc = port_op(p, o, kv(o.kv));
            }
            g_checks++;
            if (rc != c.expect_rc) {
                fail(c.name, "port_rc", "got=" + std::to_string(rc) +
                     " want=" + std::to_string(c.expect_rc));
            } else if (p.st[0] != c.expect_n_kv) {
                fail(c.name, "port_n_kv", "got=" + std::to_string(p.st[0]) +
                     " want=" + std::to_string(c.expect_n_kv));
            } else if (p.tst[0] != c.expect_n_tensors) {
                fail(c.name, "port_n_tensors", "got=" + std::to_string(p.tst[0]) +
                     " want=" + std::to_string(c.expect_n_tensors));
            } else {
                printf("OK\t%s\tport_refusal\t%lld\n", c.name.c_str(), (long long) rc);
            }
            continue;
        }

        std::string n_kv, n_t;
        std::vector<int8_t> n_bytes;
        if (!run_native(c, n_kv, n_t, n_bytes)) {
            g_fail++;
            continue;
        }

        std::vector<uint8_t> p_bytes;
        run_port(c, p, p_bytes);

        check_str(c.name, "kv_tsv", n_kv, port_kv_dump(p));
        check_str(c.name, "tensor_tsv", n_t, port_tensor_dump(p));
        check_bytes(c.name, n_bytes, p_bytes);

        {
            FILE * f = fopen(tmp.c_str(), "wb");
            if (!f) { g_checks++; fail(c.name, "reload", "cannot open temp file"); continue; }
            fwrite(p_bytes.data(), 1, p_bytes.size(), f);
            fclose(f);
            check_reload(c, tmp, n_kv, n_t);
        }
    }

    printf("FG_S4 cases=%zu checks=%d failures=%d\n", g_cases.size(), g_checks, g_fail);
    return g_fail == 0 ? 0 : 1;
}
