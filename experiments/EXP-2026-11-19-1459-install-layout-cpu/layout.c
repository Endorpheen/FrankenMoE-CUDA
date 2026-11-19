// The install layout decisions as a table: kind, destination, versioned name, the version string and the guard.
//
// These decide where a library lands and what it is called, which is the part of naming that must not drift while the
// project moves from the llama names to the Goose ones (rule 34). Pure functions, so no model and no GPU.
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

struct sl_u8 { const void * data; int64_t len; };
typedef int64_t (*ii_fn)(int64_t, int64_t);
typedef int64_t (*io_fn)(int64_t, int64_t, int64_t, struct sl_u8);

int main(int argc, char ** argv) {
    const char * path = argc > 1 ? argv[1] : "builds/flint-acc/bin/libllama.so.0";
    void * h = dlopen(path, RTLD_LAZY);
    if (!h) { printf("cannot open %s: %s\n", path, dlerror()); return 2; }
    ii_fn kind = (ii_fn) dlsym(h, "inst_kind");
    ii_fn dest = (ii_fn) dlsym(h, "inst_dest");
    ii_fn versioned = (ii_fn) dlsym(h, "inst_versioned");
    ii_fn guard = (ii_fn) dlsym(h, "inst_guard");
    io_fn version = (io_fn) dlsym(h, "inst_version");
    if (!kind || !dest || !versioned || !guard || !version) { printf("a door is missing\n"); return 2; }

    int fails = 0;
    // kind: 2 when dynamic, else 1 when shared, else 0
    struct { int dl, shared, want; } kcases[] = { {1,1,2}, {1,0,2}, {0,1,1}, {0,0,0} };
    for (size_t i = 0; i < sizeof(kcases)/sizeof(kcases[0]); ++i) {
        const int64_t got = kind(kcases[i].dl, kcases[i].shared);
        if (got != kcases[i].want) { ++fails; printf("  KIND dl=%d shared=%d got=%lld want=%d\n",
            kcases[i].dl, kcases[i].shared, (long long) got, kcases[i].want); }
    }
    // dest: 0 when not dynamic, 2 when the directory is set, 1 otherwise
    struct { int dl, set, want; } dcases[] = { {0,1,0}, {0,0,0}, {1,1,2}, {1,0,1} };
    for (size_t i = 0; i < sizeof(dcases)/sizeof(dcases[0]); ++i) {
        const int64_t got = dest(dcases[i].dl, dcases[i].set);
        if (got != dcases[i].want) { ++fails; printf("  DEST dl=%d set=%d got=%lld want=%d\n",
            dcases[i].dl, dcases[i].set, (long long) got, dcases[i].want); }
    }
    // versioned: only a dynamic build on Apple is unversioned
    struct { int dl, apple, want; } vcases[] = { {1,1,0}, {1,0,1}, {0,1,1}, {0,0,1} };
    for (size_t i = 0; i < sizeof(vcases)/sizeof(vcases[0]); ++i) {
        const int64_t got = versioned(vcases[i].dl, vcases[i].apple);
        if (got != vcases[i].want) { ++fails; printf("  VERSIONED dl=%d apple=%d got=%lld want=%d\n",
            vcases[i].dl, vcases[i].apple, (long long) got, vcases[i].want); }
    }
    // guard: only for a static build with a directory set
    struct { int dl, set, want; } gcases[] = { {0,1,1}, {0,0,0}, {1,1,0}, {1,0,0} };
    for (size_t i = 0; i < sizeof(gcases)/sizeof(gcases[0]); ++i) {
        const int64_t got = guard(gcases[i].dl, gcases[i].set);
        if (got != gcases[i].want) { ++fails; printf("  GUARD dl=%d set=%d got=%lld want=%d\n",
            gcases[i].dl, gcases[i].set, (long long) got, gcases[i].want); }
    }
    // version: major.minor.patch, with no trailing zeros trimmed
    struct { int a, b, c; const char * want; } ncases[] = {
        {3, 0, 0, "3.0.0"}, {0, 12, 4, "0.12.4"}, {12, 10, 100, "12.10.100"}, {0, 0, 0, "0.0.0"},
    };
    char out[64];
    for (size_t i = 0; i < sizeof(ncases)/sizeof(ncases[0]); ++i) {
        memset(out, 0, sizeof(out));
        const int64_t n = version(ncases[i].a, ncases[i].b, ncases[i].c, (struct sl_u8){ out, sizeof(out) - 1 });
        if (n != (int64_t) strlen(ncases[i].want) || strcmp(out, ncases[i].want) != 0) {
            ++fails; printf("  VERSION %d.%d.%d got '%s' want '%s'\n", ncases[i].a, ncases[i].b, ncases[i].c,
                            out, ncases[i].want); }
    }

    printf("install layout: kind=4 dest=4 versioned=4 guard=4 version=%zu failures=%d\n",
           sizeof(ncases)/sizeof(ncases[0]), fails);
    dlclose(h);
    return fails == 0 ? 0 : 1;
}
