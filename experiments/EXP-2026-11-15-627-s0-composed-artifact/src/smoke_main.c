// Smoke host for the composed Goose artifact. Two modes:
//
//   smoke           call one exported function from every set through the
//                   single unified shared object, so the link and the ABI are
//                   exercised together;
//   smoke --dlopen  load each per-set shared object with dlopen, resolve its
//                   runtime entry and its representative export, and report.
//
// No model, server, tokenizer or thread is involved.
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

int run_server(void);
int run_quant(void);
int run_gguf(void);
int run_p9_06(void);
int run_jinja_schema(void);

#ifndef SMOKE_LIBDIR
#define SMOKE_LIBDIR "build/lib"
#endif

struct probe {
    const char *set;
    const char *init;
    const char *entry;
};

static const struct probe PROBES[] = {
    {"server", "goose_init_server", "mcp_config_lines"},
    {"quant", "goose_init_quant", "qz_row_size"},
    {"gguf", "goose_init_gguf", "s2_write"},
    {"p9-06", "goose_init_p9_06", "cli_read_line"},
    {"jinja-schema", "goose_init_jinja_schema", "json_schema_convert"},
};

static int dlopen_check(void) {
    int bad = 0;
    for (size_t i = 0; i < sizeof PROBES / sizeof PROBES[0]; i++) {
        char path[4096];
        snprintf(path, sizeof path, "%s/libgoose_%s.so", SMOKE_LIBDIR,
                 PROBES[i].set);
        void *h = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (!h) {
            printf("dlopen: %-13s FAIL %s\n", PROBES[i].set, dlerror());
            bad++;
            continue;
        }
        void (*init)(int, char **) = (void (*)(int, char **))dlsym(h, PROBES[i].init);
        void *entry = dlsym(h, PROBES[i].entry);
        if (!init || !entry) {
            printf("dlopen: %-13s FAIL missing symbol %s\n", PROBES[i].set,
                   init ? PROBES[i].entry : PROBES[i].init);
            bad++;
            dlclose(h);
            continue;
        }
        char *argv[] = {(char *)"smoke", NULL};
        init(1, argv);
        printf("dlopen: %-13s ok  %p %s=%p\n", PROBES[i].set, (void *)init,
               PROBES[i].entry, entry);
        if (dlclose(h) != 0) {
            printf("dlopen: %-13s FAIL dlclose\n", PROBES[i].set);
            bad++;
        }
    }
    printf("dlopen: %d/%zu sets loaded\n", (int)(sizeof PROBES / sizeof PROBES[0]) - bad,
           sizeof PROBES / sizeof PROBES[0]);
    return bad ? 1 : 0;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "--dlopen") == 0) return dlopen_check();

    int fails = 0;
    fails += run_server();
    fails += run_quant();
    fails += run_gguf();
    fails += run_p9_06();
    fails += run_jinja_schema();
    printf("smoke:  %d/5 sets ok through libgoose_unified.so\n", 5 - fails);
    return fails ? 1 : 0;
}
