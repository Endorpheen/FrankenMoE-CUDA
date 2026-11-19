#include "llama-model-loader.h"

#include "ggml-alloc.h"
#include "ggml.h"
#include "gguf.h"
#include "llama-hparams.h"
#include "llama.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <type_traits>
#include <dlfcn.h>
#include <fstream>
#include <future>
#include <regex>
#include <unordered_set>

namespace {
// Compares the ported reader's view of a GGUF header with the native one. The native values are what the
// loader uses either way; this only counts disagreements, behind FG_GGUF_DIAG.
struct fg_sl_i64 { int64_t * data; int64_t len; };
struct fg_sl_u8  { uint8_t  * data; int64_t len; };
typedef int64_t (*fg_gr_parse_fn)(fg_sl_u8, int64_t, fg_sl_i64, fg_sl_i64, fg_sl_i64, fg_sl_i64, fg_sl_i64);
typedef int64_t (*fg_gr_find_fn)(fg_sl_u8, fg_sl_i64, fg_sl_i64, fg_sl_i64, fg_sl_u8);
typedef int64_t (*fg_gr_meta_int_fn)(fg_sl_u8, fg_sl_i64, fg_sl_i64, int64_t, fg_sl_i64);
typedef int64_t (*fg_gr_tinfo_fn)(fg_sl_i64, fg_sl_i64, int64_t, fg_sl_i64);
typedef int64_t (*fg_gr_span_fn)(fg_sl_i64, fg_sl_i64, int64_t, fg_sl_i64);
typedef int64_t (*fg_gr_arrstr_fn)(fg_sl_u8, fg_sl_i64, fg_sl_i64, int64_t, int64_t, fg_sl_i64);
typedef int64_t (*fg_gr_arrint_fn)(fg_sl_u8, fg_sl_i64, fg_sl_i64, int64_t, int64_t, fg_sl_i64);

// The window and the port records, kept alive after the check so a key read can be compared at the moment
// of use. One model per process, which is what the loader has anyway.
struct fg_port_store {
    std::vector<uint8_t> data;
    std::vector<int64_t> state, kv, tn, slots;
    bool ready = false;
};
static fg_port_store fg_port;

// Returns the number of disagreements; the caller decides whether that is fatal.
// Tensor lookup and type through the port, for the metadata-only path that has no file list.
extern "C" int64_t fg_port_find_tensor(const char * name, int64_t len) {
    static auto fn = []() -> fg_gr_find_fn {
        void * h = dlopen("libllama.so.0", RTLD_LAZY | RTLD_NOLOAD);
        return h ? reinterpret_cast<fg_gr_find_fn>(dlsym(h, "gr_find_tensor")) : nullptr;
    }();
    if (!fn || !fg_port.ready) { return -1; }
    fg_sl_u8  s_data{ fg_port.data.data(), (int64_t) fg_port.data.size() };
    fg_sl_i64 s_state{ fg_port.state.data(), 8 }, s_t{ fg_port.tn.data(), (int64_t) fg_port.tn.size() };
    fg_sl_i64 s_slots{ fg_port.slots.data(), (int64_t) fg_port.slots.size() };
    fg_sl_u8  key{ reinterpret_cast<uint8_t *>(const_cast<char *>(name)), len };
    return fn(s_data, s_state, s_t, s_slots, key);
}

extern "C" bool fg_port_tensor_type(int64_t index, int64_t * out) {
    static auto fn = []() -> fg_gr_tinfo_fn {
        void * h = dlopen("libllama.so.0", RTLD_LAZY | RTLD_NOLOAD);
        return h ? reinterpret_cast<fg_gr_tinfo_fn>(dlsym(h, "gr_tensor_info")) : nullptr;
    }();
    if (!fn || !fg_port.ready) { return false; }
    fg_sl_i64 s_state{ fg_port.state.data(), 8 }, s_t{ fg_port.tn.data(), (int64_t) fg_port.tn.size() };
    int64_t info[9] = {0};
    fg_sl_i64 so{ info, 9 };
    if (fn(s_state, s_t, index, so) != 0) { return false; }
    *out = info[0];
    return true;
}

// Element of an integer array, for the token-type table the vocabulary reads with gguf_get_arr_data.
extern "C" bool fg_port_array_int(int64_t key_index, int64_t element, int64_t * out) {
    static auto fn = []() -> fg_gr_arrint_fn {
        void * h = dlopen("libllama.so.0", RTLD_LAZY | RTLD_NOLOAD);
        return h ? reinterpret_cast<fg_gr_arrint_fn>(dlsym(h, "gr_array_int")) : nullptr;
    }();
    if (!fn || !fg_port.ready) { return false; }
    fg_sl_u8  s_data{ fg_port.data.data(), (int64_t) fg_port.data.size() };
    fg_sl_i64 s_state{ fg_port.state.data(), 8 }, s_kv{ fg_port.kv.data(), (int64_t) fg_port.kv.size() };
    int64_t v = 0;
    fg_sl_i64 so{ &v, 1 };
    if (fn(s_data, s_state, s_kv, key_index, element, so) != 0) { return false; }
    *out = v;
    return true;
}

// Element of a string array, for consumers that iterate the arrays themselves (llama-vocab builds the
// token list that way). C linkage so the vocabulary file can declare it without the loader's headers.
extern "C" bool fg_port_array_str(int64_t key_index, int64_t element, const uint8_t ** ptr, int64_t * len) {
    static auto fn = []() -> fg_gr_arrstr_fn {
        void * h = dlopen("libllama.so.0", RTLD_LAZY | RTLD_NOLOAD);
        return h ? reinterpret_cast<fg_gr_arrstr_fn>(dlsym(h, "gr_arr_str_span")) : nullptr;
    }();
    if (!fn || !fg_port.ready) { return false; }
    fg_sl_u8  s_data{ fg_port.data.data(), (int64_t) fg_port.data.size() };
    fg_sl_i64 s_state{ fg_port.state.data(), 8 }, s_kv{ fg_port.kv.data(), (int64_t) fg_port.kv.size() };
    int64_t out[2] = {0};
    fg_sl_i64 so{ out, 2 };
    if (fn(s_data, s_state, s_kv, key_index, element, so) != 0) { return false; }
    if (out[0] < 0 || out[1] < 0 || (size_t) (out[0] + out[1]) > fg_port.data.size()) { return false; }
    *ptr = fg_port.data.data() + out[0];
    *len = out[1];
    return true;
}

// Read one key through the port. Returns false when the port cannot answer, which the caller must treat as
// "no comparison available" rather than as agreement.
static bool fg_port_int(const int64_t k, int64_t & out) {
    static auto fn = []() -> fg_gr_meta_int_fn {
        void * h = dlopen("libllama.so.0", RTLD_LAZY | RTLD_NOLOAD);
        return h ? reinterpret_cast<fg_gr_meta_int_fn>(dlsym(h, "gr_meta_int")) : nullptr;
    }();
    if (!fn || !fg_port.ready) { return false; }
    fg_sl_u8  s_data{ fg_port.data.data(), (int64_t) fg_port.data.size() };
    fg_sl_i64 s_state{ fg_port.state.data(), 8 }, s_kv{ fg_port.kv.data(), (int64_t) fg_port.kv.size() };
    int64_t v = 0;
    fg_sl_i64 so{ &v, 1 };
    if (fn(s_data, s_state, s_kv, k, so) != 0) { return false; }
    out = v;
    return true;
}

static bool fg_port_str(const int64_t k, const uint8_t *& ptr, int64_t & len) {
    static auto fn = []() -> fg_gr_span_fn {
        void * h = dlopen("libllama.so.0", RTLD_LAZY | RTLD_NOLOAD);
        return h ? reinterpret_cast<fg_gr_span_fn>(dlsym(h, "gr_val_span")) : nullptr;
    }();
    if (!fn || !fg_port.ready) { return false; }
    fg_sl_i64 s_state{ fg_port.state.data(), 8 }, s_kv{ fg_port.kv.data(), (int64_t) fg_port.kv.size() };
    int64_t out[4] = {0};
    fg_sl_i64 so{ out, 4 };
    if (fn(s_state, s_kv, k, so) != 0) { return false; }
    if (out[1] < 0 || out[2] < 0 || (size_t) (out[1] + out[2]) > fg_port.data.size()) { return false; }
    ptr = fg_port.data.data() + out[1];
    len = out[2];
    return true;
}

// The BPE table for the ported encoder: one owner, as with the detokeniser. btl_load reads the KV arrays
// through the port's own reader and fills every table, so the host only supplies correctly sized arrays -
// sizes come from the native reader's own counts, never from a guess (EXP-2026-11-19-1378).
struct fg_bpe_store {
    std::vector<int64_t> tok_off, tok_len, tok_slots, mrg_off, mrg_len, mrg_slots;
    std::vector<int64_t> pairs, tok_type, attr, order, eog, stats;
    int64_t n_special = 0;
    bool ready = false;
};
static fg_bpe_store fg_bpe;

typedef int64_t (*fg_bt_bpe_fn)(fg_sl_u8, fg_sl_u8, fg_sl_i64, fg_sl_i64, fg_sl_i64,
                                fg_sl_i64, fg_sl_i64, fg_sl_i64, fg_sl_i64, int64_t,
                                fg_sl_i64, fg_sl_i64, fg_sl_u8, fg_sl_i64, fg_sl_i64,
                                fg_sl_i64, fg_sl_i64);

static fg_bt_bpe_fn fg_bpe_encode_resolve() {
    static fg_bt_bpe_fn fn = []() -> fg_bt_bpe_fn {
        void * h = dlopen("libllama.so.0", RTLD_LAZY | RTLD_NOLOAD);
        return h ? reinterpret_cast<fg_bt_bpe_fn>(dlsym(h, "bt_bpe")) : nullptr;
    }();
    return fn;
}

// Called once from the loader, after the port store is ready. Returns false when the engine is absent or
// refuses the vocabulary, in which case the native encoder stays the only one.
typedef int64_t (*fg_bt_build_fn)(fg_sl_u8, fg_sl_i64, fg_sl_i64, fg_sl_i64,
                                  fg_sl_i64, fg_sl_i64, fg_sl_i64, fg_sl_i64);

static bool fg_bpe_init(int64_t tok_key, int64_t mrg_key, int64_t n_tok, int64_t n_mrg,
                        const int32_t * tok_type) {
    // bt_build takes the offsets as input, so they are taken from the port's own reader: the pointer the
    // accessor returns is inside the blob the door will index. btl_load is the same work but is not
    // exported by the shipped unit (measured: 0 dynamic symbols), which is why this route exists.
    static fg_bt_build_fn build = []() -> fg_bt_build_fn {
        void * h = dlopen("libllama.so.0", RTLD_LAZY | RTLD_NOLOAD);
        return h ? reinterpret_cast<fg_bt_build_fn>(dlsym(h, "bt_build")) : nullptr;
    }();
    if (getenv("FG_TK_BPE_DIAG")) {
        fprintf(stderr, "FG_TK_BPE_ENTRY build=%d port=%d n_tok=%lld n_mrg=%lld\n",
                build != nullptr, (int) fg_port.ready, (long long) n_tok, (long long) n_mrg);
    }
    if (!build || !fg_port.ready || n_tok <= 0) { return false; }
    if (fg_bpe.ready) { return true; }
    const int64_t nm = n_mrg > 0 ? n_mrg : 1;
    int64_t tcap = 16, mcap = 16;
    while (tcap < 2 * n_tok + 2) { tcap <<= 1; }
    while (mcap < 2 * nm + 2) { mcap <<= 1; }
    const int64_t maxn = n_tok > nm ? n_tok : nm;
    fg_bpe.tok_off.assign(n_tok, 0);   fg_bpe.tok_len.assign(n_tok, 0);
    fg_bpe.mrg_off.assign(nm, 0);      fg_bpe.mrg_len.assign(nm, 0);
    fg_bpe.tok_slots.assign(tcap, 0);  fg_bpe.mrg_slots.assign(mcap, 0);
    fg_bpe.pairs.assign(2 * maxn, 0);
    fg_bpe.tok_type.assign(n_tok, 0);  fg_bpe.attr.assign(n_tok, 0);
    fg_bpe.order.assign(n_tok, 0);     fg_bpe.eog.assign(n_tok, 0);
    fg_bpe.stats.assign(8, 0);

    // Offsets and lengths of both arrays in one pass each, straight into the file's own window: the door that
    // copies the text produces a blob a caller cannot index against `fg_port.data`, and the per-element door is
    // quadratic (EXP-2026-11-19-1389, -1390).
    typedef int64_t (*fg_arr_offs_fn)(fg_sl_u8, fg_sl_i64, fg_sl_i64, int64_t, fg_sl_i64, fg_sl_i64);
    static fg_arr_offs_fn offs_fn = []() -> fg_arr_offs_fn {
        void * h = dlopen("libllama.so.0", RTLD_LAZY | RTLD_NOLOAD);
        return h ? reinterpret_cast<fg_arr_offs_fn>(dlsym(h, "gr_arr_str_offsets")) : nullptr;
    }();
    if (!offs_fn) { return false; }

    auto fill = [&](int64_t key, int64_t n, std::vector<int64_t> & offs, std::vector<int64_t> & lens) -> bool {
        const int64_t got = offs_fn(fg_sl_u8{ fg_port.data.data(), (int64_t) fg_port.data.size() },
                                    fg_sl_i64{ fg_port.state.data(), 8 },
                                    fg_sl_i64{ fg_port.kv.data(), (int64_t) fg_port.kv.size() },
                                    key,
                                    fg_sl_i64{ offs.data(), n },
                                    fg_sl_i64{ lens.data(), n });
        return got == n;
    };

    if (!fill(tok_key, n_tok, fg_bpe.tok_off, fg_bpe.tok_len)) { return false; }
    if (n_mrg > 0 && !fill(mrg_key, n_mrg, fg_bpe.mrg_off, fg_bpe.mrg_len)) { return false; }
    fg_sl_u8  s_gguf { fg_port.data.data(), (int64_t) fg_port.data.size() };
    const int64_t rc = build(s_gguf,
                             fg_sl_i64{ fg_bpe.tok_off.data(), n_tok },
                             fg_sl_i64{ fg_bpe.tok_len.data(), n_tok },
                             fg_sl_i64{ fg_bpe.tok_slots.data(), tcap },
                             fg_sl_i64{ fg_bpe.mrg_off.data(), nm },
                             fg_sl_i64{ fg_bpe.mrg_len.data(), nm },
                             fg_sl_i64{ fg_bpe.mrg_slots.data(), mcap },
                             fg_sl_i64{ fg_bpe.stats.data(), 8 });
    if (rc != 0) {
        if (getenv("FG_TK_BPE_DIAG")) { fprintf(stderr, "FG_TK_BPE_LOAD refused rc=%lld\n", (long long) rc); }
        return false;
    }
    // bt_build fills the slot tables but not the attributes; the scan needs them, and they come from the token
    // types the reader already parsed. bt_attrs derives the bits, so the rule stays in Goose.
    if (tok_type) {
        for (int64_t i = 0; i < n_tok; ++i) { fg_bpe.tok_type[(size_t) i] = (int64_t) tok_type[i]; }
        typedef int64_t (*fg_attrs_fn)(fg_sl_u8, fg_sl_i64, fg_sl_i64, fg_sl_i64, fg_sl_i64);
        static fg_attrs_fn attrs_fn = []() -> fg_attrs_fn {
            void * h = dlopen("libllama.so.0", RTLD_LAZY | RTLD_NOLOAD);
            return h ? reinterpret_cast<fg_attrs_fn>(dlsym(h, "bt_attrs")) : nullptr;
        }();
        if (attrs_fn) {
            attrs_fn(fg_sl_u8{ fg_port.data.data(), (int64_t) fg_port.data.size() },
                     fg_sl_i64{ fg_bpe.tok_off.data(), n_tok },
                     fg_sl_i64{ fg_bpe.tok_len.data(), n_tok },
                     fg_sl_i64{ fg_bpe.tok_type.data(), n_tok },
                     fg_sl_i64{ fg_bpe.attr.data(), n_tok });
        }
    }

    // the special count the scan needs: the order door fills the ids, longest text first
    {
        typedef int64_t (*fg_order_fn)(fg_sl_u8, fg_sl_i64, fg_sl_i64, fg_sl_i64, fg_sl_i64);
        static fg_order_fn order_fn = []() -> fg_order_fn {
            void * h = dlopen("libllama.so.0", RTLD_LAZY | RTLD_NOLOAD);
            return h ? reinterpret_cast<fg_order_fn>(dlsym(h, "bt_special_order")) : nullptr;
        }();
        fg_bpe.n_special = order_fn ? order_fn(fg_sl_u8{ fg_port.data.data(), (int64_t) fg_port.data.size() },
                                               fg_sl_i64{ fg_bpe.tok_off.data(), n_tok },
                                               fg_sl_i64{ fg_bpe.tok_len.data(), n_tok },
                                               fg_sl_i64{ fg_bpe.attr.data(), n_tok },
                                               fg_sl_i64{ fg_bpe.order.data(), n_tok }) : 0;
        if (fg_bpe.n_special < 0) { fg_bpe.n_special = 0; }
    }
    fg_bpe.ready = true;
    if (getenv("FG_TK_BPE_DIAG")) {
        fprintf(stderr, "FG_TK_BPE_LOAD ok n_tok=%lld n_mrg=%lld tcap=%lld mcap=%lld\n",
                (long long) n_tok, (long long) n_mrg, (long long) tcap, (long long) mcap);
    }
    return true;
}

// Encodes through the port. Returns the id count, or -1 when the engine cannot answer.
extern "C" int64_t fg_bpe_encode(const char * text, int64_t n, int64_t * ids, int64_t cap) {
    const fg_bt_bpe_fn fn = fg_bpe_encode_resolve();
    if (!fn || !fg_bpe.ready || !fg_port.ready || n < 0 || n > (int64_t) 1 << 24) { return -1; }
    const int64_t m = n > 0 ? n : 1;
    static thread_local std::vector<int64_t> cpts, seg, sym, heap, hc, info, ids_buf;
    static thread_local std::vector<uint8_t> enc;
    cpts.assign(m + 1, 0); seg.assign(4 * (m + 1), 0); enc.assign(8 * (m + 1), 0);
    sym.assign(24 * (m + 1), 0); heap.assign(15 * (m + 1) + 40, 0); hc.assign(2, 0); info.assign(8, 0);
    if (ids_buf.size() < (size_t) cap + 8) { ids_buf.resize((size_t) cap + 8); }
    const bool fg_diag = getenv("FG_TK_BPE_DIAG") != nullptr;
    if (fg_diag) { fprintf(stderr, "FG_TK_BPE_CALL n=%lld cap=%lld\n", (long long) n, (long long) cap); }

    // Special tokens decide part of the answer before any merge is looked up: the native tokenizer matches them
    // first, and the port's bt_bpe alone returns seven pieces for <|im_start|>system where the native returns two
    // (EXP-2026-11-19-1390). The scan is a Goose door; the gaps go through bt_bpe and the ids are spliced here.
    typedef int64_t (*fg_hits_fn)(fg_sl_u8, fg_sl_u8, fg_sl_i64, fg_sl_i64, fg_sl_i64, fg_sl_i64, int64_t, fg_sl_i64);
    static fg_hits_fn hits_fn = []() -> fg_hits_fn {
        void * h = dlopen("libllama.so.0", RTLD_LAZY | RTLD_NOLOAD);
        return h ? reinterpret_cast<fg_hits_fn>(dlsym(h, "bt_special_hits")) : nullptr;
    }();
    static std::vector<int64_t> hits;      // (start, end, id) triples
    static std::vector<int64_t> plain_ids; // ids of one plain segment
    int64_t n_hits = 0;
    if (hits_fn) {
        if (hits.size() < (size_t) (3 * ((size_t) n + 1))) { hits.assign(3 * ((size_t) n + 1), 0); }
        n_hits = hits_fn(fg_sl_u8{ (uint8_t *) text, n },
                         fg_sl_u8{ fg_port.data.data(), (int64_t) fg_port.data.size() },
                         fg_sl_i64{ fg_bpe.tok_off.data(), (int64_t) fg_bpe.tok_off.size() },
                         fg_sl_i64{ fg_bpe.tok_len.data(), (int64_t) fg_bpe.tok_len.size() },
                         fg_sl_i64{ fg_bpe.attr.data(), (int64_t) fg_bpe.attr.size() },
                         fg_sl_i64{ fg_bpe.order.data(), (int64_t) fg_bpe.order.size() },
                         fg_bpe.n_special,
                         fg_sl_i64{ hits.data(), (int64_t) hits.size() });
        if (n_hits < 0) { n_hits = 0; }
    }
    int64_t nout = 0;
    auto run_plain = [&](int64_t from, int64_t len) -> bool {
        if (len <= 0) { return true; }
        const int64_t room = cap - nout;
        if (room <= 0) { return false; }
        if (plain_ids.size() < (size_t) room + 8) { plain_ids.resize((size_t) room + 8); }
        const int64_t r = fn(fg_sl_u8{ (uint8_t *) text + from, len },
                             fg_sl_u8{ fg_port.data.data(), (int64_t) fg_port.data.size() },
                             fg_sl_i64{ fg_bpe.tok_off.data(), (int64_t) fg_bpe.tok_off.size() },
                             fg_sl_i64{ fg_bpe.tok_len.data(), (int64_t) fg_bpe.tok_len.size() },
                             fg_sl_i64{ fg_bpe.tok_slots.data(), (int64_t) fg_bpe.tok_slots.size() },
                             fg_sl_i64{ fg_bpe.mrg_off.data(), (int64_t) fg_bpe.mrg_off.size() },
                             fg_sl_i64{ fg_bpe.mrg_len.data(), (int64_t) fg_bpe.mrg_len.size() },
                             fg_sl_i64{ fg_bpe.mrg_slots.data(), (int64_t) fg_bpe.mrg_slots.size() },
                             fg_sl_i64{ plain_ids.data(), (int64_t) plain_ids.size() }, 0,
                             fg_sl_i64{ cpts.data(), m + 1 }, fg_sl_i64{ seg.data(), 4 * (m + 1) },
                             fg_sl_u8{ enc.data(), 8 * (m + 1) },
                             fg_sl_i64{ sym.data(), 24 * (m + 1) },
                             fg_sl_i64{ heap.data(), 15 * (m + 1) + 40 },
                             fg_sl_i64{ hc.data(), 2 }, fg_sl_i64{ info.data(), 8 });
        if (r < 0 || nout + r > cap) { return false; }
        for (int64_t i = 0; i < r; ++i) { ids_buf[(size_t) (nout + i)] = plain_ids[(size_t) i]; }
        nout += r;
        return true;
    };

    bool ok = true;
    int64_t at = 0;
    for (int64_t h = 0; h < n_hits && ok; ++h) {
        const int64_t hs = hits[(size_t) (h * 3)], he = hits[(size_t) (h * 3 + 1)], hid = hits[(size_t) (h * 3 + 2)];
        ok = run_plain(at, hs - at);
        if (ok && nout < cap) { ids_buf[(size_t) nout++] = hid; } else { ok = false; }
        at = he;
    }
    if (ok) { ok = run_plain(at, n - at); }
    const int64_t got = ok ? nout : -1;
    if (fg_diag) { fprintf(stderr, "FG_TK_BPE_DONE got=%lld info0=%lld info4=%lld\n", (long long) got, (long long) info[0], (long long) info[4]); }
    if (got < 0 || got > cap) { return -1; }
    for (int64_t i = 0; i < got; ++i) { ids[i] = ids_buf[i]; }
    return got;
}

static int fg_gguf_check(const std::string & fname, const gguf_context & native) {
    static auto fn = []() -> fg_gr_parse_fn {
        void * h = dlopen("libllama.so.0", RTLD_LAZY | RTLD_NOLOAD);
        void * sym = h ? dlsym(h, "gr_parse") : nullptr;
        return reinterpret_cast<fg_gr_parse_fn>(sym);
    }();
    if (!fn) { fprintf(stderr, "FG_GGUF_RESOLVE fn=null\n"); return -1; }
    const size_t win = 64u * 1024u * 1024u;
    std::ifstream in(fname, std::ios::binary);
    if (!in) { fprintf(stderr, "FG_GGUF_OPEN failed\n"); return -1; }
    in.seekg(0, std::ios::end);
    const int64_t size = (int64_t) in.tellg();
    in.seekg(0, std::ios::beg);
    const size_t take = size < (int64_t) win ? (size_t) size : win;
    std::vector<uint8_t> data(take);
    if (!in.read(reinterpret_cast<char *>(data.data()), (std::streamsize) take)) { return -1; }
    const int64_t nk = (int64_t) gguf_get_n_kv(&native);
    const int64_t nt = (int64_t) gguf_get_n_tensors(&native);
    fg_port.data = std::move(data);
    data = {};   // the store owns it from here
    std::vector<int64_t> & state = fg_port.state, & kv = fg_port.kv, & tn = fg_port.tn, & slots = fg_port.slots;
    state.assign(8, 0); kv.assign((size_t) nk * 8, 0); tn.assign((size_t) nt * 12, 0); slots.assign(4096, 0);
    fg_port.ready = true;

    fg_sl_u8  s_data{ fg_port.data.data(), (int64_t) fg_port.data.size() };
    fg_sl_i64 s_state{ state.data(), 8 }, s_kv{ kv.data(), (int64_t) kv.size() },
              s_t{ tn.data(), (int64_t) tn.size() }, s_slots{ slots.data(), (int64_t) slots.size() };
    fn(s_data, size, s_state, s_kv, s_t, s_slots, s_slots);

    // The encoder's table, sized from the native reader's own counts for the two arrays it needs.
    {
        // The port's own lookup, not the native one: an index from one reader is only valid in that reader,
        // and a mismatch here would look exactly like a missing array.
        typedef fg_gr_find_fn fg_find_fn;   // the same signature the check uses: data, state, kv, slots, key
        static fg_find_fn fg_find = []() -> fg_find_fn {
            void * h = dlopen("libllama.so.0", RTLD_LAZY | RTLD_NOLOAD);
            return h ? reinterpret_cast<fg_find_fn>(dlsym(h, "gr_find_kv")) : nullptr;
        }();
        const char * fg_k_tok = "tokenizer.ggml.tokens", * fg_k_mrg = "tokenizer.ggml.merges";
        const int64_t tok_key = fg_find ? fg_find(fg_sl_u8{ fg_port.data.data(), (int64_t) fg_port.data.size() },
                                                  fg_sl_i64{ fg_port.state.data(), 8 },
                                                  fg_sl_i64{ fg_port.kv.data(), (int64_t) fg_port.kv.size() },
                                                  fg_sl_i64{ fg_port.slots.data(), (int64_t) fg_port.slots.size() },
                                                  fg_sl_u8{ (uint8_t *) fg_k_tok, (int64_t) strlen(fg_k_tok) })
                                        : gguf_find_key(&native, "tokenizer.ggml.tokens");
        const int64_t mrg_key = fg_find ? fg_find(fg_sl_u8{ fg_port.data.data(), (int64_t) fg_port.data.size() },
                                                  fg_sl_i64{ fg_port.state.data(), 8 },
                                                  fg_sl_i64{ fg_port.kv.data(), (int64_t) fg_port.kv.size() },
                                                  fg_sl_i64{ fg_port.slots.data(), (int64_t) fg_port.slots.size() },
                                                  fg_sl_u8{ (uint8_t *) fg_k_mrg, (int64_t) strlen(fg_k_mrg) })
                                        : gguf_find_key(&native, "tokenizer.ggml.merges");
        if (tok_key >= 0 && mrg_key >= 0) {
            const int64_t n_tok = gguf_get_arr_n(&native, (int) tok_key);
            const int64_t n_mrg = gguf_get_arr_n(&native, (int) mrg_key);
            const int32_t * type_data = nullptr;
            const int64_t ty_key = gguf_find_key(&native, "tokenizer.ggml.token_type");
            if (ty_key >= 0 && gguf_get_arr_n(&native, (int) ty_key) >= n_tok) {
                type_data = reinterpret_cast<const int32_t *>(gguf_get_arr_data(&native, (int) ty_key));
            }
            fg_bpe_init(tok_key, mrg_key, n_tok, n_mrg, type_data);
        }
    }

    int differ = 0;
    // Every key and every tensor name, resolved through the port and compared with the index the native
    // reader gives it. Counts alone would pass a reader that indexed the wrong records.
    static fg_gr_find_fn find_kv = nullptr, find_tn = nullptr;
    if (!find_kv) {
        void * h = dlopen("libllama.so.0", RTLD_LAZY | RTLD_NOLOAD);
        find_kv = h ? reinterpret_cast<fg_gr_find_fn>(dlsym(h, "gr_find_kv")) : nullptr;
        find_tn = h ? reinterpret_cast<fg_gr_find_fn>(dlsym(h, "gr_find_tensor")) : nullptr;
    }
    if (find_kv && find_tn) {
        int kv_bad = 0, tn_bad = 0;
        for (int64_t i = 0; i < nk; ++i) {
            const char * k = gguf_get_key(&native, i);
            fg_sl_u8 key{ reinterpret_cast<uint8_t *>(const_cast<char *>(k)), (int64_t) strlen(k) };
            if (find_kv(s_data, s_state, s_kv, s_slots, key) != i) { ++kv_bad; }
        }
        for (int64_t i = 0; i < nt; ++i) {
            const char * k = gguf_get_tensor_name(&native, i);
            fg_sl_u8 key{ reinterpret_cast<uint8_t *>(const_cast<char *>(k)), (int64_t) strlen(k) };
            if (find_tn(s_data, s_state, s_t, s_slots, key) != i) { ++tn_bad; }
        }
        // Integer key values, not only their names: this is the last piece before the loader could take a
        // value from the port, because a substituted value must be known equal on every key.
        int val_bad = 0, val_cmp = 0;
        static fg_gr_meta_int_fn meta_int = nullptr;
        if (!meta_int) {
            void * h2 = dlopen("libllama.so.0", RTLD_LAZY | RTLD_NOLOAD);
            meta_int = h2 ? reinterpret_cast<fg_gr_meta_int_fn>(dlsym(h2, "gr_meta_int")) : nullptr;
        }
        if (meta_int) {
            for (int64_t i = 0; i < nk; ++i) {
                int64_t native_val = 0;
                switch (gguf_get_kv_type(&native, i)) {
                    case GGUF_TYPE_UINT8:  native_val = gguf_get_val_u8 (&native, i); break;
                    case GGUF_TYPE_INT8:   native_val = gguf_get_val_i8 (&native, i); break;
                    case GGUF_TYPE_UINT16: native_val = gguf_get_val_u16(&native, i); break;
                    case GGUF_TYPE_INT16:  native_val = gguf_get_val_i16(&native, i); break;
                    case GGUF_TYPE_UINT32: native_val = gguf_get_val_u32(&native, i); break;
                    case GGUF_TYPE_INT32:  native_val = gguf_get_val_i32(&native, i); break;
                    case GGUF_TYPE_UINT64: native_val = (int64_t) gguf_get_val_u64(&native, i); break;
                    case GGUF_TYPE_INT64:  native_val = gguf_get_val_i64(&native, i); break;
                    default: continue;
                }
                int64_t got = 0;
                fg_sl_i64 out{ &got, 1 };
                ++val_cmp;
                if (meta_int(s_data, s_state, s_kv, i, out) != 0 || got != native_val) { ++val_bad; }
            }
            fprintf(stderr, "FG_GGUF_VALUES integer=%lld/%lld\n",
                    (long long) (val_cmp - val_bad), (long long) val_cmp);
            differ += val_bad;
        }

        // String keys: the loader reads general.architecture and the tokenizer names, so their text must be
        // known equal at the moment of use, not only at file level.
        int str_bad = 0, str_cmp = 0;
        static fg_gr_span_fn val_span = nullptr;
        if (!val_span) {
            void * h4 = dlopen("libllama.so.0", RTLD_LAZY | RTLD_NOLOAD);
            val_span = h4 ? reinterpret_cast<fg_gr_span_fn>(dlsym(h4, "gr_val_span")) : nullptr;
        }
        if (val_span) {
            for (int64_t i = 0; i < nk; ++i) {
                if (gguf_get_kv_type(&native, i) != GGUF_TYPE_STRING) { continue; }
                int64_t out[4] = {0};
                fg_sl_i64 so{ out, 4 };
                ++str_cmp;
                const char * ns = gguf_get_val_str(&native, i);
                const size_t nn = strlen(ns);
                if (val_span(s_state, s_kv, i, so) != 0 || (size_t) out[2] != nn ||
                    out[1] < 0 || (size_t) (out[1] + out[2]) > fg_port.data.size() ||
                    memcmp(fg_port.data.data() + out[1], ns, nn) != 0) {
                    ++str_bad;
                }
            }
            fprintf(stderr, "FG_GGUF_STRINGS keys=%lld/%lld\n",
                    (long long) (str_cmp - str_bad), (long long) str_cmp);
        }

        // Arrays: the tokenizer tables the loader reads. Compared element by element, because a count and a
        // type can both match while the contents differ.
        int arr_bad = 0, arr_cmp = 0;
        static fg_gr_arrstr_fn arr_str = nullptr;
        static fg_gr_arrint_fn arr_int = nullptr;
        typedef int64_t (*fg_arr_offs_t)(fg_sl_u8, fg_sl_i64, fg_sl_i64, int64_t, fg_sl_i64, fg_sl_i64);
        static fg_arr_offs_t offs_fn = nullptr;
        if (!arr_str) {
            void * h5 = dlopen("libllama.so.0", RTLD_LAZY | RTLD_NOLOAD);
            arr_str = h5 ? reinterpret_cast<fg_gr_arrstr_fn>(dlsym(h5, "gr_arr_str_span")) : nullptr;
            arr_int = h5 ? reinterpret_cast<fg_gr_arrint_fn>(dlsym(h5, "gr_array_int")) : nullptr;
            offs_fn = h5 ? reinterpret_cast<fg_arr_offs_t>(dlsym(h5, "gr_arr_str_offsets")) : nullptr;
        }
        for (int64_t i = 0; i < nk && arr_str && arr_int && offs_fn; ++i) {
            if (gguf_get_kv_type(&native, i) != GGUF_TYPE_ARRAY) { continue; }
            const int64_t et = (int64_t) gguf_get_arr_type(&native, i);
            const int64_t n  = (int64_t) gguf_get_arr_n(&native, i);
            if (et == GGUF_TYPE_STRING) {
                // One pass for the whole array. The per-element door walks from the array start each call,
                // which is quadratic: at 248 320 tokens this section never finished, so the check was unusable
                // on a real vocabulary and nobody noticed because acceptance runs do not enable it
                // (EXP-2026-11-19-1390).
                static std::vector<int64_t> aoff, alen;
                aoff.assign((size_t) n, 0); alen.assign((size_t) n, 0);
                const int64_t got = offs_fn(s_data, s_state, s_kv, i,
                                            fg_sl_i64{ aoff.data(), n }, fg_sl_i64{ alen.data(), n });
                for (int64_t e = 0; e < n; ++e) {
                    const char * ns = gguf_get_arr_str(&native, i, (int) e);
                    const size_t nn = strlen(ns);
                    ++arr_cmp;
                    if (got != n || aoff[(size_t) e] < 0 || alen[(size_t) e] != (int64_t) nn ||
                        (size_t) (aoff[(size_t) e] + alen[(size_t) e]) > fg_port.data.size() ||
                        memcmp(fg_port.data.data() + aoff[(size_t) e], ns, nn) != 0) {
                        ++arr_bad;
                    }
                }
            } else if (et >= GGUF_TYPE_UINT8 && et <= GGUF_TYPE_INT32) {
                const uint8_t * raw = reinterpret_cast<const uint8_t *>(gguf_get_arr_data(&native, i));
                for (int64_t e = 0; e < n; ++e) {
                    int64_t out[1] = {0};
                    fg_sl_i64 so{ out, 1 };
                    int64_t native_el = 0;
                    switch (et) {
                        case GGUF_TYPE_UINT8:  native_el = raw[e]; break;
                        case GGUF_TYPE_INT8:   native_el = ((const int8_t *)  raw)[e]; break;
                        case GGUF_TYPE_UINT16: native_el = ((const uint16_t *) raw)[e]; break;
                        case GGUF_TYPE_INT16:  native_el = ((const int16_t *)  raw)[e]; break;
                        case GGUF_TYPE_UINT32: native_el = ((const uint32_t *) raw)[e]; break;
                        case GGUF_TYPE_INT32:  native_el = ((const int32_t *)  raw)[e]; break;
                        default: continue;
                    }
                    ++arr_cmp;
                    if (arr_int(s_data, s_state, s_kv, i, e, so) != 0 || out[0] != native_el) { ++arr_bad; }
                }
            }
        }
        if (arr_cmp) {
            fprintf(stderr, "FG_GGUF_ARRAYS elements=%lld/%lld\n",
                    (long long) (arr_cmp - arr_bad), (long long) arr_cmp);
        }

        // Tensor descriptors through the door the loader would use: dtype, dims, extents and data offset.
        int ti_bad = 0, ti_cmp = 0;
        static fg_gr_tinfo_fn tinfo = nullptr;
        if (!tinfo) {
            void * h3 = dlopen("libllama.so.0", RTLD_LAZY | RTLD_NOLOAD);
            tinfo = h3 ? reinterpret_cast<fg_gr_tinfo_fn>(dlsym(h3, "gr_tensor_info")) : nullptr;
        }
        if (tinfo) {
            for (int64_t i = 0; i < nt; ++i) {
                int64_t out[9] = {0};
                fg_sl_i64 so{ out, 9 };
                const int64_t * ne = gguf_get_tensor_ne(&native, i);
                uint32_t dims = 0;
                while (dims < 4 && ne[dims] != 1) { ++dims; }
                ++ti_cmp;
                bool ok = tinfo(s_state, s_t, i, so) == 0 &&
                          out[0] == (int64_t) gguf_get_tensor_type(&native, i) &&
                          out[1] == (int64_t) dims &&
                          (size_t) (out[8] - state[5]) == gguf_get_tensor_offset(&native, i);
                for (uint32_t d = 0; ok && d < dims; ++d) { ok = out[2 + d] == ne[d]; }
                if (!ok) { ++ti_bad; }
            }
        }

        if (ti_cmp) {
            fprintf(stderr, "FG_GGUF_TINFO descriptors=%lld/%lld\n",
                    (long long) (ti_cmp - ti_bad), (long long) ti_cmp);
        }
        differ += kv_bad + tn_bad + ti_bad + str_bad + arr_bad;
        fprintf(stderr, "FG_GGUF_LOOKUP keys=%lld/%lld tensors=%lld/%lld\n",
                (long long) (nk - kv_bad), (long long) nk, (long long) (nt - tn_bad), (long long) nt);
    }

    if (state[0] != 919) { ++differ; fprintf(stderr, "FG_GGUF_DIFF refused state0=%lld\n", (long long) state[0]); }
    if (state[2] != nk)  { ++differ; fprintf(stderr, "FG_GGUF_DIFF nk port=%lld native=%lld\n", (long long) state[2], (long long) nk); }
    if (state[3] != nt)  { ++differ; fprintf(stderr, "FG_GGUF_DIFF nt port=%lld native=%lld\n", (long long) state[3], (long long) nt); }
    if ((size_t) state[4] != gguf_get_alignment(&native)) { ++differ; fprintf(stderr, "FG_GGUF_DIFF align\n"); }
    if ((size_t) state[5] != gguf_get_data_offset(&native)) { ++differ; fprintf(stderr, "FG_GGUF_DIFF data_off\n"); }
    fprintf(stderr, "FG_GGUF_PARSE nk=%lld nt=%lld align=%lld off=%lld disagreements=%d\n",
            (long long) state[2], (long long) state[3], (long long) state[4], (long long) state[5], differ);
    return differ;
}
} // namespace

static const size_t kiB = 1024;
static const size_t MiB = 1024*kiB;
static const size_t GiB = 1024*MiB;

const char * llama_file_version_name(llama_fver version) {
    switch (version) {
        case GGUF_FILE_VERSION_V1: return "GGUF V1 (support until nov 2023)";
        case GGUF_FILE_VERSION_V2: return "GGUF V2";
        case GGUF_FILE_VERSION_V3: return "GGUF V3 (latest)";
    }

    return "unknown";
}

#define LLAMA_FTYPE_PREFIX "(guessed) "

const char * llama_ftype_name(llama_ftype ftype) {
    static constexpr size_t guessed_prefix_len = sizeof(LLAMA_FTYPE_PREFIX) - 1;
    const char * name;
    switch ((enum llama_ftype) (ftype & ~LLAMA_FTYPE_GUESSED)) {
        case LLAMA_FTYPE_ALL_F32:          name = LLAMA_FTYPE_PREFIX "all F32"; break;
        case LLAMA_FTYPE_MOSTLY_F16:       name = LLAMA_FTYPE_PREFIX "F16"; break;
        case LLAMA_FTYPE_MOSTLY_BF16:      name = LLAMA_FTYPE_PREFIX "BF16"; break;
        case LLAMA_FTYPE_MOSTLY_Q1_0:      name = LLAMA_FTYPE_PREFIX "Q1_0"; break;
        case LLAMA_FTYPE_MOSTLY_Q2_0:      name = LLAMA_FTYPE_PREFIX "Q2_0"; break;
        case LLAMA_FTYPE_MOSTLY_Q4_0:      name = LLAMA_FTYPE_PREFIX "Q4_0"; break;
        case LLAMA_FTYPE_MOSTLY_Q4_1:      name = LLAMA_FTYPE_PREFIX "Q4_1"; break;
        case LLAMA_FTYPE_MOSTLY_Q5_0:      name = LLAMA_FTYPE_PREFIX "Q5_0"; break;
        case LLAMA_FTYPE_MOSTLY_Q5_1:      name = LLAMA_FTYPE_PREFIX "Q5_1"; break;
        case LLAMA_FTYPE_MOSTLY_Q8_0:      name = LLAMA_FTYPE_PREFIX "Q8_0"; break;
        case LLAMA_FTYPE_MOSTLY_MXFP4_MOE: name = LLAMA_FTYPE_PREFIX "MXFP4 MoE"; break;
        case LLAMA_FTYPE_MOSTLY_NVFP4:     name = LLAMA_FTYPE_PREFIX "NVFP4"; break;
        case LLAMA_FTYPE_MOSTLY_Q2_K:      name = LLAMA_FTYPE_PREFIX "Q2_K - Medium"; break;
        case LLAMA_FTYPE_MOSTLY_Q2_K_S:    name = LLAMA_FTYPE_PREFIX "Q2_K - Small"; break;
        case LLAMA_FTYPE_MOSTLY_Q3_K_S:    name = LLAMA_FTYPE_PREFIX "Q3_K - Small"; break;
        case LLAMA_FTYPE_MOSTLY_Q3_K_M:    name = LLAMA_FTYPE_PREFIX "Q3_K - Medium"; break;
        case LLAMA_FTYPE_MOSTLY_Q3_K_L:    name = LLAMA_FTYPE_PREFIX "Q3_K - Large"; break;
        case LLAMA_FTYPE_MOSTLY_Q4_K_S:    name = LLAMA_FTYPE_PREFIX "Q4_K - Small"; break;
        case LLAMA_FTYPE_MOSTLY_Q4_K_M:    name = LLAMA_FTYPE_PREFIX "Q4_K - Medium"; break;
        case LLAMA_FTYPE_MOSTLY_Q5_K_S:    name = LLAMA_FTYPE_PREFIX "Q5_K - Small"; break;
        case LLAMA_FTYPE_MOSTLY_Q5_K_M:    name = LLAMA_FTYPE_PREFIX "Q5_K - Medium"; break;
        case LLAMA_FTYPE_MOSTLY_Q6_K:      name = LLAMA_FTYPE_PREFIX "Q6_K"; break;
        case LLAMA_FTYPE_MOSTLY_TQ1_0:     name = LLAMA_FTYPE_PREFIX "TQ1_0 - 1.69 bpw ternary"; break;
        case LLAMA_FTYPE_MOSTLY_TQ2_0:     name = LLAMA_FTYPE_PREFIX "TQ2_0 - 2.06 bpw ternary"; break;
        case LLAMA_FTYPE_MOSTLY_IQ2_XXS:   name = LLAMA_FTYPE_PREFIX "IQ2_XXS - 2.0625 bpw"; break;
        case LLAMA_FTYPE_MOSTLY_IQ2_XS:    name = LLAMA_FTYPE_PREFIX "IQ2_XS - 2.3125 bpw"; break;
        case LLAMA_FTYPE_MOSTLY_IQ2_S:     name = LLAMA_FTYPE_PREFIX "IQ2_S - 2.5 bpw"; break;
        case LLAMA_FTYPE_MOSTLY_IQ2_M:     name = LLAMA_FTYPE_PREFIX "IQ2_M - 2.7 bpw"; break;
        case LLAMA_FTYPE_MOSTLY_IQ3_XS:    name = LLAMA_FTYPE_PREFIX "IQ3_XS - 3.3 bpw"; break;
        case LLAMA_FTYPE_MOSTLY_IQ3_XXS:   name = LLAMA_FTYPE_PREFIX "IQ3_XXS - 3.0625 bpw"; break;
        case LLAMA_FTYPE_MOSTLY_IQ1_S:     name = LLAMA_FTYPE_PREFIX "IQ1_S - 1.5625 bpw"; break;
        case LLAMA_FTYPE_MOSTLY_IQ1_M:     name = LLAMA_FTYPE_PREFIX "IQ1_M - 1.75 bpw"; break;
        case LLAMA_FTYPE_MOSTLY_IQ4_NL:    name = LLAMA_FTYPE_PREFIX "IQ4_NL - 4.5 bpw"; break;
        case LLAMA_FTYPE_MOSTLY_IQ4_XS:    name = LLAMA_FTYPE_PREFIX "IQ4_XS - 4.25 bpw"; break;
        case LLAMA_FTYPE_MOSTLY_IQ3_S:     name = LLAMA_FTYPE_PREFIX "IQ3_S - 3.4375 bpw"; break;
        case LLAMA_FTYPE_MOSTLY_IQ3_M:     name = LLAMA_FTYPE_PREFIX "IQ3_S mix - 3.66 bpw"; break;
        default:                           name = LLAMA_FTYPE_PREFIX "unknown, may not work"; break;
    }
    return (ftype & LLAMA_FTYPE_GUESSED) ? name : name + guessed_prefix_len;
}

#undef LLAMA_FTYPE_PREFIX

// return a list of splits for a given path
// for example, given "<name>-00002-of-00004.gguf", returns list of all 4 splits
static std::vector<std::string> llama_get_list_splits(const std::string & path, const int idx, const int n_split) {
    std::vector<std::string> paths;
    std::string split_prefix;
    std::vector<char> buf(llama_path_max(), 0);

    {
        int ret = llama_split_prefix(buf.data(), buf.size(), path.c_str(), idx, n_split);
        if (!ret) {
            throw std::runtime_error(format("invalid split file name: %s", path.c_str()));
        }
        split_prefix = std::string(buf.data(), ret);
    }

    if (split_prefix.empty()) {
        throw std::runtime_error(format("invalid split file: %s", path.c_str()));
    }

    for (int idx = 0; idx < n_split; ++idx) {
        int ret = llama_split_path(buf.data(), buf.size(), split_prefix.c_str(), idx, n_split);
        paths.push_back(std::string(buf.data(), ret));
    }

    return paths;
}

namespace GGUFMeta {
    template <typename T, gguf_type gt_, T (*gfun)(const gguf_context *, const int64_t)>
    struct GKV_Base_Type {
        static constexpr gguf_type gt = gt_;

        static T getter(const gguf_context * ctx, const int kid) {
            return gfun(ctx, kid);
        }
    };

    template<typename T> struct GKV_Base;

    template<> struct GKV_Base<bool        >: GKV_Base_Type<bool,         GGUF_TYPE_BOOL,    gguf_get_val_bool> {};
    template<> struct GKV_Base<uint8_t     >: GKV_Base_Type<uint8_t,      GGUF_TYPE_UINT8,   gguf_get_val_u8  > {};
    template<> struct GKV_Base<uint16_t    >: GKV_Base_Type<uint16_t,     GGUF_TYPE_UINT16,  gguf_get_val_u16 > {};
    template<> struct GKV_Base<uint32_t    >: GKV_Base_Type<uint32_t,     GGUF_TYPE_UINT32,  gguf_get_val_u32 > {};
    template<> struct GKV_Base<uint64_t    >: GKV_Base_Type<uint64_t,     GGUF_TYPE_UINT64,  gguf_get_val_u64 > {};
    template<> struct GKV_Base<int8_t      >: GKV_Base_Type<int8_t,       GGUF_TYPE_INT8,    gguf_get_val_i8  > {};
    template<> struct GKV_Base<int16_t     >: GKV_Base_Type<int16_t,      GGUF_TYPE_INT16,   gguf_get_val_i16 > {};
    template<> struct GKV_Base<int32_t     >: GKV_Base_Type<int32_t,      GGUF_TYPE_INT32,   gguf_get_val_i32 > {};
    template<> struct GKV_Base<int64_t     >: GKV_Base_Type<int64_t,      GGUF_TYPE_INT64,   gguf_get_val_i64 > {};
    template<> struct GKV_Base<float       >: GKV_Base_Type<float,        GGUF_TYPE_FLOAT32, gguf_get_val_f32 > {};
    template<> struct GKV_Base<double      >: GKV_Base_Type<double,       GGUF_TYPE_FLOAT64, gguf_get_val_f64 > {};
    template<> struct GKV_Base<const char *>: GKV_Base_Type<const char *, GGUF_TYPE_STRING,  gguf_get_val_str > {};

    template<> struct GKV_Base<std::string> {
        static constexpr gguf_type gt = GGUF_TYPE_STRING;

        static std::string getter(const gguf_context * ctx, const int kid) {
            return gguf_get_val_str(ctx, kid);
        }
    };

    struct ArrayInfo {
        const gguf_type gt;
        const size_t length;
        const void * data;
    };

    template<> struct GKV_Base<ArrayInfo> {
        public:
        static constexpr gguf_type gt = GGUF_TYPE_ARRAY;
        static ArrayInfo getter(const gguf_context *ctx, const int k) {
            const enum gguf_type arr_type = gguf_get_arr_type(ctx, k);
            return ArrayInfo {
                arr_type,
                gguf_get_arr_n(ctx, k),
                arr_type == GGUF_TYPE_STRING ? nullptr : gguf_get_arr_data(ctx, k),
            };
        }
    };

    template<typename T>
    class GKV : public GKV_Base<T> {
        GKV() = delete;

        public:
        static T get_kv(const gguf_context * ctx, const int k) {
            const enum gguf_type kt = gguf_get_kv_type(ctx, k);

            if (kt != GKV::gt) {
                throw std::runtime_error(format("key %s has wrong type %s but expected type %s",
                    gguf_get_key(ctx, k), gguf_type_name(kt), gguf_type_name(GKV::gt)));
            }
            // Integral keys come from the port when it can answer, and the native getter is not called at
            // all in that case: this is the first converted site, and the shadow comparison below proves the
            // two agree on every read during a real load. A key the port cannot answer - a split-file shard,
            // or a key outside the window - falls back to native rather than to a default.
            if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
                int64_t port_value = 0;
                if (::fg_port_int(k, port_value)) {
                    static std::atomic<long long> served{0};
                    const long long c = served.fetch_add(1) + 1;
                    if (getenv("FG_GGUF_DIAG") && c % 8 == 0) {
                        fprintf(stderr, "FG_GGUF_SERVED from_port=%lld\n", c);
                    }
                    return (T) port_value;
                }
            }
            // Strings are next: the loader reads general.architecture and the tokenizer names this way. Only
            // std::string is converted - the const char * form would need a NUL-terminated copy with a lifetime
            // nobody has stated, and that is a separate decision.
            if constexpr (std::is_same_v<T, std::string>) {
                const uint8_t * p = nullptr; int64_t n = 0;
                if (::fg_port_str(k, p, n)) {
                    static std::atomic<long long> served_str{0};
                    const long long c = served_str.fetch_add(1) + 1;
                    if (getenv("FG_GGUF_DIAG") && c % 4 == 0) {
                        fprintf(stderr, "FG_GGUF_SERVED_STR from_port=%lld\n", c);
                    }
                    return std::string(reinterpret_cast<const char *>(p), (size_t) n);
                }
            }
            const T value = GKV::getter(ctx, k);

            // Shadow read: the same key through the port, at the moment the loader uses it. A disagreement is
            // fatal under the flag, because a converted site that read the wrong field must fail here and not
            // in generation. Counted per key so a whole load reports how many use-site reads were compared.
            static const bool shadow = getenv("FG_GGUF_DIAG") != nullptr;
            if (shadow && ::fg_port.ready) {
                static std::atomic<long long> compared{0}, differed{0};
                if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
                    int64_t port_value = 0;
                    if (::fg_port_int(k, port_value)) {
                        compared.fetch_add(1);
                        if (port_value != (int64_t) value) {
                            differed.fetch_add(1);
                            throw std::runtime_error(format("%s: ported reader disagrees on key %s (%lld vs %lld)",
                                __func__, gguf_get_key(ctx, k), (long long) port_value, (long long) value));
                        }
                    }
                } else if constexpr (std::is_same_v<T, std::string>) {
                    const uint8_t * p = nullptr; int64_t n = 0;
                    if (::fg_port_str(k, p, n)) {
                        compared.fetch_add(1);
                        if ((size_t) n != value.size() || memcmp(p, value.data(), (size_t) n) != 0) {
                            differed.fetch_add(1);
                            throw std::runtime_error(format("%s: ported reader disagrees on key %s", __func__, gguf_get_key(ctx, k)));
                        }
                    }
                }
                static std::atomic<long long> reported{0};
                const long long c = compared.load();
                if (c != reported.load() && c % 8 == 0) {
                    reported.store(c);
                    fprintf(stderr, "FG_GGUF_USES compared=%lld differed=%lld\n", c, differed.load());
                }
            }
            return value;
        }

        static const char * override_type_to_str(const llama_model_kv_override_type ty) {
            switch (ty) {
                case LLAMA_KV_OVERRIDE_TYPE_BOOL:  return "bool";
                case LLAMA_KV_OVERRIDE_TYPE_INT:   return "int";
                case LLAMA_KV_OVERRIDE_TYPE_FLOAT: return "float";
                case LLAMA_KV_OVERRIDE_TYPE_STR:   return "str";
            }
            return "unknown";
        }

        static bool validate_override(const llama_model_kv_override_type expected_type, const struct llama_model_kv_override * ovrd) {
            if (!ovrd) { return false; }
            if (ovrd->tag == expected_type) {
                LLAMA_LOG_INFO("%s: Using metadata override (%5s) '%s' = ",
                    __func__, override_type_to_str(ovrd->tag), ovrd->key);
                switch (ovrd->tag) {
                    case LLAMA_KV_OVERRIDE_TYPE_BOOL:  {
                        LLAMA_LOG_INFO("%s\n", ovrd->val_bool ? "true" : "false");
                    } break;
                    case LLAMA_KV_OVERRIDE_TYPE_INT:   {
                        LLAMA_LOG_INFO("%" PRId64 "\n", ovrd->val_i64);
                    } break;
                    case LLAMA_KV_OVERRIDE_TYPE_FLOAT: {
                        LLAMA_LOG_INFO("%.6f\n", ovrd->val_f64);
                    } break;
                    case LLAMA_KV_OVERRIDE_TYPE_STR: {
                        LLAMA_LOG_INFO("%s\n", ovrd->val_str);
                    } break;
                    default:
                        // Shouldn't be possible to end up here, but just in case...
                        throw std::runtime_error(
                            format("Unsupported attempt to override %s type for metadata key %s\n",
                                override_type_to_str(ovrd->tag), ovrd->key));
                }
                return true;
            }
            LLAMA_LOG_WARN("%s: Warning: Bad metadata override type for key '%s', expected %s but got %s\n",
                __func__, ovrd->key, override_type_to_str(expected_type), override_type_to_str(ovrd->tag));
            return false;
        }

        template<typename OT>
        static typename std::enable_if<std::is_same<OT, bool>::value, bool>::type
        try_override(OT & target, const struct llama_model_kv_override * ovrd) {
            if (validate_override(LLAMA_KV_OVERRIDE_TYPE_BOOL, ovrd)) {
                target = ovrd->val_bool;
                return true;
            }
            return false;
        }

        template<typename OT>
        static typename std::enable_if<!std::is_same<OT, bool>::value && std::is_integral<OT>::value, bool>::type
        try_override(OT & target, const struct llama_model_kv_override * ovrd) {
            if (validate_override(LLAMA_KV_OVERRIDE_TYPE_INT, ovrd)) {
                target = ovrd->val_i64;
                return true;
            }
            return false;
        }

        template<typename OT>
        static typename std::enable_if<std::is_floating_point<OT>::value, bool>::type
        try_override(T & target, const struct llama_model_kv_override * ovrd) {
            if (validate_override(LLAMA_KV_OVERRIDE_TYPE_FLOAT, ovrd)) {
                target = ovrd->val_f64;
                return true;
            }
            return false;
        }

        template<typename OT>
        static typename std::enable_if<std::is_same<OT, std::string>::value, bool>::type
        try_override(T & target, const struct llama_model_kv_override * ovrd) {
            if (validate_override(LLAMA_KV_OVERRIDE_TYPE_STR, ovrd)) {
                target = ovrd->val_str;
                return true;
            }
            return false;
        }

        static bool set(const gguf_context * ctx, const int k, T & target, const struct llama_model_kv_override * ovrd = nullptr) {
            if (try_override<T>(target, ovrd)) {
                return true;
            }
            if (k < 0) { return false; }
            target = get_kv(ctx, k);
            return true;
        }

        static bool set(const gguf_context * ctx, const char * key, T & target, const struct llama_model_kv_override * ovrd = nullptr) {
            return set(ctx, gguf_find_key(ctx, key), target, ovrd);
        }

        static bool set(const gguf_context * ctx, const std::string & key, T & target, const struct llama_model_kv_override * ovrd = nullptr) {
            return set(ctx, key.c_str(), target, ovrd);
        }
    };
}

    template<typename T>
    typename std::enable_if<std::is_integral<T>::value, bool>::type
    llama_model_loader::get_arr_n(const std::string & key, T & result, bool required) {
        const int kid = gguf_find_key(metadata, key.c_str());

        if (kid < 0) {
            if (required) {
                throw std::runtime_error(format("key not found in model: %s", key.c_str()));
            }
            return false;
        }

        struct GGUFMeta::ArrayInfo arr_info =
            GGUFMeta::GKV<GGUFMeta::ArrayInfo>::get_kv(metadata, kid);


        result = arr_info.length;
        return true;
    }

    template<typename T>
    typename std::enable_if<std::is_integral<T>::value, bool>::type
    llama_model_loader::get_arr_n(enum llm_kv kid, T & result, bool required) {
        return get_arr_n(llm_kv(kid), result, required);
    }

    template bool llama_model_loader::get_arr_n(enum llm_kv kid, uint32_t & result, bool required);
    template std::enable_if<std::is_integral<uint32_t>::value, bool>::type
    llama_model_loader::get_arr_n<uint32_t>(const std::string & key, uint32_t & result, bool required);

    template<typename T>
    bool llama_model_loader::get_arr(const std::string & key, std::vector<T> & result, bool required) {
        const gguf_context * ctx = metadata;
        const int kid = gguf_find_key(ctx, key.c_str());

        if (kid < 0 || gguf_get_kv_type(ctx, kid) != GGUF_TYPE_ARRAY) {
            if (required) {
                throw std::runtime_error(format("array key not found in model: %s", key.c_str()));
            }
            return false;
        }

        struct GGUFMeta::ArrayInfo arr_info =
            GGUFMeta::GKV<GGUFMeta::ArrayInfo>::get_kv(ctx, kid);

        bool type_ok = false;
        switch (arr_info.gt) {
            case GGUF_TYPE_UINT32:
            case GGUF_TYPE_INT32:   type_ok = (std::is_same<T,     int32_t>::value) ||
                                              (std::is_same<T,    uint32_t>::value); break;
            case GGUF_TYPE_UINT64:  type_ok = (std::is_same<T,    uint64_t>::value); break;
            case GGUF_TYPE_FLOAT32: type_ok = (std::is_same<T,       float>::value); break;
            case GGUF_TYPE_STRING:  type_ok = (std::is_same<T, std::string>::value); break;
            default:
                throw std::runtime_error(format("%s is not a string/float32/uint32/int32/uint64 array", key.c_str()));
        }
        if (!type_ok) {
            throw std::runtime_error(format("%s has wrong array element type %s", key.c_str(), gguf_type_name(arr_info.gt)));
        }

        if constexpr (std::is_same<T, std::string>::value) {
            const size_t n_items = gguf_get_arr_n(ctx, kid);
            result.clear();

            for (size_t i = 0; i < n_items; i++) {
                const T value = gguf_get_arr_str(ctx, kid, i);
                result.emplace_back(value);
            }
        } else {
            result.resize(arr_info.length);
            result.assign((const T*)arr_info.data, (const T *)arr_info.data + arr_info.length);
        }

        return true;
    }

    template<typename T, size_t N_MAX>
    bool llama_model_loader::get_arr(const std::string & key, std::array<T, N_MAX> & result, bool required) {
        const gguf_context * ctx = metadata;
        const int kid = gguf_find_key(ctx, key.c_str());

        if (kid < 0 || gguf_get_kv_type(ctx, kid) != GGUF_TYPE_ARRAY) {
            if (required) {
                throw std::runtime_error(format("array key not found in model: %s", key.c_str()));
            }
            return false;
        }

        struct GGUFMeta::ArrayInfo arr_info =
            GGUFMeta::GKV<GGUFMeta::ArrayInfo>::get_kv(ctx, kid);

        bool type_ok = false;
        switch (arr_info.gt) {
            case GGUF_TYPE_BOOL:
            case GGUF_TYPE_UINT32:
            case GGUF_TYPE_INT32:   type_ok = (std::is_same<T,     int32_t>::value) ||
                                              (std::is_same<T,    uint32_t>::value); break;
            case GGUF_TYPE_UINT64:  type_ok = (std::is_same<T,    uint64_t>::value); break;
            case GGUF_TYPE_FLOAT32: type_ok = (std::is_same<T,       float>::value); break;
            case GGUF_TYPE_STRING:  type_ok = (std::is_same<T, std::string>::value); break;
            default:
                throw std::runtime_error(format("%s is not a string/float32/uint32/int32/uint64 array", key.c_str()));
        }
        if (!type_ok) {
            throw std::runtime_error(format("%s has wrong array element type %s", key.c_str(), gguf_type_name(arr_info.gt)));
        }

        if (arr_info.length > N_MAX) {
            throw std::runtime_error(format("array length %u for key %s exceeds max %u", (uint32_t) arr_info.length, key.c_str(), (uint32_t) N_MAX));
        }

        if constexpr (std::is_same<T, std::string>::value) {
            const size_t n_items = gguf_get_arr_n(ctx, kid);

            for (size_t i = 0; i < n_items; i++) {
                const T value = gguf_get_arr_str(ctx, kid, i);
                result[i] = value;
            }
        } else {
            if (arr_info.gt == GGUF_TYPE_BOOL) {
                const int8_t * values = (const int8_t *) arr_info.data;
                std::transform(values, values + arr_info.length, result.begin(), [](int8_t x) {
                    return static_cast<T>(x != 0);
                });
            } else {
                std::copy((const T*)arr_info.data, (const T *)arr_info.data + arr_info.length, result.begin());
            }
        }

        return true;
    }

    template<typename T>
    bool llama_model_loader::get_arr(enum llm_kv kid, T & result, bool required) {
        return get_arr(llm_kv(kid), result, required);
    }

    template bool llama_model_loader::get_arr<std::vector<std::string>>(enum llm_kv kid, std::vector<std::string> & result, bool required);
    template bool llama_model_loader::get_arr<std::array<int32_t, 512>>(enum llm_kv kid, std::array<int32_t, 512> & result, bool required);
    template bool llama_model_loader::get_arr<std::vector<int32_t>>(enum llm_kv kid, std::vector<int32_t> & result, bool required);
    template bool llama_model_loader::get_arr<std::array<uint32_t, LLAMA_MAX_LAYERS>>(enum llm_kv kid, std::array<uint32_t, LLAMA_MAX_LAYERS> & result, bool required);
    template bool llama_model_loader::get_arr<std::vector<uint32_t>>(enum llm_kv kid, std::vector<uint32_t> & result, bool required);
    template bool llama_model_loader::get_arr<std::array<uint64_t, LLAMA_MAX_PLE_NGRAM>>(enum llm_kv kid, std::array<uint64_t, LLAMA_MAX_PLE_NGRAM> & result, bool required);
    template bool llama_model_loader::get_arr<std::array<uint64_t, LLAMA_MAX_PLE_HEADS>>(enum llm_kv kid, std::array<uint64_t, LLAMA_MAX_PLE_HEADS> & result, bool required);

    template<typename T>
    bool llama_model_loader::get_key(const std::string & key, T & result, bool required) {
        auto it = kv_overrides.find(key);

        const struct llama_model_kv_override * override =
            it != kv_overrides.end() ? &it->second : nullptr;

        const bool found = GGUFMeta::GKV<T>::set(metadata, key, result, override);

        if (required && !found) {
            throw std::runtime_error(format("key not found in model: %s", key.c_str()));
        }

        return found;
    }

    template<typename T>
    bool llama_model_loader::get_key(enum llm_kv kid, T & result, bool required) {
        return get_key(llm_kv(kid), result, required);
    }

    template bool llama_model_loader::get_key<bool>       (enum llm_kv kid, bool & result,        bool required);
    template bool llama_model_loader::get_key<float>      (enum llm_kv kid, float & result,       bool required);
    template bool llama_model_loader::get_key<uint32_t>   (enum llm_kv kid, uint32_t & result,    bool required);
    template bool llama_model_loader::get_key<std::string>(enum llm_kv kid, std::string & result, bool required);

    template<>
    bool llama_model_loader::get_key(enum llm_kv kid, enum llama_pooling_type & result, bool required) {
        uint32_t tmp;
        const bool found = get_key(kid, tmp, required);
        if (found) {
            result = (enum llama_pooling_type) tmp;
        } else {
            result = LLAMA_POOLING_TYPE_UNSPECIFIED;
        }
        return found;
    }

    // get array of n <= N_MAX elements, or a single element repeated n times
    template<typename T, size_t N_MAX>
    bool llama_model_loader::get_key_or_arr(const std::string & key, std::array<T, N_MAX> & result, uint32_t n, bool required) {
        const int kid = gguf_find_key(metadata, key.c_str());

        if (kid < 0) {
            if (required) {
                throw std::runtime_error(format("key not found in model: %s", key.c_str()));
            }
            return false;
        }

        if (n > N_MAX) {
            throw std::runtime_error(format("n > N_MAX: %u > %u for key %s", n, (uint32_t) N_MAX, key.c_str()));
        }

        if (gguf_get_kv_type(metadata, kid) == GGUF_TYPE_ARRAY) {
            struct GGUFMeta::ArrayInfo arr_info =
                GGUFMeta::GKV<GGUFMeta::ArrayInfo>::get_kv(metadata, kid);

            if (n != arr_info.length) {
                throw std::runtime_error(format("key %s has wrong array length; expected %u, got %u", key.c_str(), n, (uint32_t) arr_info.length));
            }

            return get_arr(key, result, required);
        }

        T value;

        bool ok = get_key(key, value, required);
        if (!ok) {
            return false;
        }

        for (uint32_t i = 0; i < n; i++) {
            result[i] = value;
        }

        return true;
    }

    template<typename T>
    bool llama_model_loader::get_key_or_arr(enum llm_kv kid, T & result, uint32_t n, bool required) {
        return get_key_or_arr(llm_kv(kid), result, n, required);
    }

    bool llama_model_loader::get_key_or_arr(enum llm_kv kid, uint32_t & result, bool required) {
        const std::string key = llm_kv(kid);

        const int id = gguf_find_key(metadata, key.c_str());

        if (id < 0) {
            if (required) {
                throw std::runtime_error(format("key not found in model: %s", key.c_str()));
            }
            return false;
        }

        // throw and error if type is an array
        if (gguf_get_kv_type(metadata, id) == GGUF_TYPE_ARRAY) {
            if (required) {
                throw std::runtime_error(format("expected scalar, found array for key: %s", key.c_str()));
            }
            return false;
        }

        return get_key(key, result, required);
    }

    // TODO: this is not very clever - figure out something better
    template bool llama_model_loader::get_key_or_arr<std::array<int,      4>>  (enum llm_kv kid, std::array<int,      4>   & result, uint32_t n, bool required);
    template bool llama_model_loader::get_key_or_arr<std::array<uint32_t, 512>>(enum llm_kv kid, std::array<uint32_t, 512> & result, uint32_t n, bool required);
    template bool llama_model_loader::get_key_or_arr<std::array<float,    512>>(enum llm_kv kid, std::array<float,    512> & result, uint32_t n, bool required);


llama_model_loader::llama_model_loader(
        struct gguf_context * meta,
        llama_model_set_tensor_data_t set_tensor_data,
        void * set_tensor_data_ud,
        const std::string & fname,
        std::vector<std::string> & splits,
        FILE * file,
        llama_load_mode load_mode,
        bool check_tensors,
        bool no_alloc,
        bool load_mtp,
        const llama_model_kv_override * param_overrides_p,
        const llama_model_tensor_buft_override * param_tensor_buft_overrides_p)
        : metadata(meta), set_tensor_data(set_tensor_data), set_tensor_data_ud(set_tensor_data_ud) {
    int trace = 0;
    if (getenv("LLAMA_TRACE")) {
        trace = atoi(getenv("LLAMA_TRACE"));
    }

    if (param_overrides_p != nullptr) {
        for (const struct llama_model_kv_override * p = param_overrides_p; p->key[0] != 0; p++) {
            kv_overrides.insert({std::string(p->key), *p});
        }
    }

    tensor_buft_overrides = param_tensor_buft_overrides_p;

    this->use_mmap      = load_mode == LLAMA_LOAD_MODE_MMAP || load_mode == LLAMA_LOAD_MODE_MMAP_MLOCK || load_mode == LLAMA_LOAD_MODE_AUTO;
    this->use_direct_io = load_mode == LLAMA_LOAD_MODE_DIRECT_IO;

    if (!fname.empty()) {
        // Load the main GGUF
        struct ggml_context * ctx = NULL;
        struct gguf_init_params params = {
            /*.no_alloc = */ true,
            /*.ctx      = */ &ctx,
        };

        metadata_ptr.reset(gguf_init_from_file(fname.c_str(), params));
        metadata = metadata_ptr.get();
        if (metadata == nullptr) {
            throw std::runtime_error(format("%s: failed to load model from %s", __func__, fname.c_str()));
        }

        // The encoder's table needs the same store the GGUF check fills, so the check's own flag is not
        // enough: FG_TK_BPE_DIAG opens it too. The comparison is read-only and its verdict is the same one
        // the check would give, so nothing is weakened by running it for this reason.
        if (getenv("FG_GGUF_DIAG") != nullptr || getenv("FG_TK_BPE_DIAG") != nullptr) {
            // A disagreement here means the port and the native reader see the same file differently, which
            // is never acceptable: refuse the load rather than continue with an unverified reader.
            const int fg_diff = fg_gguf_check(fname, *metadata_ptr);
            if (fg_diff != 0) {
                throw std::runtime_error(format("%s: ported GGUF reader disagrees with the native one (%d)", __func__, fg_diff));
            }
        }
        get_key(llm_kv(LLM_KV_GENERAL_ARCHITECTURE), arch_name, false);
        llm_kv = LLM_KV(llm_arch_from_string(arch_name));

        files.emplace_back(new llama_file(fname.c_str(), "rb", use_direct_io));
        contexts.emplace_back(ctx);

        // Save tensors data offset of the main file.
        // For subsidiary files, `meta` tensor data offset must not be used,
        // so we build a unified tensors index for weights.
        for (ggml_tensor * cur = ggml_get_first_tensor(ctx); cur; cur = ggml_get_next_tensor(ctx, cur)) {
            std::string tensor_name = std::string(cur->name);
            // make sure there is no duplicated tensor names
            if (weights_map.find(tensor_name) != weights_map.end()) {
                throw std::runtime_error(format("invalid model: tensor '%s' is duplicated", ggml_get_name(cur)));
            }
            n_elements += ggml_nelements(cur);
            n_bytes    += ggml_nbytes(cur);
            weights_map.emplace(tensor_name, llama_tensor_weight(files.back().get(), 0, metadata, cur));
        }
        uint16_t n_split = 0;
        get_key(llm_kv(LLM_KV_SPLIT_COUNT), n_split, false);

        // Load additional GGML contexts
        if (n_split > 1) {
            // make sure the main file is loaded first
            uint16_t idx = 0;
            const std::string kv_split_no = llm_kv(LLM_KV_SPLIT_NO);
            get_key(kv_split_no, idx);
            if (idx != 0) {
                throw std::runtime_error(format("illegal split file idx: %d (file: %s), model must be loaded with the first split", idx, fname.c_str()));
            }

            // generate list of splits if needed
            if (splits.empty()) {
                splits = llama_get_list_splits(fname, idx, n_split);
            }

            // in case user give a custom list of splits, check if it matches the expected number
            if (n_split != (uint16_t)splits.size()) {
                throw std::runtime_error(format("invalid split count, given: %zu splits, but expected %d", splits.size(), n_split));
            }

            if (trace > 0) {
                LLAMA_LOG_INFO("%s: loading additional %d GGUFs\n", __func__, n_split);
            }

            // load other splits
            for (idx = 1; idx < n_split; idx++) {
                const char * fname_split = splits[idx].c_str();

                struct gguf_init_params split_params = {
                    /*.no_alloc = */ true,
                    /*.ctx      = */ &ctx,
                };
                gguf_context_ptr ctx_gguf { gguf_init_from_file(fname_split, split_params) };
                if (!ctx_gguf) {
                    throw std::runtime_error(format("%s: failed to load GGUF split from %s", __func__, fname_split));
                }

                // check idx
                {
                    const int kid = gguf_find_key(ctx_gguf.get(), kv_split_no.c_str());
                    if (kid < 0) {
                        throw std::runtime_error(format("missing key %s in GGUF split %s", kv_split_no.c_str(), fname_split));
                    }
                    int idx_gguf = gguf_get_val_u16(ctx_gguf.get(), kid);
                    if (idx_gguf != idx) {
                        throw std::runtime_error(format("invalid split file idx: %d (file: %s), expected %d", idx_gguf, fname_split, idx));
                    }
                }

                files.emplace_back(new llama_file(fname_split, "rb", use_direct_io));
                contexts.emplace_back(ctx);

                // Save tensors data offset info of the shard.
                for (ggml_tensor * cur = ggml_get_first_tensor(ctx); cur; cur = ggml_get_next_tensor(ctx, cur)) {
                    std::string tensor_name = std::string(cur->name);
                    // make sure there is no duplicated tensor names
                    if (weights_map.find(tensor_name) != weights_map.end()) {
                        throw std::runtime_error(format("invalid model: tensor '%s' is duplicated", ggml_get_name(cur)));
                    }
                    n_elements += ggml_nelements(cur);
                    n_bytes    += ggml_nbytes(cur);
                    weights_map.emplace(tensor_name, llama_tensor_weight(files.back().get(), idx, ctx_gguf.get(), cur));
                }
            }

            get_key(llm_kv(LLM_KV_SPLIT_TENSORS_COUNT), n_tensors);

            // sanity check
            {
                const int n_tensors_loaded = (int) weights_map.size();
                if (n_tensors != n_tensors_loaded) {
                    throw std::runtime_error(format("corrupted model: %d tensors expected but %d found", n_tensors, n_tensors_loaded));
                }
            }

            LLAMA_LOG_INFO("%s: additional %d GGUFs metadata loaded.\n",  __func__, n_split - 1);
        }
    } else if (file != nullptr) {
        struct ggml_context * ctx = NULL;
        struct gguf_init_params params = {
            /*.no_alloc = */ true,
            /*.ctx      = */ &ctx,
        };

        metadata_ptr.reset(gguf_init_from_file_ptr(file, params));
        metadata = metadata_ptr.get();
        if (metadata == nullptr) {
            throw std::runtime_error(format("%s: failed to load model from file pointer", __func__));
        }

        // mmap places tensors at their file offsets, so an embedded GGUF must be aligned in the file too
        const size_t tensor_align = ggml_backend_buft_get_alignment(ggml_backend_cpu_buffer_type());
        if (use_mmap && gguf_get_data_offset(metadata) % tensor_align != 0) {
            throw std::runtime_error(format("%s: GGUF data section at file offset %zu is not %zu byte aligned, cannot mmap",
                __func__, gguf_get_data_offset(metadata), tensor_align));
        }

        get_key(llm_kv(LLM_KV_GENERAL_ARCHITECTURE), arch_name, false);
        llm_kv = LLM_KV(llm_arch_from_string(arch_name));

        files.emplace_back(new llama_file(file));
        contexts.emplace_back(ctx);

        // Save tensors data offset info of the main file.
        for (ggml_tensor * cur = ggml_get_first_tensor(ctx); cur; cur = ggml_get_next_tensor(ctx, cur)) {
            std::string tensor_name = std::string(cur->name);
            // make sure there is no duplicated tensor names
            if (weights_map.find(tensor_name) != weights_map.end()) {
                throw std::runtime_error(format("invalid model: tensor '%s' is duplicated", ggml_get_name(cur)));
            }
            n_elements += ggml_nelements(cur);
            n_bytes    += ggml_nbytes(cur);
            weights_map.emplace(tensor_name, llama_tensor_weight(files.back().get(), 0, metadata, cur));
        }
    } else {
        get_key(llm_kv(LLM_KV_GENERAL_ARCHITECTURE), arch_name, false);
        llm_kv = LLM_KV(llm_arch_from_string(arch_name));
    }

    n_kv      = gguf_get_n_kv(metadata);
    n_tensors = weights_map.size();

    fver = (enum llama_fver) gguf_get_version(metadata);

    LLAMA_LOG_INFO("%s: loaded meta data with %d key-value pairs and %d tensors from %s (version %s)\n",
            __func__, n_kv, n_tensors, fname.empty() ? "(file*)" : fname.c_str(), llama_file_version_name(fver));

    // determine file type based on the number of tensors for each quantization and print meta data
    // TODO: make optional
    {
        std::map<enum ggml_type, uint32_t> n_type;

        uint32_t n_type_max = 0;
        enum ggml_type type_max = GGML_TYPE_F32;

        for (const auto & it : weights_map) {
            const llama_tensor_weight & w = it.second;
            const ggml_tensor * tensor = w.tensor;

            enum ggml_type type = tensor->type;

            n_type[type]++;

            if (n_type_max < n_type[type]) {
                n_type_max = n_type[type];
                type_max   = type;
            }

            if (trace > 0) {
                const uint16_t sid = w.idx;
                LLAMA_LOG_INFO("%s: - tensor split %2d: %32s %-8s [ %s ] %8.2f MiB\n", __func__,
                        sid, ggml_get_name(tensor), ggml_type_name(type), llama_format_tensor_shape(tensor).c_str(),
                        ggml_nbytes(tensor)/1024.0f/1024.0f);
            }
        }

        switch (type_max) {
            case GGML_TYPE_F32:     ftype = LLAMA_FTYPE_ALL_F32;        break;
            case GGML_TYPE_F16:     ftype = LLAMA_FTYPE_MOSTLY_F16;     break;
            case GGML_TYPE_BF16:    ftype = LLAMA_FTYPE_MOSTLY_BF16;    break;
            case GGML_TYPE_Q4_0:    ftype = LLAMA_FTYPE_MOSTLY_Q4_0;    break;
            case GGML_TYPE_Q4_1:    ftype = LLAMA_FTYPE_MOSTLY_Q4_1;    break;
            case GGML_TYPE_Q5_0:    ftype = LLAMA_FTYPE_MOSTLY_Q5_0;    break;
            case GGML_TYPE_Q5_1:    ftype = LLAMA_FTYPE_MOSTLY_Q5_1;    break;
            case GGML_TYPE_Q8_0:    ftype = LLAMA_FTYPE_MOSTLY_Q8_0;    break;
            case GGML_TYPE_Q2_K:    ftype = LLAMA_FTYPE_MOSTLY_Q2_K;    break;
            case GGML_TYPE_Q3_K:    ftype = LLAMA_FTYPE_MOSTLY_Q3_K_M;  break;
            case GGML_TYPE_Q4_K:    ftype = LLAMA_FTYPE_MOSTLY_Q4_K_M;  break;
            case GGML_TYPE_Q5_K:    ftype = LLAMA_FTYPE_MOSTLY_Q5_K_M;  break;
            case GGML_TYPE_Q6_K:    ftype = LLAMA_FTYPE_MOSTLY_Q6_K;    break;
            case GGML_TYPE_TQ1_0:   ftype = LLAMA_FTYPE_MOSTLY_TQ1_0;   break;
            case GGML_TYPE_TQ2_0:   ftype = LLAMA_FTYPE_MOSTLY_TQ2_0;   break;
            case GGML_TYPE_IQ2_XXS: ftype = LLAMA_FTYPE_MOSTLY_IQ2_XXS; break;
            case GGML_TYPE_IQ2_XS:  ftype = LLAMA_FTYPE_MOSTLY_IQ2_XS;  break;
            case GGML_TYPE_IQ2_S:   ftype = LLAMA_FTYPE_MOSTLY_IQ2_S;   break;
            case GGML_TYPE_IQ3_XXS: ftype = LLAMA_FTYPE_MOSTLY_IQ3_XXS; break;
            case GGML_TYPE_IQ1_S:   ftype = LLAMA_FTYPE_MOSTLY_IQ1_S;   break;
            case GGML_TYPE_IQ1_M:   ftype = LLAMA_FTYPE_MOSTLY_IQ1_M;   break;
            case GGML_TYPE_IQ4_NL:  ftype = LLAMA_FTYPE_MOSTLY_IQ4_NL;  break;
            case GGML_TYPE_IQ4_XS:  ftype = LLAMA_FTYPE_MOSTLY_IQ4_XS;  break;
            case GGML_TYPE_IQ3_S:   ftype = LLAMA_FTYPE_MOSTLY_IQ3_S;   break;
            case GGML_TYPE_NVFP4:   ftype = LLAMA_FTYPE_MOSTLY_NVFP4;   break;
            case GGML_TYPE_Q1_0:    ftype = LLAMA_FTYPE_MOSTLY_Q1_0;    break;
            case GGML_TYPE_Q2_0:    ftype = LLAMA_FTYPE_MOSTLY_Q2_0;    break;
            default:
                {
                    LLAMA_LOG_WARN("%s: unknown type %s\n", __func__, ggml_type_name(type_max));
                    ftype = LLAMA_FTYPE_ALL_F32;
                } break;
        }

        // this is a way to mark that we have "guessed" the file type
        ftype = (llama_ftype) (ftype | LLAMA_FTYPE_GUESSED);

        {
            uint32_t ftype_val = 0;
            if (get_key(LLM_KV_GENERAL_FILE_TYPE, ftype_val, false)) {
                ftype = (llama_ftype) ftype_val;
            }
        }

        LLAMA_LOG_INFO("%s: Dumping metadata keys/values. Note: KV overrides do not apply in this output.\n", __func__);

        for (int i = 0; i < n_kv; i++) {
            const char * name           = gguf_get_key(metadata, i);
            const enum gguf_type type   = gguf_get_kv_type(metadata, i);
            const std::string type_name =
                type == GGUF_TYPE_ARRAY
                ? format("%s[%s,%zu]", gguf_type_name(type), gguf_type_name(gguf_get_arr_type(metadata, i)), gguf_get_arr_n(metadata, i))
                : gguf_type_name(type);

            const size_t MAX_VALUE_LEN = 40;
            std::string value          = gguf_kv_to_str(metadata, i, MAX_VALUE_LEN);
            if (value.size() > MAX_VALUE_LEN) {
                value = format("%s...", value.substr(0, MAX_VALUE_LEN - 3).c_str());
            }
            replace_all(value, "\n", "\\n");

            LLAMA_LOG_INFO("%s: - kv %3d: %42s %-16s = %s\n", __func__, i, name, type_name.c_str(), value.c_str());
        }

        // print type counts
        for (auto & kv : n_type) {
            if (kv.second == 0) {
                continue;
            }

            LLAMA_LOG_INFO("%s: - type %4s: %4d tensors\n", __func__, ggml_type_name(kv.first), kv.second);
        }
    }

    if (this->use_mmap && !llama_mmap::SUPPORTED) {
        LLAMA_LOG_WARN("%s: mmap is not supported on this platform\n", __func__);
        this->use_mmap = false;
    }

    this->check_tensors = check_tensors;
    this->no_alloc = no_alloc;
    this->load_mtp = load_mtp;
}

std::string llama_model_loader::get_arch_name() const {
    return arch_name;
}

enum llm_arch llama_model_loader::get_arch() const {
    return llm_kv.arch;
}

const llama_model_loader::llama_tensor_weight * llama_model_loader::get_weight(const char * name) const {
    auto pos = weights_map.find(name);
    if (pos != weights_map.end()) {
        return &pos->second;
    }

    return nullptr;
}

const llama_model_loader::llama_tensor_weight & llama_model_loader::require_weight(const char * name) const {
    const llama_tensor_weight * weight = get_weight(name);
    if (!weight) {
        throw std::runtime_error(format("%s: tensor '%s' not found", __func__, name));
    }
    return *weight;
}

struct ggml_tensor * llama_model_loader::get_tensor_meta(const char * name) const {
    const auto * weight = get_weight(name);
    if (!weight) {
        return nullptr;
    }
    return weight->tensor;
}

struct ggml_tensor * llama_model_loader::require_tensor_meta(const std::string & name) const {
    struct ggml_tensor * tensor = get_tensor_meta(name.c_str());
    if (!tensor) {
        throw std::runtime_error(format("%s: tensor '%s' not found", __func__, name.c_str()));
    }
    return tensor;
}

const struct ggml_tensor * llama_model_loader::check_tensor_dims(
        const std::string & name,
        const std::vector<int64_t> & ne,
        bool required,
        bool allow_reshape) const {
    const struct ggml_tensor * cur = get_tensor_meta(name.c_str());

    if (cur == NULL) {
        if (!required) {
            return NULL;
        }
        throw std::runtime_error(format("%s: tensor '%s' not found", __func__, name.c_str()));
    }

    bool is_ok = true;

    if (allow_reshape) {
        // check total number of elements only
        const int64_t ncur = ggml_nelements(cur);
        int64_t nexp = 1;
        for (size_t i = 0; i < ne.size(); ++i) {
            nexp *= ne[i];
        }
        if (ncur != nexp) {
            is_ok = false;
        }
    } else {
        for (size_t i = 0; i < GGML_MAX_DIMS; ++i) {
            if ((i < ne.size() && ne[i] != cur->ne[i]) || (i >= ne.size() && cur->ne[i] != 1)) {
                is_ok = false;
                break;
            }
        }
    }

    if (!is_ok) {
        throw std::runtime_error(
                format("%s: tensor '%s' has wrong shape; expected %s, got %s",
                    __func__, name.c_str(),
                    llama_format_tensor_shape(ne).c_str(),
                    llama_format_tensor_shape(cur).c_str()));
    }

    return cur;
}

// checks if the weight tensor can be used with the specified buffer type and device
static bool weight_buft_supported(const llama_hparams & hparams, ggml_tensor * w, ggml_op op, ggml_backend_buffer_type_t buft, ggml_backend_dev_t dev) {
    GGML_ASSERT(w != nullptr);

    if (op == GGML_OP_NONE) {
        return true;
    }

    ggml_init_params params = {
        /*.mem_size   =*/ ggml_tensor_overhead()*8,
        /*.mem_buffer =*/ NULL,
        /*.no_alloc   =*/ true,
    };
    ggml_context_ptr ctx_ptr { ggml_init(params) };
    if (!ctx_ptr) {
        throw std::runtime_error(format("failed to create ggml context"));
    }
    ggml_context * ctx = ctx_ptr.get();

    ggml_tensor * op_tensor = nullptr;

    switch (op) {
        case GGML_OP_GET_ROWS:
            {
                ggml_tensor * b = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, 512);
                op_tensor = ggml_get_rows(ctx, w, b);
            } break;
        case GGML_OP_SET_ROWS:
            {
                // w holds the row indices (e.g. d2t)
                ggml_tensor * a = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, 1, w->ne[0]);
                ggml_tensor * b = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, 1, w->ne[0]);
                op_tensor = ggml_set_rows(ctx, a, b, w);
            } break;
        case GGML_OP_MUL_MAT:
            {
                ggml_tensor * b = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, w->ne[0], 512, w->ne[2], w->ne[3]);
                op_tensor = ggml_mul_mat(ctx, w, b);
            } break;
        case GGML_OP_MUL_MAT_ID:
            {
                // Used for either MoE expert routing or embedded adapter routing
                const int n_ids_used = hparams.router_layer >= 0 ? 1 : hparams.n_expert_used_max();
                GGML_ASSERT(n_ids_used > 0);
                ggml_tensor * b = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, w->ne[0], n_ids_used, 512);
                ggml_tensor * ids = ggml_new_tensor_2d(ctx, GGML_TYPE_I32, n_ids_used, 512);
                op_tensor = ggml_mul_mat_id(ctx, w, b, ids);
            } break;
        case GGML_OP_ADD:
            {
                ggml_tensor * a = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, w->ne[0], w->ne[1], w->ne[2], w->ne[3]);
                op_tensor = ggml_add(ctx, a, w);
            } break;
        case GGML_OP_ADD_ID:
            {
                const int n_expert_used = hparams.n_expert_used_max();
                GGML_ASSERT(n_expert_used > 0);
                ggml_tensor * a = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, w->ne[0], n_expert_used, 512);
                ggml_tensor * c = ggml_new_tensor_2d(ctx, GGML_TYPE_I32, n_expert_used, 512);
                op_tensor = ggml_add_id(ctx, a, w, c);
            } break;
        case GGML_OP_MUL:
            {
                ggml_tensor * a = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, w->ne[0], w->ne[1], w->ne[2], w->ne[3]);
                op_tensor = ggml_mul(ctx, a, w);
            } break;
        case GGML_OP_DIV:
            {
                ggml_tensor * a = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, w->ne[0]);
                op_tensor = ggml_div(ctx, a, w);
            } break;
        case GGML_OP_ROPE:
            {
                const int n_embd_head = hparams.n_embd_head_v();
                const int n_head = hparams.n_head();
                ggml_tensor * a = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, n_embd_head, n_head, 512);
                ggml_tensor * b = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, 512);
                op_tensor = ggml_rope_ext(
                    ctx, a, b, w,
                    0, 0, 0, 0, 0,
                    0, 0, 0, 0
                );

            } break;
        case GGML_OP_SSM_CONV:
            {
                const int64_t n_seq_tokens = 512;
                const int64_t n_seqs       = 3;
                ggml_tensor * conv_x = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, w->ne[0] - 1 + n_seq_tokens, w->ne[1], n_seqs);
                op_tensor = ggml_ssm_conv(ctx, conv_x, w);
            } break;
        case GGML_OP_SSM_SCAN:
            {
                // w is ssm_a, which is used to distinguish Mamba-1 and Mamba-2
                const int64_t d_state      = w->ne[0] == 1 ? hparams.ssm_d_state : w->ne[0];
                const int64_t n_head       = w->ne[1];
                const int64_t head_dim     = hparams.ssm_d_inner / n_head;
                const int64_t n_group      = hparams.ssm_n_group ? hparams.ssm_n_group : 1;
                const int64_t n_seq_tokens = 512;
                const int64_t n_seqs       = 3;
                ggml_tensor * s   = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, d_state, head_dim, n_head, n_seqs);
                ggml_tensor * x   = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, head_dim, n_head, n_seq_tokens, n_seqs);
                ggml_tensor * dt  = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, n_head, n_seq_tokens, n_seqs);
                ggml_tensor * B   = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, d_state, n_group, n_seq_tokens, n_seqs);
                ggml_tensor * C   = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, d_state, n_group, n_seq_tokens, n_seqs);
                ggml_tensor * ids = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, n_seqs);
                op_tensor = ggml_ssm_scan(ctx, s, x, dt, w, B, C, ids, /*K=*/1);
            } break;
        case GGML_OP_RWKV_WKV6:
            {
                // FIXME
                const int64_t S = 123;
                const int64_t H = 123;
                const int64_t n_tokens = 123;
                const int64_t n_seqs = 123;
                ggml_tensor  * k = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, S, H, n_tokens);
                ggml_tensor  * v = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, S, H, n_tokens);
                ggml_tensor  * r = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, S, H, n_tokens);
                ggml_tensor  * tf = w;
                ggml_tensor  * td = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, S, H, n_tokens);
                ggml_tensor  * state = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, S, n_seqs, S, H);
                op_tensor = ggml_rwkv_wkv6(ctx, k, v, r, tf, td, state);
            } break;
        case GGML_OP_IM2COL:
            {
                const int n_embd_inp = hparams.n_embd_inp();
                ggml_tensor * b = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, n_embd_inp, w->ne[1], 1, 1);
                op_tensor = ggml_im2col(ctx, w, b, 1, 0, 0, 0, 1, 0, false, GGML_TYPE_F16);
            } break;
        case GGML_OP_SCALE:
            {
                op_tensor = ggml_scale(ctx, w, 1.0f);
            } break;
        default:
            GGML_ABORT("%s: missing test for op %s for tensor %s", __func__, ggml_op_name(op), w->name);
    }

    // create a temporary dummy buffer for the weight so that supports_op can check the buffer type
    GGML_ASSERT(w->buffer == nullptr);
    w->buffer = ggml_backend_buft_alloc_buffer(buft, 0);
    bool op_supported = ggml_backend_dev_supports_op(dev, op_tensor);
    ggml_backend_buffer_free(w->buffer);
    w->buffer = nullptr;

    return op_supported;
}

// find the first buffer type in the list that can use the tensor
static ggml_backend_buffer_type_t select_weight_buft(const llama_hparams & hparams, ggml_tensor * tensor, ggml_op op, const buft_list_t * buft_list) {
    GGML_ASSERT(!buft_list->empty());
    for (const auto & cur : *buft_list) {
        ggml_backend_dev_t cur_dev = cur.first;
        ggml_backend_buffer_type_t cur_buft = cur.second;
        if (weight_buft_supported(hparams, tensor, op, cur_buft, cur_dev)) {
            return cur_buft;
        }
    }

    return nullptr;
}

ggml_backend_buffer_type_t llama_model_loader::lazy_read::buft() {
    auto * cpu_dev = ggml_backend_dev_by_type(GGML_BACKEND_DEVICE_TYPE_CPU);
    if (!cpu_dev) {
        throw std::runtime_error("no CPU backend found");
    }
    return ggml_backend_dev_buffer_type(cpu_dev);
}

bool llama_model_loader::lazy_read::add(const std::string & name, const ggml_tensor * t, const llama_tensor_weight * w) {
    if (mode == LLAMA_LAZY_MODE_OFF) {
        return false;
    }

    // do not lazy-read small tensors, it has significant overhead and is not worth it
    constexpr size_t auto_min_size = 4ull * 1024 * 1024 * 1024;
    if (mode != LLAMA_LAZY_MODE_ON && ggml_nbytes(t) <= auto_min_size) {
        return false;
    }

    if (!llama_mmap::SUPPORTED) {
        LLAMA_LOG_WARN("%s: mmap is not available, so tensor %s (size = %zu MiB) is loaded into RAM in full\n",
                __func__, name.c_str(), ggml_nbytes(t)/1024/1024);
        return false;
    }

    if (w) {
        ranges[w->idx].emplace_back(w->offs, w->offs + ggml_nbytes(t));
        tensors.insert(name);

        LLAMA_LOG_INFO("%s: tensor %s (size = %zu MiB) lazy read enabled\n",
                __func__, name.c_str(), ggml_nbytes(t)/1024/1024);
    }

    return true;
}

struct ggml_tensor * llama_model_loader::create_tensor(
        const llama_hparams & hparams, const buft_list_t * buft_list_cpu, const buft_list_t * buft_list_input, const buft_list_t * buft_list_output,
        const buft_list_t * buft_list_layer, const LLM_TN_IMPL & tn, const std::initializer_list<int64_t> & ne, int flags) {
    // set below, before buft_for_tensor() runs
    bool is_lazy = false;

    auto ctx_for_buft = [&](ggml_backend_buffer_type_t buft) -> ggml_context * {
        const ctx_key key { buft, is_lazy };

        auto it = ctx_map.find(key);
        if (it == ctx_map.end()) {
            // one ggml context per buffer type
            int max_n_tensors = n_tensors;
            max_n_tensors += 1;                   // duplicated output tensor
            max_n_tensors += hparams.n_layer()*2; // duplicated rope freq tensors
            if (files.empty()) {
                max_n_tensors += hparams.n_layer()*256; // this should be well above what any model actually uses
            }
            const size_t ctx_size = ggml_tensor_overhead()*max_n_tensors;

            ggml_init_params params = {
                /*.mem_size   =*/ ctx_size,
                /*.mem_buffer =*/ NULL,
                /*.no_alloc   =*/ true,
            };

            ggml_context * ctx = ggml_init(params);
            if (!ctx) {
                throw std::runtime_error(format("failed to create ggml context"));
            }

            ctx_map.emplace(key, ctx);

            return ctx;
        }
        return it->second.get();
    };

    auto buft_for_tensor = [&](ggml_tensor * t_meta) -> ggml_backend_buffer_type_t {
        if (!t_meta) {
            if (flags & TENSOR_NOT_REQUIRED) {
                return nullptr;
            }
            throw std::runtime_error(format("missing tensor '%s'", tn.str().c_str()));
        }

        // some models use the token embedding tensor as the output, but since these are used in different layers and with different ops
        // the tensor is duplicated
        // to handle this, we check if the tensor is duplicated, and if so, we assume that it is being loaded as the output tensor
        llm_tensor tn_tensor = tn.tensor;
        if (tn.tensor == LLM_TENSOR_TOKEN_EMBD && (flags & TENSOR_DUPLICATED)) {
            tn_tensor = LLM_TENSOR_OUTPUT;
        }

        llm_tensor_info info;
        try {
            info = llm_tensor_info_for(tn_tensor);
        } catch (const std::out_of_range & e) {
            throw std::runtime_error(format("missing tensor info mapping for %s", tn.str().c_str()));
        }

        // skip unused tensors
        if (info.op == GGML_OP_NONE || (flags & TENSOR_SKIP)) {
            const size_t nbytes = ggml_nbytes(t_meta);
            LLAMA_LOG_WARN("model has unused tensor %s (size = %zu bytes) -- ignoring\n", tn.str().c_str(), nbytes);

            size_data -= nbytes;
            n_created++;

            return nullptr;
        }

        // tensors with "bias" suffix are always used with GGML_OP_ADD or GGML_OP_ADD_ID;
        // embedded-adapter ".lora_a"/".lora_b" tensors are always used with GGML_OP_MUL_MAT_ID
        ggml_op op;
        if (tn.suffix != nullptr && strcmp(tn.suffix, "bias") == 0) {
            op = info.op == GGML_OP_MUL_MAT_ID ? GGML_OP_ADD_ID : GGML_OP_ADD;
        } else if (hparams.router_layer >= 0 && tn.suffix != nullptr &&
                (strcmp(tn.suffix, "lora_a") == 0 || strcmp(tn.suffix, "lora_b") == 0)) {
            op = GGML_OP_MUL_MAT_ID;
        } else {
            op = info.op;
        }

        // sanity checks
        if (info.layer == LLM_TENSOR_LAYER_INPUT || info.layer == LLM_TENSOR_LAYER_OUTPUT) {
            if (tn.bid != -1) {
                GGML_ABORT("input/output layer tensor %s used with a layer number", tn.str().c_str());
            }
        } else {
            if (tn.bid == -1) {
                GGML_ABORT("repeating layer tensor %s used without a layer number", tn.str().c_str());
            }
        }

        if (is_lazy) {
            return lazy_read::buft();
        }

        // select the buffer type for this tensor
        const buft_list_t * buft_list;
        switch (info.layer) {
            case LLM_TENSOR_LAYER_INPUT:
                buft_list = buft_list_input;
                break;
            case LLM_TENSOR_LAYER_OUTPUT:
                buft_list = buft_list_output;
                break;
            case LLM_TENSOR_LAYER_REPEATING:
                GGML_ASSERT(buft_list_layer != nullptr);
                buft_list = buft_list_layer;
                break;
            default:
                GGML_ABORT("invalid layer %d for tensor %s", info.layer, tn.str().c_str());
        }

        ggml_backend_buffer_type_t buft = nullptr;

        // check overrides
        if (tensor_buft_overrides) {
            std::string tensor_name = tn.str();
            for (const auto * overrides = tensor_buft_overrides; overrides->pattern != nullptr; ++overrides) {
                std::regex pattern(overrides->pattern);
                if (std::regex_search(tensor_name, pattern)) {
                    if (overrides->buft == ggml_backend_cpu_buffer_type()) {
                        // when overriding to a CPU buffer, consider the extra buffer types
                        buft = select_weight_buft(hparams, t_meta, op, buft_list_cpu);
                        if (use_mmap) {
                            static std::once_flag once;
                            std::call_once(once, [] {
                                LLAMA_LOG_WARN("llama_model_loader: tensor overrides to CPU are used with mmap enabled - consider using --load-mode none for better performance\n");
                            });
                        }
                    } else {
                        buft = overrides->buft;
                    }

                    LLAMA_LOG_DEBUG("tensor %s (%zu MiB %s) buffer type overridden to %s\n",
                            tensor_name.c_str(),
                            ggml_nbytes(t_meta) / 1024 / 1024, ggml_type_name(t_meta->type),
                            ggml_backend_buft_name(buft));
                    break;
                }
            }
        }

        if (!buft) {
            buft = select_weight_buft(hparams, t_meta, op, buft_list);
            if (!buft) {
                throw std::runtime_error(format("failed to find a compatible buffer type for tensor %s", tn.str().c_str()));
            }
        }

        // avoid using a host buffer when using mmap
        auto * buft_dev = ggml_backend_buft_get_device(buft);
        if (use_mmap && buft_dev && buft == ggml_backend_dev_host_buffer_type(buft_dev)) {
            auto * cpu_dev = ggml_backend_dev_by_type(GGML_BACKEND_DEVICE_TYPE_CPU);
            if (!cpu_dev) {
                throw std::runtime_error("no CPU backend found");
            }
            buft = ggml_backend_dev_buffer_type(cpu_dev);
        }

        if (buft != buft_list->front().second) {
            if (n_tensors_moved == 0) {
                first_tensor_moved_name = t_meta->name;
                first_tensor_moved_type_name = ggml_type_name(t_meta->type);
                first_moved_from_buft = buft_list->front().second;
                first_moved_to_buft   = buft;
            }
            n_tensors_moved++;
        }

        return buft;
    };

    if (files.empty()) {
        if (flags & TENSOR_SKIP_IF_VIRTUAL) {
            return nullptr;
        }
        ggml_type type = GGML_TYPE_F32;
        // The lookup and the type come from the port when it can answer; the native context is the fallback
        // for the split-file path, where the port has no records.
        const std::string fg_name = tn.str();
        int64_t tid = fg_port_find_tensor(fg_name.c_str(), (int64_t) fg_name.size());
        int64_t fg_type = 0;
        if (tid >= 0 && fg_port_tensor_type(tid, &fg_type)) {
            type = (ggml_type) fg_type;
        } else {
            tid = gguf_find_tensor(metadata, fg_name.c_str());
            if (tid != -1) {
                type = gguf_get_tensor_type(metadata, tid);
            }
        }

        // for tensors that are not required some of the dimensions can be invalid:
        if (flags & TENSOR_NOT_REQUIRED) {
            for (size_t dim = 0; dim < ne.size(); dim++) {
                if (ne.begin()[dim] <= 0) {
                    return nullptr;
                }
            }
        }

        ggml_tensor t_meta;
        memset(&t_meta, 0, sizeof(ggml_tensor));
        t_meta.type = type;
        for (size_t dim = 0; dim < GGML_MAX_DIMS; dim++) {
            t_meta.ne[dim] = dim < ne.size() ? ne.begin()[dim] : 1;
            GGML_ASSERT(t_meta.ne[dim] >= 1);
            if (dim == 0) {
                t_meta.nb[dim] = ggml_type_size(type);
            } else if (dim == 1) {
                t_meta.nb[dim] = ggml_row_size(type, t_meta.ne[dim-1]);
            } else {
                t_meta.nb[dim] = t_meta.nb[dim-1]*t_meta.ne[dim-1];
            }
            GGML_ASSERT(t_meta.nb[dim] >= 1);
        }
        ggml_set_name(&t_meta, tn.str().c_str());

        ggml_backend_buffer_type_t buft = buft_for_tensor(&t_meta);
        GGML_ASSERT(buft != nullptr);
        ggml_context * ctx = ctx_for_buft(buft);
        ggml_tensor * ret = ggml_dup_tensor(ctx, &t_meta);
        ggml_set_name(ret, tn.str().c_str());
        return ret;
    }

    LLAMA_LOG_DEBUG("%s: loading tensor %s\n", __func__, tn.str().c_str());
    const struct ggml_tensor * cur = check_tensor_dims(tn.str(), ne, !(flags & TENSOR_NOT_REQUIRED), flags & TENSOR_ALLOW_RESHAPE);
    if (cur == NULL) {
        return NULL;
    }

    if (flags & TENSOR_READ_LAZY) {
        // the decision must not depend on the load mode, or the memory-fit pass (no_alloc, no mmap)
        is_lazy = lazy.add(tn.str(), cur, no_alloc ? nullptr : &require_weight(tn.str().c_str()));
    }

    ggml_tensor t_meta = *cur;
    if (flags & TENSOR_ALLOW_RESHAPE) {
        for (size_t dim = 0; dim < GGML_MAX_DIMS; dim++) {
            t_meta.ne[dim] = dim < ne.size() ? ne.begin()[dim] : 1;
            if (dim == 0) {
                t_meta.nb[dim] = ggml_type_size(t_meta.type);
            } else if (dim == 1) {
                t_meta.nb[dim] = ggml_row_size(t_meta.type, t_meta.ne[dim-1]);
            } else {
                t_meta.nb[dim] = t_meta.ne[dim-1]*t_meta.nb[dim-1];
            }
        }
    }

    GGML_ASSERT(ggml_nbytes(&t_meta) == ggml_nbytes(cur));

    ggml_backend_buffer_type_t buft = buft_for_tensor(&t_meta);
    if (buft == nullptr) {
        return nullptr;
    }

    ggml_context * ctx = ctx_for_buft(buft);

    // if duplicated, check if the original tensor was allocated in the same buffer type context and avoid creating a new one
    if (flags & TENSOR_DUPLICATED) {
        ggml_tensor * t = ggml_get_tensor(ctx, tn.str().c_str());
        if (t) {
            return t;
        }
    }

    const bool duplicated = flags & TENSOR_DUPLICATED;

    struct ggml_tensor * tensor = ggml_dup_tensor(ctx, &t_meta);
    ggml_set_name(tensor, ggml_get_name(&t_meta));

    if (duplicated) {
        size_data += ggml_nbytes(&t_meta);
    } else {
        n_created++;
    }

    return tensor;
}

void llama_model_loader::done_getting_tensors(bool partial) const {
    if (n_created > n_tensors) {
        throw std::runtime_error(format("%s: too many tensors created; expected %d, got %d", __func__, n_tensors, n_created));
    }
    if (n_created < n_tensors) {
        if (!partial) {
            throw std::runtime_error(format("%s: wrong number of tensors; expected %d, got %d", __func__, n_tensors, n_created));
        }
        LLAMA_LOG_INFO("%s: partial load — used %d of %d tensors in the file (rest belong to a sibling model on the same .gguf)\n",
                __func__, n_created, n_tensors);
    }
    if (n_tensors_moved > 0) {
        LLAMA_LOG_DEBUG("%s: tensor '%s' (%s) (and %zu others) cannot be used with preferred buffer type %s, using %s instead\n",
            __func__, first_tensor_moved_name.c_str(), first_tensor_moved_type_name.c_str(), n_tensors_moved - 1,
            ggml_backend_buft_name(first_moved_from_buft), ggml_backend_buft_name(first_moved_to_buft));
    }
}

void llama_model_loader::init_mappings(bool prefetch, llama_mlocks * mlock_mmaps) {
    // note: read_lazy also requires mmap; this condition make sure it's usable even when --load-mode is not set to mmap
    if (use_mmap || lazy.any()) {
        mappings.reserve(files.size());
        mmaps_used.reserve(files.size());
        for (uint32_t idx = 0; idx < files.size(); idx++) {
            const auto & file = files[idx];

            bool is_numa = false;

            auto * dev = ggml_backend_dev_by_type(GGML_BACKEND_DEVICE_TYPE_CPU);
            if (dev) {
                auto * reg = ggml_backend_dev_backend_reg(dev);
                auto * is_numa_fn = (decltype(ggml_is_numa) *) ggml_backend_reg_get_proc_address(reg, "ggml_backend_cpu_is_numa");
                if (is_numa_fn) {
                    is_numa = is_numa_fn();
                }
            }

            const size_t prefetch_size = prefetch && use_mmap ? -1 : 0;

            std::unique_ptr<llama_mmap> mapping = std::make_unique<llama_mmap>(file.get(), prefetch_size, is_numa,
                    lazy.for_file(idx));
            mmaps_used.emplace_back(mapping->size(), 0);
            if (mlock_mmaps) {
                std::unique_ptr<llama_mlock> mlock_mmap(new llama_mlock());
                mlock_mmap->init(mapping->addr());
                mlock_mmaps->emplace_back(std::move(mlock_mmap));
            }
            mappings.emplace_back(std::move(mapping));
        }
    }

    // compute the total size of all tensors for progress reporting
    for (const auto & it : weights_map) {
        size_data += ggml_nbytes(it.second.tensor);
    }
}

using mapping_ranges = std::vector<llama_model_loader::mapping_range>;

// merge a list of [first, last) ranges into a sorted, non-overlapping list
static mapping_ranges merge_ranges(mapping_ranges ranges) {
    std::sort(ranges.begin(), ranges.end(), [](const auto & a, const auto & b) { return a.first < b.first; });

    mapping_ranges res;
    for (const auto & range : ranges) {
        if (!res.empty() && range.first <= res.back().last) {
            res.back().last = std::max(res.back().last, range.last);
        } else {
            res.push_back(range);
        }
    }

    return res;
}

// subtract the sorted, non-overlapping ranges 'sub' from 'a'
static mapping_ranges sub_ranges(const mapping_ranges & a, const mapping_ranges & sub) {
    mapping_ranges res;

    for (const auto & x : a) {
        size_t cur = x.first;
        for (const auto & y : sub) {
            if (y.last <= cur || y.first >= x.last) {
                continue;
            }
            if (y.first > cur) {
                res.push_back({ cur, y.first });
            }
            cur = std::max(cur, y.last);
        }
        if (cur < x.last) {
            res.push_back({ cur, x.last });
        }
    }

    return res;
}

std::vector<llama_model_loader::mapping_range> llama_model_loader::get_mapping_ranges(uint32_t idx, ggml_context * ctx) const {
    GGML_ASSERT(idx < mappings.size());

    // data of this context, and data of the other contexts that share the same file
    // a weight that is mapped in several contexts (e.g. the token embedding) is not foreign data
    std::unordered_set<const llama_tensor_weight *> own;
    mapping_ranges own_ranges;
    mapping_ranges other;

    for (ggml_tensor * tensor = ggml_get_first_tensor(ctx); tensor; tensor = ggml_get_next_tensor(ctx, tensor)) {
        const auto * weight = get_weight(ggml_get_name(tensor));
        if (!weight || weight->idx != idx) {
            continue;
        }
        own.insert(weight);
        own_ranges.push_back({ weight->offs, weight->offs + ggml_nbytes(tensor) });
    }

    for (const auto & it : ctx_map) {
        if (!it.second || it.second.get() == ctx) {
            continue;
        }

        for (ggml_tensor * tensor = ggml_get_first_tensor(it.second.get()); tensor; tensor = ggml_get_next_tensor(it.second.get(), tensor)) {
            const auto * weight = get_weight(ggml_get_name(tensor));
            if (!weight || weight->idx != idx || own.count(weight)) {
                continue;
            }
            other.push_back({ weight->offs, weight->offs + ggml_nbytes(tensor) });
        }
    }

    return sub_ranges(merge_ranges(std::move(own_ranges)), merge_ranges(std::move(other)));
}

void llama_model_loader::unmap_weight(const llama_tensor_weight & w) const {
    if (!use_mmap) { return; }
    mappings.at(w.idx)->unmap_fragment(w.offs, w.offs + ggml_nbytes(w.tensor));
}

const void * llama_model_loader::load_data_range(const llama_tensor_weight & w, size_t offs, size_t size, void * buf) const {
    GGML_ASSERT(offs + size <= ggml_nbytes(w.tensor));

    const void * data = buf;

    if (use_mmap) {
        data = (const uint8_t *) mappings.at(w.idx)->addr() + w.offs + offs;
    } else {
        GGML_ASSERT(buf != nullptr);
        GGML_ASSERT(w.idx < files.size());
        const auto & file = files.at(w.idx);
        file->seek(w.offs + offs, SEEK_SET);
        file->read_raw(buf, size);
    }

    if (check_tensors && !ggml_validate_row_data(w.tensor->type, data, size)) {
        throw std::runtime_error(format("tensor '%s' has invalid data", ggml_get_name(w.tensor)));
    }

    return data;
}

bool llama_model_loader::load_all_data(
        struct ggml_context * ctx,
        std::vector<ggml_backend_buffer_t> & bufs,
        llama_mlocks * lmlocks,
        llama_progress_callback progress_callback,
        void * progress_callback_user_data) {
    if (files.empty()) {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != nullptr; t = ggml_get_next_tensor(ctx, t)) {
            set_tensor_data(t, set_tensor_data_ud);
        }
        return true;
    }
    GGML_ASSERT(size_data != 0 && "call init_mappings() first");

    std::vector<std::future<std::pair<ggml_tensor *, bool>>> validation_result;

    // 4 staging buffers for async uploads, each sized 1MB seems to be a good default for single NVMe drives.
    // NVMe raid configurations might require more / larger buffers.
    constexpr size_t n_buffers = 4;

    size_t alignment = 1;
    for (const auto & file : files) {
        alignment = std::max(file->read_alignment(), alignment);
    }

    // Buffer size: balance between memory usage and I/O efficiency
    const size_t buffer_size = alignment != 1 ? LLAMA_DIRECT_IO_BUFFER_SIZE + 2 * alignment : 1 * 1024 * 1024;

    std::vector<ggml_backend_buffer_t> host_buffers;
    std::vector<ggml_backend_event_t> events;
    std::vector<void *> host_ptrs;
    size_t buffer_idx = 0; // buffer to use for async loads
    ggml_backend_t upload_backend = [&](const char * func) -> ggml_backend_t {
        if (use_mmap || check_tensors) {
            return nullptr;
        }
        // When not using mmaped io use async uploads from pinned memory to GPU memory.
        // First determine if the backend supports the necessary features for async uploads.
        auto * buf = bufs.empty() ? nullptr : bufs[0];
        if (!buf) {
            LLAMA_LOG_DEBUG("%s: no buffer found for async uploads\n", func);
            return nullptr;
        }

        auto * buft = ggml_backend_buffer_get_type(buf);
        auto * dev = ggml_backend_buft_get_device(buft);
        if (!dev) {
            LLAMA_LOG_DEBUG("%s: no device found for buffer type %s for async uploads\n", func,
                ggml_backend_buft_name(buft));
            return nullptr;
        }

        if (buft != ggml_backend_dev_buffer_type(dev)) {
            LLAMA_LOG_DEBUG("%s: buffer type %s is not the default buffer type for device %s for async uploads\n", func,
                ggml_backend_buft_name(buft), ggml_backend_dev_name(dev));
            return nullptr;
        }

        ggml_backend_dev_props props;
        ggml_backend_dev_get_props(dev, &props);
        if (!props.caps.async || !props.caps.host_buffer || !props.caps.events) {
            LLAMA_LOG_DEBUG("%s: device %s does not support async, host buffers or events\n", func,
                ggml_backend_dev_name(dev));
            return nullptr;
        }

        auto * host_buft = ggml_backend_dev_host_buffer_type(dev);
        if (!host_buft) {
            LLAMA_LOG_DEBUG("%s: no host buffer type found for device %s\n", func,
                ggml_backend_dev_name(dev));
            return nullptr;
        }

        // If the backend is supported, create pinned memory buffers and events for synchronisation.
        for (size_t idx = 0; idx < n_buffers; ++idx) {
            auto * buf = ggml_backend_buft_alloc_buffer(host_buft, buffer_size);

            if (!buf) {
                LLAMA_LOG_DEBUG("%s: failed to allocate host buffer for async uploads for device %s\n", func,
                    ggml_backend_dev_name(dev));
                return nullptr;
            }

            host_buffers.emplace_back(buf);
            host_ptrs.emplace_back(ggml_backend_buffer_get_base(buf));

            auto * event = ggml_backend_event_new(dev);
            if (!event) {
                LLAMA_LOG_DEBUG("%s: failed to create event for async uploads for device %s\n", func,
                    ggml_backend_dev_name(dev));
                return nullptr;
            }

            events.emplace_back(event);
        }

        ggml_backend_t backend = ggml_backend_dev_init(dev, nullptr);
        if (!backend) {
            LLAMA_LOG_DEBUG("%s: failed to initialize backend for device %s for async uploads\n", func,
                ggml_backend_dev_name(dev));
            return nullptr;
        }

        return backend;
    }(__func__);

    if (upload_backend) {
        LLAMA_LOG_DEBUG("%s: using async uploads for device %s, buffer type %s, backend %s\n", __func__,
            ggml_backend_dev_name(ggml_backend_get_device(upload_backend)),
            ggml_backend_buft_name(ggml_backend_buffer_get_type(bufs[0])),
            ggml_backend_name(upload_backend));
    }

    std::vector<ggml_tensor *> tensors;
    for (struct ggml_tensor * cur = ggml_get_first_tensor(ctx); cur != NULL; cur = ggml_get_next_tensor(ctx, cur)) {
        tensors.push_back(cur);
    }

    // without mmap, tensors in non-host buffers are staged through a temporary buffer sized like the tensor
    // load them biggest-first so the largest staging buffer is allocated while the fewest weights are resident
    if (!use_mmap) {
        std::stable_sort(tensors.begin(), tensors.end(), [](const ggml_tensor * a, const ggml_tensor * b) {
            const bool staged_a = a->buffer && !ggml_backend_buffer_is_host(a->buffer);
            const bool staged_b = b->buffer && !ggml_backend_buffer_is_host(b->buffer);
            if (staged_a != staged_b) {
                return staged_a;
            }
            return staged_a && ggml_nbytes(a) > ggml_nbytes(b);
        });
    }

    for (struct ggml_tensor * cur : tensors) {
        const auto * weight = get_weight(ggml_get_name(cur));
        if (weight == nullptr) {
            // this can happen with split experts models
            continue;
        }

        if (progress_callback) {
            if (!progress_callback((float) size_done / size_data, progress_callback_user_data)) {
                return false;
            }
        }

        size_t n_size = ggml_nbytes(cur);

        const bool from_mapping = use_mmap || lazy.has(cur);

        if (from_mapping) {
            const auto & mapping = mappings.at(weight->idx);
            uint8_t * data = (uint8_t *) mapping->addr() + weight->offs;

            // the tensors of a file can be spread across several buffers - find the one containing this tensor's data
            ggml_backend_buffer_t buf_mmap = nullptr;
            for (const auto & buf : bufs) {
                const uint8_t * base = (const uint8_t *) ggml_backend_buffer_get_base(buf);
                if (base && base <= data && data + n_size <= base + ggml_backend_buffer_get_size(buf)) {
                    buf_mmap = buf;
                    break;
                }
            }

            if (check_tensors) {
                validation_result.emplace_back(std::async(std::launch::async, [cur, data, n_size] {
                    return std::make_pair(cur, ggml_validate_row_data(cur->type, data, n_size));
                }));
            }

            GGML_ASSERT(buf_mmap || cur->data); // either we have a buffer to allocate the tensor in, or it is already allocated
            if (buf_mmap && cur->data == nullptr) {
                ggml_backend_tensor_alloc(buf_mmap, cur, data);

                // locking a lazy tensor would fault all of it in, which is what lazy avoids
                if (lmlocks && !lazy.has(cur)) {
                    const auto & lmlock = lmlocks->at(weight->idx);
                    lmlock->grow_to(weight->offs + n_size);
                }

                auto & mmap_used = mmaps_used[weight->idx];
                mmap_used.first  = std::min(mmap_used.first,  weight->offs);
                mmap_used.second = std::max(mmap_used.second, weight->offs + n_size);
            } else {
                ggml_backend_tensor_set(cur, data, 0, n_size);
            }
        } else {
            const auto & file = files.at(weight->idx);

            if (ggml_backend_buffer_is_host(cur->buffer)) {
                file->seek(weight->offs, SEEK_SET);
                file->read_raw(cur->data, n_size);
                if (check_tensors) {
                    validation_result.emplace_back(std::async(std::launch::async, [cur, n_size] {
                        return std::make_pair(cur, ggml_validate_row_data(cur->type, cur->data, n_size));
                    }));
                }
            } else {
                // If upload_backend is valid load the tensor in chunks to pinned memory and upload the buffers asynchronously to the GPU.
                if (upload_backend) {
                    size_t offset = weight->offs;
                    alignment = file->read_alignment();
                    size_t aligned_offset = offset & ~(alignment - 1);
                    size_t offset_from_alignment = offset - aligned_offset;
                    file->seek(aligned_offset, SEEK_SET);

                    // Calculate aligned read boundaries
                    size_t read_start = aligned_offset;
                    size_t read_end = (offset + n_size + alignment - 1) & ~(alignment - 1);

                    size_t bytes_read = 0;
                    size_t data_read = 0;  // Actual tensor data copied (excluding padding)

                    while (bytes_read < read_end - read_start) {
                        size_t read_size = std::min<size_t>(buffer_size, read_end - read_start - bytes_read);

                        // Align the destination pointer within the pinned buffer
                        uintptr_t ptr_dest_aligned = (reinterpret_cast<uintptr_t>(host_ptrs[buffer_idx]) + alignment - 1) & ~(alignment - 1);

                        // Wait for previous upload to complete before reusing buffer
                        ggml_backend_event_synchronize(events[buffer_idx]);

                        // Read aligned chunk from file
                        file->read_raw_unsafe(reinterpret_cast<void *>(ptr_dest_aligned), read_size);

                        // Calculate actual data portion (excluding alignment padding)
                        uintptr_t ptr_data = ptr_dest_aligned;
                        size_t data_to_copy = read_size;

                        // Skip alignment padding at start of first chunk
                        if (bytes_read == 0) {
                            ptr_data += offset_from_alignment;
                            data_to_copy -= offset_from_alignment;
                        }

                        // Trim alignment padding at end of last chunk
                        if (aligned_offset + bytes_read + read_size > offset + n_size) {
                            data_to_copy -= (read_end - (offset + n_size));
                        }

                        // Async upload actual data to GPU
                        ggml_backend_tensor_set_async(upload_backend, cur,
                                                      reinterpret_cast<void *>(ptr_data), data_read, data_to_copy);
                        ggml_backend_event_record(events[buffer_idx], upload_backend);

                        data_read += data_to_copy;
                        bytes_read += read_size;

                        ++buffer_idx;
                        buffer_idx %= n_buffers;
                    }
                } else {
                    // scoped to one tensor so only one staging buffer is alive at a time
                    std::vector<no_init<uint8_t>> read_buf(n_size);
                    file->seek(weight->offs, SEEK_SET);
                    file->read_raw(read_buf.data(), n_size);
                    ggml_backend_tensor_set(cur, read_buf.data(), 0, n_size);
                    if (check_tensors && !ggml_validate_row_data(cur->type, read_buf.data(), n_size)) {
                        throw std::runtime_error(format("tensor '%s' has invalid data", ggml_get_name(cur)));
                    }
                }
            }
        }

        size_done += n_size;
    }

    // free temporary resources used for async uploads
    for (auto * event : events) {
        ggml_backend_event_synchronize(event);
        ggml_backend_event_free(event);
    }
    for (auto * buf : host_buffers) {
        ggml_backend_buffer_free(buf);
    }
    ggml_backend_free(upload_backend);

    // check validation results
    bool validation_failed = false;
    for (auto & future : validation_result) {
        auto result = future.get();
        if (!result.second) {
            LLAMA_LOG_ERROR("%s: tensor '%s' has invalid data\n", __func__, ggml_get_name(result.first));
            validation_failed = true;
        }
    }
    if (validation_failed) {
        throw std::runtime_error("found tensors with invalid data");
    }

    // check if this is the last call and do final cleanup
    if (size_done >= size_data) {
        // unmap offloaded tensors and metadata
        if (use_mmap) {
            for (uint32_t idx = 0; idx < mappings.size(); idx++) {
                const auto & mmap_used = mmaps_used.at(idx);
                auto & mapping = mappings.at(idx);
                mapping->unmap_fragment(0, mmap_used.first);
                if (mmap_used.second != 0) {
                    mapping->unmap_fragment(mmap_used.second, mapping->size());
                }
            }
        }
        if (progress_callback) {
            // Even though the model is done loading, we still honor
            // cancellation since we need to free allocations.
            return progress_callback(1.0f, progress_callback_user_data);
        }
    }

    return true;
}

std::string llama_model_loader::ftype_name() const {
    return llama_ftype_name(ftype);
}

void llama_model_loader::print_info() const {
    LLAMA_LOG_INFO("%s: file format = %s\n", __func__, llama_file_version_name(fver));
    LLAMA_LOG_INFO("%s: file type   = %s\n", __func__, llama_ftype_name(ftype));
    if (n_bytes < GiB) {
        LLAMA_LOG_INFO("%s: file size   = %.2f MiB (%.2f BPW) \n", __func__, n_bytes/1024.0/1024.0,        n_bytes*8.0/n_elements);
    } else {
        LLAMA_LOG_INFO("%s: file size   = %.2f GiB (%.2f BPW) \n", __func__, n_bytes/1024.0/1024.0/1024.0, n_bytes*8.0/n_elements);
    }
}
