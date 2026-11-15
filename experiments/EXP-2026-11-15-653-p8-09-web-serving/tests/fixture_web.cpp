// Differential gate for P8-09 S2 (the web UI serve decision).
//
// Native arm: a transcription of the embedded-asset branch of
// server_http_context::init in server-http.cpp (341-425). That branch is a
// chain of lambdas registered with httplib and cannot be called from outside,
// so the transcription below is written statement by statement against the
// cited lines, in the native order:
//
//   handle_gzip_header      341-355   the 415 gate, then Content-Encoding
//   cache_immutable         359       the immutable Cache-Control literal
//   serve_asset_cached      362-387   ETag, the 304 early return, isolation
//   serve_asset_nocache     389-401   Cache-Control: no-cache
//   index registration      403-404   "/" and "/index.html"
//   no_cache_names          409-414
//   remaining routes        416-425
//
// The asset table is the real generated one (ui.cpp), so names, ETags and the
// sorted order all come from the accepted generator.
// Port arm: the web_ exports of source/web-assets.goose.
#include "web.h"

#include "ui.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

static int cases = 0;
static int values = 0;
static int fails = 0;

// The response-header bits of web-assets.goose. Goose exports functions, not
// constants, so the protocol constants are restated here from the module doc.
constexpr unsigned WEB_H_ETAG      = 1;
constexpr unsigned WEB_H_ISOLATION = 2;
constexpr unsigned WEB_H_CACHE     = 4;
constexpr unsigned WEB_H_ENCODING  = 8;
constexpr unsigned WEB_H_TYPE      = 16;

static const char * const CACHE_IMMUTABLE  = "public, max-age=31536000, immutable";
static const char * const CACHE_REVALIDATE = "no-cache";

static void eq_int(const char * what, const std::string & name, int64_t want, int64_t got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-16s %-40s want=%lld got=%lld\n", what, name.c_str(),
               (long long) want, (long long) got);
    }
}

static void eq_str(const char * what, const std::string & name, const std::string & want,
                   const std::string & got) {
    values++;
    if (want != got) {
        fails++;
        printf("FAIL %-16s %-40s want=%s got=%s\n", what, name.c_str(), want.c_str(), got.c_str());
    }
}

static sl_u8 SL(const std::string & s) {
    return sl_u8{(uint8_t *) (void *) s.data(), (int64_t) s.size()};
}

// ---- the native arm -------------------------------------------------------

struct decision {
    int         status = 404;
    long        idx    = -1;
    unsigned    mask   = 0;
    bool        enc    = false;
    bool        iso    = false;
    std::string cc;
};

static decision native_serve(const std::string & path, const std::string & prefix, bool use_gzip,
                             const std::string & accept, const std::string & inm) {
    const auto & assets = llama_ui_get_assets();
    decision d;

    // 188-196 + 403-425: the routes the server registers. A path outside the
    // table never enters a handler, so it is 404 before the gzip gate.
    long index = -1;
    for (size_t i = 0; i < assets.size(); i++) {
        if (assets[i].name == "index.html") {
            index = (long) i;
        }
    }
    std::unordered_map<std::string, long> routes;
    if (index >= 0) {
        routes[prefix + "/"]           = index;
        routes[prefix + "/index.html"] = index;
    }
    static const std::set<std::string> no_cache_names = {
        "sw.js", "manifest.webmanifest", "_app/version.json", "build.json",
    };
    for (size_t i = 0; i < assets.size(); i++) {
        if ((long) i == index) {
            continue;
        }
        routes[prefix + "/" + assets[i].name] = (long) i;
    }
    auto it = routes.find(path);
    if (it == routes.end()) {
        return d;                                // 404, no headers
    }
    const long idx = it->second;
    const std::string & name = assets[idx].name;
    const std::string & etag = assets[idx].etag;
    // httplib resolved the route before the handler ran, so the asset is known
    // even when the handler's gzip gate answers 415
    d.idx = idx;

    // 341-355: handle_gzip_header, inside the registered handler
    if (use_gzip) {
        if (accept.find("gzip") == std::string::npos) {
            d.status = 415;
            d.mask   = WEB_H_TYPE;
            return d;
        }
        d.mask |= WEB_H_ENCODING;
        d.enc = true;
    }

    const bool nocache_name = no_cache_names.count(name) != 0;

    // 362-387: serve_asset_cached; 389-401: serve_asset_nocache has no ETag
    if (!nocache_name) {
        d.mask |= WEB_H_ETAG;                    // 370
        if (!inm.empty() && (inm == etag || inm == std::string("W/") + etag)) {
            d.status = 304;                      // 371-374
            return d;
        }
    }
    if (name == "index.html") {                  // isolation == true
        d.mask |= WEB_H_ISOLATION;               // 376-379
        d.iso = true;
    }
    const bool nocache = name == "index.html" || nocache_name;
    d.mask |= WEB_H_CACHE | WEB_H_TYPE;          // 380-385
    d.cc = nocache ? CACHE_REVALIDATE : CACHE_IMMUTABLE;
    d.status = 200;
    return d;
}

// ---- the request corpus ---------------------------------------------------

struct request {
    const char * path;
    const char * prefix;
    bool         use_gzip;
    const char * accept;
    const char * inm;
    const char * note;
};

int main(int argc, char ** argv) {
    goose_init(argc, argv);
    const bool verbose = argc > 1 && std::string(argv[1]) == "-v";

    const auto & assets = llama_ui_get_assets();

    // pack the real table for the port: names blob + offsets, etags blob + offsets
    std::string names, etags;
    std::vector<int64_t> off, eoff;
    for (const auto & a : assets) {
        off.push_back((int64_t) names.size());
        names += a.name;
        eoff.push_back((int64_t) etags.size());
        etags += a.etag;
    }
    off.push_back((int64_t) names.size());
    eoff.push_back((int64_t) etags.size());

    auto find = [&](const std::string & name) -> long {
        for (size_t i = 0; i < assets.size(); i++) {
            if (assets[i].name == name) {
                return (long) i;
            }
        }
        return -1;
    };

    const std::string etag_index = assets[find("index.html")].etag;
    const std::string etag_css   = assets[find("assets/site.css")].etag;
    const std::string etag_sw    = assets[find("sw.js")].etag;

    std::vector<request> corpus = {
        // the index asset, in both spellings
        {"/", "", true, "gzip, deflate", "", "index at /"},
        {"/index.html", "", true, "gzip, deflate", "", "index explicit"},
        {"/", "", false, "", "", "index, no gzip build"},
        {"/", "", false, "gzip", "", "no gzip build ignores accept"},
        // the gzip gate
        {"/", "", true, "", "", "415 before route"},
        {"/does-not-exist", "", true, "", "", "404 survives without the gate"},
        {"/", "", true, "deflate", "", "no gzip token"},
        {"/", "", true, "br;q=1, gzip;q=0.5", "", "gzip later in the list"},
        {"/", "", true, "x-gzip-please", "", "substring, not a token"},
        // 404 shapes
        {"/does-not-exist", "", true, "gzip", "", "plain 404"},
        {"/assets/", "", true, "gzip", "", "trailing slash is not a route"},
        {"/INDEX.HTML", "", true, "gzip", "", "case sensitive route"},
        {"/index.html/", "", true, "gzip", "", "no directory redirect"},
        {"//index.html", "", true, "gzip", "", "double slash"},
        // the validator
        {"/", "", true, "gzip", "PLACEHOLDER_INDEX", "304 for the index"},
        {"/", "", true, "gzip", "PLACEHOLDER_WEAK_INDEX", "weak 304 for the index"},
        {"/assets/site.css", "", true, "gzip", "PLACEHOLDER_CSS", "304 for css"},
        {"/assets/site.css", "", true, "gzip", "\"0xdeadbeefdeadbeef\"", "stale etag"},
        {"/assets/site.css", "", true, "gzip", "W/\"0xnope\"", "weak malformed"},
        // cache policy split
        {"/assets/site.css", "", true, "gzip", "", "immutable css"},
        {"/sw.js", "", true, "gzip", "", "no-cache sw"},
        {"/sw.js", "", true, "gzip", "PLACEHOLDER_SW", "no ETag, inm ignored"},
        {"/build.json", "", true, "gzip", "\"0x0000000000000000\"", "no ETag on build.json"},
        {"/manifest.webmanifest", "", true, "gzip", "", "no-cache manifest"},
        {"/build.json", "", true, "gzip", "", "no-cache build"},
        {"/_app/version.json", "", true, "gzip", "", "no-cache _app version"},
        {"/version.json", "", true, "gzip", "", "immutable root version"},
        {"/workbox.9f8e7d.js", "", true, "gzip", "", "immutable workbox"},
        {"/_app/immutable/bundle.a1b2c3.js", "", true, "gzip", "", "immutable bundle"},
        {"/assets/notes.txt", "", true, "gzip", "", "octet-stream asset"},
        // a non-empty api prefix
        {"/api/", "/api", true, "gzip", "", "index under prefix"},
        {"/api/index.html", "/api", true, "gzip", "", "explicit index under prefix"},
        {"/api/assets/site.css", "/api", true, "gzip", "", "asset under prefix"},
        {"/", "/api", true, "gzip", "", "root is not a prefix route"},
        {"/api", "/api", true, "gzip", "", "prefix without slash"},
        {"/apiassets/site.css", "/api", true, "gzip", "", "prefix glue"},
        {"/api/assets/site.css", "/apix", true, "gzip", "", "wrong prefix"},
        {"/api/nope", "/api", true, "", "", "404 under a prefix, no gzip"},
    };

    for (auto & r : corpus) {
        std::string inm = r.inm;
        if (inm == "PLACEHOLDER_INDEX") {
            inm = etag_index;
        } else if (inm == "PLACEHOLDER_WEAK_INDEX") {
            inm = "W/" + etag_index;
        } else if (inm == "PLACEHOLDER_CSS") {
            inm = etag_css;
        } else if (inm == "PLACEHOLDER_SW") {
            inm = etag_sw;
        }

        const decision want = native_serve(r.path, r.prefix, r.use_gzip, r.accept, inm);

        int64_t out[6] = {0, 0, 0, 0, 0, 0};
        sl_i64 o{out, 6};
        const int64_t rc = web_serve(SL(r.path), SL(r.prefix), SL(names), sl_i64{(int64_t *) off.data(), (int64_t) off.size()},
                                     SL(etags), sl_i64{(int64_t *) eoff.data(), (int64_t) eoff.size()},
                                     r.use_gzip ? 1 : 0, SL(r.accept), SL(inm), o);
        uint8_t cbuf[128];
        sl_u8 co{cbuf, (int64_t) sizeof cbuf};
        const int64_t cn = web_cache_text(out[5], co);
        const std::string cc = cn < 0 ? std::string() : std::string((const char *) cbuf, (size_t) cn);

        cases++;
        const std::string label = std::string(r.note) + " " + r.path + " prefix=" +
                                  (r.prefix[0] ? r.prefix : "<none>");
        eq_int("status", label, want.status, out[0]);
        eq_int("return value", label, want.status, rc);
        eq_int("asset index", label, want.idx, out[1]);
        eq_int("header mask", label, (int64_t) want.mask, out[2]);
        eq_int("encoding", label, want.enc ? 1 : 0, out[3]);
        eq_int("isolation", label, want.iso ? 1 : 0, out[4]);
        eq_str("cache-control", label, want.cc, cc);
        if (verbose) {
            printf("CASE %-58s status=%d idx=%ld mask=%u enc=%d iso=%d cc=%s\n", label.c_str(),
                   want.status, want.idx, want.mask, want.enc ? 1 : 0, want.iso ? 1 : 0,
                   want.cc.empty() ? "-" : want.cc.c_str());
        }
    }

    // the route decision on its own, including paths the serve gate would 415
    const char * routes[][2] = {
        {"/", ""}, {"/index.html", ""}, {"/assets/site.css", ""}, {"/assets/", ""},
        {"/assets", ""}, {"/api/", "/api"}, {"/api", "/api"}, {"/api/assets/site.css", "/api"},
        {"/"}, {"/apix/", "/api"}, {"/version.json", ""}, {"/_app/version.json", ""},
    };
    for (auto & rc : routes) {
        cases++;
        const std::string path = rc[0];
        const std::string prefix = rc[1] ? rc[1] : "";
        const decision want = native_serve(path, prefix, false, "", "");
        const int64_t got = web_route(SL(path), SL(prefix), SL(names),
                                      sl_i64{(int64_t *) off.data(), (int64_t) off.size()},
                                      (int64_t) assets.size());
        eq_int("route", path + " prefix=" + prefix, want.idx, got);
        // isolation of whatever the route resolved to
        eq_int("isolation_of", path + " prefix=" + prefix, want.iso ? 1 : 0,
               web_isolation_of(SL(names), sl_i64{(int64_t *) off.data(), (int64_t) off.size()}, got));
    }

    // the two Cache-Control literals, as text
    cases++;
    {
        uint8_t buf[128];
        sl_u8 out{buf, (int64_t) sizeof buf};
        int64_t n = web_cache_text(0, out);
        eq_str("cache text", "immutable", CACHE_IMMUTABLE, std::string((const char *) buf, (size_t) n));
        n = web_cache_text(1, out);
        eq_str("cache text", "revalidate", CACHE_REVALIDATE, std::string((const char *) buf, (size_t) n));
        n = web_cache_text(2, out);
        eq_str("cache text", "nocache", CACHE_REVALIDATE, std::string((const char *) buf, (size_t) n));
        eq_int("cache text", "unknown policy", -1, web_cache_text(3, out));
    }

    printf("FGWEB_WEB cases=%d values=%d failures=%d\n", cases, values, fails);
    return fails == 0 ? 0 : 1;
}
