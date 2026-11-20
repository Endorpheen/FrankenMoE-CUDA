#include "ggml.h"
#include "gguf.h"
#include "llama.h"

#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

extern "C" void llama_goose_register(const gguf_context *, const char *);
extern "C" void llama_goose_unregister(const gguf_context *);
extern "C" bool llama_goose_check(const gguf_context *);
extern "C" void fg_goose_host_ensure_init();
extern "C" int64_t goose_sampler_calls();

llama_sampler * llama_sampler_init_dry_testing(float, float, int32_t, int32_t,
                                               const std::vector<std::vector<llama_token>> &);

static bool test_reader(const char * path) {
    gguf_context * writer = gguf_init_empty();
    ggml_context * tensors = ggml_init({4096, nullptr, false});
    ggml_tensor * tensor = ggml_new_tensor_2d(tensors, GGML_TYPE_F32, 1, 4);
    ggml_set_name(tensor, "fixture.tensor");
    std::memset(tensor->data, 0, ggml_nbytes(tensor));
    gguf_add_tensor(writer, tensor);
    gguf_set_val_bool(writer, "fixture.bool", true);
    gguf_set_val_str(writer, "fixture.string", "Goose");
    const float floats[] = {0.25f, -0.0f, 3.0f};
    const char * texts[] = {"", "token", "token with space"};
    gguf_set_arr_data(writer, "fixture.floats", GGUF_TYPE_FLOAT32, floats, 3);
    gguf_set_arr_str(writer, "fixture.strings", texts, 3);
    gguf_set_val_u32(writer, "fixture.scalar", 19);
    const bool written = gguf_write_to_file(writer, path, false);
    gguf_free(writer);
    ggml_free(tensors);
    if (!written) {
        return false;
    }

    gguf_context * reader = gguf_init_from_file(path, {true, nullptr});
    if (!reader) {
        return false;
    }
    llama_goose_register(reader, path);
    const bool matched = llama_goose_check(reader);
    const int64_t scalar_index = gguf_find_key(reader, "fixture.scalar");
    gguf_set_val_u32(reader, "fixture.scalar", 20);
    const bool rejected_value = scalar_index == gguf_find_key(reader, "fixture.scalar")
        && !llama_goose_check(reader);
    llama_goose_unregister(reader);
    const bool rejected_unregistered = !llama_goose_check(reader);
    gguf_free(reader);
    std::fprintf(stderr, "GGUF matched=%d rejected_value=%d rejected_unregistered=%d\n",
                 matched, rejected_value, rejected_unregistered);
    return matched && rejected_value && rejected_unregistered;
}

static bool test_dry(bool want_goose) {
    fg_goose_host_ensure_init();
    std::mt19937 rng(31337);
    const std::vector<std::vector<llama_token>> breakers = {{10}, {10, 13}};
    const int64_t before = goose_sampler_calls();
    for (int test = 0; test < 200; ++test) {
        auto * sampler = llama_sampler_init_dry_testing(0.8f, 1.75f, 2, 32, breakers);
        if (!sampler) {
            return false;
        }
        const int history = 24 + rng() % 24;
        for (int i = 0; i < history; ++i) {
            switch (rng() % 4) {
                case 0: llama_sampler_accept(sampler, 10); break;
                case 1: llama_sampler_accept(sampler, 7); break;
                case 2: llama_sampler_accept(sampler, 20 + rng() % 5); break;
                default: llama_sampler_accept(sampler, rng() % 40); break;
            }
        }
        std::vector<llama_token_data> data(16 + rng() % 32);
        for (auto & candidate : data) {
            candidate.id = rng() % 40;
            candidate.logit = -8.0f + 8.0f * static_cast<float>(rng() % 1000) / 1000.0f;
            candidate.p = 0.0f;
        }
        llama_token_data_array candidates{data.data(), data.size(), false};
        llama_sampler_apply(sampler, &candidates);
        llama_sampler_free(sampler);
        std::printf("case %d\n", test);
        for (const auto & candidate : data) {
            uint32_t bits;
            std::memcpy(&bits, &candidate.logit, sizeof bits);
            std::printf("%d %08x\n", candidate.id, static_cast<unsigned>(bits));
        }
    }
    const int64_t calls = goose_sampler_calls() - before;
    std::fprintf(stderr, "DRY mode=%s, Goose calls=%lld\n", want_goose ? "goose" : "native", (long long) calls);
    return calls == (want_goose ? 200 : 0);
}

int main(int argc, char ** argv) {
    if (argc == 3 && std::strcmp(argv[1], "gguf") == 0) {
        return test_reader(argv[2]) ? 0 : 1;
    }
    if (argc == 2 && (std::strcmp(argv[1], "goose") == 0 || std::strcmp(argv[1], "native") == 0)) {
        return test_dry(std::strcmp(argv[1], "goose") == 0) ? 0 : 1;
    }
    return 2;
}
