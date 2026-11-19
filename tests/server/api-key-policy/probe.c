/* The API-key door against the middleware's documented behaviour.
 *
 * server-http.cpp's middleware: skip when no keys are configured; skip when the path is public; take
 * Authorization, else X-Api-Key; strip a "Bearer " prefix; accept when the result is one of the
 * configured keys. This asks the Goose door key_authorize the same questions and checks the verdict.
 */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct sl { const void * data; int64_t len; };
typedef int64_t (*auth_fn)(struct sl, struct sl, struct sl, struct sl, struct sl, struct sl, struct sl);

static auth_fn g_auth;

static int64_t ask(const char * keys, const int64_t * koff, int64_t nk,
                   const char * path, const char * pub, const int64_t * poff, int64_t np,
                   const char * auth, const char * alt) {
    return g_auth((struct sl){ keys, (int64_t) strlen(keys) },
                  (struct sl){ koff, nk },
                  (struct sl){ path, (int64_t) strlen(path) },
                  (struct sl){ pub, (int64_t) strlen(pub) },
                  (struct sl){ poff, np },
                  (struct sl){ auth, (int64_t) strlen(auth) },
                  (struct sl){ alt, (int64_t) strlen(alt) });
}

int main(int argc, char ** argv) {
    void * h = dlopen(argc > 1 ? argv[1] : "libllama.so.0", RTLD_LAZY);
    if (!h) { printf("cannot open: %s\n", dlerror()); return 2; }
    g_auth = (auth_fn) dlsym(h, "key_authorize");
    if (!g_auth) { printf("key_authorize is missing\n"); return 2; }

    const char * keys = "secret";           /* one configured key */
    const int64_t koff[1] = { 0 };
    const char * pub = "/health";           /* one public path */
    const int64_t poff[1] = { 0 };

    struct { const char * name; const char * path; const char * auth; const char * alt; int want; } cases[] = {
        { "bearer ok",        "/v1/chat", "Bearer secret", "",       1 },
        { "raw key ok",       "/v1/chat", "secret",        "",       1 },
        { "wrong key",        "/v1/chat", "Bearer wrong",  "",       0 },
        { "no credential",    "/v1/chat", "",              "",       0 },
        { "anthropic header", "/v1/chat", "",              "secret", 1 },
        { "public path",      "/health",  "",              "",       1 },
        { "bearer empty",     "/v1/chat", "Bearer ",       "",       0 },
    };

    int fails = 0;
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        const int64_t r = ask(keys, koff, 1, cases[i].path, pub, poff, 1, cases[i].auth, cases[i].alt);
        const int got = r > 0 ? 1 : (r == 0 ? 0 : -1);
        if (got != cases[i].want) {
            ++fails;
            printf("  %-18s verdict=%d want=%d (raw %lld)\n", cases[i].name, got, cases[i].want, (long long) r);
        }
    }
    printf("api-key door: cases=%zu failures=%d\n", sizeof cases / sizeof cases[0], fails);
    return fails == 0 ? 0 : 1;
}
