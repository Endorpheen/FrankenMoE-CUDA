#include <llama.h>
#include <cstdio>
#include <cstring>
#include <vector>
static uint64_t st = 12345;
static float lcg() { st = st * 6364136223846793005ULL + 1442695040888963407ULL; return (float) ((st >> 40) & 0xFFFFFF) / (float) 0x1000000; }
int main(int argc, char ** argv) {
    const int n = 248320, draws = 32;
    std::vector<float> lg(n);
    std::vector<llama_token_data> d(n);
    std::vector<int32_t> out;
    for (int cfg = 0; cfg < 3; ++cfg) {
        const int32_t k = cfg == 0 ? 20 : (cfg == 1 ? 40 : 100);
        const float tp = cfg == 0 ? 0.95f : (cfg == 1 ? 0.9f : 0.5f);
        const float temp = cfg == 0 ? 1.0f : (cfg == 1 ? 0.8f : 1.2f);
        const uint32_t seeds[4] = {0u, 1u, 42u, 123456789u};
        for (int si = 0; si < 4; ++si) {
            st = 12345;
            for (auto & x : lg) x = lcg() * 20.0f - 10.0f;
            auto cp = llama_sampler_chain_default_params();
            llama_sampler * ch = llama_sampler_chain_init(cp);
            llama_sampler_chain_add(ch, llama_sampler_init_top_k(k));
            llama_sampler_chain_add(ch, llama_sampler_init_top_p(tp, 1));
            llama_sampler_chain_add(ch, llama_sampler_init_min_p(0.0f, 1));
            llama_sampler_chain_add(ch, llama_sampler_init_temp(temp));
            llama_sampler_chain_add(ch, llama_sampler_init_dist(seeds[si]));
            for (int dr = 0; dr < draws; ++dr) {
                for (int i = 0; i < n; ++i) d[(size_t) i] = llama_token_data{(llama_token) i, lg[(size_t) i], 0.0f};
                llama_token_data_array cur{d.data(), d.size(), -1, false};
                llama_sampler_apply(ch, &cur);
                if (cur.selected < 0 || (size_t) cur.selected >= cur.size) { out.push_back(-1); continue; }
                out.push_back((int32_t) cur.data[cur.selected].id);
            }
            llama_sampler_free(ch);
        }
    }
    FILE * f = fopen(argv[1], "wb");
    fwrite(out.data(), sizeof(int32_t), out.size(), f);
    fclose(f);
    int64_t nonzero = 0; for (auto v : out) nonzero += v >= 0;
    printf("draws=%zu valid=%lld first=%d\n", out.size(), (long long) nonzero, out.empty() ? -1 : out[0]);
    return 0;
}
