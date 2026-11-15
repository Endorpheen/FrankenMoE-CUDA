// Differential gate for P8-09 S4 (the CORS decision and origin_is_localhost).
//
// Native arm: a transcription of the pre-routing CORS block of
// server_http_context::init in tools/server/server-http.cpp (278-299) and of
// origin_is_localhost (50-58). The block lives inside a lambda registered with
// httplib, so the transcription is written statement by statement against the
// cited lines, in the native order, and it calls the real
// common_http_parse_url from common/http.h for the parse.
//
// Port arm: the cors_ exports of source/cors-policy.goose.
//
// The port never parses a URL: the driver parses the origin once, exactly as
// origin_is_localhost does, and hands the module the host slice plus a parsed
// flag (0 is the native catch). That is the documented interface; the gate
// checks both the standalone host test and the composed branch decision.
#include "cors.h"

#include "http.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

static int cases = 0;
static int values = 0;
static int fails = 0;

static void eq_int(const char * what, const std::string & name, int64_t want, int64_t got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-16s %-58s want=%lld got=%lld\n", what, name.c_str(),
               (long long) want, (long long) got);
    }
}

static void eq_str(const char * what, const std::string & name, const std::string & want,
                   const std::string & got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-16s %-58s want=[%s] got=[%s]\n", what, name.c_str(), want.c_str(),
               got.c_str());
    }
}

static sl_u8 SL(const std::string & s) {
    return sl_u8{(uint8_t *) (void *) s.data(), (int64_t) s.size()};
}

// ---- the native arm -------------------------------------------------------

// origin_is_localhost, server-http.cpp 50-58.
static bool origin_is_localhost_native(const std::string & origin) {
    try {
        const std::string host = common_http_parse_url(origin).host;
        return host == "localhost" || host == "127.0.0.1" || host == "::1";
    } catch (const std::exception &) {
        return false;
    }
}

struct native_cors {
    bool   set = false;    // Access-Control-Allow-Origin set
    std::string value;     // its value
    bool   warn = false;   // "(CORS) skip non-localhost origin"
};

// The pre-routing block, server-http.cpp 278-291.
static native_cors cors_native(const std::string & origins, bool credentials,
                               const std::string & origin) {
    native_cors r;

    if (credentials && origins == "*") {
        r.set   = true;
        r.value = origin;
    } else if (origins == "localhost") {
        if (!origin.empty() && origin_is_localhost_native(origin)) {
            r.set   = true;
            r.value = origin;
        } else if (!origin.empty()) {
            r.warn = true;
        }
    } else {
        r.set   = true;
        r.value = origins;
    }
    return r;
}

// ---- the port arm ---------------------------------------------------------

struct parsed_origin {
    bool        ok = false;
    std::string host;
};

static parsed_origin parse_origin(const std::string & origin) {
    parsed_origin r;

    try {
        r.host = common_http_parse_url(origin).host;
        r.ok   = true;
    } catch (const std::exception &) {
        r.ok = false;
    }
    return r;
}

struct port_cors {
    int64_t     rc = -1;
    int64_t     set = 0;
    int64_t     warn = 0;
    std::string value;
};

static port_cors cors_port(const std::string & origins, int64_t credentials,
                           const std::string & origin) {
    port_cors r;
    const parsed_origin po = parse_origin(origin);
    const int64_t local = cors_is_localhost(SL(po.host), po.ok ? 1 : 0);
    uint8_t buf[512];
    int64_t res[2] = {-1, -1};
    sl_u8 out{buf, (int64_t) sizeof buf};

    r.rc = cors_allow_origin(SL(origins), credentials, SL(origin), local, out,
                             sl_i64{res, 2});
    r.set  = res[0];
    r.warn = res[1];
    if (r.rc >= 0) {
        r.value.assign((const char *) buf, (size_t) r.rc);
    }
    return r;
}

// A short label so a 192-case cartesian product still logs readably.
static std::string label(const std::string & origins, int64_t creds, const std::string & origin) {
    std::string s = "origins=[" + origins + "] creds=" + (creds ? "1" : "0") + " origin=[";
    s += origin;
    s += "]";
    return s;
}

int main(int argc, char ** argv) {
    goose_init(argc, argv);

    // ---- 1. origin_is_localhost on raw origin strings ---------------------
    const std::vector<std::string> origins_hdr = {
        "http://localhost",
        "http://localhost:8080",
        "https://localhost:443/path?x=1",
        "http://127.0.0.1",
        "https://127.0.0.1:3000",
        "http://[::1]",
        "http://[::1]:1234",
        "https://[0:0:0:0:0:0:0:1]:9000",
        "http://localhost.evil.com",
        "http://notlocalhost:1",
        "http://LOCALHOST:1",
        "https://localhost.",
        "http://127.0.0.2",
        "http://127.0.0.1.evil.com",
        "http://0.0.0.0:8080",
        "http://user:pw@localhost:1234/x",
        "http://user@127.0.0.1:1",
        "localhost:8080",
        "127.0.0.1",
        "ftp://localhost",
        "file://localhost/etc",
        "http://localhost:99999",
        "http://",
        "http:///path",
        "",
        "null",
        "::1",
        "[::1]",
    };

    for (const auto & o : origins_hdr) {
        cases++;
        const parsed_origin po = parse_origin(o);
        eq_int("localhost", "origin=[" + o + "]",
               origin_is_localhost_native(o) ? 1 : 0,
               cors_is_localhost(SL(po.host), po.ok ? 1 : 0));
    }

    // a parse failure is the native catch, whatever the host slice says
    for (const char * h : {"localhost", "127.0.0.1", "::1", "localhost.evil.com", ""}) {
        cases++;
        eq_int("localhost", std::string("parsed=0 host=[") + h + "]", 0, cors_is_localhost(SL(h), 0));
    }
    cases++;
    eq_int("localhost", "parsed=1 with the empty host", 0, cors_is_localhost(SL(""), 1));

    // ---- 2. the three ACAO branches, full cartesian product ---------------
    const std::vector<std::string> origins_cfg = {
        "*", "localhost", "http://example.com", "", "https://app.example:8443", "LOCALHOST",
    };
    const std::vector<std::string> req_origin = {
        "",
        "http://localhost",
        "http://localhost:8080",
        "https://127.0.0.1:3000",
        "http://[::1]:1234",
        "http://localhost.evil.com",
        "http://LOCALHOST:1",
        "localhost:8080",
        "ftp://localhost",
        "http://127.0.0.2",
        "http://user:pw@localhost:1234/x",
        "https://localhost.",
    };

    for (const auto & cfg : origins_cfg) {
        for (int64_t creds = 0; creds <= 1; creds++) {
            for (const auto & o : req_origin) {
                cases++;
                const native_cors want = cors_native(cfg, creds != 0, o);
                const port_cors got   = cors_port(cfg, creds, o);
                const std::string name = label(cfg, creds, o);

                eq_int("acao set", name, want.set ? 1 : 0, got.set);
                eq_int("acao warn", name, want.warn ? 1 : 0, got.warn);
                eq_int("acao rc", name, want.set ? 1 : -1, got.rc >= 0 ? 1 : -1);
                if (want.set && got.set) {
                    eq_str("acao value", name, want.value, got.value);
                }
            }
        }
    }

    // a too-small buffer is the only failure the native path cannot hit; the
    // contract is "return -1, leave res[0] clear"
    cases++;
    {
        uint8_t tiny[2];
        int64_t res[2] = {-1, -1};
        const int64_t rc = cors_allow_origin(SL("http://example.com"), 0,
                                             SL("http://evil.example"), 0,
                                             sl_u8{tiny, 2}, sl_i64{res, 2});
        eq_int("capacity", "static origin does not fit", -1, rc);
        eq_int("capacity", "no header claimed on overflow", 0, res[0]);
    }

    // ---- 3. the preflight branch ------------------------------------------
    const std::vector<std::string> methods = {
        "OPTIONS", "GET", "POST", "DELETE", "options", "Options", "OPTIONS ", " OPTIONS", "",
    };

    for (const auto & m : methods) {
        cases++;
        const bool want = m == "OPTIONS";
        eq_int("preflight", "method=[" + m + "]", want ? 1 : 0, cors_is_preflight(SL(m)));

        uint8_t buf[32];
        sl_u8 out{buf, (int64_t) sizeof buf};
        const int64_t n = cors_credentials_text(want ? 1 : 0, out);
        const std::string cred((const char *) buf, (size_t) (n < 0 ? 0 : n));
        eq_str("preflight", "credentials method=[" + m + "]",
               want ? "true" : "false", n < 0 ? "<error>" : cred);
    }

    // the native ternary takes any non-zero value as true
    for (int64_t flag : {-1, 0, 1, 2, 42}) {
        cases++;
        uint8_t buf[16];
        sl_u8 out{buf, (int64_t) sizeof buf};
        const int64_t n = cors_credentials_text(flag, out);
        eq_str("preflight", "credentials flag=" + std::to_string(flag),
               flag != 0 ? "true" : "false",
               n < 0 ? "<error>" : std::string((const char *) buf, (size_t) n));
    }

    cases++;
    {
        uint8_t buf[32];
        sl_u8 out{buf, (int64_t) sizeof buf};
        const int64_t n = cors_preflight_type(out);
        eq_str("preflight", "placeholder content type", "text/html",
               n < 0 ? "<error>" : std::string((const char *) buf, (size_t) n));
    }

    printf("FGCORS cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
