// Goose GGUF reader ownership for the shipping library.
//
// Why this file exists: the reader (goose/model/gguf-reader.goose) answers every
// metadata question, but it borrows a byte window and caller-owned tables that
// only a host can map. Goose cannot open or mmap the file itself - the extern
// declaration collides with the libc prototype in the generated unit - so the
// mapping lives here, in one place, and everything else stays in Goose.
//
// This is adapter debt by the project's own rule, and it is counted as such:
// tools/migration-status.sh picks up every integration/goose-standalone/*.cpp.
// It holds bytes, a mutex and the tables' storage. It decides nothing about the
// format: key lookup, array shape, payload spans and the NUL-terminated string
// table all come back from the reader.
//
// The C++ bodies it replaces are removed in a later slice, after the tokenizer
// and the loader answer through Goose and the gates pass on a real model.

#include "gguf.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Layout mirrors the generated header: the ABI is two fields per slice and the
// names differ only because this translation unit never includes that header.
struct gsl_u8x  { uint8_t * data; int64_t len; };
struct gsl_i64x { int64_t * data; int64_t len; };

extern "C" void    fg950_goose_init(int argc, char ** argv);
extern "C" int64_t gr_parse(gsl_u8x data, int64_t file_bytes, gsl_i64x state, gsl_i64x kv, gsl_i64x tensors,
                            gsl_i64x kv_slots, gsl_i64x tensor_slots);
extern "C" int64_t gr_find_kv(gsl_u8x data, gsl_i64x state, gsl_i64x kv, gsl_i64x slots, gsl_u8x key);
extern "C" int64_t gr_find_tensor(gsl_u8x data, gsl_i64x state, gsl_i64x tensors, gsl_i64x slots, gsl_u8x key);
extern "C" int64_t gr_kv_type(gsl_i64x state, gsl_i64x kv, int64_t index);
extern "C" int64_t gr_arr_type(gsl_i64x state, gsl_i64x kv, int64_t index);
extern "C" int64_t gr_arr_n(gsl_i64x state, gsl_i64x kv, int64_t index);
extern "C" int64_t gr_val_span(gsl_i64x state, gsl_i64x kv, int64_t index, gsl_i64x out);
extern "C" int64_t gr_tensor_info(gsl_i64x state, gsl_i64x tensors, int64_t index, gsl_i64x out);
extern "C" int64_t gr_string_array_index(gsl_u8x data, gsl_i64x state, gsl_i64x kv, int64_t index, gsl_i64x pairs);
extern "C" int64_t gr_arr_str_span(gsl_u8x data, gsl_i64x state, gsl_i64x kv, int64_t index, int64_t element,
                                  gsl_i64x out);
extern "C" int64_t gr_arr_str_table(gsl_u8x data, gsl_i64x state, gsl_i64x kv, int64_t index,
                                    gsl_u8x text, gsl_i64x offs);
extern "C" int64_t gr_array_int(gsl_u8x data, gsl_i64x state, gsl_i64x kv, int64_t index, int64_t element,
                                gsl_i64x out);
extern "C" int64_t bt_bpe(gsl_u8x text, gsl_u8x gguf,
                          gsl_i64x tok_off, gsl_i64x tok_len, gsl_i64x tok_slots,
                          gsl_i64x mrg_off, gsl_i64x mrg_len, gsl_i64x mrg_slots,
                          gsl_i64x ids, int64_t nout,
                          gsl_i64x cpts, gsl_i64x seg, gsl_u8x enc,
                          gsl_i64x sym, gsl_i64x heap, gsl_i64x hc, gsl_i64x info);
extern "C" int64_t btl_load(gsl_u8x gguf, gsl_i64x state, gsl_i64x kv, gsl_i64x kvslots,
                            gsl_i64x tok_off, gsl_i64x tok_len, gsl_i64x tok_slots,
                            gsl_i64x mrg_off, gsl_i64x mrg_len, gsl_i64x mrg_slots,
                            gsl_i64x pairs, gsl_i64x tok_type, gsl_i64x attr, gsl_i64x order,
                            gsl_i64x eog, int64_t eos_id, int64_t eot_id, int64_t eom_id,
                            gsl_i64x stats);

extern "C" int64_t bt_token_to_piece(gsl_u8x gguf, gsl_i64x tok_off, gsl_i64x tok_len, gsl_i64x attr,
                                    int64_t token, gsl_u8x out, int64_t at, int64_t lstrip,
                                    int64_t special, int64_t escape_ws);

namespace {

// The reader refuses a header window above 64 MiB; the same bound is used here
// because the tables and every span are offsets into this window.
constexpr int64_t HDR_MAX = 64 * 1024 * 1024;
// Sized well above any file this runtime loads (Apex: 52 keys, 753 tensors).
// gr_parse refuses a file that does not fit, and the caller then falls back.
constexpr int64_t KV_RECORDS = 4096;
constexpr int64_t TENSOR_RECORDS = 16384;
constexpr int64_t KV_SLOTS = 8192;
constexpr int64_t TENSOR_SLOTS = 32768;

struct reader_meta {
    void * hdr = nullptr;
    int64_t len = 0;
    int64_t fd = -1;
    std::vector<int64_t> state, kv, tens, kvsl, tensl;
    bool ok = false;
    struct arr_text { std::string text; std::vector<int64_t> offs; };
    mutable std::map<int64_t, arr_text> arr_str; // stable for the model's life

    // BPE tables built once by btl_load. Present only when the file carries the
    // three tokenizer keys; `tok_ok` says whether the engine can tokenise.
    bool tok_ok = false;
    std::vector<int64_t> tok_off, tok_len, tok_slots, mrg_off, mrg_len, mrg_slots;
    std::vector<int64_t> pairs, tok_type, attr, order, eog;
    std::vector<int64_t> stats;
    ~reader_meta() {
        if (hdr && hdr != MAP_FAILED) munmap(hdr, (size_t) len);
        if (fd >= 0) close(fd);
    }
};

// Positive witness: incremented by every answer the reader gives, so a test can
// tell "Goose decided" from "Goose was linked but the native path ran".
extern "C" int llama_goose_queries = 0;

std::mutex g_mu;
std::map<const void *, std::unique_ptr<reader_meta>> g_metas;
// The registration the ctx-less vendor entry point answers from; one file is mapped at a time.
const struct gguf_context * g_current = nullptr;
bool g_inited = false;

const reader_meta * meta_of(const struct gguf_context * ctx) {
    auto it = g_metas.find(ctx);
    return (it == g_metas.end() || !it->second->ok) ? nullptr : it->second.get();
}

gsl_u8x  u8x(const void * p, int64_t n) { return gsl_u8x{ const_cast<uint8_t *>(static_cast<const uint8_t *>(p)), n }; }
gsl_i64x i64x(const std::vector<int64_t> & v) { return gsl_i64x{ const_cast<int64_t *>(v.data()), (int64_t) v.size() }; }

} // namespace

// Maps the header window once per gguf context and parses it with Goose. Called
// by the loader right after the context is created.
extern "C" void llama_goose_register(const struct gguf_context * ctx, const char * path) {
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_inited) { fg950_goose_init(0, nullptr); g_inited = true; }
    auto m = std::make_unique<reader_meta>();
    struct stat st{};
    m->fd = open(path, O_RDONLY);
    if (m->fd >= 0 && fstat(m->fd, &st) == 0 && st.st_size > 0) {
        m->len = st.st_size < HDR_MAX ? (int64_t) st.st_size : HDR_MAX;
        m->hdr = mmap(nullptr, (size_t) m->len, PROT_READ, MAP_PRIVATE, m->fd, 0);
        if (m->hdr != MAP_FAILED) {
            m->state.assign(8, 0);
            m->kv.assign(KV_RECORDS * 8, 0);
            m->tens.assign(TENSOR_RECORDS * 12, 0);
            m->kvsl.assign(KV_SLOTS, 0);
            m->tensl.assign(TENSOR_SLOTS, 0);
            m->ok = gr_parse(u8x(m->hdr, m->len), st.st_size, i64x(m->state), i64x(m->kv),
                             i64x(m->tens), i64x(m->kvsl), i64x(m->tensl)) == 0;
            if (m->ok && std::getenv("GGML_GOOSE_TOKENIZER") != nullptr &&
                std::strcmp(std::getenv("GGML_GOOSE_TOKENIZER"), "0") == 0) {
                // Disabled by flag: keep the mapping for the reader, skip the tables so
                // the rollback really costs nothing beyond the header window.
                m->tok_ok = false;
            } else if (m->ok) {
                // btl_load sizes nothing itself: give it the array counts first.
                const int64_t ti = gr_find_kv(u8x(m->hdr, m->len), i64x(m->state), i64x(m->kv),
                                              i64x(m->kvsl), u8x("tokenizer.ggml.tokens", 21));
                const int64_t mi = gr_find_kv(u8x(m->hdr, m->len), i64x(m->state), i64x(m->kv),
                                              i64x(m->kvsl), u8x("tokenizer.ggml.merges", 21));
                if (ti >= 0 && mi >= 0) {
                    const int64_t n_tok = gr_arr_n(i64x(m->state), i64x(m->kv), ti);
                    const int64_t n_mrg = gr_arr_n(i64x(m->state), i64x(m->kv), mi);
                    if (n_tok > 0 && n_mrg >= 0) {
                        // Hash tables need a power-of-two capacity above twice the entries.
                        int64_t tcap = 16, mcap = 16;
                        while (tcap < 2 * n_tok + 2) tcap <<= 1;
                        while (mcap < 2 * n_mrg + 2) mcap <<= 1;
                        m->tok_off.assign(n_tok, 0);
                        m->tok_len.assign(n_tok, 0);
                        m->tok_slots.assign(tcap, 0);
                        m->mrg_off.assign(n_mrg > 0 ? n_mrg : 1, 0);
                        m->mrg_len.assign(n_mrg > 0 ? n_mrg : 1, 0);
                        m->mrg_slots.assign(mcap, 0);
                        m->pairs.assign(2 * (n_tok > n_mrg ? n_tok : n_mrg), 0);
                        m->tok_type.assign(n_tok, 0);
                        m->attr.assign(n_tok, 0);
                        m->order.assign(n_tok, 0);
                        m->eog.assign(n_tok, 0);
                        m->stats.assign(8, 0);
                        m->tok_ok = btl_load(u8x(m->hdr, m->len), i64x(m->state), i64x(m->kv),
                                             i64x(m->kvsl), i64x(m->tok_off), i64x(m->tok_len),
                                             i64x(m->tok_slots), i64x(m->mrg_off), i64x(m->mrg_len),
                                             i64x(m->mrg_slots), i64x(m->pairs), i64x(m->tok_type),
                                             i64x(m->attr), i64x(m->order), i64x(m->eog),
                                             -1, -1, -1, i64x(m->stats)) == 0;
                    }
                }
            }
        }
    }
    g_metas[ctx] = std::move(m);
    g_current = ctx;
}

extern "C" void llama_goose_unregister(const struct gguf_context * ctx) {
    std::lock_guard<std::mutex> lock(g_mu);
    g_metas.erase(ctx);
    if (g_current == ctx) { g_current = nullptr; }
}

extern "C" bool llama_goose_check(const struct gguf_context * ctx) {
    std::lock_guard<std::mutex> lock(g_mu);
    const reader_meta * m = meta_of(ctx);
    if (!m) {
        std::fprintf(stderr, "FG_GGUF_PARSE refused by the Goose reader\n");
        return false;
    }

    const auto data = u8x(m->hdr, m->len);
    const auto state = i64x(m->state);
    const auto kv = i64x(m->kv);
    const auto tensors = i64x(m->tens);
    const int64_t nk = gguf_get_n_kv(ctx), nt = gguf_get_n_tensors(ctx);
    const size_t data_offset = gguf_get_data_offset(ctx);
    const auto payload_matches = [&](int64_t offset, int64_t length, const void * native, size_t size) {
        return offset >= 0 && offset <= data.len && length >= 0 && length <= data.len - offset
            && static_cast<size_t>(length) == size
            && (size == 0 || (native && std::memcmp(data.data + offset, native, size) == 0));
    };
    // Widths describe the native GGUF wire types on little-endian hosts.
    static constexpr size_t widths[GGUF_TYPE_COUNT] = {1, 1, 2, 2, 4, 4, 4, 1, 0, 0, 8, 8, 8};
    int64_t disagreements = (m->state[1] != gguf_get_version(ctx)) +
        (m->state[2] != nk) + (m->state[3] != nt) +
        (static_cast<size_t>(m->state[4]) != gguf_get_alignment(ctx)) +
        (static_cast<size_t>(m->state[5]) != data_offset);
    std::vector<int64_t> spans;
    for (int64_t i = 0; i < nk; ++i) {
        const char * name = gguf_get_key(ctx, i);
        const auto type = gguf_get_kv_type(ctx, i);
        int64_t span[4]{};
        if (gr_find_kv(data, state, kv, i64x(m->kvsl), u8x(name, std::strlen(name))) != i ||
                gr_val_span(state, kv, i, {span, 4}) != 0 || span[0] != type) {
            ++disagreements;
            continue;
        }
        if (type == GGUF_TYPE_STRING) {
            const char * str = gguf_get_val_str(ctx, i);
            disagreements += !payload_matches(span[1], span[2], str, std::strlen(str));
        } else if (type == GGUF_TYPE_ARRAY) {
            const auto element = gguf_get_arr_type(ctx, i);
            const size_t count = gguf_get_arr_n(ctx, i);
            const int64_t goose_count = gr_arr_n(state, kv, i);
            if (span[3] != element || goose_count < 0 || static_cast<size_t>(goose_count) != count) {
                ++disagreements;
            } else if (element == GGUF_TYPE_STRING) {
                spans.resize(count * 2);
                if (gr_string_array_index(data, state, kv, i, i64x(spans)) != goose_count) {
                    ++disagreements;
                    continue;
                }
                for (size_t j = 0; j < count; ++j) {
                    const char * str = gguf_get_arr_str(ctx, i, j);
                    disagreements += !payload_matches(spans[j * 2], spans[j * 2 + 1], str, std::strlen(str));
                }
            } else {
                disagreements += !payload_matches(span[1], span[2], gguf_get_arr_data(ctx, i), count * widths[element]);
            }
        } else {
            disagreements += !payload_matches(span[1], span[2], gguf_get_val_data(ctx, i), widths[type]);
        }
    }
    for (int64_t i = 0; i < nt; ++i) {
        const char * name = gguf_get_tensor_name(ctx, i);
        int64_t info[9]{};
        if (gr_find_tensor(data, state, tensors, i64x(m->tensl), u8x(name, std::strlen(name))) != i ||
                gr_tensor_info(state, tensors, i, {info, 9}) != 0) {
            ++disagreements;
            continue;
        }
        const auto * extents = gguf_get_tensor_ne(ctx, i);
        disagreements += info[0] != gguf_get_tensor_type(ctx, i)
            || info[7] < 0 || static_cast<size_t>(info[7]) != gguf_get_tensor_size(ctx, i)
            || info[8] < 0 || static_cast<size_t>(info[8]) != data_offset + gguf_get_tensor_offset(ctx, i)
            || std::memcmp(info + 2, extents, 4 * sizeof(int64_t)) != 0;
    }
    std::fprintf(stderr, "FG_GGUF_PARSE nk=%lld nt=%lld align=%lld off=%lld disagreements=%lld\n",
                 (long long) m->state[2], (long long) m->state[3], (long long) m->state[4],
                 (long long) m->state[5], (long long) disagreements);
    return disagreements == 0;
}

// One element of a string array, asked by key index alone.
//
// vendor/flint/src/llama-impl.cpp is compiled from the vendor tree (there is no staged copy) and
// calls this four-argument form; the staged carrier serves the same question through
// llama_goose_arr_str(ctx, index, element). The mapping belongs to one file at a time, which is
// the same contract the vendor lineage's fg_port store has, so the answer comes from the current
// registration. See docs/goose-migration/CARRIER-DIVERGENCE-20261119.md.
extern "C" bool fg_port_array_str(int64_t key_index, int64_t element, const uint8_t ** ptr, int64_t * len) {
    std::lock_guard<std::mutex> lock(g_mu);
    if (!g_current) { return false; }
    const reader_meta * m = meta_of(g_current);
    if (!m || !m->ok) { return false; }
    int64_t out[2] = { 0, 0 };
    if (gr_arr_str_span(gsl_u8x{ (uint8_t *) m->hdr, m->len }, i64x(m->state), i64x(m->kv),
                        key_index, element, gsl_i64x{ out, 2 }) != 0) {
        return false;
    }
    if (out[0] < 0 || out[1] < 0 || (size_t) (out[0] + out[1]) > (size_t) m->len) { return false; }
    *ptr = (const uint8_t *) m->hdr + out[0];
    *len = out[1];
    return true;
}

extern "C" int llama_goose_ready(const struct gguf_context * ctx) {
    std::lock_guard<std::mutex> lock(g_mu);
    return meta_of(ctx) ? 1 : 0;
}

extern "C" int64_t llama_goose_find_kv(const struct gguf_context * ctx, const char * key) {
    std::lock_guard<std::mutex> lock(g_mu);
    llama_goose_queries++;
    const reader_meta * m = meta_of(ctx);
    if (!m) return -1;
    return gr_find_kv(u8x(m->hdr, m->len), i64x(m->state), i64x(m->kv), i64x(m->kvsl),
                      u8x(key, (int64_t) std::strlen(key)));
}

extern "C" int64_t llama_goose_tensor_index(const struct gguf_context * ctx, const char * name) {
    std::lock_guard<std::mutex> lock(g_mu);
    llama_goose_queries++;
    const reader_meta * m = meta_of(ctx);
    if (!m) return -1;
    return gr_find_tensor(u8x(m->hdr, m->len), i64x(m->state), i64x(m->tens), i64x(m->tensl),
                          u8x(name, (int64_t) std::strlen(name)));
}

extern "C" int64_t llama_goose_kv_type(const struct gguf_context * ctx, int64_t index) {
    std::lock_guard<std::mutex> lock(g_mu);
    llama_goose_queries++;
    const reader_meta * m = meta_of(ctx);
    return m ? gr_kv_type(i64x(m->state), i64x(m->kv), index) : -1;
}

extern "C" int64_t llama_goose_arr_type(const struct gguf_context * ctx, int64_t index) {
    std::lock_guard<std::mutex> lock(g_mu);
    llama_goose_queries++;
    const reader_meta * m = meta_of(ctx);
    return m ? gr_arr_type(i64x(m->state), i64x(m->kv), index) : -1;
}

extern "C" int64_t llama_goose_arr_n(const struct gguf_context * ctx, int64_t index) {
    std::lock_guard<std::mutex> lock(g_mu);
    llama_goose_queries++;
    const reader_meta * m = meta_of(ctx);
    return m ? gr_arr_n(i64x(m->state), i64x(m->kv), index) : -1;
}

// Payload of a non-string array: the reader answers where it is, this returns
// the pointer and the byte count. The bytes stay owned by the mapping.
extern "C" const void * llama_goose_arr_data(const struct gguf_context * ctx, int64_t index, int64_t * bytes) {
    std::lock_guard<std::mutex> lock(g_mu);
    llama_goose_queries++;
    const reader_meta * m = meta_of(ctx);
    if (!m) return nullptr;
    int64_t out[4] = {0, 0, 0, 0};
    if (gr_val_span(i64x(m->state), i64x(m->kv), index, gsl_i64x{ out, 4 }) != 0) return nullptr;
    if (out[0] != 9 || out[1] < 0 || out[1] > m->len) return nullptr;
    if (bytes) *bytes = out[2];
    return (const uint8_t *) m->hdr + out[1];
}

// One element of a string array as a C string. GGUF has no terminator on the
// wire, so the reader lays the whole array out once with the terminators it can
// write from the known lengths; this function only holds that table.
extern "C" const char * llama_goose_arr_str(const struct gguf_context * ctx, int64_t index, int64_t element) {
    std::lock_guard<std::mutex> lock(g_mu);
    llama_goose_queries++;
    const reader_meta * m = meta_of(ctx);
    if (!m) return nullptr;
    auto it = m->arr_str.find(index);
    if (it == m->arr_str.end()) {
        const int64_t n = gr_arr_n(i64x(m->state), i64x(m->kv), index);
        if (n < 0) return nullptr;
        reader_meta::arr_text t;
        t.offs.assign((size_t) n, 0);
        int64_t rc = gr_arr_str_table(u8x(m->hdr, m->len), i64x(m->state), i64x(m->kv), index,
                                      gsl_u8x{ nullptr, 0 }, gsl_i64x{ t.offs.data(), (int64_t) t.offs.size() });
        if (rc < 0) {
            const int64_t need = -rc - 1;
            if (need <= 0) return nullptr;
            t.text.resize((size_t) need);
            rc = gr_arr_str_table(u8x(m->hdr, m->len), i64x(m->state), i64x(m->kv), index,
                                  u8x(t.text.data(), (int64_t) t.text.size()),
                                  gsl_i64x{ t.offs.data(), (int64_t) t.offs.size() });
        }
        if (rc < 0) return nullptr;
        it = m->arr_str.emplace(index, std::move(t)).first;
    }
    if (element < 0 || element >= (int64_t) it->second.offs.size()) return nullptr;
    return it->second.text.data() + it->second.offs[(size_t) element];
}

// The context that the vocabulary path can use. The loader registers one file per
// model and the tokenizer belongs to that model, so a single lookup is enough and
// the caller never has to thread the gguf pointer through the vocab object.
extern "C" int64_t llama_goose_tokenizer_ctx(void) {
    std::lock_guard<std::mutex> lock(g_mu);
    for (const auto & kv : g_metas) {
        if (kv.second && kv.second->ok && kv.second->tok_ok) {
            return (int64_t) (uintptr_t) kv.first;
        }
    }
    return 0;
}

// One raw text fragment through the BPE engine. Returns the id count, or -1 when
// the file has no usable tokenizer table or the engine refuses the input.
extern "C" int64_t llama_goose_tokenize(const struct gguf_context * ctx,
                                        const char * text, int64_t len,
                                        int32_t * out, int64_t out_cap) {
    std::lock_guard<std::mutex> lock(g_mu);
    const reader_meta * m = meta_of(ctx);
    if (!m || !m->tok_ok || len < 0 || out_cap < 0) return -1;
    // Scratch is sized from the fragment, exactly as the gated EXP920 corpus did.
    const int64_t n = len + 1;
    std::vector<int64_t> cpts(n, 0), seg(4 * n, 0), sym(24 * n, 0), heap(15 * n + 40, 0);
    std::vector<uint8_t> enc(8 * n, 0);
    std::vector<int64_t> ids(out_cap > 0 ? out_cap : 1, 0), hc(2, 0), info(8, 0);
    const int64_t rc = bt_bpe(u8x(text, len), u8x(m->hdr, m->len),
                              i64x(m->tok_off), i64x(m->tok_len), i64x(m->tok_slots),
                              i64x(m->mrg_off), i64x(m->mrg_len), i64x(m->mrg_slots),
                              i64x(ids), 0, i64x(cpts), i64x(seg), u8x(enc.data(), (int64_t) enc.size()),
                              i64x(sym), i64x(heap), i64x(hc), i64x(info));
    if (rc < 0 || rc > out_cap) return -1;
    for (int64_t i = 0; i < rc; i++) out[i] = ids[(size_t) i];
    llama_goose_queries++;
    return rc;
}

// One token as its piece bytes. The engine owns the attribute pass, the GPT-2
// byte decode, the lstrip rule and the negative "needs this much room" return.
extern "C" int64_t llama_goose_token_to_piece(const struct gguf_context * ctx, int64_t token,
                                              char * buf, int64_t length, int64_t lstrip,
                                              int64_t special, int64_t escape_ws) {
    std::lock_guard<std::mutex> lock(g_mu);
    const reader_meta * m = meta_of(ctx);
    if (!m || !m->tok_ok) return -1;
    const int64_t rc = bt_token_to_piece(u8x(m->hdr, m->len), i64x(m->tok_off), i64x(m->tok_len),
                                         i64x(m->attr), token, u8x(buf, length), 0,
                                         lstrip, special, escape_ws);
    if (rc >= 0 || rc < -1) llama_goose_queries++;
    return rc;
}

// The mapped window and the parse tables, for a caller that runs Goose
// functions itself. Pure accessor: no layout decision is made here.
extern "C" int llama_goose_reader(const struct gguf_context * ctx, const uint8_t ** hdr, int64_t * len,
                                  const int64_t ** state, const int64_t ** kv, const int64_t ** kvsl,
                                  int64_t * kv_count, int64_t * kvsl_count) {
    std::lock_guard<std::mutex> lock(g_mu);
    const reader_meta * m = meta_of(ctx);
    if (!m) return 0;
    *hdr = (const uint8_t *) m->hdr;
    *len = m->len;
    *state = m->state.data();
    *kv = m->kv.data();
    *kvsl = m->kvsl.data();
    *kv_count = (int64_t) m->kv.size();
    *kvsl_count = (int64_t) m->kvsl.size();
    return 1;
}
