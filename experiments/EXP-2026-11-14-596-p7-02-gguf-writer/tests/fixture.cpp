// Gate fixture for the P7-02 S2 GGUF writer port.
//
// One binary, two implementations of the same metadata cases:
//   - native: gguf_set_* + gguf_write_to_buf from ggml/src/gguf.cpp;
//   - port:   s1_* + s2_write from source/gguf-writer.goose.
//
// For every case the fixture compares the canonical KV TSV and then the
// serialized bytes for only_meta=true and only_meta=false. Cases marked
// `reload` re-read the port bytes with gguf_init_from_file (the only public way
// to give a native context a non-default `alignment`) and re-serialize with the
// native writer before comparing, so alignment 64 and 1 are still checked
// against native code.
//
// Exit code is 1 when any comparison fails.
#include "gguf.h"
#include "gguf_writer.h"

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

enum CaseMode { MODE_DIRECT, MODE_RELOAD };

struct Case {
    const char * name;
    int64_t align;
    CaseMode mode;
};

static std::vector<Case> g_cases;
static std::vector<std::vector<Op>> g_ops;

static void add_case(const char * name, std::vector<Op> ops, int64_t align = 32,
                     CaseMode mode = MODE_DIRECT) {
    g_cases.push_back(Case{name, align, mode});
    g_ops.push_back(std::move(ops));
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

static void apply_native(gguf_context * c, const std::vector<Op> & ops) {
    for (const Op & o : ops) {
        switch (o.kind) {
            case OP_VAL:   set_native_scalar(c, o.key, o.type, o.bits); break;
            case OP_STR:   gguf_set_val_str(c, o.key, o.text); break;
            case OP_ARR:   gguf_set_arr_data(c, o.key, (gguf_type) o.type, o.data, (size_t) o.n); break;
            case OP_ARRSTR: gguf_set_arr_str(c, o.key, o.strs, (size_t) o.n); break;
            case OP_REMOVE: gguf_remove_key(c, o.key); break;
            case OP_KVCOPY: {
                gguf_context * src = gguf_init_empty();
                apply_native(src, *o.sub);
                gguf_set_kv(c, src);
                gguf_free(src);
                break;
            }
        }
    }
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

// ---------------------------------------------------------------- port state

struct PortCtx {
    static const int MAX = 64;
    int64_t st[8];
    unsigned char pool[8192];
    int64_t ko[MAX], kl[MAX], ty[MAX], ar[MAX], v0[MAX], v1[MAX], v2[MAX];
    int64_t out[2];

    sl_i64 A(int64_t * p) { return sl_i64{ p, MAX }; }
    sl_u8  S(void * p, int64_t n) { return sl_u8{ (uint8_t *) p, n }; }

    void init() { s1_init(A(st), A(ko), A(kl), A(ty), A(ar), A(v0), A(v1), A(v2)); }

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

static int64_t port_write(PortCtx & c, std::vector<uint8_t> & out, int64_t align,
                          int64_t only_meta) {
    out.resize(1 << 20);
    const int64_t n = s2_write(c.A(c.st), c.S(c.pool, sizeof c.pool), c.A(c.ko), c.A(c.kl),
                               c.A(c.ty), c.A(c.ar), c.A(c.v0), c.A(c.v1), c.A(c.v2),
                               c.S(out.data(), (int64_t) out.size()), 3, align, only_meta);
    out.resize(n > 0 ? (size_t) n : 0);
    return n;
}

// ------------------------------------------------------------------- harness

static PortCtx g_port_ctx;
static std::vector<uint8_t> g_port_bytes;

static int g_checks = 0;
static int g_fail = 0;

static void fail(const char * name, const char * what, const std::string & detail) {
    g_fail++;
    printf("FAIL\t%s\t%s\t%s\n", name, what, detail.c_str());
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

// gguf_write_to_buf is not in the public header; gguf_get_meta_size/data and
// gguf_write_to_file are, and they call the same writer with only_meta=true and
// only_meta=false respectively.
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

static bool write_reload_case(const char * name, int64_t align,
                              bool only_meta, const std::string & path, int64_t want_size) {
    const std::string what = only_meta ? "reload_meta" : "reload_full";
    FILE * f = fopen(path.c_str(), "wb");
    if (!f) { fail(name, what.c_str(), "cannot open temp file"); return false; }
    fwrite(g_port_bytes.data(), 1, g_port_bytes.size(), f);
    fclose(f);

    gguf_init_params params;
    params.no_alloc = true;
    params.ctx = nullptr;
    gguf_context * ctx = gguf_init_from_file(path.c_str(), params);
    remove(path.c_str());
    if (!ctx) { fail(name, what.c_str(), "native reload failed"); return false; }

    g_checks++;
    if ((int64_t) gguf_get_alignment(ctx) != align) {
        char d[128];
        snprintf(d, sizeof d, "native alignment=%zu want=%lld", gguf_get_alignment(ctx),
                 (long long) align);
        fail(name, what.c_str(), d);
        gguf_free(ctx);
        return false;
    }
    const std::string nd = native_dump(ctx);
    const std::string pd = port_dump(g_port_ctx);
    g_checks++;
    if (nd != pd) fail(name, (what + "_kv").c_str(), "TSV mismatch");
    else printf("OK\t%s\t%s_kv\t%zu\n", name, what.c_str(), nd.size());

    bool wok = false;
    const std::vector<int8_t> nb = native_write(ctx, only_meta, path, wok);
    gguf_free(ctx);
    if (!wok) { fail(name, what.c_str(), "native rewrite failed"); return false; }
    if ((int64_t) nb.size() != want_size) {
        char d[128];
        snprintf(d, sizeof d, "port_written=%lld native_rewrite=%zu",
                 (long long) want_size, nb.size());
        fail(name, what.c_str(), d);
        return false;
    }
    compare_bytes(name, what.c_str(), nb, g_port_bytes);
    return true;
}

int main(int argc, char ** argv) {
    goose_init(argc, argv);

    // ---- writer cases -----------------------------------------------------
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

    // long and multibyte strings: u64 length prefix beyond one byte
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

    // one context with every group in a mixed order: the KV order must survive
    static const char * mixed_strs[] = {"q", "rr"};
    static const uint16_t mixed_u16[] = {1, 65535};
    add_case("mixed", {
        val("z_u8", 0, 9), str("a_str", "order"), val("m_i32", 5, (uint32_t) -7),
        arr_str("b_arr_str", mixed_strs, 2),
        val("y_bool", 7, 0), val("x_f64", 12, 0x400921fb54442d18ull),
        arr("w_arr_u16", 2, mixed_u16, 4, 2),
    });

    // alignment: native in-memory contexts always use 32, so non-default
    // alignment is checked by reloading the port bytes through the native reader
    add_case("align64_mixed", {
        val("general.alignment", 4, 64), val("a", 4, 1), str("b", "pad"),
        arr("c", 0, arr_u8, 3, 3) }, 64, MODE_RELOAD);
    add_case("align64_kv_only", {
        val("general.alignment", 4, 64) }, 64, MODE_RELOAD);
    add_case("align1_mixed", {
        val("general.alignment", 4, 1), val("a", 4, 1), str("b", "pad") }, 1, MODE_RELOAD);

    const std::string tmp = argc > 1 ? argv[1] : "align_tmp.bin";

    for (size_t ci = 0; ci < g_cases.size(); ci++) {
        const Case & cs = g_cases[ci];
        const std::vector<Op> & ops = g_ops[ci];

        gguf_context * nctx = gguf_init_empty();
        apply_native(nctx, ops);

        g_port_ctx.init();
        apply_port(g_port_ctx, ops);

        const std::string nd = native_dump(nctx);
        const std::string pd = port_dump(g_port_ctx);
        g_checks++;
        if (nd != pd) {
            fail(cs.name, "kv_tsv", "native and port KV dumps differ");
        } else {
            printf("OK\t%s\tkv_tsv\t%zu\n", cs.name, nd.size());
        }

        if (cs.mode == MODE_DIRECT) {
            for (int meta = 0; meta < 2; meta++) {
                const int64_t align = 32; // native gguf_init_empty default
                const int64_t n = port_write(g_port_ctx, g_port_bytes, align, meta);
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
                // port must ignore only_meta in the same way: both calls agree
                std::vector<uint8_t> again;
                port_write(g_port_ctx, again, align, meta ? 0 : 1);
                g_checks++;
                if (again != g_port_bytes) fail(cs.name, "only_meta_invariance", "port bytes differ");
            }
        } else {
            for (int meta = 0; meta < 2; meta++) {
                const int64_t n = port_write(g_port_ctx, g_port_bytes, cs.align, meta);
                write_reload_case(cs.name, cs.align, meta != 0, tmp, n);
            }
        }

        gguf_free(nctx);
    }

    printf("FG_G2 cases=%zu checks=%d failures=%d\n", g_cases.size(), g_checks, g_fail);
    return g_fail == 0 ? 0 : 1;
}
