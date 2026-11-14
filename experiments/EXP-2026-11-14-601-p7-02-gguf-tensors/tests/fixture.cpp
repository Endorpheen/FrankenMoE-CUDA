// Gate fixture for the P7-02 S3 GGUF writer port (tensor descriptors + data).
//
// One binary, two implementations:
//   - native: gguf_add_tensor / gguf_set_tensor_type / gguf_set_tensor_data,
//     gguf_get_meta_size/data and gguf_write_to_file from ggml/src/gguf.cpp;
//   - port:   s1_* + s3_* from source/gguf-tensors.goose.
//
// For every case the fixture compares the canonical KV dump, the tensor
// descriptor dump (name, type, dims, ne[4], offset, nbytes), the metadata bytes
// (only_meta=true) and the full bytes (only_meta=false).
//
// Every tensor case is also written to a temp file and reloaded through the
// native reader (no_alloc=false): the alignment, descriptor dump and the data
// read back through ggml_get_tensor are checked, then the native writer
// re-serializes the reloaded context and its bytes are compared with the port.
// That round trip is the only public way to give a native context a non-default
// alignment (32 is fixed by gguf_init_empty), so the 64/1 cases use it.
//
// Exit code is 1 when any comparison fails.
#include "gguf.h"
#include "ggml.h"
#include "gguf_tensors.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------- case model

enum OpKind { OP_VAL, OP_STR, OP_ARR, OP_ARRSTR, OP_REMOVE, OP_KVCOPY };

struct Op {
    OpKind kind = OP_VAL;
    const char * key = "";
    int type = 0;
    uint64_t bits = 0;
    const void * data = nullptr;
    const char ** strs = nullptr;
    const char * text = nullptr;
    int64_t n = 0;
    int64_t bytes = 0;
    const std::vector<Op> * sub = nullptr;
};

enum CaseMode { MODE_DIRECT, MODE_RELOAD, MODE_META_ONLY };

struct Case {
    const char * name;
    int64_t align;
    CaseMode mode;
};

// One synthetic tensor. `type_add` is the type at gguf_add_tensor time,
// `type_set` the type passed to gguf_set_tensor_type afterwards (-1 none),
// exactly the native call order.
struct TSpec {
    const char * name;
    int64_t ne[4];
    int ndims;
    int type_add;
    int type_set;
    bool with_data;
    int seed;
};

static std::vector<Case> g_cases;
static std::vector<std::vector<Op>> g_ops;
static std::vector<std::vector<TSpec>> g_tensors;

static void add_case(const char * name, std::vector<Op> ops, int64_t align = 32,
                     CaseMode mode = MODE_DIRECT) {
    g_cases.push_back(Case{name, align, mode});
    g_ops.push_back(std::move(ops));
    g_tensors.push_back({});
}

static void add_tcase(const char * name, std::vector<Op> ops, std::vector<TSpec> ts,
                      int64_t align = 32, CaseMode mode = MODE_DIRECT) {
    g_cases.push_back(Case{name, align, mode});
    g_ops.push_back(std::move(ops));
    g_tensors.push_back(std::move(ts));
}

static Op val(const char * key, int t, uint64_t bits) {
    Op o; o.kind = OP_VAL; o.key = key; o.type = t; o.bits = bits; return o;
}
static Op str(const char * key, const char * text) {
    Op o; o.kind = OP_STR; o.key = key; o.text = text; return o;
}
static Op arr(const char * key, int t, const void * data, int64_t bytes, int64_t n) {
    Op o; o.kind = OP_ARR; o.key = key; o.type = t; o.data = data; o.bytes = bytes; o.n = n; return o;
}
static Op arr_str(const char * key, const char ** strs, int64_t n) {
    Op o; o.kind = OP_ARRSTR; o.key = key; o.strs = strs; o.n = n; return o;
}
static Op remove_op(const char * key) {
    Op o; o.kind = OP_REMOVE; o.key = key; return o;
}
static Op kv_copy(const std::vector<Op> * sub) {
    Op o; o.kind = OP_KVCOPY; o.sub = sub; return o;
}

static TSpec ts(const char * name, int64_t ne0, int64_t ne1, int64_t ne2, int64_t ne3,
                int ndims, int type_add, int type_set, int seed) {
    TSpec s;
    s.name = name;
    s.ne[0] = ne0; s.ne[1] = ne1; s.ne[2] = ne2; s.ne[3] = ne3;
    s.ndims = ndims;
    s.type_add = type_add;
    s.type_set = type_set;
    s.with_data = true;
    s.seed = seed;
    return s;
}

static TSpec ts_nodata(const char * name, int64_t ne0, int ndims, int type) {
    TSpec s = ts(name, ne0, 1, 1, 1, ndims, type, -1, 0);
    s.with_data = false;
    return s;
}

// ------------------------------------------------------------ native context

static std::string native_hex(const void * p, size_t n) {
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

static void set_native_scalar(gguf_context * c, const char * k, int t, uint64_t bits) {
    switch (t) {
        case GGUF_TYPE_UINT8:   gguf_set_val_u8  (c, k, (uint8_t)  bits); break;
        case GGUF_TYPE_INT8:    gguf_set_val_i8  (c, k, (int8_t)   bits); break;
        case GGUF_TYPE_UINT16:  gguf_set_val_u16 (c, k, (uint16_t) bits); break;
        case GGUF_TYPE_INT16:   gguf_set_val_i16 (c, k, (int16_t)  bits); break;
        case GGUF_TYPE_UINT32:  gguf_set_val_u32 (c, k, (uint32_t) bits); break;
        case GGUF_TYPE_INT32:   gguf_set_val_i32 (c, k, (int32_t)  bits); break;
        case GGUF_TYPE_UINT64:  gguf_set_val_u64 (c, k, bits); break;
        case GGUF_TYPE_INT64:   gguf_set_val_i64 (c, k, (int64_t)  bits); break;
        case GGUF_TYPE_FLOAT32: { float v; uint32_t b = (uint32_t) bits; memcpy(&v, &b, 4);
                                  gguf_set_val_f32(c, k, v); break; }
        case GGUF_TYPE_FLOAT64: { double v; memcpy(&v, &bits, 8); gguf_set_val_f64(c, k, v); break; }
        case GGUF_TYPE_BOOL:    gguf_set_val_bool(c, k, bits != 0); break;
        default: break;
    }
}

static void apply_native_op(gguf_context * c, const Op & o) {
    switch (o.kind) {
        case OP_VAL:   set_native_scalar(c, o.key, o.type, o.bits); break;
        case OP_STR:   gguf_set_val_str(c, o.key, o.text); break;
        case OP_ARR:   gguf_set_arr_data(c, o.key, (gguf_type) o.type, o.data, (size_t) o.n); break;
        case OP_ARRSTR: gguf_set_arr_str(c, o.key, o.strs, (size_t) o.n); break;
        case OP_REMOVE: gguf_remove_key(c, o.key); break;
        case OP_KVCOPY: {
            gguf_context * src = gguf_init_empty();
            for (const Op & s : *o.sub) apply_native_op(src, s);
            gguf_set_kv(c, src);
            gguf_free(src);
            break;
        }
    }
}

static void apply_native(gguf_context * c, const std::vector<Op> & ops) {
    for (const Op & o : ops) apply_native_op(c, o);
}

static std::string native_dump(const gguf_context * ctx) {
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
                    s += native_hex(e, strlen(e));
                }
            } else {
                s += native_hex(gguf_get_arr_data(ctx, i), n * type_size(at));
            }
            s += "\n";
        } else if (t == GGUF_TYPE_STRING) {
            const char * v = gguf_get_val_str(ctx, i);
            snprintf(line, sizeof line, "KV\t%s\t%d\t0\t8\t%lld\t", key, t, (long long) strlen(v));
            s += line;
            s += native_hex(v, strlen(v));
            s += "\n";
        } else {
            unsigned char buf[8] = {0};
            switch (t) {
                case GGUF_TYPE_UINT8:   { uint8_t  v = gguf_get_val_u8 (ctx, i); memcpy(buf, &v, 1); break; }
                case GGUF_TYPE_INT8:    { int8_t   v = gguf_get_val_i8 (ctx, i); memcpy(buf, &v, 1); break; }
                case GGUF_TYPE_UINT16:  { uint16_t v = gguf_get_val_u16(ctx, i); memcpy(buf, &v, 2); break; }
                case GGUF_TYPE_INT16:   { int16_t  v = gguf_get_val_i16(ctx, i); memcpy(buf, &v, 2); break; }
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
            s += native_hex(buf, type_size(t));
            s += "\n";
        }
    }
    return s;
}

// n_dims as ggml_n_dims computes it: the highest dimension index > 1.
static int dims_of(const int64_t * ne) {
    for (int d = GGML_MAX_DIMS - 1; d >= 1; --d) {
        if (ne[d] > 1) return d + 1;
    }
    return 1;
}

static std::string native_tensor_dump(const gguf_context * c) {
    std::string s;
    char line[512];
    const int64_t n = gguf_get_n_tensors(c);
    snprintf(line, sizeof line, "TN\t%lld\n", (long long) n);
    s += line;
    for (int64_t i = 0; i < n; i++) {
        const int64_t * ne = gguf_get_tensor_ne(c, i);
        snprintf(line, sizeof line, "T\t%s\t%d\t%d\t%lld\t%lld\t%lld\t%lld\t%lld\t%lld\n",
                 gguf_get_tensor_name(c, i), (int) gguf_get_tensor_type(c, i), dims_of(ne),
                 (long long) ne[0], (long long) ne[1], (long long) ne[2], (long long) ne[3],
                 (long long) gguf_get_tensor_offset(c, i),
                 (long long) gguf_get_tensor_size(c, i));
        s += line;
    }
    return s;
}

// ---------------------------------------------------------------- port state

struct PortCtx {
    static const int MAX = 64;
    static const int TMAX = 16;
    int64_t st[8];
    unsigned char pool[8192];
    int64_t ko[MAX], kl[MAX], ty[MAX], ar[MAX], v0[MAX], v1[MAX], v2[MAX];
    int64_t out[2];

    int64_t tst[4];
    unsigned char tpool[65536];
    int64_t tne[TMAX * 4];
    int64_t trec[TMAX * 8];
    int64_t align = 32;

    sl_i64 A(int64_t * p, int64_t n = MAX) { return sl_i64{ p, n }; }
    sl_u8  S(void * p, int64_t n) { return sl_u8{ (uint8_t *) p, n }; }

    void init() { s1_init(A(st), A(ko), A(kl), A(ty), A(ar), A(v0), A(v1), A(v2)); }
    void init_tensors(int64_t a) {
        align = a;
        s3_init_tensors(A(tst, 4), a);
    }

    void set_val(const char * key, int t, int64_t bits) {
        s1_set_val(A(st), S(pool, sizeof pool), A(ko), A(kl), A(ty), A(ar), A(v0), A(v1), A(v2),
                   S((void *) key, (int64_t) strlen(key)), t, bits, A(out));
    }
    void set_str(const char * key, const char * text) {
        s1_set_str(A(st), S(pool, sizeof pool), A(ko), A(kl), A(ty), A(ar), A(v0), A(v1), A(v2),
                   S((void *) key, (int64_t) strlen(key)),
                   S((void *) text, (int64_t) strlen(text)), A(out));
    }
    void set_arr(const char * key, int t, const void * data, int64_t bytes, int64_t n) {
        s1_set_arr(A(st), S(pool, sizeof pool), A(ko), A(kl), A(ty), A(ar), A(v0), A(v1), A(v2),
                   S((void *) key, (int64_t) strlen(key)), t,
                   S((void *) data, bytes), n, A(out));
    }
    void set_arr_str(const char * key, const std::string & blob, int64_t n) {
        s1_set_arr_str(A(st), S(pool, sizeof pool), A(ko), A(kl), A(ty), A(ar), A(v0), A(v1), A(v2),
                       S((void *) key, (int64_t) strlen(key)),
                       S((void *) blob.data(), (int64_t) blob.size()), n, A(out));
    }
    void copy_from(PortCtx & src) {
        s1_set_kv(A(st), S(pool, sizeof pool), A(ko), A(kl), A(ty), A(ar), A(v0), A(v1), A(v2),
                  src.A(src.st), src.S(src.pool, sizeof src.pool),
                  src.A(src.ko), src.A(src.kl), src.A(src.ty), src.A(src.ar),
                  src.A(src.v0), src.A(src.v1), src.A(src.v2), A(out));
    }
};

static void apply_port(PortCtx & c, const std::vector<Op> & ops) {
    for (const Op & o : ops) {
        switch (o.kind) {
            case OP_VAL:   c.set_val(o.key, o.type, (int64_t) o.bits); break;
            case OP_STR:   c.set_str(o.key, o.text); break;
            case OP_ARR:   c.set_arr(o.key, o.type, o.data, o.bytes, o.n); break;
            case OP_ARRSTR: {
                std::string blob;
                for (int64_t i = 0; i < o.n; i++) {
                    size_t l = strlen(o.strs[i]);
                    blob.append(o.strs[i], l);
                    blob.push_back('\0');
                }
                c.set_arr_str(o.key, blob, o.n);
                break;
            }
            case OP_REMOVE:
                s1_remove(c.A(c.st), c.S(c.pool, sizeof c.pool), c.A(c.ko), c.A(c.kl),
                          c.A(c.ty), c.A(c.ar), c.A(c.v0), c.A(c.v1), c.A(c.v2),
                          c.S((void *) o.key, (int64_t) strlen(o.key)));
                break;
            case OP_KVCOPY: {
                PortCtx src;
                src.init();
                apply_port(src, *o.sub);
                c.copy_from(src);
                break;
            }
        }
    }
}

static std::string port_dump(const PortCtx & c) {
    std::string s;
    char line[256];
    snprintf(line, sizeof line, "N\t%lld\n", (long long) c.st[0]);
    s += line;
    for (int64_t i = 0; i < c.st[0]; i++) {
        std::string key((const char *) c.pool + c.ko[i], (size_t) c.kl[i]);
        const int t = (int) c.ty[i];
        if (c.ar[i] != 0) {
            const int64_t n = c.v1[i];
            snprintf(line, sizeof line, "KV\t%s\t%d\t1\t%d\t%lld\t", key.c_str(), t, t,
                     (long long) n);
            s += line;
            if (c.ar[i] == 2) {
                int64_t p = c.v0[i], end = c.v0[i] + c.v2[i], k = 0;
                for (; p <= end && k < n; k++) {
                    int64_t b = p;
                    while (p < end && c.pool[p] != 0) p++;
                    if (k) s += ",";
                    s += native_hex(c.pool + b, (size_t) (p - b));
                    p++;
                }
            } else {
                s += native_hex(c.pool + c.v0[i], (size_t) c.v2[i]);
            }
            s += "\n";
        } else if (t == 8) {
            snprintf(line, sizeof line, "KV\t%s\t%d\t0\t8\t%lld\t", key.c_str(), t,
                     (long long) c.v2[i]);
            s += line;
            s += native_hex(c.pool + c.v0[i], (size_t) c.v2[i]);
            s += "\n";
        } else {
            const uint64_t bits = (uint64_t) c.v0[i];
            const size_t n = type_size(t);
            snprintf(line, sizeof line, "KV\t%s\t%d\t0\t%d\t%lld\t", key.c_str(), t, t,
                     (long long) n);
            s += line;
            unsigned char b[8];
            for (size_t k = 0; k < n; k++) b[k] = (unsigned char) (bits >> (8 * k));
            s += native_hex(b, n);
            s += "\n";
        }
    }
    return s;
}

static std::string port_tensor_dump(PortCtx & c) {
    std::string s;
    char line[512];
    const int64_t n = c.tst[0];
    snprintf(line, sizeof line, "TN\t%lld\n", (long long) n);
    s += line;
    for (int64_t i = 0; i < n; i++) {
        const int64_t * rec = c.trec + i * 8;
        const std::string name((const char *) c.tpool + rec[0], (size_t) rec[1]);
        const int64_t * ne = c.tne + i * 4;
        snprintf(line, sizeof line, "T\t%s\t%d\t%d\t%lld\t%lld\t%lld\t%lld\t%lld\t%lld\n",
                 name.c_str(), (int) rec[2], dims_of(ne),
                 (long long) ne[0], (long long) ne[1], (long long) ne[2], (long long) ne[3],
                 (long long) rec[6],
                 (long long) s3_tensor_size(c.A(c.tne, PortCtx::TMAX * 4),
                                            c.A(c.trec, PortCtx::TMAX * 8), i));
        s += line;
    }
    return s;
}

static int64_t port_write_s2(PortCtx & c, std::vector<uint8_t> & out, int64_t align,
                             int64_t only_meta) {
    out.resize(1 << 20);
    const int64_t n = s2_write(c.A(c.st), c.S(c.pool, sizeof c.pool), c.A(c.ko), c.A(c.kl),
                               c.A(c.ty), c.A(c.ar), c.A(c.v0), c.A(c.v1), c.A(c.v2),
                               c.S(out.data(), (int64_t) out.size()), 3, align, only_meta);
    out.resize(n > 0 ? (size_t) n : 0);
    return n;
}

static int64_t port_write_s3(PortCtx & c, std::vector<uint8_t> & out, int64_t only_meta) {
    out.resize(1 << 20);
    const int64_t n = s3_write(c.A(c.st), c.S(c.pool, sizeof c.pool), c.A(c.ko), c.A(c.kl),
                               c.A(c.ty), c.A(c.ar), c.A(c.v0), c.A(c.v1), c.A(c.v2),
                               c.A(c.tst, 4), c.S(c.tpool, sizeof c.tpool),
                               c.A(c.tne, PortCtx::TMAX * 4), c.A(c.trec, PortCtx::TMAX * 8),
                               c.S(out.data(), (int64_t) out.size()), 3, only_meta);
    out.resize(n > 0 ? (size_t) n : 0);
    return n;
}

static PortCtx g_port_ctx;
static std::vector<uint8_t> g_port_bytes;

static int g_checks = 0;
static int g_fail = 0;

static void fail(const char * name, const char * what, const std::string & detail) {
    g_fail++;
    printf("FAIL\t%s\t%s\t%s\n", name, what, detail.c_str());
}

static void expect(const char * name, const char * what, bool ok, const char * detail = "") {
    g_checks++;
    if (ok) printf("OK\t%s\t%s\n", name, what);
    else { g_fail++; printf("FAIL\t%s\t%s\t%s\n", name, what, detail); }
}

static void compare_bytes(const char * name, const char * what,
                          const std::vector<int8_t> & a, const std::vector<uint8_t> & b) {
    g_checks++;
    if (a.size() != b.size()) {
        char d[128];
        snprintf(d, sizeof d, "size native=%zu port=%zu", a.size(), b.size());
        fail(name, what, d);
        return;
    }
    for (size_t i = 0; i < a.size(); i++) {
        if ((uint8_t) a[i] != b[i]) {
            char d[256];
            const size_t lo = i > 8 ? i - 8 : 0;
            const size_t hi_n = i + 8 < a.size() ? i + 8 : a.size();
            snprintf(d, sizeof d, "first_diff=%zu native=%s port=%s", i,
                     native_hex((const uint8_t *) a.data() + lo, hi_n - lo).c_str(),
                     native_hex(b.data() + lo, hi_n - lo).c_str());
            fail(name, what, d);
            return;
        }
    }
    printf("OK\t%s\t%s\t%zu\n", name, what, a.size());
}

static std::vector<int8_t> native_write(gguf_context * c, bool only_meta, const std::string & path,
                                        bool & ok) {
    std::vector<int8_t> b;
    ok = true;
    if (only_meta) {
        const size_t n = gguf_get_meta_size(c);
        b.resize(n);
        gguf_get_meta_data(c, b.data());
    } else {
        const std::string full = path + ".full";
        if (!gguf_write_to_file(c, full.c_str(), false)) {
            ok = false;
            return b;
        }
        FILE * f = fopen(full.c_str(), "rb");
        if (!f) { ok = false; return b; }
        fseek(f, 0, SEEK_END);
        const long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        b.resize(n > 0 ? (size_t) n : 0);
        if (!b.empty() && fread(b.data(), 1, b.size(), f) != b.size()) ok = false;
        fclose(f);
        remove(full.c_str());
    }
    return b;
}

// A native context whose alignment is not the gguf_init_empty default. The only
// public way to get one is to read a file that carries general.alignment, so we
// write an alignment-only file with the port (metadata only, no tensors), reload
// it natively, and then append the remaining KVs in order. gguf_add_tensor on
// this context computes offsets with the loaded alignment, which makes the
// alignment 64 and 1 cases byte-comparable without any unreachable native
// setter.
static gguf_context * native_aligned_ctx(int64_t align, const std::vector<Op> & ops,
                                         const std::string & path, const char * case_name) {
    PortCtx seed;
    seed.init();
    seed.set_val("general.alignment", 4, align);
    std::vector<uint8_t> bytes;
    const int64_t n = port_write_s2(seed, bytes, align, 1);
    if (n <= 0) { fail(case_name, "align_seed", "port refused the seed file"); return nullptr; }
    FILE * f = fopen(path.c_str(), "wb");
    if (!f) { fail(case_name, "align_seed", "cannot open temp file"); return nullptr; }
    fwrite(bytes.data(), 1, bytes.size(), f);
    fclose(f);

    gguf_init_params params;
    params.no_alloc = true;
    params.ctx = nullptr;
    gguf_context * c = gguf_init_from_file(path.c_str(), params);
    remove(path.c_str());
    if (!c) { fail(case_name, "align_seed", "native reload failed"); return nullptr; }
    if ((int64_t) gguf_get_alignment(c) != align) {
        fail(case_name, "align_seed", "reloaded alignment differs");
        gguf_free(c);
        return nullptr;
    }
    // general.alignment is already first in the reloaded list; anything else is
    // appended in the case's order, exactly like the port saw it.
    for (const Op & o : ops) {
        if (o.kind == OP_VAL && strcmp(o.key, "general.alignment") == 0) continue;
        apply_native_op(c, o);
    }
    return c;
}


static void fill_pat(uint8_t * p, int64_t n, int seed) {
    for (int64_t i = 0; i < n; i++) p[i] = (uint8_t) ((seed * 53 + i * 97 + 11) & 255);
}

// ggml_nbytes of a contiguous tensor, from the native type table.
static int64_t nbytes_native(const TSpec & s, int type) {
    const int64_t blck = ggml_blck_size((ggml_type) type);
    const int64_t tsz = (int64_t) ggml_type_size((ggml_type) type);
    if (blck <= 0 || tsz <= 0) return -1;
    if (s.ne[0] % blck != 0) return -1;
    int64_t nb = tsz * (s.ne[0] / blck);
    for (int d = 1; d < GGML_MAX_DIMS; d++) nb *= s.ne[d];
    return nb;
}

// Build the native tensors exactly like the port is driven: add, optional
// retype, optional data.
static bool build_native_tensors(gguf_context * c, const std::vector<TSpec> & specs,
                                 const std::vector<std::vector<uint8_t>> & pats,
                                 const char * case_name) {
    ggml_init_params gp;
    gp.mem_size = 1u << 20;
    gp.mem_buffer = nullptr;
    gp.no_alloc = true;
    ggml_context * gctx = ggml_init(gp);
    if (!gctx) { fail(case_name, "ggml_init", "failed"); return false; }

    for (const TSpec & s : specs) {
        ggml_tensor * t = ggml_new_tensor_4d(gctx, (ggml_type) s.type_add,
                                             s.ne[0], s.ne[1], s.ne[2], s.ne[3]);
        if (!t) { fail(case_name, "ggml_new_tensor", s.name); ggml_free(gctx); return false; }
        ggml_set_name(t, s.name);
        gguf_add_tensor(c, t);
    }
    for (const TSpec & s : specs) {
        if (s.type_set >= 0) gguf_set_tensor_type(c, s.name, (ggml_type) s.type_set);
    }
    for (size_t j = 0; j < specs.size(); j++) {
        int type = specs[j].type_set >= 0 ? specs[j].type_set : specs[j].type_add;
        const int64_t want = nbytes_native(specs[j], type);
        const int64_t got = (int64_t) gguf_get_tensor_size(c, (int64_t) j);
        if (got != want) {
            char d[128];
            snprintf(d, sizeof d, "tensor %s native_size=%lld formula=%lld",
                     specs[j].name, (long long) got, (long long) want);
            fail(case_name, "fixture_size", d);
        }
        if (specs[j].with_data) {
            if ((size_t) want != pats[j].size()) {
                fail(case_name, "fixture_pat", "pattern length mismatch");
                continue;
            }
            gguf_set_tensor_data(c, specs[j].name, pats[j].data());
        }
    }
    ggml_free(gctx);
    return true;
}

static bool build_port_tensors(PortCtx & c, const std::vector<TSpec> & specs,
                               const std::vector<std::vector<uint8_t>> & pats,
                               const char * case_name) {
    for (const TSpec & s : specs) {
        sl_i64 ne{ (int64_t *) s.ne, 4 };
        sl_u8 empty{ (uint8_t *) "", 0 };
        const int64_t idx = s3_add_tensor(c.A(c.tst, 4), c.S(c.tpool, sizeof c.tpool),
                                          c.A(c.tne, PortCtx::TMAX * 4),
                                          c.A(c.trec, PortCtx::TMAX * 8),
                                          c.S((void *) s.name, (int64_t) strlen(s.name)),
                                          s.ndims, ne, s.type_add, empty);
        if (idx < 0) { fail(case_name, "port_add", s.name); return false; }
    }
    for (const TSpec & s : specs) {
        if (s.type_set >= 0) {
            const int64_t rc = s3_set_tensor_type(c.A(c.tst, 4), c.S(c.tpool, sizeof c.tpool),
                                                  c.A(c.tne, PortCtx::TMAX * 4),
                                                  c.A(c.trec, PortCtx::TMAX * 8),
                                                  c.S((void *) s.name, (int64_t) strlen(s.name)),
                                                  s.type_set);
            if (rc != 0) { fail(case_name, "port_set_type", s.name); return false; }
        }
    }
    for (size_t j = 0; j < specs.size(); j++) {
        if (!specs[j].with_data) continue;
        const int64_t rc = s3_set_tensor_data(c.A(c.tst, 4), c.S(c.tpool, sizeof c.tpool),
                                              c.A(c.tne, PortCtx::TMAX * 4),
                                              c.A(c.trec, PortCtx::TMAX * 8),
                                              c.S((void *) specs[j].name,
                                                  (int64_t) strlen(specs[j].name)),
                                              c.S((void *) pats[j].data(),
                                                  (int64_t) pats[j].size()));
        if (rc != 0) { fail(case_name, "port_set_data", specs[j].name); return false; }
    }
    return true;
}

// Write the port's full bytes to a file, reload them with the native reader
// (no_alloc=false), verify the alignment, the descriptor dump and the tensor
// data, then re-serialize with the native writer and compare both sections.
static void roundtrip(const Case & cs, const std::vector<TSpec> & specs,
                      const std::vector<std::vector<uint8_t>> & pats,
                      const std::vector<uint8_t> & meta,
                      const std::vector<uint8_t> & full,
                      const std::string & path) {
    FILE * f = fopen(path.c_str(), "wb");
    if (!f) { fail(cs.name, "reload", "cannot open temp file"); return; }
    fwrite(full.data(), 1, full.size(), f);
    fclose(f);

    ggml_context * gctx = nullptr;
    gguf_init_params params;
    params.no_alloc = false;
    params.ctx = &gctx;
    gguf_context * r = gguf_init_from_file(path.c_str(), params);
    remove(path.c_str());
    if (!r) { fail(cs.name, "reload", "native reload failed"); return; }

    g_checks++;
    if (gctx == nullptr) {
        fail(cs.name, "reload_ctx", "ggml context missing");
        gguf_free(r);
        return;
    }

    g_checks++;
    if ((int64_t) gguf_get_alignment(r) != cs.align) {
        char d[128];
        snprintf(d, sizeof d, "native alignment=%zu want=%lld", gguf_get_alignment(r),
                 (long long) cs.align);
        fail(cs.name, "reload_align", d);
    } else {
        printf("OK\t%s\treload_align\t%lld\n", cs.name, (long long) cs.align);
    }

    const std::string nd = native_tensor_dump(r);
    const std::string pd = port_tensor_dump(g_port_ctx);
    g_checks++;
    if (nd != pd) fail(cs.name, "reload_tensors", "descriptor dump mismatch");
    else printf("OK\t%s\treload_tensors\t%zu\n", cs.name, nd.size());

    for (const TSpec & s : specs) {
        if (!s.with_data) continue;
        const ggml_tensor * t = ggml_get_tensor(gctx, s.name);
        g_checks++;
        if (!t || !t->data) {
            fail(cs.name, "reload_data", s.name);
            continue;
        }
        const size_t j = (size_t) (&s - specs.data());
        if (memcmp(t->data, pats[j].data(), pats[j].size()) != 0) {
            fail(cs.name, "reload_data", s.name);
        } else {
            printf("OK\t%s\treload_data\t%s\n", cs.name, s.name);
        }
    }

    // the native writer needs its own data pointers after a reload
    for (const TSpec & s : specs) {
        if (!s.with_data) continue;
        const size_t j = (size_t) (&s - specs.data());
        gguf_set_tensor_data(r, s.name, pats[j].data());
    }

    // the reloaded context cannot be fully re-serialized: the native reader
    // leaves ggml_tensor::buffer uninitialized, so gguf_write_to_file would take
    // the backend-copy branch. Metadata and tensor data are still verified
    // (above), and the full layout below: every byte between a tensor's data and
    // the next padded offset must be zero.
    bool wok = false;
    const std::vector<int8_t> nm = native_write(r, true, path, wok);
    if (!wok) fail(cs.name, "reload_meta", "native meta rewrite failed");
    else compare_bytes(cs.name, "reload_meta", nm, meta);

    const size_t data_start = meta.size();
    g_checks++;
    bool pad_ok = true;
    for (const TSpec & s : specs) {
        if (!s.with_data) continue;
        const size_t j = (size_t) (&s - specs.data());
        const int64_t rel = g_port_ctx.trec[j * 8 + 6];
        const int64_t nb = (int64_t) pats[j].size();
        const int64_t end = (rel + nb + cs.align - 1) / cs.align * cs.align;
        if (data_start + (size_t) rel + (size_t) nb > full.size() ||
            data_start + (size_t) end > full.size()) {
            pad_ok = false;
            break;
        }
        for (int64_t b = rel + nb; b < end; b++) {
            if (full[data_start + (size_t) b] != 0) { pad_ok = false; break; }
        }
        if (!pad_ok) break;
    }
    if (!pad_ok) fail(cs.name, "reload_pad", "non-zero or truncated padding");
    else printf("OK\t%s\treload_pad\t%zu\n", cs.name, data_start);

    gguf_free(r);
    ggml_free(gctx);
}

static int64_t sum_padded(const std::vector<TSpec> & specs, int64_t align) {
    int64_t total = 0;
    for (const TSpec & s : specs) {
        int type = s.type_set >= 0 ? s.type_set : s.type_add;
        total += (nbytes_native(s, type) + align - 1) / align * align;
    }
    return total;
}

static void run_case(const Case & cs, const std::vector<Op> & ops,
                     const std::vector<TSpec> & specs, const std::string & tmp) {
    PortCtx & pc = g_port_ctx;
    pc.init();
    pc.init_tensors(cs.align);
    apply_port(pc, ops);

    std::vector<std::vector<uint8_t>> pats(specs.size());
    for (size_t j = 0; j < specs.size(); j++) {
        const int type = specs[j].type_set >= 0 ? specs[j].type_set : specs[j].type_add;
        const int64_t nb = nbytes_native(specs[j], type);
        if (nb < 0) { fail(cs.name, "fixture_nbytes", specs[j].name); return; }
        if (specs[j].with_data) {
            pats[j].resize((size_t) nb);
            fill_pat(pats[j].data(), nb, specs[j].seed);
        }
    }
    if (!build_port_tensors(pc, specs, pats, cs.name)) return;

    std::vector<uint8_t> pmeta, pfull;
    const int64_t nmeta = port_write_s3(pc, pmeta, 1);
    g_checks++;
    if (nmeta < 0) { fail(cs.name, "port_meta", "writer refused"); return; }
    printf("OK\t%s\tport_meta\t%lld\n", cs.name, (long long) nmeta);

    if (cs.mode == MODE_META_ONLY) {
        const int64_t nfull = port_write_s3(pc, pfull, 0);
        expect(cs.name, "port_full_refused_without_data", nfull < 0, "writer accepted");
        pfull.clear();

        gguf_context * nc = gguf_init_empty();
        apply_native(nc, ops);
        build_native_tensors(nc, specs, pats, cs.name);
        const std::string nd = native_dump(nc);
        const std::string pd = port_dump(pc);
        g_checks++;
        if (nd != pd) fail(cs.name, "kv_tsv", "native and port KV dumps differ");
        else printf("OK\t%s\tkv_tsv\t%zu\n", cs.name, nd.size());
        const std::string ntd = native_tensor_dump(nc);
        const std::string ptd = port_tensor_dump(pc);
        g_checks++;
        if (ntd != ptd) fail(cs.name, "tensor_tsv", "descriptor dumps differ");
        else printf("OK\t%s\ttensor_tsv\t%zu\n", cs.name, ntd.size());
        bool wok = false;
        const std::vector<int8_t> nm = native_write(nc, true, tmp, wok);
        if (!wok) fail(cs.name, "meta", "native meta write failed");
        else compare_bytes(cs.name, "meta", nm, pmeta);
        gguf_free(nc);
        return;
    }

    const int64_t nfull = port_write_s3(pc, pfull, 0);
    g_checks++;
    if (nfull < 0) { fail(cs.name, "port_full", "writer refused"); return; }
    printf("OK\t%s\tport_full\t%lld\n", cs.name, (long long) nfull);

    // only_meta output must be a prefix of the full output, and the tail must
    // be exactly the padded tensor data.
    g_checks++;
    if (pfull.size() != pmeta.size() + (size_t) sum_padded(specs, cs.align) ||
        memcmp(pfull.data(), pmeta.data(), pmeta.size()) != 0) {
        fail(cs.name, "meta_prefix", "meta is not the prefix of the data section");
    } else {
        printf("OK\t%s\tmeta_prefix\t%zu\n", cs.name, pmeta.size());
    }

    gguf_context * nc = nullptr;
    if (cs.mode == MODE_DIRECT) {
        nc = gguf_init_empty();
        apply_native(nc, ops);
    } else {
        nc = native_aligned_ctx(cs.align, ops, tmp + ".seed", cs.name);
    }
    if (nc) {
        build_native_tensors(nc, specs, pats, cs.name);

        const std::string nd = native_dump(nc);
        const std::string pd = port_dump(pc);
        g_checks++;
        if (nd != pd) fail(cs.name, "kv_tsv", "native and port KV dumps differ");
        else printf("OK\t%s\tkv_tsv\t%zu\n", cs.name, nd.size());

        const std::string ntd = native_tensor_dump(nc);
        const std::string ptd = port_tensor_dump(pc);
        g_checks++;
        if (ntd != ptd) fail(cs.name, "tensor_tsv", "descriptor dumps differ");
        else printf("OK\t%s\ttensor_tsv\t%zu\n", cs.name, ntd.size());

        bool wok = false;
        const std::vector<int8_t> nm = native_write(nc, true, tmp, wok);
        if (!wok) fail(cs.name, "meta", "native meta write failed");
        else compare_bytes(cs.name, "meta", nm, pmeta);

        wok = false;
        const std::vector<int8_t> nf = native_write(nc, false, tmp, wok);
        if (!wok) fail(cs.name, "full", "native full write failed");
        else compare_bytes(cs.name, "full", nf, pfull);

        gguf_free(nc);
    }

    roundtrip(cs, specs, pats, pmeta, pfull, tmp);
}

// Port-only refusals. Native aborts (GGML_ABORT) on a duplicate name, an
// unknown tensor and a missing data pointer, so those cannot run in the same
// process; the port must return -1 instead.
static void run_negative() {
    const char * name = "negative";
    PortCtx & pc = g_port_ctx;
    pc.init();
    pc.init_tensors(32);
    pc.set_val("general.alignment", 4, 32);

    int64_t ne[4] = {2, 2, 1, 1};
    sl_i64 ne4{ ne, 4 };
    sl_u8 empty{ (uint8_t *) "", 0 };
    const char * dup = "dup";
    const int64_t i0 = s3_add_tensor(pc.A(pc.tst, 4), pc.S(pc.tpool, sizeof pc.tpool),
                                     pc.A(pc.tne, PortCtx::TMAX * 4),
                                     pc.A(pc.trec, PortCtx::TMAX * 8),
                                     pc.S((void *) dup, 3), 2, ne4, GGML_TYPE_F32, empty);
    expect(name, "add_first", i0 == 0);

    const int64_t i1 = s3_add_tensor(pc.A(pc.tst, 4), pc.S(pc.tpool, sizeof pc.tpool),
                                     pc.A(pc.tne, PortCtx::TMAX * 4),
                                     pc.A(pc.trec, PortCtx::TMAX * 8),
                                     pc.S((void *) dup, 3), 2, ne4, GGML_TYPE_F32, empty);
    expect(name, "duplicate_name_refused", i1 < 0);
    expect(name, "duplicate_keeps_count", pc.tst[0] == 1);

    expect(name, "find_present",
           s3_find(pc.A(pc.tst, 4), pc.S(pc.tpool, sizeof pc.tpool),
                   pc.A(pc.trec, PortCtx::TMAX * 8), pc.S((void *) dup, 3)) == 0);
    expect(name, "find_absent",
           s3_find(pc.A(pc.tst, 4), pc.S(pc.tpool, sizeof pc.tpool),
                   pc.A(pc.trec, PortCtx::TMAX * 8), pc.S((void *) "zz", 2)) < 0);

    uint8_t d16[16];
    fill_pat(d16, 16, 1);
    expect(name, "set_data",
           s3_set_tensor_data(pc.A(pc.tst, 4), pc.S(pc.tpool, sizeof pc.tpool),
                              pc.A(pc.tne, PortCtx::TMAX * 4),
                              pc.A(pc.trec, PortCtx::TMAX * 8),
                              pc.S((void *) dup, 3), pc.S(d16, 16)) == 0);

    expect(name, "set_type_missing_refused",
           s3_set_tensor_type(pc.A(pc.tst, 4), pc.S(pc.tpool, sizeof pc.tpool),
                              pc.A(pc.tne, PortCtx::TMAX * 4),
                              pc.A(pc.trec, PortCtx::TMAX * 8),
                              pc.S((void *) "nope", 4), GGML_TYPE_F16) < 0);
    expect(name, "set_data_missing_refused",
           s3_set_tensor_data(pc.A(pc.tst, 4), pc.S(pc.tpool, sizeof pc.tpool),
                              pc.A(pc.tne, PortCtx::TMAX * 4),
                              pc.A(pc.trec, PortCtx::TMAX * 8),
                              pc.S((void *) "nope", 4), pc.S(d16, 16)) < 0);

    int64_t ne5[4] = {2, 1, 1, 1};
    expect(name, "bad_ndims_refused",
           s3_add_tensor(pc.A(pc.tst, 4), pc.S(pc.tpool, sizeof pc.tpool),
                         pc.A(pc.tne, PortCtx::TMAX * 4),
                         pc.A(pc.trec, PortCtx::TMAX * 8),
                         pc.S((void *) "x", 1), 5, sl_i64{ ne5, 4 }, GGML_TYPE_F32, empty) < 0);
    expect(name, "empty_name_refused",
           s3_add_tensor(pc.A(pc.tst, 4), pc.S(pc.tpool, sizeof pc.tpool),
                         pc.A(pc.tne, PortCtx::TMAX * 4),
                         pc.A(pc.trec, PortCtx::TMAX * 8),
                         sl_u8{ empty.data, 0 }, 1, sl_i64{ ne5, 4 }, GGML_TYPE_F32, empty) < 0);
    expect(name, "removed_type_refused",
           s3_add_tensor(pc.A(pc.tst, 4), pc.S(pc.tpool, sizeof pc.tpool),
                         pc.A(pc.tne, PortCtx::TMAX * 4),
                         pc.A(pc.trec, PortCtx::TMAX * 8),
                         pc.S((void *) "gone", 4), 1, sl_i64{ ne5, 4 }, 4, empty) < 0);
    expect(name, "bad_alignment_refused",
           s3_init_tensors(pc.A(pc.tst, 4), 24) < 0);

    // re-init and drive the short-data path: f32 [2,2] with 16 bytes, then a
    // retype to f64 that needs 32 bytes. Meta still writes, full refuses.
    pc.init();
    pc.init_tensors(32);
    pc.set_val("general.alignment", 4, 32);
    const int64_t a0 = s3_add_tensor(pc.A(pc.tst, 4), pc.S(pc.tpool, sizeof pc.tpool),
                                     pc.A(pc.tne, PortCtx::TMAX * 4),
                                     pc.A(pc.trec, PortCtx::TMAX * 8),
                                     pc.S((void *) dup, 3), 2, ne4, GGML_TYPE_F32, empty);
    expect(name, "short_add", a0 == 0);
    expect(name, "short_set_data",
           s3_set_tensor_data(pc.A(pc.tst, 4), pc.S(pc.tpool, sizeof pc.tpool),
                              pc.A(pc.tne, PortCtx::TMAX * 4),
                              pc.A(pc.trec, PortCtx::TMAX * 8),
                              pc.S((void *) dup, 3), pc.S(d16, 16)) == 0);
    expect(name, "short_retype",
           s3_set_tensor_type(pc.A(pc.tst, 4), pc.S(pc.tpool, sizeof pc.tpool),
                              pc.A(pc.tne, PortCtx::TMAX * 4),
                              pc.A(pc.trec, PortCtx::TMAX * 8),
                              pc.S((void *) dup, 3), GGML_TYPE_F64) == 0);
    std::vector<uint8_t> out;
    out.resize(1 << 20);
    int64_t nf = s3_write(pc.A(pc.st), pc.S(pc.pool, sizeof pc.pool), pc.A(pc.ko), pc.A(pc.kl),
                          pc.A(pc.ty), pc.A(pc.ar), pc.A(pc.v0), pc.A(pc.v1), pc.A(pc.v2),
                          pc.A(pc.tst, 4), pc.S(pc.tpool, sizeof pc.tpool),
                          pc.A(pc.tne, PortCtx::TMAX * 4), pc.A(pc.trec, PortCtx::TMAX * 8),
                          pc.S(out.data(), (int64_t) out.size()), 3, 0);
    expect(name, "short_data_full_refused", nf < 0);
    int64_t nm = s3_write(pc.A(pc.st), pc.S(pc.pool, sizeof pc.pool), pc.A(pc.ko), pc.A(pc.kl),
                          pc.A(pc.ty), pc.A(pc.ar), pc.A(pc.v0), pc.A(pc.v1), pc.A(pc.v2),
                          pc.A(pc.tst, 4), pc.S(pc.tpool, sizeof pc.tpool),
                          pc.A(pc.tne, PortCtx::TMAX * 4), pc.A(pc.trec, PortCtx::TMAX * 8),
                          pc.S(out.data(), (int64_t) out.size()), 3, 1);
    expect(name, "short_data_meta_ok", nm > 0);

    // a tensor with no data image at all: meta writes, full refuses
    pc.init();
    pc.init_tensors(32);
    pc.set_val("general.alignment", 4, 32);
    int64_t ne2[4] = {2, 1, 1, 1};
    const int64_t b0 = s3_add_tensor(pc.A(pc.tst, 4), pc.S(pc.tpool, sizeof pc.tpool),
                                     pc.A(pc.tne, PortCtx::TMAX * 4),
                                     pc.A(pc.trec, PortCtx::TMAX * 8),
                                     pc.S((void *) "nodata", 6), 1, sl_i64{ ne2, 4 },
                                     GGML_TYPE_F32, empty);
    expect(name, "nodata_add", b0 == 0);
    nf = s3_write(pc.A(pc.st), pc.S(pc.pool, sizeof pc.pool), pc.A(pc.ko), pc.A(pc.kl),
                  pc.A(pc.ty), pc.A(pc.ar), pc.A(pc.v0), pc.A(pc.v1), pc.A(pc.v2),
                  pc.A(pc.tst, 4), pc.S(pc.tpool, sizeof pc.tpool),
                  pc.A(pc.tne, PortCtx::TMAX * 4), pc.A(pc.trec, PortCtx::TMAX * 8),
                  pc.S(out.data(), (int64_t) out.size()), 3, 0);
    expect(name, "nodata_full_refused", nf < 0);
    nm = s3_write(pc.A(pc.st), pc.S(pc.pool, sizeof pc.pool), pc.A(pc.ko), pc.A(pc.kl),
                  pc.A(pc.ty), pc.A(pc.ar), pc.A(pc.v0), pc.A(pc.v1), pc.A(pc.v2),
                  pc.A(pc.tst, 4), pc.S(pc.tpool, sizeof pc.tpool),
                  pc.A(pc.tne, PortCtx::TMAX * 4), pc.A(pc.trec, PortCtx::TMAX * 8),
                  pc.S(out.data(), (int64_t) out.size()), 3, 1);
    expect(name, "nodata_meta_ok", nm > 0);
}

int main(int argc, char ** argv) {
    goose_init(argc, argv);

    // ---- S2 regression cases: no tensors, s2_write unchanged --------------
    add_case("empty", {});

    struct Sc { const char * label; int t; uint64_t bits; } scalars[] = {
        {"u8", 0, 250}, {"i8", 1, (uint8_t) -5}, {"u16", 2, 65000},
        {"i16", 3, (uint16_t) -1234}, {"u32", 4, 4000000000ull},
        {"i32", 5, (uint32_t) -2000000000}, {"u64", 10, 0x8000000000000005ull},
        {"i64", 11, 0xC000000000000000ull}, {"bool", 7, 1},
    };
    for (const Sc & s : scalars) add_case(s.label, { val("key", s.t, s.bits) });

    { float v = 1.5f; uint32_t b; memcpy(&b, &v, 4);
      add_case("f32", { val("key", 6, b) }); }
    { double v = -3.25; uint64_t b; memcpy(&b, &v, 8);
      add_case("f64", { val("key", 12, b) }); }
    add_case("str", { str("key", "hello") });
    add_case("str_empty", { str("key", "") });

    static uint8_t arr_u8[] = {1, 2, 255};
    static int32_t arr_i32[] = {-1, 0, 7};
    static float arr_f32[] = {0.5f, -1.5f};
    static uint8_t arr_bool[] = {1, 0, 1};
    static const char * arr_str3[] = {"a", "bc", ""};
    add_case("arr_u8",   { arr("a", 0, arr_u8, 3, 3) });
    add_case("arr_i32",  { arr("a", 5, arr_i32, 12, 3) });
    add_case("arr_f32",  { arr("a", 6, arr_f32, 8, 2) });
    add_case("arr_bool", { arr("a", 7, arr_bool, 3, 3) });
    add_case("arr_str",  { arr_str("a", arr_str3, 3) });
    add_case("arr_str_empty", { arr_str("a", arr_str3, 0) });
    add_case("arr_u8_empty",  { arr("a", 0, arr_u8, 0, 0) });

    add_case("replace_reserved", {
        val("k", 4, 1), val("k", 4, 2), val("general.alignment", 4, 64) });
    add_case("remove", {
        val("a", 0, 1), val("b", 0, 2), val("c", 0, 3),
        remove_op("b"), remove_op("zz") });

    static const uint32_t kv_arr[] = {7, 8};
    static const char * kv_strs[] = {"x", "yz"};
    static const std::vector<Op> kv_src = {
        val("s_u16", 2, 60000), str("s_str", "copy"),
        arr("s_arr", 4, kv_arr, 8, 2),
        arr_str("s_sarr", kv_strs, 2),
    };
    add_case("set_kv", { val("s_u16", 2, 1), kv_copy(&kv_src) });

    static char long_key[41], long_val[301], long_elem[201];
    memset(long_key, 'k', 40); long_key[40] = 0;
    memset(long_val, 'x', 300); long_val[300] = 0;
    memset(long_elem, 'y', 200); long_elem[200] = 0;
    static const char * arr_long[] = {long_elem, "z", ""};
    add_case("long_str", { str(long_key, long_val) });
    add_case("long_arr_str", { arr_str(long_key, arr_long, 3) });
    add_case("utf8", {
        str("\xd0\xba\xd0\xbb\xd1\x8e\xd1\x87", "\xd0\xb7\xd0\xbd\xd0\xb0\xd1\x87\xd0\xb5\xd0\xbd\xd0\xb8\xd0\xb5/\xe6\xa8\xa1\xe5\x9e\x8b"),
        val("\xf0\x9f\x98\x80", 4, 7) });

    static const char * mixed_strs[] = {"q", "rr"};
    static const uint16_t mixed_u16[] = {1, 65535};
    add_case("mixed", {
        val("z_u8", 0, 9), str("a_str", "order"), val("m_i32", 5, (uint32_t) -7),
        arr_str("b_arr_str", mixed_strs, 2),
        val("y_bool", 7, 0), val("x_f64", 12, 0x400921fb54442d18ull),
        arr("w_arr_u16", 2, mixed_u16, 4, 2) });

    add_case("align64_mixed", {
        val("general.alignment", 4, 64), val("a", 4, 1), str("b", "pad"),
        arr("c", 0, arr_u8, 3, 3) }, 64, MODE_RELOAD);
    add_case("align64_kv_only", {
        val("general.alignment", 4, 64) }, 64, MODE_RELOAD);
    add_case("align1_mixed", {
        val("general.alignment", 4, 1), val("a", 4, 1), str("b", "pad") }, 1, MODE_RELOAD);

    // ---- S3 tensor cases --------------------------------------------------
    // no tensors through s3_write must equal the S2 bytes
    add_tcase("t_none", { val("general.alignment", 4, 32) }, {});

    add_tcase("t_one", {}, { ts("w", 2, 2, 1, 1, 2, GGML_TYPE_F32, -1, 1) });

    // the required four: f32 [2,2], f16 [4,2], q8_0 [32], i32 [3]
    const std::vector<TSpec> four = {
        ts("blk.0", 2, 2, 1, 1, 2, GGML_TYPE_F32, -1, 1),
        ts("blk.1", 4, 2, 1, 1, 2, GGML_TYPE_F16, -1, 2),
        ts("blk.2", 32, 1, 1, 1, 1, GGML_TYPE_Q8_0, -1, 3),
        ts("blk.3", 3, 1, 1, 1, 1, GGML_TYPE_I32, -1, 4),
    };
    add_tcase("t_four", {}, four);

    add_tcase("t_four_kv", {
        str("general.architecture", "synthetic"),
        str("general.name", "four tensors"),
        val("synthetic.block_count", 4, 1),
        val("general.alignment", 4, 32),
        val("synthetic.flag", 7, 0),
    }, four);

    // retype a middle tensor: the offsets of the following tensors must move
    add_tcase("t_retype_mid", {}, {
        ts("a", 8, 1, 1, 1, 1, GGML_TYPE_F32, -1, 5),
        ts("b", 8, 1, 1, 1, 1, GGML_TYPE_F32, GGML_TYPE_F16, 6),
        ts("c", 8, 1, 1, 1, 1, GGML_TYPE_F32, -1, 7),
    });

    // retype the first tensor: every later offset moves
    add_tcase("t_retype_first", {}, {
        ts("a", 4, 2, 1, 1, 2, GGML_TYPE_F32, GGML_TYPE_F16, 8),
        ts("b", 3, 1, 1, 1, 1, GGML_TYPE_I32, -1, 9),
    });

    // shapes and dims: 1D..4D plus a ne[1]=1 tensor declared with ndims=2
    add_tcase("t_shapes", {}, {
        ts("s0", 1, 1, 1, 1, 1, GGML_TYPE_F32, -1, 10),
        ts("s1", 3, 1, 1, 1, 1, GGML_TYPE_I32, -1, 11),
        ts("s2", 2, 3, 1, 1, 2, GGML_TYPE_F32, -1, 12),
        ts("s3", 2, 2, 2, 1, 3, GGML_TYPE_F16, -1, 13),
        ts("s4", 2, 2, 2, 2, 4, GGML_TYPE_I8, -1, 14),
        ts("s5", 4, 1, 1, 1, 2, GGML_TYPE_F32, -1, 15),
    });

    // alignment 64: reload path, offsets and data padded to 64
    add_tcase("t_align64", {
        val("general.alignment", 4, 64),
        str("general.architecture", "synthetic"),
    }, four, 64, MODE_RELOAD);

    // alignment 1: no padding anywhere in the data section
    add_tcase("t_align1", { val("general.alignment", 4, 1) }, {
        ts("a", 3, 1, 1, 1, 1, GGML_TYPE_F32, -1, 16),
        ts("b", 5, 1, 1, 1, 1, GGML_TYPE_I32, -1, 17),
    }, 1, MODE_RELOAD);

    // descriptors without data: only_meta must still be byte-exact, the full
    // write must refuse instead of reading a missing pointer
    add_tcase("t_meta_no_data", {
        val("general.alignment", 4, 32),
    }, { ts_nodata("nd0", 4, 1, GGML_TYPE_F32),
         ts_nodata("nd1", 32, 1, GGML_TYPE_Q8_0) }, 32, MODE_META_ONLY);

    const std::string tmp = argc > 1 ? argv[1] : "align_tmp.bin";

    for (size_t ci = 0; ci < g_cases.size(); ci++) {
        const Case & cs = g_cases[ci];
        const std::vector<Op> & ops = g_ops[ci];
        const std::vector<TSpec> & specs = g_tensors[ci];

        if (!specs.empty()) {
            run_case(cs, ops, specs, tmp);
            continue;
        }

        // no tensors: the S2 path, s2_write keeps its original contract
        gguf_context * nctx = gguf_init_empty();
        apply_native(nctx, ops);
        g_port_ctx.init();
        apply_port(g_port_ctx, ops);

        const std::string nd = native_dump(nctx);
        const std::string pd = port_dump(g_port_ctx);
        g_checks++;
        if (nd != pd) fail(cs.name, "kv_tsv", "native and port KV dumps differ");
        else printf("OK\t%s\tkv_tsv\t%zu\n", cs.name, nd.size());

        if (cs.mode == MODE_DIRECT) {
            for (int meta = 0; meta < 2; meta++) {
                const int64_t n = port_write_s2(g_port_ctx, g_port_bytes, 32, meta);
                bool wok = false;
                const std::vector<int8_t> nb = native_write(nctx, meta != 0, tmp, wok);
                if (!wok) { g_checks++; fail(cs.name, "native_write", "writer refused"); continue; }
                char what[32];
                snprintf(what, sizeof what, "%s", meta ? "meta" : "full");
                if ((int64_t) nb.size() != n) {
                    g_checks++;
                    char d[128];
                    snprintf(d, sizeof d, "port_len=%lld native_len=%zu",
                             (long long) n, nb.size());
                    fail(cs.name, what, d);
                } else {
                    compare_bytes(cs.name, what, nb, g_port_bytes);
                }
                std::vector<uint8_t> again;
                port_write_s2(g_port_ctx, again, 32, meta ? 0 : 1);
                g_checks++;
                if (again != g_port_bytes) fail(cs.name, "only_meta_invariance", "port bytes differ");
            }
        } else {
            for (int meta = 0; meta < 2; meta++) {
                const int64_t n = port_write_s2(g_port_ctx, g_port_bytes, cs.align, meta);
                // reload the port bytes, assert alignment, rewrite natively
                FILE * f = fopen(tmp.c_str(), "wb");
                if (!f) { fail(cs.name, "reload", "cannot open temp file"); break; }
                fwrite(g_port_bytes.data(), 1, g_port_bytes.size(), f);
                fclose(f);
                gguf_init_params params;
                params.no_alloc = true;
                params.ctx = nullptr;
                gguf_context * r = gguf_init_from_file(tmp.c_str(), params);
                remove(tmp.c_str());
                if (!r) { fail(cs.name, "reload", "native reload failed"); break; }
                g_checks++;
                if ((int64_t) gguf_get_alignment(r) != cs.align) {
                    char d[128];
                    snprintf(d, sizeof d, "native alignment=%zu want=%lld",
                             gguf_get_alignment(r), (long long) cs.align);
                    fail(cs.name, "reload_align", d);
                } else {
                    printf("OK\t%s\treload_align\t%lld\n", cs.name, (long long) cs.align);
                }
                bool wok = false;
                const std::vector<int8_t> nb = native_write(r, meta != 0, tmp, wok);
                gguf_free(r);
                if (!wok) { fail(cs.name, "reload_write", "native rewrite failed"); break; }
                char what[32];
                snprintf(what, sizeof what, "reload_%s", meta ? "meta" : "full");
                compare_bytes(cs.name, what, nb, g_port_bytes);
                (void) n;
            }
        }
        gguf_free(nctx);
    }

    run_negative();

    printf("FG_G3 cases=%zu checks=%d failures=%d\n", g_cases.size(), g_checks, g_fail);
    return g_fail == 0 ? 0 : 1;
}
