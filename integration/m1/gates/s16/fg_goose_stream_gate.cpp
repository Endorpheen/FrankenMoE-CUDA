// S1.6 differential gate: the switched stream decisions against the native
// transcription, in one process, with synthetic tables and schedules only.
//
// No model, no GPU, no disk: every case is an array of integers, and the
// compared artefacts are the return value plus every mutated array byte for
// byte. Each case runs the selected implementation (Goose when
// FG_GOOSE_STREAM=ON) on one copy of the state and the native transcription on
// an identical copy, then diffs the two.
//
//   xl_ touch/promote    12 cases
//   xl_ evict            11 cases
//   xl_ budget           12 cases
//   dw_ subtract         12 cases
//   ov_ wave list        12 cases
//   rs_ table verdict    12 cases
//
// Output: one `case=...` line per case (identical between an OFF and an ON
// build except the summary) and one FG641_SMOKE summary line.
#include "fg-stream-policy.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace {

constexpr int64_t NIL = -1;
constexpr int64_t MAX_EXPS = 3;

int g_cases = 0;
int g_failures = 0;
// How many times the gate asked the SELECTED implementation for an answer.
// Compared with the bridge's own counter, so a lost or doubled dispatch is a
// failure and not a silent pass.
int64_t g_selected_calls = 0;
int g_group_cases[6] = {0, 0, 0, 0, 0, 0};
int g_group_failures[6] = {0, 0, 0, 0, 0, 0};
const char * const GROUP_NAMES[6] = {"xl_touch", "xl_evict", "xl_budget",
                                     "dw_subtract", "ov_wave", "rs_verdict"};

uint64_t fnv(uint64_t h, const void * p, size_t n) {
    const uint8_t * b = (const uint8_t *) p;
    for (size_t i = 0; i < n; ++i) {
        h ^= b[i];
        h *= 1099511628211ull;
    }
    return h;
}

uint64_t fnv_vec(uint64_t h, const std::vector<int64_t> & v) {
    return fnv(h, v.data(), v.size() * sizeof(int64_t));
}

fg_i64 S(std::vector<int64_t> & v) { return fg_i64{v.data(), (int64_t) v.size()}; }
fg_i64 S(const std::vector<int64_t> & v) {
    return fg_i64{const_cast<int64_t *>(v.data()), (int64_t) v.size()};
}
fg_i64 S(int64_t * p, int64_t n) { return fg_i64{p, n}; }
fg_u8 U(std::vector<uint8_t> & v) { return fg_u8{v.data(), (int64_t) v.size()}; }

void report(int group, const char * name, bool ok, int64_t ret, uint64_t digest) {
    ++g_cases;
    ++g_group_cases[group];
    if (!ok) {
        ++g_failures;
        ++g_group_failures[group];
    }
    std::printf("case=%s/%s ret=%lld digest=%016llx ok=%d\n", GROUP_NAMES[group], name,
                (long long) ret, (unsigned long long) digest, ok ? 1 : 0);
}

// --- xl_ state -------------------------------------------------------------
//
// lay  per layer: bound, nb2 of projections 0..2 (expert_stream_source.cpp:737)
// cv   cvalid_, cs cspec_, prev/next the intrusive list, ever ever_evicted_
// st   {chead, ctail}
// sacc {cresident, chits, clookups, rereads, spec_useful}
struct Xl {
    int64_t n_expert;
    int64_t n_layer;
    std::vector<int64_t> lay;
    std::vector<int64_t> cv, cs, prev, next, ever;
    int64_t st[2] = {NIL, NIL};
    int64_t sacc[5] = {0, 0, 0, 0, 0};

    Xl(int64_t ne, int64_t nl) : n_expert(ne), n_layer(nl) {
        lay.assign((size_t) (nl * 4), 0);
        const size_t n = (size_t) (ne * nl);
        cv.assign(n, 0);
        cs.assign(n, 0);
        prev.assign(n, NIL);
        next.assign(n, NIL);
        ever.assign(n, 0);
    }

    void layer(int64_t il, int64_t bound, int64_t p0, int64_t p1, int64_t p2) {
        lay[il * 4] = bound;
        lay[il * 4 + 1] = p0;
        lay[il * 4 + 2] = p1;
        lay[il * 4 + 3] = p2;
    }

    int64_t bytes_of(int64_t id) const {
        const int64_t il = id / n_expert;
        return lay[il * 4 + 1] + lay[il * 4 + 2] + lay[il * 4 + 3];
    }

    // Make `ids` resident and put them on the list in the given MRU -> LRU
    // order, with cresident_ equal to their byte total. Host plumbing: it sets
    // up the state the decisions read, it decides nothing.
    void seed(const std::vector<int64_t> & ids) {
        int64_t bytes = 0;
        for (size_t i = 0; i < ids.size(); ++i) {
            const int64_t id = ids[i];
            cv[id] = 1;
            prev[id] = i == 0 ? NIL : ids[i - 1];
            next[id] = i + 1 == ids.size() ? NIL : ids[i + 1];
            bytes += bytes_of(id);
        }
        st[0] = ids.empty() ? NIL : ids.front();
        st[1] = ids.empty() ? NIL : ids.back();
        sacc[0] = bytes;
    }
};

bool same(const Xl & a, const Xl & b) {
    return a.cv == b.cv && a.cs == b.cs && a.prev == b.prev && a.next == b.next &&
           a.ever == b.ever && a.st[0] == b.st[0] && a.st[1] == b.st[1] &&
           std::memcmp(a.sacc, b.sacc, sizeof a.sacc) == 0;
}

uint64_t digest(const Xl & a) {
    uint64_t h = 1469598103934665603ull;
    h = fnv_vec(h, a.lay);
    h = fnv_vec(h, a.cv);
    h = fnv_vec(h, a.cs);
    h = fnv_vec(h, a.prev);
    h = fnv_vec(h, a.next);
    h = fnv_vec(h, a.ever);
    h = fnv(h, a.st, sizeof a.st);
    h = fnv(h, a.sacc, sizeof a.sacc);
    return h;
}

using XlOp = std::function<int64_t(Xl &, bool)>;

void xl_case(int group, const char * name, const Xl & base, const XlOp & op) {
    Xl a = base, b = base; // `a` runs the selected path, `b` the native one
    const int64_t ra = op(a, false);
    const int64_t rb = op(b, true);
    report(group, name, ra == rb && same(a, b), ra, digest(a));
}

// --- dw_ state -------------------------------------------------------------
struct Ranges {
    int64_t n;
    std::vector<int64_t> ra, rb;
    std::vector<int64_t> oa, ob; // oversized, filled with a sentinel first

    uint64_t digest() const {
        uint64_t h = 1469598103934665603ull;
        h = fnv_vec(h, oa);
        h = fnv_vec(h, ob);
        return h;
    }
};

// --- ov_ state -------------------------------------------------------------
struct Wave {
    int64_t mask;
    std::vector<int64_t> staged;
    std::vector<uint8_t> miss;
    std::vector<int64_t> out;

    uint64_t digest() const {
        uint64_t h = 1469598103934665603ull;
        h = fnv(h, out.data(), out.size() * sizeof(int64_t));
        return h;
    }
};

// --- xl_ touch/promote -----------------------------------------------------

int64_t touch(Xl & s, bool native, int64_t il, int64_t e, int64_t promote, int64_t co_proj,
              int64_t commit_ok) {
    if (native) {
        return fg_native_xl_touch(S(s.lay), S(s.cv), S(s.cs), S(s.prev), S(s.next), S(s.ever),
                                  S(s.st, 2), S(s.sacc, 5), s.n_expert, il, e, promote,
                                  co_proj, commit_ok);
    }
    ++g_selected_calls;
    return fg_xl_touch(S(s.lay), S(s.cv), S(s.cs), S(s.prev), S(s.next), S(s.ever), S(s.st, 2),
                       S(s.sacc, 5), s.n_expert, il, e, promote, co_proj, commit_ok);
}

void group_touch() {
    const int64_t ne = 4;

    // A cache of two layers x four experts, three entries resident in layer 0.
    auto base = [&] {
        Xl s(ne, 2);
        s.layer(0, 1, 64, 64, 64);
        s.layer(1, 1, 128, 0, 0); // fused layout: projection 2 absent
        return s;
    };

    { // 1: hit on the MRU entry, promote moves nothing
        Xl s = base();
        s.seed({0, 1, 2});
        xl_case(0, "hit_promote_mru", s, [&](Xl & t, bool n) { return touch(t, n, 0, 0, 1, -1, 1); });
    }
    { // 2: hit on the coldest entry, promote is a real move
        Xl s = base();
        s.seed({0, 1, 2});
        xl_case(0, "hit_promote_tail", s, [&](Xl & t, bool n) { return touch(t, n, 0, 2, 1, -1, 1); });
    }
    { // 3: same hit with promote=0: the order must not move
        Xl s = base();
        s.seed({0, 1, 2});
        xl_case(0, "hit_no_promote", s, [&](Xl & t, bool n) { return touch(t, n, 0, 2, 0, -1, 1); });
    }
    { // 4: a hit served by a speculative prefetch clears cspec_ and counts once
        Xl s = base();
        s.seed({0, 1, 2});
        s.cs[1] = 1;
        xl_case(0, "hit_spec_useful", s, [&](Xl & t, bool n) { return touch(t, n, 0, 1, 0, -1, 1); });
    }
    { // 5: miss, the commit succeeds: the entry enters at the MRU end
        Xl s = base();
        s.seed({0, 1, 2});
        xl_case(0, "miss_commit", s, [&](Xl & t, bool n) { return touch(t, n, 0, 3, 1, -1, 1); });
    }
    { // 6: miss, the commit fails: nothing changes, not even the counters
        Xl s = base();
        s.seed({0, 1, 2});
        xl_case(0, "miss_commit_fail", s, [&](Xl & t, bool n) { return touch(t, n, 0, 3, 1, -1, 0); });
    }
    { // 7: a miss of an entry that was resident before is the cache paying twice
        Xl s = base();
        s.seed({0, 1, 2});
        s.ever[3] = 1;
        xl_case(0, "miss_reread", s, [&](Xl & t, bool n) { return touch(t, n, 0, 3, 0, -1, 1); });
    }
    { // 8: fused layout, only projection 0 present: 128 bytes, not 3 x 128
        Xl s = base();
        s.seed({5}); // layer 1, expert 1
        xl_case(0, "miss_fused_bytes", s, [&](Xl & t, bool n) { return touch(t, n, 1, 2, 0, -1, 1); });
    }
    { // 9: hit on a one-element list, promote=1: unlink and front on the same id
        Xl s = base();
        s.seed({2});
        xl_case(0, "hit_single", s, [&](Xl & t, bool n) { return touch(t, n, 0, 2, 1, -1, 1); });
    }
    { // 10: commit_only_proj is an argument on both sides and changes no state
        Xl s = base();
        s.seed({0, 1});
        xl_case(0, "miss_commit_only_p0", s,
                [&](Xl & t, bool n) { return touch(t, n, 0, 2, 1, 0, 1); });
    }
    { // 11: empty cache, first miss of the run
        Xl s = base();
        xl_case(0, "miss_empty_cache", s,
                [&](Xl & t, bool n) { return touch(t, n, 1, 3, 1, -1, 1); });
    }
    { // 12: id arithmetic across layers: layer 1 expert 3 is id 7, not 3
        Xl s = base();
        s.seed({5, 6}); // layer 1 experts 1, 2
        xl_case(0, "hit_other_layer", s,
                [&](Xl & t, bool n) { return touch(t, n, 1, 3, 1, -1, 1); });
    }
}

// --- xl_ evict -------------------------------------------------------------

int64_t evict(Xl & s, bool native) {
    if (native) {
        return fg_native_xl_evict_tail(S(s.lay), S(s.cv), S(s.cs), S(s.prev), S(s.next),
                                       S(s.ever), S(s.st, 2), S(s.sacc, 5), s.n_expert);
    }
    ++g_selected_calls;
    return fg_xl_evict_tail(S(s.lay), S(s.cv), S(s.cs), S(s.prev), S(s.next), S(s.ever),
                            S(s.st, 2), S(s.sacc, 5), s.n_expert);
}

void group_evict() {
    const int64_t ne = 4;
    auto base = [&] {
        Xl s(ne, 2);
        s.layer(0, 1, 64, 64, 64);
        s.layer(1, 1, 128, 0, 32);
        return s;
    };

    { // 1: empty cache: -1, nothing written
        Xl s = base();
        xl_case(1, "empty", s, [&](Xl & t, bool n) { return evict(t, n); });
    }
    { // 2: the only entry leaves, the list empties, cresident_ goes to zero
        Xl s = base();
        s.seed({2});
        xl_case(1, "single", s, [&](Xl & t, bool n) { return evict(t, n); });
    }
    { // 3: three entries: the coldest goes, the other two keep their order
        Xl s = base();
        s.seed({0, 1, 2});
        xl_case(1, "coldest_of_three", s, [&](Xl & t, bool n) { return evict(t, n); });
    }
    { // 4: two evictions in a row, as set_cache_budget's loop does
        Xl s = base();
        s.seed({0, 1, 2});
        xl_case(1, "twice", s, [&](Xl & t, bool n) {
            const int64_t r1 = evict(t, n);
            const int64_t r2 = evict(t, n);
            return r1 * 100 + r2;
        });
    }
    { // 5: fused layer, partial bytes: cresident_ drops by 160, not by 384
        Xl s = base();
        s.seed({5, 6, 7}); // layer 1: 128 + 0 + 32
        xl_case(1, "fused_bytes", s, [&](Xl & t, bool n) { return evict(t, n); });
    }
    { // 6: evict until empty, then one more call
        Xl s = base();
        s.seed({1, 2});
        xl_case(1, "until_empty", s, [&](Xl & t, bool n) {
            const int64_t r1 = evict(t, n);
            const int64_t r2 = evict(t, n);
            const int64_t r3 = evict(t, n);
            return r1 + r2 * 10 + r3 * 100;
        });
    }
    { // 7: the head is untouched when the tail leaves
        Xl s = base();
        s.seed({0, 1, 2, 3});
        xl_case(1, "head_survives", s, [&](Xl & t, bool n) { return evict(t, n); });
    }
    { // 8: an evicted speculative entry loses cspec_ as well as cvalid_
        Xl s = base();
        s.seed({0, 1});
        s.cs[1] = 1;
        xl_case(1, "spec_cleared", s, [&](Xl & t, bool n) { return evict(t, n); });
    }
    { // 9: an entry that was ever evicted stays marked, and evicting it again is fine
        Xl s = base();
        s.seed({0, 1});
        s.ever[1] = 1;
        xl_case(1, "ever_set", s, [&](Xl & t, bool n) { return evict(t, n); });
    }
    { // 10: ids outside the list are untouched by an eviction
        Xl s = base();
        s.seed({0, 3});
        s.cv[1] = 0; // never resident
        xl_case(1, "bystanders", s, [&](Xl & t, bool n) { return evict(t, n); });
    }
    { // 11: interleaved layers: layer 1 expert 0 is id 4
        Xl s = base();
        s.seed({4, 5, 7});
        xl_case(1, "layer1_tail", s, [&](Xl & t, bool n) { return evict(t, n); });
    }
}

// --- xl_ budget ------------------------------------------------------------

int64_t budget(Xl & s, bool native, int64_t b, int64_t cur, int64_t total) {
    const int64_t off = cur == 0 ? 1 : 0;
    if (native) {
        return fg_native_xl_set_budget(S(s.lay), S(s.cv), S(s.cs), S(s.prev), S(s.next),
                                       S(s.ever), S(s.st, 2), S(s.sacc, 5), b, cur, total,
                                       off, s.n_expert);
    }
    ++g_selected_calls;
    return fg_xl_set_budget(S(s.lay), S(s.cv), S(s.cs), S(s.prev), S(s.next), S(s.ever),
                            S(s.st, 2), S(s.sacc, 5), b, cur, total, off, s.n_expert);
}

void group_budget() {
    const int64_t ne = 4;
    const int64_t total = 3 * 4 * 192; // 3 layers x 4 experts x 192 bytes
    auto base = [&] {
        Xl s(ne, 3);
        s.layer(0, 1, 64, 64, 64);
        s.layer(1, 1, 64, 64, 64);
        s.layer(2, 1, 128, 64, 0);
        return s;
    };
    // every entry of layer 0 and 1 costs 192 bytes, of layer 2 costs 192 too

    { // 1: grow below the whole expert set
        Xl s = base();
        s.seed({0, 1});
        xl_case(2, "grow", s, [&](Xl & t, bool n) { return budget(t, n, 1000, 400, total); });
    }
    { // 2: grow past the whole set is clamped, nothing is evicted
        Xl s = base();
        s.seed({0, 1});
        xl_case(2, "grow_clamped", s, [&](Xl & t, bool n) { return budget(t, n, 999999, 400, total); });
    }
    { // 3: grow to exactly the whole set
        Xl s = base();
        s.seed({0});
        xl_case(2, "grow_exact", s, [&](Xl & t, bool n) { return budget(t, n, total, 192, total); });
    }
    { // 4: equal budget: no work at all
        Xl s = base();
        s.seed({0, 1});
        xl_case(2, "equal", s, [&](Xl & t, bool n) { return budget(t, n, 400, 400, total); });
    }
    { // 5: cache off at entry: no-op, the budget stays zero
        Xl s = base();
        xl_case(2, "cache_off", s, [&](Xl & t, bool n) { return budget(t, n, 1000, 0, total); });
    }
    { // 6: shrink that fits: no eviction
        Xl s = base();
        s.seed({0, 1});
        xl_case(2, "shrink_fits", s, [&](Xl & t, bool n) { return budget(t, n, 384, 400, total); });
    }
    { // 7: shrink evicting one entry
        Xl s = base();
        s.seed({0, 1, 2});
        xl_case(2, "shrink_one", s, [&](Xl & t, bool n) { return budget(t, n, 450, 576, total); });
    }
    { // 8: shrink to zero: floored at one byte, evicts everything
        Xl s = base();
        s.seed({0, 1, 2});
        xl_case(2, "shrink_zero_floor", s, [&](Xl & t, bool n) { return budget(t, n, 0, 576, total); });
    }
    { // 9: shrink to exactly the resident bytes
        Xl s = base();
        s.seed({0, 1, 2});
        xl_case(2, "shrink_equal_resident", s,
                [&](Xl & t, bool n) { return budget(t, n, 576, 600, total); });
    }
    { // 10: shrink one byte under the resident set
        Xl s = base();
        s.seed({0, 1, 2});
        xl_case(2, "shrink_under", s, [&](Xl & t, bool n) { return budget(t, n, 575, 600, total); });
    }
    { // 11: negative request is floored, not clamped to the total
        Xl s = base();
        s.seed({0, 1});
        xl_case(2, "shrink_negative", s, [&](Xl & t, bool n) { return budget(t, n, -64, 400, total); });
    }
    { // 12: shrink with an empty cache: nothing to evict, the ceiling still moves
        Xl s = base();
        xl_case(2, "shrink_empty", s, [&](Xl & t, bool n) { return budget(t, n, 300, 400, total); });
    }
}

// --- dw_ subtract ----------------------------------------------------------

void dw_case(const char * name, const Ranges & base, int64_t a, int64_t b) {
    Ranges x = base, y = base;
    ++g_selected_calls;
    const int64_t rx = fg_dw_subtract(S(x.ra), S(x.rb), x.n, a, b, S(x.oa), S(x.ob));
    const int64_t ry = fg_native_dw_subtract(S(y.ra), S(y.rb), y.n, a, b, S(y.oa), S(y.ob));
    report(3, name, rx == ry && x.oa == y.oa && x.ob == y.ob && x.digest() == y.digest(), rx,
           x.digest());
}

Ranges ranges(std::vector<int64_t> a, std::vector<int64_t> b) {
    Ranges r;
    r.n = (int64_t) a.size();
    r.ra = std::move(a);
    r.rb = std::move(b);
    r.oa.assign(r.ra.size() + 2, -777);
    r.ob.assign(r.ra.size() + 2, -777);
    return r;
}

void group_subtract() {
    { // 1: empty range list
        dw_case("empty", ranges({}, {}), 10, 20);
    }
    { // 2: the subtraction is entirely before every range
        dw_case("before", ranges({100, 200}, {150, 260}), 10, 20);
    }
    { // 3: entirely after every range
        dw_case("after", ranges({100, 200}, {150, 260}), 300, 400);
    }
    { // 4: inside the gap between two ranges
        dw_case("in_gap", ranges({100, 300}, {150, 350}), 200, 250);
    }
    { // 5: splits one range in the middle: two outputs
        dw_case("split", ranges({100}, {500}), 200, 300);
    }
    { // 6: covers one whole range, leaves the neighbour
        dw_case("cover_one", ranges({100, 300}, {200, 400}), 90, 210);
    }
    { // 7: head overlap
        dw_case("head", ranges({100}, {200}), 50, 150);
    }
    { // 8: tail overlap
        dw_case("tail", ranges({100}, {200}), 150, 250);
    }
    { // 9: zero-length subtraction inside a range still splits it
        dw_case("zero_length", ranges({100}, {200}), 150, 150);
    }
    { // 10: adjacent ranges, subtraction straddling both
        dw_case("straddle", ranges({100, 200}, {200, 300}), 150, 250);
    }
    { // 11: boundary-exact subtraction of the first range
        dw_case("exact_first", ranges({100, 500}, {200, 600}), 100, 200);
    }
    { // 12: many ranges, one hit in the middle of the list
        dw_case("many", ranges({0, 100, 200, 300, 400}, {50, 150, 250, 350, 450}), 220, 230);
    }
}

// --- ov_ wave list ---------------------------------------------------------

void ov_case(const char * name, const Wave & base) {
    Wave x = base, y = base;
    ++g_selected_calls;
    const int64_t rx = fg_ov_wave_list(x.mask, S(x.staged), (int64_t) x.staged.size(), U(x.miss),
                                       S(x.out));
    const int64_t ry = fg_native_ov_wave_list(y.mask, S(y.staged), (int64_t) y.staged.size(),
                                              U(y.miss), S(y.out));
    report(4, name, rx == ry && x.out == y.out && x.digest() == y.digest(), rx, x.digest());
}

Wave wave(int64_t mask, std::vector<int64_t> staged, std::vector<uint8_t> miss) {
    Wave w;
    w.mask = mask;
    w.staged = std::move(staged);
    w.miss = std::move(miss);
    w.out.assign(w.staged.size() * MAX_EXPS * 2 + 4, -777);
    return w;
}

void group_wave() {
    { // 1: wave one only (the first present projection), every expert missed
        ov_case("p0_only", wave(1, {2, 5, 7}, {1, 1, 1}));
    }
    { // 2: every projection present, every expert missed: projection major
        ov_case("all_proj", wave(7, {2, 5, 7}, {1, 1, 1}));
    }
    { // 3: every projection, mixed hits: hits schedule nothing
        ov_case("mixed_hits", wave(7, {2, 5, 7}, {1, 0, 1, 0, 0, 0, 0, 0}));
    }
    { // 4: no projection present
        ov_case("no_proj", wave(0, {2, 5}, {1, 1}));
    }
    { // 5: every staged expert hit the cache
        ov_case("all_hits", wave(7, {2, 5}, {0, 0}));
    }
    { // 6: one staged expert
        ov_case("single", wave(7, {3}, {1}));
    }
    { // 7: two-wave second wave: projection 1 only, after wave one published
        ov_case("wave_two", wave(2, {1, 4, 9}, {1, 1, 1}));
    }
    { // 8: fused layout: projections 0 and 2 present, 1 absent
        ov_case("fused_mask", wave(5, {0, 1}, {1, 1}));
    }
    { // 9: expert id 0 is a valid expert, not a terminator
        ov_case("expert_zero", wave(7, {0, 1, 2}, {1, 0, 1, 0, 0, 0, 0, 0}));
    }
    { // 10: an eight-expert layer with alternating hits
        ov_case("wide", wave(7, {0, 1, 2, 3, 4, 5, 6, 7}, {1, 0, 1, 0, 1, 0, 1, 0}));
    }
    { // 11: nothing staged
        ov_case("empty", wave(7, {}, {}));
    }
    { // 12: mask with only the last projection (a layout without gate_up)
        ov_case("tail_only", wave(4, {6, 1, 3}, {1, 1, 0}));
    }
}

// --- rs_ table verdict -----------------------------------------------------
//
// One reference is a triple (get_rows, slot, idx_ok). The table's counts are
// host plumbing over the native rule from router_hook.cpp:1427-1433: only the
// TABLE position (slot 0) of a row gather whose index is already materialized
// counts as gathered, everything else is disqualified.
struct Ref {
    int64_t get_rows, slot, idx_ok;
};

void rs_case(const char * name, const std::vector<Ref> & refs) {
    int64_t gathered = 0, disq = 0;
    for (const Ref & r : refs) {
        if (r.get_rows != 0 && r.slot == 0 && r.idx_ok != 0) {
            ++gathered;
        } else {
            ++disq;
        }
    }
    ++g_selected_calls;
    const int64_t rx = fg_rs_table_verdict(gathered, disq);
    const int64_t ry = fg_native_rs_table_verdict(gathered, disq);
    report(5, name, rx == ry, rx, (uint64_t) (gathered * 1000 + disq));
}

void group_verdict() {
    rs_case("no_refs", {});
    rs_case("one_gather", {{1, 0, 1}});
    rs_case("two_gathers", {{1, 0, 1}, {1, 0, 1}});
    rs_case("gather_plus_multiply", {{1, 0, 1}, {0, 0, 0}});
    rs_case("gather_wrong_slot", {{1, 0, 1}, {1, 1, 1}});
    rs_case("gather_unmaterialized_idx", {{1, 0, 1}, {1, 0, 0}});
    rs_case("only_disq", {{0, 0, 0}});
    rs_case("five_gathers", {{1, 0, 1}, {1, 0, 1}, {1, 0, 1}, {1, 0, 1}, {1, 0, 1}});
    rs_case("three_gathers_one_veto", {{1, 0, 1}, {1, 0, 1}, {1, 0, 1}, {0, 0, 0}});
    rs_case("weight_only", {{0, 0, 0}, {0, 1, 0}});
    rs_case("gather_slot_zero_only", {{1, 0, 1}, {1, 0, 1}, {1, 2, 1}});
    rs_case("hundred_gathers", [] {
        std::vector<Ref> v;
        for (int i = 0; i < 100; ++i) v.push_back({1, 0, 1});
        return v;
    }());
}

} // namespace

int main() {
    fg_stream_init();

    group_touch();
    group_evict();
    group_budget();
    group_subtract();
    group_wave();
    group_verdict();

    const int64_t calls = fg_stream_goose_calls();
    const bool goose = std::strcmp(fg_stream_mode(), "goose") == 0;
    // On a Goose build every selected call must have entered the module; on a
    // native build none may have. g_selected_calls counts the gate's own calls.
    const int64_t want = goose ? g_selected_calls : 0;

    for (int g = 0; g < 6; ++g) {
        std::printf("group=%s cases=%d failures=%d\n", GROUP_NAMES[g], g_group_cases[g],
                    g_group_failures[g]);
    }
    if (calls != want) {
        std::printf("case=dispatch/call_count ret=%lld want=%lld digest=0 ok=0\n",
                    (long long) calls, (long long) want);
        ++g_failures;
    }
    if (g_selected_calls <= 0) {
        std::printf("case=dispatch/selected_calls ret=0 digest=0 ok=0\n");
        ++g_failures;
    }
    std::printf("FG641_SMOKE mode=%s cases=%d failures=%d goose_calls=%lld decisions=4 entries=6\n",
                fg_stream_mode(), g_cases, g_failures, (long long) calls);
    return g_failures == 0 ? 0 : 1;
}
