// The Hugging Face cache naming rules as a table.
//
// Repo validity, token and commit shapes, the folder name and its inverse, and whether a subpath stays under its base.
// These decide which path the loader writes to, so a change here is a change in where files land. Pure functions: no model,
// no GPU, no network.
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

struct sl_u8 { const void * data; int64_t len; };
typedef int64_t (*s_fn)(struct sl_u8);
typedef int64_t (*so_fn)(struct sl_u8, struct sl_u8);
typedef int64_t (*ss_fn)(struct sl_u8, struct sl_u8);

static struct sl_u8 S(const char * s) { struct sl_u8 r = { s, (int64_t) strlen(s) }; return r; }

int main(int argc, char ** argv) {
    const char * path = argc > 1 ? argv[1] : "builds/flint-acc/bin/libllama.so.0";
    void * h = dlopen(path, RTLD_LAZY);
    if (!h) { printf("cannot open %s: %s\n", path, dlerror()); return 2; }
    s_fn repo_valid = (s_fn) dlsym(h, "hf_repo_valid");
    s_fn commit_valid = (s_fn) dlsym(h, "hf_commit_valid");
    s_fn oid_valid = (s_fn) dlsym(h, "hf_oid_valid");
    so_fn folder = (so_fn) dlsym(h, "hf_repo_folder");
    so_fn repo_of = (so_fn) dlsym(h, "hf_folder_repo");
    ss_fn sub_ok = (ss_fn) dlsym(h, "hf_subpath_ok");
    if (!repo_valid || !commit_valid || !oid_valid || !folder || !repo_of || !sub_ok) {
        printf("a door is missing\n"); return 2; }
    int fails = 0;

    // repository ids: exactly one '/', base characters only, specials surrounded by base characters
    struct { const char * s; int want; } rc[] = {
        { "org/model", 1 }, { "openai/whisper-large", 1 }, { "a/b_c.d-e", 1 }, { "a/b", 1 },
        { "", 0 }, { "model", 0 }, { "/model", 0 }, { "org/", 0 }, { "org//model", 0 },
        { "or g/model", 0 }, { "org/mo del", 0 }, { "org/model/", 0 }, { "org/mo..del", 0 }, { "org/model-name", 1 },
        { "-org/model", 0 }, { "org/.model", 0 },
    };
    for (size_t i = 0; i < sizeof(rc)/sizeof(rc[0]); ++i) {
        const int64_t got = repo_valid(S(rc[i].s)) != 0;
        if (got != rc[i].want) { ++fails; printf("  REPO '%s' got=%lld want=%d\n", rc[i].s, (long long) got, rc[i].want); }
    }
    // commits: exactly 40 hex; oids: 40 or 64 hex
    struct { const char * s; int commit; int oid; } cc[] = {
        { "0123456789abcdef0123456789abcdef01234567", 1, 1 },
        { "0123456789ABCDEF0123456789abcdef01234567", 1, 1 },
        { "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef", 0, 1 },
        { "0123456789abcdef0123456789abcdef0123456", 0, 0 },
        { "0123456789abcdef0123456789abcdef0123456g", 0, 0 },
        { "", 0, 0 },
    };
    for (size_t i = 0; i < sizeof(cc)/sizeof(cc[0]); ++i) {
        const int64_t c = commit_valid(S(cc[i].s)) != 0, o = oid_valid(S(cc[i].s)) != 0;
        if (c != cc[i].commit || o != cc[i].oid) { ++fails;
            printf("  COMMIT '%s' commit=%lld want=%d oid=%lld want=%d\n", cc[i].s, (long long) c, cc[i].commit,
                   (long long) o, cc[i].oid); }
    }
    // folder name and back: models--org--name, and the inverse refuses a missing prefix
    struct { const char * repo; const char * want; } fc[] = {
        { "org/model", "models--org--model" }, { "a/b_c.d-e", "models--a--b_c.d-e" },
    };
    char out[256];
    for (size_t i = 0; i < sizeof(fc)/sizeof(fc[0]); ++i) {
        memset(out, 0, sizeof(out));
        const int64_t n = folder(S(fc[i].repo), (struct sl_u8){ out, sizeof(out) - 1 });
        if (n != (int64_t) strlen(fc[i].want) || strcmp(out, fc[i].want) != 0) { ++fails;
            printf("  FOLDER '%s' -> '%s' want '%s'\n", fc[i].repo, out, fc[i].want); }
        memset(out, 0, sizeof(out));
        const int64_t m = repo_of(S(fc[i].want), (struct sl_u8){ out, sizeof(out) - 1 });
        if (m <= 0 || strcmp(out, fc[i].repo) != 0) { ++fails;
            printf("  INVERSE '%s' -> '%s' want '%s'\n", fc[i].want, out, fc[i].repo); }
    }
    {
        memset(out, 0, sizeof(out));
        const int64_t n = repo_of(S("datasets--org--model"), (struct sl_u8){ out, sizeof(out) - 1 });
        if (n > 0) { ++fails; printf("  MISSING PREFIX accepted -> '%s'\n", out); }
    }
    // a subpath may not climb out of its base
    struct { const char * base; const char * sub; int want; } sc[] = {
        { "/cache/models--a--b", "snapshots/abc/file.gguf", 1 },
        { "/cache/models--a--b", "snapshots/abc/../../../../etc/passwd", 0 },
        { "/cache/models--a--b", "", 1 },
        { "/cache/models--a--b", "/abs/path", 0 },
        { "/cache/models--a--b", "refs/main", 1 },
    };
    for (size_t i = 0; i < sizeof(sc)/sizeof(sc[0]); ++i) {
        const int64_t got = sub_ok(S(sc[i].base), S(sc[i].sub)) != 0;
        if (got != sc[i].want) { ++fails;
            printf("  SUBPATH base='%s' sub='%s' got=%lld want=%d\n", sc[i].base, sc[i].sub, (long long) got, sc[i].want); }
    }

    printf("hf naming: repo=%zu commit_oid=%zu folder=%zu subpath=%zu failures=%d\n",
           sizeof(rc)/sizeof(rc[0]), sizeof(cc)/sizeof(cc[0]), sizeof(fc)/sizeof(fc[0]),
           sizeof(sc)/sizeof(sc[0]), fails);
    dlclose(h);
    return fails == 0 ? 0 : 1;
}
