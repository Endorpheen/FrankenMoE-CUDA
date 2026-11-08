// worker-2/storage-bridge.cpp
//
// Opaque storage bridge for the Goose module worker-2/cells.goose.
//
// This file deliberately implements ONLY:
//   * a flat slot get/put store for cell scalars and per-sequence bits,
//   * generic ordered-set primitives over std::set<int64_t>.
//
// It does NOT implement or call any llama_kv_cells control method. All
// container policy (when to insert/erase, ordering key composition,
// branching) lives in cells.goose. No llama_kv_cells private layout is
// touched; the adapter owns its own cells_storage class.
//
// UNVERIFIED_PROPOSAL. Not integrated. Parent must wire configure(),
// build against the real toolchain, and run the differential fixture.

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <set>
#include <utility>
#include <unordered_set>
#include <unordered_map>

#include "llama.h"

namespace {

struct oset {
    enum kind_t { OWN = 0, USED = 1, SEQPOS = 2 };
    kind_t kind = OWN;
    std::set<int64_t> * own = nullptr;
    std::set<uint32_t> * used = nullptr;
    std::set<std::pair<llama_pos, uint32_t>> * seqpos = nullptr;

    struct itv {
        kind_t kind = OWN;
        std::set<int64_t>::iterator a;
        std::set<uint32_t>::iterator b;
        std::set<std::pair<llama_pos, uint32_t>>::iterator c;
    };
    // Iterator handles are 1-based indices into this table. Entries are never
    // removed; a handle is only dereferenced while its element is alive.
    std::vector<itv> its;

    int64_t save(itv v) { its.push_back(v); return static_cast<int64_t>(its.size()); }
    itv & load(int64_t h) { return its.at(static_cast<size_t>(h - 1)); }
    size_t size() const {
        if (kind == OWN)   { return own ? own->size() : 0; }
        if (kind == USED)  { return used ? used->size() : 0; }
        return seqpos ? seqpos->size() : 0;
    }
};

int64_t pair_key(int64_t pos, uint32_t cell) {
    return pos * 4294967296LL + (int64_t) cell;
}
std::pair<llama_pos, uint32_t> key_pair(int64_t key) {
    const int64_t pos = key / 4294967296LL;
    return { (llama_pos) pos, (uint32_t) (key - pos * 4294967296LL) };
}

struct cells_storage {
    int64_t n_cells = 0;
    int64_t has_shift = 0;
    int64_t max_seq = 0;

    std::vector<int64_t> pos;
    std::vector<int64_t> shift;
    std::vector<int64_t> ext_x;
    std::vector<int64_t> ext_y;
    std::vector<int64_t> ext_tok;

    // seq[s][i] is 0/1 membership (bitset-like native structure).
    std::vector<std::vector<uint8_t>> seq;

    // Handles owned by the Goose side; this adapter only stores them.
    int64_t used_handle = 0;
    std::vector<int64_t> seq_pos_handle;
};

int64_t g_max_seq = 0;
int64_t g_token_null = 0;

// Attached REAL llama_kv_cells handles. Implemented in kv-native-adapter.cpp;
// weak so the standalone store links and runs without the native build.
std::unordered_set<int64_t> g_native_handles;
std::unordered_map<int64_t, void *> g_native_ptrs;
int64_t g_native_next = 1;

inline bool is_native(int64_t p) {
    return g_native_handles.find(p) != g_native_handles.end();
}

inline cells_storage * as_cells(int64_t p) {
    return reinterpret_cast<cells_storage *>(static_cast<intptr_t>(p));
}
inline oset * as_set(int64_t h) {
    if (h == 0) {
        std::fprintf(stderr, "fg_w2: null ordered-set handle\n");
        std::abort();
    }
    return reinterpret_cast<oset *>(static_cast<intptr_t>(h));
}
inline int64_t to_handle(void * ptr) {
    return static_cast<int64_t>(reinterpret_cast<intptr_t>(ptr));
}

} // namespace

extern "C" {

// Parent supplies the two compile-time values; the native build passes
// LLAMA_MAX_SEQ / LLAMA_TOKEN_NULL from llama.h.
void fg_w2_configure(int64_t max_seq, int64_t token_null) {
    g_max_seq = max_seq;
    g_token_null = token_null;
}

int64_t fg_w2_native_get(int64_t p, int64_t k, int64_t i) __attribute__((weak));
void    fg_w2_native_forget(int64_t p) __attribute__((weak));
void    fg_w2_native_put(int64_t p, int64_t k, int64_t i, int64_t v) __attribute__((weak));

// Register a real llama_kv_cells* as an opaque handle. The native adapter owns
// the pointer; destruction only detaches the handle.
int64_t fg_w2_attach_cells(void * opaque) {
    const int64_t id = g_native_next++;
    g_native_handles.insert(id);
    g_native_ptrs[id] = opaque;
    return id;
}
void * fg_w2_native_ptr(int64_t id) {
    auto it = g_native_ptrs.find(id);
    return it == g_native_ptrs.end() ? nullptr : it->second;
}

// Wrap a REAL std::set as a generic ordered set. kind 1 = set<uint32_t>,
// kind 2 = set<pair<pos,cell>>; the pointed-to set stays owned by the cache.
int64_t fg_w2_set_wrap(int64_t kind, void * a) {
    oset * s = new oset();
    if (kind == 1) { s->kind = oset::USED;   s->used   = static_cast<std::set<uint32_t> *>(a); }
    else           { s->kind = oset::SEQPOS; s->seqpos = static_cast<std::set<std::pair<llama_pos, uint32_t>> *>(a); }
    return to_handle(s);
}

int64_t fg_w2_get(int64_t p, int64_t k, int64_t i) {
    if (p == 0) {
        if (k == 900) { return g_max_seq; }
        if (k == 901) { return g_token_null; }
        std::fprintf(stderr, "fg_w2_get: bad global key %lld\n", (long long) k);
        std::abort();
    }
    if (is_native(p)) {
        if (fg_w2_native_get == nullptr) { std::abort(); }
        return fg_w2_native_get(p, k, i);
    }
    cells_storage * c = as_cells(p);
    if (k == 0)  { return c->n_cells; }
    if (k == 1)  { return c->has_shift; }
    if (k == 10) { return c->pos.at(static_cast<size_t>(i)); }
    if (k == 11) { return c->shift.at(static_cast<size_t>(i)); }
    if (k == 12) { return c->ext_x.at(static_cast<size_t>(i)); }
    if (k == 13) { return c->ext_y.at(static_cast<size_t>(i)); }
    if (k == 14) { return c->ext_tok.at(static_cast<size_t>(i)); }
    if (k == 20) { return c->used_handle; }
    if (k == 25) { return c->seq_pos_handle.at(static_cast<size_t>(i)); }
    if (k > 25 && k < 25 + c->max_seq) { return c->seq_pos_handle.at(static_cast<size_t>(k - 25)); }
    if (k >= 1000) {
        const size_t s = static_cast<size_t>(k - 1000);
        if (s < c->seq.size()) {
            return static_cast<int64_t>(c->seq[s].at(static_cast<size_t>(i)));
        }
    }
    std::fprintf(stderr, "fg_w2_get: bad key %lld\n", (long long) k);
    std::abort();
}

void fg_w2_put(int64_t p, int64_t k, int64_t i, int64_t v) {
    if (p == 0) {
        std::fprintf(stderr, "fg_w2_put: globals are read-only\n");
        std::abort();
    }
    if (is_native(p)) {
        if (fg_w2_native_put == nullptr) { std::abort(); }
        fg_w2_native_put(p, k, i, v);
        return;
    }
    cells_storage * c = as_cells(p);
    if (k == 0)  { c->n_cells = v; return; }
    if (k == 1)  { c->has_shift = v; return; }
    if (k == 10) { c->pos.at(static_cast<size_t>(i)) = v; return; }
    if (k == 11) { c->shift.at(static_cast<size_t>(i)) = v; return; }
    if (k == 12) { c->ext_x.at(static_cast<size_t>(i)) = v; return; }
    if (k == 13) { c->ext_y.at(static_cast<size_t>(i)) = v; return; }
    if (k == 14) { c->ext_tok.at(static_cast<size_t>(i)) = v; return; }
    if (k == 20) { c->used_handle = v; return; }
    if (k == 25) { c->seq_pos_handle.at(static_cast<size_t>(i)) = v; return; }
    if (k > 25 && k < 25 + c->max_seq) { c->seq_pos_handle.at(static_cast<size_t>(k - 25)) = v; return; }
    if (k >= 1000) {
        const size_t s = static_cast<size_t>(k - 1000);
        if (s < c->seq.size()) {
            c->seq[s].at(static_cast<size_t>(i)) = static_cast<uint8_t>(v != 0);
            return;
        }
    }
    std::fprintf(stderr, "fg_w2_put: bad key %lld\n", (long long) k);
    std::abort();
}

// service: 0 trap, 1 alloc cells storage, 2 free cells storage.
// Ordered sets are created/destroyed by Goose through fg_w2_set_*; the
// adapter does not free them here, so ownership stays explicit in Goose.
int64_t fg_w2_service(int64_t op, int64_t a, int64_t b) {
    (void) b;
    switch (op) {
        case 0:
            std::fprintf(stderr, "fg_w2: Goose invariant trap\n");
            std::abort();
        case 1: {
            if (a < 0) { std::abort(); }
            if (g_max_seq <= 0) {
                std::fprintf(stderr, "fg_w2: configure(max_seq, token_null) must run first\n");
                std::abort();
            }
            cells_storage * c = new cells_storage();
            c->n_cells = a;
            c->has_shift = 0;
            c->max_seq = g_max_seq;
            const size_t n = static_cast<size_t>(a);
            c->pos.assign(n, -1);
            c->shift.assign(n, 0);
            c->ext_x.assign(n, 0);
            c->ext_y.assign(n, 0);
            c->ext_tok.assign(n, g_token_null);
            c->seq.assign(static_cast<size_t>(g_max_seq), std::vector<uint8_t>(n, 0));
            c->used_handle = 0;
            c->seq_pos_handle.assign(static_cast<size_t>(g_max_seq), 0);
            return to_handle(c);
        }
        case 2: {
            if (a == 0) { return 0; }
            if (is_native(a)) {
                g_native_handles.erase(a);
                g_native_ptrs.erase(a);
                if (fg_w2_native_forget) { fg_w2_native_forget(a); }
                return 0; // the real llama_kv_cells stays owned by the cache
            }
            delete as_cells(a);
            return 0;
        }
        default:
            std::fprintf(stderr, "fg_w2_service: bad op %lld\n", (long long) op);
            std::abort();
    }
}

void fg_w2_witness(int64_t mode) {
    (void) mode; // fixture harness may log externally; module stays silent
}

// ---- opaque ordered set primitives over owned or attached std::set ----

int64_t fg_w2_set_create(void) {
    oset * s = new oset();
    s->kind = oset::OWN;
    s->own = new std::set<int64_t>();
    return to_handle(s);
}

void fg_w2_set_destroy(int64_t h) {
    if (h == 0) { return; }
    oset * s = as_set(h);
    // Attached wrappers must outlive the cache: keep them alive so a cached
    // handle remains valid after reset()/destroy() paths in Goose.
    if (s->kind != oset::OWN) { return; }
    delete s->own;
    delete s;
}

int64_t fg_w2_set_insert(int64_t h, int64_t key) {
    oset * s = as_set(h);
    if (s->kind == oset::OWN)  { return s->own->insert(key).second ? 1 : 0; }
    if (s->kind == oset::USED) { return s->used->insert((uint32_t) key).second ? 1 : 0; }
    return s->seqpos->insert(key_pair(key)).second ? 1 : 0;
}

int64_t fg_w2_set_erase(int64_t h, int64_t key) {
    oset * s = as_set(h);
    if (s->kind == oset::OWN)  { return (int64_t) s->own->erase(key); }
    if (s->kind == oset::USED) { return (int64_t) s->used->erase((uint32_t) key); }
    return (int64_t) s->seqpos->erase(key_pair(key));
}

int64_t fg_w2_set_lower_bound(int64_t h, int64_t key) {
    oset * s = as_set(h);
    oset::itv v;
    v.kind = s->kind;
    if (s->kind == oset::OWN) {
        v.a = s->own->lower_bound(key);
        if (v.a == s->own->end()) { return 0; }
    } else if (s->kind == oset::USED) {
        v.b = s->used->lower_bound((uint32_t) key);
        if (v.b == s->used->end()) { return 0; }
    } else {
        v.c = s->seqpos->lower_bound(key_pair(key));
        if (v.c == s->seqpos->end()) { return 0; }
    }
    return s->save(v);
}

int64_t fg_w2_set_count(int64_t h, int64_t key) {
    oset * s = as_set(h);
    if (s->kind == oset::OWN)  { return (int64_t) s->own->count(key); }
    if (s->kind == oset::USED) { return (int64_t) s->used->count((uint32_t) key); }
    return (int64_t) s->seqpos->count(key_pair(key));
}

int64_t fg_w2_set_first(int64_t h) {
    oset * s = as_set(h);
    if (s->size() == 0) { return 0; }
    oset::itv v;
    v.kind = s->kind;
    if (s->kind == oset::OWN)  { v.a = s->own->begin(); }
    else if (s->kind == oset::USED) { v.b = s->used->begin(); }
    else { v.c = s->seqpos->begin(); }
    return s->save(v);
}

int64_t fg_w2_set_last(int64_t h) {
    oset * s = as_set(h);
    if (s->size() == 0) { return 0; }
    oset::itv v;
    v.kind = s->kind;
    if (s->kind == oset::OWN)  { auto j = s->own->end(); --j; v.a = j; }
    else if (s->kind == oset::USED) { auto j = s->used->end(); --j; v.b = j; }
    else { auto j = s->seqpos->end(); --j; v.c = j; }
    return s->save(v);
}

int64_t fg_w2_set_next(int64_t h, int64_t it) {
    oset * s = as_set(h);
    oset::itv v = s->load(it);
    oset::itv n = v;
    if (v.kind == oset::OWN) {
        ++n.a; if (n.a == s->own->end()) { return 0; }
    } else if (v.kind == oset::USED) {
        ++n.b; if (n.b == s->used->end()) { return 0; }
    } else {
        ++n.c; if (n.c == s->seqpos->end()) { return 0; }
    }
    return s->save(n);
}

int64_t fg_w2_set_prev(int64_t h, int64_t it) {
    oset * s = as_set(h);
    oset::itv v = s->load(it);
    oset::itv n = v;
    if (v.kind == oset::OWN) {
        if (n.a == s->own->begin()) { return 0; }
        --n.a;
    } else if (v.kind == oset::USED) {
        if (n.b == s->used->begin()) { return 0; }
        --n.b;
    } else {
        if (n.c == s->seqpos->begin()) { return 0; }
        --n.c;
    }
    return s->save(n);
}

int64_t fg_w2_set_size(int64_t h) {
    return (int64_t) as_set(h)->size();
}

int64_t fg_w2_set_key(int64_t h, int64_t it) {
    oset * s = as_set(h);
    oset::itv & v = s->load(it);
    if (v.kind == oset::OWN)  { return *v.a; }
    if (v.kind == oset::USED) { return (int64_t) *v.b; }
    return pair_key((int64_t) v.c->first, v.c->second);
}

} // extern "C"
