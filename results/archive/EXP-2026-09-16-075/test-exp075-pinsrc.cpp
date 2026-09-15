// EXP-2026-09-16-075 model-free gate for the pinned-source overlap-split path.
//
// Drives the accepted pinned-ring backend interface directly, no model and no server:
//   runtime  - replay the real EXP-045/070 range list through set_tensor_async_pinned_ring,
//              read the device bytes back and hash them (EXP-074 hash-list format)
//   selftest - synthetic ranges with guards, padding and partial overlaps
//   shifted  - register range set A, then request a shifted set B that only partly overlaps A
//
// The release counters are captured through the ggml log callback, so the binary reports the
// direct/split/fallback split and the teardown result without a model.
//
// Run the same binary against build/exp062-ring3 (accepted ring) and build/exp075-pinsrc-overlap:
// the hash lists must be identical in every mode.
//
// Env:
//   GGML_EXPERT_PIN_OVERLAP_SPLIT=0  restore the EXP-074 behaviour (refuse on any overlap)
//   GGML_EXPERT_PIN_REGISTER_FAIL=1  force every registration to fail (fallback end to end)
//   GGML_EXPERT_PIN_SOURCE=0         disable the pinned source (ring only)

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

// ---------------------------------------------------------------------------
// release counter capture through the ggml log callback
// ---------------------------------------------------------------------------
struct PinCounters {
    unsigned long long direct = 0, split = 0, fallback = 0, fail = 0, intervals = 0;
    unsigned long long bytes_released = 0, bytes_still = 0, sub_direct = 0, sub_reg = 0;
    bool seen = false;
};

static PinCounters g_pin;

static void log_cb(enum ggml_log_level level, const char * text, void * user_data) {
    (void) level; (void) user_data;
    fputs(text, stderr);
    static const char * key = "expert pinned source:";
    const char * p = strstr(text, key);
    if (p == nullptr) {
        return;
    }
    PinCounters c;
    if (sscanf(p + strlen(key),
               " %llu direct ranges, %llu split ranges, %llu fallback ranges, %llu register failures,"
               " %llu intervals released, %llu bytes released, %llu bytes still pinned,"
               " %llu covered sub-copies, %llu registered sub-copies",
               &c.direct, &c.split, &c.fallback, &c.fail, &c.intervals,
               &c.bytes_released, &c.bytes_still, &c.sub_direct, &c.sub_reg) == 9) {
        c.seen = true;
        g_pin = c;
    }
}

// ---------------------------------------------------------------------------
// range list and shard mapping
// ---------------------------------------------------------------------------
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

static bool call_ring(DeviceArena & a, const void * src, size_t dst_off, size_t size) {
    return a.backend->iface.set_tensor_async_pinned_ring != nullptr &&
        a.backend->iface.set_tensor_async_pinned_ring(a.backend, a.tensor, src, dst_off, size);
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
        const bool handled = call_ring(arena, src, (size_t) off, (size_t) r.bytes);
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
    const long hwm_after = proc_status_kib("VmHWM");

    FILE * f = fopen(out_path, "w");
    if (!f) { fprintf(stderr, "cannot write %s\n", out_path); return 2; }
    fprintf(f, "# seq fnv1a64\n");
    for (size_t i = 0; i < hashes.size(); ++i) {
        fprintf(f, "%llu %llu\n", (unsigned long long) (i + 1), (unsigned long long) hashes[i]);
    }
    fclose(f);

    printf("{\"mode\":\"runtime\",\"ranges\":%zu,\"bytes\":%llu,\"refused\":%zu,"
           "\"elapsed_ms\":%.1f,\"pin_before_kib\":%ld,\"pin_after_kib\":%ld,"
           "\"swap_before_kib\":%ld,\"swap_after_kib\":%ld,\"va_hwm_kib\":%ld}\n",
           ranges.size(), (unsigned long long) total_bytes, refused, elapsed_us / 1000.0,
           pin_before, pin_after, swap_before, swap_after, hwm_after);
    fflush(stdout);

    // frees the backend context: the direct-DMA teardown must release the registrations
    arena_free(arena);
    const long pin_free = proc_status_kib("VmPin");
    const long swap_free = proc_status_kib("VmSwap");
    printf("{\"mode\":\"runtime-teardown\",\"pin_after_free_kib\":%ld,\"swap_after_free_kib\":%ld,"
           "\"counters_seen\":%s,\"direct\":%llu,\"split\":%llu,\"fallback\":%llu,\"fail\":%llu,"
           "\"intervals\":%llu,\"bytes_released\":%llu,\"bytes_still_pinned\":%llu,"
           "\"sub_direct\":%llu,\"sub_reg\":%llu}\n",
           pin_free, swap_free, g_pin.seen ? "true" : "false", g_pin.direct, g_pin.split,
           g_pin.fallback, g_pin.fail, g_pin.intervals, g_pin.bytes_released,
           g_pin.bytes_still, g_pin.sub_direct, g_pin.sub_reg);
    fflush(stdout);

    for (ShardMap & sm : maps) munmap(sm.base, (size_t) sm.size);
    return checks_fail == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------
// selftest: synthetic ranges, guards, padding, partial overlaps, teardown
// ---------------------------------------------------------------------------
static int mode_selftest(void) {
    const size_t EXP  = (getenv("EXP075_EXPERT_SIZE") ? (size_t) atoll(getenv("EXP075_EXPERT_SIZE")) : (size_t)(4 << 20));
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
    // range (split when enabled, fallback in EXP-074 mode), and a range contained in a
    // registered range (free direct copy).
    struct TRange { size_t off; size_t size; const char * name; };
    const size_t pad = std::min<size_t>(EXP, 512);
    std::vector<TRange> ranges = {
        { 0 * EXP,                EXP,        "first expert (direct)" },
        { 2 * EXP,                EXP + pad,  "expert + padding (direct)" },
        { 3 * EXP + 4096,         EXP,        "page-adjacent (direct)" },
        { 6 * EXP,                EXP + pad,  "later expert (direct)" },
        { 6 * EXP + EXP / 4,      2 * EXP,    "overlaps registered range (split)" },
        { 6 * EXP,                EXP,        "contained in registered range (direct)" },
        { 8 * EXP,                EXP,        "page-aligned end (split when enabled)" },
    };

    bool all_ok = true;
    for (const TRange & r : ranges) {
        printf("SELFTEST range %-40s off=%zu size=%zu\n", r.name, r.off, r.size);
        fflush(stdout);
        const bool handled = call_ring(arena, src + r.off, GUARD + r.off, r.size);
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

    const bool split_on = getenv("GGML_EXPERT_PIN_OVERLAP_SPLIT") == nullptr ||
        strcmp(getenv("GGML_EXPERT_PIN_OVERLAP_SPLIT"), "0") != 0;
    printf("SELFTEST counters seen=%d direct=%llu split=%llu fallback=%llu fail=%llu intervals=%llu "
           "bytes_released=%llu bytes_still=%llu sub_direct=%llu sub_reg=%llu\n",
           g_pin.seen, g_pin.direct, g_pin.split, g_pin.fallback, g_pin.fail, g_pin.intervals,
           g_pin.bytes_released, g_pin.bytes_still, g_pin.sub_direct, g_pin.sub_reg);
    check(g_pin.seen, "selftest: release counters captured");
    check(g_pin.bytes_still == 0, "selftest: teardown leaves no registered byte");
    if (split_on) {
        check(g_pin.fallback == 0 && g_pin.split >= 1, "selftest: overlap served by split, no fallback");
    } else {
        check(g_pin.split == 0 && g_pin.fallback >= 1, "selftest: overlap refused in EXP-074 mode");
    }

    munmap(src_map, SRC_BYTES);
    return checks_fail == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------
// shifted: register set A, then request a shifted set B that partly overlaps A
// ---------------------------------------------------------------------------
static int mode_shifted(void) {
    const size_t E    = 4 << 20;   // expert spacing
    const size_t BLK  = 3 << 20;   // registered block per expert
    const size_t NK   = 8;         // experts in set A
    const size_t GUARD = 4096;
    const size_t REGION = 64 << 20;
    const size_t SRC_BYTES = NK * E + E;
    const size_t arena_bytes = 2 * GUARD + REGION;

    void * src_map = mmap(nullptr, SRC_BYTES, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (src_map == MAP_FAILED) { fprintf(stderr, "anon mmap failed\n"); return 2; }
    uint8_t * src = (uint8_t *) src_map;
    for (size_t i = 0; i < SRC_BYTES; ++i) src[i] = (uint8_t) (i * 131 + (i >> 7) * 17 + 5);

    DeviceArena arena;
    if (!arena_init(arena, arena_bytes)) return 2;

    std::vector<uint8_t> zero(arena_bytes, 0xA5);
    ggml_backend_tensor_set(arena.tensor, zero.data(), 0, arena_bytes);

    std::vector<uint8_t> want(REGION, 0xA5);

    struct Req { const uint8_t * src; size_t src_off; size_t dst_off; size_t size; const char * name; };
    std::vector<Req> reqs;
    // set A: 8 page-aligned experts (3 MiB each, 1 MiB gaps)
    for (size_t k = 0; k < NK; ++k) {
        reqs.push_back({ src, k * E, k * BLK, BLK, "A expert" });
    }
    // set B fully contained: inside each registered expert
    for (size_t k = 0; k < NK; ++k) {
        reqs.push_back({ src, k * E + BLK / 2, 24 * (1 << 20) + k * (1 << 20), 1 << 20, "B contained" });
    }
    // set B shifted: starts inside an expert, crosses its end, ends inside the next expert
    for (size_t k = 0; k + 1 < NK; ++k) {
        reqs.push_back({ src, k * E + 2 * (1 << 20), 33 * (1 << 20) + k * BLK, BLK, "B shifted" });
    }

    const long swap_before = proc_status_kib("VmSwap");
    bool all_ok = true;
    for (const Req & r : reqs) {
        const bool handled = call_ring(arena, r.src + r.src_off, GUARD + r.dst_off, r.size);
        ggml_backend_synchronize(arena.backend);
        if (!handled) { all_ok = false; printf("SHIFTED refused %s\n", r.name); }
        memcpy(want.data() + r.dst_off, r.src + r.src_off, r.size);
    }

    std::vector<uint8_t> got(REGION);
    ggml_backend_tensor_get(arena.tensor, got.data(), GUARD, REGION);
    check(all_ok && got == want, "shifted: device image byte-exact over A and B");

    std::vector<uint8_t> g(2 * GUARD, 0xA5);
    ggml_backend_tensor_get(arena.tensor, g.data(), 0, GUARD);
    ggml_backend_tensor_get(arena.tensor, g.data() + GUARD, GUARD + REGION, GUARD);
    check(memcmp(g.data(), std::vector<uint8_t>(2 * GUARD, 0xA5).data(), 2 * GUARD) == 0, "shifted: sentinel guards unchanged");

    const long hwm = proc_status_kib("VmHWM");
    const long swap_during = proc_status_kib("VmSwap");

    arena_free(arena);
    const long swap_free = proc_status_kib("VmSwap");

    const bool split_on = getenv("GGML_EXPERT_PIN_OVERLAP_SPLIT") == nullptr ||
        strcmp(getenv("GGML_EXPERT_PIN_OVERLAP_SPLIT"), "0") != 0;
    const bool regfail = getenv("GGML_EXPERT_PIN_REGISTER_FAIL") != nullptr;

    printf("{\"mode\":\"shifted\",\"split_env\":%s,\"regfail\":%s,\"counters_seen\":%s,\"direct\":%llu,\"split\":%llu,"
           "\"fallback\":%llu,\"fail\":%llu,\"intervals\":%llu,\"bytes_released\":%llu,"
           "\"bytes_still_pinned\":%llu,\"sub_direct\":%llu,\"sub_reg\":%llu,"
           "\"swap_before_kib\":%ld,\"swap_during_kib\":%ld,\"swap_after_free_kib\":%ld,\"va_hwm_kib\":%ld}\n",
           split_on ? "true" : "false", regfail ? "true" : "false", g_pin.seen ? "true" : "false",
           g_pin.direct, g_pin.split, g_pin.fallback, g_pin.fail, g_pin.intervals, g_pin.bytes_released,
           g_pin.bytes_still, g_pin.sub_direct, g_pin.sub_reg,
           swap_before, swap_during, swap_free, hwm);
    fflush(stdout);

    check(g_pin.seen, "shifted: release counters captured");
    check(swap_during == 0 && swap_free == 0, "shifted: VmSwap stays 0");
    check(g_pin.bytes_still == 0, "shifted: teardown leaves no registered byte");
    if (regfail) {
        // the first attempt per A interval fails and blacklists it; later requests overlap the
        // blacklist and fall back without a new register attempt
        check(g_pin.direct == 0 && g_pin.split == 0 && g_pin.fallback == 2 * NK + (NK - 1) &&
              g_pin.fail >= 1,
              "shifted: register-fail forces fallback end to end");
    } else if (split_on) {
        check(g_pin.fallback == 0 && g_pin.direct == 16 && g_pin.split == NK - 1,
              "shifted: split ON serves B, no fallback");
        check(g_pin.sub_direct > g_pin.sub_reg && g_pin.sub_direct > 0,
              "shifted: majority of sub-copies are direct (already registered)");
    } else {
        check(g_pin.split == 0 && g_pin.fallback == NK - 1 && g_pin.direct == 16,
              "shifted: split OFF refuses B, falls back to the ring");
    }

    munmap(src_map, SRC_BYTES);
    return checks_fail == 0 ? 0 : 1;
}

int main(int argc, char ** argv) {
    ggml_log_set(log_cb, nullptr);
    if (argc < 2) {
        fprintf(stderr, "usage: %s runtime|selftest|shifted ...\n", argv[0]);
        return 1;
    }
    if (!strcmp(argv[1], "runtime"))  return mode_runtime(argc - 1, argv + 1);
    if (!strcmp(argv[1], "selftest")) return mode_selftest();
    if (!strcmp(argv[1], "shifted"))  return mode_shifted();
    fprintf(stderr, "unknown mode %s\n", argv[1]);
    return 1;
}
