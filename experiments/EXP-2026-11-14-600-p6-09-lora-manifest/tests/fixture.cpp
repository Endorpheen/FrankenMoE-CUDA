// Differential gate for P6-09 S1, LoRA manifest parsing and tensor pairing
// (EXP-2026-11-14-583).
//
// Oracle strategy: the real llama_adapter_lora_init_impl() is executed on
// synthetic adapter files written by the fixture with the GGUF writer. No base
// model weights are loaded: TestModel implements the seven pure virtuals of
// llama_model as unused stubs and carries only arch, tensors_by_name and
// hdmd.weight_blocks, the fields the LoRA path reads.
//
// Every case builds three views of the same scenario:
//   - the GGUF adapter file on disk, which the native code parses, validates
//     and pairs with real llama_adapter_lora_init_from_file_ptr();
//   - the flat metadata/tensor view the Goose port consumes (a caller-decoded
//     GGUF, so the port owns no file format);
//   - the base model tensor table.
// The gate then compares the native outcome with the port outcome: the error
// class, alpha, aLoRA tokens, the pair set, the A/B tensor names inside every
// pair and the rank/alpha scale. On success it also reads the native adapter
// tensors back and checks their bytes against the generator pattern, so a
// swapped pair cannot pass unnoticed.
//
// Output: FG583_LORA cases=N values=M failures=K
// Exit: 0 ok, 1 failures, 3 native setup failed.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <exception>
#include <map>
#include <string>
#include <vector>
#include <algorithm>

#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpp.h"
#include "gguf.h"
#include "llama.h"

#define private public
#include "llama-adapter.h"
#undef private

#include "llama-model.h"

#include "lora-manifest-gen.h"

// Defined in src/llama-adapter.cpp and exported by libllama, but not declared in
// a public header of this tree.
llama_adapter_lora * llama_adapter_lora_init_from_file_ptr(llama_model * model, FILE * file);

static int cases = 0;
static int values = 0;
static int fails = 0;

static std::string g_dir = ".";

static std::string g_err;

static void log_cb(ggml_log_level level, const char * text, void * user_data) {
    (void) user_data;
    if (level == GGML_LOG_LEVEL_ERROR) {
        g_err += text;
    }
}

static void fail(const std::string & c, const std::string & what) {
    ++fails;
    if (fails <= 200) {
        printf("FAIL %s: %s\n", c.c_str(), what.c_str());
    }
}

static void eq_i64(const std::string & c, const char * what, int64_t native, int64_t port) {
    ++values;
    if (native != port) {
        fail(c, std::string(what) + ": native=" + std::to_string(native) +
                    " port=" + std::to_string(port));
    }
}

static void eq_str(const std::string & c, const char * what,
                   const std::string & native, const std::string & port) {
    ++values;
    if (native != port) {
        fail(c, std::string(what) + ": native='" + native + "' port='" + port + "'");
    }
}

static void eq_f32(const std::string & c, const char * what, float native, float port) {
    ++values;
    if (memcmp(&native, &port, sizeof(float)) != 0) {
        fail(c, std::string(what) + ": native=" + std::to_string(native) +
                    " port=" + std::to_string(port));
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
// Scenario description

struct KV {
    std::string key;
    int64_t     type = 8;    // gguf_type: 4 u32, 6 f32, 8 string, 9 array
    std::string str;
    float       f32 = 0.0f;
    uint32_t    u32 = 0;
    int64_t     elem = 4;    // array element type
    std::vector<int64_t> elems;
};

struct TS {
    std::string name;
    int64_t     ne0 = 8;
    int64_t     ne1 = 8;
    bool        hadamard = false;
};

struct Case {
    std::string     name;
    std::string     arch = "llama";   // base model arch name
    std::vector<KV> kv;
    std::vector<TS> tensors;          // adapter tensors, GGUF order
    std::vector<TS> model;            // base-model tensors
};

static KV kv_str(const std::string & k, const std::string & v) {
    KV e;
    e.key = k;
    e.type = 8;
    e.str = v;
    return e;
}

static KV kv_f32(const std::string & k, float v) {
    KV e;
    e.key = k;
    e.type = 6;
    e.f32 = v;
    return e;
}

static KV kv_u32(const std::string & k, uint32_t v) {
    KV e;
    e.key = k;
    e.type = 4;
    e.u32 = v;
    return e;
}

static KV kv_arr(const std::string & k, int64_t elem, std::vector<int64_t> v) {
    KV e;
    e.key = k;
    e.type = 9;
    e.elem = elem;
    e.elems = std::move(v);
    return e;
}

static TS ts(const std::string & n, int64_t ne0, int64_t ne1) {
    TS t;
    t.name = n;
    t.ne0 = ne0;
    t.ne1 = ne1;
    return t;
}

// A regular pair: model [in, out], a [in, rank], b [rank, out].
static void add_pair(Case & c, const std::string & base, int64_t in, int64_t out, int64_t rank) {
    c.tensors.push_back(ts(base + ".lora_a", in, rank));
    c.tensors.push_back(ts(base + ".lora_b", rank, out));
    c.model.push_back(ts(base, in, out));
}

static Case base_case(const std::string & name) {
    Case c;
    c.name = name;
    c.kv.push_back(kv_str("general.type", "adapter"));
    c.kv.push_back(kv_str("general.architecture", "llama"));
    c.kv.push_back(kv_str("adapter.type", "lora"));
    c.kv.push_back(kv_f32("adapter.lora.alpha", 8.0f));
    return c;
}

// ---------------------------------------------------------------------------
// Synthetic adapter file

// Deterministic per-name payload, so a swapped A/B pair shows up in the bytes.
static std::vector<float> pattern(const std::string & name, size_t n) {
    uint32_t h = 2166136261u;
    for (unsigned char ch : name) {
        h = (h ^ ch) * 16777619u;
    }
    std::vector<float> v(n);
    for (size_t i = 0; i < n; ++i) {
        v[i] = (float) ((h + i * 7u) % 101u) * 0.25f - 8.0f;
    }
    return v;
}

static bool write_gguf(const Case & cs, const std::string & path) {
    const size_t overhead = (cs.tensors.size() + 4) * ggml_tensor_overhead() + (1u << 20);
    ggml_init_params ip = { overhead, nullptr, false };
    ggml_context * ctx = ggml_init(ip);
    if (ctx == nullptr) {
        return false;
    }

    std::vector<ggml_tensor *> tensors;
    for (const TS & s : cs.tensors) {
        ggml_tensor * t = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, s.ne0, s.ne1);
        if (t == nullptr) {
            ggml_free(ctx);
            return false;
        }
        ggml_set_name(t, s.name.c_str());
        const std::vector<float> p = pattern(s.name, (size_t) (s.ne0 * s.ne1));
        memcpy(t->data, p.data(), p.size() * sizeof(float));
        tensors.push_back(t);
    }

    gguf_context * g = gguf_init_empty();
    for (const KV & e : cs.kv) {
        if (e.type == 8) {
            gguf_set_val_str(g, e.key.c_str(), e.str.c_str());
        } else if (e.type == 6) {
            gguf_set_val_f32(g, e.key.c_str(), e.f32);
        } else if (e.type == 4) {
            gguf_set_val_u32(g, e.key.c_str(), e.u32);
        } else if (e.type == 9) {
            if (e.elem == 4) {
                std::vector<uint32_t> a(e.elems.size());
                for (size_t i = 0; i < a.size(); ++i) {
                    a[i] = (uint32_t) e.elems[i];
                }
                gguf_set_arr_data(g, e.key.c_str(), GGUF_TYPE_UINT32, a.data(), a.size());
            } else {
                std::vector<int32_t> a(e.elems.size());
                for (size_t i = 0; i < a.size(); ++i) {
                    a[i] = (int32_t) e.elems[i];
                }
                gguf_set_arr_data(g, e.key.c_str(), GGUF_TYPE_INT32, a.data(), a.size());
            }
        }
    }
    for (ggml_tensor * t : tensors) {
        gguf_add_tensor(g, t);
    }
    const bool ok = gguf_write_to_file(g, path.c_str(), false);
    gguf_free(g);
    ggml_free(ctx);
    return ok;
}

static int64_t file_size(const std::string & path) {
    FILE * f = fopen(path.c_str(), "rb");
    if (f == nullptr) {
        return -1;
    }
    fseek(f, 0, SEEK_END);
    const int64_t n = (int64_t) ftell(f);
    fclose(f);
    return n;
}

// ---------------------------------------------------------------------------
// Native oracle

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

struct NativeModel {
    TestModel                  model;
    ggml_context *             ctx = nullptr;
    ggml_backend_buffer_t      buf = nullptr;
    bool                       ok = false;

    NativeModel() : model(llama_model_default_params()) {}

    ~NativeModel() {
        if (buf != nullptr) {
            ggml_backend_buffer_free(buf);
        }
        if (ctx != nullptr) {
            ggml_free(ctx);
        }
    }

    bool build(const Case & cs) {
        model.arch = LLM_ARCH_LLAMA;
        model.hparams.vocab_only = false;

        const size_t overhead = (cs.model.size() + 4) * ggml_tensor_overhead() + (1u << 20);
        ggml_init_params ip = { overhead, nullptr, true };
        ctx = ggml_init(ip);
        if (ctx == nullptr) {
            return false;
        }
        for (const TS & s : cs.model) {
            ggml_tensor * t = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, s.ne0, s.ne1);
            if (t == nullptr) {
                return false;
            }
            ggml_set_name(t, s.name.c_str());
            model.tensors_by_name.emplace_back(s.name, t);
            if (s.hadamard) {
                model.hdmd.weight_blocks[s.name] = 1u;
            }
        }

        ggml_backend_dev_t cpu = ggml_backend_dev_by_type(GGML_BACKEND_DEVICE_TYPE_CPU);
        if (cpu == nullptr) {
            return false;
        }
        if (model.tensors_by_name.empty()) {
            // An adapter with no pairs never calls get_tensor, and an empty
            // context cannot be allocated by ggml_backend_alloc_ctx_tensors_from_buft.
            ok = true;
            return true;
        }
        ggml_backend_buffer_type_t buft = ggml_backend_dev_buffer_type(cpu);
        if (buft == nullptr) {
            return false;
        }
        buf = ggml_backend_alloc_ctx_tensors_from_buft(ctx, buft);
        if (buf == nullptr) {
            return false;
        }
        ggml_backend_buffer_clear(buf, 0);
        ok = true;
        return true;
    }
};

static int classify(const std::string & msg) {
    if (msg.find("failed to load lora adapter from file") != std::string::npos) return -1;
    if (msg.find("expect general.type to be 'adapter'") != std::string::npos) return -2;
    if (msg.find("model arch and LoRA arch mismatch") != std::string::npos) return -3;
    if (msg.find("expect adapter.type to be 'lora'") != std::string::npos) return -4;
    if (msg.find("invalid gguf type for adapter.alora") != std::string::npos) return -5;
    if (msg.find("invalid gguf element type for adapter.alora") != std::string::npos) return -6;
    if (msg.find("has unexpected suffix") != std::string::npos) return -7;
    if (msg.find("is missing one component") != std::string::npos) return -8;
    if (msg.find("does not exist in base model") != std::string::npos) return -9;
    if (msg.find("prism.hadamard folded weight") != std::string::npos) return -10;
    if (msg.find("has incorrect shape") != std::string::npos) return -11;
    if (msg.find("lora_a tensor is not transposed") != std::string::npos) return -12;
    if (msg.find("is not within the file bounds") != std::string::npos) return -13;
    return -1;
}

struct Outcome {
    int    code = 0;
    bool   ok = false;
    float  alpha = 0.0f;
    std::string err_name;
    std::vector<int64_t> toks;
    std::map<std::string, std::pair<std::string, std::string>> ab; // base -> (a, b)
    std::map<std::string, float> scale;
};

// The name the native error message quotes, e.g. "LoRA tensor pair for 'x' ...".
static std::string first_quoted(const std::string & s) {
    const size_t a = s.find('\'');
    if (a == std::string::npos) {
        return "";
    }
    const size_t b = s.find('\'', a + 1);
    if (b == std::string::npos) {
        return "";
    }
    return s.substr(a + 1, b - a - 1);
}

static Outcome run_native(const Case & cs, const std::string & path, const std::string & cname) {
    Outcome out;
    NativeModel nm;
    if (!nm.build(cs)) {
        fail(cname, "native model setup failed");
        out.code = -100;
        return out;
    }

    FILE * f = fopen(path.c_str(), "rb");
    if (f == nullptr) {
        fail(cname, "cannot open " + path);
        out.code = -100;
        return out;
    }

    g_err.clear();
    llama_log_set(log_cb, nullptr);
    llama_adapter_lora * a = llama_adapter_lora_init_from_file_ptr(&nm.model, f);
    fclose(f);

    if (a == nullptr) {
        out.code = classify(g_err);
        // The metadata and transpose messages quote no tensor name.
        if (out.code <= -7 && out.code != -12) {
            out.err_name = first_quoted(g_err);
        }
        out.ok = false;
        return out;
    }

    out.ok = true;
    out.code = 0;
    out.alpha = a->alpha;
    for (llama_token t : a->alora_invocation_tokens) {
        out.toks.push_back((int64_t) t);
    }

    for (const auto & it : a->ab_map) {
        const llama_adapter_lora_weight & w = it.second;
        const std::string an = w.a == nullptr ? "<null>" : w.a->name;
        const std::string bn = w.b == nullptr ? "<null>" : w.b->name;
        out.ab[it.first] = { an, bn };
        out.scale[it.first] = w.get_scale(a->alpha, 1.0f);

        // The adapter tensors must hold the bytes generated for those names.
        for (ggml_tensor * t : { w.a, w.b }) {
            if (t == nullptr) {
                continue;
            }
            const size_t n = ggml_nbytes(t);
            std::vector<uint8_t> got(n, 0);
            ggml_backend_tensor_get(t, got.data(), 0, n);
            const std::vector<float> want = pattern(t->name, n / sizeof(float));
            if (memcmp(got.data(), want.data(), n) != 0) {
                fail(cname, "native adapter bytes differ for " + std::string(t->name));
            }
        }
    }

    llama_adapter_lora_free(a);
    return out;
}

// ---------------------------------------------------------------------------
// Port view

struct PortView {
    std::vector<int64_t> kv_off, kv_len, kv_type, kv_v0, kv_v1, kv_v2;
    std::vector<float>   kv_f32;
    std::vector<int64_t> arr;
    std::vector<uint8_t> pool;

    std::vector<int64_t> t_off, t_len, t_ne0, t_ne1, t_offdata, t_nbytes;
    std::vector<uint8_t> tpool;

    std::vector<int64_t> m_off, m_len, m_ne0, m_ne1, m_had;
    std::vector<uint8_t> mpool;

    std::vector<uint8_t> names;
    std::vector<int64_t> pairs;
    std::vector<int64_t> pst{ std::vector<int64_t>(8, 0) };
    std::vector<int64_t> toks{ std::vector<int64_t>(64, 0) };

    int64_t data_base = 0;
    int64_t file_bytes = 0;
};

static void pool_add(std::vector<uint8_t> & pool, const std::string & s, int64_t & off, int64_t & len) {
    off = (int64_t) pool.size();
    len = (int64_t) s.size();
    pool.insert(pool.end(), s.begin(), s.end());
}

// Build the flat view from the case and the offsets the written file has.
static bool build_view(const Case & cs, const std::string & path, PortView & v) {
    for (const KV & e : cs.kv) {
        int64_t off = 0;
        int64_t len = 0;
        int64_t v0 = 0;
        int64_t v1 = 0;

        if (e.type == 8) {
            pool_add(v.pool, e.str, off, len);
            v0 = off;
            v1 = len;
        }
        if (e.type == 4) {
            v0 = (int64_t) e.u32;
        }
        if (e.type == 9) {
            v0 = (int64_t) v.arr.size();
            v1 = (int64_t) e.elems.size();
            for (int64_t x : e.elems) {
                v.arr.push_back(x);
            }
        }
        v.kv_f32.push_back(e.type == 6 ? e.f32 : 0.0f);

        int64_t koff = 0;
        int64_t klen = 0;
        pool_add(v.pool, e.key, koff, klen);
        v.kv_off.push_back(koff);
        v.kv_len.push_back(klen);
        v.kv_type.push_back(e.type);
        v.kv_v0.push_back(v0);
        v.kv_v1.push_back(v1);
        v.kv_v2.push_back(e.elem);
    }

    std::vector<int64_t> file_off;
    std::vector<int64_t> file_nbytes;
    {
        ggml_context * meta = nullptr;
        gguf_init_params gp = { /*.no_alloc =*/ true, /*.ctx =*/ &meta };
        gguf_context * g = gguf_init_from_file(path.c_str(), gp);
        if (g == nullptr) {
            return false;
        }
        ggml_free(meta);
        v.data_base = (int64_t) gguf_get_data_offset(g);
        for (const TS & s : cs.tensors) {
            const int64_t id = gguf_find_tensor(g, s.name.c_str());
            if (id < 0) {
                gguf_free(g);
                return false;
            }
            file_off.push_back((int64_t) gguf_get_tensor_offset(g, id));
            file_nbytes.push_back((int64_t) gguf_get_tensor_size(g, id));
        }
        gguf_free(g);
    }
    v.file_bytes = file_size(path);
    if (v.file_bytes < 0) {
        return false;
    }

    for (size_t i = 0; i < cs.tensors.size(); ++i) {
        int64_t off = 0;
        int64_t len = 0;
        pool_add(v.tpool, cs.tensors[i].name, off, len);
        v.t_off.push_back(off);
        v.t_len.push_back(len);
        v.t_ne0.push_back(cs.tensors[i].ne0);
        v.t_ne1.push_back(cs.tensors[i].ne1);
        v.t_offdata.push_back(file_off[i]);
        v.t_nbytes.push_back(file_nbytes[i]);
    }
    for (const TS & s : cs.model) {
        int64_t off = 0;
        int64_t len = 0;
        pool_add(v.mpool, s.name, off, len);
        v.m_off.push_back(off);
        v.m_len.push_back(len);
        v.m_ne0.push_back(s.ne0);
        v.m_ne1.push_back(s.ne1);
        v.m_had.push_back(s.hadamard ? 1 : 0);
    }

    v.names.assign(4096, 0);
    v.pairs.assign((size_t) (4 * (cs.tensors.size() + 2)), 0);
    return true;
}

static Outcome run_port(const Case & cs, const std::string & path, const std::string & cname) {
    Outcome out;
    PortView v;
    if (!build_view(cs, path, v)) {
        fail(cname, "cannot build the port view");
        out.code = -100;
        return out;
    }

    std::vector<uint8_t> arch(cs.arch.begin(), cs.arch.end());
    std::vector<int64_t> mst(4, 0);
    std::vector<float>   outf(1, 0.0f);

    int64_t rc = lora_manifest(
        (int64_t) v.kv_off.size(),
        si(v.kv_off), si(v.kv_len), si(v.kv_type), si(v.kv_v0), si(v.kv_v1), si(v.kv_v2),
        sf(v.kv_f32), si(v.arr), su(v.pool), su(arch),
        si(mst), sf(outf), si(v.toks));

    out.code = (int) rc;
    if (rc != 0) {
        return out;
    }
    out.alpha = outf[0];
    for (int64_t i = 0; i < mst[0]; ++i) {
        out.toks.push_back(v.toks[(size_t) i]);
    }

    rc = lora_pair(
        (int64_t) cs.tensors.size(), si(v.t_off), si(v.t_len), su(v.tpool),
        su(v.names), si(v.pairs), si(v.pst));
    out.code = (int) rc;
    if (rc != 0) {
        if (v.pst[3] >= 0 && v.pst[4] > 0) {
            out.err_name.assign((const char *) v.tpool.data() + v.pst[3], (size_t) v.pst[4]);
        }
        return out;
    }
    const int64_t n_pairs = v.pst[0];

    std::vector<int64_t> vst(4, 0);
    rc = lora_validate_pairs(
        n_pairs, si(v.pairs), su(v.names), si(v.t_ne0), si(v.t_ne1),
        (int64_t) v.m_off.size(), si(v.m_off), si(v.m_len), si(v.m_ne0), si(v.m_ne1), si(v.m_had),
        su(v.mpool), si(vst));
    out.code = (int) rc;
    if (rc != 0) {
        if (rc != -12 && vst[0] >= 0 && vst[1] > 0) {
            out.err_name.assign((const char *) v.names.data() + vst[0], (size_t) vst[1]);
        }
        return out;
    }

    std::vector<int64_t> bst(2, 0);
    rc = lora_bounds(
        n_pairs, si(v.pairs), si(v.t_offdata), si(v.t_nbytes),
        v.data_base, v.file_bytes, si(bst));
    out.code = (int) rc;
    if (rc != 0) {
        const int64_t t = bst[0];
        if (t >= 0) {
            out.err_name.assign((const char *) v.tpool.data() + v.t_off[(size_t) t],
                                (size_t) v.t_len[(size_t) t]);
        }
        return out;
    }

    out.ok = true;
    for (int64_t p = 0; p < n_pairs; ++p) {
        const int64_t noff = v.pairs[(size_t) (4 * p + 0)];
        const int64_t nlen = v.pairs[(size_t) (4 * p + 1)];
        const int64_t a = v.pairs[(size_t) (4 * p + 2)];
        const int64_t b = v.pairs[(size_t) (4 * p + 3)];

        const std::string base((const char *) v.names.data() + noff, (size_t) nlen);
        const std::string an((const char *) v.tpool.data() + v.t_off[(size_t) a],
                             (size_t) v.t_len[(size_t) a]);
        const std::string bn((const char *) v.tpool.data() + v.t_off[(size_t) b],
                             (size_t) v.t_len[(size_t) b]);
        out.ab[base] = { an, bn };
        out.scale[base] = lora_scale(out.alpha, 1.0f, v.t_ne0[(size_t) b]);
    }
    return out;
}


// ---------------------------------------------------------------------------
// Cases

static void run_case(const Case & cs) {
    ++cases;
    const std::string path = g_dir + "/" + cs.name + ".gguf";
    if (!write_gguf(cs, path)) {
        fail(cs.name, "cannot write " + path);
        return;
    }
    const Outcome n = run_native(cs, path, cs.name);
    const Outcome p = run_port(cs, path, cs.name);
    if (n.code == -100 || p.code == -100) {
        return;
    }
    printf("CASE %-24s code=%3d pairs=%zu tokens=%zu err='%s'\n",
           cs.name.c_str(), n.code, n.ab.size(), n.toks.size(), n.err_name.c_str());
    eq_i64(cs.name, "code", n.code, p.code);
    eq_str(cs.name, "err_name", n.err_name, p.err_name);
    if (!n.ok || !p.ok) {
        return;
    }
    eq_f32(cs.name, "alpha", n.alpha, p.alpha);
    eq_i64(cs.name, "n_tokens", (int64_t) n.toks.size(), (int64_t) p.toks.size());
    for (size_t i = 0; i < n.toks.size() && i < p.toks.size(); ++i) {
        eq_i64(cs.name, ("token" + std::to_string(i)).c_str(), n.toks[i], p.toks[i]);
    }
    eq_i64(cs.name, "n_pairs", (int64_t) n.ab.size(), (int64_t) p.ab.size());
    for (const auto & it : n.ab) {
        auto pit = p.ab.find(it.first);
        if (pit == p.ab.end()) {
            fail(cs.name, "pair missing in port: '" + it.first + "'");
            continue;
        }
        eq_str(cs.name, ("a:" + it.first).c_str(), it.second.first, pit->second.first);
        eq_str(cs.name, ("b:" + it.first).c_str(), it.second.second, pit->second.second);
        eq_f32(cs.name, ("scale:" + it.first).c_str(), n.scale.at(it.first), p.scale.at(it.first));
    }
    for (const auto & it : p.ab) {
        if (n.ab.find(it.first) == n.ab.end()) {
            fail(cs.name, "pair missing in native: '" + it.first + "'");
        }
    }
}

// 1. Two well-formed pairs, alpha present.
static void c01_min_valid() {
    Case c = base_case("c01_min_valid");
    add_pair(c, "blk.0.attn_q.weight", 8, 8, 4);
    add_pair(c, "blk.0.ffn_down.weight", 8, 6, 2);
    run_case(c);
}

// 2. token_embd.weight uses the flipped shape rule.
static void c02_token_embd() {
    Case c = base_case("c02_token_embd");
    c.tensors.push_back(ts("token_embd.weight.lora_a", 8, 4));
    c.tensors.push_back(ts("token_embd.weight.lora_b", 2, 8));
    c.model.push_back(ts("token_embd.weight", 8, 4));
    add_pair(c, "blk.1.attn_k.weight", 8, 8, 4);
    run_case(c);
}

// 3. alpha absent: 0.0, and the scale keeps the caller value.
static void c03_alpha_missing() {
    Case c = base_case("c03_alpha_missing");
    c.kv.erase(c.kv.begin() + 3);
    add_pair(c, "blk.0.attn_v.weight", 8, 8, 4);
    run_case(c);
}

// 4. alpha 0.0 present, rank 1.
static void c04_alpha_zero() {
    Case c = base_case("c04_alpha_zero");
    c.kv[3] = kv_f32("adapter.lora.alpha", 0.0f);
    c.tensors.push_back(ts("blk.0.attn_q.weight.lora_a", 8, 1));
    c.tensors.push_back(ts("blk.0.attn_q.weight.lora_b", 1, 8));
    c.model.push_back(ts("blk.0.attn_q.weight", 8, 8));
    run_case(c);
}

// 5. aLoRA invocation tokens.
static void c05_alora_tokens() {
    Case c = base_case("c05_alora_tokens");
    c.kv.push_back(kv_arr("adapter.alora.invocation_tokens", 4, { 1, 0, 42, 4294967295LL }));
    add_pair(c, "blk.2.ffn_up.weight", 8, 10, 3);
    run_case(c);
}

// 6. _norm.weight tensors are skipped, not paired.
static void c06_norm_skip() {
    Case c = base_case("c06_norm_skip");
    c.tensors.push_back(ts("blk.0.attn_norm.weight", 8, 8));
    add_pair(c, "blk.0.attn_q.weight", 8, 8, 4);
    c.tensors.push_back(ts("blk.0.ffn_norm.weight", 8, 8));
    run_case(c);
}

// 7. general.type is not "adapter".
static void c07_general_type_wrong() {
    Case c = base_case("c07_general_type_wrong");
    c.kv[0] = kv_str("general.type", "model");
    add_pair(c, "blk.0.attn_q.weight", 8, 8, 4);
    run_case(c);
}

// 8. general.type missing entirely.
static void c08_general_type_missing() {
    Case c = base_case("c08_general_type_missing");
    c.kv.erase(c.kv.begin() + 0);
    add_pair(c, "blk.0.attn_q.weight", 8, 8, 4);
    run_case(c);
}

// 9. Adapter arch differs from the base model arch.
static void c09_arch_mismatch() {
    Case c = base_case("c09_arch_mismatch");
    c.kv[1] = kv_str("general.architecture", "qwen2");
    add_pair(c, "blk.0.attn_q.weight", 8, 8, 4);
    run_case(c);
}

// 10. adapter.type is not "lora".
static void c10_adapter_type_wrong() {
    Case c = base_case("c10_adapter_type_wrong");
    c.kv[2] = kv_str("adapter.type", "cvec");
    add_pair(c, "blk.0.attn_q.weight", 8, 8, 4);
    run_case(c);
}

// 11. aLoRA key is not an array.
static void c11_alora_not_array() {
    Case c = base_case("c11_alora_not_array");
    c.kv.push_back(kv_str("adapter.alora.invocation_tokens", "1,2"));
    add_pair(c, "blk.0.attn_q.weight", 8, 8, 4);
    run_case(c);
}

// 12. aLoRA array element type is not u32.
static void c12_alora_wrong_elem() {
    Case c = base_case("c12_alora_wrong_elem");
    c.kv.push_back(kv_arr("adapter.alora.invocation_tokens", 5, { 1, 2, 3 }));
    add_pair(c, "blk.0.attn_q.weight", 8, 8, 4);
    run_case(c);
}

// 13. A tensor with an unexpected suffix.
static void c13_unexpected_suffix() {
    Case c = base_case("c13_unexpected_suffix");
    add_pair(c, "blk.0.attn_q.weight", 8, 8, 4);
    c.tensors.push_back(ts("blk.0.attn_q.weight.bias", 8, 1));
    c.model.push_back(ts("blk.0.attn_q.weight.bias", 8, 1));
    run_case(c);
}

// 14. One half of a pair is missing.
static void c14_missing_component() {
    Case c = base_case("c14_missing_component");
    add_pair(c, "blk.3.attn_output.weight", 8, 8, 4);
    c.tensors.push_back(ts("blk.9.ffn_down.weight.lora_a", 8, 4));
    c.model.push_back(ts("blk.9.ffn_down.weight", 8, 8));
    run_case(c);
}

// 15. Pair name absent from the base model.
static void c15_missing_in_model() {
    Case c = base_case("c15_missing_in_model");
    add_pair(c, "blk.0.attn_q.weight", 8, 8, 4);
    add_pair(c, "blk.7.ffn_gate.weight", 8, 8, 4);
    c.model.pop_back();
    run_case(c);
}

// 16. Pair targets a Hadamard-folded weight.
static void c16_hadamard() {
    Case c = base_case("c16_hadamard");
    add_pair(c, "blk.0.attn_q.weight", 8, 8, 4);
    add_pair(c, "blk.1.attn_k.weight", 8, 8, 4);
    c.model[1].hadamard = true;
    run_case(c);
}

// 17. Wrong shape for a regular weight.
static void c17_wrong_shape() {
    Case c = base_case("c17_wrong_shape");
    c.tensors.push_back(ts("blk.0.attn_q.weight.lora_a", 8, 4));
    c.tensors.push_back(ts("blk.0.attn_q.weight.lora_b", 4, 8));
    c.model.push_back(ts("blk.0.attn_q.weight", 6, 8));
    run_case(c);
}

// 18. lora_a is not transposed: a.ne1 != b.ne0.
static void c18_not_transposed() {
    Case c = base_case("c18_not_transposed");
    c.tensors.push_back(ts("blk.0.attn_q.weight.lora_a", 8, 5));
    c.tensors.push_back(ts("blk.0.attn_q.weight.lora_b", 4, 8));
    c.model.push_back(ts("blk.0.attn_q.weight", 8, 8));
    run_case(c);
}

// 19. A name that contains ".lora_a" twice: replace_all removes both, so this
// tensor pairs with a plain ".lora_b" name instead of staying unpaired.
static void c19_strip_all() {
    Case c = base_case("c19_strip_all");
    c.tensors.push_back(ts("blk.0.attn.lora_a.weight.lora_a", 8, 4));
    c.tensors.push_back(ts("blk.0.attn.weight.lora_b", 4, 8));
    c.model.push_back(ts("blk.0.attn.weight", 8, 8));
    run_case(c);
}

// 20. Empty adapter: valid metadata, no tensors.
static void c20_empty() {
    Case c = base_case("c20_empty");
    run_case(c);
}

// 21. A broken half pair plus an unexpected suffix: the suffix scan runs over
// every tensor first, so the suffix error wins regardless of name order.
static void c21_suffix_before_pairs() {
    Case c = base_case("c21_suffix_before_pairs");
    c.tensors.push_back(ts("blk.0.attn_q.weight.lora_a", 8, 4));
    c.tensors.push_back(ts("zzz.unknown.tensor", 8, 8));
    c.model.push_back(ts("blk.0.attn_q.weight", 8, 8));
    run_case(c);
}

// 22. Two invalid pairs: the sorted name decides which error surfaces.
static void c22_pair_error_order() {
    Case c = base_case("c22_pair_error_order");
    c.tensors.push_back(ts("blk.1.ffn_up.weight.lora_a", 8, 4));
    c.tensors.push_back(ts("blk.0.attn_q.weight.lora_a", 8, 4));
    c.tensors.push_back(ts("blk.1.ffn_up.weight.lora_b", 4, 8));
    c.model.push_back(ts("blk.1.ffn_up.weight", 8, 8));
    run_case(c);
}

// 23. Three pairs, sorted order, two ranks and a token id list.
static void c23_three_pairs() {
    Case c = base_case("c23_three_pairs");
    add_pair(c, "blk.2.attn_v.weight", 8, 8, 2);
    add_pair(c, "blk.0.attn_q.weight", 8, 8, 4);
    add_pair(c, "blk.1.attn_k.weight", 8, 8, 1);
    c.kv.push_back(kv_arr("adapter.alora.invocation_tokens", 4, { 7 }));
    run_case(c);
}

// 24. token_embd.weight with the straight layout: the flipped rule must reject
// it even though a regular name with the same dimensions would pass.
static void c24_token_embd_shape() {
    Case c = base_case("c24_token_embd_shape");
    c.tensors.push_back(ts("token_embd.weight.lora_a", 8, 4));
    c.tensors.push_back(ts("token_embd.weight.lora_b", 4, 8));
    c.model.push_back(ts("token_embd.weight", 8, 8));
    run_case(c);
}

// 25. Two half pairs: the lexicographically first base name is the one the
// native sorted map reports, so the error name proves the port's ordering.
static void c25_sorted_order() {
    Case c = base_case("c25_sorted_order");
    c.tensors.push_back(ts("blk.2.attn_q.weight.lora_a", 8, 4));
    c.tensors.push_back(ts("blk.10.attn_q.weight.lora_a", 8, 4));
    run_case(c);
}

// 26. Model and b agree on ne0, but ne1 differs: the second shape comparison
// must still catch it.
static void c26_wrong_shape_ne1() {
    Case c = base_case("c26_wrong_shape_ne1");
    add_pair(c, "blk.0.attn_q.weight", 8, 8, 4);
    c.model[0].ne1 = 7;
    run_case(c);
}

// Port-only contract checks for inputs the native loader cannot accept.
static int64_t manifest_of(PortView & v, std::vector<int64_t> & toks) {
    std::vector<uint8_t> arch{ 'l', 'l', 'a', 'm', 'a' };
    std::vector<int64_t> mst(4, 0);
    std::vector<float>   outf(1, 0.0f);
    return lora_manifest(
        (int64_t) v.kv_off.size(), si(v.kv_off), si(v.kv_len), si(v.kv_type),
        si(v.kv_v0), si(v.kv_v1), si(v.kv_v2), sf(v.kv_f32), si(v.arr),
        su(v.pool), su(arch), si(mst), sf(outf), si(toks));
}

static void contract() {
    const std::string c = "contract";

    Case cs = base_case("contract");
    add_pair(cs, "blk.0.attn_q.weight", 8, 8, 4);
    const std::string path = g_dir + "/contract.gguf";
    if (!write_gguf(cs, path)) {
        fail(c, "cannot write " + path);
        return;
    }

    PortView v;
    if (!build_view(cs, path, v)) {
        fail(c, "cannot build the port view");
        return;
    }

    // 1. aLoRA token buffer too small.
    {
        PortView t = v;
        int64_t koff = 0;
        int64_t klen = 0;
        pool_add(t.pool, "adapter.alora.invocation_tokens", koff, klen);
        t.kv_off.push_back(koff);
        t.kv_len.push_back(klen);
        t.kv_type.push_back(9);
        t.kv_v0.push_back((int64_t) t.arr.size());
        t.kv_v1.push_back(5);
        t.kv_v2.push_back(4);
        t.kv_f32.push_back(0.0f);
        for (int64_t x = 0; x < 5; ++x) {
            t.arr.push_back(x);
        }
        std::vector<int64_t> toks(4, 0);
        eq_i64(c, "tokens_too_small", -14, manifest_of(t, toks));
    }

    // 2. general.type carries the wrong GGUF type.
    {
        PortView t = v;
        t.kv_type[0] = 4;   // u32 where a string is required
        std::vector<int64_t> toks(64, 0);
        eq_i64(c, "general_type_kv_type", -15, manifest_of(t, toks));
    }

    // 3. alpha carries the wrong GGUF type.
    {
        PortView t = v;
        t.kv_type[3] = 8;
        std::vector<int64_t> toks(64, 0);
        eq_i64(c, "alpha_kv_type", -15, manifest_of(t, toks));
    }

    // 4. Pair arena too small.
    {
        std::vector<uint8_t> tiny(4, 0);
        std::vector<int64_t> pairs(16, 0);
        std::vector<int64_t> pst(8, 0);
        eq_i64(c, "arena_too_small", -14, lora_pair(
            (int64_t) cs.tensors.size(), si(v.t_off), si(v.t_len), su(v.tpool),
            su(tiny), si(pairs), si(pst)));
    }

    // 5. The real pair table, then the bounds rules.
    {
        std::vector<int64_t> pst(8, 0);
        const int64_t rc = lora_pair(
            (int64_t) cs.tensors.size(), si(v.t_off), si(v.t_len), su(v.tpool),
            su(v.names), si(v.pairs), si(pst));
        eq_i64(c, "pair_ok", 0, rc);
        eq_i64(c, "pair_count", 1, pst[0]);
        eq_i64(c, "pair_name_len", 19, pst[5]);

        std::vector<int64_t> bst(2, 0);
        eq_i64(c, "bounds_ok", 0, lora_bounds(
            pst[0], si(v.pairs), si(v.t_offdata), si(v.t_nbytes), v.data_base, v.file_bytes, si(bst)));

        eq_i64(c, "bounds_short_file", -13, lora_bounds(
            pst[0], si(v.pairs), si(v.t_offdata), si(v.t_nbytes), v.data_base,
            v.data_base + 16, si(bst)));

        eq_i64(c, "bounds_wrap", -13, lora_bounds(
            pst[0], si(v.pairs), si(v.t_offdata), si(v.t_nbytes),
            9223372036854775807LL - 8, v.file_bytes, si(bst)));
    }

    // 6. Duplicate .lora_a for one base name: the later tensor wins. GGUF
    // forbids duplicate tensor names (gguf reader and writer both reject them),
    // so this defensive branch of the native map loop is port-only.
    {
        PortView d;
        for (const std::string & n : { std::string("x.lora_a"), std::string("x.lora_b"),
                                       std::string("x.lora_a") }) {
            int64_t off = 0;
            int64_t len = 0;
            pool_add(d.tpool, n, off, len);
            d.t_off.push_back(off);
            d.t_len.push_back(len);
        }
        d.names.assign(64, 0);
        d.pairs.assign(16, 0);
        std::vector<int64_t> pst(8, 0);
        eq_i64(c, "dup_pair_ok", 0, lora_pair(
            3, si(d.t_off), si(d.t_len), su(d.tpool), su(d.names), si(d.pairs), si(pst)));
        eq_i64(c, "dup_pair_count", 1, pst[0]);
        eq_i64(c, "dup_pair_a", 2, d.pairs[2]);
        eq_i64(c, "dup_pair_b", 1, d.pairs[3]);
    }

    // 7. Inconsistent view lengths are refused before any index is taken.
    {
        std::vector<uint8_t> arch{ 'l', 'l', 'a', 'm', 'a' };
        std::vector<int64_t> mst(4, 0);
        std::vector<float>   outf(1, 0.0f);
        std::vector<int64_t> toks(8, 0);
        eq_i64(c, "n_kv_negative", -14, lora_manifest(
            -1, si(v.kv_off), si(v.kv_len), si(v.kv_type), si(v.kv_v0), si(v.kv_v1), si(v.kv_v2),
            sf(v.kv_f32), si(v.arr), su(v.pool), su(arch), si(mst), sf(outf), si(toks)));

        std::vector<int64_t> pst(8, 0);
        eq_i64(c, "n_tensors_negative", -14, lora_pair(
            -1, si(v.t_off), si(v.t_len), su(v.tpool), su(v.names), si(v.pairs), si(pst)));

        std::vector<int64_t> vst(4, 0);
        eq_i64(c, "n_pairs_negative", -14, lora_validate_pairs(
            -1, si(v.pairs), su(v.names), si(v.t_ne0), si(v.t_ne1),
            1, si(v.m_off), si(v.m_len), si(v.m_ne0), si(v.m_ne1), si(v.m_had),
            su(v.mpool), si(vst)));
    }

    // 8. The two scale branches.
    eq_f32(c, "scale_alpha_zero", 0.5f, lora_scale(0.0f, 0.5f, 4));
    eq_f32(c, "scale_alpha", 0.625f, lora_scale(2.5f, 1.0f, 4));
}

int main(int argc, char ** argv) {
    if (argc > 1) {
        g_dir = argv[1];
    }

    llama_backend_init();

    ggml_backend_dev_t cpu = ggml_backend_dev_by_type(GGML_BACKEND_DEVICE_TYPE_CPU);
    if (cpu == nullptr) {
        fprintf(stderr, "FG583_LORA no CPU backend\n");
        llama_backend_free();
        return 3;
    }

    goose_init(0, nullptr);

    c01_min_valid();
    c02_token_embd();
    c03_alpha_missing();
    c04_alpha_zero();
    c05_alora_tokens();
    c06_norm_skip();
    c07_general_type_wrong();
    c08_general_type_missing();
    c09_arch_mismatch();
    c10_adapter_type_wrong();
    c11_alora_not_array();
    c12_alora_wrong_elem();
    c13_unexpected_suffix();
    c14_missing_component();
    c15_missing_in_model();
    c16_hadamard();
    c17_wrong_shape();
    c18_not_transposed();
    c19_strip_all();
    c20_empty();
    c21_suffix_before_pairs();
    c22_pair_error_order();
    c23_three_pairs();
    c24_token_embd_shape();
    c25_sorted_order();
    c26_wrong_shape_ne1();
    contract();

    printf("FG583_LORA cases=%d values=%d failures=%d\n", cases, values, fails);

    llama_backend_free();
    return fails == 0 ? 0 : 1;
}
