// EXP-2026-09-16-070: model-free comparison of the prefill expert-transfer path.
//
// Arm A (current): two-thread CPU gather into N pinned 16 MiB slots, H2D on one stream.
// Arm B:           the source ranges are pinned once (cuMemHostRegister) and copied directly
//                  to the device with no CPU gather and no staging slot.
// Arm C:           the current gather+slot scheme, but the device copies are issued on a
//                  dedicated copy stream; the compute stream waits on the copy event.
//
// Workload and metric are those of EXP-2026-09-08-045: the real 17 886-range / 21.641 GiB list
// replayed into a wrapping device arena; the measured quantity is the full gather + H2D sequence
// with a final drain.
//
// The local CUDA 12.6 toolkit cannot compile any .cu file against glibc 2.43, so this harness is
// written against the CUDA Driver API and built with a plain host compiler. No device code is
// needed for a memcpy-only workload.
//
// Usage:
//   exp070_transfer3 --selftest
//   exp070_transfer3 --ranges FILE --shard PATH [--shard PATH ...] --mode A|B|C
//                    [--slots N] [--two-thread 0|1] [--pass LABEL] [--warmup N] [--passes N]
//                    [--hash FILE] [--arena MiB] [--max-range-bytes N]
//   exp070_transfer3 --ranges FILE --shard PATH ... --pin-probe [--max-range-bytes N]

#include <cuda.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static constexpr uint64_t PAGE          = 4096;
static constexpr size_t   RING_SLOT_SIZE = 16ull * 1024 * 1024;
static constexpr size_t   CACHE_LINE     = 64;
static constexpr int      N_BUCKETS       = 4;

static const char * cu_err(CUresult e) {
    const char * n = nullptr;
    cuGetErrorName(e, &n);
    return n ? n : "?";
}

static bool cu_ok(CUresult e, const char * where) {
    if (e == CUDA_SUCCESS) return true;
    fprintf(stderr, "CUDA error at %s: %s\n", where, cu_err(e));
    return false;
}

// ---------------------------------------------------------------------------
// time and memory
// ---------------------------------------------------------------------------
static uint64_t now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000000000ull + (uint64_t) ts.tv_nsec;
}

static uint64_t thread_cpu_ns() {
    struct timespec ts;
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0) return 0;
    return (uint64_t) ts.tv_sec * 1000000000ull + (uint64_t) ts.tv_nsec;
}

struct ProcStat {
    uint64_t cpu_ns = 0;
    long     rss_kb    = 0;
    long     vmhwm_kb  = 0;
    long     vmswap_kb = 0;
};

static ProcStat proc_stat() {
    ProcStat s;
    FILE * f = fopen("/proc/self/stat", "r");
    if (f) {
        char line[2048];
        if (fgets(line, sizeof(line), f)) {
            char * p = strrchr(line, ')');
            if (p) {
                unsigned long ut = 0, st = 0;
                if (sscanf(p, ") %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu",
                           &ut, &st) == 2) {
                    const long hz = sysconf(_SC_CLK_TCK);
                    s.cpu_ns = (uint64_t) (ut + st) * 1000000000ull / (uint64_t) hz;
                }
            }
        }
        fclose(f);
    }
    f = fopen("/proc/self/status", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (sscanf(line, "VmRSS: %ld kB", &s.rss_kb) == 1)     continue;
            if (sscanf(line, "VmHWM: %ld kB", &s.vmhwm_kb) == 1)   continue;
            if (sscanf(line, "VmSwap: %ld kB", &s.vmswap_kb) == 1) continue;
        }
        fclose(f);
    }
    return s;
}

// ---------------------------------------------------------------------------
// persistent helper thread (unchanged EXP-045 protocol)
// ---------------------------------------------------------------------------
struct GatherJob {
    std::atomic<uint64_t> seq{0};
    std::atomic<uint64_t> done{0};
    std::atomic<bool>     quit{false};
    std::atomic<uint64_t> cpu_ns{0};
    const uint8_t * src  = nullptr;
    uint8_t *       dst  = nullptr;
    size_t          head = 0;
    size_t          tail = 0;
};

static void helper_run(GatherJob * job) {
    uint64_t served = 0;
    while (!job->quit.load(std::memory_order_acquire)) {
        if (job->seq.load(std::memory_order_acquire) == served) {
            bool got = false;
            for (int spin = 0; spin < 8192; ++spin) {
                if (job->quit.load(std::memory_order_acquire)) return;
                if (job->seq.load(std::memory_order_acquire) != served) { got = true; break; }
#if defined(__x86_64__)
                __builtin_ia32_pause();
#endif
            }
            if (!got) {
                sched_yield();
                continue;
            }
        }
        const uint64_t c0 = thread_cpu_ns();
        if (job->tail > 0) {
            memcpy(job->dst + job->head, job->src + job->head, job->tail);
        }
        served = job->seq.load(std::memory_order_relaxed);
        job->cpu_ns.fetch_add(thread_cpu_ns() - c0, std::memory_order_relaxed);
        job->done.store(served, std::memory_order_release);
    }
}

// ---------------------------------------------------------------------------
// workload
// ---------------------------------------------------------------------------
struct Range { int shard; uint64_t addr; uint64_t bytes; };

static bool load_ranges(const char * path, std::vector<Range> & out) {
    FILE * f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot open ranges file %s\n", path); return false; }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        Range r;
        unsigned long long a = 0, b = 0;
        if (sscanf(line, "%d %llu %llu", &r.shard, &a, &b) != 3) {
            fprintf(stderr, "bad range line: %s", line);
            fclose(f);
            return false;
        }
        r.addr = a; r.bytes = b;
        out.push_back(r);
    }
    fclose(f);
    return !out.empty();
}

struct ShardMap { void * base = nullptr; uint64_t size = 0; };

// MAP_SHARED file mappings cannot be registered with the driver, so the shards are mapped
// MAP_PRIVATE PROT_READ and the source is registered read-only.
static bool map_shards(const std::vector<std::string> & paths, std::vector<ShardMap> & out) {
    out.resize(paths.size());
    for (size_t i = 0; i < paths.size(); ++i) {
        int fd = open(paths[i].c_str(), O_RDONLY);
        if (fd < 0) { fprintf(stderr, "cannot open shard %s\n", paths[i].c_str()); return false; }
        struct stat st;
        if (fstat(fd, &st) != 0) { close(fd); return false; }
        void * p = mmap(nullptr, (size_t) st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
        if (p == MAP_FAILED) { fprintf(stderr, "mmap failed for %s\n", paths[i].c_str()); return false; }
        out[i].base = p;
        out[i].size = (uint64_t) st.st_size;
    }
    return true;
}

struct Interval { int shard; uint64_t start; uint64_t end; };

static void build_intervals(const std::vector<Range> & ranges, std::vector<Interval> & out) {
    for (int si = 0; si < 3; ++si) {
        std::vector<Interval> v;
        for (const Range & r : ranges) {
            if (r.shard != si) continue;
            uint64_t s = (r.addr / PAGE) * PAGE;
            uint64_t e = ((r.addr + r.bytes + PAGE - 1) / PAGE) * PAGE;
            v.push_back({ r.shard, s, e });
        }
        if (v.empty()) continue;
        std::sort(v.begin(), v.end(), [](const Interval & a, const Interval & b) {
            return a.start < b.start;
        });
        std::vector<Interval> merged;
        for (const Interval & iv : v) {
            if (!merged.empty() && iv.start <= merged.back().end) {
                if (iv.end > merged.back().end) merged.back().end = iv.end;
            } else {
                merged.push_back(iv);
            }
        }
        for (const Interval & iv : merged) out.push_back(iv);
    }
}

// registers every page-aligned interval of the selected ranges once; returns per-range host
// pointers valid only after a successful registration
struct PinState {
    std::vector<Interval>          registered;
    std::vector<const uint8_t *>   range_ptr;
    uint64_t                       bytes  = 0;
    size_t                         failed = 0;
};

static bool pin_ranges(const std::vector<Range> & ranges, const std::vector<ShardMap> & maps,
                       PinState & pin) {
    std::vector<Interval> ivs;
    build_intervals(ranges, ivs);
    pin.range_ptr.assign(ranges.size(), nullptr);

    for (const Interval & iv : ivs) {
        void * p = (uint8_t *) maps[iv.shard].base + iv.start;
        size_t n = (size_t) (iv.end - iv.start);
        CUresult e = cuMemHostRegister(p, n, CU_MEMHOSTREGISTER_READ_ONLY);
        if (e != CUDA_SUCCESS) {
            pin.failed++;
            fprintf(stderr, "PINFAIL shard=%d offset=%llu size=%zu err=%s\n",
                    iv.shard, (unsigned long long) iv.start, n, cu_err(e));
            return false;
        }
        pin.registered.push_back(iv);
        pin.bytes += n;
    }

    // intervals were grouped per shard, so the global order by start does not hold across shards
    std::sort(pin.registered.begin(), pin.registered.end(), [](const Interval & a, const Interval & b) {
        return a.shard != b.shard ? a.shard < b.shard : a.start < b.start;
    });

    for (size_t ri = 0; ri < ranges.size(); ++ri) {
        const Range & r = ranges[ri];
        const auto it = std::lower_bound(pin.registered.begin(), pin.registered.end(), r,
            [](const Interval & iv, const Range & r2) {
                return iv.shard != r2.shard ? iv.shard < r2.shard : iv.start < r2.addr;
            });
        const Interval * cover = nullptr;
        if (it != pin.registered.end() && it->shard == r.shard && it->start <= r.addr &&
            r.addr + r.bytes <= it->end) {
            cover = &(*it);
        } else if (it != pin.registered.begin()) {
            const Interval & prev = *(it - 1);
            if (prev.shard == r.shard && prev.start <= r.addr && r.addr + r.bytes <= prev.end) {
                cover = &prev;
            }
        }
        if (!cover) { fprintf(stderr, "range %zu not covered by a registered interval\n", ri); return false; }
        pin.range_ptr[ri] = (const uint8_t *) maps[r.shard].base + r.addr;
    }
    return true;
}

static void unpin_all(PinState & pin, const std::vector<ShardMap> & maps) {
    for (const Interval & iv : pin.registered) {
        cuMemHostUnregister((uint8_t *) maps[iv.shard].base + iv.start);
    }
}

// ---------------------------------------------------------------------------
// ring / metrics
// ---------------------------------------------------------------------------
struct RingState {
    std::vector<void *>   slots;
    std::vector<CUevent>  events;
    std::vector<uint8_t>  pending;
    size_t                next = 0;
};

static const size_t  BUCKET_EDGES[] = { 600ull * 1024, 1200ull * 1024, 2560ull * 1024, SIZE_MAX };
static const char *  BUCKET_NAMES[] = { "lt600KiB", "600KiB_1p2MiB", "1p2MiB_2p5MiB", "ge2p5MiB" };

struct Bucket { uint64_t count = 0; uint64_t bytes = 0; };

struct RunMetrics {
    uint64_t wall_ns   = 0;
    uint64_t gather_ns = 0;
    uint64_t submit_ns = 0;
    uint64_t slotwait_ns = 0;
    uint64_t drain_ns  = 0;
    uint64_t span_ns   = 0;
    uint64_t calls     = 0;
    uint64_t chunks    = 0;
    uint64_t bytes     = 0;
    uint64_t caller_cpu_ns = 0;
    uint64_t gather_cpu_ns = 0;
    uint64_t helper_cpu_ns = 0;
    ProcStat proc;
    Bucket   buckets[N_BUCKETS];
};

struct ArmConfig {
    char   mode             = 'A';
    bool   two_thread       = true;
    bool   dedicated_copy   = false;
    size_t slots            = 3;
    size_t pin_regions      = 0;
    uint64_t pin_bytes      = 0;
    size_t pin_failed       = 0;
};

static inline size_t split_head(size_t len) {
    return (len / 2) / CACHE_LINE * CACHE_LINE;
}

static bool stage_range(RingState & ring, CUstream compute_stream, CUstream copy_stream,
                        uint8_t * dst_base, uint64_t arena_cap, uint64_t & dst_cursor,
                        const uint8_t * src, size_t nbytes, const ArmConfig & cfg,
                        GatherJob & job, RunMetrics & m,
                        std::vector<uint64_t> * h_out, uint64_t & seq, bool check_slot) {
    size_t remaining = nbytes;
    size_t done      = 0;
    m.calls++;

    while (remaining > 0) {
        const size_t chunk    = remaining < RING_SLOT_SIZE ? remaining : RING_SLOT_SIZE;
        const size_t slot_idx = ring.next;
        ring.next = (ring.next + 1) % cfg.slots;

        if (ring.pending[slot_idx]) {
            const uint64_t w0 = now_ns();
            if (!cu_ok(cuEventSynchronize(ring.events[slot_idx]), "eventSynchronize")) return false;
            m.slotwait_ns += now_ns() - w0;
            ring.pending[slot_idx] = 0;
        }

        uint8_t *       slot  = (uint8_t *) ring.slots[slot_idx];
        const uint8_t * src_p = src + done;

        const uint64_t g0  = now_ns();
        const uint64_t gc0 = thread_cpu_ns();
        if (cfg.two_thread) {
            const size_t head = split_head(chunk);
            job.src  = src_p;
            job.dst  = slot;
            job.head = head;
            job.tail = chunk - head;
            const uint64_t s = ++seq;
            job.seq.store(s, std::memory_order_release);
            memcpy(slot, src_p, head);
            while (job.done.load(std::memory_order_acquire) != s) {
#if defined(__x86_64__)
                __builtin_ia32_pause();
#endif
            }
        } else {
            memcpy(slot, src_p, chunk);
        }
        m.gather_cpu_ns += thread_cpu_ns() - gc0;
        m.gather_ns     += now_ns() - g0;

        if (check_slot && memcmp(slot, src_p, chunk) != 0) {
            fprintf(stderr, "FAIL: staged slot differs from source at call %llu\n",
                    (unsigned long long) m.calls);
            return false;
        }
        if (h_out) {
            uint64_t h = 1469598103934665603ull;
            for (size_t i = 0; i < chunk; ++i) { h ^= slot[i]; h *= 1099511628211ull; }
            h_out->push_back(h);
        }

        uint64_t off = dst_cursor;
        if (off + chunk > arena_cap) off = 0;
        dst_cursor = off + chunk;

        const uint64_t s0 = now_ns();
        if (!cu_ok(cuMemcpyHtoDAsync((CUdeviceptr) (dst_base + off), slot, chunk, copy_stream),
                   "memcpyHtoDAsync")) return false;
        if (!cu_ok(cuEventRecord(ring.events[slot_idx], copy_stream), "eventRecord")) return false;
        if (cfg.dedicated_copy) {
            if (!cu_ok(cuStreamWaitEvent(compute_stream, ring.events[slot_idx], 0), "streamWaitEvent")) return false;
        }
        m.submit_ns += now_ns() - s0;
        ring.pending[slot_idx] = 1;

        int b = 0;
        while (chunk >= BUCKET_EDGES[b]) b++;
        m.buckets[b].count++;
        m.buckets[b].bytes += chunk;

        m.chunks++;
        m.bytes  += chunk;
        done     += chunk;
        remaining -= chunk;
    }
    return true;
}

static bool stage_direct(CUstream stream, uint8_t * dst_base, uint64_t arena_cap,
                         uint64_t & dst_cursor, const uint8_t * src, size_t nbytes,
                         RunMetrics & m, std::vector<uint64_t> * h_out) {
    size_t remaining = nbytes;
    size_t done      = 0;
    m.calls++;

    while (remaining > 0) {
        const size_t chunk = remaining < RING_SLOT_SIZE ? remaining : RING_SLOT_SIZE;
        const uint8_t * src_p = src + done;

        if (h_out) {
            uint64_t h = 1469598103934665603ull;
            for (size_t i = 0; i < chunk; ++i) { h ^= src_p[i]; h *= 1099511628211ull; }
            h_out->push_back(h);
        }

        uint64_t off = dst_cursor;
        if (off + chunk > arena_cap) off = 0;
        dst_cursor = off + chunk;

        const uint64_t s0 = now_ns();
        if (!cu_ok(cuMemcpyHtoDAsync((CUdeviceptr) (dst_base + off), src_p, chunk, stream),
                   "memcpyHtoDAsync")) return false;
        m.submit_ns += now_ns() - s0;

        int b = 0;
        while (chunk >= BUCKET_EDGES[b]) b++;
        m.buckets[b].count++;
        m.buckets[b].bytes += chunk;

        m.chunks++;
        m.bytes  += chunk;
        done     += chunk;
        remaining -= chunk;
    }
    return true;
}

static bool perf_pass(const std::vector<Range> & ranges, const std::vector<ShardMap> & shards,
                      const PinState * pin, RingState & ring,
                      CUstream compute_stream, CUstream copy_stream,
                      CUevent span_start, CUevent span_end,
                      uint8_t * arena, uint64_t arena_cap,
                      GatherJob & job, const ArmConfig & cfg,
                      std::vector<uint64_t> * h_out, RunMetrics & m) {
    uint64_t seq        = 0;
    uint64_t dst_cursor = 0;

    const uint64_t cc0 = thread_cpu_ns();
    const uint64_t hc0 = job.cpu_ns.load(std::memory_order_relaxed);

    if (!cu_ok(cuEventRecord(span_start, copy_stream), "spanStart")) return false;

    const uint64_t t0 = now_ns();
    for (size_t ri = 0; ri < ranges.size(); ++ri) {
        const Range & r = ranges[ri];
        if ((size_t) r.shard >= shards.size()) {
            fprintf(stderr, "FAIL: bad shard index %d\n", r.shard);
            return false;
        }
        const ShardMap & sm = shards[r.shard];
        if (r.addr + r.bytes > sm.size) {
            fprintf(stderr, "FAIL: range outside shard %d\n", r.shard);
            return false;
        }
        const uint8_t * src = (const uint8_t *) sm.base + r.addr;
        if (cfg.mode == 'B') {
            if (pin) src = pin->range_ptr[ri];
            if (!stage_direct(copy_stream, arena, arena_cap, dst_cursor, src, r.bytes,
                              m, h_out)) return false;
        } else {
            if (!stage_range(ring, compute_stream, copy_stream, arena, arena_cap, dst_cursor,
                             src, r.bytes, cfg, job, m, h_out, seq, h_out != nullptr)) return false;
        }
    }
    if (!cu_ok(cuEventRecord(span_end, copy_stream), "spanEnd")) return false;
    const uint64_t d0 = now_ns();

    if (cfg.dedicated_copy) {
        if (!cu_ok(cuStreamSynchronize(compute_stream), "compute drain")) return false;
    }
    if (!cu_ok(cuStreamSynchronize(copy_stream), "copy drain")) return false;
    const uint64_t t1 = now_ns();

    float span_ms = 0.0f;
    if (!cu_ok(cuEventElapsedTime(&span_ms, span_start, span_end), "spanElapsed")) return false;

    m.drain_ns      = t1 - d0;
    m.wall_ns       = t1 - t0;
    m.span_ns       = (uint64_t) (span_ms * 1e6f);
    m.caller_cpu_ns = thread_cpu_ns() - cc0;
    m.helper_cpu_ns = job.cpu_ns.load(std::memory_order_relaxed) - hc0;
    m.proc          = proc_stat();
    return true;
}

static void print_metrics(const RunMetrics & m, const ArmConfig & cfg, const char * label) {
    const double wall_ms = m.wall_ns / 1e6;
    const double span_ms = m.span_ns / 1e6;
    const double bytes   = (double) m.bytes;
    printf("{\"mode\":\"%c\",\"label\":\"%s\",\"slots\":%zu,\"two_thread\":%s,\"dedicated_copy\":%s,"
           "\"wall_ms\":%.3f,\"gather_ms\":%.3f,\"submit_ms\":%.3f,\"slotwait_ms\":%.3f,"
           "\"drain_ms\":%.3f,\"span_ms\":%.3f,"
           "\"calls\":%llu,\"chunks\":%llu,\"bytes\":%llu,"
           "\"bw_wall_gib_s\":%.3f,\"bw_span_gib_s\":%.3f,"
           "\"caller_cpu_ms\":%.3f,\"gather_cpu_ms\":%.3f,\"helper_cpu_ms\":%.3f,"
           "\"proc_cpu_ms\":%.3f,\"rss_mib\":%.1f,\"vmhwm_mib\":%.1f,\"vmswap_kib\":%ld,"
           "\"pin_regions\":%zu,\"pin_bytes\":%llu,\"pin_failed\":%zu,\"buckets\":{",
           cfg.mode, label, cfg.slots, cfg.two_thread ? "true" : "false",
           cfg.dedicated_copy ? "true" : "false",
           wall_ms, m.gather_ns / 1e6, m.submit_ns / 1e6, m.slotwait_ns / 1e6,
           m.drain_ns / 1e6, span_ms,
           (unsigned long long) m.calls, (unsigned long long) m.chunks, (unsigned long long) m.bytes,
           bytes / 1073741824.0 / (wall_ms / 1e3), bytes / 1073741824.0 / (span_ms / 1e3),
           m.caller_cpu_ns / 1e6, m.gather_cpu_ns / 1e6, m.helper_cpu_ns / 1e6, m.proc.cpu_ns / 1e6,
           m.proc.rss_kb / 1024.0, m.proc.vmhwm_kb / 1024.0, m.proc.vmswap_kb,
           cfg.pin_regions, (unsigned long long) cfg.pin_bytes, cfg.pin_failed);
    for (int b = 0; b < N_BUCKETS; ++b) {
        printf("%s\"%s\":{\"count\":%llu,\"bytes\":%llu}",
               b ? "," : "", BUCKET_NAMES[b],
               (unsigned long long) m.buckets[b].count, (unsigned long long) m.buckets[b].bytes);
    }
    printf("}}\n");
    fflush(stdout);
}

// ---------------------------------------------------------------------------
// selftest
// ---------------------------------------------------------------------------
static int checks_pass = 0;
static int checks_fail = 0;

static void check(bool ok, const char * name) {
    printf("CHECK %s %s\n", ok ? "PASS" : "FAIL", name);
    if (ok) checks_pass++; else checks_fail++;
    fflush(stdout);
}

static uint64_t fnv(const uint8_t * p, size_t n) {
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}

static int selftest() {
    const size_t SRC_BYTES = 128ull * 1024 * 1024;
    const size_t DEV_BYTES = 128ull * 1024 * 1024;
    const size_t SLOTS     = 3;

    if (!cu_ok(cuInit(0), "init")) return 2;
    CUdevice dev;
    if (!cu_ok(cuDeviceGet(&dev, 0), "deviceGet")) return 2;
    CUcontext ctx;
    if (!cu_ok(cuDevicePrimaryCtxRetain(&ctx, dev), "primaryCtxRetain")) return 2;
    if (!cu_ok(cuCtxSetCurrent(ctx), "ctxSetCurrent")) return 2;

    uint8_t * src = (uint8_t *) mmap(nullptr, SRC_BYTES, PROT_READ | PROT_WRITE,
                                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (src == MAP_FAILED) { fprintf(stderr, "source mmap failed\n"); return 2; }
    for (size_t i = 0; i < SRC_BYTES; ++i) src[i] = (uint8_t) (i * 31 + (i >> 8) * 7 + 11);
    const uint64_t src_hash0 = fnv(src, SRC_BYTES);
    if (!cu_ok(cuMemHostRegister(src, SRC_BYTES, 0), "register selftest src")) return 2;

    CUstream stream, copy_stream;
    if (!cu_ok(cuStreamCreate(&stream, CU_STREAM_DEFAULT), "streamCreate")) return 2;
    if (!cu_ok(cuStreamCreate(&copy_stream, CU_STREAM_DEFAULT), "copyStreamCreate")) return 2;

    CUdeviceptr devp;
    if (!cu_ok(cuMemAlloc(&devp, DEV_BYTES), "dev alloc")) return 2;
    uint8_t * dst = (uint8_t *) (uintptr_t) devp;

    RingState ring;
    ring.slots.resize(SLOTS);
    ring.events.resize(SLOTS);
    ring.pending.assign(SLOTS, 0);
    for (size_t i = 0; i < SLOTS; ++i) {
        if (!cu_ok(cuMemHostAlloc(&ring.slots[i], RING_SLOT_SIZE, 0), "slot alloc")) return 2;
        if (!cu_ok(cuEventCreate(&ring.events[i], CU_EVENT_DISABLE_TIMING), "event create")) return 2;
    }
    CUevent span_start, span_end;
    if (!cu_ok(cuEventCreate(&span_start, CU_EVENT_DEFAULT), "span start")) return 2;
    if (!cu_ok(cuEventCreate(&span_end, CU_EVENT_DEFAULT), "span end")) return 2;

    struct Case { size_t off; size_t bytes; };
    const std::vector<Case> cases = {
        { 0, 1 }, { 3, 63 }, { 1, 64 }, { 1, 65 }, { 3, 127 },
        { 1, 524800 }, { 3, 524801 }, { 1, RING_SLOT_SIZE - 1 }, { 0, RING_SLOT_SIZE },
        { 1, RING_SLOT_SIZE + 1 }, { 3, RING_SLOT_SIZE + 65 }, { 1, 17ull << 20 },
        { 1, 33ull << 20 },
    };
    std::vector<size_t> dev_off(cases.size(), 0);
    size_t need = 0;
    for (size_t i = 0; i < cases.size(); ++i) { dev_off[i] = need; need += cases[i].bytes; }
    if (need > DEV_BYTES) { fprintf(stderr, "selftest cases do not fit\n"); return 2; }

    std::vector<uint64_t> hashes_ref;
    bool ok = true;

    for (int mode = 0; mode < 3 && ok; ++mode) {
        ArmConfig cfg;
        cfg.mode           = (mode == 0) ? 'A' : (mode == 1) ? 'B' : 'C';
        cfg.two_thread     = true;
        cfg.dedicated_copy = (mode == 2);
        cfg.slots          = SLOTS;

        GatherJob job;
        std::thread helper;
        if (cfg.mode != 'B') helper = std::thread(helper_run, &job);

        const bool trivially_pinned = (cfg.mode == 'B');
        (void) trivially_pinned;

        uint64_t seq = 0;
        std::vector<uint64_t> hashes;
        for (int rep = 0; rep < 3 && ok; ++rep) {
            for (size_t ci = 0; ci < cases.size() && ok; ++ci) {
                const Case & c = cases[ci];
                RunMetrics m;
                uint64_t cursor = dev_off[ci];
                if (cfg.mode == 'B') {
                    ok = stage_direct(stream, dst, DEV_BYTES, cursor, src + c.off, c.bytes, m, &hashes);
                } else {
                    ok = stage_range(ring, stream, copy_stream, dst, DEV_BYTES, cursor,
                                     src + c.off, c.bytes, cfg, job, m, &hashes, seq, true);
                }
            }
        }
        if (cfg.dedicated_copy) cuStreamSynchronize(stream);
        ok = ok && cu_ok(cuStreamSynchronize(copy_stream), "selftest drain");

        job.quit.store(true, std::memory_order_release);
        if (helper.joinable()) helper.join();

        if (mode == 0) {
            hashes_ref.swap(hashes);
            check(ok, "st3_mode_A_all_checks");
        } else if (mode == 1) {
            check(ok && hashes == hashes_ref, "st4_mode_B_matches_A");
        } else {
            check(ok && hashes == hashes_ref, "st5_mode_C_matches_A");
        }
    }

    check(fnv(src, SRC_BYTES) == src_hash0, "st6_source_unchanged");

    std::vector<uint8_t> back(need);
    if (cu_ok(cuMemcpyDtoH(back.data(), devp, need), "dtoh")) {
        bool dev_ok = true;
        for (size_t ci = 0; ci < cases.size() && dev_ok; ++ci) {
            if (memcmp(back.data() + dev_off[ci], src + cases[ci].off, cases[ci].bytes) != 0) dev_ok = false;
        }
        check(dev_ok, "st7_device_roundtrip_equals_source");
    } else {
        check(false, "st7_device_roundtrip_equals_source");
    }

    cuMemHostUnregister(src);
    munmap(src, SRC_BYTES);
    for (size_t i = 0; i < SLOTS; ++i) {
        cuEventDestroy(ring.events[i]);
        cuMemFreeHost(ring.slots[i]);
    }
    cuEventDestroy(span_start);
    cuEventDestroy(span_end);
    cuMemFree(devp);
    cuStreamDestroy(stream);
    cuStreamDestroy(copy_stream);
    cuDevicePrimaryCtxRelease(dev);

    printf("SELFTEST %s (%d pass, %d fail)\n", checks_fail ? "FAIL" : "PASS",
           checks_pass, checks_fail);
    return checks_fail ? 1 : 0;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char ** argv) {
    const char * ranges_path = nullptr;
    const char * mode_str    = nullptr;
    const char * label       = "run";
    const char * hash_path   = nullptr;
    int    warmup    = 1;
    int    passes    = 4;
    size_t arena_mib = 64;
    size_t slots     = 3;
    bool   two_thread = true;
    uint64_t max_range_bytes = 0;
    bool   do_selftest = false;
    bool   pin_probe   = false;
    std::vector<std::string> shards;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--selftest") do_selftest = true;
        else if (a == "--pin-probe") pin_probe = true;
        else if (a == "--ranges" && i + 1 < argc) ranges_path = argv[++i];
        else if (a == "--shard") { if (i + 1 < argc) shards.push_back(argv[++i]); }
        else if (a == "--mode" && i + 1 < argc) mode_str = argv[++i];
        else if (a == "--pass" && i + 1 < argc) label = argv[++i];
        else if (a == "--warmup" && i + 1 < argc) warmup = atoi(argv[++i]);
        else if (a == "--passes" && i + 1 < argc) passes = atoi(argv[++i]);
        else if (a == "--hash" && i + 1 < argc) hash_path = argv[++i];
        else if (a == "--arena" && i + 1 < argc) arena_mib = (size_t) atoll(argv[++i]);
        else if (a == "--slots" && i + 1 < argc) slots = (size_t) atoll(argv[++i]);
        else if (a == "--two-thread" && i + 1 < argc) two_thread = atoi(argv[++i]) != 0;
        else if (a == "--max-range-bytes" && i + 1 < argc) max_range_bytes = strtoull(argv[++i], nullptr, 10);
        else { fprintf(stderr, "unknown argument: %s\n", a.c_str()); return 2; }
    }

    if (do_selftest) return selftest();
    if (!ranges_path || shards.empty() || (!mode_str && !pin_probe)) {
        fprintf(stderr, "usage: %s --ranges FILE --shard PATH [...] --mode A|B|C [options]\n", argv[0]);
        return 2;
    }
    if (slots < 1) { fprintf(stderr, "slots must be >= 1\n"); return 2; }

    std::vector<Range> ranges;
    if (!load_ranges(ranges_path, ranges)) return 2;
    if (max_range_bytes > 0) {
        std::vector<Range> sel;
        uint64_t acc = 0;
        for (const Range & r : ranges) {
            if (acc + r.bytes > max_range_bytes) break;
            sel.push_back(r); acc += r.bytes;
        }
        if (sel.empty()) { fprintf(stderr, "max-range-bytes too small for the first range\n"); return 2; }
        ranges.swap(sel);
        fprintf(stderr, "range limit: %zu ranges, %llu bytes\n",
                ranges.size(), (unsigned long long) acc);
    }

    std::vector<ShardMap> shard_maps;
    if (!map_shards(shards, shard_maps)) return 2;

    if (!cu_ok(cuInit(0), "init")) return 2;
    CUdevice dev;
    if (!cu_ok(cuDeviceGet(&dev, 0), "deviceGet")) return 2;
    CUcontext ctx;
    if (!cu_ok(cuDevicePrimaryCtxRetain(&ctx, dev), "primaryCtxRetain")) return 2;
    if (!cu_ok(cuCtxSetCurrent(ctx), "ctxSetCurrent")) return 2;

    ArmConfig cfg;
    cfg.slots      = slots;
    cfg.two_thread = two_thread;
    if (pin_probe) {
        PinState pin;
        const bool ok = pin_ranges(ranges, shard_maps, pin);
        printf("{\"mode\":\"pin-probe\",\"ranges\":%zu,\"pin_regions\":%zu,"
               "\"pin_bytes\":%llu,\"pin_gib\":%.4f,\"pin_failed\":%zu,\"complete\":%s}\n",
               ranges.size(), pin.registered.size(), (unsigned long long) pin.bytes,
               pin.bytes / 1073741824.0, pin.failed, ok ? "true" : "false");
        unpin_all(pin, shard_maps);
        cuDevicePrimaryCtxRelease(dev);
        for (ShardMap & sm : shard_maps) munmap(sm.base, (size_t) sm.size);
        return ok ? 0 : 1;
    }

    cfg.mode           = mode_str[0];
    cfg.dedicated_copy = (cfg.mode == 'C');
    if (cfg.mode != 'A' && cfg.mode != 'B' && cfg.mode != 'C') {
        fprintf(stderr, "mode must be A, B or C\n");
        return 2;
    }

    CUstream stream, copy_stream;
    if (!cu_ok(cuStreamCreate(&stream, CU_STREAM_DEFAULT), "streamCreate")) return 2;
    if (!cu_ok(cuStreamCreate(&copy_stream, CU_STREAM_DEFAULT), "copyStreamCreate")) return 2;

    RingState ring;
    if (cfg.mode != 'B') {
        ring.slots.resize(cfg.slots);
        ring.events.resize(cfg.slots);
        ring.pending.assign(cfg.slots, 0);
        for (size_t i = 0; i < cfg.slots; ++i) {
            if (!cu_ok(cuMemHostAlloc(&ring.slots[i], RING_SLOT_SIZE, 0), "slot alloc")) return 2;
            if (!cu_ok(cuEventCreate(&ring.events[i], CU_EVENT_DISABLE_TIMING), "event create")) return 2;
        }
    }

    CUevent span_start, span_end;
    if (!cu_ok(cuEventCreate(&span_start, CU_EVENT_DEFAULT), "span start")) return 2;
    if (!cu_ok(cuEventCreate(&span_end, CU_EVENT_DEFAULT), "span end")) return 2;

    const uint64_t arena_cap = (uint64_t) arena_mib * 1024 * 1024;
    CUdeviceptr arena_p;
    if (!cu_ok(cuMemAlloc(&arena_p, arena_cap), "arena malloc")) return 2;
    uint8_t * arena = (uint8_t *) (uintptr_t) arena_p;

    PinState pin;
    if (cfg.mode == 'B') {
        if (!pin_ranges(ranges, shard_maps, pin)) {
            fprintf(stderr, "Arm B: pinning failed\n");
            return 2;
        }
        cfg.pin_regions = pin.registered.size();
        cfg.pin_bytes   = pin.bytes;
        cfg.pin_failed  = pin.failed;
        fprintf(stderr, "Arm B: pinned %zu regions, %llu bytes (%.4f GiB)\n",
                cfg.pin_regions, (unsigned long long) cfg.pin_bytes, cfg.pin_bytes / 1073741824.0);
    }

    GatherJob job;
    std::thread helper;
    if (cfg.mode != 'B') helper = std::thread(helper_run, &job);

    std::vector<uint64_t> hashes;
    std::vector<uint64_t> * hash_vec = nullptr;
    if (hash_path) hash_vec = &hashes;

    int rc = 0;
    for (int p = 0; p < warmup + passes; ++p) {
        RunMetrics m;
        const PinState * pinp = (cfg.mode == 'B') ? &pin : nullptr;
        if (!perf_pass(ranges, shard_maps, pinp, ring, stream, copy_stream, span_start, span_end,
                       arena, arena_cap, job, cfg, hash_vec, m)) {
            rc = 1;
            break;
        }
        if (p >= warmup) print_metrics(m, cfg, label);
    }

    if (hash_path) {
        FILE * f = fopen(hash_path, "w");
        if (!f) { fprintf(stderr, "cannot write hash file %s\n", hash_path); rc = 1; }
        else {
            fprintf(f, "# seq fnv1a64\n");
            uint64_t seq = 0;
            for (uint64_t h : hashes) fprintf(f, "%llu %llu\n", (unsigned long long) ++seq, (unsigned long long) h);
            fclose(f);
        }
    }

    job.quit.store(true, std::memory_order_release);
    if (helper.joinable()) helper.join();

    if (cfg.mode == 'B') unpin_all(pin, shard_maps);
    for (size_t i = 0; i < cfg.slots && cfg.mode != 'B'; ++i) {
        cuEventDestroy(ring.events[i]);
        cuMemFreeHost(ring.slots[i]);
    }
    cuEventDestroy(span_start);
    cuEventDestroy(span_end);
    cuMemFree(arena_p);
    cuStreamDestroy(stream);
    cuStreamDestroy(copy_stream);
    cuDevicePrimaryCtxRelease(dev);
    for (ShardMap & sm : shard_maps) munmap(sm.base, (size_t) sm.size);
    return rc;
}
