#include "src/llama-mmap-expert-pin.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace llama_mmap_expert_pin;
static int calls = 0, fail = -1, releases = 0;
static bool reg(void *, size_t) { return calls++ != fail; }
static void unreg(void *) { ++releases; }
int main() {
    auto spans = merge_pages({{8193, 9000}, {1, 4096}, {4095, 4097}, {32768, 32769}}, 4096);
    assert(spans.size() == 2 && spans[0].begin == 0 && spans[0].end == 12288);
    assert(spans[1].begin == 32768 && spans[1].end == 36864);
    for (auto p : {uintptr_t(0), uintptr_t(3)}) {
        bool threw = false;
        try { merge_pages({{1, 2}}, p); } catch (const std::runtime_error &) { threw = true; }
        assert(threw);
    }
    bool threw = false;
    try { merge_pages({{UINTPTR_MAX - 2, UINTPTR_MAX}}, 4096); }
    catch (const std::runtime_error &) { threw = true; }
    assert(threw);
    constexpr uint64_t gib = UINT64_C(1) << 30;
    assert(fits_budget(20*gib, 64*gib, 28*gib));
    assert(!fits_budget(20*gib, 64*gib, 28*gib-1));
    assert(fits_budget(20*gib, 64*gib, 40*gib));
    assert(!fits_budget(33*gib, 64*gib, 64*gib));
    assert(!fits_budget(0, 64*gib, 64*gib));
    for (int failed = 0; failed < 3; ++failed) {
        calls = releases = 0; fail = failed;
        { owner o; assert(!o.acquire({{4096,8192},{12288,16384},{20480,24576}}, reg, unreg)); assert(o.owned == 0); }
        assert(calls == failed+1 && releases == failed);
    }
    calls = releases = 0; fail = -1;
    { owner o; assert(o.acquire(spans, reg, unreg)); o.release(); o.release(); }
    assert(calls == 2 && releases == 2);
    calls = releases = 0;
    try { owner o; assert(o.acquire(spans, reg, unreg)); throw std::runtime_error("injected"); }
    catch (const std::runtime_error &) {}
    assert(calls == 2 && releases == 2);
    std::cout << "PASS: merging, alignment overflow, budget boundaries, all partial failures, idempotent cleanup, exception cleanup\n";
}
