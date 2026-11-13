// Gate for EXP250: the Goose slot-budget logic against a direct C transcription of server_slot.
#include <cstdint>
#include <cstdio>

extern "C" int64_t fg250s_remaining(int64_t n_predict_max, int64_t n_gen);
extern "C" int64_t fg250s_has_budget(int64_t n_predict_max, int64_t n_gen);

static int64_t ref_remaining(int64_t m, int64_t g) { return m == -1 ? -1 : m - g; }
static int64_t ref_has_budget(int64_t m, int64_t g) { return (m == -1 || ref_remaining(m, g) > 0) ? 1 : 0; }

int main() {
    int cases = 0, failures = 0;
    const int64_t maxes[] = {-1, 0, 1, 2, 7, 128, 4096, 32768, 1000000};
    for (int64_t m : maxes) {
        for (int64_t g = 0; g <= (m < 0 ? 4096 : (m < 4096 ? m + 2 : 4098)); ++g) {
            ++cases;
            if (fg250s_remaining(m, g) != ref_remaining(m, g)) {
                if (failures < 3) std::fprintf(stderr, "DIFF remaining m=%lld g=%lld goose=%lld ref=%lld\n",
                                               (long long) m, (long long) g,
                                               (long long) fg250s_remaining(m, g), (long long) ref_remaining(m, g));
                ++failures;
            }
            if (fg250s_has_budget(m, g) != ref_has_budget(m, g)) {
                if (failures < 6) std::fprintf(stderr, "DIFF has_budget m=%lld g=%lld\n", (long long) m, (long long) g);
                ++failures;
            }
        }
    }
    std::printf("FG250S_GATE cases=%d failures=%d\n", cases, failures);
    return failures ? 1 : 0;
}
