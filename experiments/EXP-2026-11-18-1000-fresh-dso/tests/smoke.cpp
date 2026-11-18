// A fresh build of the DSO from current sources must be loadable and must actually
// sample. This calls the sampler directly - no model, no weights, no server - so it is
// a smoke test of the artifact, not a generation run.
//
// The point: EXP999 showed the ACCEPTED library contains none of the canonical unit's
// exports. This asks whether a build from today's sources is a library that works, so
// the gap is "not yet accepted" rather than "does not build".
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <vector>

#include "llama.h"

static int checks = 0;
static int fails = 0;

static void check(int cond, const char *what) {
    checks++;
    if (!cond) {
        fails++;
        std::printf("FAIL %s\n", what);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::printf("usage: %s <libllama.so.0>\n", argv[0]);
        return 2;
    }
    void *h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        std::printf("FAIL dlopen: %s\n", dlerror());
        return 1;
    }
    check(1, "the fresh build loads");

    // Symbols that only exist in a build composed from current sources.
    const char *fresh[6] = { "btl_load", "bt_bpe", "bt_token_to_piece",
                             "tp_apply", "tns_apply", "dry_apply" };
    for (int i = 0; i < 6; i++) {
        char msg[96];
        std::snprintf(msg, sizeof msg, "%s is exported", fresh[i]);
        check(dlsym(h, fresh[i]) != nullptr, msg);
    }

    // And it does the work: the dist sampler picks an index and normalises.
    auto init_dist = (llama_sampler * (*)(uint32_t))dlsym(h, "llama_sampler_init_dist");
    auto apply = (void (*)(llama_sampler *, llama_token_data_array *))dlsym(h, "llama_sampler_apply");
    auto free_s = (void (*)(llama_sampler *))dlsym(h, "llama_sampler_free");
    check(init_dist && apply && free_s, "the sampler entry points are present");
    if (init_dist && apply && free_s) {
        llama_sampler *s = init_dist(1234);
        check(s != nullptr, "a dist sampler can be created");
        if (s) {
            std::vector<llama_token_data> cur(8);
            for (int i = 0; i < 8; i++) {
                cur[(size_t) i] = llama_token_data{ i, (float) i + 1.0f, 0.0f };
            }
            llama_token_data_array arr{ cur.data(), cur.size(), false };
            apply(s, &arr);
            check(arr.selected >= 0 && arr.selected < 8, "a token is selected");
            float sum = 0.0f;
            for (int i = 0; i < 8; i++) sum += cur[(size_t) i].p;
            check(sum > 0.99f && sum < 1.01f, "probabilities are normalised");
            free_s(s);
        }
    }

    std::printf("%s checks=%d failures=%d\n", fails == 0 ? "PASS" : "BAD", checks, fails);
    dlclose(h);
    return fails ? 1 : 0;
}
