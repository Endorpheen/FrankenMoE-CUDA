// Differential gate for P8-09 S6 (the API-key middleware).
//
// Native arm:
//   - frontend_paths / get_public_endpoints are transcribed from
//     tools/server/server-http.cpp 187-204, over std::unordered_set, with the
//     same path list the driver hands the port;
//   - middleware_validate_api_key is transcribed from 206-251, including the
//     Authorization-then-X-Api-Key fallback and the exact "Bearer " strip;
//   - the 401 body comes from the real common_json type, i.e. the backing
//     nlohmann::ordered_json with the same dump arguments server-common.cpp
//     passes in dump_safe (ordered keys, no indent, error handler replace), so
//     the literal the port exports is checked against the library, not a
//     reading of it.
//
// Port arm: the key_ exports of source/api-key-policy.goose.
#include "key.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <cstdio>
#include <string>
#include <unordered_set>
#include <vector>

static int cases = 0;
static int values = 0;
static int fails = 0;

static std::string show(const std::string & s) {
    std::string out;

    for (unsigned char c : s) {
        if (c >= 32 && c < 127 && c != '\\') {
            out += (char) c;
        } else {
            char buf[8];
            snprintf(buf, sizeof buf, "\\x%02X", c);
            out += buf;
        }
    }
    return out;
}

static void eq_int(const char * what, const std::string & name, int64_t want, int64_t got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-14s %-56s want=%lld got=%lld\n", what, name.c_str(),
               (long long) want, (long long) got);
    }
}

static void eq_str(const char * what, const std::string & name, const std::string & want,
                   const std::string & got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-14s %-56s want=[%s] got=[%s]\n", what, name.c_str(), show(want).c_str(),
               show(got).c_str());
    }
}

static sl_u8 SL(const std::string & s) {
    return sl_u8{(uint8_t *) (void *) s.data(), (int64_t) s.size()};
}

// The embedded assets of the accepted bundle, as the UI generator emits them.
static const std::vector<std::string> asset_names = {
    "index.html",
    "manifest.webmanifest",
    "sw.js",
    "build.json",
    "version.json",
    "_app/immutable/bundle.a1b2c3.js",
    "_app/immutable/bundle.a1b2c3.css",
    "workbox.9f8e7d.js",
    "assets/site.css",
    "docs/page.html",
};

// frontend_paths, server-http.cpp 187-194.
static std::vector<std::string> frontend_paths() {
    std::vector<std::string> paths{"/"};

    for (const auto & name : asset_names) {
        paths.push_back("/" + name);
    }
    return paths;
}

// get_public_endpoints, server-http.cpp 196-204.
static std::unordered_set<std::string> public_endpoints() {
    std::unordered_set<std::string> endpoints{"/health", "/v1/health"};

    for (const auto & p : frontend_paths()) {
        endpoints.insert(p);
    }
    return endpoints;
}

// middleware_validate_api_key, server-http.cpp 206-244, decision only.
static bool validate_api_key_native(const std::vector<std::string> & api_keys,
                                    const std::string & path,
                                    const std::string & auth,
                                    const std::string & apikey) {
    static const auto endpoints = public_endpoints();

    if (api_keys.empty()) {
        return true;
    }
    if (endpoints.count(path)) {
        return true;
    }
    std::string req_api_key = auth;
    if (req_api_key.empty()) {
        req_api_key = apikey;
    }
    static const std::string prefix = "Bearer ";
    if (req_api_key.substr(0, prefix.size()) == prefix) {
        req_api_key = req_api_key.substr(prefix.size());
    }
    for (const auto & k : api_keys) {
        if (k == req_api_key) {
            return true;
        }
    }
    return false;
}

// safe_json_to_str of the 401 body: common_json is nlohmann::ordered_json with
// dump(-1, ' ', false, replace).
static std::string unauthorized_body_native() {
    nlohmann::ordered_json j = {
        {"error", {
            {"message", "Invalid API Key"},
            {"type", "authentication_error"},
            {"code", 401},
        }},
    };
    return j.dump(-1, ' ', false, nlohmann::ordered_json::error_handler_t::replace);
}

// ---- the port arm ---------------------------------------------------------

struct key_table {
    std::string          blob;
    std::vector<int64_t> off{0};
};

static key_table pack(const std::vector<std::string> & keys) {
    key_table t;

    for (const auto & k : keys) {
        t.blob += k;
        t.off.push_back((int64_t) t.blob.size());
    }
    return t;
}

static int64_t authorize_port(const std::vector<std::string> & keys, const std::string & path,
                              const std::string & auth, const std::string & apikey) {
    const key_table kt = pack(keys);
    const key_table pt = pack(frontend_paths());

    return key_authorize(SL(kt.blob), sl_i64{(int64_t *) kt.off.data(), (int64_t) kt.off.size()},
                         SL(path), SL(pt.blob),
                         sl_i64{(int64_t *) pt.off.data(), (int64_t) pt.off.size()}, SL(auth),
                         SL(apikey));
}

int main(int argc, char ** argv) {
    goose_init(argc, argv);

    // ---- 1. public endpoint membership ------------------------------------
    const std::vector<std::pair<std::string, bool>> path_cases = {
        {"/health", true},
        {"/v1/health", true},
        {"/", true},
        {"/index.html", true},
        {"/sw.js", true},
        {"/_app/immutable/bundle.a1b2c3.js", true},
        {"/docs/page.html", true},
        {"/v1/chat/completions", false},
        {"/v1/models", false},
        {"/props", false},
        {"/metrics", false},
        {"/health/", false},
        {"/Health", false},
        {"/v1/Health", false},
        {"/index.htm", false},
        {"/index.html/", false},
        {"/_app/immutable/bundle.a1b2c3.JS", false},
        {"", false},
        {"/", true},
    };

    {
        const key_table pt = pack(frontend_paths());

        for (const auto & [path, want] : path_cases) {
            cases++;
            eq_int("public", "path=[" + path + "]", want ? 1 : 0,
                   key_is_public(SL(path), SL(pt.blob),
                                 sl_i64{(int64_t *) pt.off.data(), (int64_t) pt.off.size()}));
        }
    }

    // ---- 2. credential selection and the Bearer prefix --------------------
    const std::vector<std::pair<std::string, std::string>> header_cases = {
        {"", ""},
        {"Bearer k1", ""},
        {"k1", ""},
        {"", "k1"},
        {"", "Bearer k1"},
        {"Bearer k1", "k2"},
        {"k2", "Bearer k1"},
        {"bearer k1", ""},
        {"BEARER k1", ""},
        {"Bearer  k1", ""},
        {"Bearer k1 ", ""},
        {"Bearer ", ""},
        {"Bearer", ""},
        {" Bearer k1", ""},
        {"Bearer Bearer k1", ""},
        {"Bearer\tk1", ""},
        {"Bearer k1", "k1"},
        {"  ", "k1"},
    };

    for (const auto & [auth, apikey] : header_cases) {
        cases++;
        const std::vector<std::string> keys = {"k1", "k2"};
        const key_table kt = pack(keys);
        const std::string want_cand = auth.empty() ? apikey : auth;
        const std::string want_stripped =
            want_cand.substr(0, 7) == "Bearer " ? want_cand.substr(7) : want_cand;
        uint8_t cbuf[256];
        uint8_t sbuf[256];

        const int64_t cn = key_credential(SL(auth), SL(apikey), sl_u8{cbuf, (int64_t) sizeof cbuf});
        eq_str("select", "auth=[" + show(auth) + "] apikey=[" + show(apikey) + "]", want_cand,
               cn < 0 ? "<error>" : std::string((const char *) cbuf, (size_t) cn));

        const int64_t sn = key_strip_bearer(SL(want_cand), sl_u8{sbuf, (int64_t) sizeof sbuf});
        eq_str("strip", "auth=[" + show(auth) + "] apikey=[" + show(apikey) + "]", want_stripped,
               sn < 0 ? "<error>" : std::string((const char *) sbuf, (size_t) sn));

        // the combined matcher must agree with the native verdict
        const bool want_ok = validate_api_key_native(keys, "/v1/chat/completions", auth, apikey);
        const int64_t got = key_match_credential(
            SL(kt.blob), sl_i64{(int64_t *) kt.off.data(), (int64_t) kt.off.size()}, SL(auth),
            SL(apikey));
        eq_int("match", "auth=[" + show(auth) + "] apikey=[" + show(apikey) + "]", want_ok ? 1 : 0,
               got);
    }

    // ---- 3. the whole middleware over its inputs --------------------------
    const std::vector<std::vector<std::string>> key_sets = {
        {},
        {"k1"},
        {"k1", "k2"},
        {"", "k1"},
        {"Bearer k1"},
        {"k1 ", " k1"},
        {"a b", "c+d"},
        {"k1", "k1"},
        {"\xC3\xA9"},
    };
    const std::vector<std::pair<std::string, std::string>> creds = {
        {"", ""},
        {"Bearer k1", ""},
        {"Bearer k2", ""},
        {"k1", ""},
        {"Bearer wrong", ""},
        {"", "k1"},
        {"", "Bearer k2"},
        {"Bearer wrong", "k1"},
        {"Bearer ", ""},
        {"Bearer", ""},
        {"Bearer Bearer k1", ""},
        {" bearer k1", ""},
    };
    const std::vector<std::string> paths = {
        "/health",
        "/v1/health",
        "/",
        "/index.html",
        "/sw.js",
        "/v1/chat/completions",
        "/health/",
        "/Health",
        "/metrics",
        "",
    };

    for (const auto & keys : key_sets) {
        for (const auto & path : paths) {
            for (const auto & [auth, apikey] : creds) {
                cases++;
                const bool want = validate_api_key_native(keys, path, auth, apikey);
                const int64_t got = authorize_port(keys, path, auth, apikey);
                eq_int("authorize",
                       "keys=" + std::to_string(keys.size()) + " path=[" + path + "] auth=[" +
                           show(auth) + "] apikey=[" + show(apikey) + "]",
                       want ? 1 : 0, got);
            }
        }
    }

    // ---- 4. the 401 body and content type --------------------------------
    {
        cases++;
        uint8_t buf[512];
        const int64_t n = key_error_body(sl_u8{buf, (int64_t) sizeof buf});
        eq_str("body", "401 body", unauthorized_body_native(),
               n < 0 ? "<error>" : std::string((const char *) buf, (size_t) n));
    }
    {
        cases++;
        uint8_t buf[128];
        const int64_t n = key_error_type(sl_u8{buf, (int64_t) sizeof buf});
        eq_str("body", "401 content type", "application/json; charset=utf-8",
               n < 0 ? "<error>" : std::string((const char *) buf, (size_t) n));
    }

    // ---- 5. capacity contract --------------------------------------------
    {
        cases++;
        uint8_t buf[8];
        eq_int("capacity", "401 body does not fit", -1,
               key_error_body(sl_u8{buf, (int64_t) sizeof buf}));
    }
    {
        cases++;
        uint8_t buf[4];
        eq_int("capacity", "credential does not fit", -1,
               key_credential(SL("Bearer abcdef"), SL(""), sl_u8{buf, (int64_t) sizeof buf}));
    }

    printf("FKEY cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
