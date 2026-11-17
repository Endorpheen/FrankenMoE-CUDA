#include "runtime-gen.h"
#include <cstdio>
#include <cstring>
#include <vector>

static int fails = 0, checks = 0;
#define CHECK(c, ...) do { ++checks; if (!(c)) { std::printf("FAIL " __VA_ARGS__); std::printf("\n"); ++fails; } } while (0)

static void expect(int64_t tid, const std::vector<int64_t> &p, const std::vector<int64_t> &want) {
    int64_t out[4] = {0,0,0,0};
    int64_t nd = mx_shape(tid, sl_i64{(int64_t*)p.data(), (int64_t)p.size()}, sl_i64{out,4});
    CHECK(nd == (int64_t)want.size(), "tid %lld ndims %lld want %zu", (long long)tid, (long long)nd, want.size());
    for (size_t i = 0; i < want.size() && i < 4; ++i)
        CHECK(out[i] == want[i], "tid %lld dim%zu %lld want %lld", (long long)tid, i, (long long)out[i], (long long)want[i]);
}

int main() {
    goose_init(0, nullptr);
    // real Maxwell: hidden 2048, 32 heads, 4 kv, head_dim 128, ff 6144,
    // expert ff 512, shared 1024, 256 experts, 45 layers, 3 dense, hc 4
    std::vector<int64_t> p = {2048, 200704, 32, 4, 128, 6144, 512, 1024, 256, 45, 3, 4, 4, 128};
    expect(0, p, {2048, 200704});
    expect(5, p, {2048, 4096});     // attn_q
    expect(6, p, {2048, 512});      // attn_k
    expect(7, p, {2048, 512});      // attn_v
    expect(8, p, {4096, 2048});     // attn_o
    expect(9, p, {128});            // q norm
    expect(11, p, {8192, 24});      // hc_attn_fn
    expect(12, p, {24});            // hc base
    expect(13, p, {3});             // hc scale
    expect(17, p, {2048, 6144});    // dense gate
    expect(19, p, {6144, 2048});    // dense down
    expect(20, p, {2048, 256});     // gate_inp
    expect(21, p, {256});           // expert bias
    expect(22, p, {2048, 512, 256});// gate_exps
    expect(24, p, {512, 2048, 256});// down_exps
    expect(25, p, {2048, 1024});    // shexp gate
    expect(27, p, {1024, 2048});    // shexp down
    expect(28, p, {8192, 4});       // hc head fn
    expect(29, p, {4});             // hc head base
    expect(30, p, {1});             // hc head scale
    expect(31, p, {2048, 512});     // indexer q_proj (4 groups x 128)
    expect(32, p, {2048, 128});     // indexer k_proj
    expect(33, p, {128});           // indexer q norm
    expect(34, p, {128});           // indexer k norm
    expect(35, p, {2048});          // mtp enorm
    expect(36, p, {2048});          // mtp hnorm
    expect(37, p, {2048, 2048});    // mtp e_proj
    expect(38, p, {2048, 2048});    // mtp h_proj
    expect(39, p, {8192, 4});       // mtp hc head fn
    expect(40, p, {2048});          // mtp final norm

    std::vector<int64_t> kinds(45, -1);
    int64_t n = mx_plan(sl_i64{(int64_t*)p.data(), (int64_t)p.size()}, sl_i64{kinds.data(), (int64_t)kinds.size()});
    CHECK(n == 45, "plan n=%lld", (long long)n);
    for (int i = 0; i < 45; ++i) CHECK(kinds[i] == (i < 3 ? 0 : 1), "kind %d = %lld", i, (long long)kinds[i]);
    CHECK(mx_n_tensors() == 41, "n tensors %lld", (long long)mx_n_tensors());
    std::printf("checks=%d failures=%d\n", checks, fails);
    if (!fails) std::printf("MAXWELL-SHAPES OK\n");
    return fails ? 1 : 0;
}
