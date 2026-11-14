// Differential gate for P9-03 (dense weights and expert streaming/LRU sources).
//
// Native side: a transcription of the DECISIONS in
// runtime/core/src/moe/expert_stream_source.cpp, dense_weights.cpp and
// row_stream.cpp at commit df3ce73a, with the source line cited at every
// function. The transcription keeps the native state fields and the native
// order of every step; what it does not keep is the file, the kernel and the
// threads — a commit outcome, MemAvailable and blaze takeover success are
// inputs here.
//
// Port side: the exports of source/expert_lru.goose, dense_place.goose and
// stream_plan.goose, combined by source/stream_policy.goose.
//
// Both sides run the same scripted scenarios over synthetic tables; the traces
// are compared. No disk, no GPU, no model, no thread.
#include "g.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

using std::string;
using std::vector;

static int g_cases = 0;
static int g_checks = 0;
static int g_fails = 0;
static int g_shown = 0;

static void cmp(const char * what, const string & nat, const string & port) {
    g_checks++;
    if (nat == port) return;
    g_fails++;
    if (g_shown < 10) {
        g_shown++;
        std::printf("FAIL %s\n  nat : %s\n  port: %s\n", what, nat.c_str(), port.c_str());
    }
}

template <typename T>
static string fmt(const vector<T> & v) {
    string s = "[";
    for (size_t i = 0; i < v.size(); ++i) {
        if (i) s += ",";
        s += std::to_string((long long) v[i]);
    }
    return s + "]";
}

static sl_i64 sl(vector<int64_t> & v) { return sl_i64{v.data(), (int64_t) v.size()}; }
static sl_i64 csl(const vector<int64_t> & v) {
    return sl_i64{(int64_t *) (void *) v.data(), (int64_t) v.size()};
}

// ─────────────── slice A: expert cache policy ────────────────────────────────
// expert_stream_source.cpp.
namespace nat_a {

const int MAXP = 3;

struct Cache {
    int n_layer = 0, n_expert = 0;
    vector<char> bound;
    vector<size_t> nb2; // n_layer*3
    vector<uint8_t> cvalid, cspec, seen_, ever_evicted;
    vector<int32_t> cprev, cnext, cstamp;
    int32_t chead = -1, ctail = -1;
    uint32_t cgen = 0;
    size_t cresident = 0;
    long long chits = 0, clookups = 0, rereads = 0, evictions = 0, spec_useful = 0;
    size_t cache_max = 0, total = 0;
    size_t token_demand = 0, layer_demand = 0, demand_acc = 0, layer_demand_acc = 0;
    int last_il = -1;

    void resize(int nl, int ne, const vector<char> & b, const vector<size_t> & sizes) {
        n_layer = nl;
        n_expert = ne;
        bound = b;
        nb2 = sizes;
        size_t n = (size_t) nl * (size_t) ne;
        cvalid.assign(n, 0);
        cspec.assign(n, 0);
        ever_evicted.assign(n, 0);
        cprev.assign(n, -1);
        cnext.assign(n, -1);
        cstamp.assign(n, 0);
        seen_.assign((size_t) ne, 0);
        for (int il = 0; il < nl; ++il)
            for (int p = 0; p < MAXP; ++p) total += sizes[(size_t) il * MAXP + p];
    }

    size_t entry_bytes(int il) const { // 741
        size_t s = 0;
        for (int p = 0; p < MAXP; ++p) s += nb2[(size_t) il * MAXP + p];
        return s;
    }

    void lru_unlink(int32_t id) { // 709
        int32_t pv = cprev[id], nx = cnext[id];
        if (pv != -1) cnext[pv] = nx; else chead = nx;
        if (nx != -1) cprev[nx] = pv; else ctail = pv;
        cprev[id] = cnext[id] = -1;
    }
    void lru_push_front(int32_t id) { // 721
        cprev[id] = -1;
        cnext[id] = chead;
        if (chead != -1) cprev[chead] = id; else ctail = id;
        chead = id;
    }
    void lru_push_back(int32_t id) { // 732
        cnext[id] = -1;
        cprev[id] = ctail;
        if (ctail != -1) cnext[ctail] = id; else chead = id;
        ctail = id;
    }

    // 917. Returns -1 (commit failed), 1 (hit) or 0 (miss).
    int touch_entry(int il, int e, int promote, bool commit_ok, bool & hit) {
        const int32_t id = il * n_expert + e;
        clookups++;
        cstamp[id] = (int32_t) cgen;
        if (cvalid[id]) {
            chits++;
            if (cspec[id]) { spec_useful++; cspec[id] = 0; }
            if (promote) { lru_unlink(id); lru_push_front(id); }
            hit = true;
            return 1;
        }
        if (!commit_ok) return -1; // the vm commit failing is the only false return
        if (ever_evicted[id]) rereads++;
        cvalid[id] = 1;
        cspec[id] = 0;
        cresident += entry_bytes(il);
        lru_push_front(id);
        hit = false;
        return 0;
    }

    void evict_tail() { // 761
        const int32_t id = ctail;
        evictions++;
        ever_evicted[id] = 1;
        cvalid[id] = 0;
        cspec[id] = 0;
        cresident -= entry_bytes(id / n_expert);
        lru_unlink(id);
    }

    size_t set_cache_budget(size_t bytes) { // 675
        if (cache_max == 0) return cache_max;
        if (bytes > cache_max) {
            cache_max = std::min(bytes, total);
            return cache_max;
        }
        if (bytes == cache_max) return cache_max;
        cache_max = std::max<size_t>(1, bytes);
        while (cresident > cache_max && ctail != -1) evict_tail();
        return cache_max;
    }

    int retain(int il, const vector<int> & ids) { // 777
        if (cache_max == 0) return 0;
        int moved = 0;
        for (int e : ids) {
            if (e < 0 || e >= n_expert) continue;
            const int32_t id = il * n_expert + e;
            if (!cvalid[id]) continue;
            lru_unlink(id);
            lru_push_front(id);
            moved++;
        }
        return moved;
    }

    vector<int> query(int il, const vector<int> & ids) const { // 882
        vector<int> out(ids.size(), 0);
        if (cache_max == 0) return out;
        for (size_t i = 0; i < ids.size(); ++i) {
            const int e = ids[i];
            if (e < 0 || e >= n_expert) continue;
            const int32_t id = il * n_expert + e;
            if (!cvalid[id]) continue;
            out[i] = cspec[id] ? 2 : 1;
        }
        return out;
    }

    vector<int> filter(int il, const vector<int> & ids, const vector<int> & rem) const { // 434
        vector<int> out;
        if (cache_max == 0) return out;
        for (int e : ids) {
            if (e < 0 || e >= n_expert) continue;
            const int32_t id = il * n_expert + e;
            if (cvalid[id] || rem[id] != 0) continue;
            out.push_back(e);
        }
        return out;
    }

    int integrate(const vector<int> & done, const vector<int> & rem, bool adopt) { // 812
        int got = 0;
        for (int32_t id : done) {
            if (cvalid[id] == 0 && rem[id] == 0) {
                cvalid[id] = 1;
                cspec[id] = 1;
                cstamp[id] = 0;
                cresident += entry_bytes(id / n_expert);
                if (adopt) lru_push_front(id); else lru_push_back(id);
                got++;
            }
        }
        return got;
    }

    void account_demand(int il, int n_unique) { // 648
        if (il <= last_il) {
            token_demand = demand_acc;
            layer_demand = layer_demand_acc;
            demand_acc = 0;
            layer_demand_acc = 0;
        }
        last_il = il;
        size_t bytes = (size_t) n_unique * entry_bytes(il);
        demand_acc += bytes;
        layer_demand_acc = std::max(layer_demand_acc, bytes);
    }

    size_t worst_cycle(int top_k) const { // 697
        if (top_k <= 0) return 0;
        size_t cycle = 0;
        for (int il = 0; il < n_layer; ++il)
            if (bound[il])
                cycle += entry_bytes(il) * (size_t) std::min(top_k, n_expert);
        return cycle;
    }

    int nproj(int il) const {
        int k = 0;
        for (int p = 0; p < MAXP; ++p) if (nb2[(size_t) il * MAXP + p]) k++;
        return k;
    }

    // 954-1071, cache on: dedup, stage, account, evict. Reads become a count.
    int load(int il, const vector<int> & ids, string & km, long long & ev_out, int & uniq_out,
             int & dup_out, bool commit_ok) {
        cgen = (uint32_t) ((cgen + 1u) & 0xffffffffu);
        std::fill(seen_.begin(), seen_.end(), (uint8_t) 0);
        int n_unique = 0, dup = 0, jobs = 0;
        km.clear();
        for (int e : ids) {
            if (e < 0 || e >= n_expert) { km += 'x'; continue; }
            const int32_t id = il * n_expert + e;
            if (seen_[e]) { // 1007: promote, never read again
                km += '.';
                dup++;
                if (cache_max) { lru_unlink(id); lru_push_front(id); }
                continue;
            }
            seen_[e] = 1;
            n_unique++;
            if (cache_max == 0) { // 978: shared slot, nothing kept
                km += 'S';
                jobs += nproj(il);
                continue;
            }
            bool hit = false;
            int r = touch_entry(il, e, 1, commit_ok, hit);
            if (r < 0) return -1;
            km += hit ? 'H' : 'M';
            if (!hit) jobs += nproj(il);
        }
        account_demand(il, n_unique);
        long long ev = 0;
        if (cache_max) {
            while (cresident > cache_max && ctail != -1 && cstamp[ctail] != (int32_t) cgen) {
                evict_tail();
                ev++;
            }
        }
        ev_out = ev;
        uniq_out = n_unique;
        dup_out = dup;
        return jobs;
    }

    string order() const {
        vector<long long> ids;
        for (int32_t i = chead; i != -1; i = cnext[i]) ids.push_back(i);
        return fmt(ids);
    }
};

} // namespace nat_a

// The same state, driven through the Goose exports.
struct PCache {
    int n_layer = 0, n_expert = 0;
    vector<int64_t> lay, cvalid, cspec, ever, prev, next, st, sacc, cstamp, seen, acc;
    size_t cache_max = 0, total = 0;
    uint32_t cgen = 0;
    long long ev_total = 0;

    void resize(int nl, int ne, const vector<char> & b, const vector<size_t> & sizes) {
        n_layer = nl;
        n_expert = ne;
        lay.assign((size_t) nl * 4, 0);
        for (int il = 0; il < nl; ++il) {
            lay[(size_t) il * 4] = b[il] ? 1 : 0;
            for (int p = 0; p < 3; ++p) lay[(size_t) il * 4 + 1 + p] = (int64_t) sizes[(size_t) il * 3 + p];
        }
        size_t n = (size_t) nl * (size_t) ne;
        cvalid.assign(n, 0); cspec.assign(n, 0); ever.assign(n, 0);
        prev.assign(n, -1); next.assign(n, -1); cstamp.assign(n, 0);
        seen.assign((size_t) ne, 0);
        st.assign(2, -1);
        sacc.assign(5, 0);
        acc.assign(5, 0);
        acc[4] = -1;
        for (int il = 0; il < nl; ++il)
            for (int p = 0; p < 3; ++p) total += sizes[(size_t) il * 3 + p];
    }

    size_t cres() const { return (size_t) sacc[0]; }
    int nproj(int il) const {
        int k = 0;
        for (int p = 0; p < 3; ++p) if (lay[(size_t) il * 4 + 1 + p]) k++;
        return k;
    }

    int load(int il, const vector<int> & ids, string & km, long long & ev_out, int & uniq_out,
             int & dup_out, bool commit_ok) {
        cgen = (uint32_t) ((cgen + 1u) & 0xffffffffu);
        std::fill(seen.begin(), seen.end(), 0);
        vector<int64_t> idv(ids.begin(), ids.end());
        vector<int64_t> kind(ids.size(), 0), out(2, 0);
        xl_load_plan(sl(seen), csl(idv), (int64_t) ids.size(), n_expert, sl(kind), sl(out));
        int jobs = 0;
        km.clear();
        for (size_t i = 0; i < ids.size(); ++i) {
            const int e = ids[i];
            if (kind[i] == 0) { km += 'x'; continue; }
            const int32_t id = il * n_expert + e;
            if (kind[i] == 2) {
                km += '.';
                if (cache_max) {
                    xl_unlink_e(sl(prev), sl(next), sl(st), id);
                    xl_push_front(sl(prev), sl(next), sl(st), id);
                }
                continue;
            }
            if (cache_max == 0) {
                km += 'S';
                jobs += nproj(il);
                continue;
            }
            xl_stamp(sl(cstamp), cgen, il, e, n_expert);
            int r = (int) xl_touch(csl(lay), sl(cvalid), sl(cspec), sl(prev), sl(next), sl(ever),
                                   sl(st), sl(sacc), n_expert, il, e, 1, -1, commit_ok ? 1 : 0);
            if (r < 0) return -1;
            km += r ? 'H' : 'M';
            if (!r) jobs += nproj(il);
        }
        xl_account(sl(acc), il, out[0], (int64_t) xl_entry_bytes(csl(lay), il));
        long long ev = 0;
        if (cache_max)
            ev = (long long) xl_evict_load(csl(lay), sl(cvalid), sl(cspec), sl(prev), sl(next),
                                           sl(ever), sl(st), sl(sacc), csl(cstamp), cgen,
                                           (int64_t) cache_max, n_expert);
        ev_total += ev;
        ev_out = ev;
        uniq_out = (int) out[0];
        dup_out = (int) out[1];
        return jobs;
    }

    size_t set_cache_budget(size_t bytes) {
        int64_t off = cache_max == 0 ? 1 : 0;
        size_t before = cres();
        int64_t r = xl_set_budget(csl(lay), sl(cvalid), sl(cspec), sl(prev), sl(next), sl(ever),
                                  sl(st), sl(sacc), (int64_t) bytes, (int64_t) cache_max,
                                  (int64_t) total, off, n_expert);
        (void) before;
        if (!off) cache_max = (size_t) r;
        return (size_t) r;
    }

    int retain(int il, const vector<int> & ids) {
        if (cache_max == 0) return 0;
        vector<int64_t> idv(ids.begin(), ids.end());
        return (int) xl_retain(csl(cvalid), sl(prev), sl(next), sl(st), il, csl(idv),
                               (int64_t) ids.size(), n_expert);
    }

    vector<int> query(int il, const vector<int> & ids) const {
        vector<int64_t> idv(ids.begin(), ids.end());
        vector<int64_t> out(ids.size(), 0);
        xl_query(csl(cvalid), csl(cspec), il, csl(idv), (int64_t) ids.size(), n_expert,
                 cache_max == 0 ? 1 : 0, sl(out));
        vector<int> r(ids.size());
        for (size_t i = 0; i < ids.size(); ++i) r[i] = (int) out[i];
        return r;
    }

    vector<int> filter(int il, const vector<int> & ids, const vector<int> & rem) const {
        vector<int64_t> idv(ids.begin(), ids.end());
        vector<int64_t> remv(rem.begin(), rem.end());
        vector<int64_t> out(ids.size(), 0);
        int64_t k = xl_filter(csl(cvalid), csl(remv), il, csl(idv), (int64_t) ids.size(),
                              n_expert, cache_max == 0 ? 1 : 0, sl(out));
        vector<int> r;
        for (int64_t i = 0; i < k; ++i) r.push_back((int) out[(size_t) i]);
        return r;
    }

    int integrate(const vector<int> & done, const vector<int> & rem, bool adopt) {
        vector<int64_t> dv(done.begin(), done.end());
        vector<int64_t> rv(rem.begin(), rem.end());
        return (int) xl_integrate(csl(lay), sl(cvalid), sl(cspec), sl(cstamp), sl(prev), sl(next),
                                  sl(st), sl(sacc), csl(dv), (int64_t) done.size(), csl(rv),
                                  n_expert, adopt ? 1 : 0);
    }

    string order() const {
        vector<long long> ids;
        for (int64_t i = st[0]; i != -1; i = next[(size_t) i]) ids.push_back(i);
        return fmt(ids);
    }

    // Evictions attributed to one set_cache_budget call: the entries that were
    // resident before and are not now. Nothing is inserted by a budget call.
    long long budget_evictions(const vector<int64_t> & before) const {
        long long n = 0;
        for (size_t i = 0; i < cvalid.size(); ++i)
            if (before[i] != 0 && cvalid[i] == 0) n++;
        return n;
    }
};

// ─────────────── slice B: dense placement ────────────────────────────────────
// dense_weights.cpp.
namespace nat_b {

typedef std::pair<uint64_t, uint64_t> R;

vector<R> byte_ranges(vector<R> expert, uint64_t file_size) { // 20
    std::sort(expert.begin(), expert.end());
    vector<R> dense;
    uint64_t pos = 0;
    for (const auto & r : expert) {
        if (r.first > pos) dense.push_back({pos, r.first});
        pos = std::max(pos, r.second);
    }
    if (pos < file_size) dense.push_back({pos, file_size});
    return dense;
}

void subtract_range(vector<R> & ranges, uint64_t a, uint64_t b) { // 98
    vector<R> out;
    for (const auto & r : ranges) {
        if (b <= r.first || a >= r.second) { out.push_back(r); continue; }
        if (r.first < a) out.push_back({r.first, a});
        if (b < r.second) out.push_back({b, r.second});
    }
    ranges.swap(out);
}

vector<int> classify(const vector<uint64_t> & size, const vector<int> & rowwant, uint64_t avail,
                     bool takeover) { // 128 then 172
    vector<int> code(size.size(), 0);
    if (avail)
        for (size_t i = 0; i < size.size(); ++i)
            if (size[i] > avail) code[i] = 1;
    if (takeover)
        for (size_t i = 0; i < size.size(); ++i)
            if (rowwant[i]) code[i] = 2;
    return code;
}

int held_count(const vector<int> & code) {
    int n = 0;
    for (int c : code) if (c) n++;
    return n;
}

int verdict(int mode, size_t n_resident, bool pinned_ok) { // 52-79
    if (mode == 2 || mode == 3) {
        if (n_resident == 0) return 3;
        if (mode == 3 && !pinned_ok) return 0;
        return 1;
    }
    if (mode == 1) return 2;
    return 4;
}

} // namespace nat_b

static string ranges_n(const vector<nat_b::R> & r) {
    string s;
    for (auto & p : r)
        s += "[" + std::to_string(p.first) + "," + std::to_string(p.second) + ")";
    return s.empty() ? "-" : s;
}
static string ranges_p(const vector<int64_t> & a, const vector<int64_t> & b, int64_t n) {
    string s;
    for (int64_t i = 0; i < n; ++i)
        s += "[" + std::to_string(a[(size_t) i]) + "," + std::to_string(b[(size_t) i]) + ")";
    return s.empty() ? "-" : s;
}

// ─────────────── slice C: row stream plan ────────────────────────────────────
// row_stream.cpp.
namespace nat_c {

const uint64_t SLAB = 16ull << 10;
const uint64_t PAGE = 4096;

uint64_t round_up(uint64_t v, uint64_t a) { return (v + a - 1) / a * a; }

uint64_t budget_of(uint64_t budget) { // 36
    if (!budget) budget = 64ull << 20;
    return std::max<uint64_t>(budget, SLAB);
}
uint64_t n_slabs(uint64_t size) { return round_up(size, SLAB) / SLAB; } // 71
uint64_t slab_len(uint64_t size, uint64_t s) { // 109
    uint64_t off = s * SLAB;
    return std::min<uint64_t>(SLAB, size - off);
}
uint64_t commit_bytes(uint64_t size, uint64_t s, uint64_t span) { // 123
    uint64_t off = s * SLAB;
    uint64_t len = slab_len(size, s);
    return std::min<uint64_t>(round_up(len, PAGE), span - off);
}

struct Plan { vector<uint64_t> need; uint64_t missing = 0; uint32_t gen = 0; };

Plan gather(uint64_t n_rows, uint64_t row_bytes, uint64_t size, const vector<int> & idx,
            uint64_t nb_slabs, vector<uint32_t> & stamp, uint32_t gen_in,
            const vector<uint8_t> & resident) { // 170
    Plan p;
    uint32_t gen = gen_in + 1u;
    if (gen == 0) {
        std::fill(stamp.begin(), stamp.end(), 0u);
        gen = 1;
    }
    p.gen = gen;
    for (int r : idx) {
        if (r < 0 || (uint64_t) r >= n_rows) continue;
        const uint64_t a = (uint64_t) r * row_bytes;
        const uint64_t b = std::min<uint64_t>(a + row_bytes, size);
        for (uint64_t s = a / SLAB; s * SLAB < b; ++s) {
            if (s >= nb_slabs) break;
            if (stamp[s] == gen) continue;
            stamp[s] = gen;
            p.need.push_back(s);
            if (!resident[s]) p.missing += SLAB;
        }
    }
    return p;
}

struct Evict { vector<uint64_t> order; uint64_t resident = 0; uint64_t evicted = 0; };

Evict evict_to_budget(uint64_t budget, uint64_t resident_in, uint64_t headroom,
                      vector<uint64_t> order, uint32_t gen, const vector<uint32_t> & stamp,
                      const vector<uint64_t> & bytes, vector<uint8_t> & resident) { // 148
    Evict e;
    e.resident = resident_in;
    for (size_t i = 0; i < order.size();) {
        if (e.resident + headroom <= budget) break;
        const uint64_t s = order[i];
        if (stamp[s] == gen) { ++i; continue; }
        e.resident -= bytes[s];
        resident[s] = 0;
        e.evicted++;
        order.erase(order.begin() + (long) i);
    }
    e.order = order;
    return e;
}

bool materialize_ok(uint64_t size, uint64_t budget) { return size <= budget; } // 213

} // namespace nat_c

// ─────────────── scenarios: slice A ─────────────────────────────────────────
namespace {

enum AKind { A_LOAD, A_RETAIN, A_QUERY, A_FILTER, A_EVICT, A_BUDGET, A_INTEGRATE };

struct AOp {
    AKind kind;
    int il = 0;
    vector<int> ids;
    vector<int> rem;
    long long val = 0;
    int adopt = 0;
    int commit_ok = 1;
};

struct AShape {
    int n_expert = 8;
    vector<char> bound;
    vector<size_t> sizes;
    size_t cache_max = 0;
};

void run_a(const char * name, const AShape & sh, const vector<AOp> & ops) {
    g_cases++;
    nat_a::Cache nc;
    nc.resize((int) sh.bound.size(), sh.n_expert, sh.bound, sh.sizes);
    nc.cache_max = sh.cache_max;
    PCache pc;
    pc.resize((int) sh.bound.size(), sh.n_expert, sh.bound, sh.sizes);
    pc.cache_max = sh.cache_max;

    string nat, port;
    for (size_t oi = 0; oi < ops.size(); ++oi) {
        const AOp & op = ops[oi];
        char tag[32];
        std::snprintf(tag, sizeof tag, "%s[%zu] ", name, oi);
        string ln, lp;
        switch (op.kind) {
        case A_LOAD: {
            string km_n, km_p;
            long long ev_n = 0, ev_p = 0;
            int u_n = 0, u_p = 0, d_n = 0, d_p = 0;
            int j_n = nc.load(op.il, op.ids, km_n, ev_n, u_n, d_n, op.commit_ok != 0);
            int j_p = pc.load(op.il, op.ids, km_p, ev_p, u_p, d_p, op.commit_ok != 0);
            ln = "LOAD il=" + std::to_string(op.il) + " g=" + std::to_string(nc.cgen) + " km=" + km_n +
                 " u=" + std::to_string(u_n) + " d=" + std::to_string(d_n) + " j=" + std::to_string(j_n) +
                 " ev=" + std::to_string(ev_n) + " res=" + std::to_string(nc.cresident) + " ord=" + nc.order();
            lp = "LOAD il=" + std::to_string(op.il) + " g=" + std::to_string(pc.cgen) + " km=" + km_p +
                 " u=" + std::to_string(u_p) + " d=" + std::to_string(d_p) + " j=" + std::to_string(j_p) +
                 " ev=" + std::to_string(ev_p) + " res=" + std::to_string(pc.cres()) + " ord=" + pc.order();
            break;
        }
        case A_RETAIN: {
            int n = nc.retain(op.il, op.ids);
            int p = pc.retain(op.il, op.ids);
            ln = "RETAIN m=" + std::to_string(n) + " ord=" + nc.order();
            lp = "RETAIN m=" + std::to_string(p) + " ord=" + pc.order();
            break;
        }
        case A_QUERY: {
            vector<int> n = nc.query(op.il, op.ids), p = pc.query(op.il, op.ids);
            ln = "QUERY " + fmt(vector<long long>(n.begin(), n.end()));
            lp = "QUERY " + fmt(vector<long long>(p.begin(), p.end()));
            break;
        }
        case A_FILTER: {
            vector<int> n = nc.filter(op.il, op.ids, op.rem), p = pc.filter(op.il, op.ids, op.rem);
            ln = "FILTER " + fmt(vector<long long>(n.begin(), n.end()));
            lp = "FILTER " + fmt(vector<long long>(p.begin(), p.end()));
            break;
        }
        case A_EVICT: {
            if (nc.ctail == -1) {
                ln = "EVICT empty";
            } else {
                int32_t victim = nc.ctail;
                nc.evict_tail();
                ln = "EVICT id=" + std::to_string(victim) + " res=" + std::to_string(nc.cresident) + " ord=" + nc.order();
            }
            int64_t r = xl_evict_tail(csl(pc.lay), sl(pc.cvalid), sl(pc.cspec), sl(pc.prev),
                                      sl(pc.next), sl(pc.ever), sl(pc.st), sl(pc.sacc), pc.n_expert);
            if (r == -1) {
                lp = "EVICT empty";
            } else {
                pc.ev_total++;
                lp = "EVICT id=" + std::to_string(r) + " res=" + std::to_string(pc.cres()) + " ord=" + pc.order();
            }
            break;
        }
        case A_BUDGET: {
            vector<int64_t> before = pc.cvalid;
            size_t n = nc.set_cache_budget((size_t) op.val);
            size_t p = pc.set_cache_budget((size_t) op.val);
            pc.ev_total += pc.budget_evictions(before);
            ln = "BUDGET b=" + std::to_string(n) + " res=" + std::to_string(nc.cresident) + " ord=" + nc.order();
            lp = "BUDGET b=" + std::to_string(p) + " res=" + std::to_string(pc.cres()) + " ord=" + pc.order();
            break;
        }
        case A_INTEGRATE: {
            int n = nc.integrate(op.ids, op.rem, op.adopt != 0);
            int p = pc.integrate(op.ids, op.rem, op.adopt != 0);
            ln = "INTEG g=" + std::to_string(n) + " res=" + std::to_string(nc.cresident) + " ord=" + nc.order();
            lp = "INTEG g=" + std::to_string(p) + " res=" + std::to_string(pc.cres()) + " ord=" + pc.order();
            break;
        }
        }
        nat += ln;
        port += lp;
        cmp(tag, ln, lp);
    }
    string sn = "STATE res=" + std::to_string(nc.cresident) + " hits=" + std::to_string(nc.chits) +
                " look=" + std::to_string(nc.clookups) + " re=" + std::to_string(nc.rereads) +
                " ev=" + std::to_string(nc.evictions) + " use=" + std::to_string(nc.spec_useful) +
                " dem=" + std::to_string(nc.token_demand) + "/" + std::to_string(nc.layer_demand) +
                " acc=" + std::to_string(nc.demand_acc) + "/" + std::to_string(nc.layer_demand_acc) +
                " last=" + std::to_string(nc.last_il) +
                " wc3=" + std::to_string(nc.worst_cycle(3)) + " wc99=" + std::to_string(nc.worst_cycle(99)) +
                " valid=" + fmt(vector<long long>(nc.cvalid.begin(), nc.cvalid.end())) +
                " spec=" + fmt(vector<long long>(nc.cspec.begin(), nc.cspec.end())) +
                " ever=" + fmt(vector<long long>(nc.ever_evicted.begin(), nc.ever_evicted.end())) +
                " stamp=" + fmt(vector<long long>(nc.cstamp.begin(), nc.cstamp.end()));
    string sp = "STATE res=" + std::to_string(pc.cres()) + " hits=" + std::to_string(pc.sacc[1]) +
                " look=" + std::to_string(pc.sacc[2]) + " re=" + std::to_string(pc.sacc[3]) +
                " ev=" + std::to_string(pc.ev_total) + " use=" + std::to_string(pc.sacc[4]) +
                " dem=" + std::to_string(pc.acc[0]) + "/" + std::to_string(pc.acc[1]) +
                " acc=" + std::to_string(pc.acc[2]) + "/" + std::to_string(pc.acc[3]) +
                " last=" + std::to_string(pc.acc[4]) +
                " wc3=" + std::to_string(xl_worst_cycle(csl(pc.lay), pc.n_layer, pc.n_expert, 3)) +
                " wc99=" + std::to_string(xl_worst_cycle(csl(pc.lay), pc.n_layer, pc.n_expert, 99)) +
                " valid=" + fmt(pc.cvalid) + " spec=" + fmt(pc.cspec) + " ever=" + fmt(pc.ever) +
                " stamp=" + fmt(pc.cstamp);
    cmp(name, sn, sp);
}

AOp op_load(int il, vector<int> ids, int commit_ok = 1) {
    AOp o; o.kind = A_LOAD; o.il = il; o.ids = std::move(ids); o.commit_ok = commit_ok; return o;
}
AOp op_retain(int il, vector<int> ids) { AOp o; o.kind = A_RETAIN; o.il = il; o.ids = std::move(ids); return o; }
AOp op_query(int il, vector<int> ids) { AOp o; o.kind = A_QUERY; o.il = il; o.ids = std::move(ids); return o; }
AOp op_filter(int il, vector<int> ids, vector<int> rem) {
    AOp o; o.kind = A_FILTER; o.il = il; o.ids = std::move(ids); o.rem = std::move(rem); return o;
}
AOp op_evict() { AOp o; o.kind = A_EVICT; return o; }
AOp op_budget(long long b) { AOp o; o.kind = A_BUDGET; o.val = b; return o; }
AOp op_integ(vector<int> ids, vector<int> rem, int adopt) {
    AOp o; o.kind = A_INTEGRATE; o.ids = std::move(ids); o.rem = std::move(rem); o.adopt = adopt; return o;
}

// ─────────────── scenarios: slice B ─────────────────────────────────────────
typedef nat_b::R NR;

void case_ranges(const char * name, vector<NR> expert, uint64_t file_size) {
    g_cases++;
    vector<NR> n = nat_b::byte_ranges(expert, file_size);
    vector<NR> sorted = expert;
    std::sort(sorted.begin(), sorted.end());
    vector<int64_t> ea, eb;
    for (auto & r : sorted) { ea.push_back((int64_t) r.first); eb.push_back((int64_t) r.second); }
    vector<int64_t> oa(ea.size() + 1, 0), ob(ea.size() + 1, 0);
    int64_t k = dw_byte_ranges(sl(ea), sl(eb), (int64_t) ea.size(), (int64_t) file_size,
                               sl(oa), sl(ob));
    cmp(name, ranges_n(n), ranges_p(oa, ob, k));
}

void case_subtract(const char * name, vector<NR> base, uint64_t a, uint64_t b) {
    g_cases++;
    vector<NR> n = base;
    nat_b::subtract_range(n, a, b);
    vector<int64_t> ra, rb;
    for (auto & r : base) { ra.push_back((int64_t) r.first); rb.push_back((int64_t) r.second); }
    vector<int64_t> oa(ra.size() + 2, 0), ob(ra.size() + 2, 0);
    int64_t k = dw_subtract(sl(ra), sl(rb), (int64_t) ra.size(), (int64_t) a, (int64_t) b,
                            sl(oa), sl(ob));
    cmp(name, ranges_n(n), ranges_p(oa, ob, k));
}

void case_plan(const char * name, const vector<uint64_t> & sizes, const vector<uint64_t> & offs,
               const vector<int> & rowwant, const vector<NR> & base_dense, uint64_t avail,
               bool takeover, int mode, bool pinned_ok) {
    g_cases++;
    vector<int> ncode = nat_b::classify(sizes, rowwant, avail, takeover);
    vector<int64_t> sz, want, code(sizes.size(), 0);
    for (auto s : sizes) sz.push_back((int64_t) s);
    for (int w : rowwant) want.push_back(w);
    int64_t left = dw_classify(csl(sz), csl(want), (int64_t) sz.size(), (int64_t) avail,
                               takeover ? 1 : 0, sl(code));
    cmp(name, "CODE " + fmt(vector<long long>(ncode.begin(), ncode.end())) + " left=" +
                  std::to_string(nat_b::held_count(ncode)),
             "CODE " + fmt(code) + " left=" + std::to_string(left));

    // The range subtraction follows tensor order: hold-back runs first, the row
    // takeover second, exactly as init() calls them.
    vector<NR> nr = base_dense, pr = base_dense;
    for (size_t i = 0; i < sizes.size(); ++i)
        if (ncode[i] != 0) nat_b::subtract_range(nr, offs[i], offs[i] + sizes[i]);
    vector<int64_t> ra, rb;
    for (auto & r : pr) { ra.push_back((int64_t) r.first); rb.push_back((int64_t) r.second); }
    for (size_t i = 0; i < sizes.size(); ++i) {
        if (code[i] == 0) continue;
        vector<int64_t> oa(ra.size() + 2, 0), ob(ra.size() + 2, 0);
        int64_t k = dw_subtract(sl(ra), sl(rb), (int64_t) ra.size(), (int64_t) offs[i],
                                (int64_t) (offs[i] + sizes[i]), sl(oa), sl(ob));
        ra.assign(oa.begin(), oa.begin() + k);
        rb.assign(ob.begin(), ob.begin() + k);
    }
    cmp(name, "RANGES " + ranges_n(nr), "RANGES " + ranges_p(ra, rb, (int64_t) ra.size()));
    cmp(name, "VERDICT " + std::to_string(nat_b::verdict(mode, (size_t) 1, pinned_ok)),
             "VERDICT " + std::to_string(dw_verdict(mode, 1, pinned_ok ? 1 : 0)));
    size_t resident = 0;
    for (size_t i = 0; i < sizes.size(); ++i) if (ncode[i] == 0) resident += sizes[i];
    int64_t pres = dw_resident_bytes(csl(sz), csl(code), (int64_t) sz.size());
    cmp(name, "RESIDENT " + std::to_string(resident), "RESIDENT " + std::to_string(pres));
}

void case_verdicts() {
    const char * modes[4] = {"mmap", "warm", "anon", "ahwb"};
    for (int m = 0; m < 4; ++m) {
        for (int nres = 0; nres < 2; ++nres) {
            for (int pok = 0; pok < 2; ++pok) {
                string name = string("verdict_") + modes[m] + "_n" + std::to_string(nres) + "_p" +
                              std::to_string(pok);
                g_cases++;
                cmp(name.c_str(), std::to_string(nat_b::verdict(m, (size_t) nres, pok != 0)),
                     std::to_string(dw_verdict(m, nres, pok)));
            }
        }
    }
}

// ─────────────── scenarios: slice C ─────────────────────────────────────────
void case_gather(const char * name, uint64_t n_rows, uint64_t row_bytes, uint64_t size,
                 vector<int> idx, uint32_t gen_in, vector<uint8_t> resident) {
    g_cases++;
    uint64_t ns = nat_c::n_slabs(size);
    vector<uint32_t> st_n((size_t) ns, 0), st_p((size_t) ns, 0);
    // A stamped history that cannot collide with the generations this corpus
    // uses (1..8): the dedup guard is exercised by repeated rows in one plan,
    // not by the seed. The wrap case clears all stamps anyway.
    for (uint64_t i = 0; i < ns; ++i) st_n[(size_t) i] = st_p[(size_t) i] = 1000u;
    nat_c::Plan pn = nat_c::gather(n_rows, row_bytes, size, idx, ns, st_n, gen_in, resident);
    vector<int64_t> need((size_t) ns + 4, 0), out(3, 0), resid(resident.begin(), resident.end());
    vector<int64_t> idv(idx.begin(), idx.end());
    vector<int64_t> stamp(st_p.begin(), st_p.end());
    int64_t k = sp_gather_plan((int64_t) n_rows, (int64_t) row_bytes, (int64_t) size, csl(idv),
                               (int64_t) idx.size(), (int64_t) ns, (int64_t) nat_c::SLAB, sl(stamp),
                               (int64_t) gen_in, csl(resid), sl(need), sl(out));
    (void) k;
    vector<long long> nn, pneed;
    for (auto s : pn.need) nn.push_back((long long) s);
    for (int64_t i = 0; i < out[0]; ++i) pneed.push_back((long long) need[(size_t) i]);
    cmp(name, "GATHER need=" + fmt(nn) + " miss=" + std::to_string(pn.missing) + " g=" + std::to_string(pn.gen) +
              " stamp=" + fmt(vector<long long>(st_n.begin(), st_n.end())),
             "GATHER need=" + fmt(pneed) + " miss=" + std::to_string(out[1]) + " g=" + std::to_string(out[2]) +
              " stamp=" + fmt(stamp));
}

void case_evict(const char * name, uint64_t budget, uint64_t resident_in, uint64_t headroom,
                vector<uint64_t> order, uint32_t gen, vector<uint32_t> stamp,
                vector<uint64_t> bytes, vector<uint8_t> resident) {
    g_cases++;
    vector<uint8_t> rn = resident, rp = resident;
    nat_c::Evict e = nat_c::evict_to_budget(budget, resident_in, headroom, order, gen, stamp, bytes, rn);
    vector<int64_t> ord, stv, byv, keep(order.size() + 1, 0), out(2, 0);
    vector<int64_t> rv(resident.begin(), resident.end());
    for (auto s : order) ord.push_back((int64_t) s);
    for (auto s : stamp) stv.push_back((int64_t) s);
    for (auto b : bytes) byv.push_back((int64_t) b);
    int64_t ev = sp_evict(sl(rv), csl(stv), (int64_t) gen, (int64_t) budget, (int64_t) resident_in,
                          (int64_t) headroom, csl(ord), (int64_t) ord.size(), csl(byv), sl(keep),
                          sl(out));
    vector<long long> ko, pk;
    for (auto s : e.order) ko.push_back((long long) s);
    for (int64_t i = 0; i < out[1]; ++i) pk.push_back((long long) keep[(size_t) i]);
    cmp(name, "EVICT n=" + std::to_string(e.evicted) + " res=" + std::to_string(e.resident) +
              " ord=" + fmt(ko) + " resv=" + fmt(vector<long long>(rn.begin(), rn.end())),
             "EVICT n=" + std::to_string(ev) + " res=" + std::to_string(out[0]) +
              " ord=" + fmt(pk) + " resv=" + fmt(rv));
}

void case_geometry() {
    g_cases++;
    uint64_t sizes[] = {0, 1, 4095, 4096, 16383, 16384, 16385, 100000};
    for (uint64_t sz : sizes) {
        string name = "geom_" + std::to_string(sz);
        uint64_t ns = nat_c::n_slabs(sz);
        int64_t pns = sp_n_slabs((int64_t) sz, (int64_t) nat_c::SLAB);
        cmp(name.c_str(), "N " + std::to_string(ns), "N " + std::to_string(pns));
        for (uint64_t s = 0; s < ns; ++s) {
            uint64_t l = nat_c::slab_len(sz, s);
            int64_t pl = sp_slab_len((int64_t) sz, (int64_t) s, (int64_t) nat_c::SLAB);
            cmp(name.c_str(), "L" + std::to_string(s) + " " + std::to_string(l),
                "L" + std::to_string(s) + " " + std::to_string(pl));
            uint64_t span = (sz + 4095) / 4096 * 4096;
            int64_t pspan = dw_span((int64_t) sz, 4096);
            cmp(name.c_str(), "SPAN " + std::to_string(span), "SPAN " + std::to_string(pspan));
        }
    }
    uint64_t budgets[] = {0, 1, 16383, 16384, 16385, 1ull << 20};
    for (uint64_t b : budgets) {
        string name = "budget_" + std::to_string(b);
        cmp(name.c_str(), "B " + std::to_string(nat_c::budget_of(b)),
            "B " + std::to_string(sp_round_budget((int64_t) b, (int64_t) nat_c::SLAB, 64ll << 20)));
    }
}

void case_materialize(const char * name, uint64_t size, uint64_t budget) {
    g_cases++;
    cmp(name, "MAT " + std::to_string(nat_c::materialize_ok(size, budget) ? 1 : 0),
         "MAT " + std::to_string(sp_materialize_ok((int64_t) size, (int64_t) budget)));
}

void case_order(const char * name, vector<int> need, vector<uint8_t> resident) {
    g_cases++;
    std::sort(need.begin(), need.end());
    vector<int> reads;
    for (int s : need) if (!resident[(size_t) s]) reads.push_back(s);
    vector<int64_t> nv(need.begin(), need.end());
    vector<int64_t> rv(resident.begin(), resident.end());
    vector<int64_t> out(nv.size() + 1, 0);
    int64_t k = sp_fetch_order(csl(nv), (int64_t) nv.size(), csl(rv), sl(out));
    vector<long long> pr;
    for (int64_t i = 0; i < k; ++i) pr.push_back((long long) out[(size_t) i]);
    cmp(name, "ORDER " + fmt(vector<long long>(reads.begin(), reads.end())), "ORDER " + fmt(pr));
}

} // namespace

int main() {
    goose_init(0, nullptr);
    int ck0 = g_checks, ca0 = g_cases, fl0 = g_fails;
    // ── slice A: expert cache policy ────────────────────────────────────────
    {
        AShape sh;
        sh.n_expert = 4;
        sh.bound = {1};
        sh.sizes = {10, 10, 10};
        sh.cache_max = 1000;
        vector<AOp> ops = {
            op_load(0, {1, 2}), op_load(0, {2, 3}), op_load(0, {1, 2, 3}),
            op_query(0, {0, 1, 2, 3}), op_evict(), op_evict(), op_evict(), op_evict(), op_evict(),
        };
        run_a("a_basic", sh, ops);
    }
    {
        AShape sh; // cache off: the shared-slot branch keeps nothing
        sh.n_expert = 4;
        sh.bound = {1};
        sh.sizes = {10, 10, 10};
        sh.cache_max = 0;
        vector<AOp> ops = {
            op_load(0, {0, 1, 1}), op_query(0, {0, 1}), op_filter(0, {2, 3}, {0, 0, 0, 0}),
            op_retain(0, {0, 1}), op_evict(), op_budget(500),
        };
        run_a("a_cache_off", sh, ops);
    }
    {
        AShape sh; // a hit promotes; the older entry becomes the eviction target
        sh.n_expert = 4;
        sh.bound = {1};
        sh.sizes = {10, 10, 10};
        sh.cache_max = 1000;
        vector<AOp> ops = {
            op_load(0, {0}), op_load(0, {1}), op_load(0, {2}), op_load(0, {0}),
            op_retain(0, {2, 0}), op_retain(0, {3, -1, 9}),
        };
        run_a("a_promote", sh, ops);
    }
    {
        AShape sh; // grow never evicts and clamps at the whole expert set
        sh.n_expert = 6;
        sh.bound = {1, 1};
        sh.sizes = {10, 10, 10, 20, 20, 20};
        sh.cache_max = 200;
        vector<AOp> ops = {
            op_load(0, {0, 1, 2, 3, 4, 5}), op_load(1, {0, 1, 2}),
            op_budget(100), op_budget(360), op_budget(100000),
        };
        run_a("a_grow", sh, ops);
    }
    {
        AShape sh; // shrink to the 1-byte floor, then eviction stops on empty
        sh.n_expert = 4;
        sh.bound = {1};
        sh.sizes = {10, 10, 10};
        sh.cache_max = 1000;
        vector<AOp> ops = {
            op_load(0, {0, 1, 2, 3}), op_budget(0), op_budget(1), op_budget(0), op_evict(),
        };
        run_a("a_shrink", sh, ops);
    }
    {
        AShape sh; // budget below one entry: every load evicts the tail it just wrote
        sh.n_expert = 4;
        sh.bound = {1};
        sh.sizes = {10, 10, 10};
        sh.cache_max = 45;
        vector<AOp> ops = {
            op_load(0, {0}), op_load(0, {1}), op_load(0, {2}), op_load(0, {3}),
            op_load(0, {0}),
        };
        run_a("a_tight", sh, ops);
    }
    {
        AShape sh; // duplicates: the first is staged, every repeat promotes
        sh.n_expert = 5;
        sh.bound = {1};
        sh.sizes = {8, 8, 8};
        sh.cache_max = 1000;
        vector<AOp> ops = {
            op_load(0, {1, 2, 1, 3, 2, 4}), op_load(0, {4, 4, 4}),
            op_query(0, {1, 2, 3, 4}),
        };
        run_a("a_dups", sh, ops);
    }
    {
        AShape sh; // invalid ids are skipped, not read, not counted
        sh.n_expert = 3;
        sh.bound = {1};
        sh.sizes = {10, 10, 10};
        sh.cache_max = 1000;
        vector<AOp> ops = {
            op_load(0, {-1, 3, 0, 99, 2}), op_load(0, {-5, -1}),
            op_query(0, {-1, 0, 2, 3}),
        };
        run_a("a_invalid", sh, ops);
    }
    {
        AShape sh; // the generation guard stops the whole post-load eviction
        sh.n_expert = 4;
        sh.bound = {1};
        sh.sizes = {10, 10, 10};
        sh.cache_max = 60;
        // budget 60 = two entries; the load stages two and the tail is the
        // entry it just touched, so nothing is evicted.
        vector<AOp> ops = {op_load(0, {0, 1, 2}), op_load(0, {3})};
        run_a("a_gen_guard", sh, ops);
    }
    {
        AShape sh; // prefetch candidates: resident, queued, invalid, duplicate
        sh.n_expert = 6;
        sh.bound = {1};
        sh.sizes = {10, 10, 10};
        sh.cache_max = 1000;
        vector<int> rem(6, 0);
        rem[1] = 2; // already queued
        rem[4] = 1; // queued with a different count
        vector<AOp> ops = {
            op_load(0, {0, 2}),
            op_filter(0, {0, 1, 2, 3, 3, 4, -1, 9}, rem),
        };
        run_a("a_filter", sh, ops);
    }
    {
        AShape sh; // integrate: a guess enters cold, a committed read enters hot
        sh.n_expert = 5;
        sh.bound = {1};
        sh.sizes = {10, 10, 10};
        sh.cache_max = 1000;
        vector<int> rem(5, 0);
        rem[3] = 1; // still in flight: must be skipped
        vector<AOp> ops = {
            op_load(0, {0}),
            op_integ({1, 2, 3, 1}, rem, 0),
            op_query(0, {0, 1, 2, 3}),
            op_integ({4, 4}, rem, 1),
        };
        run_a("a_integrate", sh, ops);
    }
    {
        AShape sh; // speculative hits are counted useful once, then become plain
        sh.n_expert = 4;
        sh.bound = {1};
        sh.sizes = {10, 10, 10};
        sh.cache_max = 1000;
        vector<int> rem(4, 0);
        vector<AOp> ops = {
            op_integ({2}, rem, 0), op_query(0, {2}), op_load(0, {2}),
            op_query(0, {2}), op_load(0, {2}),
        };
        run_a("a_spec_hit", sh, ops);
    }
    {
        AShape sh; // fused layout: the third projection is absent everywhere
        sh.n_expert = 4;
        sh.bound = {1};
        sh.sizes = {10, 10, 0};
        sh.cache_max = 1000;
        vector<AOp> ops = {op_load(0, {0, 1}), op_evict(), op_budget(21)};
        run_a("a_fused", sh, ops);
    }
    {
        AShape sh; // demand accounting across the token boundary, unbound layer
        sh.n_expert = 4;
        sh.bound = {1, 0, 1};
        sh.sizes = {10, 10, 10, 0, 0, 0, 5, 5, 5};
        sh.cache_max = 10000;
        vector<AOp> ops = {
            op_load(0, {0, 1}), op_load(2, {0, 1, 2}), op_load(0, {2}), op_load(2, {3}),
        };
        run_a("a_demand", sh, ops);
    }
    {
        AShape sh; // a commit failure leaves the entry unaccounted
        sh.n_expert = 4;
        sh.bound = {1};
        sh.sizes = {10, 10, 10};
        sh.cache_max = 1000;
        vector<AOp> ops = {op_load(0, {0, 1}, 0), op_load(0, {0, 1}, 1), op_query(0, {0, 1})};
        run_a("a_commit_fail", sh, ops);
    }

    int ckA = g_checks - ck0, caA = g_cases - ca0, flA = g_fails - fl0;
    ck0 = g_checks; ca0 = g_cases; fl0 = g_fails;
    // ── slice B: dense placement ────────────────────────────────────────────
    case_ranges("b_one", {{4096, 8192}}, 16384);
    case_ranges("b_unsorted", {{8192, 12288}, {0, 4096}, {4096, 6000}}, 16384);
    case_ranges("b_overlap", {{1000, 5000}, {2000, 3000}, {4000, 9000}}, 12000);
    case_ranges("b_touch", {{0, 100}, {100, 200}, {200, 300}}, 300);
    case_ranges("b_edges", {{0, 100}, {900, 1000}}, 1000);
    case_ranges("b_empty", {}, 7777);
    case_ranges("b_beyond", {{500, 4000}}, 1000);
    case_ranges("b_zero_len", {{0, 0}, {10, 10}, {20, 30}}, 40);
    case_subtract("b_sub_mid", {{0, 1000}}, 300, 700);
    case_subtract("b_sub_head", {{0, 1000}}, 0, 300);
    case_subtract("b_sub_tail", {{0, 1000}}, 700, 1000);
    case_subtract("b_sub_disjoint", {{0, 100}, {200, 300}}, 120, 180);
    case_subtract("b_sub_enclosing", {{100, 200}, {300, 400}}, 0, 500);
    case_subtract("b_sub_multi", {{0, 100}, {200, 300}, {400, 500}}, 50, 450);
    {
        vector<uint64_t> sizes = {100, 200, 300, 400};
        vector<int> want = {0, 0, 1, 1};
        vector<uint64_t> offs = {0, 100, 300, 600};
        case_plan("b_plan_fit", sizes, offs, want, {{0, 5000}}, 1000, true, 2, true);
        case_plan("b_plan_tight", sizes, offs, want, {{0, 5000}}, 200, true, 2, true);
        case_plan("b_plan_takeover_fail", sizes, offs, want, {{0, 5000}}, 1000, false, 2, true);
        case_plan("b_plan_anon_empty", {0, 0}, {0, 100}, {0, 0}, {{0, 8}}, 1000, true, 2, true);
        case_plan("b_plan_ahwb_no", sizes, offs, {0, 0, 0, 0}, {{0, 5000}}, 1000, true, 3, false);
        case_plan("b_plan_avail0", sizes, offs, {0, 0, 1, 1}, {{0, 5000}}, 0, true, 2, true);
    }
    case_verdicts();
    {
        g_cases++;
        cmp("b_span", "SPAN 4096 8192", "SPAN " + std::to_string(dw_span(4096, 4096)) + " " +
                                           std::to_string(dw_span(4097, 4096)));
    }

    int ckB = g_checks - ck0, caB = g_cases - ca0, flB = g_fails - fl0;
    ck0 = g_checks; ca0 = g_cases; fl0 = g_fails;
    // ── slice C: row stream plan ────────────────────────────────────────────
    const uint64_t SL = nat_c::SLAB;
    case_gather("c_gather_empty", 1000, 1024, 100 * SL, {}, 0, vector<uint8_t>(100, 0));
    case_gather("c_gather_invalid", 10, 1024, 10 * SL, {-1, 10, 999}, 0, vector<uint8_t>(10, 0));
    case_gather("c_gather_dedup", 1000, 1024, 100 * SL, {5, 5, 5}, 0, vector<uint8_t>(100, 0));
    case_gather("c_gather_straddle", 1000, 20000, 100 * SL, {0}, 0, vector<uint8_t>(100, 0));
    case_gather("c_gather_boundary", 100000, 1, 100 * SL, {16384}, 0, vector<uint8_t>(100, 0));
    case_gather("c_gather_boundary2", 100000, 1, 100 * SL, {16383, 16384}, 0, vector<uint8_t>(100, 0));
    case_gather("c_gather_wide", 100, 100000, 100 * SL, {0}, 0, vector<uint8_t>(100, 0));
    case_gather("c_gather_tail", 1000, 20000, 100 * SL + 111, {81}, 0, vector<uint8_t>(101, 0));
    case_gather("c_gather_wrap", 100, 1024, 10 * SL, {0, 1}, 4294967295u, vector<uint8_t>(10, 0));
    case_gather("c_gather_hot", 100, 1024, 10 * SL, {0, 1, 2}, 7, vector<uint8_t>(10, 1));

    {
        // one table, four slabs: 0 oldest, 3 newest
        vector<uint64_t> order = {0, 1, 2, 3};
        vector<uint32_t> stamp(4, 0);
        vector<uint64_t> bytes(4, SL);
        vector<uint8_t> res(4, 1);
        case_evict("c_evict_fit", 100 * SL, 4 * SL, SL, order, 9, stamp, bytes, res);
        case_evict("c_evict_two", 2 * SL, 4 * SL, SL, order, 9, stamp, bytes, res);
        case_evict("c_evict_all", SL, 4 * SL, SL, order, 9, stamp, bytes, res);
        stamp[0] = 9; // the oldest slab is needed by the gather in flight
        case_evict("c_evict_stamped", 2 * SL, 4 * SL, SL, order, 9, stamp, bytes, res);
        stamp[0] = 0;
        stamp[3] = 9; // the newest is stamped: the walk skips it and takes older ones
        case_evict("c_evict_newest_stamped", 2 * SL, 4 * SL, SL, order, 9, stamp, bytes, res);
        case_evict("c_evict_headroom_big", 4 * SL, 4 * SL, 10 * SL, order, 0, vector<uint32_t>(4, 0),
                   bytes, res);
    }
    case_geometry();
    case_materialize("c_mat_equal", 1 << 20, 1 << 20);
    case_materialize("c_mat_over", (1 << 20) + 1, 1 << 20);
    case_materialize("c_mat_under", 1, 1 << 20);
    case_order("c_order_scatter", {9, 3, 7, 1, 3}, vector<uint8_t>(10, 0));
    {
        vector<uint8_t> res(10, 0);
        res[1] = 1;
        res[7] = 1;
        case_order("c_order_holes", {9, 3, 7, 1}, res);
    }

    int ckC = g_checks - ck0, caC = g_cases - ca0, flC = g_fails - fl0;
    std::printf("FG_P9_03_A cases=%d checks=%d failures=%d\n", caA, ckA, flA);
    std::printf("FG_P9_03_B cases=%d checks=%d failures=%d\n", caB, ckB, flB);
    std::printf("FG_P9_03_C cases=%d checks=%d failures=%d\n", caC, ckC, flC);
    std::printf("FG_P9_03_STREAM cases=%d checks=%d failures=%d\n", g_cases, g_checks, g_fails);
    return g_fails ? 1 : 0;
}
