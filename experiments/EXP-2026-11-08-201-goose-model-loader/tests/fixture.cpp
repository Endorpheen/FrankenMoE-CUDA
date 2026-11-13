// Differential gate for the Goose model-loader decision module.
//
// The reference is the two branches of check_tensor_dims written out the way
// llama-model-loader.cpp writes them, so the comparison is against the code being replaced.
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <random>
#include <vector>
#include <algorithm>
#include <utility>

extern "C" {
int64_t fg201l_nelements(uint64_t, int64_t);
int64_t fg201l_dims_ok(uint64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
int64_t fg201l_shape_ok(int64_t, uint64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
int fg201l_shape_check(int, const int64_t *, int64_t, int64_t, const int64_t *);
int64_t fg201l_or_arr_too_many(int64_t, int64_t);
int64_t fg201l_is_array_kv(int64_t, int64_t);
int64_t fg201l_arr_length_ok(int64_t, int64_t);
int64_t fg201l_use_mmap_mode(int64_t);
int64_t fg201l_use_direct_io_mode(int64_t);
int64_t fg201l_merge_ranges(uint64_t, int64_t, uint64_t, int64_t);
int64_t fg201l_sub_ranges(uint64_t, int64_t, uint64_t, int64_t, uint64_t, int64_t);
int64_t fg201l_sort_pairs(uint64_t, int64_t);
}

static int checks, failures;
static void ck(bool ok, const char * what, long long a, long long b) {
    ++checks;
    if (!ok) {
        if (failures < 15) printf("FAIL %s: goose=%lld reference=%lld\n", what, a, b);
        ++failures;
    }
}

static void run_trap(const char * what, int expected, void (*fn)(void)) {
    int fd[2];
    if (pipe(fd) != 0) { ++failures; return; }
    pid_t pid = fork();
    if (pid == 0) { close(fd[0]); dup2(fd[1], 2); close(fd[1]); fn(); _exit(0); }
    close(fd[1]);
    char buf[256] = {0};
    ssize_t n = read(fd[0], buf, sizeof buf - 1);
    close(fd[0]);
    int st = 0;
    waitpid(pid, &st, 0);
    ++checks;
    bool died = WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT;
    long long code = (n > 0 && strstr(buf, "validation code")) ? atoll(strstr(buf, "validation code") + 16) : -1;
    if (!died || code != expected) {
        if (failures < 15) printf("FAIL trap %s: died=%d code=%lld expected=%d (%s)\n", what, died, code, expected, buf);
        ++failures;
    }
}

static void trap_nreq_oor(void) { fg201l_nelements(0, 5); }

// Native reference, transcribed.
static bool ref_dims(const int64_t d[4], const std::vector<int64_t> & ne) {
    for (int i = 0; i < 4; ++i) {
        if ((i < (int)ne.size() && ne[i] != d[i]) || (i >= (int)ne.size() && d[i] != 1)) return false;
    }
    return true;
}
static int64_t ref_nelements(const int64_t d[4]) { return d[0]*d[1]*d[2]*d[3]; }

int main(void) {
    std::mt19937 rng(201001);

    // nelements over random dims and request lengths.
    for (int t = 0; t < 200; ++t) {
        int64_t d[4];
        for (int i = 0; i < 4; ++i) d[i] = 1 + (int64_t)(rng() % 8);
        ck(fg201l_nelements((uint64_t)(uintptr_t)d, 4) == ref_nelements(d), "nelements", 0, 0);
        const int64_t n = (int64_t)(rng() % 4);
        int64_t want = 1;
        for (int i = 0; i < n; ++i) want *= d[i];
        ck(fg201l_nelements((uint64_t)(uintptr_t)d, n) == want, "nelements n", 0, 0);
    }

    // dims_ok against the native expression, over every request length 0..4.
    for (int t = 0; t < 400; ++t) {
        int64_t d[4];
        for (int i = 0; i < 4; ++i) d[i] = (int64_t)(rng() % 4);   // includes 0 and 1
        std::vector<int64_t> ne;
        const int n = (int)(rng() % 5);
        for (int i = 0; i < n; ++i) ne.push_back((int64_t)(rng() % 4));
        const bool ref = ref_dims(d, ne);
        const int64_t g = fg201l_dims_ok((uint64_t)(uintptr_t)d, (int64_t)ne.size(),
                                          ne.size() > 0 ? ne[0] : 0, ne.size() > 1 ? ne[1] : 0,
                                          ne.size() > 2 ? ne[2] : 0, ne.size() > 3 ? ne[3] : 0);
        ck((g != 0) == ref, "dims_ok", g, ref);
    }

    // shape_ok: both branches.
    for (int t = 0; t < 400; ++t) {
        int64_t d[4];
        for (int i = 0; i < 4; ++i) d[i] = 1 + (int64_t)(rng() % 6);
        const bool allow = (rng() % 2) != 0;
        const int64_t total = ref_nelements(d);
        const int64_t total_alt = total + ((rng() % 3) - 1);   // sometimes off by one
        const int64_t use = allow ? total_alt : total;
        const bool ref = allow ? (ref_nelements(d) == use) : true; // dims match by construction
        const int64_t g = fg201l_shape_ok(allow ? 1 : 0, (uint64_t)(uintptr_t)d, 4, use,
                                          d[0], d[1], d[2], d[3]);
        ck((g != 0) == ref, "shape_ok", g, ref);
    }

    // The integration entry point mirrors the same decision.
    {
        int64_t d[4] = {4, 3, 2, 1};
        int64_t req[4] = {4, 3, 2, 1};
        ck(fg201l_shape_check(0, d, 4, 24, req) == 1, "entry match", 0, 1);
        int64_t req2[4] = {4, 3, 2, 2};
        ck(fg201l_shape_check(0, d, 4, 24, req2) == 0, "entry mismatch", 0, 0);
        ck(fg201l_shape_check(1, d, 4, 24, req) == 1, "entry reshape", 0, 1);
        ck(fg201l_shape_check(1, d, 4, 25, req) == 0, "entry reshape count", 0, 0);
    }

    // get_key_or_arr and load-mode decisions.
    for (int i = 0; i < 300; ++i) {
        const int64_t n = (int64_t)(rng() % 600), nmax = (int64_t)(rng() % 600);
        ck((fg201l_or_arr_too_many(n, nmax) != 0) == (n > nmax), "or_arr_too_many", 0, 0);

        const int64_t kt = (int64_t)(rng() % 8), at = (int64_t)(rng() % 8);
        ck((fg201l_is_array_kv(kt, at) != 0) == (kt == at), "is_array_kv", 0, 0);

        const int64_t l1 = (int64_t)(rng() % 600), l2 = (int64_t)(rng() % 600);
        ck((fg201l_arr_length_ok(l1, l2) != 0) == (l1 == l2), "arr_length_ok", 0, 0);

        // Every load mode enum value from -1 to 4.
        const int64_t lm = (int64_t)(rng() % 7) - 1;
        const bool ref_mmap = (lm == -1 || lm == 1 || lm == 3);
        const bool ref_dio  = (lm == 4);
        ck((fg201l_use_mmap_mode(lm) != 0) == ref_mmap, "use_mmap", lm, ref_mmap);
        ck((fg201l_use_direct_io_mode(lm) != 0) == ref_dio, "use_direct_io", lm, ref_dio);
    }

    // merge_ranges / sub_ranges against std::algorithm transcriptions.
    for (int t = 0; t < 200; ++t) {
        const int n = 1 + (int)(rng() % 8);
        std::vector<std::pair<int64_t,int64_t>> rs;
        int64_t cur = 0;
        for (int i = 0; i < n; ++i) {
            const int64_t len = (int64_t)(rng() % 20) + 1;
            const int64_t gap = (int64_t)(rng() % 10);
            rs.push_back({cur + gap, cur + gap + len});
            cur = cur + gap + len;
        }
        std::vector<int64_t> src; for (auto & p : rs) { src.push_back(p.first); src.push_back(p.second); }

        // native merge
        std::vector<std::pair<int64_t,int64_t>> merged;
        for (auto & r : rs) {
            if (!merged.empty() && r.first <= merged.back().second)
                merged.back().second = std::max(merged.back().second, r.second);
            else merged.push_back(r);
        }
        std::vector<int64_t> dst(64, 0);
        const int64_t k = fg201l_merge_ranges((uint64_t)(uintptr_t)src.data(), n,
                                              (uint64_t)(uintptr_t)dst.data(), 32);
        ck(k == (int64_t)merged.size(), "merge count", k, (int64_t)merged.size());
        for (size_t i = 0; i < merged.size() && (int64_t)i < k; ++i) {
            ck(dst[i*2] == merged[i].first && dst[i*2+1] == merged[i].second, "merge pair", 0, 0);
        }

        // sub_ranges with a random sub-cover
        std::vector<std::pair<int64_t,int64_t>> subs;
        if (n > 0 && (rng() % 2)) {
            const int ns2 = 1 + (int)(rng() % 3);
            for (int i = 0; i < ns2; ++i) {
                const int64_t a = (int64_t)(rng() % (unsigned)(cur + 1));
                const int64_t b = a + (int64_t)(rng() % 20);
                subs.push_back({a, b});
            }
            std::sort(subs.begin(), subs.end());
        }
        std::vector<int64_t> subv; for (auto & p : subs) { subv.push_back(p.first); subv.push_back(p.second); }

        std::vector<std::pair<int64_t,int64_t>> subres;
        for (auto & x : rs) {
            int64_t c = x.first;
            for (auto & y : subs) {
                if (y.second <= c || y.first >= x.second) continue;
                if (y.first > c) subres.push_back({c, y.first});
                c = std::max(c, y.second);
            }
            if (c < x.second) subres.push_back({c, x.second});
        }
        std::vector<int64_t> d2(64, 0);
        const int64_t k2 = fg201l_sub_ranges((uint64_t)(uintptr_t)src.data(), n,
                                             (uint64_t)(uintptr_t)subv.empty() ? 0 : (uint64_t)(uintptr_t)subv.data(),
                                             (int64_t)subs.size(), (uint64_t)(uintptr_t)d2.data(), 32);
        ck(k2 == (int64_t)subres.size(), "sub count", k2, (int64_t)subres.size());
        for (size_t i = 0; i < subres.size() && (int64_t)i < k2; ++i) {
            ck(d2[i*2] == subres[i].first && d2[i*2+1] == subres[i].second, "sub pair", 0, 0);
        }
    }

    // sort_pairs: stable ordering by first over the same pair layout merge_ranges uses.
    for (int t = 0; t < 200; ++t) {
        const int n = 1 + (int)(rng() % 8);
        std::vector<int64_t> v, ref;
        for (int i = 0; i < n; ++i) {
            const int64_t f = (int64_t)(rng() % 10), l = f + (int64_t)(rng() % 10);
            v.push_back(f); v.push_back(l);
        }
        ref = v;
        std::stable_sort(ref.begin(), ref.end(), [](int64_t a, int64_t b) { return a < b; });
        // rebuild as pairs to sort by first only
        std::vector<std::pair<int64_t,int64_t>> pr;
        for (int i = 0; i < n; ++i) pr.push_back({v[i*2], v[i*2+1]});
        std::stable_sort(pr.begin(), pr.end(), [](const auto & a, const auto & b) { return a.first < b.first; });

        std::vector<int64_t> w = v;
        const int64_t rc = fg201l_sort_pairs((uint64_t)(uintptr_t)w.data(), n);
        ck(rc == n, "sort count", rc, n);
        for (int i = 0; i < n; ++i) {
            ck(w[i*2] == pr[i].first && w[i*2+1] == pr[i].second, "sort pair", w[i*2], pr[i].first);
        }
        // the sort must not lose or duplicate elements
        std::vector<int64_t> a1, a2;
        for (int i = 0; i < n; ++i) { a1.push_back(v[i*2]); a2.push_back(w[i*2]); }
        std::sort(a1.begin(), a1.end()); std::sort(a2.begin(), a2.end());
        ck(a1 == a2, "sort preserves multiset", 0, 0);
    }

    run_trap("nelements n out of range", 10, trap_nreq_oor);

    printf("FG201L_FIXTURE checks=%d failures=%d\n", checks, failures);
    if (failures) { printf("EXP201_LOCAL_GATES_FAIL\n"); return 1; }
    printf("EXP201_LOCAL_GATES_PASS\n");
    return 0;
}
