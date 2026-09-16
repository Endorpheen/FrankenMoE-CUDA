// EXP-2026-09-17-077 model-free gate for the idle-gated asynchronous pinned-source path.
//
// Drives the accepted pinned-ring backend interface directly, no model and no server:
//   runtime  - replay the real EXP-045/070 range list through set_tensor_async_pinned_ring,
//              read the device bytes back and hash them (EXP-074 hash-list format)
//   async    - repeat that list for several passes and iterations; with --wait-drain it waits for
//              the background pin worker to finish and checks the next pass is fully direct
//   idle     - drive a continuous enqueue stream, assert the worker issued no batch while the
//              stream was active, then drain and check the next pass is fully direct
//   selftest - synthetic ranges with guards, padding and partial overlaps
//   shifted  - register range set A, then request a shifted set B that only partly overlaps A
//   adjacent - two page-adjacent registrations, then a request spanning their merged boundary
//
// The release counters are captured through the ggml log callback, so the binary reports the
// direct/split/fallback split, the worker counters and the teardown result without a model.
//
// Env:
//   GGML_EXPERT_PIN_ASYNC=0          restore the EXP-075 synchronous split (request-thread pin)
//   GGML_EXPERT_PIN_OVERLAP_SPLIT=0  restore the EXP-074 behaviour (refuse on any overlap)
//   GGML_EXPERT_PIN_REGISTER_FAIL=1  force every registration to fail (fallback end to end)
//   GGML_EXPERT_PIN_SOURCE=0         disable the pinned source (ring only)
//   GGML_EXPERT_PIN_TRACE=1          log the direct/ring class of every request
//   GGML_EXPERT_PIN_IDLE_MS          idle gate threshold, default 750
//   GGML_EXPERT_PIN_BATCH_MB         worker batch cap in MiB, default 4
//   GGML_EXPERT_PIN_BATCH_SLEEP_MS   sleep between worker batches, default 100

#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-backend-impl.h"
#include "ggml-cuda.h"

#include <algorithm>
#include <atomic>
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
    unsigned long long direct = 0, split = 0, fallback = 0, fail = 0, intervals = 0, regions = 0;
    unsigned long long bytes_released = 0, bytes_still = 0, sub_direct = 0, sub_reg = 0;
    unsigned long long queued = 0, worker_registered = 0, dropped = 0, dedup = 0, batches = 0;
    unsigned long long joined = 0, saturated = 0;
    unsigned long long idle_defer = 0, first_batch_ms = 0;
    bool seen = false;
};

static PinCounters g_pin;

// EXP-076 worker progress and per-request trace, used by the async mode
static std::atomic<long long> g_worker_processed{0};
static std::atomic<long long> g_worker_queued{0};
static std::atomic<long long> g_worker_depth{0};
static std::atomic<long long> g_worker_max_queued{0};
static std::atomic<bool>      g_worker_drained{false};
static std::atomic<long long> g_worker_batches_logged{0};
static std::atomic<long long> g_first_batch_delay_us{0};
static std::atomic<long long> g_trace_direct{0};
static std::atomic<long long> g_trace_ring{0};

static void log_cb(enum ggml_log_level level, const char * text, void * user_data) {
    (void) level; (void) user_data;
    fputs(text, stderr);

    const char * wp = strstr(text, "expert pin worker: processed");
    if (wp != nullptr) {
        unsigned long long processed = 0, queued = 0, depth = 0;
        if (sscanf(wp, "expert pin worker: processed %llu of %llu queued ranges, queue %llu",
                   &processed, &queued, &depth) == 3) {
            g_worker_processed.store((long long) processed);
            g_worker_queued.store((long long) queued);
            g_worker_depth.store((long long) depth);
            g_worker_batches_logged.fetch_add(1);
            if ((long long) queued > g_worker_max_queued.load()) {
                g_worker_max_queued.store((long long) queued);
            }
            if (depth == 0 && processed >= queued && queued > 0) {
                g_worker_drained.store(true);
            }
        }
        return;
    }
    const char * fp = strstr(text, "expert pin worker: first batch after");
    if (fp != nullptr) {
        double ms = 0.0;
        if (sscanf(fp, "expert pin worker: first batch after %lf ms", &ms) == 1) {
            g_first_batch_delay_us.store((long long) (ms * 1000.0));
        }
        return;
    }
    if (strstr(text, "expert pin trace: direct") != nullptr) {
        g_trace_direct.fetch_add(1);
        return;
    }
    if (strstr(text, "expert pin trace: ring") != nullptr) {
        g_trace_ring.fetch_add(1);
        return;
    }

    static const char * key = "expert pinned source:";
    const char * p = strstr(text, key);
    if (p == nullptr) {
        return;
    }
    PinCounters c;
    // EXP-077 log: idle-gate counters appended to the EXP-076 fields
    if (sscanf(p + strlen(key),
               " %llu direct ranges, %llu split ranges, %llu fallback ranges, %llu register failures,"
               " %llu intervals released, %llu physical regions released, %llu bytes released,"
               " %llu bytes still pinned, %llu covered sub-copies, %llu registered sub-copies,"
               " %llu queued ranges, %llu worker registered ranges, %llu dropped ranges,"
               " %llu dedup skipped ranges, %llu worker batches, %llu worker joined, %llu saturated,"
               " %llu idle deferrals, %llu first batch delay ms",
               &c.direct, &c.split, &c.fallback, &c.fail, &c.intervals, &c.regions,
               &c.bytes_released, &c.bytes_still, &c.sub_direct, &c.sub_reg,
               &c.queued, &c.worker_registered, &c.dropped, &c.dedup, &c.batches,
               &c.joined, &c.saturated, &c.idle_defer, &c.first_batch_ms) == 19) {
        c.seen = true;
        g_pin = c;
        return;
    }
    // EXP-076 log: worker counters appended to the EXP-075 fields
    if (sscanf(p + strlen(key),
               " %llu direct ranges, %llu split ranges, %llu fallback ranges, %llu register failures,"
               " %llu intervals released, %llu physical regions released, %llu bytes released,"
               " %llu bytes still pinned, %llu covered sub-copies, %llu registered sub-copies,"
               " %llu queued ranges, %llu worker registered ranges, %llu dropped ranges,"
               " %llu dedup skipped ranges, %llu worker batches, %llu worker joined, %llu saturated",
               &c.direct, &c.split, &c.fallback, &c.fail, &c.intervals, &c.regions,
               &c.bytes_released, &c.bytes_still, &c.sub_direct, &c.sub_reg,
               &c.queued, &c.worker_registered, &c.dropped, &c.dedup, &c.batches,
               &c.joined, &c.saturated) == 17) {
        c.seen = true;
        g_pin = c;
        return;
    }
    // EXP-075 log: separate physical registration count
    if (sscanf(p + strlen(key),
               " %llu direct ranges, %llu split ranges, %llu fallback ranges, %llu register failures,"
               " %llu intervals released, %llu physical regions released, %llu bytes released,"
               " %llu bytes still pinned, %llu covered sub-copies, %llu registered sub-copies",
               &c.direct, &c.split, &c.fallback, &c.fail, &c.intervals, &c.regions,
               &c.bytes_released, &c.bytes_still, &c.sub_direct, &c.sub_reg) == 10) {
        c.seen = true;
        g_pin = c;
        return;
    }
    // pre-fix log: merged intervals only, physical count fell back to the interval count
    if (sscanf(p + strlen(key),
               " %llu direct ranges, %llu split ranges, %llu fallback ranges, %llu register failures,"
               " %llu intervals released, %llu bytes released, %llu bytes still pinned,"
               " %llu covered sub-copies, %llu registered sub-copies",
               &c.direct, &c.split, &c.fallback, &c.fail, &c.intervals,
               &c.bytes_released, &c.bytes_still, &c.sub_direct, &c.sub_reg) == 9) {
        c.seen = true;
        c.regions = c.intervals;
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
// adjacent: two page-adjacent registrations merge logically; a request that spans
// the hidden physical boundary must be cut there, not copied across it
// ---------------------------------------------------------------------------
static int mode_adjacent(void) {
    const size_t ADJ   = 4 << 20;   // one registration per adjacent block
    const size_t SPAN  = 1 << 20;   // spanning request starts inside block 1
    const size_t SPAN_N = 6 << 20;  // [1 MiB, 7 MiB) crosses the shared boundary at 4 MiB
    const size_t GUARD = 4096;
    const size_t REGION = 16 << 20;
    const size_t arena_bytes = 2 * GUARD + REGION;

    void * src_map = mmap(nullptr, 2 * ADJ, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (src_map == MAP_FAILED) { fprintf(stderr, "anon mmap failed\n"); return 2; }
    uint8_t * src = (uint8_t *) src_map;
    for (size_t i = 0; i < 2 * ADJ; ++i) src[i] = (uint8_t) (i * 53 + (i >> 6) * 29 + 3);

    DeviceArena arena;
    if (!arena_init(arena, arena_bytes)) return 2;

    std::vector<uint8_t> zero(arena_bytes, 0xA5);
    ggml_backend_tensor_set(arena.tensor, zero.data(), 0, arena_bytes);

    const bool split_on = getenv("GGML_EXPERT_PIN_OVERLAP_SPLIT") == nullptr ||
        strcmp(getenv("GGML_EXPERT_PIN_OVERLAP_SPLIT"), "0") != 0;

    std::vector<uint8_t> want(REGION, 0xA5);

    struct Req { size_t src_off; size_t dst_off; size_t size; const char * name; };
    std::vector<Req> reqs = {
        { 0,        0,        ADJ, "adjacent A1 (register)" },
        { ADJ,      ADJ,      ADJ, "adjacent A2 (register)" },
    };
    if (split_on) {
        reqs.push_back({ SPAN, 8 << 20, SPAN_N, "spanning boundary (clamped copies)" });
    } else {
        // the spanning request is fully inside the merged interval, and the EXP-074 whole-range
        // path would issue one copy across the physical end; that latent case is out of scope here
        printf("ADJACENT note: spanning request skipped with the split disabled (EXP-074 path)\n");
    }

    const long swap_before = proc_status_kib("VmSwap");
    bool all_ok = true;
    for (const Req & r : reqs) {
        const bool handled = call_ring(arena, src + r.src_off, GUARD + r.dst_off, r.size);
        ggml_backend_synchronize(arena.backend);
        if (!handled) { all_ok = false; printf("ADJACENT refused %s\n", r.name); }
        memcpy(want.data() + r.dst_off, src + r.src_off, r.size);
    }

    std::vector<uint8_t> got(REGION);
    ggml_backend_tensor_get(arena.tensor, got.data(), GUARD, REGION);
    check(all_ok && got == want, "adjacent: device image byte-exact over the adjacent blocks");

    std::vector<uint8_t> g(2 * GUARD, 0xA5);
    ggml_backend_tensor_get(arena.tensor, g.data(), 0, GUARD);
    ggml_backend_tensor_get(arena.tensor, g.data() + GUARD, GUARD + REGION, GUARD);
    check(memcmp(g.data(), std::vector<uint8_t>(2 * GUARD, 0xA5).data(), 2 * GUARD) == 0, "adjacent: sentinel guards unchanged");

    arena_free(arena);
    const long swap_free = proc_status_kib("VmSwap");

    printf("{\"mode\":\"adjacent\",\"split_env\":%s,\"counters_seen\":%s,\"direct\":%llu,\"split\":%llu,"
           "\"fallback\":%llu,\"fail\":%llu,\"intervals\":%llu,\"regions\":%llu,\"bytes_released\":%llu,"
           "\"bytes_still_pinned\":%llu,\"sub_direct\":%llu,\"sub_reg\":%llu,"
           "\"swap_before_kib\":%ld,\"swap_after_free_kib\":%ld}\n",
           split_on ? "true" : "false", g_pin.seen ? "true" : "false",
           g_pin.direct, g_pin.split, g_pin.fallback, g_pin.fail, g_pin.intervals, g_pin.regions,
           g_pin.bytes_released, g_pin.bytes_still, g_pin.sub_direct, g_pin.sub_reg,
           swap_before, swap_free);
    fflush(stdout);

    check(g_pin.seen, "adjacent: release counters captured");
    check(swap_before == 0 && swap_free == 0, "adjacent: VmSwap stays 0");
    if (split_on) {
        check(g_pin.fallback == 0 && g_pin.direct == 3 && g_pin.split == 0,
              "adjacent: spanning request served, no fallback");
        check(g_pin.regions == 2 && g_pin.intervals == 1 && g_pin.sub_direct == 2,
              "adjacent: two physical regions, copies clamped at the shared boundary");
        check(g_pin.bytes_still == 0, "adjacent: teardown leaves no registered byte");
    } else {
        check(g_pin.regions == 2 && g_pin.bytes_still == 0,
              "adjacent: two physical regions released");
    }

    munmap(src_map, 2 * ADJ);
    return checks_fail == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------
// shifted: register set A, then request a shifted set B that partly overlaps A
// ---------------------------------------------------------------------------
static int mode_shifted(void) {
    const size_t E    = 4 << 20;   // expert spacing
    const size_t BLK  = 3 << 20;   // registered block per expert
    const size_t NK   = 8;         // experts in set A
    const size_t ADJ  = 4 << 20;   // page-adjacent registration block
    const size_t TAIL = NK * E;    // first page-adjacent block
    const size_t GUARD = 4096;
    const size_t REGION = 96 << 20;
    const size_t SRC_BYTES = NK * E + 3 * E;
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
    // set A adjacent: two page-adjacent registrations that merge into one logical interval
    reqs.push_back({ src, TAIL, TAIL, ADJ, "A adjacent 1" });
    reqs.push_back({ src, TAIL + ADJ, TAIL + ADJ, ADJ, "A adjacent 2" });
    // set B fully contained: inside each registered expert
    for (size_t k = 0; k < NK; ++k) {
        reqs.push_back({ src, k * E + BLK / 2, 32 * (1 << 20) + k * (1 << 20), 1 << 20, "B contained" });
    }
    // set B shifted: starts inside an expert, crosses its end, ends inside the next expert
    for (size_t k = 0; k + 1 < NK; ++k) {
        reqs.push_back({ src, k * E + 2 * (1 << 20), 40 * (1 << 20) + k * BLK, BLK, "B shifted" });
    }
    // set B spanning: crosses the hidden boundary shared by the two adjacent registrations
    reqs.push_back({ src, TAIL + (1 << 20), 64 * (1 << 20), 8 << 20, "B spanning" });

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
           "\"fallback\":%llu,\"fail\":%llu,\"intervals\":%llu,\"regions\":%llu,\"bytes_released\":%llu,"
           "\"bytes_still_pinned\":%llu,\"sub_direct\":%llu,\"sub_reg\":%llu,"
           "\"swap_before_kib\":%ld,\"swap_during_kib\":%ld,\"swap_after_free_kib\":%ld,\"va_hwm_kib\":%ld}\n",
           split_on ? "true" : "false", regfail ? "true" : "false", g_pin.seen ? "true" : "false",
           g_pin.direct, g_pin.split, g_pin.fallback, g_pin.fail, g_pin.intervals, g_pin.regions,
           g_pin.bytes_released, g_pin.bytes_still, g_pin.sub_direct, g_pin.sub_reg,
           swap_before, swap_during, swap_free, hwm);
    fflush(stdout);

    check(g_pin.seen, "shifted: release counters captured");
    check(swap_during == 0 && swap_free == 0, "shifted: VmSwap stays 0");
    check(g_pin.bytes_still == 0, "shifted: teardown leaves no registered byte");
    if (regfail) {
        // the first attempt per A interval fails and blacklists it; later requests overlap the
        // blacklist and fall back without a new register attempt
        check(g_pin.direct == 0 && g_pin.split == 0 && g_pin.fallback == 3 * NK + 2 &&
              g_pin.fail >= 1,
              "shifted: register-fail forces fallback end to end");
    } else if (split_on) {
        check(g_pin.fallback == 0 && g_pin.direct == 2 * NK + 2 && g_pin.split == NK,
              "shifted: split ON serves B, no fallback");
        check(g_pin.sub_direct == NK + 2 * (NK - 1) + 2 && g_pin.sub_reg == 2 * NK + 2 &&
              g_pin.regions == 2 * NK + 2,
              "shifted: clamped copies at every physical boundary");
    } else {
        check(g_pin.split == 0 && g_pin.fallback == NK && g_pin.direct == 2 * NK + 2,
              "shifted: split OFF refuses B, falls back to the ring");
    }

    munmap(src_map, SRC_BYTES);
    return checks_fail == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------
// async: run the real range list for several passes and iterations. With --wait-drain, the pass
// after the first waits for the background pin worker to empty its queue and must then be fully
// direct. Byte-exactness is verified by comparing the hash lists of the passes.
// ---------------------------------------------------------------------------
static bool run_pass(DeviceArena & arena, const std::vector<Range> & ranges,
                     const std::vector<ShardMap> & maps, size_t arena_bytes,
                     std::vector<uint64_t> & hashes, uint64_t & refused, uint64_t & total_bytes) {
    hashes.clear();
    hashes.reserve(ranges.size());
    refused = 0;
    total_bytes = 0;
    uint64_t cursor = 0;
    std::vector<uint8_t> readback(16 << 20);
    for (const Range & r : ranges) {
        if ((size_t) r.shard >= maps.size() || r.addr + r.bytes > maps[r.shard].size) {
            fprintf(stderr, "FAIL: range out of bounds\n");
            return false;
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
    return true;
}

static bool wait_worker_drain(int timeout_ms) {
    const int64_t t0 = ggml_time_us();
    while (ggml_time_us() - t0 < (int64_t) timeout_ms * 1000) {
        if (g_worker_drained.load() &&
            g_worker_depth.load() == 0 &&
            g_worker_queued.load() > 0 &&
            g_worker_queued.load() == g_worker_max_queued.load() &&
            g_worker_processed.load() >= g_worker_queued.load()) {
            return true;
        }
        usleep(2000);
    }
    return false;
}

static int mode_async(int argc, char ** argv) {
    const char * ranges_path = nullptr;
    const char * out_prefix  = nullptr;
    size_t arena_mib = 64;
    long long max_ranges = 0;
    int iterations = 1;
    int passes = 2;
    bool use_wait_drain = false;
    bool trace = false;
    bool direct_assert = true;
    long long drain_timeout_ms = 120000;
    std::vector<std::string> shards;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--ranges" && i + 1 < argc) ranges_path = argv[++i];
        else if (a == "--out" && i + 1 < argc) out_prefix = argv[++i];
        else if (a == "--shard" && i + 1 < argc) shards.push_back(argv[++i]);
        else if (a == "--arena" && i + 1 < argc) arena_mib = (size_t) atoll(argv[++i]);
        else if (a == "--max-ranges" && i + 1 < argc) max_ranges = atoll(argv[++i]);
        else if (a == "--iterations" && i + 1 < argc) iterations = atoi(argv[++i]);
        else if (a == "--passes" && i + 1 < argc) passes = atoi(argv[++i]);
        else if (a == "--wait-drain") use_wait_drain = true;
        else if (a == "--drain-timeout-ms" && i + 1 < argc) drain_timeout_ms = atoll(argv[++i]);
        else if (a == "--trace") trace = true;
        else if (a == "--no-direct-assert") direct_assert = false;
        else { fprintf(stderr, "unknown arg %s\n", a.c_str()); return 2; }
    }
    if (!ranges_path || !out_prefix || shards.empty() || iterations < 1 || passes < 1) {
        fprintf(stderr, "usage: %s async --ranges FILE --out PREFIX --shard PATH [...] "
                        "[--iterations N] [--passes N] [--wait-drain] [--trace]\n", argv[0]);
        return 2;
    }
    if (trace) {
        setenv("GGML_EXPERT_PIN_TRACE", "1", 1);
    }

    std::vector<Range> ranges;
    if (!load_ranges(ranges_path, ranges)) return 2;
    if (max_ranges > 0 && (long long) ranges.size() > max_ranges) ranges.resize((size_t) max_ranges);
    std::vector<ShardMap> maps;
    if (!map_shards(shards, maps)) return 2;

    const size_t arena_bytes = arena_mib << 20;
    const size_t n_ranges = ranges.size();

    for (int it = 0; it < iterations; ++it) {
        DeviceArena arena;
        if (!arena_init(arena, arena_bytes)) return 2;
        std::vector<uint8_t> zero(arena_bytes, 0xA5);
        ggml_backend_tensor_set(arena.tensor, zero.data(), 0, arena_bytes);

        g_worker_processed.store(0);
        g_worker_queued.store(0);
        g_worker_depth.store(0);
        g_worker_max_queued.store(0);
        g_worker_drained.store(false);

        const long pin_before = proc_status_kib("VmPin");
        const long swap_before = proc_status_kib("VmSwap");

        std::vector<std::vector<uint64_t>> pass_hashes;
        std::vector<uint64_t> pass_refused((size_t) passes, 0), pass_bytes((size_t) passes, 0);

        for (int p = 0; p < passes; ++p) {
            if (p == 1 && use_wait_drain) {
                const bool drained = wait_worker_drain((int) drain_timeout_ms);
                check(drained, "async: background worker reported its queue drained");
                g_trace_direct.store(0);
                g_trace_ring.store(0);
            }
            std::vector<uint64_t> hashes;
            if (!run_pass(arena, ranges, maps, arena_bytes, hashes, pass_refused[(size_t) p], pass_bytes[(size_t) p])) {
                arena_free(arena);
                return 2;
            }
            pass_hashes.push_back(std::move(hashes));

            if (trace) {
                const long long d = g_trace_direct.load();
                const long long rr = g_trace_ring.load();
                printf("{\"mode\":\"async-pass\",\"iter\":%d,\"pass\":%d,\"ranges\":%zu,"
                       "\"trace_direct\":%lld,\"trace_ring\":%lld}\n", it + 1, p + 1, n_ranges, d, rr);
                fflush(stdout);
                if (p > 0 && direct_assert) {
                    check(d == (long long) n_ranges && rr == 0,
                          "async: post-drain pass served every range directly");
                }
                g_trace_direct.store(0);
                g_trace_ring.store(0);
            }
        }

        if (passes >= 2) {
            check(pass_hashes[0] == pass_hashes[1], "async: pass 1 and pass 2 hash lists are identical");
        }

        for (int p = 0; p < passes; ++p) {
            char path[1024];
            snprintf(path, sizeof(path), "%s-it%d-p%d.txt", out_prefix, it + 1, p + 1);
            FILE * f = fopen(path, "w");
            if (!f) { fprintf(stderr, "cannot write %s\n", path); arena_free(arena); return 2; }
            fprintf(f, "# seq fnv1a64\n");
            for (size_t i = 0; i < pass_hashes[(size_t) p].size(); ++i) {
                fprintf(f, "%llu %llu\n", (unsigned long long) (i + 1),
                        (unsigned long long) pass_hashes[(size_t) p][i]);
            }
            fclose(f);
        }

        const long swap_during = proc_status_kib("VmSwap");
        arena_free(arena);
        const long pin_free = proc_status_kib("VmPin");
        const long swap_free = proc_status_kib("VmSwap");

        check(g_pin.seen, "async: release counters captured");
        check(g_pin.bytes_still == 0, "async: teardown leaves no registered byte");
        check(pin_free >= 0 && (pin_before < 0 || pin_free <= pin_before + 1024),
              "async: pinned bytes released on teardown");
        check(swap_free == 0, "async: VmSwap stays 0");

        printf("{\"mode\":\"async-iter\",\"iter\":%d,\"ranges\":%zu,\"iterations\":%d,\"passes\":%d,"
               "\"wait_drain\":%s,\"pass_refused\":%llu,\"pin_before_kib\":%ld,\"pin_after_free_kib\":%ld,"
               "\"swap_during_kib\":%ld,\"swap_after_free_kib\":%ld,\"counters_seen\":%s,"
               "\"direct\":%llu,\"split\":%llu,\"fallback\":%llu,\"fail\":%llu,\"intervals\":%llu,"
               "\"regions\":%llu,\"bytes_released\":%llu,\"bytes_still_pinned\":%llu,"
               "\"sub_direct\":%llu,\"sub_reg\":%llu,\"queued\":%llu,"
               "\"worker_registered\":%llu,\"dropped\":%llu,\"dedup\":%llu,\"batches\":%llu,"
               "\"joined\":%llu,\"saturated\":%llu}\n",
               it + 1, n_ranges, iterations, passes, use_wait_drain ? "true" : "false",
               (unsigned long long) pass_refused[(size_t) passes - 1], pin_before, pin_free,
               swap_during, swap_free, g_pin.seen ? "true" : "false",
               g_pin.direct, g_pin.split, g_pin.fallback, g_pin.fail, g_pin.intervals,
               g_pin.regions, g_pin.bytes_released, g_pin.bytes_still,
               g_pin.sub_direct, g_pin.sub_reg, g_pin.queued,
               g_pin.worker_registered, g_pin.dropped, g_pin.dedup, g_pin.batches,
               g_pin.joined, g_pin.saturated);
        fflush(stdout);
    }

    for (ShardMap & sm : maps) munmap(sm.base, (size_t) sm.size);
    return checks_fail == 0 ? 0 : 1;
}

// ---------------------------------------------------------------------------
// idle: drive a continuous enqueue stream and prove the pin worker opens its first batch only
// after the request thread has been quiet for GGML_EXPERT_PIN_IDLE_MS. Then wait for the drain,
// serve the same list again (must be fully direct) and compare the hash lists.
// ---------------------------------------------------------------------------
static int mode_idle(int argc, char ** argv) {
    const char * ranges_path = nullptr;
    const char * out_prefix  = nullptr;
    size_t arena_mib = 64;
    long long max_ranges = 1200;
    long long drain_timeout_ms = 900000;
    std::vector<std::string> shards;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--ranges" && i + 1 < argc) ranges_path = argv[++i];
        else if (a == "--out" && i + 1 < argc) out_prefix = argv[++i];
        else if (a == "--shard" && i + 1 < argc) shards.push_back(argv[++i]);
        else if (a == "--arena" && i + 1 < argc) arena_mib = (size_t) atoll(argv[++i]);
        else if (a == "--max-ranges" && i + 1 < argc) max_ranges = atoll(argv[++i]);
        else if (a == "--drain-timeout-ms" && i + 1 < argc) drain_timeout_ms = atoll(argv[++i]);
        else { fprintf(stderr, "unknown arg %s\n", a.c_str()); return 2; }
    }
    if (!ranges_path || !out_prefix || shards.empty() || max_ranges < 1) {
        fprintf(stderr, "usage: %s idle --ranges FILE --out PREFIX --shard PATH [...] "
                        "[--max-ranges N] [--drain-timeout-ms N]\n", argv[0]);
        return 2;
    }

    setenv("GGML_EXPERT_PIN_TRACE", "1", 1);
    const char * idle_env = getenv("GGML_EXPERT_PIN_IDLE_MS");
    const unsigned idle_ms = idle_env != nullptr ? (unsigned) atoll(idle_env) : 750;

    std::vector<Range> ranges;
    if (!load_ranges(ranges_path, ranges)) return 2;
    if ((long long) ranges.size() > max_ranges) ranges.resize((size_t) max_ranges);
    std::vector<ShardMap> maps;
    if (!map_shards(shards, maps)) return 2;

    const size_t arena_bytes = arena_mib << 20;
    DeviceArena arena;
    if (!arena_init(arena, arena_bytes)) return 2;
    std::vector<uint8_t> zero(arena_bytes, 0xA5);
    ggml_backend_tensor_set(arena.tensor, zero.data(), 0, arena_bytes);

    g_worker_processed.store(0);
    g_worker_queued.store(0);
    g_worker_depth.store(0);
    g_worker_max_queued.store(0);
    g_worker_drained.store(false);
    g_worker_batches_logged.store(0);
    g_first_batch_delay_us.store(0);

    std::vector<uint64_t> hashes;
    hashes.reserve(ranges.size());
    uint64_t cursor = 0;
    std::vector<uint8_t> readback(16 << 20);

    // continuous stream: enqueue every range back to back, no pause longer than the idle threshold
    const int64_t stream_start_us = ggml_time_us();
    int64_t last_enqueue_us = stream_start_us;
    double max_gap_ms = 0.0;
    for (const Range & r : ranges) {
        const uint8_t * src = (const uint8_t *) maps[r.shard].base + r.addr;
        uint64_t off = cursor;
        if (off + r.bytes > arena_bytes) off = 0;
        cursor = off + r.bytes;
        if (r.bytes > readback.size()) readback.resize(r.bytes);

        const bool handled = call_ring(arena, src, (size_t) off, (size_t) r.bytes);
        ggml_backend_synchronize(arena.backend);
        if (!handled) {
            ggml_backend_tensor_set_async(arena.backend, arena.tensor, src, (size_t) off, (size_t) r.bytes);
            ggml_backend_synchronize(arena.backend);
        }
        ggml_backend_tensor_get(arena.tensor, readback.data(), (size_t) off, (size_t) r.bytes);
        hashes.push_back(fnv1a64(readback.data(), (size_t) r.bytes));

        const int64_t now = ggml_time_us();
        const double gap_ms = (now - last_enqueue_us) / 1000.0;
        if (gap_ms > max_gap_ms) max_gap_ms = gap_ms;
        last_enqueue_us = now;
    }
    const int64_t stream_end_us = ggml_time_us();
    const double stream_ms = (stream_end_us - stream_start_us) / 1000.0;

    check(max_gap_ms < (double) idle_ms, "idle: enqueue stream stayed continuous (every gap < idle threshold)");
    const long long batches_during = g_worker_batches_logged.load();
    check(batches_during == 0, "idle: worker issued no batch while the request stream was active");

    const bool drained = wait_worker_drain((int) drain_timeout_ms);
    check(drained, "idle: worker drained the queue after the stream went quiet");

    const long long batches_after = g_worker_batches_logged.load();
    const double first_batch_ms = g_first_batch_delay_us.load() / 1000.0;
    check(batches_after > 0, "idle: worker issued batches after the idle gate opened");
    check(first_batch_ms >= (double) idle_ms, "idle: first batch waited at least the idle threshold");
    check(first_batch_ms >= stream_ms + (double) idle_ms - 250.0,
          "idle: first batch opened only after the stream plus the idle window");

    // second pass: everything is pinned now, so every range must take the direct path
    g_trace_direct.store(0);
    g_trace_ring.store(0);
    std::vector<uint64_t> hashes2;
    hashes2.reserve(ranges.size());
    cursor = 0;
    for (const Range & r : ranges) {
        const uint8_t * src = (const uint8_t *) maps[r.shard].base + r.addr;
        uint64_t off = cursor;
        if (off + r.bytes > arena_bytes) off = 0;
        cursor = off + r.bytes;
        if (r.bytes > readback.size()) readback.resize(r.bytes);
        const bool handled = call_ring(arena, src, (size_t) off, (size_t) r.bytes);
        ggml_backend_synchronize(arena.backend);
        if (!handled) {
            ggml_backend_tensor_set_async(arena.backend, arena.tensor, src, (size_t) off, (size_t) r.bytes);
            ggml_backend_synchronize(arena.backend);
        }
        ggml_backend_tensor_get(arena.tensor, readback.data(), (size_t) off, (size_t) r.bytes);
        hashes2.push_back(fnv1a64(readback.data(), (size_t) r.bytes));
    }
    check(g_trace_direct.load() == (long long) ranges.size() && g_trace_ring.load() == 0,
          "idle: post-drain pass served every range directly");
    check(hashes == hashes2, "idle: pass 1 and pass 2 hash lists are identical");

    char path[1024];
    snprintf(path, sizeof(path), "%s-p1.txt", out_prefix);
    FILE * f1 = fopen(path, "w");
    snprintf(path, sizeof(path), "%s-p2.txt", out_prefix);
    FILE * f2 = fopen(path, "w");
    if (!f1 || !f2) { fprintf(stderr, "cannot write hash lists\n"); if (f1) fclose(f1); if (f2) fclose(f2); arena_free(arena); return 2; }
    fprintf(f1, "# seq fnv1a64\n");
    fprintf(f2, "# seq fnv1a64\n");
    for (size_t i = 0; i < hashes.size(); ++i) {
        fprintf(f1, "%llu %llu\n", (unsigned long long) (i + 1), (unsigned long long) hashes[i]);
        fprintf(f2, "%llu %llu\n", (unsigned long long) (i + 1), (unsigned long long) hashes2[i]);
    }
    fclose(f1);
    fclose(f2);

    printf("{\"mode\":\"idle\",\"ranges\":%zu,\"idle_ms\":%u,\"stream_ms\":%.1f,\"max_gap_ms\":%.1f,"
           "\"batches_during_stream\":%lld,\"batches_after\":%lld,\"first_batch_ms\":%.1f}\n",
           ranges.size(), idle_ms, stream_ms, max_gap_ms, batches_during, batches_after, first_batch_ms);
    fflush(stdout);

    const long swap_during = proc_status_kib("VmSwap");
    arena_free(arena);
    const long swap_free = proc_status_kib("VmSwap");

    check(g_pin.seen, "idle: release counters captured");
    check(g_pin.bytes_still == 0, "idle: teardown leaves no registered byte");
    check(g_pin.joined == 1, "idle: worker joined before unregister");
    check(g_pin.idle_defer >= 1, "idle: at least one idle-gate deferral was counted");
    check(swap_during == 0 && swap_free == 0, "idle: VmSwap stays 0");

    printf("{\"mode\":\"idle-teardown\",\"counters_seen\":%s,\"direct\":%llu,\"fallback\":%llu,"
           "\"intervals\":%llu,\"regions\":%llu,\"bytes_released\":%llu,\"bytes_still_pinned\":%llu,"
           "\"queued\":%llu,\"worker_registered\":%llu,\"batches\":%llu,\"idle_deferrals\":%llu,"
           "\"release_first_batch_ms\":%llu,\"joined\":%llu,\"saturated\":%llu,"
           "\"swap_during_kib\":%ld,\"swap_after_free_kib\":%ld}\n",
           g_pin.seen ? "true" : "false", g_pin.direct, g_pin.fallback, g_pin.intervals, g_pin.regions,
           g_pin.bytes_released, g_pin.bytes_still, g_pin.queued, g_pin.worker_registered, g_pin.batches,
           g_pin.idle_defer, g_pin.first_batch_ms, g_pin.joined, g_pin.saturated, swap_during, swap_free);
    fflush(stdout);

    for (ShardMap & sm : maps) munmap(sm.base, (size_t) sm.size);
    return checks_fail == 0 ? 0 : 1;
}

int main(int argc, char ** argv) {
    ggml_log_set(log_cb, nullptr);
    if (argc < 2) {
        fprintf(stderr, "usage: %s runtime|async|idle|selftest|shifted|adjacent ...\n", argv[0]);
        return 1;
    }
    if (!strcmp(argv[1], "runtime"))  return mode_runtime(argc - 1, argv + 1);
    if (!strcmp(argv[1], "async"))    return mode_async(argc - 1, argv + 1);
    if (!strcmp(argv[1], "idle"))     return mode_idle(argc - 1, argv + 1);
    if (!strcmp(argv[1], "selftest")) return mode_selftest();
    if (!strcmp(argv[1], "shifted"))  return mode_shifted();
    if (!strcmp(argv[1], "adjacent")) return mode_adjacent();
    fprintf(stderr, "unknown mode %s\n", argv[1]);
    return 1;
}
