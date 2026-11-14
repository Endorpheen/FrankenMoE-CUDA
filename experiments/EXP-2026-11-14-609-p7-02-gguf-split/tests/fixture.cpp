// Gate fixture for the P7-02 S4 GGUF split/merge port.
//
// Oracle: the native gguf-split tool built from upstream/llama.cpp (see
// tests/build-native.sh). For every case the fixture
//   1. writes a tiny GGUF with the port writer (S1 + S3, same file),
//   2. runs the native tool on it and keeps the shards on disk,
//   3. splits the same bytes in memory with gsp_* and compares the shard name
//      set, the shard count, every shard byte, the KV dump and the tensor dump
//      (name, dims, ne, type, nbytes) of the port's own reader against the
//      native reader,
//   4. reloads every port shard natively with no_alloc=false and compares each
//      tensor's data with the pattern that went into the input,
//   5. optionally merges the native shards with both tools and compares the
//      merged bytes, the KV dump, the tensor dump and every tensor's data with
//      the original input.
//
// Refusals are checked against the native exit code where native survives the
// case (missing shard, existing output, bad input, a too small size limit) and
// on the port only where native dies (split-max-tensors 0 divides by zero).
//
// Exit code 1 on any difference. The last line is
//   FG_G4 cases=N checks=M failures=0
#include "ggml.h"
#include "gguf.h"
#include "llama.h"
#include "gguf_split.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <dirent.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>

// ------------------------------------------------------------------ utility

static int g_checks = 0;
static int g_fail = 0;
static int g_cases = 0;

static void fail(const char * name, const char * what, const std::string & detail) {
    g_fail++;
    printf("FAIL\t%s\t%s\t%s\n", name, what, detail.c_str());
}

static void expect(const char * name, const char * what, bool ok, const std::string & detail = "") {
    g_checks++;
    if (ok) {
        printf("OK\t%s\t%s\n", name, what);
    } else {
        fail(name, what, detail);
    }
}

static void compare_dump(const char * name, const char * what,
                         const std::string & a, const std::string & b) {
    g_checks++;
    if (a == b) {
        printf("OK\t%s\t%s\t%zu\n", name, what, a.size());
        return;
    }
    std::string detail = "dumps differ";
    size_t la = 0, lb = 0;
    while (la < a.size() && lb < b.size()) {
        size_t ea = a.find('\n', la);
        size_t eb = b.find('\n', lb);
        if (ea == std::string::npos) ea = a.size();
        if (eb == std::string::npos) eb = b.size();
        if (a.compare(la, ea - la, b, lb, eb - lb) != 0) {
            detail = "native{" + a.substr(la, ea - la) + "} vs port{" + b.substr(lb, eb - lb) + "}";
            break;
        }
        la = ea + 1;
        lb = eb + 1;
    }
    fail(name, what, detail);
}

static sl_u8 S(void * p, int64_t n) {
    sl_u8 s;
    s.data = (uint8_t *) p;
    s.len = n;
    return s;
}

static sl_i64 A(int64_t * p, int64_t n) {
    sl_i64 s;
    s.data = p;
    s.len = n;
    return s;
}

static std::string run_cmd(const std::string & cmd) {
    std::string full = cmd + " >/dev/null 2>&1";
    int rc = std::system(full.c_str());
    if (rc == -1) {
        return "spawn-failed";
    }
    return std::to_string(WEXITSTATUS(rc));
}

static std::string q(const std::string & s) {
    return "'" + s + "'";
}

static void mkdir_p(const std::string & path) {
    ::mkdir(path.c_str(), 0777);
}

static std::vector<std::string> list_dir(const std::string & dir) {
    std::vector<std::string> out;
    DIR * d = opendir(dir.c_str());
    if (!d) {
        return out;
    }
    struct dirent * e;
    while ((e = readdir(d)) != nullptr) {
        std::string n = e->d_name;
        if (n == "." || n == "..") {
            continue;
        }
        out.push_back(n);
    }
    closedir(d);
    std::sort(out.begin(), out.end());
    return out;
}

static bool read_file(const std::string & path, std::vector<uint8_t> & out) {
    FILE * f = fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    out.resize(n > 0 ? (size_t) n : 0);
    if (!out.empty() && fread(out.data(), 1, out.size(), f) != out.size()) {
        fclose(f);
        return false;
    }
    fclose(f);
    return true;
}

static bool write_file(const std::string & path, const std::vector<uint8_t> & b) {
    FILE * f = fopen(path.c_str(), "wb");
    if (!f) {
        return false;
    }
    if (!b.empty() && fwrite(b.data(), 1, b.size(), f) != b.size()) {
        fclose(f);
        return false;
    }
    fclose(f);
    return true;
}

static std::string hex(const void * p, size_t n) {
    static const char * d = "0123456789abcdef";
    const unsigned char * b = (const unsigned char *) p;
    std::string s;
    for (size_t i = 0; i < n; i++) {
        s += d[b[i] >> 4];
        s += d[b[i] & 15];
    }
    return s;
}

static void compare_bytes(const char * name, const char * what,
                          const void * a, size_t an, const void * b, size_t bn) {
    g_checks++;
    if (an != bn) {
        fail(name, what, "size " + std::to_string(an) + " vs " + std::to_string(bn));
        return;
    }
    const uint8_t * x = (const uint8_t *) a;
    const uint8_t * y = (const uint8_t *) b;
    for (size_t i = 0; i < an; i++) {
        if (x[i] != y[i]) {
            size_t lo = i > 8 ? i - 8 : 0;
            size_t hi = i + 8 < an ? i + 8 : an;
            fail(name, what, "first_diff=" + std::to_string(i) + " native=" +
                 hex(x + lo, hi - lo) + " port=" + hex(y + lo, hi - lo));
            return;
        }
    }
    printf("OK\t%s\t%s\t%zu\n", name, what, an);
}

// ------------------------------------------------------------- case model

struct KV {
    int kind;  // 0 val, 1 str, 2 numeric array, 3 string array
    std::string key;
    int type = 0;
    uint64_t bits = 0;
    std::vector<uint8_t> data;
    std::vector<std::string> strs;
    int64_t n = 0;
};

struct TSpec {
    std::string name;
    int64_t ne[4] = {1, 1, 1, 1};
    int ndims = 1;
    int type = 0;
    int seed = 0;
};

struct Dataset {
    std::vector<KV> kvs;
    std::vector<TSpec> ts;
    int64_t align = 32;
};

static KV kv_val(const char * k, int t, uint64_t bits) {
    KV v; v.kind = 0; v.key = k; v.type = t; v.bits = bits; return v;
}
static KV kv_str(const char * k, const char * s) {
    KV v; v.kind = 1; v.key = k;
    v.data.assign((const uint8_t *) s, (const uint8_t *) s + strlen(s));
    return v;
}
static KV kv_arr(const char * k, int t, const void * p, int64_t bytes, int64_t n) {
    KV v; v.kind = 2; v.key = k; v.type = t; v.n = n;
    v.data.assign((const uint8_t *) p, (const uint8_t *) p + bytes);
    return v;
}
static KV kv_arr_str(const char * k, const std::vector<std::string> & strs) {
    KV v; v.kind = 3; v.key = k; v.strs = strs; v.n = (int64_t) strs.size(); return v;
}
static TSpec ts(const char * name, int64_t ne0, int64_t ne1, int64_t ne2, int64_t ne3,
                int ndims, int type, int seed) {
    TSpec s; s.name = name;
    s.ne[0] = ne0; s.ne[1] = ne1; s.ne[2] = ne2; s.ne[3] = ne3;
    s.ndims = ndims; s.type = type; s.seed = seed;
    return s;
}

static Dataset ds_four() {
    Dataset d;
    d.kvs.push_back(kv_str("general.architecture", "synthetic"));
    d.kvs.push_back(kv_str("general.name", "split four"));
    d.kvs.push_back(kv_val("synthetic.block_count", 4, 4));
    d.kvs.push_back(kv_val("synthetic.flag", 7, 0));
    d.ts.push_back(ts("blk.0", 2, 2, 1, 1, 2, GGML_TYPE_F32, 1));
    d.ts.push_back(ts("blk.1", 4, 2, 1, 1, 2, GGML_TYPE_F16, 2));
    d.ts.push_back(ts("blk.2", 32, 1, 1, 1, 1, GGML_TYPE_Q8_0, 3));
    d.ts.push_back(ts("blk.3", 3, 1, 1, 1, 1, GGML_TYPE_I32, 4));
    return d;
}

static Dataset ds_six() {
    Dataset d;
    uint32_t arr[3] = {1, 2, 4000000000u};
    uint8_t u8s[3] = {9, 8, 7};
    d.kvs.push_back(kv_str("general.architecture", "six"));
    d.kvs.push_back(kv_val("general.alignment", 4, 32));
    d.kvs.push_back(kv_arr("six.arr", 4, arr, 12, 3));
    d.kvs.push_back(kv_arr("six.bytes", 0, u8s, 3, 3));
    d.kvs.push_back(kv_arr_str("six.names", {"alpha", "", "gamma"}));
    for (int i = 0; i < 6; i++) {
        std::string n = "six." + std::to_string(i);
        d.ts.push_back(ts(n.c_str(), 8, 1, 1, 1, 1,
                          i % 2 ? GGML_TYPE_F32 : GGML_TYPE_F16, 10 + i));
    }
    return d;
}

static Dataset ds_empty() {
    Dataset d;
    d.kvs.push_back(kv_str("general.architecture", "empty"));
    d.kvs.push_back(kv_val("empty.count", 4, 0));
    return d;
}

// three 1 MiB tensors: size-mode splits need more than the 1M minimum
static Dataset ds_big() {
    Dataset d;
    d.kvs.push_back(kv_str("general.architecture", "big"));
    d.kvs.push_back(kv_val("general.alignment", 4, 32));
    for (int i = 0; i < 3; i++) {
        std::string n = "big." + std::to_string(i);
        d.ts.push_back(ts(n.c_str(), 262144, 1, 1, 1, 1, GGML_TYPE_F32, 20 + i));
    }
    return d;
}

// two exactly 2,000,000-byte tensors: the boundary of --split-max-size 2M
static Dataset ds_edge() {
    Dataset d;
    d.kvs.push_back(kv_val("general.alignment", 4, 32));
    for (int i = 0; i < 2; i++) {
        std::string n = "edge." + std::to_string(i);
        d.ts.push_back(ts(n.c_str(), 500000, 1, 1, 1, 1, GGML_TYPE_F32, 30 + i));
    }
    return d;
}

// an input that already carries split.* keys: native copies them, then
// replaces the three, which moves them to the end of the list
static Dataset ds_splitkv() {
    Dataset d;
    d.kvs.push_back(kv_str("general.architecture", "resplit"));
    d.kvs.push_back(kv_val("split.no", 2, 0));
    d.kvs.push_back(kv_val("split.count", 2, 2));
    d.kvs.push_back(kv_val("split.tensors.count", 5, 2));
    d.kvs.push_back(kv_val("resplit.extra", 4, 77));
    d.ts.push_back(ts("rs.0", 4, 1, 1, 1, 1, GGML_TYPE_F32, 40));
    d.ts.push_back(ts("rs.1", 2, 2, 1, 1, 2, GGML_TYPE_F16, 41));
    return d;
}

static Dataset ds_align64() {
    Dataset d = ds_four();
    d.align = 64;
    d.kvs.push_back(kv_val("general.alignment", 4, 64));
    return d;
}

static Dataset make_dataset(int kind) {
    switch (kind) {
        case 0: return ds_four();
        case 1: return ds_six();
        case 2: return ds_empty();
        case 3: return ds_big();
        case 4: return ds_edge();
        case 5: return ds_align64();
        case 6: return ds_splitkv();
        default: return ds_four();
    }
}

// ------------------------------------------------------------ port builder

static int64_t b_st[8], b_ko[128], b_kl[128], b_ty[128], b_ar[128], b_v0[128], b_v1[128], b_v2[128];
static uint8_t b_pool[1 << 16];
static int64_t b_tst[4], b_tne[32 * 4], b_trec[32 * 8];
static uint8_t b_tpool[8 << 20];
static int64_t b_out[2];

static int64_t r_st[8], r_ko[128], r_kl[128], r_ty[128], r_ar[128], r_v0[128], r_v1[128], r_v2[128];
static uint8_t r_pool[1 << 16];
static int64_t r_tst[4], r_tne[32 * 4], r_trec[32 * 8];
static uint8_t r_tpool[1 << 16];
static int64_t r_info[16];

static int64_t type_size_native(int t) {
    switch (t) {
        case GGUF_TYPE_UINT8: case GGUF_TYPE_INT8: case GGUF_TYPE_BOOL: return 1;
        case GGUF_TYPE_UINT16: case GGUF_TYPE_INT16: return 2;
        case GGUF_TYPE_UINT32: case GGUF_TYPE_INT32: case GGUF_TYPE_FLOAT32: return 4;
        case GGUF_TYPE_UINT64: case GGUF_TYPE_INT64: case GGUF_TYPE_FLOAT64: return 8;
        default: return 0;
    }
}

static void fill_pat(uint8_t * p, int64_t n, int seed) {
    for (int64_t i = 0; i < n; i++) {
        p[i] = (uint8_t) ((seed * 53 + i * 97 + 11) & 255);
    }
}

static int64_t tensor_nbytes(const TSpec & s) {
    const int64_t blck = ggml_blck_size((ggml_type) s.type);
    const int64_t tsz = (int64_t) ggml_type_size((ggml_type) s.type);
    if (blck <= 0 || tsz <= 0) {
        return -1;
    }
    int64_t nb = tsz * (s.ne[0] / blck);
    for (int d = 1; d < GGML_MAX_DIMS; d++) {
        nb *= s.ne[d];
    }
    return nb;
}

static std::map<std::string, std::vector<uint8_t>> g_pat;

static bool build_input(const Dataset & d, std::vector<uint8_t> & out) {
    g_pat.clear();
    int64_t rc = s1_init(A(b_st, 8), A(b_ko, 128), A(b_kl, 128), A(b_ty, 128), A(b_ar, 128),
                         A(b_v0, 128), A(b_v1, 128), A(b_v2, 128));
    if (rc != 0) {
        return false;
    }
    for (const KV & k : d.kvs) {
        switch (k.kind) {
            case 0:
                rc = s1_set_val(A(b_st, 8), S(b_pool, sizeof b_pool), A(b_ko, 128), A(b_kl, 128),
                                A(b_ty, 128), A(b_ar, 128), A(b_v0, 128), A(b_v1, 128),
                                A(b_v2, 128), S((void *) k.key.c_str(), (int64_t) k.key.size()),
                                k.type, (int64_t) k.bits, A(b_out, 2));
                break;
            case 1:
                rc = s1_set_str(A(b_st, 8), S(b_pool, sizeof b_pool), A(b_ko, 128), A(b_kl, 128),
                                A(b_ty, 128), A(b_ar, 128), A(b_v0, 128), A(b_v1, 128),
                                A(b_v2, 128), S((void *) k.key.c_str(), (int64_t) k.key.size()),
                                S((void *) k.data.data(), (int64_t) k.data.size()), A(b_out, 2));
                break;
            case 2:
                rc = s1_set_arr(A(b_st, 8), S(b_pool, sizeof b_pool), A(b_ko, 128), A(b_kl, 128),
                                A(b_ty, 128), A(b_ar, 128), A(b_v0, 128), A(b_v1, 128),
                                A(b_v2, 128), S((void *) k.key.c_str(), (int64_t) k.key.size()),
                                k.type, S((void *) k.data.data(), (int64_t) k.data.size()),
                                k.n, A(b_out, 2));
                break;
            case 3: {
                std::vector<uint8_t> blob;
                for (const std::string & s : k.strs) {
                    blob.insert(blob.end(), s.begin(), s.end());
                    blob.push_back(0);
                }
                if (!blob.empty()) {
                    blob.pop_back();
                }
                uint8_t dummy = 0;
                rc = s1_set_arr_str(A(b_st, 8), S(b_pool, sizeof b_pool), A(b_ko, 128),
                                    A(b_kl, 128), A(b_ty, 128), A(b_ar, 128), A(b_v0, 128),
                                    A(b_v1, 128), A(b_v2, 128),
                                    S((void *) k.key.c_str(), (int64_t) k.key.size()),
                                    blob.empty() ? S(&dummy, 0) : S(blob.data(), (int64_t) blob.size()),
                                    k.n, A(b_out, 2));
                break;
            }
        }
        if (rc != 0) {
            return false;
        }
    }
    rc = s3_init_tensors(A(b_tst, 4), d.align);
    if (rc != 0) {
        return false;
    }
    for (const TSpec & s : d.ts) {
        int64_t ne[4] = {s.ne[0], s.ne[1], s.ne[2], s.ne[3]};
        const int64_t nb = tensor_nbytes(s);
        if (nb < 0) {
            return false;
        }
        std::vector<uint8_t> pat((size_t) nb);
        fill_pat(pat.data(), nb, s.seed);
        g_pat[s.name] = pat;
        uint8_t dummy = 0;
        const int64_t idx = s3_add_tensor(A(b_tst, 4), S(b_tpool, sizeof b_tpool),
                                          A(b_tne, 32 * 4), A(b_trec, 32 * 8),
                                          S((void *) s.name.c_str(), (int64_t) s.name.size()),
                                          s.ndims, A(ne, 4), s.type, S(&dummy, 0));
        if (idx < 0) {
            return false;
        }
        rc = s3_set_tensor_data(A(b_tst, 4), S(b_tpool, sizeof b_tpool), A(b_tne, 32 * 4),
                                A(b_trec, 32 * 8),
                                S((void *) s.name.c_str(), (int64_t) s.name.size()),
                                S(pat.data(), (int64_t) pat.size()));
        if (rc != 0) {
            return false;
        }
    }
    int64_t total = 1 << 20;
    for (const TSpec & s : d.ts) {
        total += tensor_nbytes(s);
    }
    out.assign((size_t) total, 0);
    const int64_t n = s3_write(A(b_st, 8), S(b_pool, sizeof b_pool), A(b_ko, 128), A(b_kl, 128),
                               A(b_ty, 128), A(b_ar, 128), A(b_v0, 128), A(b_v1, 128), A(b_v2, 128),
                               A(b_tst, 4), S(b_tpool, sizeof b_tpool), A(b_tne, 32 * 4),
                               A(b_trec, 32 * 8), S(out.data(), (int64_t) out.size()),
                               3, 0);
    if (n <= 0) {
        return false;
    }
    out.resize((size_t) n);
    return true;
}

// ------------------------------------------------------------- native dumps

static std::string native_dump(gguf_context * ctx) {
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
                    if (j) {
                        s += ",";
                    }
                    s += hex(e, strlen(e));
                }
            } else {
                s += hex(gguf_get_arr_data(ctx, i), n * type_size_native(at));
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
                     (long long) type_size_native(t));
            s += line;
            s += hex(buf, type_size_native(t));
            s += "\n";
        }
    }
    return s;
}

static int dims_of(const int64_t * ne) {
    for (int d = GGML_MAX_DIMS - 1; d >= 1; --d) {
        if (ne[d] > 1) {
            return d + 1;
        }
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

static std::string port_dump() {
    std::string s;
    char line[256];
    snprintf(line, sizeof line, "N\t%lld\n", (long long) r_st[0]);
    s += line;
    for (int64_t i = 0; i < r_st[0]; i++) {
        std::string key((const char *) r_pool + r_ko[i], (size_t) r_kl[i]);
        const int t = (int) r_ty[i];
        if (r_ar[i] != 0) {
            snprintf(line, sizeof line, "KV\t%s\t%d\t1\t%d\t%lld\t", key.c_str(), t, t,
                     (long long) r_v1[i]);
            s += line;
            if (r_ar[i] == 2) {
                int64_t p = r_v0[i];
                const int64_t end = r_v0[i] + r_v2[i];
                for (int64_t k = 0; k < r_v1[i]; k++) {
                    const int64_t b = p;
                    while (p < end && r_pool[p] != 0) {
                        p++;
                    }
                    if (k) {
                        s += ",";
                    }
                    s += hex(r_pool + b, (size_t) (p - b));
                    p++;
                }
            } else {
                s += hex(r_pool + r_v0[i], (size_t) r_v2[i]);
            }
            s += "\n";
        } else if (t == 8) {
            snprintf(line, sizeof line, "KV\t%s\t%d\t0\t8\t%lld\t", key.c_str(), t,
                     (long long) r_v2[i]);
            s += line;
            s += hex(r_pool + r_v0[i], (size_t) r_v2[i]);
            s += "\n";
        } else {
            const uint64_t bits = (uint64_t) r_v0[i];
            const size_t n = (size_t) type_size_native(t);
            snprintf(line, sizeof line, "KV\t%s\t%d\t0\t%d\t%lld\t", key.c_str(), t, t,
                     (long long) n);
            s += line;
            unsigned char b[8];
            for (size_t k = 0; k < n; k++) {
                b[k] = (unsigned char) (bits >> (8 * k));
            }
            s += hex(b, n);
            s += "\n";
        }
    }
    return s;
}

static std::string port_tensor_dump() {
    std::string s;
    char line[512];
    snprintf(line, sizeof line, "TN\t%lld\n", (long long) r_tst[0]);
    s += line;
    for (int64_t i = 0; i < r_tst[0]; i++) {
        const int64_t * rec = r_trec + i * 8;
        const std::string name((const char *) r_tpool + rec[0], (size_t) rec[1]);
        const int64_t * ne = r_tne + i * 4;
        snprintf(line, sizeof line, "T\t%s\t%d\t%d\t%lld\t%lld\t%lld\t%lld\t%lld\t%lld\n",
                 name.c_str(), (int) rec[2], dims_of(ne),
                 (long long) ne[0], (long long) ne[1], (long long) ne[2], (long long) ne[3],
                 (long long) rec[6],
                 (long long) s3_tensor_size(A(r_tne, 32 * 4), A(r_trec, 32 * 8), i));
        s += line;
    }
    return s;
}

static bool port_parse(const std::vector<uint8_t> & b, const char * name) {
    int64_t rc = gsp_read_kv(S((void *) b.data(), (int64_t) b.size()), A(r_st, 8),
                             S(r_pool, sizeof r_pool), A(r_ko, 128), A(r_kl, 128),
                             A(r_ty, 128), A(r_ar, 128), A(r_v0, 128), A(r_v1, 128),
                             A(r_v2, 128), A(r_info, 16));
    if (rc != 0) {
        fail(name, "port_read_kv", "refused");
        return false;
    }
    rc = gsp_read_tensors(S((void *) b.data(), (int64_t) b.size()), r_info[4], r_info[2],
                          r_info[1], 0, A(r_tst, 4), S(r_tpool, sizeof r_tpool),
                          A(r_tne, 32 * 4), A(r_trec, 32 * 8), A(r_info, 16));
    if (rc != 0) {
        fail(name, "port_read_tensors", "refused");
        return false;
    }
    return true;
}

// Reload one file natively (no_alloc=false) and check every tensor's data
// against the pattern that went into the input.
static void native_data_check(const char * name, const std::string & path) {
    ggml_context * gctx = nullptr;
    gguf_init_params params;
    params.no_alloc = false;
    params.ctx = &gctx;
    gguf_context * r = gguf_init_from_file(path.c_str(), params);
    if (!r) {
        fail(name, "reload", "native load failed");
        return;
    }
    const int64_t n = gguf_get_n_tensors(r);
    for (int64_t i = 0; i < n; i++) {
        const char * tn = gguf_get_tensor_name(r, i);
        auto it = g_pat.find(tn);
        if (it == g_pat.end()) {
            fail(name, "reload_data", std::string("unknown tensor ") + tn);
            continue;
        }
        const ggml_tensor * t = ggml_get_tensor(gctx, tn);
        if (!t || !t->data) {
            fail(name, "reload_data", tn);
            continue;
        }
        compare_bytes(name, (std::string("reload_data_") + tn).c_str(), it->second.data(),
                      it->second.size(), t->data, it->second.size());
    }
    gguf_free(r);
    ggml_free(gctx);
}

// --------------------------------------------------------------- case table

struct SplitCase {
    const char * name;
    int ds;
    int mode;      // 1 tensor count, 2 size
    int64_t limit;
    bool notf;
    bool dry;
    bool do_merge;
    bool merge_dry;
    bool skip_read;
};

static std::string g_work;
static std::string g_tool;

static std::string native_split_cmd(const SplitCase & c, const std::string & dir) {
    std::string cmd = q(g_tool);
    if (c.mode == 2) {
        int64_t mb = c.limit / 1000000;
        cmd += " --split-max-size " + std::to_string(mb) + "M";
    } else {
        cmd += " --split-max-tensors " + std::to_string(c.limit);
    }
    if (c.notf) {
        cmd += " --no-tensor-first-split";
    }
    if (c.dry) {
        cmd += " --dry-run";
    }
    cmd += " " + q(dir + "/input.gguf") + " " + q(dir + "/model");
    return cmd;
}

// Run a split case: native on disk vs port in memory.
static void run_split_case(const SplitCase & c) {
    g_cases++;
    const std::string dir = g_work + "/" + c.name;
    mkdir_p(dir);

    Dataset d = make_dataset(c.ds);
    std::vector<uint8_t> input;
    if (!build_input(d, input)) {
        fail(c.name, "build_input", "writer refused");
        return;
    }
    if (!write_file(dir + "/input.gguf", input)) {
        fail(c.name, "write_input", "cannot write");
        return;
    }
    expect(c.name, "input_written", true);

    const std::string rc_native = run_cmd(native_split_cmd(c, dir));
    expect(c.name, "native_split_exit", rc_native == "0", "exit=" + rc_native);

    std::vector<std::string> native_files;
    for (const std::string & n : list_dir(dir)) {
        if (n.rfind("model-", 0) == 0) {
            native_files.push_back(n);
        }
    }
    std::sort(native_files.begin(), native_files.end());

    // port: parse, decide, write
    if (!port_parse(input, c.name)) {
        return;
    }
    const int64_t n_tensors = r_tst[0];
    std::vector<int64_t> nb((size_t) n_tensors);
    for (int64_t i = 0; i < n_tensors; i++) {
        nb[(size_t) i] = s3_tensor_size(A(r_tne, 32 * 4), A(r_trec, 32 * 8), i);
    }
    int64_t t0[32], t1[32];
    for (int i = 0; i < 32; i++) {
        t0[i] = -1;
        t1[i] = -1;
    }
    int64_t n_split = gsp_split_layout(n_tensors, c.mode, c.limit, c.limit,
                                       c.notf ? 1 : 0, A(nb.data(), (int64_t) nb.size()),
                                       A(t0, 32), A(t1, 32));
    if (n_split < 0) {
        fail(c.name, "port_layout", "refused");
        return;
    }
    g_checks++;
    printf("OK\t%s\tport_layout\t%lld\n", c.name, (long long) n_split);

    if (c.dry) {
        expect(c.name, "dry_run_wrote_nothing", native_files.empty(),
               std::to_string(native_files.size()) + " files");
        // the plan must still match the native one: re-run native without
        // --dry-run into a scratch prefix and compare the shard count
        SplitCase wet = c;
        std::string scratch = q(g_tool) + (c.mode == 2
            ? " --split-max-size " + std::to_string(c.limit / 1000000) + "M"
            : " --split-max-tensors " + std::to_string(c.limit));
        if (c.notf) {
            scratch += " --no-tensor-first-split";
        }
        scratch += " " + q(dir + "/input.gguf") + " " + q(dir + "/plan");
        wet.dry = false;
        const std::string rc2 = run_cmd(scratch);
        expect(c.name, "native_plan_exit", rc2 == "0", "exit=" + rc2);
        size_t planned = 0;
        for (const std::string & n : list_dir(dir)) {
            if (n.rfind("plan-", 0) == 0) {
                planned++;
            }
        }
        expect(c.name, "port_plan_matches_native", (int64_t) planned == n_split,
               "native=" + std::to_string(planned) + " port=" + std::to_string(n_split));
        return;
    }

    std::vector<uint8_t> out(input.size() + (1 << 20), 0);
    int64_t sh_off[32], sh_len[32];
    int64_t total = gsp_split_write(S((void *) input.data(), (int64_t) input.size()),
                                    A(r_st, 8), S(r_pool, sizeof r_pool), A(r_ko, 128),
                                    A(r_kl, 128), A(r_ty, 128), A(r_ar, 128), A(r_v0, 128),
                                    A(r_v1, 128), A(r_v2, 128), A(r_tst, 4),
                                    S(r_tpool, sizeof r_tpool), A(r_tne, 32 * 4),
                                    A(r_trec, 32 * 8), A(t0, 32), A(t1, 32), n_split,
                                    S(out.data(), (int64_t) out.size()),
                                    A(sh_off, 32), A(sh_len, 32));
    if (total <= 0) {
        fail(c.name, "port_split_write", "refused");
        return;
    }
    expect(c.name, "shard_count", (int64_t) native_files.size() == n_split,
           "native=" + std::to_string(native_files.size()) + " port=" + std::to_string(n_split));

    for (int64_t s = 0; s < n_split; s++) {
        uint8_t namebuf[128];
        const int64_t nl = gsp_split_name(S((void *) (dir + "/model").data(),
                                            (int64_t) (dir + "/model").size()),
                                           s, n_split, S(namebuf, sizeof namebuf));
        if (nl <= 0) {
            fail(c.name, "port_name", std::to_string(s));
            continue;
        }
        std::string port_name((const char *) namebuf, (size_t) nl);
        const std::string base = port_name.substr(dir.size() + 1);
        expect(c.name, ("name_" + std::to_string(s)).c_str(),
               (size_t) s < native_files.size() && native_files[(size_t) s] == base,
               "port=" + base);

        std::vector<uint8_t> nat, pt(out.begin() + sh_off[s], out.begin() + sh_off[s] + sh_len[s]);
        if (!read_file(dir + "/" + base, nat)) {
            fail(c.name, "read_native_shard", base);
            continue;
        }
        compare_bytes(c.name, ("shard_" + std::to_string(s)).c_str(),
                      nat.data(), nat.size(), pt.data(), pt.size());

        // cross-check the port reader against the native reader. The 64-byte
        // alignment case is bytes-only: native copies general.alignment 64 into
        // a shard that it pads at 32, so the file is internally inconsistent
        // and no reader can describe it correctly.
        if (c.skip_read) {
            continue;
        }
        if (!port_parse(pt, c.name)) {
            continue;
        }
        const std::string pk = port_dump();
        const std::string pt_dump = port_tensor_dump();
        const std::string port_path = dir + "/port-" + std::to_string(s) + ".gguf";
        write_file(port_path, pt);

        gguf_init_params ip;
        ip.no_alloc = true;
        ip.ctx = nullptr;
        gguf_context * nc = gguf_init_from_file((dir + "/" + base).c_str(), ip);
        if (!nc) {
            fail(c.name, "native_load_shard", base);
            continue;
        }
        compare_dump(c.name, ("kv_" + std::to_string(s)).c_str(), native_dump(nc), pk);
        compare_dump(c.name, ("tensors_" + std::to_string(s)).c_str(), native_tensor_dump(nc), pt_dump);
        gguf_free(nc);
        native_data_check((c.name), port_path);
    }

    if (c.do_merge) {
        // concatenate the native shards, merge with both tools
        std::vector<uint8_t> buf;
        std::vector<int64_t> off, len;
        bool ok = true;
        for (const std::string & n : native_files) {
            std::vector<uint8_t> b;
            if (!read_file(dir + "/" + n, b)) {
                ok = false;
                break;
            }
            off.push_back((int64_t) buf.size());
            len.push_back((int64_t) b.size());
            buf.insert(buf.end(), b.begin(), b.end());
        }
        expect(c.name, "merge_inputs", ok && (int64_t) off.size() == n_split,
               std::to_string(off.size()) + " shards");
        if (!ok) {
            return;
        }

        std::string mcmd = q(g_tool) + " --merge ";
        if (c.merge_dry) {
            mcmd += "--dry-run ";
        }
        mcmd += q(dir + "/" + native_files[0]) + " " + q(dir + "/merged.gguf");
        const std::string mrc = run_cmd(mcmd);
        expect(c.name, "native_merge_exit", mrc == "0", "exit=" + mrc);

        std::vector<uint8_t> mout(buf.size() + (1 << 20), 0);
        const int64_t mn = gsp_merge(S(buf.data(), (int64_t) buf.size()),
                                     A(off.data(), (int64_t) off.size()),
                                     A(len.data(), (int64_t) len.size()), n_split, 0,
                                     S(mout.data(), (int64_t) mout.size()),
                                     A(r_st, 8), S(r_pool, sizeof r_pool), A(r_ko, 128),
                                     A(r_kl, 128), A(r_ty, 128), A(r_ar, 128), A(r_v0, 128),
                                     A(r_v1, 128), A(r_v2, 128), A(r_tst, 4),
                                     S(r_tpool, sizeof r_tpool), A(r_tne, 32 * 4),
                                     A(r_trec, 32 * 8), A(r_info, 16));
        if (mn <= 0) {
            fail(c.name, "port_merge", "refused");
            return;
        }
        mout.resize((size_t) mn);

        if (c.merge_dry) {
            std::vector<uint8_t> check;
            expect(c.name, "merge_dry_no_file", !read_file(dir + "/merged.gguf", check),
                   "merged.gguf exists");
            expect(c.name, "merge_plan_tensors", r_info[1] == (int64_t) g_pat.size(),
                   "port=" + std::to_string(r_info[1]));
            return;
        }

        std::vector<uint8_t> nm;
        if (!read_file(dir + "/merged.gguf", nm)) {
            fail(c.name, "read_native_merge", "missing");
            return;
        }
        compare_bytes(c.name, "merge_bytes", nm.data(), nm.size(), mout.data(), mout.size());

        const std::string port_merged = dir + "/port-merged.gguf";
        write_file(port_merged, mout);

        // port reader over the merged bytes vs the native reader
        if (port_parse(mout, c.name)) {
            gguf_init_params ip;
            ip.no_alloc = true;
            ip.ctx = nullptr;
            gguf_context * nc = gguf_init_from_file((dir + "/merged.gguf").c_str(), ip);
            if (!nc) {
                fail(c.name, "native_load_merge", "failed");
            } else {
                compare_dump(c.name, "merge_kv", native_dump(nc), port_dump());
                compare_dump(c.name, "merge_tensors", native_tensor_dump(nc), port_tensor_dump());
                gguf_free(nc);
            }
        }
        // every merged tensor still carries the original pattern
        native_data_check(c.name, port_merged);
        expect(c.name, "merge_split_count_zero",
               gsp_kv_u16(S(mout.data(), (int64_t) mout.size()), S((void *) "split.count", 11)) == 0,
               "split.count is not 0");
        expect(c.name, "merge_tensor_total", r_tst[0] == (int64_t) g_pat.size(),
               "port=" + std::to_string(r_tst[0]) + " input=" + std::to_string(g_pat.size()));
    }
}

// --------------------------------------------------------------- refusals

static void run_edge_cases() {
    g_cases++;
    const std::string dir = g_work + "/edges";
    mkdir_p(dir);

    Dataset four = ds_four();
    std::vector<uint8_t> input;
    if (!build_input(four, input)) {
        fail("edges", "build_input", "writer refused");
        return;
    }
    write_file(dir + "/input.gguf", input);

    // a valid two-shard split to work from
    run_cmd(q(g_tool) + " --split-max-tensors 2 " + q(dir + "/input.gguf") + " " + q(dir + "/two"));
    std::vector<uint8_t> s0, s1;
    read_file(dir + "/two-00001-of-00002.gguf", s0);
    read_file(dir + "/two-00002-of-00002.gguf", s1);
    expect("edges", "shards_available", !s0.empty() && !s1.empty());

    std::vector<uint8_t> buf(s0);
    buf.insert(buf.end(), s1.begin(), s1.end());
    int64_t off[4] = {0, (int64_t) s0.size(), 0, 0};
    int64_t len[4] = {(int64_t) s0.size(), (int64_t) s1.size(), 0, 0};
    std::vector<uint8_t> out(buf.size() + (1 << 20), 0);

    auto merge_call = [&](int64_t n, int64_t have) -> int64_t {
        return gsp_merge(S(buf.data(), (int64_t) buf.size()), A(off, 4), A(len, 4), n, have,
                         S(out.data(), (int64_t) out.size()), A(r_st, 8),
                         S(r_pool, sizeof r_pool), A(r_ko, 128), A(r_kl, 128), A(r_ty, 128),
                         A(r_ar, 128), A(r_v0, 128), A(r_v1, 128), A(r_v2, 128), A(r_tst, 4),
                         S(r_tpool, sizeof r_tpool), A(r_tne, 32 * 4), A(r_trec, 32 * 8),
                         A(r_info, 16));
    };

    // 1. an existing output file is refused by both tools
    write_file(dir + "/exists.gguf", {0, 0, 0});
    const std::string erc = run_cmd(q(g_tool) + " --merge " + q(dir + "/two-00001-of-00002.gguf") +
                                    " " + q(dir + "/exists.gguf"));
    expect("edge", "native_existing_output_refused", erc != "0", "exit=" + erc);
    expect("edge", "port_existing_output_refused", merge_call(2, 1) < 0);

    // 2. one shard missing: native cannot open the second file, the port sees
    // a declared count larger than the provided shard list
    run_cmd("rm -f " + q(dir + "/two-00002-of-00002.gguf"));
    const std::string mrc = run_cmd(q(g_tool) + " --merge " + q(dir + "/two-00001-of-00002.gguf") +
                                    " " + q(dir + "/missing.gguf"));
    expect("edge", "native_missing_shard_refused", mrc != "0", "exit=" + mrc);
    expect("edge", "port_missing_shard_refused", merge_call(1, 0) < 0);
    write_file(dir + "/two-00002-of-00002.gguf", s1);

    // 3. an input file without split.count
    const std::string nrc = run_cmd(q(g_tool) + " --merge " + q(dir + "/input.gguf") +
                                    " " + q(dir + "/nosplit.gguf"));
    expect("edge", "native_no_split_key_refused", nrc != "0", "exit=" + nrc);
    std::vector<uint8_t> inbuf(input);
    int64_t o1[2] = {0, 0};
    int64_t l1[2] = {(int64_t) inbuf.size(), 0};
    int64_t r = gsp_merge(S(inbuf.data(), (int64_t) inbuf.size()), A(o1, 2), A(l1, 2), 1, 0,
                          S(out.data(), (int64_t) out.size()), A(r_st, 8),
                          S(r_pool, sizeof r_pool), A(r_ko, 128), A(r_kl, 128), A(r_ty, 128),
                          A(r_ar, 128), A(r_v0, 128), A(r_v1, 128), A(r_v2, 128), A(r_tst, 4),
                          S(r_tpool, sizeof r_tpool), A(r_tne, 32 * 4), A(r_trec, 32 * 8),
                          A(r_info, 16));
    expect("edge", "port_no_split_key_refused", r < 0);

    // 4. shard naming: the port must reproduce llama_split_prefix refusals
    char pbuf[256];
    const char * good = "model-00001-of-00002.gguf";
    const int64_t nl = gsp_merge_prefix(S((void *) good, (int64_t) strlen(good)), 0, 2,
                                        S(pbuf, sizeof pbuf));
    compare_bytes("edge", "merge_prefix_name", "model", 5, pbuf, (size_t) (nl < 0 ? 0 : nl));
    char lbuf[256];
    const int32_t ln = llama_split_prefix(lbuf, sizeof lbuf, good, 0, 2);
    expect("edge", "merge_prefix_len", nl == (int64_t) ln,
           "port=" + std::to_string(nl) + " native=" + std::to_string(ln));
    const char * bad = "model.gguf";
    char pbuf2[256], lbuf2[256];
    const int64_t nb = gsp_merge_prefix(S((void *) bad, (int64_t) strlen(bad)), 0, 2,
                                        S(pbuf2, sizeof pbuf2));
    const int32_t lb = llama_split_prefix(lbuf2, sizeof lbuf2, bad, 0, 2);
    expect("edge", "merge_prefix_bad_name", nb < 0 && lb == 0,
           "port=" + std::to_string(nb) + " native=" + std::to_string(lb));
    const char * wrong_no = "model-00002-of-00002.gguf";
    char pbuf3[256], lbuf3[256];
    const int64_t nw = gsp_merge_prefix(S((void *) wrong_no, (int64_t) strlen(wrong_no)), 0, 2,
                                        S(pbuf3, sizeof pbuf3));
    const int32_t lw = llama_split_prefix(lbuf3, sizeof lbuf3, wrong_no, 0, 2);
    expect("edge", "merge_prefix_wrong_number", nw < 0 && lw == 0,
           "port=" + std::to_string(nw) + " native=" + std::to_string(lw));

    // 5. an unusable tensor limit: native divides by zero, the port refuses
    Dataset six = ds_six();
    std::vector<uint8_t> sixb;
    build_input(six, sixb);
    port_parse(sixb, "edge");
    std::vector<int64_t> nb6((size_t) r_tst[0]);
    for (int64_t i = 0; i < r_tst[0]; i++) {
        nb6[(size_t) i] = s3_tensor_size(A(r_tne, 32 * 4), A(r_trec, 32 * 8), i);
    }
    int64_t e0[32], e1[32];
    expect("edge", "port_zero_tensor_limit_refused",
           gsp_split_layout(r_tst[0], 1, 0, 0, 0, A(nb6.data(), (int64_t) nb6.size()),
                            A(e0, 32), A(e1, 32)) < 0);

    // 6. a size limit below the first tensor: native exits with "0 tensors"
    Dataset big = ds_big();
    std::vector<uint8_t> bigb;
    build_input(big, bigb);
    write_file(dir + "/big.gguf", bigb);
    const std::string brc = run_cmd(q(g_tool) + " --split-max-size 1M " + q(dir + "/big.gguf") +
                                    " " + q(dir + "/big"));
    expect("edge", "native_size_too_small_refused", brc != "0", "exit=" + brc);
    port_parse(bigb, "edge");
    std::vector<int64_t> nbb((size_t) r_tst[0]);
    for (int64_t i = 0; i < r_tst[0]; i++) {
        nbb[(size_t) i] = s3_tensor_size(A(r_tne, 32 * 4), A(r_trec, 32 * 8), i);
    }
    expect("edge", "port_size_too_small_refused",
           gsp_split_layout(r_tst[0], 2, 0, 1000000, 0, A(nbb.data(), (int64_t) nbb.size()),
                            A(e0, 32), A(e1, 32)) < 0);

    // 7. merge must start on shard 0: the native prefix check rejects shard 2
    std::vector<uint8_t> s1only(s1);
    int64_t o2[2] = {0, 0};
    int64_t l2[2] = {(int64_t) s1only.size(), 0};
    int64_t r2 = gsp_merge(S(s1only.data(), (int64_t) s1only.size()), A(o2, 2), A(l2, 2), 1, 0,
                           S(out.data(), (int64_t) out.size()), A(r_st, 8),
                           S(r_pool, sizeof r_pool), A(r_ko, 128), A(r_kl, 128), A(r_ty, 128),
                           A(r_ar, 128), A(r_v0, 128), A(r_v1, 128), A(r_v2, 128), A(r_tst, 4),
                           S(r_tpool, sizeof r_tpool), A(r_tne, 32 * 4), A(r_trec, 32 * 8),
                           A(r_info, 16));
    expect("edge", "port_not_first_shard_refused", r2 < 0);
}

// ----------------------------------------------------------- parse cases

struct ParseCase {
    const char * name;
    std::vector<std::string> args;
    bool ok;
    int64_t op, mode, limit_t, limit_b;
    bool notf, dry, del;
    bool help;
};

static void run_parse_case(const ParseCase & p, const std::string & base_in,
                           const std::string & shard_in) {
    g_cases++;
    const std::string dir = g_work + "/parse-" + p.name;
    mkdir_p(dir);
    const std::string in = p.args.size() > 0 && p.args.back() == "$SHARD" ? shard_in : base_in;
    const std::string out = dir + "/out";

    std::vector<std::string> argv;
    argv.push_back("gguf-split");
    for (const std::string & a : p.args) {
        if (a == "$IN") {
            argv.push_back(base_in);
        } else if (a == "$SHARD") {
            argv.push_back(shard_in);
        } else if (a == "$OUT") {
            argv.push_back(out);
        } else {
            argv.push_back(a);
        }
    }
    std::string blob;
    std::vector<int64_t> aoff, alen;
    for (const std::string & a : argv) {
        aoff.push_back((int64_t) blob.size());
        alen.push_back((int64_t) a.size());
        blob += a;
        blob.push_back('\0');
    }
    int64_t params[16] = {0};
    const int64_t rc = gsp_parse((int64_t) argv.size(), S((void *) blob.data(), (int64_t) blob.size()),
                                 A(aoff.data(), (int64_t) aoff.size()),
                                 A(alen.data(), (int64_t) alen.size()), A(params, 16));
    expect(p.name, "port_parse_accept", (rc == 0) == p.ok, "rc=" + std::to_string(rc));
    if (rc == 0 && p.ok) {
        expect(p.name, "operation", params[0] == p.op,
               "op=" + std::to_string(params[0]));
        expect(p.name, "mode", params[1] == p.mode, "mode=" + std::to_string(params[1]));
        if (p.mode == 1) {
            expect(p.name, "tensor_limit", params[2] == p.limit_t,
                   "limit=" + std::to_string(params[2]));
        }
        if (p.mode == 2) {
            expect(p.name, "size_limit", params[3] == p.limit_b,
                   "limit=" + std::to_string(params[3]));
        }
        expect(p.name, "no_tensor_first", (params[4] != 0) == p.notf);
        expect(p.name, "dry_run", (params[5] != 0) == p.dry);
        expect(p.name, "delete_splits", (params[6] != 0) == p.del);
        if (!p.help) {
            expect(p.name, "input_position", params[7] == (int64_t) (argv.size() - 2));
            expect(p.name, "output_position", params[8] == (int64_t) (argv.size() - 1));
        }
    }

    // native acceptance for the same argv
    std::string cmd = q(g_tool);
    for (size_t i = 1; i < argv.size(); i++) {
        cmd += " " + q(argv[i]);
    }
    const std::string nrc = run_cmd(cmd);
    if (p.help) {
        expect(p.name, "native_help_exit", nrc == "0", "exit=" + nrc);
        expect(p.name, "help_marker", params[9] == 1);
    } else if (p.ok) {
        expect(p.name, "native_exit", nrc == "0", "exit=" + nrc);
    } else {
        expect(p.name, "native_refused", nrc != "0", "exit=" + nrc);
    }
}

// -------------------------------------------------------------- main

int main(int argc, char ** argv) {
    goose_init(argc, argv);

    g_work = argc > 1 ? argv[1] : "gate-work";
    g_tool = argc > 2 ? argv[2] : "gguf-split";
    mkdir_p(g_work);

    // the base inputs and the shard used by the merge parse cases
    Dataset four = ds_four();
    std::vector<uint8_t> input;
    if (!build_input(four, input)) {
        printf("FAIL\tmain\tbuild_input\n");
        return 1;
    }
    const std::string base_dir = g_work + "/base";
    mkdir_p(base_dir);
    write_file(base_dir + "/input.gguf", input);
    run_cmd(q(g_tool) + " --split-max-tensors 2 " + q(base_dir + "/input.gguf") + " " +
            q(base_dir + "/base"));
    const std::string shard = base_dir + "/base-00001-of-00002.gguf";

    const SplitCase splits[] = {
        {"split_t128_four", 0, 1, 128, false, false, true, false, false},
        {"split_t2_four",   0, 1, 2,   false, false, true, false, false},
        {"split_t1_four",   0, 1, 1,   false, false, true, false, false},
        {"split_t3_four",   0, 1, 3,   false, false, true, false, false},
        {"split_t2_six",    1, 1, 2,   false, false, true, false, false},
        {"split_t1_six",    1, 1, 1,   false, false, true, false, false},
        {"split_notf_four", 0, 1, 1,   true,  false, true, false, false},
        {"split_notf_six",  1, 1, 2,   true,  false, true, false, false},
        {"split_empty",     2, 1, 1,   false, false, true, false, false},
        {"split_size_big2", 3, 2, 2000000, false, false, true, false, false},
        {"split_size_big4", 3, 2, 4000000, false, false, true, false, false},
        {"split_size_edge", 4, 2, 2000000, false, false, true, false, false},
        {"split_align64",   5, 1, 1,   false, false, false, false, true},
        {"split_splitkv",   6, 1, 1,   false, false, true, false, false},
        {"split_dry_four",  0, 1, 2,   false, true,  false, false, false},
        {"split_t2_six_dry",1, 1, 2,   false, true,  false, true, false},
    };

    for (const SplitCase & c : splits) {
        run_split_case(c);
    }

    // a merge plan only (native --dry-run writes nothing)
    SplitCase d = {"split_merge_dry", 0, 1, 2, false, false, true, true, false};
    run_split_case(d);

    run_edge_cases();

    const ParseCase parses[] = {
        {"p_defaults",        {"$IN", "$OUT"}, true, 1, 1, 128, 0, false, false, false, false},
        {"p_split",           {"--split", "$IN", "$OUT"}, true, 1, 1, 128, 0, false, false, false, false},
        {"p_merge",           {"--merge", "$SHARD", "$OUT"}, true, 2, 1, 128, 0, false, false, false, false},
        {"p_dry",             {"--dry-run", "$IN", "$OUT"}, true, 1, 1, 128, 0, false, true, false, false},
        {"p_notf",            {"--no-tensor-first-split", "$IN", "$OUT"}, true, 1, 1, 128, 0, true, false, false, false},
        {"p_delete",          {"--delete-splits", "$IN", "$OUT"}, true, 1, 1, 128, 0, false, false, true, false},
        {"p_t2",              {"--split-max-tensors", "2", "$IN", "$OUT"}, true, 1, 1, 2, 0, false, false, false, false},
        {"p_size2m",          {"--split-max-size", "2M", "$IN", "$OUT"}, true, 1, 2, 128, 2000000, false, false, false, false},
        {"p_size1g",          {"--split-max-size", "1G", "$IN", "$OUT"}, true, 1, 2, 128, 1000000000, false, false, false, false},
        {"p_conflict_ops",    {"--split", "--merge", "$IN", "$OUT"}, false, 0, 0, 0, 0, false, false, false, false},
        {"p_conflict_modes",  {"--split-max-tensors", "2", "--split-max-size", "2M", "$IN", "$OUT"}, false, 0, 0, 0, 0, false, false, false, false},
        {"p_unknown",         {"--bogus", "$IN", "$OUT"}, false, 0, 0, 0, 0, false, false, false, false},
        {"p_one_pos",         {"$IN"}, false, 0, 0, 0, 0, false, false, false, false},
        {"p_three_pos",       {"$IN", "$OUT", "extra"}, false, 0, 0, 0, 0, false, false, false, false},
        {"p_bad_unit",        {"--split-max-size", "128K", "$IN", "$OUT"}, false, 0, 0, 0, 0, false, false, false, false},
        {"p_zero_size",       {"--split-max-size", "0M", "$IN", "$OUT"}, false, 0, 0, 0, 0, false, false, false, false},
        {"p_underscore",      {"--split_max_tensors", "3", "$IN", "$OUT"}, true, 1, 1, 3, 0, false, false, false, false},
        {"p_dry_underscore",  {"--dry_run", "$IN", "$OUT"}, true, 1, 1, 128, 0, false, true, false, false},
        {"p_help",            {"--help"}, true, 0, 0, 0, 0, false, false, false, true},
        {"p_missing_value",   {"--split-max-tensors"}, false, 0, 0, 0, 0, false, false, false, false},
        {"p_dash_h",          {"-h", "$IN", "$OUT"}, false, 0, 0, 0, 0, false, false, false, false},
    };
    for (const ParseCase & p : parses) {
        run_parse_case(p, base_dir + "/input.gguf", shard);
    }

    // split_str_to_n_bytes and should_split values that no argv reaches
    int64_t nb_out[2] = {0, 0};
    expect("unit", "s2n_128M",
           gsp_str_to_n_bytes(S((void *) "128M", 4), A(nb_out, 2)) == 0 && nb_out[0] == 128000000);
    expect("unit", "s2n_4G",
           gsp_str_to_n_bytes(S((void *) "4G", 2), A(nb_out, 2)) == 0 && nb_out[0] == 4000000000ll);
    expect("unit", "s2n_no_digits_refused",
           gsp_str_to_n_bytes(S((void *) "M", 1), A(nb_out, 2)) < 0);
    expect("unit", "s2n_zero_refused",
           gsp_str_to_n_bytes(S((void *) "0M", 2), A(nb_out, 2)) < 0);
    expect("unit", "s2n_lowercase_refused",
           gsp_str_to_n_bytes(S((void *) "12m", 3), A(nb_out, 2)) < 0);
    expect("unit", "should_split_size",
           gsp_should_split(2, 0, 3, 1000001, 128, 1000000) == 1);
    expect("unit", "should_split_size_boundary",
           gsp_should_split(2, 0, 3, 1000000, 128, 1000000) == 0);
    expect("unit", "should_split_tensor",
           gsp_should_split(1, 4, 6, 0, 4, 0) == 1 && gsp_should_split(1, 3, 6, 0, 4, 0) == 0);
    expect("unit", "should_split_bad_mode", gsp_should_split(0, 0, 1, 0, 1, 1) < 0);

    printf("FG_G4 cases=%d checks=%d failures=%d\n", g_cases, g_checks, g_fail);
    return g_fail == 0 ? 0 : 1;
}
