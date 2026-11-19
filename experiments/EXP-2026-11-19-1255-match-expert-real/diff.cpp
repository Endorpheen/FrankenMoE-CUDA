// The matcher against the real native one. router_hook.cpp is included as a translation unit so
// its static match_expert is reachable, which removes the last transcription from the recipe check.
#include "core/src/moe/router_hook.cpp"
#include <dlfcn.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <random>

// White-box stub, and it cannot influence the compared decision: major_faults() feeds a telemetry
// counter that match_expert never reads. platform_io.cpp is compiled only in the full engine build,
// which cannot be built against this tree (EXP-1250).
namespace bmoe { namespace pio { uint64_t major_faults() { return 0; } } }
// Workload capture/replay is a separate engine feature, also absent from the policy build. Stubbed for
// the same reason as above: match_expert reads neither, and a stub that returns "nothing recorded"
// keeps the decision path identical.
namespace bmoe {
void DeterministicWorkload::set_static(std::string, int, int) {}
bool DeterministicWorkload::check_static(const std::string &, int, int) { return false; }
bool DeterministicWorkload::prompt(std::vector<int32_t> &) { return false; }
void DeterministicWorkload::begin_batch(int, int, int) {}
bool DeterministicWorkload::capture_topk(int, int, int, const int32_t *) { return false; }
bool DeterministicWorkload::replay_topk(int, int, int, const int32_t *&) { return false; }
bool DeterministicWorkload::token(size_t, int32_t, int32_t &) { return false; }
bool DeterministicWorkload::finish(size_t) { return false; }
}

struct sl_u8  { uint8_t * data; int64_t len; };
struct sl_i64 { int64_t * data; int64_t len; };
typedef int64_t (*fn_match)(sl_u8, int64_t, sl_i64);

int main() {
    void * h = dlopen("/home/end0/AI/FrankenMoE-CUDA/builds/flint-acc/bin/libllama.so.0", RTLD_LAZY);
    if (!h) { printf("dlopen: %s\n", dlerror()); return 2; }
    auto port = (fn_match) dlsym(h, "ar_match");
    if (!port) { printf("missing door ar_match\n"); return 3; }

    std::mt19937 rng(909);
    int fails = 0, cases = 0;
    const int n = bmoe::n_moe_recipes();
    for (int row = 0; row < n; ++row) {
        const bmoe::MoeRecipe * rec = bmoe::moe_recipe_at(row);
        for (int slot = 0; slot < bmoe::MoeRecipe::max_exps; ++slot) {
            const char * suf = rec->exps_suffix[slot];
            if (suf == nullptr) continue;
            for (int rep = 0; rep < 40; ++rep) {
                const int layer = (int)(rng() % 200);
                // positive, the .scale companion, a wrong suffix, and a missing prefix
                const std::string variants[4] = {
                    "blk." + std::to_string(layer) + "." + suf + ".weight",
                    "blk." + std::to_string(layer) + "." + suf + ".scale",
                    "blk." + std::to_string(layer) + ".not_a_suffix.weight",
                    "layer" + std::to_string(layer) + "." + suf + ".weight",
                };
                for (const auto & nm : variants) {
                    int native_lay = -1;
                    const int native = match_expert(nm.c_str(), *rec, native_lay);

                    int64_t lay[2] = { -1, -1 };
                    sl_i64 slay{ lay, 2 };
                    sl_u8 sn{ (uint8_t *) nm.data(), (int64_t) nm.size() };
                    const int64_t got = port(sn, row, slay);

                    ++cases;
                    const bool ok = native == (int) got &&
                                    (native < 0 || native_lay == (int) lay[0]);
                    if (!ok) {
                        ++fails;
                        if (fails <= 5) {
                            printf("MISMATCH name=%s native=%d(native_lay=%d) port=%lld(port_lay=%lld)\n",
                                   nm.c_str(), native, native_lay, (long long) got, (long long) lay[0]);
                        }
                    }
                }
            }
        }
    }
    printf("match_expert differential: cases=%d fails=%d recipes=%d\n", cases, fails, n);
    return fails ? 1 : 0;
}
