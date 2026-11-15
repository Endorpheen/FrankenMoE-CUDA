// Differential gate for P9-08 G5: the sampler chain construction.
//
// The oracle is the carrier's own text: tests/native_extract.py copies
// session.cpp:937-945 verbatim (the static_assert and the four
// llama_sampler_chain_add calls) and this fixture compiles it against a stub of
// the llama sampler API that records the chain instead of allocating it. Both
// arms see the same SamplingConfig; the gate compares the existence of a chain
// and every stage with its arguments, in order, bit-exactly.
//
// The stub is the whole point: `llama_sampler_init_*` returning a tagged record
// turns "which calls did the carrier make" into comparable data, so a port that
// adds the stages in another order or with another argument cannot pass.
//
// Checks are explicit (no <cassert>): NDEBUG is defined in Release builds.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "bmoe/config.h" // the real SamplingConfig
#include "sch.h"          // sch_plan, goose_init

using namespace bmoe; // main.cpp:40 — the carrier's own using-directive

// ── stub llama sampler API ─────────────────────────────────────────────────
#define LLAMA_DEFAULT_SEED 0xFFFFFFFFu

struct llama_sampler;
struct llama_sampler_chain_params {
    bool no_perf = false;
};

namespace {

struct Stage {
    int kind;   // 0 chain, 1 top_k, 2 top_p, 3 temp, 4 dist
    int64_t iarg;
    double farg;
    double min_keep; // only top_p uses it; -1 elsewhere
};

std::vector<Stage> g_stages;
Stage g_pool[128];
int g_pool_n = 0;

Stage * take(int kind) {
    Stage * s = &g_pool[g_pool_n++ % 128];
    s->kind = kind;
    s->iarg = 0;
    s->farg = 0.0;
    s->min_keep = -1.0;
    return s;
}

int g_chain = 0;

} // namespace

llama_sampler_chain_params llama_sampler_chain_default_params() { return llama_sampler_chain_params{}; }

llama_sampler * llama_sampler_chain_init(llama_sampler_chain_params) {
    g_chain = 1;
    return (llama_sampler *) take(0);
}

void llama_sampler_chain_add(llama_sampler *, llama_sampler * child) {
    g_stages.push_back(*(const Stage *) child);
}

llama_sampler * llama_sampler_init_top_k(int32_t k) {
    Stage * s = take(1);
    s->iarg = k;
    return (llama_sampler *) s;
}

llama_sampler * llama_sampler_init_top_p(float p, size_t min_keep) {
    Stage * s = take(2);
    s->min_keep = (double) min_keep;
    s->farg = (double) p;
    return (llama_sampler *) s;
}

llama_sampler * llama_sampler_init_temp(float t) {
    Stage * s = take(3);
    s->farg = (double) t;
    return (llama_sampler *) s;
}

llama_sampler * llama_sampler_init_dist(uint32_t seed) {
    Stage * s = take(4);
    s->iarg = (int64_t) seed;
    return (llama_sampler *) s;
}

namespace {

struct Im {
    llama_sampler * smpl = nullptr;
};

int g_cases = 0, g_values = 0, g_fails = 0;

void new_case() { ++g_cases; }
void note(bool ok, const std::string & what) {
    ++g_values;
    if (!ok) {
        ++g_fails;
        std::printf("  FAIL [G5] %s\n", what.c_str());
    }
}

// Run the carrier text with `cfg` in scope, exactly as Session::open does.
void native_chain(const bmoe::SamplingConfig & sc, Im & im) {
    g_stages.clear();
    g_pool_n = 0;
    g_chain = 0;
    bmoe::RunConfig cfg;
    cfg.sampling = sc;
    {
#include "native_sampler_chain.inc"
    }
}

void run_case(const char * tag, float temp, int top_k, float top_p, uint32_t seed) {
    new_case();
    bmoe::SamplingConfig sc;
    sc.temp = temp;
    sc.top_k = top_k;
    sc.top_p = top_p;
    sc.seed = seed;

    Im im;
    native_chain(sc, im);
    const bool native_chain_on = im.smpl != nullptr || g_chain != 0;

    int64_t out[64];
    double outf[16];
    std::memset(out, 0, sizeof out);
    std::memset(outf, 0, sizeof outf);
    sl_i64 ov{out, 64};
    sl_f64 of{outf, 16};
    const int64_t n = sch_plan((double) temp, (int64_t) top_k, (double) top_p, (int64_t) seed, ov, of);

    const std::string t = tag;
    note(sch_on((double) temp) == (native_chain_on ? 1 : 0), t + " chain presence");
    note(sch_entries((double) temp) == n, t + " stage count");
    note(n == (int64_t) g_stages.size(), t + " stage count matches carrier");
    const size_t common = (size_t) (n < (int64_t) g_stages.size() ? n : (int64_t) g_stages.size());
    for (size_t k = 0; k < common; ++k) {
        const Stage & s = g_stages[k];
        note(out[3 * k] == (int64_t) s.kind, t + " kind[" + std::to_string(k) + "]");
        note(out[3 * k + 1] == s.iarg, t + " iarg[" + std::to_string(k) + "]");
        if (s.min_keep >= 0.0)
            note(out[3 * k + 2] == (int64_t) s.min_keep, t + " min_keep[" + std::to_string(k) + "]");
        else
            note(out[3 * k + 2] == 0, t + " no min_keep[" + std::to_string(k) + "]");
        // Bit-exact float comparison: the carrier stores the f32 and the plan
        // carries the widened value, so widening both sides is the comparison.
        note(std::memcmp(&outf[k], &s.farg, sizeof(double)) == 0, t + " farg[" + std::to_string(k) + "]");
    }

    // The order the carrier relies on: top_k, top_p, temp, dist.
    if (g_stages.size() == 4) {
        note(g_stages[0].kind == 1 && g_stages[1].kind == 2 && g_stages[2].kind == 3 && g_stages[3].kind == 4,
             t + " carrier order");
    }
}

} // namespace

int main(int argc, char ** argv) {
    goose_init(argc, argv);

    // The default-seed assertion the carrier static_asserts, stated once here so
    // a config.h edit that moves the default fails the gate rather than a sample.
    {
        new_case();
        bmoe::SamplingConfig d;
        note((int64_t) d.seed == sch_default_seed(), "default seed mirror");
        note(sch_default_seed() == (int64_t) LLAMA_DEFAULT_SEED, "default seed is LLAMA_DEFAULT_SEED");
        note(sch_seed_gate((int64_t) d.seed) == 1, "seed gate accepts the default");
        note(sch_seed_gate((int64_t) d.seed - 1) == 0, "seed gate rejects a changed default");
        note(sch_min_keep() == 1, "top_p min_keep is one");
    }

    // Greedy, then the whole opt-in range. The tiny positive temp is the case
    // the `>= 0` mutant would take for greedy and the `> 1e-6` mutant would drop.
    run_case("temp=0", 0.0f, 40, 0.95f, 0xFFFFFFFFu);
    run_case("temp=-1", -1.0f, 40, 0.95f, 0xFFFFFFFFu);
    run_case("temp=-0.5", -0.5f, 8, 0.5f, 7u);
    run_case("temp=1e-7", 1e-7f, 40, 0.95f, 0xFFFFFFFFu);
    run_case("temp=1e-3", 1e-3f, 1, 0.001f, 0u);
    run_case("defaults", 0.8f, 40, 0.95f, 0xFFFFFFFFu);
    run_case("topk-off", 0.1f, 0, 0.9f, 1234u);
    run_case("topk-one", 2.0f, 1, 1.0f, 0u);
    run_case("wide", 1.5f, 100, 0.5f, 4294967294u);
    run_case("sharp", 0.999f, 5, 0.001f, 99u);
    run_case("hot", 3.0f, 64, 0.9f, 0xFFFFFFFFu);
    run_case("unit", 1.0f, 40, 0.95f, 0xFFFFFFFEu);
    run_case("tiny-p", 0.7f, 20, 1e-6f, 42u);

    std::printf("FG654_SCH cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails;
}
