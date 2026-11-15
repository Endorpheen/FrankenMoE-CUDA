// S1.6: the staged build's stream-policy bridge. See fg-stream-policy.h.
//
// The native half is a transcription, not a copy: the runtime's versions are
// member functions over ExpertStreamSource/DenseWeights/RouterHook state, so
// each one is rewritten over the same guest arrays the Goose exports use, with
// the native line numbers in the comment. Every branch, every guard and the
// return encoding are the native ones; what is gone is the I/O (vm_commit,
// vm_evict, pread), which is why the commit outcome arrives as an argument.
#include "fg-stream-policy.h"

#include <atomic>
#include <cstddef>

#ifdef FG_GOOSE_STREAM
#include "stream-policy.h"

// The bridge passes the host's fg_i64/fg_u8 straight to the generated C ABI.
// If the generated header ever changes that layout, this fails to compile
// instead of corrupting a slice.
static_assert(sizeof(fg_i64) == sizeof(sl_i64), "fg_i64 must match sl_i64");
static_assert(offsetof(fg_i64, data) == offsetof(sl_i64, data), "fg_i64.data offset");
static_assert(offsetof(fg_i64, len) == offsetof(sl_i64, len), "fg_i64.len offset");
static_assert(sizeof(fg_u8) == sizeof(sl_u8), "fg_u8 must match sl_u8");
static_assert(offsetof(fg_u8, data) == offsetof(sl_u8, data), "fg_u8.data offset");
static_assert(offsetof(fg_u8, len) == offsetof(sl_u8, len), "fg_u8.len offset");
#endif

namespace {

std::atomic<long long> g_goose_calls{0};

#ifdef FG_GOOSE_STREAM
// The layouts are asserted equal above; these are the casts, written as copies
// so no reinterpret_cast appears on the call path.
sl_i64 to_sl(fg_i64 v) {
    sl_i64 s;
    s.data = v.data;
    s.len = v.len;
    return s;
}

sl_u8 to_su(fg_u8 v) {
    sl_u8 s;
    s.data = v.data;
    s.len = v.len;
    return s;
}
#endif

constexpr int64_t NIL = -1;
constexpr int64_t MAX_EXPS = 3; // MoeRecipe::max_exps

// expert_stream_source.cpp:737-745 — one entry is every projection of one
// expert; an absent projection of a fused layout contributes 0.
int64_t entry_bytes(const int64_t * lay, int64_t il) {
    int64_t bytes = 0;
    for (int64_t p = 0; p < MAX_EXPS; ++p) bytes += lay[il * 4 + 1 + p];
    return bytes;
}

// expert_stream_source.cpp:709-719 — the intrusive list unlink.
void lru_unlink(int64_t * prev, int64_t * next, int64_t * st, int64_t id) {
    const int64_t pv = prev[id], nx = next[id];
    if (pv != NIL) {
        next[pv] = nx;
    } else {
        st[0] = nx; // chead_
    }
    if (nx != NIL) {
        prev[nx] = pv;
    } else {
        st[1] = pv; // ctail_
    }
    prev[id] = next[id] = NIL;
}

// expert_stream_source.cpp:721-730 — MRU insert.
void lru_push_front(int64_t * prev, int64_t * next, int64_t * st, int64_t id) {
    prev[id] = NIL;
    next[id] = st[0];
    if (st[0] != NIL) {
        prev[st[0]] = id;
    } else {
        st[1] = id;
    }
    st[0] = id;
}

// expert_stream_source.cpp:761-772 — one eviction of the coldest entry, with
// the caller's `ctail_ != -1` guard folded in so an empty cache answers -1
// instead of touching id -1.
int64_t evict_tail(const int64_t * lay, int64_t * cv, int64_t * cs, int64_t * prev,
                   int64_t * next, int64_t * ever, int64_t * st, int64_t * sacc,
                   int64_t n_expert) {
    if (st[1] == NIL) return NIL;
    const int64_t id = st[1];
    ever[id] = 1;   // the next read of this entry pays twice
    cv[id] = 0;
    cs[id] = 0;
    sacc[0] -= entry_bytes(lay, id / n_expert); // cresident_
    lru_unlink(prev, next, st, id);
    return id;
}

// expert_stream_source.cpp:675-700 — set_cache_budget. Growing never evicts and
// is clamped to the whole expert set; a shrink is floored at one byte and
// evicts down to the new ceiling; cache_max_ == 0 at entry is a no-op.
int64_t set_budget(const int64_t * lay, int64_t * cv, int64_t * cs, int64_t * prev,
                   int64_t * next, int64_t * ever, int64_t * st, int64_t * sacc,
                   int64_t budget, int64_t cur, int64_t total, int64_t n_expert) {
    if (cur == 0) return cur; // initialised off: the LRU buffers do not exist
    if (budget > cur) return budget <= total ? budget : total;
    if (budget == cur) return cur;
    const int64_t b = budget < 1 ? 1 : budget;
    while (sacc[0] > b && st[1] != NIL)
        evict_tail(lay, cv, cs, prev, next, ever, st, sacc, n_expert);
    return b;
}

} // namespace

// --- native transcription ---------------------------------------------------

// expert_stream_source.cpp:917-945 — touch_entry, with the commit outcome as an
// argument. `promote` = 0 skips the hit-path LRU move; `co_proj` selects the
// commit-only projection on a miss and is unused here for the same reason it is
// unused in the Goose port: a single commit outcome stands for the syscalls.
int64_t fg_native_xl_touch(fg_i64 lay, fg_i64 cv, fg_i64 cs, fg_i64 prev, fg_i64 next,
                           fg_i64 ever, fg_i64 st, fg_i64 sacc, int64_t n_expert,
                           int64_t il, int64_t e, int64_t promote, int64_t co_proj,
                           int64_t commit_ok) {
    (void) co_proj;
    const int64_t id = il * n_expert + e;
    sacc.data[2]++; // clookups_
    if (cv.data[id]) {
        sacc.data[1]++; // chits_
        if (cs.data[id]) {
            sacc.data[4]++; // spec_useful_
            cs.data[id] = 0;
        }
        if (promote) {
            lru_unlink(prev.data, next.data, st.data, id);
            lru_push_front(prev.data, next.data, st.data, id);
        }
        return 1;
    }
    if (commit_ok == 0) return NIL;
    if (ever.data[id]) sacc.data[3]++; // rereads_
    cv.data[id] = 1;
    cs.data[id] = 0;
    sacc.data[0] += entry_bytes(lay.data, id / n_expert); // cresident_
    lru_push_front(prev.data, next.data, st.data, id);
    return 0;
}

// expert_stream_source.cpp:761 — evict_tail. The native function is the body of
// the loop above; the entry it returns is the id it unlinked.
int64_t fg_native_xl_evict_tail(fg_i64 lay, fg_i64 cv, fg_i64 cs, fg_i64 prev,
                                fg_i64 next, fg_i64 ever, fg_i64 st, fg_i64 sacc,
                                int64_t n_expert) {
    return evict_tail(lay.data, cv.data, cs.data, prev.data, next.data, ever.data,
                      st.data, sacc.data, n_expert);
}

// expert_stream_source.cpp:675 — set_cache_budget; returns cache_max_ in force.
int64_t fg_native_xl_set_budget(fg_i64 lay, fg_i64 cv, fg_i64 cs, fg_i64 prev,
                                fg_i64 next, fg_i64 ever, fg_i64 st, fg_i64 sacc,
                                int64_t budget, int64_t cur, int64_t total,
                                int64_t cache_off, int64_t n_expert) {
    // The Goose export takes the "cache_max_ == 0 at entry" fact as a flag; the
    // native condition is the same fact read off cur. cache_off and cur == 0
    // must therefore agree on the way in, and the gate always builds the pair.
    (void) cache_off;
    return set_budget(lay.data, cv.data, cs.data, prev.data, next.data, ever.data,
                      st.data, sacc.data, budget, cur, total, n_expert);
}

// dense_weights.cpp:98-110 — subtract_range: remove [a, b) from a sorted list of
// disjoint ranges, splitting the one range it falls in.
int64_t fg_native_dw_subtract(fg_i64 ra, fg_i64 rb, int64_t n, int64_t a, int64_t b,
                              fg_i64 oa, fg_i64 ob) {
    int64_t k = 0;
    for (int64_t i = 0; i < n; ++i) {
        if (b <= ra.data[i] || a >= rb.data[i]) { // disjoint
            oa.data[k] = ra.data[i];
            ob.data[k] = rb.data[i];
            ++k;
            continue;
        }
        if (ra.data[i] < a) {
            oa.data[k] = ra.data[i];
            ob.data[k] = a;
            ++k;
        }
        if (b < rb.data[i]) {
            oa.data[k] = b;
            ob.data[k] = rb.data[i];
            ++k;
        }
    }
    return k;
}

// expert_stream_source.cpp:1188-1196 — the job list of one wave, projection
// major over the ascending staged set, hits excluded because they are already
// ready. `wave_mask` bit p is L.proj[p].nb2 != 0.
int64_t fg_native_ov_wave_list(int64_t wave_mask, fg_i64 staged, int64_t n_staged,
                               fg_u8 miss, fg_i64 out) {
    int64_t k = 0;
    for (int64_t p = 0; p < MAX_EXPS; ++p) {
        if ((wave_mask & (int64_t{1} << p)) == 0) continue;
        for (int64_t i = 0; i < n_staged; ++i) {
            if (miss.data[i] == 0) continue; // cache hit, already marked ready
            if (2 * k + 1 < out.len) {
                out.data[2 * k] = p;
                out.data[2 * k + 1] = staged.data[i];
            }
            ++k;
        }
    }
    return k;
}

// router_hook.cpp:195-200 — row_gathered_weights() is the set difference
// gathered MINUS disqualified; one reference that is not a table-position row
// gather vetoes the table.
int64_t fg_native_rs_table_verdict(int64_t gathered_refs, int64_t disq_refs) {
    return gathered_refs > 0 && disq_refs == 0 ? 1 : 0;
}

// --- the switched entry points ---------------------------------------------

int64_t fg_xl_touch(fg_i64 lay, fg_i64 cv, fg_i64 cs, fg_i64 prev, fg_i64 next,
                    fg_i64 ever, fg_i64 st, fg_i64 sacc, int64_t n_expert,
                    int64_t il, int64_t e, int64_t promote, int64_t co_proj,
                    int64_t commit_ok) {
#ifdef FG_GOOSE_STREAM
    g_goose_calls.fetch_add(1, std::memory_order_relaxed);
    return xl_touch(to_sl(lay), to_sl(cv), to_sl(cs), to_sl(prev), to_sl(next),
                    to_sl(ever), to_sl(st), to_sl(sacc), n_expert, il, e, promote, co_proj,
                    commit_ok);
#else
    return fg_native_xl_touch(lay, cv, cs, prev, next, ever, st, sacc, n_expert, il, e,
                              promote, co_proj, commit_ok);
#endif
}

int64_t fg_xl_evict_tail(fg_i64 lay, fg_i64 cv, fg_i64 cs, fg_i64 prev, fg_i64 next,
                         fg_i64 ever, fg_i64 st, fg_i64 sacc, int64_t n_expert) {
#ifdef FG_GOOSE_STREAM
    g_goose_calls.fetch_add(1, std::memory_order_relaxed);
    return xl_evict_tail(to_sl(lay), to_sl(cv), to_sl(cs), to_sl(prev), to_sl(next),
                         to_sl(ever), to_sl(st), to_sl(sacc), n_expert);
#else
    return fg_native_xl_evict_tail(lay, cv, cs, prev, next, ever, st, sacc, n_expert);
#endif
}

int64_t fg_xl_set_budget(fg_i64 lay, fg_i64 cv, fg_i64 cs, fg_i64 prev, fg_i64 next,
                         fg_i64 ever, fg_i64 st, fg_i64 sacc, int64_t budget,
                         int64_t cur, int64_t total, int64_t cache_off,
                         int64_t n_expert) {
#ifdef FG_GOOSE_STREAM
    g_goose_calls.fetch_add(1, std::memory_order_relaxed);
    return xl_set_budget(to_sl(lay), to_sl(cv), to_sl(cs), to_sl(prev), to_sl(next),
                         to_sl(ever), to_sl(st), to_sl(sacc), budget, cur, total, cache_off,
                         n_expert);
#else
    return fg_native_xl_set_budget(lay, cv, cs, prev, next, ever, st, sacc, budget, cur,
                                   total, cache_off, n_expert);
#endif
}

int64_t fg_dw_subtract(fg_i64 ra, fg_i64 rb, int64_t n, int64_t a, int64_t b,
                       fg_i64 oa, fg_i64 ob) {
#ifdef FG_GOOSE_STREAM
    g_goose_calls.fetch_add(1, std::memory_order_relaxed);
    return dw_subtract(to_sl(ra), to_sl(rb), n, a, b, to_sl(oa), to_sl(ob));
#else
    return fg_native_dw_subtract(ra, rb, n, a, b, oa, ob);
#endif
}

int64_t fg_ov_wave_list(int64_t wave_mask, fg_i64 staged, int64_t n_staged,
                        fg_u8 miss, fg_i64 out) {
#ifdef FG_GOOSE_STREAM
    g_goose_calls.fetch_add(1, std::memory_order_relaxed);
    return ov_wave_list(wave_mask, to_sl(staged), n_staged, to_su(miss), to_sl(out));
#else
    return fg_native_ov_wave_list(wave_mask, staged, n_staged, miss, out);
#endif
}

int64_t fg_rs_table_verdict(int64_t gathered_refs, int64_t disq_refs) {
#ifdef FG_GOOSE_STREAM
    g_goose_calls.fetch_add(1, std::memory_order_relaxed);
    return rs_table_verdict(gathered_refs, disq_refs);
#else
    return fg_native_rs_table_verdict(gathered_refs, disq_refs);
#endif
}

void fg_stream_init(void) {
#ifdef FG_GOOSE_STREAM
    goose_init(0, nullptr);
#endif
}

int64_t fg_stream_goose_calls(void) {
    return (int64_t) g_goose_calls.load(std::memory_order_relaxed);
}

const char * fg_stream_mode(void) {
#ifdef FG_GOOSE_STREAM
    return "goose";
#else
    return "native";
#endif
}
