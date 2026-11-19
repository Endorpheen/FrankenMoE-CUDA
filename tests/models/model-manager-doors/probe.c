#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct sl_u8  { const void * data; int64_t len; };
struct sl_i64 { const int64_t * data; int64_t len; };

int main(int argc, char ** argv) {
    void * h = dlopen(argc > 1 ? argv[1] : "libllama.so.0", RTLD_LAZY);
    if (!h) { printf("cannot open the library: %s\n", dlerror()); return 2; }
    typedef int64_t (*str_fn)(int64_t, struct sl_u8);
    typedef int64_t (*two_fn)(int64_t, int64_t);
    typedef int64_t (*unload_fn)(int64_t, int64_t, struct sl_i64);
    typedef int64_t (*remove_fn)(int64_t, int64_t, struct sl_i64);
    str_fn status = (str_fn) dlsym(h, "mg_status_str");
    str_fn source = (str_fn) dlsym(h, "mg_source_str");
    two_fn stop  = (two_fn) dlsym(h, "mg_stop_gate");
    unload_fn unload = (unload_fn) dlsym(h, "mg_unload_action");
    remove_fn remove = (remove_fn) dlsym(h, "mg_remove_gate");
    if (!status || !source || !stop || !unload || !remove) { printf("a door is missing\n"); return 2; }
    int fails = 0;
    char buf[32];
    static const char * want_status[] = { "downloading", "incomplete", "downloaded", "unloaded", "loading", "loaded", "sleeping" };
    for (int i = 0; i < 7; ++i) {
        memset(buf, 0, sizeof buf);
        if (status(i, (struct sl_u8){ buf, sizeof buf - 1 }) <= 0 || strcmp(buf, want_status[i]) != 0) {
            ++fails; printf("  STATUS %d -> '%s' want '%s'\n", i, buf, want_status[i]);
        }
    }
    static const char * want_source[] = { "preset", "models_dir", "cache" };
    for (int i = 0; i < 3; ++i) {
        memset(buf, 0, sizeof buf);
        if (source(i, (struct sl_u8){ buf, sizeof buf - 1 }) <= 0 || strcmp(buf, want_source[i]) != 0) {
            ++fails; printf("  SOURCE %d -> '%s' want '%s'\n", i, buf, want_source[i]);
        }
    }
    // mg_stop_gate: 1 only for a mapped name with no stop in flight
    struct { int mapped, stopping, want; } s[] = { {0,0,0}, {0,1,0}, {1,1,0}, {1,0,1} };
    for (size_t i = 0; i < sizeof s / sizeof s[0]; ++i) {
        const int64_t got = stop(s[i].mapped, s[i].stopping);
        if ((got != 0) != (s[i].want != 0)) { ++fails;
            printf("  STOP mapped=%d stopping=%d got=%lld want=%d\n", s[i].mapped, s[i].stopping, (long long) got, s[i].want); }
    }
    // mg_remove_gate: 0 accepted, 1 not found, 2 not from the cache
    struct { int mapped, src, want; } r[] = { {0,2,1}, {1,2,0}, {1,0,2}, {1,1,2} };
    for (size_t i = 0; i < sizeof r / sizeof r[0]; ++i) {
        int64_t out[2] = { -1, -1 };
        remove(r[i].mapped, r[i].src, (struct sl_i64){ out, 2 });
        if (out[0] != r[i].want) { ++fails;
            printf("  REMOVE mapped=%d src=%d verdict=%lld want=%d\n", r[i].mapped, r[i].src, (long long) out[0], r[i].want); }
    }
    // mg_unload_action: 0 nothing, 1 downloading, 2 running, 3 idle
    struct { int mapped, st, action; } u[] = { {0,5,0}, {1,0,1}, {1,5,2}, {1,1,3} };
    for (size_t i = 0; i < sizeof u / sizeof u[0]; ++i) {
        int64_t out[4] = { -1, -1, -1, -1 };
        unload(u[i].mapped, u[i].st, (struct sl_i64){ out, 4 });
        if (out[0] != u[i].action) { ++fails;
            printf("  UNLOAD mapped=%d status=%d action=%lld want=%d\n", u[i].mapped, u[i].st,
                   (long long) out[0], u[i].action); }
    }
    printf("model manager doors: status=7 source=3 stop=4 remove=4 unload=4 failures=%d\n", fails);
    return fails == 0 ? 0 : 1;
}
