// Differential gate for P10-12 S2: the prefill chunk plan.
//
// The oracle is the carrier's loop text: tests/native_extract.py copies the
// `for (...)` header and the `const int chunk = std::min(...)` line verbatim
// out of session.cpp:1541-1542, and this fixture supplies the loop body that
// records (i, chunk). Both arms see the same (n_common, n_prompt, n_batch)
// triples; the gate compares every pair, the pair count, and the invariants the
// carrier relies on (chunks tile the prompt exactly, the last one is the tail).
//
// It also compares two properties that the trace frame depends on: the first
// chunk starts at n_common (absolute positions) and no chunk is empty.
//
// Checks are explicit (no <cassert>): NDEBUG is defined in Release builds.

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "pf.h" // pf_plan, goose_init

namespace {

int g_cases = 0, g_values = 0, g_fails = 0;

void new_case() { ++g_cases; }
void note(bool ok, const std::string & what) {
    ++g_values;
    if (!ok) {
        ++g_fails;
        std::printf("  FAIL [S2] %s\n", what.c_str());
    }
}

// The native arm: the base-commit lines, verbatim, with a recording body.
struct Cfg {
    int n_batch = 1;
};
struct Im {
    Cfg cfg;
};

std::vector<std::pair<int, int>> native_plan(int n_common, int n_prompt, int n_batch) {
    Im im;
    im.cfg.n_batch = n_batch;
    std::vector<std::pair<int, int>> out;
#include "native_prefill_loop.inc"
    out.emplace_back(i, chunk);
    }
    return out;
}

void run_case(int n_common, int n_prompt, int n_batch) {
    new_case();
    const std::vector<std::pair<int, int>> nat = native_plan(n_common, n_prompt, n_batch);

    std::vector<int64_t> out((size_t) (n_prompt - n_common > 0 ? (n_prompt - n_common) * 2 + 8 : 16), 0);
    sl_i64 v{out.data(), (int64_t) out.size()};
    const int64_t n = pf_plan(n_common, n_prompt, n_batch, v);

    char tag[96];
    std::snprintf(tag, sizeof tag, "plan(%d,%d,%d)", n_common, n_prompt, n_batch);
    const std::string t = tag;

    note(n == (int64_t) nat.size(), t + " pair count");
    const size_t common = std::min((size_t) (n < 0 ? 0 : n), nat.size());
    for (size_t k = 0; k < common; ++k) {
        note(out[2 * k] == nat[k].first, t + " start[" + std::to_string(k) + "]");
        note(out[2 * k + 1] == nat[k].second, t + " chunk[" + std::to_string(k) + "]");
    }

    // Invariants the trace frame and the KV bookkeeping rely on.
    long long covered = 0;
    for (size_t k = 0; k < nat.size(); ++k) {
        note(nat[k].second > 0, t + " non-empty chunk[" + std::to_string(k) + "]");
        note(nat[k].second <= n_batch, t + " chunk within batch[" + std::to_string(k) + "]");
        covered += nat[k].second;
    }
    const long long expect = n_prompt > n_common ? (long long) n_prompt - n_common : 0;
    note(covered == expect, t + " chunks tile the prompt");
    if (!nat.empty()) note(nat[0].first == n_common, t + " first start is n_common");
    for (size_t k = 1; k < nat.size(); ++k)
        note(nat[k].first == nat[k - 1].first + nat[k - 1].second, t + " contiguous[" + std::to_string(k) + "]");
}

} // namespace

int main(int argc, char ** argv) {
    goose_init(argc, argv);

    // Reused prefix, tail chunks, a batch larger than the prompt, and the
    // single-token cases. n_batch is the llm_ubatch the config validator keeps
    // at >= 1, so the corpus states it the same way.
    run_case(0, 1, 1);
    run_case(0, 8, 8);
    run_case(0, 8, 512);
    run_case(0, 512, 512);
    run_case(0, 513, 512);
    run_case(0, 1024, 128);
    run_case(0, 1023, 128);
    run_case(7, 7, 1);      // nothing left to prefill
    run_case(7, 8, 4);      // one token after a reused prefix
    run_case(100, 4096, 512);
    run_case(100, 4096, 100); // batch divides the remaining prompt exactly
    run_case(0, 4096, 4095);
    run_case(2048, 2049, 1);
    run_case(0, 3, 2);
    run_case(1, 100, 33);
    run_case(5, 6, 7);

    std::printf("FG649_PF cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails;
}
