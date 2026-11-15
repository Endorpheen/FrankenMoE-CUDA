// Differential gate for P10-08 S5 (the GGUF public header surface).
//
// Native arms:
//   - compile time: the header macros and the enum, plus the layout of
//     gguf_init_params;
//   - build time: tests/extract_native.py parses GGUF_TYPE_NAME and
//     GGUF_TYPE_SIZE out of ggml/src/gguf.cpp;
//   - run time: the reference libggml-base.so is driven through dlopen. It
//     produces a metadata block whose magic and version are compared byte for
//     byte, answers gguf_type_name for all thirteen types, and is asked to parse
//     crafted files whose alignment, data offset and reader chunking are then
//     compared with the port's model. The invalid-alignment and short-read
//     branches are exercised natively and must refuse.
//
// Port arm: the ggh_ exports of source/gguf-headers.goose.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <dlfcn.h>

#include "gguf.h"

#include "ggh.h"
#include "native_abi.h"

static int cases = 0;
static int values = 0;
static int fails = 0;

static void eq_int(const char * what, int64_t want, int64_t got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-46s want=%lld got=%lld\n", what, (long long) want, (long long) got);
    }
}

static void eq_str(const char * what, const std::string & want, const std::string & got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-46s want=[%s] got=[%s]\n", what, want.c_str(), got.c_str());
    }
}

// ── the native constant / layout map ────────────────────────────────────────
static std::vector<std::pair<std::string, int64_t>> NAT;

static void nat_add(const char * name, int64_t v) { NAT.emplace_back(name, v); }

static void build_nat() {
    nat_add("GGUF_MAGIC[0]", (int64_t) (unsigned char) GGUF_MAGIC[0]);
    nat_add("GGUF_MAGIC[1]", (int64_t) (unsigned char) GGUF_MAGIC[1]);
    nat_add("GGUF_MAGIC[2]", (int64_t) (unsigned char) GGUF_MAGIC[2]);
    nat_add("GGUF_MAGIC[3]", (int64_t) (unsigned char) GGUF_MAGIC[3]);
    nat_add("GGUF_MAGIC_U32",
            (int64_t) ((uint32_t) (unsigned char) GGUF_MAGIC[0] |
                       ((uint32_t) (unsigned char) GGUF_MAGIC[1] << 8) |
                       ((uint32_t) (unsigned char) GGUF_MAGIC[2] << 16) |
                       ((uint32_t) (unsigned char) GGUF_MAGIC[3] << 24)));
#define GGH(id) nat_add(#id, (int64_t) (id));
#define GGH_SZ(t) nat_add("sizeof:" #t, (int64_t) sizeof(t));
#define GGH_OFF(t, f) nat_add("offsetof:" #t "." #f, (int64_t) offsetof(t, f));
    nat_add("GGUF_KEY_GENERAL_ALIGNMENT_LEN", (int64_t) strlen(GGUF_KEY_GENERAL_ALIGNMENT));
#include "native_ggh.inc"
#undef GGH
#undef GGH_SZ
#undef GGH_OFF
}

static_assert(GGUF_VERSION == 3, "format version");
static_assert(GGUF_DEFAULT_ALIGNMENT == 32, "default alignment");
static_assert(GGUF_TYPE_COUNT == 13, "type count");
static_assert(sizeof(gguf_init_params) == 16, "init params layout");

// ── crafted GGUF files ──────────────────────────────────────────────────────
static void put_u32(std::vector<uint8_t> & b, uint32_t v) {
    for (int i = 0; i < 4; i++) b.push_back((uint8_t) (v >> (8 * i)));
}
static void put_u64(std::vector<uint8_t> & b, uint64_t v) {
    for (int i = 0; i < 8; i++) b.push_back((uint8_t) (v >> (8 * i)));
}

// A file with one alignment key (optionally) and one F32 tensor (optionally).
static std::vector<uint8_t> make_gguf(bool with_alignment, uint32_t alignment, bool with_tensor) {
    std::vector<uint8_t> b;
    b.insert(b.end(), GGUF_MAGIC, GGUF_MAGIC + 4);
    put_u32(b, GGUF_VERSION);
    put_u64(b, with_tensor ? 1 : 0);
    put_u64(b, with_alignment ? 1 : 0);
    if (with_alignment) {
        const std::string key = GGUF_KEY_GENERAL_ALIGNMENT;
        put_u64(b, key.size());
        b.insert(b.end(), key.begin(), key.end());
        put_u32(b, GGUF_TYPE_UINT32);
        put_u32(b, alignment);
    }
    if (with_tensor) {
        const std::string name = "t";
        put_u64(b, name.size());
        b.insert(b.end(), name.begin(), name.end());
        put_u32(b, 1);       // n_dims
        put_u64(b, 1);       // ne[0]
        put_u32(b, 0);       // GGML_TYPE_F32
        put_u64(b, 0);       // data offset
    }
    return b;
}

struct recorder {
    const uint8_t * data = nullptr;
    std::vector<size_t> chunk;
    size_t short_by = 0;
};

static size_t record_cb(void * userdata, void * out, uint64_t offset, size_t len) {
    recorder * r = (recorder *) userdata;
    r->chunk.push_back(len);
    const size_t n = r->short_by ? len - r->short_by : len;
    if (r->data != nullptr) {
        memcpy(out, r->data + offset, n);
    }
    return n;
}

// ── port helpers ────────────────────────────────────────────────────────────
static std::string port_str(int64_t (*fn)(int64_t, sl_u8), int64_t key) {
    char buf[128];
    memset(buf, 0x5a, sizeof buf);
    int64_t n = fn(key, sl_u8{(uint8_t *) buf, (int64_t) sizeof buf - 1});
    if (n < 0) {
        return "!";
    }
    return std::string(buf, (size_t) n);
}

// ── the reference library ───────────────────────────────────────────────────
struct ref_lib {
    void * h = nullptr;
    struct gguf_context * (*init_empty)(void) = nullptr;
    void (*free_ctx)(struct gguf_context *) = nullptr;
    size_t (*meta_size)(const struct gguf_context *) = nullptr;
    void (*meta_data)(const struct gguf_context *, void *) = nullptr;
    uint32_t (*version)(const struct gguf_context *) = nullptr;
    size_t (*alignment)(const struct gguf_context *) = nullptr;
    size_t (*data_offset)(const struct gguf_context *) = nullptr;
    const char * (*type_name)(enum gguf_type) = nullptr;
    struct gguf_context * (*init_cb)(gguf_reader_callback_t, void *, size_t, uint64_t,
                                     struct gguf_init_params) = nullptr;
};

static ref_lib REF;

static bool ref_open() {
    const std::string path = std::string(NAT_SO_DIR) + "/libggml-base.so.0";
    REF.h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (REF.h == nullptr) {
        printf("FAIL dlopen %s: %s\n", path.c_str(), dlerror());
        fails++;
        return false;
    }
#define BIND(field, name)                                                     \
    REF.field = (decltype(REF.field)) dlsym(REF.h, name);                     \
    if (REF.field == nullptr) {                                               \
        printf("FAIL dlsym %s\n", name);                                      \
        fails++;                                                              \
        return false;                                                         \
    }
    BIND(init_empty, "gguf_init_empty");
    BIND(free_ctx, "gguf_free");
    BIND(meta_size, "gguf_get_meta_size");
    BIND(meta_data, "gguf_get_meta_data");
    BIND(version, "gguf_get_version");
    BIND(alignment, "gguf_get_alignment");
    BIND(data_offset, "gguf_get_data_offset");
    BIND(type_name, "gguf_type_name");
    BIND(init_cb, "gguf_init_from_callback");
#undef BIND
    return true;
}

int main() {
    goose_init(0, nullptr);
    build_nat();

    // 1. constants and layout, both directions.
    {
        std::vector<std::pair<std::string, int64_t>> port;
        const int64_t n = ggh_const_count();
        eq_int("const count", (int64_t) NAT.size(), n);
        for (int64_t i = 0; i < n; i++) {
            port.emplace_back(port_str(ggh_const_name, i), ggh_const_value(i));
            cases++;
        }
        for (auto & p : port) {
            bool found = false;
            for (auto & q : NAT) {
                if (q.first == p.first) {
                    found = true;
                    eq_int(("const " + p.first).c_str(), q.second, p.second);
                }
            }
            if (!found) {
                values++;
                fails++;
                printf("FAIL const %-40s no native identifier\n", p.first.c_str());
            }
        }
        for (auto & q : NAT) {
            bool found = false;
            for (auto & p : port) found = found || p.first == q.first;
            values++;
            if (!found) {
                fails++;
                printf("FAIL const %-40s missing from the port\n", q.first.c_str());
            }
        }
    }

    // 2. boundaries of the constant table.
    eq_int("const_name(-1)", -1, ggh_const_name(-1, sl_u8{(uint8_t *) nullptr, 0}));
    eq_int("const_name(count)", -1, ggh_const_name(ggh_const_count(), sl_u8{(uint8_t *) nullptr, 0}));
    eq_int("const_value(-1)", -1, ggh_const_value(-1));

    // 3. type names and sizes against the source maps, both directions.
    {
        eq_int("type count", NAT_GGH_TYPE_N, ggh_type_count());
        for (int i = 0; i < NAT_GGH_TYPE_N; i++) {
            cases++;
            eq_int("type value", NAT_GGH_TYPE[i].val, ggh_type_value(i));
            eq_str("type name", NAT_GGH_TYPE[i].text, port_str(ggh_type_name, NAT_GGH_TYPE[i].val));
            eq_int("type size", NAT_GGH_TYPE[i].size, ggh_type_size(NAT_GGH_TYPE[i].val));
        }
        // reverse: every value the port knows is in the native map
        for (int64_t i = 0; i < ggh_type_count(); i++) {
            bool found = false;
            for (int k = 0; k < NAT_GGH_TYPE_N; k++) found = found || NAT_GGH_TYPE[k].val == ggh_type_value(i);
            values++;
            if (!found) {
                fails++;
                printf("FAIL type %lld is not in the native enum\n", (long long) ggh_type_value(i));
            }
        }
    }

    // 4. boundaries: an unknown type has no name and size 0.
    for (int64_t bad : {-1, 13, 42, 1000000}) {
        cases++;
        eq_int("type_name unknown", -1, ggh_type_name(bad, sl_u8{(uint8_t *) nullptr, 0}));
        eq_int("type_size unknown", 0, ggh_type_size(bad));
    }

    // 5. the alignment key text.
    {
        cases++;
        char buf[64];
        int64_t n = ggh_key_general_alignment(sl_u8{(uint8_t *) buf, 63});
        eq_str("alignment key", GGUF_KEY_GENERAL_ALIGNMENT, std::string(buf, (size_t) n));
    }

    // 6. the alignment rule, natively exercised through the reference library.
    if (ref_open()) {
        for (int64_t a : {0, 1, 2, 3, 4, 7, 8, 16, 32, 1024, 4096}) {
            cases++;
            eq_int(("alignment_valid " + std::to_string(a)).c_str(), a != 0 && (a & (a - 1)) == 0 ? 1 : 0,
                   ggh_alignment_valid(a));
            std::vector<uint8_t> file = make_gguf(true, (uint32_t) a, false);
            recorder rec;
            rec.data = file.data();
            gguf_init_params params{};
            params.no_alloc = true;
            struct gguf_context * ctx = REF.init_cb(record_cb, &rec, 0, file.size(), params);
            const bool native_ok = ctx != nullptr;
            eq_int(("native alignment accept " + std::to_string(a)).c_str(), ggh_alignment_valid(a),
                   native_ok ? 1 : 0);
            if (ctx != nullptr) {
                eq_int(("native alignment value " + std::to_string(a)).c_str(), a,
                       (int64_t) REF.alignment(ctx));
                REF.free_ctx(ctx);
            }
        }

        // 7. metadata: magic, version and the empty-context facts.
        cases++;
        {
            struct gguf_context * ctx = REF.init_empty();
            if (ctx == nullptr) {
                fails++;
                printf("FAIL gguf_init_empty\n");
            } else {
                eq_int("native version", GGUF_VERSION, (int64_t) REF.version(ctx));
                eq_int("native default alignment", GGUF_DEFAULT_ALIGNMENT, (int64_t) REF.alignment(ctx));
                // an in-memory context has no data section yet
                eq_int("native empty data offset", 0, (int64_t) REF.data_offset(ctx));
                const size_t size = REF.meta_size(ctx);
                std::vector<uint8_t> meta(size + 1);
                REF.meta_data(ctx, meta.data());
                // the writer pads the metadata to the default alignment
                eq_int("meta size", 32, (int64_t) size);
                eq_int("meta magic", 1179993927,
                       (int64_t) ((uint32_t) meta[0] | ((uint32_t) meta[1] << 8) |
                                  ((uint32_t) meta[2] << 16) | ((uint32_t) meta[3] << 24)));
                eq_int("meta version", GGUF_VERSION,
                       (int64_t) ((uint32_t) meta[4] | ((uint32_t) meta[5] << 8) |
                                  ((uint32_t) meta[6] << 16) | ((uint32_t) meta[7] << 24)));
                REF.free_ctx(ctx);
            }
        }

        // 8. data offset: padded only when the file has tensors.
        for (int with_tensor = 0; with_tensor <= 1; with_tensor++) {
            cases++;
            std::vector<uint8_t> file = make_gguf(false, GGUF_DEFAULT_ALIGNMENT, with_tensor != 0);
            const int64_t meta_end = (int64_t) file.size();
            if (with_tensor) {
                file.resize((size_t) ((meta_end + 31) & ~31LL), 0);
            }
            recorder rec;
            rec.data = file.data();
            gguf_init_params params{};
            params.no_alloc = true;
            struct gguf_context * ctx = REF.init_cb(record_cb, &rec, 0, file.size(), params);
            if (ctx == nullptr) {
                fails++;
                printf("FAIL crafted file with_tensor=%d was rejected\n", with_tensor);
            } else {
                eq_int("data offset rule", ggh_data_offset(meta_end, GGUF_DEFAULT_ALIGNMENT, with_tensor),
                       (int64_t) REF.data_offset(ctx));
                REF.free_ctx(ctx);
            }
        }

        // 9. the reader callback: the parser reads the magic as four one-byte
        // requests, then version (4), n_tensors (8) and n_kv (8). Each request
        // is split by read_raw into chunks of min(max_chunk, remaining).
        for (int64_t chunk : {0, 1, 3, 5, 7, 24, 25}) {
            cases++;
            std::vector<uint8_t> file = make_gguf(false, GGUF_DEFAULT_ALIGNMENT, false);
            recorder rec;
            rec.data = file.data();
            gguf_init_params params{};
            params.no_alloc = true;
            struct gguf_context * ctx = REF.init_cb(record_cb, &rec, (size_t) chunk, file.size(), params);
            if (ctx == nullptr) {
                fails++;
                printf("FAIL chunk=%lld rejected\n", (long long) chunk);
                continue;
            }
            std::vector<int64_t> want;
            for (int64_t read_size : {1, 1, 1, 1, 4, 8, 8}) {
                const int64_t calls = ggh_reader_calls(read_size, chunk);
                for (int64_t i = 0; i < calls; i++) {
                    want.push_back(ggh_reader_chunk(read_size, chunk, i));
                }
            }
            eq_int(("reader calls chunk=" + std::to_string(chunk)).c_str(),
                   (int64_t) want.size(), (int64_t) rec.chunk.size());
            for (size_t i = 0; i < want.size() && i < rec.chunk.size(); i++) {
                eq_int(("reader chunk " + std::to_string(chunk) + "/" + std::to_string(i)).c_str(),
                       want[i], (int64_t) rec.chunk[i]);
            }
            REF.free_ctx(ctx);
        }

        // 10. reader refusals: a short read and a null callback.
        cases++;
        {
            std::vector<uint8_t> file = make_gguf(false, GGUF_DEFAULT_ALIGNMENT, false);
            recorder rec;
            rec.data = file.data();
            rec.short_by = 1;
            gguf_init_params params{};
            params.no_alloc = true;
            struct gguf_context * ctx = REF.init_cb(record_cb, &rec, 4, file.size(), params);
            eq_int("short read is eof", 0, ggh_reader_continues(4, 3));
            values++;
            if (ctx != nullptr) {
                fails++;
                printf("FAIL a short read was accepted\n");
                REF.free_ctx(ctx);
            }
            struct gguf_context * null_ctx = REF.init_cb(nullptr, nullptr, 0, file.size(), params);
            values++;
            if (null_ctx != nullptr) {
                fails++;
                printf("FAIL a null callback was accepted\n");
                REF.free_ctx(null_ctx);
            }
        }
        dlclose(REF.h);
    }

    printf("FG651_GGH cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
