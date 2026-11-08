// bridge.cpp - per-instance typed storage for the Goose batch-allocr port.
// PROPOSAL ONLY for Codex. Not compiled, not integrated.
//
// No globals. One state_t per llama_batch_allocr instance. Temp vectors,
// helper vectors, ubatches and the unique-seq scratch set live inside the
// state and are released on state_free or at the next split reset (op 135).
// Handles are pointers owned by that state; a handle from another instance is
// rejected by the magic check.

#include "bridge.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <vector>

namespace {

constexpr uint64_t kStateMagic = 0x4631353342415354ull;  // "FG153BAST"
constexpr uint64_t kVecMagic   = 0x4631353342415643ull;  // "FG153BAC"
constexpr uint64_t kUbMagic    = 0x4631353342415542ull;  // "FG153BUB"

// Real values from the readable snapshots:
//   inputs/llama.h:39        #define LLAMA_TOKEN_NULL -1
//   inputs/llama-cparams.h:8 #define LLAMA_MAX_SEQ 256
constexpr int64_t kTokenNull = -1;
constexpr int64_t kMaxSeq    = 256;

struct vec_t {
    uint64_t magic = kVecMagic;
    std::vector<int64_t> v;
};

struct ubatch_t {
    uint64_t magic = kUbMagic;
    void * owner = nullptr;
    std::vector<int64_t> token, pos, nsq, output, type, decision, bidx;
    std::vector<int64_t> seqids, nseq_per, squnq, sqidx;
    std::vector<float> embd, state;
    int64_t nt = 0, nseq = 0, eq = 0;
};

struct state_t {
    uint64_t magic = kStateMagic;

    int64_t n_tok = 0, n_embd = 0, n_embd_state = 0, n_seq_max = 0, n_pos = 1;
    int64_t n_outputs = 0, n_used = 0;
    int64_t has_cpl = 0, mixed = 0, has_token = 0, has_embd = 0, has_state = 0;
    int64_t debug = 0;

    std::map<int64_t, vec_t> vecs;
    std::vector<float> embd, state;

    std::map<int64_t, std::set<int64_t>> tok_set;
    std::map<int64_t, std::set<int64_t>> seq_pos;
    std::set<std::pair<int64_t, int64_t>> cpl;
    std::map<int64_t, std::set<int64_t>> seqset;
    std::set<int64_t> unq;

    struct slot_t { int64_t rep = -1; std::vector<int64_t> idxs; };
    std::vector<slot_t> slots;

    std::vector<std::unique_ptr<vec_t>> temps;
    std::vector<std::unique_ptr<vec_t>> aux;
    std::vector<std::unique_ptr<ubatch_t>> ubs;
};

state_t * S(int64_t h) {
    auto * s = reinterpret_cast<state_t *>((uintptr_t) h);
    return (s && s->magic == kStateMagic) ? s : nullptr;
}
vec_t * V(int64_t h) {
    auto * v = reinterpret_cast<vec_t *>((uintptr_t) h);
    return (v && v->magic == kVecMagic) ? v : nullptr;
}
ubatch_t * U(int64_t h) {
    auto * u = reinterpret_cast<ubatch_t *>((uintptr_t) h);
    return (u && u->magic == kUbMagic) ? u : nullptr;
}
state_t * SU(ubatch_t * u) {
    return u ? S((int64_t) (uintptr_t) u->owner) : nullptr;
}
vec_t * new_vec(state_t * s) {
    s->temps.push_back(std::make_unique<vec_t>());
    return s->temps.back().get();
}
vec_t * new_aux(state_t * s) {
    s->aux.push_back(std::make_unique<vec_t>());
    return s->aux.back().get();
}

int64_t vlen(vec_t * v) { return v ? (int64_t) v->v.size() : 0; }
int64_t vget(vec_t * v, int64_t i) { return (v && i >= 0 && (size_t) i < v->v.size()) ? v->v[(size_t) i] : 0; }
void vset(vec_t * v, int64_t i, int64_t x) {
    if (!v || i < 0) return;
    if ((size_t) i >= v->v.size()) v->v.resize((size_t) i + 1, 0);
    v->v[(size_t) i] = x;
}

}  // namespace

extern "C" {

fg153_ba_state * fg153_ba_state_new(void) {
    return reinterpret_cast<fg153_ba_state *>(new state_t());
}

void fg153_ba_state_free(fg153_ba_state * s) {
    auto * st = reinterpret_cast<state_t *>(s);
    if (!st || st->magic != kStateMagic) return;
    st->magic = 0;
    delete st;
}

int64_t fg153_ba_get(int64_t a, int64_t k, int64_t i) {
    state_t * s = S(a);
    if (!s) return 0;
    switch (k) {
        case 0: return s->n_tok;
        case 1: return s->n_embd;
        case 2: return s->n_embd_state;
        case 3: return s->n_seq_max;
        case 4: return s->n_pos;
        case 5: return s->n_outputs;
        case 6: return s->n_used;
        case 7: return s->has_cpl;
        case 8: return s->mixed;
        case 9: return s->has_token;
        case 10: return s->has_embd;
        case 11: return s->has_state;
        case 12: return s->debug;
        default: return 0;
    }
}

void fg153_ba_put(int64_t a, int64_t k, int64_t i, int64_t v) {
    state_t * s = S(a);
    if (!s) return;
    switch (k) {
        case 0: s->n_tok = v; return;
        case 1: s->n_embd = v; return;
        case 2: s->n_embd_state = v; return;
        case 3: s->n_seq_max = v; return;
        case 4: s->n_pos = v; return;
        case 5: s->n_outputs = v; return;
        case 6: s->n_used = v; return;
        case 7: s->has_cpl = v; return;
        case 8: s->mixed = v; return;
        case 9: s->has_token = v; return;
        case 10: s->has_embd = v; return;
        case 11: s->has_state = v; return;
        case 12: s->debug = v; return;
        default: return;
    }
}

int64_t fg153_ba_call(int64_t op, int64_t a, int64_t b, int64_t c, int64_t d) {
    switch (op) {
        // diagnostics (0/1/2 sinks; constants are inlined in Goose from headers)
        case 0: case 1: case 2: case 3: case 4: return 0;
        default: break;
    }

    // ---- operations on a state handle ----
    if (state_t * s = S(a)) {
        switch (op) {
            case 20: return (int64_t) s->tok_set[b].size();
            case 21: { auto & t = s->tok_set[b]; if (c < 0 || (size_t) c >= t.size()) return -1; auto it = t.begin(); std::advance(it, c); return *it; }
            case 22: return s->tok_set[b].count(c) ? 1 : 0;
            case 23: s->tok_set[b].clear(); return 0;
            case 24: s->tok_set[b].insert(c); return 0;
            case 25: { auto & x = s->tok_set[b]; auto & y = s->tok_set[c]; for (auto v : x) if (y.count(v)) return 1; return 0; }
            case 26: { auto & x = s->tok_set[b]; auto & y = s->tok_set[c]; for (auto v : x) if (!y.count(v)) return 0; return 1; }
            case 27: return s->tok_set[b] == s->tok_set[c] ? 1 : 0;

            case 28: s->slots.clear(); return 0;
            case 29: {
                for (size_t i = 0; i < s->slots.size(); ++i)
                    if (s->slots[i].rep >= 0 && s->tok_set[s->slots[i].rep] == s->tok_set[b]) return (int64_t) i;
                s->slots.push_back({b, {}});
                return (int64_t) s->slots.size() - 1;
            }
            case 30: {
                for (size_t i = 0; i < s->slots.size(); ++i)
                    if (s->slots[i].rep >= 0 && s->tok_set[s->slots[i].rep] == s->tok_set[b]) return (int64_t) i;
                return -1;
            }
            case 31: { if (b < 0 || (size_t) b >= s->slots.size()) return 0; return (int64_t) s->slots[b].idxs.size(); }
            case 32: { if (b < 0 || (size_t) b >= s->slots.size()) return -1; auto & v = s->slots[b].idxs; if (c < 0 || (size_t) c >= v.size()) return -1; return v[c]; }
            case 33: { if (b < 0 || (size_t) b >= s->slots.size()) return 0; s->slots[b].idxs.push_back(c); return 0; }
            case 34: { if (b < 0 || (size_t) b >= s->slots.size()) return -1; return s->slots[b].rep; }

            case 35: s->seq_pos[b].clear(); return 0;
            case 36: s->seq_pos[b].insert(c); return 0;
            case 37: { auto & t = s->seq_pos[b]; return t.empty() ? -1 : *t.begin(); }
            case 38: { auto & t = s->seq_pos[b]; return t.empty() ? -1 : *t.rbegin(); }
            case 39: return s->seq_pos[b].empty() ? 1 : 0;
            case 40: return (int64_t) s->seq_pos[b].size();
            case 41: s->seq_pos.clear(); return 0;

            case 42: s->cpl.clear(); return 0;
            case 43: s->cpl.insert({b, c}); return 0;
            case 44: return s->cpl.count({b, c}) ? 1 : 0;

            case 45: {
                s->seqset.clear();
                for (int64_t q = 0; q < s->n_seq_max; ++q)
                    for (int64_t v = 0; v < s->n_seq_max; ++v) s->seqset[q].insert(v);
                return 0;
            }
            case 46: {
                auto & dst = s->seqset[b];
                auto & src = s->tok_set[c];
                std::set<int64_t> out;
                std::set_intersection(dst.begin(), dst.end(), src.begin(), src.end(),
                                      std::inserter(out, out.begin()));
                dst.swap(out);
                return 0;
            }
            case 47: return s->seqset[b].empty() ? 1 : 0;

            case 70: return (int64_t) s->vecs[b].v.size();
            case 71: return vget(&s->vecs[b], c);
            case 72: vset(&s->vecs[b], c, d); return 0;
            case 73: s->vecs[b].v.push_back(c); return 0;
            case 74: if (c >= 0) s->vecs[b].v.resize((size_t) c, d); return 0;
            case 75: s->vecs[b].v.clear(); return 0;

            case 80: {
                s->ubs.push_back(std::make_unique<ubatch_t>());
                s->ubs.back()->owner = s;
                return (int64_t) (uintptr_t) s->ubs.back().get();
            }
            case 101: {
                auto & nsq = s->vecs[25].v; auto & off = s->vecs[27].v; auto & flat = s->vecs[26].v;
                if (b < 0 || (size_t) b >= nsq.size() || nsq[(size_t) b] <= 0) return -1;
                if ((size_t) b >= off.size()) return -1;
                size_t k = (size_t) off[(size_t) b];
                return k < flat.size() ? flat[k] : -1;
            }
            case 111: return a;  // unique-seq scratch is per state
            case 112: case 118: s->unq.clear(); return 0;
            case 113: case 115: s->unq.insert(b); return 0;
            case 114: case 116: return s->unq.count(b) ? 1 : 0;

            case 120: case 121: case 126: {
                vec_t * v = new_aux(s);
                v->v.assign((size_t) std::max<int64_t>(b, 0), op == 126 ? -1 : c);
                return (int64_t) (uintptr_t) v;
            }

            case 129: {
                auto * x = reinterpret_cast<const fg153_ba_input *>((uintptr_t) b);
                if (!x || c < 0 || !x->tok_embd_off) return -1;
                if (c >= x->n_tok) return -1;
                size_t off = (size_t) x->tok_embd_off[c];
                size_t row = (size_t) c, w = (size_t) s->n_embd;
                if (!x->embd || (row + 1) * w > s->embd.size()) return -1;
                if (off + w > (size_t) x->n_tok * w) return -1;
                std::memcpy(s->embd.data() + row * w, x->embd + off, w * sizeof(float));
                return (int64_t) (w * sizeof(float));
            }
            case 130: {
                auto * x = reinterpret_cast<const fg153_ba_input *>((uintptr_t) b);
                size_t n = (size_t) std::max<int64_t>(s->n_embd * s->n_tok, 0);
                if (!x || !x->embd || s->embd.size() < n) return -1;
                std::memcpy(s->embd.data(), x->embd, n * sizeof(float));
                return (int64_t) (n * sizeof(float));
            }
            case 131: {
                auto * x = reinterpret_cast<const fg153_ba_input *>((uintptr_t) b);
                size_t n = (size_t) std::max<int64_t>(s->n_embd_state * s->n_tok, 0);
                if (!x || !x->state || s->state.size() < n) return -1;
                std::memcpy(s->state.data(), x->state, n * sizeof(float));
                return (int64_t) (n * sizeof(float));
            }
            case 135: s->temps.clear(); s->aux.clear(); return 0;
            default: break;
        }
    }

    // ---- input accessors (a = fg153_ba_input*) ----
    if (auto * x = reinterpret_cast<const fg153_ba_input *>((uintptr_t) a)) {
        switch (op) {
            case 50: return x->n_tok;
            case 51: return x->n_embd;
            case 52: return x->n_embd_inp;
            case 53: return x->n_embd_state;
            case 54: return x->n_seq_max;
            case 55: return x->n_vocab;
            case 56: return x->n_pos;
            case 57: return x->allow_mixed;
            case 58: return (x->tok_id && b >= 0 && b < x->n_tok) ? x->tok_id[b] : kTokenNull;
            case 59: return (x->tok_has_embd && b >= 0 && b < x->n_tok) ? x->tok_has_embd[b] : 0;
            case 60: return (x->tok_has_state && b >= 0 && b < x->n_tok) ? x->tok_has_state[b] : 0;
            case 61: return (x->tok_output && b >= 0 && b < x->n_tok) ? x->tok_output[b] : 0;
            case 62: return (x->tok_decision && b >= 0 && b < x->n_tok) ? x->tok_decision[b] : 0;
            case 63: {
                if (!x->tok_pos || b < 0 || b >= x->n_tok || c < 0 || c >= x->n_pos) return 0;
                return x->tok_pos[b * x->n_pos + c];
            }
            case 64: {
                if (!x->tok_seq_off || b < 0 || b + 1 > x->n_tok) return 0;
                return x->tok_seq_off[b + 1] - x->tok_seq_off[b];
            }
            case 65: {
                if (!x->tok_seq_off || !x->tok_seq_flat || b < 0 || b >= x->n_tok || c < 0) return -1;
                int64_t s = x->tok_seq_off[b] + c;
                if (s >= x->tok_seq_off[b + 1]) return -1;
                return x->tok_seq_flat[s];
            }
            case 66: return x->has_mem;
            case 67: return (x->mem_max && b >= 0 && b < x->n_seq_max) ? x->mem_max[b] : -1;
            case 68: return (x->mem_min && b >= 0 && b < x->n_seq_max) ? x->mem_min[b] : -1;
            default: break;
        }
    }

    // ---- temp / auxiliary vector handles ----
    if (vec_t * v = V(a)) {
        switch (op) {
            case 97: v->v.clear(); return 0;
            case 98: v->v.push_back(b); return 0;
            case 99: return (int64_t) v->v.size();
            case 100: case 103: case 107: case 109: return vget(v, b);
            case 104: case 106: vset(v, b, c); return 0;
            case 110: if (!v->v.empty()) v->v.pop_back(); return 0;
            case 122: case 125: case 127: return vget(v, b);
            case 123: case 124: case 128: vset(v, b, c); return 0;
            default: break;
        }
    }

    // ---- ubatch handles ----
    if (ubatch_t * u = U(a)) {
        state_t * s = SU(u);
        switch (op) {
            case 81: *u = ubatch_t{}; u->owner = s; return 0;
            case 82: {
                if (!s) return 0;
                u->nt = b;
                u->token.assign((size_t) std::max<int64_t>(b, 0), 0);
                u->pos.assign((size_t) std::max<int64_t>(b * s->n_pos, 0), 0);
                u->nsq.assign((size_t) std::max<int64_t>(b, 0), 0);
                u->output.assign((size_t) std::max<int64_t>(b, 0), 0);
                u->bidx.assign((size_t) std::max<int64_t>(b, 0), 0);
                u->embd.assign((size_t) std::max<int64_t>(c, 0), 0.0f);
                u->state.assign((size_t) std::max<int64_t>(d, 0), 0.0f);
                return 0;
            }
            case 83: u->type.assign((size_t) std::max<int64_t>(b, 0), 0); u->decision.assign((size_t) std::max<int64_t>(c, 0), 0); return 0;
            case 84: if (b >= 0 && (size_t) b < u->token.size()) u->token[(size_t) b] = c; return 0;
            case 85: if (b >= 0 && (size_t) b < u->pos.size()) u->pos[(size_t) b] = c; return 0;
            case 86: if (b >= 0 && (size_t) b < u->nsq.size()) u->nsq[(size_t) b] = c; return 0;
            case 87: if (b >= 0 && (size_t) b < u->output.size()) u->output[(size_t) b] = c; return 0;
            case 88: if (b >= 0 && (size_t) b < u->type.size()) u->type[(size_t) b] = c; return 0;
            case 89: if (b >= 0 && (size_t) b < u->decision.size()) u->decision[(size_t) b] = c; return 0;
            case 90: if (b >= 0 && (size_t) b < u->bidx.size()) u->bidx[(size_t) b] = c; return 0;
            case 91: {
                if (b < 0 || (size_t) b >= u->nsq.size()) return 0;
                u->seqids.push_back(c);
                if (u->nseq_per.size() < u->nsq.size()) u->nseq_per.resize(u->nsq.size(), 0);
                u->nseq_per[(size_t) b] += 1;
                return 0;
            }
            case 92: u->squnq.push_back(b); return 0;
            case 93: if (b >= 0) { if ((size_t) b >= u->sqidx.size()) u->sqidx.resize((size_t) b + 1, -1); u->sqidx[(size_t) b] = c; } return 0;
            case 94: u->nt = b; u->nseq = c; u->eq = d; return a;
            case 95: return u->nt;
            case 117: return (int64_t) u->squnq.size();
            case 119: {
                if (!s) return -1;
                size_t w = (size_t) s->n_embd;
                if (b < 0 || c < 0) return -1;
                size_t so = (size_t) b * w, dofs = (size_t) c * w;
                if (so + w > s->embd.size() || dofs + w > u->embd.size()) return -1;
                std::memcpy(u->embd.data() + dofs, s->embd.data() + so, w * sizeof(float));
                return (int64_t) (w * sizeof(float));
            }
            case 132: {
                if (!s) return -1;
                size_t w = (size_t) s->n_embd_state;
                if (b < 0 || c < 0) return -1;
                size_t so = (size_t) b * w, dofs = (size_t) c * w;
                if (so + w > s->state.size() || dofs + w > u->state.size()) return -1;
                std::memcpy(u->state.data() + dofs, s->state.data() + so, w * sizeof(float));
                return (int64_t) (w * sizeof(float));
            }
            default: break;
        }
    }

    // ---- creation ops need a state handle (a = state) ----
    if (state_t * s = S(a)) {
        switch (op) {
            case 96: case 102: case 105: {
                vec_t * v = new_vec(s);
                if (op != 96) v->v.assign((size_t) std::max<int64_t>(b, 0), 0);
                return (int64_t) (uintptr_t) v;
            }
            default: break;
        }
    }

    return 0;
}

int64_t fg153_ba_rcopy(int64_t a, int64_t src_kind, int64_t src_row, int64_t dst_row) {
    state_t * s = S(a);
    if (!s) return -1;
    const int64_t width = src_kind == 0 ? s->n_embd : s->n_embd_state;
    std::vector<float> & buf = src_kind == 0 ? s->embd : s->state;
    if (width <= 0 || src_row < 0 || dst_row < 0) return -1;
    size_t so = (size_t) src_row * (size_t) width, dofs = (size_t) dst_row * (size_t) width;
    if (so + (size_t) width > buf.size() || dofs + (size_t) width > buf.size()) return -1;
    if (so == dofs) return 0;
    std::memmove(buf.data() + dofs, buf.data() + so, (size_t) width * sizeof(float));
    return (int64_t) (width * sizeof(float));
}

void fg153_ba_witness(int64_t mode) { (void) mode; }

int64_t fg153_ba_ub_n(int64_t h, int64_t key) {
    ubatch_t * u = U(h);
    if (!u) return 0;
    switch (key) {
        case 0: return u->nt;
        case 1: return u->nseq;
        case 2: return u->nseq > 0 ? u->nt / u->nseq : 0;
        case 3: return u->eq;
        case 4: return (int64_t) u->seqids.size();
        case 5: return (int64_t) u->nsq.size();
        case 6: return (int64_t) u->sqidx.size();
        case 7: return (int64_t) u->squnq.size();
        default: return 0;
    }
}

int64_t fg153_ba_ub_i(int64_t h, int64_t key, int64_t idx) {
    ubatch_t * u = U(h);
    if (!u || idx < 0) return -1;
    const std::vector<int64_t> * v = nullptr;
    switch (key) {
        case 0: v = &u->token; break;
        case 1: v = &u->pos; break;
        case 2: v = &u->nsq; break;
        case 3: v = &u->output; break;
        case 4: v = &u->type; break;
        case 5: v = &u->decision; break;
        case 6: v = &u->bidx; break;
        case 7: v = &u->squnq; break;
        case 8: v = &u->sqidx; break;
        case 9: v = &u->seqids; break;
        default: return -1;
    }
    if ((size_t) idx >= v->size()) return -1;
    return (*v)[(size_t) idx];
}

const float * fg153_ba_ub_embd(int64_t h, int64_t * n) {
    ubatch_t * u = U(h);
    if (!u) return nullptr;
    if (n) *n = (int64_t) u->embd.size();
    return u->embd.empty() ? nullptr : u->embd.data();
}

const float * fg153_ba_ub_state(int64_t h, int64_t * n) {
    ubatch_t * u = U(h);
    if (!u) return nullptr;
    if (n) *n = (int64_t) u->state.size();
    return u->state.empty() ? nullptr : u->state.data();
}

int64_t fg153_ba_import_embd(int64_t a, int64_t src_ptr, int64_t n_floats) {
    state_t * s = S(a);
    const float * src = reinterpret_cast<const float *>((uintptr_t) src_ptr);
    if (!s || !src || n_floats < 0) return -1;
    s->embd.assign(src, src + n_floats);
    return n_floats;
}

int64_t fg153_ba_import_state(int64_t a, int64_t src_ptr, int64_t n_floats) {
    state_t * s = S(a);
    const float * src = reinterpret_cast<const float *>((uintptr_t) src_ptr);
    if (!s || !src || n_floats < 0) return -1;
    s->state.assign(src, src + n_floats);
    return n_floats;
}

const float * fg153_ba_state_embd(int64_t a, int64_t * n) {
    state_t * s = S(a);
    if (!s) return nullptr;
    if (n) *n = (int64_t) s->embd.size();
    return s->embd.empty() ? nullptr : s->embd.data();
}

const float * fg153_ba_state_state(int64_t a, int64_t * n) {
    state_t * s = S(a);
    if (!s) return nullptr;
    if (n) *n = (int64_t) s->state.size();
    return s->state.empty() ? nullptr : s->state.data();
}

void fg153_ba_ub_release(int64_t h) {    ubatch_t * u = U(h);
    if (!u) return;
    state_t * s = SU(u);
    if (!s) return;
    u->magic = 0;
    for (auto it = s->ubs.begin(); it != s->ubs.end(); ++it) {
        if (it->get() == u) { s->ubs.erase(it); return; }
    }
}

}  // extern "C"
