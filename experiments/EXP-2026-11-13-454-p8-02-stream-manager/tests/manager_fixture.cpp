// Differential fixture for the Goose stream session manager policy (EXP454).
//
// Both arms simulate the same operation sequence over a conversation map with
// per-session cancel/finalize flags. The right arm transcribes the native
// create_or_replace / evict / evict_and_cancel / stop_gc / gc_loop bodies; the
// left arm applies the Goose decisions. After every operation the map contents
// and every session's flags must match.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <map>
#include <vector>
#include <random>
#include <string>

extern "C" {
int64_t fg454_retire(int64_t op, int64_t found, uint64_t out);
int64_t fg454_stop(uint64_t out);
int64_t fg454_cutoff(int64_t now, int64_t ttl, uint64_t out);
int64_t fg454_gc(int64_t completed, int64_t now, int64_t ttl, uint64_t out);
void    fg454_st_i64(uint64_t p, uint64_t o, int64_t v);
}

void fg454_st_i64(uint64_t p, uint64_t o, int64_t v) {
    *(int64_t *) ((char *) (uintptr_t) p + o) = v;
}

struct Sess {
    int id;
    bool cancelled = false;
    bool finalized = false;
    int64_t completed = 0;   // 0 while alive
};

struct Universe {
    std::vector<Sess *> pool;
    std::map<int, Sess *> map;

    Sess * create(int conv) {
        Sess * s = new Sess{conv};
        pool.push_back(s);
        return s;
    }
    ~Universe() { for (auto * s : pool) delete s; }
};

static int cases = 0, checks = 0, fails = 0;

static void fail(const char * what, const char * detail) {
    ++fails;
    if (fails <= 10) printf("FAIL %s: %s\n", what, detail);
}

// ---- native arm ------------------------------------------------------------
static void native_create(Universe & u, int conv) {
    Sess * fresh = u.create(conv);
    auto it = u.map.find(conv);
    if (it != u.map.end()) {
        Sess * prev = it->second;
        it->second = fresh;
        prev->cancelled = true;   // create_or_replace cancels
        prev->finalized = true;   // then finalizes
    } else {
        u.map.emplace(conv, fresh);
    }
}

static void native_evict(Universe & u, int conv) {
    auto it = u.map.find(conv);
    if (it == u.map.end()) return;
    Sess * s = it->second;
    u.map.erase(it);
    s->finalized = true;          // finalize only
}

static void native_evict_cancel(Universe & u, int conv) {
    auto it = u.map.find(conv);
    if (it == u.map.end()) return;
    Sess * s = it->second;
    u.map.erase(it);
    s->cancelled = true;          // cancel first
    s->finalized = true;          // then finalize
}

static void native_stop(Universe & u) {
    std::vector<Sess *> snapshot;
    for (auto & kv : u.map) snapshot.push_back(kv.second);
    u.map.clear();
    for (auto * s : snapshot) s->finalized = true;   // no cancel
}

static void native_gc(Universe & u, int64_t now, int64_t ttl) {
    int64_t cutoff = now - ttl;
    for (auto it = u.map.begin(); it != u.map.end();) {
        int64_t completed = it->second->completed;
        if (completed != 0 && completed <= cutoff) {
            Sess * s = it->second;
            it = u.map.erase(it);
            s->finalized = true;
        } else {
            ++it;
        }
    }
}

// ---- goose arm -------------------------------------------------------------
static void goose_create(Universe & u, int conv) {
    Sess * fresh = u.create(conv);
    auto it = u.map.find(conv);
    int found = it != u.map.end() ? 1 : 0;
    int64_t plan[3] = {-1, -1, -1};
    fg454_retire(1, found, (uint64_t) (uintptr_t) plan);
    if (found) {
        Sess * prev = it->second;
        if (plan[1]) prev->cancelled = true;
        if (plan[2]) prev->finalized = true;
        if (plan[0]) it->second = fresh;
        else fail("create", "plan did not replace");
    } else {
        u.map.emplace(conv, fresh);
    }
}

static void goose_evict(Universe & u, int conv, int op) {
    auto it = u.map.find(conv);
    if (it == u.map.end()) {
        int64_t plan[3] = {-1, -1, -1};
        fg454_retire(op, 0, (uint64_t) (uintptr_t) plan);
        if (plan[1] || plan[2] || plan[0]) fail("evict-absent", "plan acted on a missing session");
        return;
    }
    Sess * s = it->second;
    int64_t plan[3] = {-1, -1, -1};
    fg454_retire(op, 1, (uint64_t) (uintptr_t) plan);
    u.map.erase(it);
    if (plan[1]) s->cancelled = true;
    if (plan[2]) s->finalized = true;
}

static void goose_stop(Universe & u) {
    int64_t plan[2] = {-1, -1};
    fg454_stop((uint64_t) (uintptr_t) plan);
    std::vector<Sess *> snapshot;
    for (auto & kv : u.map) snapshot.push_back(kv.second);
    u.map.clear();
    for (auto * s : snapshot) {
        if (plan[0]) s->cancelled = true;
        if (plan[1]) s->finalized = true;
    }
}

static void goose_gc(Universe & u, int64_t now, int64_t ttl) {
    std::vector<int> drop;
    for (auto & kv : u.map) {
        int64_t d[1] = {-1};
        fg454_gc(kv.second->completed, now, ttl, (uint64_t) (uintptr_t) d);
        if (d[0]) drop.push_back(kv.first);
    }
    for (int conv : drop) {
        auto it = u.map.find(conv);
        Sess * s = it->second;
        u.map.erase(it);
        s->finalized = true;
    }
}

// ---- comparison ------------------------------------------------------------
static void compare(const Universe & n, const Universe & g, const char * where) {
    ++cases; ++checks;
    if (n.map.size() != g.map.size()) {
        char d[128];
        snprintf(d, sizeof d, "%s map size native=%zu goose=%zu", where, n.map.size(), g.map.size());
        fail("map", d);
        return;
    }
    auto ni = n.map.begin();
    auto gi = g.map.begin();
    for (; ni != n.map.end(); ++ni, ++gi) {
        ++checks;
        if (ni->first != gi->first || ni->second->id != gi->second->id) {
            fail("map-key", where);
        }
    }
    ++checks;
    if (n.pool.size() != g.pool.size()) { fail("pool-size", where); return; }
    for (size_t i = 0; i < n.pool.size(); ++i) {
        ++cases; ++checks; ++checks;
        if (n.pool[i]->cancelled != g.pool[i]->cancelled) {
            char d[128];
            snprintf(d, sizeof d, "%s conv=%d cancelled native=%d goose=%d", where, n.pool[i]->id,
                     n.pool[i]->cancelled, g.pool[i]->cancelled);
            fail("cancelled", d);
        }
        if (n.pool[i]->finalized != g.pool[i]->finalized) {
            char d[128];
            snprintf(d, sizeof d, "%s conv=%d finalized native=%d goose=%d", where, n.pool[i]->id,
                     n.pool[i]->finalized, g.pool[i]->finalized);
            fail("finalized", d);
        }
    }
}

int main(void) {
    std::mt19937 rng(454001);
    const int64_t ttl = 300;   // STREAM_SESSION_TTL_SECONDS

    // 1. fixed operation script covering every path, including absent keys
    {
        Universe n, g;
        const struct { int op, conv; } script[] = {
            {1, 1}, {1, 1}, {2, 2}, {2, 1}, {3, 3}, {3, 1}, {1, 3},
            {2, 3}, {3, 2}, {1, 0}, {1, 0}, {2, 0}, {3, 0},
        };
        for (size_t i = 0; i < sizeof(script) / sizeof(script[0]); ++i) {
            switch (script[i].op) {
                case 1: native_create(n, script[i].conv); goose_create(g, script[i].conv); break;
                case 2: native_evict(n, script[i].conv); goose_evict(g, script[i].conv, 2); break;
                case 3: native_evict_cancel(n, script[i].conv); goose_evict(g, script[i].conv, 3); break;
            }
            compare(n, g, "script");
        }
        native_stop(n); goose_stop(g);
        compare(n, g, "stop");
    }

    // 2. GC boundaries: alive sessions are never dropped, expired ones are
    {
        Universe n, g;
        for (int conv = 0; conv < 5; ++conv) { native_create(n, conv); goose_create(g, conv); }
        // conv 0 alive, 1 completed exactly at cutoff, 2 one second after, 3 long ago, 4 completed in the future
        const int64_t now = 1000;
        n.map[0]->completed = 0;
        n.map[1]->completed = now - ttl;
        n.map[2]->completed = now - ttl + 1;
        n.map[3]->completed = now - ttl - 50;
        n.map[4]->completed = now + 10;
        for (int conv = 0; conv < 5; ++conv) g.map[conv]->completed = n.map[conv]->completed;

        int64_t cut[1] = {-1};
        fg454_cutoff(now, ttl, (uint64_t) (uintptr_t) cut);
        ++cases; ++checks;
        if (cut[0] != now - ttl) fail("cutoff", "goose cutoff differs from now - ttl");

        native_gc(n, now, ttl);
        goose_gc(g, now, ttl);
        compare(n, g, "gc-boundary");

        const bool expect[] = {true, false, true, false, true};  // conv alive/present
        for (int conv = 0; conv < 5; ++conv) {
            ++cases; ++checks;
            bool present = g.map.count(conv) != 0;
            if (present != expect[conv])
                fail("gc-membership", conv == 1 ? "boundary session must be dropped" : "unexpected membership");
        }
    }

    // 3. randomised sequences with time advancing
    for (int round = 0; round < 40; ++round) {
        Universe n, g;
        int64_t now = 1000;
        for (int step = 0; step < 60; ++step) {
            int op = (int) (rng() % 100);
            int conv = (int) (rng() % 4);
            if (op < 45) { native_create(n, conv); goose_create(g, conv); }
            else if (op < 65) { native_evict(n, conv); goose_evict(g, conv, 2); }
            else if (op < 85) { native_evict_cancel(n, conv); goose_evict(g, conv, 3); }
            else if (op < 92) { native_stop(n); goose_stop(g); }
            else {
                now += (int64_t) (rng() % 400);
                native_gc(n, now, ttl);
                goose_gc(g, now, ttl);
            }
            // finish some sessions so GC has something to expire; both arms
            // allocate in the same order, so index i is the same session
            for (size_t i = 0; i < n.pool.size(); ++i) {
                if (n.pool[i]->completed == 0 && (rng() % 100) < 5) {
                    n.pool[i]->completed = now;
                    g.pool[i]->completed = now;
                }
            }
            compare(n, g, "random");
        }
    }

    printf("FG454_MANAGER cases=%d values=%d failures=%d\n", cases, checks, fails);
    return fails ? 1 : 0;
}
