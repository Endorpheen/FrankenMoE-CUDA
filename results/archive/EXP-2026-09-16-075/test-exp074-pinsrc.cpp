// EXP-2026-09-16-074 model-free gate for the pinned-source direct-DMA prefill path.
//
// Drives the accepted pinned-ring backend interface directly, no model and no server:
//   runtime - replay the real EXP-045/070 range list through
//             set_tensor_async_pinned_ring, read the device bytes back and hash them
//   selftest - synthetic ranges with guards and padding, plus pin/swap teardown accounting
//
// Run the same binary against build/exp062-ring3 (accepted ring) and build/exp074-pinsrc
// (pin source); the hash lists must be identical in every mode.

#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-backend-impl.h"
#include "ggml-cuda.h"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static int checks_pass = 0;
static int checks_fail = 0;

static void check(bool ok, const char * name) {
    printf("CHECK %s %s\n", ok ? "PASS" : "FAIL", name);
    if (ok) checks_pass++; else checks_fail++;
    fflush(stdout);
}

static uint64_t fnv1a64(const uint8_t * p, size_t n) {
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}

static long proc_status_kib(const char * field) {
    FILE * f = fopen("/proc/self/status", "r");
    if (!f) return -1;
    char line[256];
    long v = -1;
    char pat[64];
    snprintf(pat, sizeof(pat), "%s:", field);
    const size_t pl = strlen(pat);
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, pat, pl) == 0) { sscanf(line + pl, " %ld", &v); break; }
    }
    fclose(f);
    return v;
}

struct Range { int shard; uint64_t addr; uint64_t bytes; };

static bool load_ranges(const char * path, std::vector<Range> & out) {
    FILE * f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return false; }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        Range r;
        unsigned long long a = 0, b = 0;
        if (sscanf(line, "%d %llu %llu", &r.shard, &a, &b) != 3) { fclose(f); return false; }
        r.addr = a; r.bytes = b; out.push_back(r);
    }
    fclose(f);
    return true;
}

struct ShardMap { void * base = nullptr; uint64_t size = 0; };

static bool map_shards(const std::vector<std::string> & paths, std::vector<ShardMap> & out) {
    out.resize(paths.size());
    for (size_t i = 0; i < paths.size(); ++i) {
        const int fd = open(paths[i].c_str(), O_RDONLY);
        if (fd < 0) { fprintf(stderr, "cannot open %s\n", paths[i].c_str()); return false; }
        struct stat st;
        if (fstat(fd, &st) != 0) { close(fd); return false; }
        void * p = mmap(nullptr, (size_t) st.st_size, PROT_READ, MAP_SHARED, fd, 0);
        close(fd);
        if (p == MAP_FAILED) { fprintf(stderr, "mmap failed %s\n", paths[i].c_str()); return false; }
        out[i].base = p; out[i].size = (uint64_t) st.st_size;
    }
    return true;
}

// one arena-backed destination tensor on the CUDA device
struct DeviceArena {
    ggml_backend_t        backend = nullptr;
    ggml_backend_buffer_t buffer  = nullptr;
    ggml_context *        ctx     = nullptr;
    ggml_tensor *         tensor  = nullptr;
};

static bool arena_init(DeviceArena & a, size_t bytes) {
    a.backend = ggml_backend_init_by_type(GGML_BACKEND_DEVICE_TYPE_GPU, nullptr);
    if (!a.backend) { fprintf(stderr, "no CUDA backend\n"); return false; }
    a.buffer = ggml_backend_buft_alloc_buffer(ggml_backend_cuda_buffer_type(0), bytes);
    if (!a.buffer) { fprintf(stderr, "no CUDA buffer\n"); return false; }
    struct ggml_init_params params = { /*mem_size*/ 1u << 16, /*mem_buffer*/ nullptr, /*no_alloc*/ true };
    a.ctx = ggml_init(params);
    a.tensor = ggml_new_tensor_1d(a.ctx, GGML_TYPE_I8, (int64_t) bytes);
    ggml_backend_tensor_alloc(a.buffer, a.tensor, ggml_backend_buffer_get_base(a.buffer));
    return true;
}

static void arena_free(DeviceArena & a) {
    if (a.backend) ggml_backend_free(a.backend);
    if (a.buffer)  ggml_backend_buffer_free(a.buffer);
    if (a.ctx)     ggml_free(a.ctx);
    a = DeviceArena{};
}

// ---------------------------------------------------------------------------
// runtime: replay the real range list
// ---------------------------------------------------------------------------
static int mode_runtime(int argc, char ** argv) {
    const char * ranges_path = nullptr;
    const char * out_path    = nullptr;
    size_t arena_mib = 64;
    long long max_ranges = 0;
    std::vector<std::string> shards;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--ranges" && i + 1 < argc) ranges_path = argv[++i];
        else if (a == "--out" && i + 1 < argc) out_path = argv[++i];
        else if (a == "--shard" && i + 1 < argc) shards.push_back(argv[++i]);
        else if (a == "--arena" && i + 1 < argc) arena_mib = (size_t) atoll(argv[++i]);
        else if (a == "--max-ranges" && i + 1 < argc) max_ranges = atoll(argv[++i]);
        else { fprintf(stderr, "unknown arg %s\n", a.c_str()); return 2; }
    }
    if (!ranges_path || !out_path || shards.empty()) {
        fprintf(stderr, "usage: %s runtime --ranges FILE --out FILE --shard PATH [...]\n", argv[0]);
        return 2;
    }

    std::vector<Range> ranges;
    if (!load_ranges(ranges_path, ranges)) return 2;
    if (max_ranges > 0 && (long long) ranges.size() > max_ranges) ranges.resize((size_t) max_ranges);

    std::vector<ShardMap> maps;
    if (!map_shards(shards, maps)) return 2;

    const size_t arena_bytes = arena_mib << 20;
    DeviceArena arena;
    if (!arena_init(arena, arena_bytes)) return 2;

    const long pin_before = proc_status_kib("VmPin");
    const long swap_before = proc_status_kib("VmSwap");

    std::vector<uint64_t> hashes;
    hashes.reserve(ranges.size());
    uint64_t total_bytes = 0;
    size_t   refused = 0;
    uint64_t cursor = 0;
    std::vector<uint8_t> readback(16 << 20);

    const uint64_t t0 = (uint64_t) ggml_time_us();
    for (size_t ri = 0; ri < ranges.size(); ++ri) {
        const Range & r = ranges[ri];
        if ((size_t) r.shard >= maps.size() || r.addr + r.bytes > maps[r.shard].size) {
            fprintf(stderr, "FAIL: range %zu out of bounds\n", ri);
            return 2;
        }
        const uint8_t * src = (const uint8_t *) maps[r.shard].base + r.addr;

        uint64_t off = cursor;
        if (off + r.bytes > arena_bytes) off = 0;
        cursor = off + r.bytes;

        if (r.bytes > readback.size()) readback.resize(r.bytes);
        const bool handled = arena.backend->iface.set_tensor_async_pinned_ring != nullptr &&
            arena.backend->iface.set_tensor_async_pinned_ring(arena.backend, arena.tensor, src, (size_t) off, (size_t) r.bytes);
        ggml_backend_synchronize(arena.backend);
        if (!handled) {
            refused++;
            ggml_backend_tensor_set_async(arena.backend, arena.tensor, src, (size_t) off, (size_t) r.bytes);
            ggml_backend_synchronize(arena.backend);
        }
        ggml_backend_tensor_get(arena.tensor, readback.data(), (size_t) off, (size_t) r.bytes);
        hashes.push_back(fnv1a64(readback.data(), (size_t) r.bytes));
        total_bytes += r.bytes;
    }
    const uint64_t elapsed_us = (uint64_t) ggml_time_us() - t0;

    const long pin_after = proc_status_kib("VmPin");
    const long swap_after = proc_status_kib("VmSwap");

    FILE * f = fopen(out_path, "w");
    if (!f) { fprintf(stderr, "cannot write %s\n", out_path); return 2; }
    fprintf(f, "# seq fnv1a64\n");
    for (size_t i = 0; i < hashes.size(); ++i) {
        fprintf(f, "%llu %llu\n", (unsigned long long) (i + 1), (unsigned long long) hashes[i]);
    }
    fclose(f);

    printf("{\"mode\":\"runtime\",\"ranges\":%zu,\"bytes\":%llu,\"refused\":%zu,"
           "\"elapsed_ms\":%.1f,\"pin_before_kib\":%ld,\"pin_after_kib\":%ld,"
           "\"swap_before_kib\":%ld,\"swap_after_kib\":%ld}\n",
           ranges.size(), (unsigned long long) total_bytes, refused, elapsed_us / 1000.0,
           pin_before, pin_after, swap_before, swap_after);
    fflush(stdout);

    // frees the backend context: the direct-DMA teardown must release the registrations
    arena_free(arena);
    const long pin_free = proc_status_kib("VmPin");
    const long swap_free = proc_status_kib("VmSwap");
    printf("{\"mode\":\"runtime-teamdown\",\"pin_after_free_kib\":%ld,\"swap_after_free_kib\":%ld}\n",
           pin_free, swap_free);
    for (ShardMap & sm : maps) munmap(sm.base, (size_t) sm.size);
    return checks_fail == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------
// selftest: synthetic ranges, guards, padding, teardown
// ---------------------------------------------------------------------------
static int mode_selftest(void) {
    const size_t EXP  = (getenv("EXP074_EXPERT_SIZE") ? (size_t) atoll(getenv("EXP074_EXPERT_SIZE")) : (size_t)(4 << 20));
    const size_t NEXP = 10;
    const size_t SRC_BYTES = EXP * NEXP;
    const size_t GUARD = 4096;
    const size_t arena_bytes = 2 * GUARD + SRC_BYTES;

    void * src_map = mmap(nullptr, SRC_BYTES, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (src_map == MAP_FAILED) { fprintf(stderr, "anon mmap failed\n"); return 2; }
    uint8_t * src = (uint8_t *) src_map;
    for (size_t i = 0; i < SRC_BYTES; ++i) src[i] = (uint8_t) (i * 31 + (i >> 8) * 7 + 11);
    std::vector<uint8_t> src_copy(src, src + SRC_BYTES);

    DeviceArena arena;
    if (!arena_init(arena, arena_bytes)) return 2;

    // head/tail guards around the weight region; the whole arena starts as 0xA5
    std::vector<uint8_t> zero(arena_bytes, 0xA5);
    ggml_backend_tensor_set(arena.tensor, zero.data(), 0, arena_bytes);

    const long pin_before = proc_status_kib("VmPin");
    const long swap_before = proc_status_kib("VmSwap");

    // Ranges: page-disjoint experts, a page-adjacent expert, a range that overlaps a registered
    // range (must fall back), and a range contained in a registered range (must be free).
    struct TRange { size_t off; size_t size; const char * name; };
    const size_t pad = std::min<size_t>(EXP, 512);
    std::vector<TRange> ranges = {
        { 0 * EXP,                EXP,        "first expert (direct)" },
        { 2 * EXP,                EXP + pad,  "expert + padding (direct)" },
        { 3 * EXP + 4096,         EXP,        "page-adjacent (direct)" },
        { 6 * EXP,                EXP + pad,  "later expert (direct)" },
        { 6 * EXP + EXP / 4,      2 * EXP,    "overlaps registered range (fallback)" },
        { 6 * EXP,                EXP,        "contained in registered range (direct)" },
        { 8 * EXP,                EXP,        "page-aligned end (direct)" },
    };

    bool all_ok = true;
    for (const TRange & r : ranges) {
        printf("SELFTEST range %-32s off=%zu size=%zu\n", r.name, r.off, r.size);
        fflush(stdout);
        const bool handled = arena.backend->iface.set_tensor_async_pinned_ring != nullptr &&
            arena.backend->iface.set_tensor_async_pinned_ring(arena.backend, arena.tensor, src + r.off, GUARD + r.off, r.size);
        ggml_backend_synchronize(arena.backend);
        if (!handled) {
            all_ok = false;
            printf("SELFTEST refused %s\n", r.name);
        }
    }

    // device image equals the source over the covered ranges, guards untouched
    std::vector<uint8_t> got(SRC_BYTES);
    ggml_backend_tensor_get(arena.tensor, got.data(), GUARD, SRC_BYTES);
    std::vector<uint8_t> want(SRC_BYTES, 0xA5);
    for (const TRange & r : ranges) memcpy(want.data() + r.off, src + r.off, r.size);
    check(all_ok && got == want, "selftest: covered ranges copied byte-exact, others untouched");

    std::vector<uint8_t> g(2 * GUARD);
    ggml_backend_tensor_get(arena.tensor, g.data(), 0, GUARD);
    ggml_backend_tensor_get(arena.tensor, g.data() + GUARD, GUARD + SRC_BYTES, GUARD);
    check(memcmp(g.data(), std::vector<uint8_t>(2 * GUARD, 0xA5).data(), 2 * GUARD) == 0, "selftest: sentinel guards unchanged");
    check(memcmp(src, src_copy.data(), SRC_BYTES) == 0, "selftest: source mapping unchanged");

    const long pin_during = proc_status_kib("VmPin");
    printf("SELFTEST pin_before=%ld during=%ld swap_before=%ld\n", pin_before, pin_during, swap_before);

    arena_free(arena);
    const long pin_free = proc_status_kib("VmPin");
    const long swap_free = proc_status_kib("VmSwap");
    check(pin_free >= 0 && (pin_before < 0 || pin_free <= pin_before + 1024), "selftest: pinned bytes released on teardown");
    check(swap_free == 0, "selftest: VmSwap stays 0");
    printf("SELFTEST pin_after_free=%ld swap_after_free=%ld\n", pin_free, swap_free);

    munmap(src_map, SRC_BYTES);
    return checks_fail == 0 ? 0 : 1;
}

int main(int argc, char ** argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s runtime|selftest ...\n", argv[0]); return 1; }
    if (!strcmp(argv[1], "runtime"))  return mode_runtime(argc - 1, argv + 1);
    if (!strcmp(argv[1], "selftest")) return mode_selftest();
    fprintf(stderr, "unknown mode %s\n", argv[1]);
    return 1;
}
