// Staged S1.2 gate: the Goose GGUF writer against the native writer.
//
// One binary, two implementations of the same three public calls:
//   native     gguf_write_to_buf_native / gguf_get_meta_size_native /
//              gguf_get_meta_data_native (the original bodies in gguf.cpp);
//   selected   gguf_write_to_buf / gguf_get_meta_size / gguf_get_meta_data
//              (native with FG_GOOSE_GGUF=OFF, the Goose module with it ON).
//
// For every synthetic context the gate compares the full byte image (header, KV
// list, tensor descriptors, pad, data section), the metadata size and the
// metadata bytes. No model, no GPU, no server: contexts are built through the
// public gguf API only.
//
// Exit code 1 when any comparison fails.
#include "ggml.h"
#include "gguf.h"

#include "fg-goose-gguf-native.h"
#ifdef FG_GOOSE_GGUF
#include "fg-goose-gguf-bridge.h"
#endif

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int g_cases  = 0;
static int g_checks = 0;
static int g_fail   = 0;

static void expect(const char * name, const char * what, bool ok, const std::string & detail = "") {
    g_checks++;
    if (ok) {
        printf("OK\t%s\t%s\n", name, what);
    } else {
        g_fail++;
        printf("FAIL\t%s\t%s\t%s\n", name, what, detail.c_str());
    }
}

static void compare_bytes(const char * name, const char * what,
                          const std::vector<int8_t> & a, const std::vector<int8_t> & b) {
    g_checks++;
    if (a.size() != b.size()) {
        char d[128];
        snprintf(d, sizeof d, "size native=%zu goose=%zu", a.size(), b.size());
        g_fail++;
        printf("FAIL\t%s\t%s\t%s\n", name, what, d);
        return;
    }
    for (size_t i = 0; i < a.size(); i++) {
        if (a[i] != b[i]) {
            char d[160];
            snprintf(d, sizeof d, "first_diff=%zu native=0x%02x goose=0x%02x",
                     i, (unsigned) (uint8_t) a[i], (unsigned) (uint8_t) b[i]);
            g_fail++;
            printf("FAIL\t%s\t%s\t%s\n", name, what, d);
            return;
        }
    }
    printf("OK\t%s\t%s\n", name, what);
}

// ---------------------------------------------------------------- case model

struct Case {
    std::string name;
    gguf_context * ctx;
    bool meta_only = false;  // set when the context has no safe data image
};

static std::vector<Case> g_cases_list;
static ggml_context *     g_ggml = nullptr;

static gguf_context * new_case(const char * name) {
    gguf_context * ctx = gguf_init_empty();
    g_cases_list.push_back(Case{name, ctx});
    return ctx;
}

// One tensor with a deterministic byte pattern.
static ggml_tensor * make_tensor(const char * name, enum ggml_type type,
                                 int64_t ne0, int64_t ne1, int64_t ne2, int64_t ne3,
                                 uint8_t seed) {
    ggml_tensor * t = ggml_new_tensor_4d(g_ggml, type, ne0, ne1, ne2, ne3);
    ggml_set_name(t, name);
    const size_t n = ggml_nbytes(t);
    auto * p = static_cast<uint8_t *>(t->data);
    for (size_t i = 0; i < n; i++) {
        p[i] = static_cast<uint8_t>(seed + i * 31u);
    }
    return t;
}

// Hand-built fixture: a GGUF v3 file whose declared alignment (64) is
// self-consistent, so the native reader accepts it and gives the context
// alignment = 64. gguf_init_empty() fixes alignment at 32 and no public setter
// changes it, so a reload is the only way to reach the 64-byte path.
// Layout: one KV (general.alignment = u32 64) and one F32 tensor with 8
// elements; header 24 B, KV 43 B, tensor meta 30 B, pad to 64, data 32 B, pad.
static bool write_align64_fixture(const char * path) {
    std::vector<uint8_t> b;
    auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; i++) b.push_back((v >> (8 * i)) & 0xff); };
    auto u64 = [&](uint64_t v) { for (int i = 0; i < 8; i++) b.push_back((v >> (8 * i)) & 0xff); };
    auto str = [&](const char * s) {
        const uint64_t n = strlen(s);
        u64(n);
        b.insert(b.end(), s, s + n);
    };

    b.push_back('G'); b.push_back('G'); b.push_back('U'); b.push_back('F');
    u32(3);            // version
    u64(1);            // n_tensors
    u64(1);            // n_kv
    str("general.alignment");
    u32(4);            // GGUF_TYPE_UINT32
    u32(64);
    str("r.a");
    u32(1);            // n_dims
    u64(8);            // ne[0]
    u32(0);            // GGML_TYPE_F32
    u64(0);            // offset
    while (b.size() % 64 != 0) b.push_back(0);
    for (size_t i = 0; i < 8 * sizeof(float); i++) {
        b.push_back(static_cast<uint8_t>(91 + i * 7));
    }
    while (b.size() % 64 != 0) b.push_back(0);

    FILE * f = fopen(path, "wb");
    if (f == nullptr) return false;
    const bool ok = fwrite(b.data(), 1, b.size(), f) == b.size();
    fclose(f);
    return ok;
}

static void build_cases() {
  // 1. an empty context: only the header and the alignment pad
  {
      new_case("empty");
  }

  // 2. every scalar type
  {
      gguf_context * c = new_case("scalars-all");
      gguf_set_val_u8  (c, "k.u8",   200);
      gguf_set_val_i8  (c, "k.i8",   -100);
      gguf_set_val_u16 (c, "k.u16",  60000);
      gguf_set_val_i16 (c, "k.i16",  -30000);
      gguf_set_val_u32 (c, "k.u32",  4000000000u);
      gguf_set_val_i32 (c, "k.i32",  -2000000000);
      gguf_set_val_f32 (c, "k.f32",  -3.5f);
      gguf_set_val_u64 (c, "k.u64",  0x123456789abcdef0ull);
      gguf_set_val_i64 (c, "k.i64",  -0x123456789);
      gguf_set_val_f64 (c, "k.f64",  2.718281828459045);
      gguf_set_val_bool(c, "k.bool", true);
      gguf_set_val_str (c, "general.name", "fg-goose-gguf-s1.2");
  }

  // 3. numeric and bool arrays
  {
      gguf_context * c = new_case("arrays-numeric");
      const uint32_t u32[3] = {1u, 2u, 0xdeadbeefu};
      const int32_t  i32[2] = {-7, 42};
      const float    f32[4] = {0.5f, -0.25f, 1e10f, -0.0f};
      const uint8_t  bl[5]  = {1, 0, 1, 1, 0};
      gguf_set_arr_data(c, "a.u32", GGUF_TYPE_UINT32,  u32, 3);
      gguf_set_arr_data(c, "a.i32", GGUF_TYPE_INT32,   i32, 2);
      gguf_set_arr_data(c, "a.f32", GGUF_TYPE_FLOAT32, f32, 4);
      gguf_set_arr_data(c, "a.bool", GGUF_TYPE_BOOL,   bl,  5);
  }

  // 4. array of strings
  {
      gguf_context * c = new_case("array-str");
      const char * s[3] = {"alpha", "", "gamma-delta"};
      gguf_set_arr_str(c, "tokenizer.ggml.tokens", s, 3);
      const char * e[1] = {"single"};
      gguf_set_arr_str(c, "tokenizer.ggml.merges", e, 1);
  }

  // 5. a key set twice: the native setter removes and appends
  {
      gguf_context * c = new_case("overwrite-key");
      gguf_set_val_str(c, "general.name", "first");
      gguf_set_val_u32(c, "general.quantization_version", 2);
      gguf_set_val_str(c, "general.name", "second");
  }

  // 6. remove in the middle, then re-add: wire order must follow
  {
      gguf_context * c = new_case("remove-readd");
      gguf_set_val_u32(c, "x.a", 1);
      gguf_set_val_u32(c, "x.b", 2);
      gguf_set_val_u32(c, "x.c", 3);
      gguf_remove_key(c, "x.b");
      gguf_set_val_u32(c, "x.b", 4);
  }

  // 7. general.alignment as a KV while the context alignment stays 32:
  //    the writer pads by the context alignment, not by the key
  {
      gguf_context * c = new_case("reserved-alignment-kv");
      gguf_set_val_u32(c, "general.alignment", 64);
      gguf_set_val_str(c, "general.name", "align-in-kv");
      gguf_add_tensor(c, make_tensor("t.a", GGML_TYPE_F32, 8, 1, 1, 1, 3));
  }

  // 8. one 2-D f32 tensor
  {
      gguf_context * c = new_case("tensor-f32-2d");
      gguf_set_val_u32(c, "general.alignment", 32);
      gguf_add_tensor(c, make_tensor("blk.0.attn_q.weight", GGML_TYPE_F32, 8, 4, 1, 1, 11));
  }

  // 9. f16 and i8 1-D tensors
  {
      gguf_context * c = new_case("tensor-f16-i8");
      gguf_add_tensor(c, make_tensor("t.f16", GGML_TYPE_F16, 33, 1, 1, 1, 5));
      gguf_add_tensor(c, make_tensor("t.i8",  GGML_TYPE_I8,  17, 1, 1, 1, 9));
  }

  // 10. quantized types
  {
      gguf_context * c = new_case("tensor-quant");
      gguf_add_tensor(c, make_tensor("t.q4_0", GGML_TYPE_Q4_0, 64, 2, 1, 1, 21));
      gguf_add_tensor(c, make_tensor("t.q8_0", GGML_TYPE_Q8_0, 32, 1, 1, 1, 23));
  }

  // 11. 4-D tensor: n_dims comes from ne[], not from the caller
  {
      gguf_context * c = new_case("tensor-4d");
      gguf_add_tensor(c, make_tensor("t.4d", GGML_TYPE_F32, 2, 3, 4, 5, 31));
  }

  // 12. three tensors with unequal sizes: the data section padding chain
  {
      gguf_context * c = new_case("tensor-chain");
      gguf_add_tensor(c, make_tensor("t.small", GGML_TYPE_F32, 3, 1, 1, 1, 41));
      gguf_add_tensor(c, make_tensor("t.mid",   GGML_TYPE_F16, 7, 3, 1, 1, 43));
      gguf_add_tensor(c, make_tensor("t.odd",   GGML_TYPE_I16, 5, 1, 1, 1, 47));
  }

  // 13. a larger tensor: the full data section, not just descriptors
  {
      gguf_context * c = new_case("tensor-big");
      gguf_set_val_str(c, "general.name", "big");
      gguf_add_tensor(c, make_tensor("t.big", GGML_TYPE_F32, 64, 64, 1, 1, 61));
  }

  // 14. a context reloaded from a 64-aligned file: the reader is the only
  //     public way to give a context a non-default alignment
  {
      const char * path = "/tmp/fg630-align64.gguf";
      if (!write_align64_fixture(path)) {
          fprintf(stderr, "cannot create %s\n", path);
          return;
      }

      gguf_init_params ip{};
      ip.no_alloc = true;
      ggml_context * tensor_ctx = nullptr;
      ip.ctx = &tensor_ctx;
      gguf_context * r = gguf_init_from_file(path, ip);
      if (r == nullptr) {
          fprintf(stderr, "cannot reload %s\n", path);
          return;
      }
      if (gguf_get_alignment(r) != 64) {
          fprintf(stderr, "reloaded alignment is %zu, expected 64\n", gguf_get_alignment(r));
      }
      // The reader leaves ggml_tensor::buffer uninitialised in a parsed
      // context, so a full-data write of a reloaded context is unsafe in the
      // native path itself; this case compares the metadata section only.
      g_cases_list.push_back(Case{"align64-reload", r, /*meta_only =*/ true});
      remove(path);
  }
}

#ifdef FG_GOOSE_GGUF
// ---- split plan: the whole-plan form against the per-tensor predicate -------
//
// gsp_split_layout must return the same shard ranges as replaying
// gsp_should_split over the same nbytes with the native padding rule. This is
// an internal invariant of the module (layout and predicate are two views of
// one plan), not a native oracle: the native differential for the plan is the
// split tool comparison in scripts/split-gate.sh.

struct PlanScenario {
    const char * name;
    std::vector<std::int64_t> nb;
    std::int32_t mode;
    std::int64_t limit_tensors;
    std::int64_t limit_bytes;
    bool no_tensor_first;
};

static std::int64_t pad32(std::int64_t v) {
    return ((v + 31) / 32) * 32;
}

static std::int64_t replay_plan(const PlanScenario & sc, std::vector<std::int64_t> & t0,
                                std::vector<std::int64_t> & t1) {
    const std::int64_t n = static_cast<std::int64_t>(sc.nb.size());
    std::int64_t n_split = 1;
    t0.assign(1, 0);
    std::int64_t cur = 0;
    if (sc.no_tensor_first) {
        n_split = 2;
        t0.push_back(0);
    }
    std::int64_t last = n_split - 1;
    for (std::int64_t i = 0; i < n; i++) {
        const std::int64_t padded = pad32(sc.nb[i]);
        const std::int64_t next   = cur + padded;
        const std::int64_t s = fg_goose::should_split(sc.mode, i, n, next,
                                                      sc.limit_tensors, sc.limit_bytes);
        if (s < 0) {
            return -1;
        }
        if (s > 0) {
            const std::int64_t cnt = i - t0[last];
            if (cnt == 0 && !(last == 0 && sc.no_tensor_first)) {
                return -1;
            }
            n_split++;
            last++;
            t0.push_back(i);
            cur = padded;
        } else {
            cur = next;
        }
    }
    t1.assign(n_split, 0);
    for (std::int64_t s = 0; s < n_split; s++) {
        t1[s] = s + 1 < n_split ? t0[s + 1] : n;
    }
    return n_split;
}

static void run_plan_case(const PlanScenario & sc) {
    const std::int64_t n = static_cast<std::int64_t>(sc.nb.size());
    const std::int64_t cap = n + 2;
    std::vector<std::int64_t> t0(cap, 0), t1(cap, 0);
    const std::int64_t got = fg_goose::split_layout(n, sc.mode, sc.limit_tensors,
                                                    sc.limit_bytes, sc.no_tensor_first,
                                                    sc.nb.data(), t0.data(), t1.data());

    std::vector<std::int64_t> r0, r1;
    const std::int64_t want = replay_plan(sc, r0, r1);

    expect(sc.name, "plan-count", got == want,
           std::to_string(got) + " vs " + std::to_string(want));

    // Both sides refusing the same scenario is agreement as well: the module
    // and the replay return -1 for a shard that would hold no tensor.
    bool same = got == want;
    if (same && got > 0) {
        for (std::int64_t s = 0; s < got; s++) {
            if (t0[s] != r0[s] || t1[s] != r1[s]) {
                same = false;
                break;
            }
        }
    }
    expect(sc.name, "plan-ranges", same);
}

static void run_plan_cases() {
    std::vector<PlanScenario> sc;
    {
        PlanScenario s{"plan-uniform-size", {}, 2, 0, 1000000, false};
        s.nb.assign(128, 16384);
        sc.push_back(s);
    }
    {
        PlanScenario s{"plan-uniform-size-notf", {}, 2, 0, 1000000, true};
        s.nb.assign(128, 16384);
        sc.push_back(s);
    }
    {
        PlanScenario s{"plan-mixed-size", {1000, 2000, 500, 4096, 32, 768, 16384, 96},
                       2, 0, 4096, false};
        sc.push_back(s);
    }
    {
        PlanScenario s{"plan-tensor-mode", {}, 1, 3, 0, false};
        s.nb.assign(10, 512);
        sc.push_back(s);
    }
    {
        PlanScenario s{"plan-tensor-mode-notf", {}, 1, 4, 0, true};
        s.nb.assign(10, 512);
        sc.push_back(s);
    }
    {
        PlanScenario s{"plan-single-over-limit", {64}, 2, 0, 32, false};
        sc.push_back(s);
    }
    {
        PlanScenario s{"plan-exact-boundary", {}, 2, 0, 1000000, false};
        s.nb.assign(61, 16384);
        s.nb.push_back(576);  // 61*16384 + 576 == 1,000,000 exactly
        sc.push_back(s);
    }
    for (const PlanScenario & s : sc) {
        run_plan_case(s);
    }
}
#endif

static void run_case(const Case & c) {
    std::vector<int8_t> meta_native, meta_goose, full_native, full_goose;

    if (!c.meta_only) {
        gguf_write_to_buf_native(c.ctx, full_native, /*only_meta =*/ false);
        gguf_write_to_buf(c.ctx, full_goose, /*only_meta =*/ false);
        compare_bytes(c.name.c_str(), "full-bytes", full_native, full_goose);
    }

    const size_t sz_native = gguf_get_meta_size_native(c.ctx);
    const size_t sz_goose  = gguf_get_meta_size(c.ctx);
    expect(c.name.c_str(), "meta-size", sz_native == sz_goose,
           std::to_string(sz_native) + " vs " + std::to_string(sz_goose));

    meta_native.resize(sz_native);
    meta_goose.resize(sz_goose);
    gguf_get_meta_data_native(c.ctx, meta_native.data());
    gguf_get_meta_data(c.ctx, meta_goose.data());
    compare_bytes(c.name.c_str(), "meta-bytes", meta_native, meta_goose);

    g_cases++;
}

int main() {
    ggml_init_params gp{};
    gp.mem_size   = 256u * 1024u * 1024u;
    gp.mem_buffer = nullptr;
    gp.no_alloc   = false;
    g_ggml = ggml_init(gp);
    if (g_ggml == nullptr) {
        fprintf(stderr, "ggml_init failed\n");
        return 1;
    }

    build_cases();
    for (const Case & c : g_cases_list) {
        run_case(c);
    }
#ifdef FG_GOOSE_GGUF
    run_plan_cases();
#endif
    for (const Case & c : g_cases_list) {
        gguf_free(c.ctx);
    }
    ggml_free(g_ggml);

#ifdef FG_GOOSE_GGUF
    const char * mode = "goose";
    const uint64_t calls = fg_goose::calls(fg_goose::ENTRY_WRITE) +
                           fg_goose::calls(fg_goose::ENTRY_META_SIZE) +
                           fg_goose::calls(fg_goose::ENTRY_META_DATA) +
                           fg_goose::calls(fg_goose::ENTRY_SPLIT) +
                           fg_goose::calls(fg_goose::ENTRY_LAYOUT);
    printf("FG630_SMOKE mode=%s cases=%d checks=%d failures=%d goose_calls=%llu "
           "(write=%llu meta_size=%llu meta_data=%llu split=%llu layout=%llu)\n",
           mode, g_cases, g_checks, g_fail, (unsigned long long) calls,
           (unsigned long long) fg_goose::calls(fg_goose::ENTRY_WRITE),
           (unsigned long long) fg_goose::calls(fg_goose::ENTRY_META_SIZE),
           (unsigned long long) fg_goose::calls(fg_goose::ENTRY_META_DATA),
           (unsigned long long) fg_goose::calls(fg_goose::ENTRY_SPLIT),
           (unsigned long long) fg_goose::calls(fg_goose::ENTRY_LAYOUT));
#else
    printf("FG630_SMOKE mode=native cases=%d checks=%d failures=%d goose_calls=0\n",
           g_cases, g_checks, g_fail);
#endif
    return g_fail == 0 ? 0 : 1;
}
