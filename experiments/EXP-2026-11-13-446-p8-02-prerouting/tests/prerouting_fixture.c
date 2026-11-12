// Differential fixture for the Goose pre-routing policy (EXP446).
//
// Right arm: a C transcription of server-http.cpp set_pre_routing_handler,
// middleware_validate_api_key and origin_is_localhost (host extraction matches
// common_http_parse_url for the accepted inputs).
// Left arm: the Goose module emitted by the pinned FrankenGoose compiler.
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int64_t fg446_preroute(int64_t a, int64_t b, int64_t c, uint64_t d, int64_t e, int64_t f,
                       int64_t g, int64_t h, int64_t i, int64_t j, uint64_t k, int64_t l,
                       uint64_t m, int64_t n, uint64_t o, int64_t p, uint64_t q);

uint32_t fg446_ld_u8(uint64_t p, uint64_t o) { return ((const uint8_t *) (uintptr_t) p)[o]; }

void fg446_st_i64(uint64_t p, uint64_t o, int64_t v) {
    *(int64_t *) ((char *) (uintptr_t) p + o) = v;
}

enum { W_ACTION = 0, W_STATUS, W_ALLOW_MODE, W_ALLOW_LEN, W_OPTIONS, W_REASON };
enum { CORS_LITERAL = 0, CORS_STAR = 1, CORS_LOCALHOST = 2 };

static int origin_is_localhost_native(const char * o) {
    size_t n = strlen(o);
    const char * p = strstr(o, "://");
    const char * s = p ? p + 3 : o;
    const char * e = s + strcspn(s, "/?#");
    for (const char * q = s; q < e; ++q) if (*q == '@') s = q + 1;
    if (s < e && *s == '[') {
        const char * r = s + 1;
        while (r < e && *r != ']') ++r;
        if (r >= e) return 0;
        return (size_t) (r - (s + 1)) == 3 && s[1] == ':' && s[2] == ':' && s[3] == '1';
    }
    const char * colon = NULL;
    for (const char * q = s; q < e; ++q) if (*q == ':') colon = q;
    const char * he = colon ? colon : e;
    size_t hl = (size_t) (he - s);
    if (hl == 9 && memcmp(s, "localhost", 9) == 0) return 1;
    if (hl == 9 && memcmp(s, "127.0.0.1", 9) == 0) return 1;
    if (hl == 3 && memcmp(s, "::1", 3) == 0) return 1;
    return 0;
}

struct P {
    int64_t cors_cred, cors_mode, cors_lit_len;
    const char * origin;
    int64_t is_options, is_ready, is_frontend, is_public;
    int64_t n_keys;
    const char * auth;
    const char * xapi;
    const char * key;
};

static void native_rules(const struct P * p, int64_t out[6]) {
    const int64_t origin_len = (int64_t) strlen(p->origin);
    int64_t action = 0, status = 0, allow_mode = 0, allow_len = 0, options_hdr = 0, reason = 0;

    if (p->cors_mode == CORS_STAR) {
        if (p->cors_cred) { allow_mode = 1; allow_len = origin_len; }
        else { allow_mode = 2; allow_len = p->cors_lit_len; }
    } else if (p->cors_mode == CORS_LOCALHOST) {
        if (origin_len > 0 && origin_is_localhost_native(p->origin)) {
            allow_mode = 1; allow_len = origin_len;
        }
    } else {
        allow_mode = 2; allow_len = p->cors_lit_len;
    }

    if (p->is_options) {
        options_hdr = 1; action = 1; status = 200;
    } else if (!p->is_ready && !p->is_frontend) {
        action = 1; status = 503; reason = 1;
    } else if (p->n_keys > 0 && !p->is_public) {
        const char * v = p->auth[0] ? p->auth : p->xapi;
        const char * k = v;
        size_t kl = strlen(v);
        if (kl >= 7 && memcmp(k, "Bearer ", 7) == 0) { k += 7; kl -= 7; }
        int ok = kl == strlen(p->key) && memcmp(k, p->key, kl) == 0;
        if (!ok) { action = 1; status = 401; reason = 2; }
    }

    out[W_ACTION] = action; out[W_STATUS] = status;
    out[W_ALLOW_MODE] = allow_mode; out[W_ALLOW_LEN] = allow_len;
    out[W_OPTIONS] = options_hdr; out[W_REASON] = reason;
}

static int cases = 0, checks = 0, fails = 0;

static void one(const struct P * p) {
    int64_t ref[6] = {0}, go[6] = {0};
    native_rules(p, ref);
    fg446_preroute(p->cors_cred, p->cors_mode, p->cors_lit_len,
                   (uint64_t) (uintptr_t) p->origin, (int64_t) strlen(p->origin),
                   p->is_options, p->is_ready, p->is_frontend, p->is_public, p->n_keys,
                   (uint64_t) (uintptr_t) p->auth, (int64_t) strlen(p->auth),
                   (uint64_t) (uintptr_t) p->xapi, (int64_t) strlen(p->xapi),
                   (uint64_t) (uintptr_t) p->key, (int64_t) strlen(p->key),
                   (uint64_t) (uintptr_t) go);
    ++cases;
    for (int w = 0; w < 6; ++w) {
        ++checks;
        if (ref[w] != go[w]) {
            ++fails;
            if (fails <= 6) {
                printf("FAIL case=%d word=%d native=%lld goose=%lld "
                       "[cred=%lld mode=%lld litlen=%lld origin='%s' opts=%lld ready=%lld front=%lld pub=%lld "
                       "keys=%lld auth='%s' xapi='%s' key='%s']\n",
                       cases, w, (long long) ref[w], (long long) go[w],
                       (long long) p->cors_cred, (long long) p->cors_mode, (long long) p->cors_lit_len,
                       p->origin, (long long) p->is_options, (long long) p->is_ready,
                       (long long) p->is_frontend, (long long) p->is_public, (long long) p->n_keys,
                       p->auth, p->xapi, p->key);
            }
        }
    }
}

int main(void) {
    const char * origins[] = {
        "", "http://localhost:8080", "https://localhost", "http://127.0.0.1:3000",
        "http://[::1]:8080", "https://example.com", "null", "localhost",
        "https://user:pass@localhost:80/x", "http://LOCALHOST:80", "http://127.0.0.2",
        "http://[::2]", "file://localhost/etc", "http://localhostevil.com",
        "https://sub.localhost", "http://[::1]", "http://127.0.0.1",
    };
    const char * auths[] = {"", "Bearer sk-ok", "sk-ok", "Bearer  sk-ok", "Bearer sk-bad",
                            "bearer sk-ok", "Bearer ", "Beare"};
    const char * xapis[] = {"", "sk-ok", "sk-bad"};
    const char * keys[] = {"sk-ok", "", "sk-ok-2"};
    const int64_t creds[] = {0, 1};
    const int64_t modes[] = {CORS_LITERAL, CORS_STAR, CORS_LOCALHOST};
    const int64_t litlens[] = {0, 1, 14};
    const int64_t flags[] = {0, 1};
    const int64_t nkeys[] = {0, 1, 3};

    for (size_t o = 0; o < sizeof(origins) / sizeof(origins[0]); ++o)
    for (size_t a = 0; a < sizeof(auths) / sizeof(auths[0]); ++a)
    for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); ++k)
    for (size_t c = 0; c < 2; ++c)
    for (size_t m = 0; m < 3; ++m)
    for (size_t l = 0; l < 3; ++l)
    for (size_t f1 = 0; f1 < 2; ++f1)   // options
    for (size_t f2 = 0; f2 < 2; ++f2)   // ready
    for (size_t f3 = 0; f3 < 2; ++f3)   // frontend
    for (size_t f4 = 0; f4 < 2; ++f4)   // public
    for (size_t nk = 0; nk < 3; ++nk) {
        struct P p = {
            creds[c], modes[m], litlens[l], origins[o],
            flags[f1], flags[f2], flags[f3], flags[f4], nkeys[nk],
            auths[a], "", keys[k],
        };
        one(&p);
    }

    // X-Api-Key fallback is only reachable with an empty Authorization header.
    for (size_t o = 0; o < sizeof(origins) / sizeof(origins[0]); ++o)
    for (size_t x = 0; x < sizeof(xapis) / sizeof(xapis[0]); ++x)
    for (size_t k = 0; k < sizeof(keys) / sizeof(keys[0]); ++k) {
        struct P p = {0, CORS_LITERAL, 0, origins[o], 0, 1, 0, 0, 1, "", xapis[x], keys[k]};
        one(&p);
    }

    printf("FG446_PREROUTE cases=%d values=%d failures=%d\n", cases, checks, fails);
    return fails ? 1 : 0;
}
