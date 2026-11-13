// Differential fixture for handler binding and path-parameter extraction
// (EXP463). Right arm: a transcription of httplib's PathParamsMatcher
// (constructor and match). Left arm: the exported Goose module.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>

#include "bind-gen.h"

extern "C" {
void fg463_st_i64(uint64_t, uint64_t, int64_t);
}

void fg463_st_i64(uint64_t p, uint64_t o, int64_t v) {
    *(int64_t *) ((char *) (uintptr_t) p + o) = v;
}

static int cases = 0, checks = 0, fails = 0;

static void fail(const char * what, const std::string & detail) {
    ++fails;
    if (fails <= 10) printf("FAIL %s: %s\n", what, detail.c_str());
}

// ---- native transcription of PathParamsMatcher ----------------------------
struct NativeMatcher {
    std::string pattern;
    std::vector<std::string> fragments;
    std::vector<std::string> names;
    bool malformed = false;

    explicit NativeMatcher(const std::string & p) : pattern(p) {
        const std::string marker = "/:";
        size_t last_end = 0;
        while (true) {
            const size_t mpos = pattern.find(marker, last_end == 0 ? last_end : last_end - 1);
            if (mpos == std::string::npos) break;
            fragments.push_back(pattern.substr(last_end, mpos - last_end + 1));
            const size_t name_start = mpos + marker.size();
            size_t sep = pattern.find('/', name_start);
            if (sep == std::string::npos) sep = pattern.size();
            const std::string name = pattern.substr(name_start, sep - name_start);
            for (const auto & n : names) if (n == name) malformed = true;
            names.push_back(name);
            last_end = sep + 1;
        }
        if (last_end < pattern.size()) fragments.push_back(pattern.substr(last_end));
    }

    bool match(const std::string & path, std::map<std::string, std::string> & params) const {
        params.clear();
        if (names.empty()) return path == pattern;
        size_t start = 0;
        for (size_t i = 0; i < fragments.size(); ++i) {
            const std::string & f = fragments[i];
            if (start + f.size() > path.size()) return false;
            if (path.compare(start, f.size(), f) != 0) return false;
            start += f.size();
            if (i >= names.size()) continue;
            size_t sep = path.find('/', start);
            if (sep == std::string::npos) sep = path.size();
            params[names[i]] = path.substr(start, sep - start);
            start = sep + 1;
        }
        return start >= path.size();
    }
};

static sl_u8 S(const std::string & s) {
    sl_u8 r; r.data = (uint8_t *) (uintptr_t) s.data(); r.len = (int64_t) s.size(); return r;
}

int main(void) {
    goose_init(0, 0);

    struct Case { const char * pattern; };
    const Case patterns[] = {
        {"/health"},
        {"/slots/:id_slot"},
        {"/path/fragments/:capture/more/fragments/:second_capture"},
        {"/a/:x/b/:y/c"},
        {"/:only"},
        {"/leading/:p"},
        {"/trailing/:p/"},
        {"/dup/:a/:a"},
        {"/users/:id/subscriptions"},
        {"/v1/chat/completions"},
    };
    const char * paths[] = {
        "/health", "/health/", "", "/", "/slots/", "/slots/abc", "/slots/abc/def",
        "/slots/a.b", "/slots/%2F", "/path/fragments/1/more/fragments/2",
        "/path/fragments/1/more/fragments/", "/a/1/b/2/c", "/a/1/b/2/c/",
        "/a//b//c", "/x", "/leading/p", "/trailing/p/", "/trailing/p",
        "/users/7/subscriptions", "/users/7/subscriptions/x", "/v1/chat/completions",
    };

    for (const auto & c : patterns) {
        NativeMatcher nm(c.pattern);
        // fragments and names for the Goose arm
        std::string fb;
        std::vector<int64_t> foffs, flens;
        for (const auto & f : nm.fragments) {
            foffs.push_back((int64_t) fb.size());
            flens.push_back((int64_t) f.size());
            fb += f;
        }

        for (const char * p : paths) {
            std::map<std::string, std::string> params;
            const bool want = nm.match(p, params);

            int64_t res[16];
            for (int i = 0; i < 16; ++i) res[i] = -1;
            sl_i64 fo{foffs.data(), (int64_t) foffs.size()};
            sl_i64 fl{flens.data(), (int64_t) flens.size()};
            sl_i64 ro{res, 16};
            const int64_t got = path_match(S(fb), fo, fl, (int64_t) nm.names.size(),
                                           S(std::string(p)), ro);
            ++cases; ++checks;
            if ((got != 0) != want) {
                fail("match", std::string(c.pattern) + " vs " + p);
            }
            ++checks;
            if ((res[0] != 0) != want) fail("match-out", std::string(c.pattern) + " vs " + p);
            if (!want) continue;
            ++checks;
            if (res[1] != (int64_t) params.size()) {
                fail("capture-count", std::string(c.pattern) + " vs " + p);
                continue;
            }
            size_t idx = 0;
            for (const auto & kv : params) {
                ++checks;
                const int64_t off = res[2 + 2 * (int64_t) idx];
                const int64_t len = res[3 + 2 * (int64_t) idx];
                const std::string got_v = std::string(p).substr((size_t) off, (size_t) len);
                if (got_v != kv.second) {
                    fail("capture", std::string(c.pattern) + " vs " + p + " key=" + kv.first);
                }
                ++idx;
            }
        }
    }

    // duplicates are a construction error
    const char * dup_names[] = {"a", "b", "a"};
    std::string nb;
    std::vector<int64_t> noffs, nlens;
    for (const char * n : dup_names) {
        noffs.push_back((int64_t) nb.size());
        nlens.push_back((int64_t) strlen(n));
        nb += n;
    }
    ++cases; ++checks;
    sl_i64 dno{noffs.data(), (int64_t) noffs.size()};
    sl_i64 dnl{nlens.data(), (int64_t) nlens.size()};
    if (duplicate(S(nb), dno, dnl) != 1)
        fail("duplicate", "repeated name not detected");
    noffs.pop_back(); nlens.pop_back();
    ++cases; ++checks;
    sl_i64 dno2{noffs.data(), (int64_t) noffs.size()};
    sl_i64 dnl2{nlens.data(), (int64_t) nlens.size()};
    if (duplicate(S(nb), dno2, dnl2) != 0)
        fail("duplicate", "distinct names flagged");

    // first registration wins
    for (int existing = 0; existing < 2; ++existing) {
        ++cases; ++checks;
        if (bind_keep(existing) != existing) fail("bind_keep", std::to_string(existing));
    }

    // registration path = prefix + route
    const char * prefixes[] = {"", "/api", "/api/v1"};
    const char * routes[] = {"/health", "/slots/:id_slot"};
    for (const char * pre : prefixes)
    for (const char * r : routes) {
        const std::string pstr(pre), rstr(r);
        uint8_t buf[128];
        sl_u8 pb; pb.data = (uint8_t *) (uintptr_t) pstr.data(); pb.len = (int64_t) pstr.size();
        sl_u8 rb; rb.data = (uint8_t *) (uintptr_t) rstr.data(); rb.len = (int64_t) rstr.size();
        sl_u8 db; db.data = buf; db.len = (int64_t) sizeof buf;
        const int64_t n = full_path(pb, rb, db);
        const std::string want = std::string(pre) + r;
        ++cases; ++checks;
        if (n != (int64_t) want.size() || std::string((const char *) buf, (size_t) n) != want)
            fail("full_path", want);
    }

    printf("FG463_BIND cases=%d values=%d failures=%d\n", cases, checks, fails);
    return fails ? 1 : 0;
}
