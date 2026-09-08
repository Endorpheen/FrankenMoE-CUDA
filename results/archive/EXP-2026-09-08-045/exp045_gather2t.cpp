// EXP-2026-09-08-045: model-free prototype for a two-thread CPU gather into the pinned expert ring.
//
// A = the accepted EXP-038/041 ring path: one memcpy per chunk on the calling thread.
// B = the candidate: the calling thread and one persistent helper copy two disjoint parts of the
//     same chunk (split rounded down to a cache line); both parts are complete before cudaMemcpyAsync.
//
// Everything else matches the accepted runtime: 2 pinned slots x 16 MiB, chunking, slot event wait
// before refill, submission order, one stream, CUDA calls on the calling thread only.
//
// Usage:
//   exp045_gather2t --selftest
//   exp045_gather2t --ranges FILE --shard PATH [--shard PATH ...] --mode A|B
//                   [--pass LABEL] [--warmup N] [--passes N] [--hash FILE] [--arena MiB]

#include <cuda_runtime.h>

#define CUDA_CHECK(call, where) do { cudaError_t e_ = (call); if (e_ != cudaSuccess) { \
    fprintf(stderr, "CUDA error at %s: %s\n", where, cudaGetErrorString(e_)); return 1; } } while (0)

#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <sched.h>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static constexpr size_t RING_SLOTS     = 2;
static constexpr size_t RING_SLOT_SIZE = 16ull * 1024 * 1024;
static constexpr size_t CACHE_LINE     = 64;

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
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0) {
        return 0;
    }
    return (uint64_t) ts.tv_sec * 1000000000ull + (uint64_t) ts.tv_nsec;
}

struct ProcStat {
    uint64_t cpu_ns = 0;     // utime + stime of the whole process
    long     rss_kb = 0;
    long     vmhwm_kb = 0;
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
                // fields 14 and 15 after comm: utime, stime (clock ticks)
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
// persistent helper thread
// ---------------------------------------------------------------------------
struct GatherJob {
    std::atomic<uint64_t> seq{0};    // caller publishes the payload with this release store
    std::atomic<uint64_t> done{0};   // helper confirms its part with this release store
    std::atomic<bool>     quit{false};
    std::atomic<uint64_t> cpu_ns{0}; // helper CPU time spent inside jobs
    // payload, read by the helper only after it observed a new seq
    const uint8_t * src  = nullptr;
    uint8_t *       dst  = nullptr;
    size_t          head = 0;   // caller part
    size_t          tail = 0;   // helper part
};

static void helper_run(GatherJob * job) {
    uint64_t served = 0;
    while (!job->quit.load(std::memory_order_acquire)) {
        if (job->seq.load(std::memory_order_acquire) == served) {
            bool got = false;
            for (int spin = 0; spin < 8192; ++spin) {
                if (job->quit.load(std::memory_order_acquire)) {
                    return;
                }
                if (job->seq.load(std::memory_order_acquire) != served) {
                    got = true;
                    break;
                }
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
// metrics
// ---------------------------------------------------------------------------
static const size_t  BUCKET_EDGES[] = { 600ull * 1024, 1200ull * 1024, 2560ull * 1024, SIZE_MAX };
static const char *  BUCKET_NAMES[] = { "lt600KiB", "600KiB_1p2MiB", "1p2MiB_2p5MiB", "ge2p5MiB" };
static constexpr int N_BUCKETS = 4;

struct Bucket {
    uint64_t count = 0;
    uint64_t bytes = 0;
};

struct RunMetrics {
    uint64_t wall_ns = 0;
    uint64_t gather_ns = 0;      // caller-visible gather interval (B includes the helper wait)
    uint64_t submit_ns = 0;
    uint64_t slotwait_ns = 0;
    uint64_t drain_ns = 0;
    uint64_t calls = 0;
    uint64_t chunks = 0;
    uint64_t bytes = 0;
    uint64_t caller_cpu_ns = 0;
    uint64_t gather_cpu_ns = 0;  // caller CPU inside gather; symmetric for A and B
    uint64_t helper_cpu_ns = 0;
    ProcStat proc;
    Bucket   buckets[N_BUCKETS];
};

// ---------------------------------------------------------------------------
// workload
// ---------------------------------------------------------------------------
struct Range {
    int      shard;
    uint64_t addr;   // absolute file offset
    uint64_t bytes;
};

static bool load_ranges(const char * path, std::vector<Range> & out) {
    FILE * f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "cannot open ranges file %s\n", path);
        return false;
    }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        Range r;
        unsigned long long a = 0, b = 0;
        if (sscanf(line, "%d %llu %llu", &r.shard, &a, &b) != 3) {
            fprintf(stderr, "bad range line: %s", line);
            fclose(f);
            return false;
        }
        r.addr  = a;
        r.bytes = b;
        out.push_back(r);
    }
    fclose(f);
    return !out.empty();
}

struct ShardMap {
    void *   base = nullptr;
    uint64_t size = 0;
};

static bool map_shards(const std::vector<std::string> & paths, std::vector<ShardMap> & out) {
    out.resize(paths.size());
    for (size_t i = 0; i < paths.size(); ++i) {
        int fd = open(paths[i].c_str(), O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "cannot open shard %s\n", paths[i].c_str());
            return false;
        }
        struct stat st;
        if (fstat(fd, &st) != 0) {
            close(fd);
            return false;
        }
        void * p = mmap(nullptr, (size_t) st.st_size, PROT_READ, MAP_SHARED, fd, 0);
        close(fd);
        if (p == MAP_FAILED) {
            fprintf(stderr, "mmap failed for %s\n", paths[i].c_str());
            return false;
        }
        out[i].base = p;
        out[i].size = (uint64_t) st.st_size;
    }
    return true;
}

// ---------------------------------------------------------------------------
// ring
// ---------------------------------------------------------------------------
struct RingState {
    void *      slots[RING_SLOTS]   = { nullptr };
    cudaEvent_t events[RING_SLOTS]  = { nullptr };
    bool        pending[RING_SLOTS] = { false };
    size_t      next = 0;
};

static inline size_t split_head(size_t len) {
    return (len / 2) / CACHE_LINE * CACHE_LINE;
}

// the ring loop of ggml_backend_cuda_set_tensor_async_pinned_ring; only the gather step differs.
// h_out (optional) receives one FNV-1a hash per staged chunk; check_slot compares the slot with
// the source before submission (correctness runs only — both add cost and are off in timed runs).
static bool stage_range(RingState & ring, cudaStream_t stream, uint8_t * dst_base,
                        uint64_t arena_cap, uint64_t & dst_cursor,
                        const uint8_t * src, size_t nbytes, bool two_thread,
                        GatherJob & job, RunMetrics & m,
                        std::vector<uint64_t> * h_out, uint64_t & seq, bool check_slot) {
    size_t remaining = nbytes;
    size_t done      = 0;
    m.calls++;

    while (remaining > 0) {
        const size_t chunk    = remaining < RING_SLOT_SIZE ? remaining : RING_SLOT_SIZE;
        const size_t slot_idx = ring.next;
        ring.next = (ring.next + 1) % RING_SLOTS;

        if (ring.pending[slot_idx]) {
            const uint64_t w0 = now_ns();
            if (cudaEventSynchronize(ring.events[slot_idx]) != cudaSuccess) {
                return false;
            }
            m.slotwait_ns += now_ns() - w0;
            ring.pending[slot_idx] = false;
        }

        uint8_t *       slot  = (uint8_t *) ring.slots[slot_idx];
        const uint8_t * src_p = src + done;

        const uint64_t g0  = now_ns();
        const uint64_t gc0 = thread_cpu_ns();
        if (two_thread) {
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
        const uint64_t g1 = now_ns();
        m.gather_ns += g1 - g0;

        if (check_slot && memcmp(slot, src_p, chunk) != 0) {
            fprintf(stderr, "FAIL: staged slot differs from source at chunk %llu\n",
                    (unsigned long long) seq + 1);
            return false;
        }
        if (h_out) {
            uint64_t h = 1469598103934665603ull;
            for (size_t i = 0; i < chunk; ++i) {
                h ^= slot[i];
                h *= 1099511628211ull;
            }
            h_out->push_back(h);
        }

        uint64_t off = dst_cursor;
        if (off + chunk > arena_cap) {
            off = 0;
        }
        dst_cursor = off + chunk;

        const uint64_t s0 = now_ns();
        if (cudaMemcpyAsync(dst_base + off, slot, chunk, cudaMemcpyHostToDevice, stream) != cudaSuccess) {
            return false;
        }
        if (cudaEventRecord(ring.events[slot_idx], stream) != cudaSuccess) {
            return false;
        }
        m.submit_ns += now_ns() - s0;
        ring.pending[slot_idx] = true;

        int b = 0;
        while (chunk >= BUCKET_EDGES[b]) {
            b++;
        }
        m.buckets[b].count++;
        m.buckets[b].bytes += chunk;

        m.chunks++;
        m.bytes += chunk;
        done      += chunk;
        remaining -= chunk;
    }
    return true;
}

// ---------------------------------------------------------------------------
// perf pass
// ---------------------------------------------------------------------------
static bool perf_pass(const std::vector<Range> & ranges, const std::vector<ShardMap> & shards,
                      RingState & ring, cudaStream_t stream, uint8_t * arena, uint64_t arena_cap,
                      GatherJob & job, bool two_thread, std::vector<uint64_t> * h_out,
                      RunMetrics & m) {
    uint64_t seq        = 0;
    uint64_t dst_cursor = 0;

    const uint64_t cc0 = thread_cpu_ns();
    const uint64_t hc0 = job.cpu_ns.load(std::memory_order_relaxed);

    const uint64_t t0 = now_ns();
    for (const Range & r : ranges) {
        if ((size_t) r.shard >= shards.size()) {
            fprintf(stderr, "FAIL: bad shard index %d\n", r.shard);
            return false;
        }
        const ShardMap & sm = shards[r.shard];
        if (r.addr + r.bytes > sm.size) {
            fprintf(stderr, "FAIL: range outside shard %d\n", r.shard);
            return false;
        }
        if (!stage_range(ring, stream, arena, arena_cap, dst_cursor,
                         (const uint8_t *) sm.base + r.addr, r.bytes,
                         two_thread, job, m, h_out, seq, h_out != nullptr)) {
            return false;
        }
    }
    const uint64_t d0 = now_ns();
    if (cudaStreamSynchronize(stream) != cudaSuccess) {
        fprintf(stderr, "FAIL: drain sync failed\n");
        return false;
    }
    const uint64_t t1 = now_ns();

    m.drain_ns      = t1 - d0;
    m.wall_ns       = t1 - t0;
    m.caller_cpu_ns = thread_cpu_ns() - cc0;
    m.helper_cpu_ns = job.cpu_ns.load(std::memory_order_relaxed) - hc0;
    m.proc          = proc_stat();
    return true;
}

static void print_metrics(const RunMetrics & m, const char * mode, const char * label) {
    printf("{\"mode\":\"%s\",\"label\":\"%s\",\"wall_ms\":%.3f,\"gather_ms\":%.3f,"
           "\"submit_ms\":%.3f,\"slotwait_ms\":%.3f,\"drain_ms\":%.3f,"
           "\"calls\":%llu,\"chunks\":%llu,\"bytes\":%llu,"
           "\"caller_cpu_ms\":%.3f,\"gather_cpu_ms\":%.3f,\"helper_cpu_ms\":%.3f,"
           "\"proc_cpu_ms\":%.3f,\"rss_mib\":%.1f,\"vmhwm_mib\":%.1f,\"vmswap_kib\":%ld,"
           "\"buckets\":{",
           mode, label,
           m.wall_ns / 1e6, m.gather_ns / 1e6, m.submit_ns / 1e6, m.slotwait_ns / 1e6, m.drain_ns / 1e6,
           (unsigned long long) m.calls, (unsigned long long) m.chunks, (unsigned long long) m.bytes,
           m.caller_cpu_ns / 1e6, m.gather_cpu_ns / 1e6, m.helper_cpu_ns / 1e6, m.proc.cpu_ns / 1e6,
           m.proc.rss_kb / 1024.0, m.proc.vmhwm_kb / 1024.0, m.proc.vmswap_kb);
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
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

static int selftest() {
    const size_t SRC_BYTES = 128ull * 1024 * 1024;
    const size_t DEV_BYTES = 128ull * 1024 * 1024;

    uint8_t * src = (uint8_t *) mmap(nullptr, SRC_BYTES, PROT_READ | PROT_WRITE,
                                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (src == MAP_FAILED) {
        fprintf(stderr, "source mmap failed\n");
        return 2;
    }
    for (size_t i = 0; i < SRC_BYTES; ++i) {
        src[i] = (uint8_t) (i * 31 + (i >> 8) * 7 + 11);
    }
    const uint64_t src_hash0 = fnv(src, SRC_BYTES);

    CUDA_CHECK(cudaSetDevice(0), "setDevice");
    cudaStream_t stream;
    CUDA_CHECK(cudaStreamCreate(&stream), "streamCreate");
    uint8_t * dev = nullptr;
    CUDA_CHECK(cudaMalloc(&dev, DEV_BYTES), "dev malloc");

    RingState ring;
    for (size_t i = 0; i < RING_SLOTS; ++i) {
        if (cudaHostAlloc(&ring.slots[i], RING_SLOT_SIZE, cudaHostAllocDefault) != cudaSuccess) {
            fprintf(stderr, "pinned slot alloc failed\n");
            return 2;
        }
        if (cudaEventCreateWithFlags(&ring.events[i], cudaEventDisableTiming) != cudaSuccess) {
            fprintf(stderr, "event create failed\n");
            return 2;
        }
    }

    struct Case {
        size_t off;
        size_t bytes;
    };
    const std::vector<Case> cases = {
        { 0, 1 }, { 3, 63 }, { 1, 64 }, { 1, 65 }, { 3, 127 },
        { 1, 524800 }, { 3, 524801 }, { 1, RING_SLOT_SIZE - 1 }, { 0, RING_SLOT_SIZE },
        { 1, RING_SLOT_SIZE + 1 }, { 3, RING_SLOT_SIZE + 65 }, { 1, 17ull << 20 },
        { 1, 33ull << 20 },
    };
    std::vector<size_t> dev_off(cases.size(), 0);
    size_t need = 0;
    for (size_t i = 0; i < cases.size(); ++i) {
        dev_off[i] = need;
        need += cases[i].bytes;
    }
    if (need > DEV_BYTES) {
        fprintf(stderr, "selftest cases do not fit the device buffer\n");
        return 2;
    }

    std::vector<uint64_t> hashes_a, hashes_b;
    bool ok = true;

    // focused check: nothing beyond the chunk is written into a freshly poisoned slot
    for (int mode = 0; mode < 2 && ok; ++mode) {
        const bool two_thread = (mode == 1);
        GatherJob job;
        std::thread helper(helper_run, &job);
        RunMetrics m;
        uint64_t seq = 0;
        const size_t chunk = 524801;
        uint64_t cursor = 0;
        memset(ring.slots[0], 0xCC, RING_SLOT_SIZE);
        memset(ring.slots[1], 0xCC, RING_SLOT_SIZE);
        ring.pending[0] = ring.pending[1] = false;
        ring.next = 0;
        ok = stage_range(ring, stream, dev, DEV_BYTES, cursor, src + 7, chunk,
                         two_thread, job, m, nullptr, seq, true);
        const size_t used = (ring.next + RING_SLOTS - 1) % RING_SLOTS;
        const uint8_t * s = (const uint8_t *) ring.slots[used];
        bool tail_clean = true;
        for (size_t i = chunk; i < chunk + 4096; ++i) {
            if (s[i] != 0xCC) {
                tail_clean = false;
                break;
            }
        }
        check(ok && tail_clean, two_thread ? "st2_slot_tail_untouched_B" : "st2_slot_tail_untouched_A");
        job.quit.store(true, std::memory_order_release);
        helper.join();
    }

    // full case list through both modes, three repetitions (slot reuse), hashes compared
    for (int mode = 0; mode < 2 && ok; ++mode) {
        const bool two_thread = (mode == 1);
        GatherJob job;
        std::thread helper(helper_run, &job);

        uint64_t seq = 0;
        std::vector<uint64_t> hashes;
        for (int rep = 0; rep < 3 && ok; ++rep) {
            for (size_t ci = 0; ci < cases.size() && ok; ++ci) {
                const Case & c = cases[ci];
                RunMetrics m;
                uint64_t cursor = dev_off[ci];
                ok = stage_range(ring, stream, dev, DEV_BYTES, cursor, src + c.off, c.bytes,
                                 two_thread, job, m, &hashes, seq, true);
            }
        }
        if (cudaStreamSynchronize(stream) != cudaSuccess) {
            fprintf(stderr, "selftest drain failed\n");
            ok = false;
        }
        // helper stops before any buffer goes away
        job.quit.store(true, std::memory_order_release);
        helper.join();

        if (mode == 0) {
            hashes_a.swap(hashes);
            check(ok, "st3_mode_A_all_checks");
        } else {
            hashes_b.swap(hashes);
            check(ok, "st3_mode_B_all_checks");
        }
    }

    check(hashes_a.size() == hashes_b.size() && hashes_a == hashes_b,
          "st4_slot_bytes_B_equals_A");
    check(fnv(src, SRC_BYTES) == src_hash0, "st5_source_unchanged");

    // device round-trip: every case region equals its source bytes
    std::vector<uint8_t> back(need);
    if (cudaMemcpy(back.data(), dev, need, cudaMemcpyDeviceToHost) == cudaSuccess) {
        bool dev_ok = true;
        for (size_t ci = 0; ci < cases.size() && dev_ok; ++ci) {
            const Case & c = cases[ci];
            if (memcmp(back.data() + dev_off[ci], src + c.off, c.bytes) != 0) {
                dev_ok = false;
            }
        }
        check(dev_ok, "st6_device_roundtrip_equals_source");
    } else {
        check(false, "st6_device_roundtrip_equals_source");
    }

    // lifetime: the helper is already joined before the source is unmapped
    munmap(src, SRC_BYTES);
    src = nullptr;
    usleep(100000);
    check(true, "st7_helper_stopped_before_munmap");

    printf("SELFTEST %s (%d pass, %d fail)\n", checks_fail ? "FAIL" : "PASS",
           checks_pass, checks_fail);
    for (size_t i = 0; i < RING_SLOTS; ++i) {
        cudaEventDestroy(ring.events[i]);
        cudaFreeHost(ring.slots[i]);
    }
    cudaFree(dev);
    cudaStreamDestroy(stream);
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
    int  warmup   = 1;
    int  passes   = 1;
    size_t arena_mib = 256;
    std::vector<std::string> shards;

    bool do_selftest = false;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--selftest") {
            do_selftest = true;
        } else if (a == "--ranges" && i + 1 < argc) {
            ranges_path = argv[++i];
        } else if (a == "--shard") {
            if (i + 1 < argc) shards.push_back(argv[++i]);
        } else if (a == "--mode" && i + 1 < argc) {
            mode_str = argv[++i];
        } else if (a == "--pass" && i + 1 < argc) {
            label = argv[++i];
        } else if (a == "--warmup" && i + 1 < argc) {
            warmup = atoi(argv[++i]);
        } else if (a == "--passes" && i + 1 < argc) {
            passes = atoi(argv[++i]);
        } else if (a == "--hash" && i + 1 < argc) {
            hash_path = argv[++i];
        } else if (a == "--arena" && i + 1 < argc) {
            arena_mib = (size_t) atoll(argv[++i]);
        } else {
            fprintf(stderr, "unknown argument: %s\n", a.c_str());
            return 2;
        }
    }

    if (do_selftest) {
        return selftest();
    }
    if (!ranges_path || shards.empty() || !mode_str) {
        fprintf(stderr, "usage: %s --ranges FILE --shard PATH [--shard PATH ...] --mode A|B [options]\n",
                argv[0]);
        return 2;
    }
    const bool two_thread = strcmp(mode_str, "B") == 0;
    if (!two_thread && strcmp(mode_str, "A") != 0) {
        fprintf(stderr, "mode must be A or B\n");
        return 2;
    }

    std::vector<Range> ranges;
    if (!load_ranges(ranges_path, ranges)) {
        return 2;
    }
    std::vector<ShardMap> shard_maps;
    if (!map_shards(shards, shard_maps)) {
        return 2;
    }

    CUDA_CHECK(cudaSetDevice(0), "setDevice");
    cudaStream_t stream;
    CUDA_CHECK(cudaStreamCreate(&stream), "streamCreate");

    RingState ring;
    for (size_t i = 0; i < RING_SLOTS; ++i) {
        if (cudaHostAlloc(&ring.slots[i], RING_SLOT_SIZE, cudaHostAllocDefault) != cudaSuccess) {
            fprintf(stderr, "pinned slot alloc failed\n");
            return 2;
        }
        if (cudaEventCreateWithFlags(&ring.events[i], cudaEventDisableTiming) != cudaSuccess) {
            fprintf(stderr, "event create failed\n");
            return 2;
        }
    }
    const uint64_t arena_cap = (uint64_t) arena_mib * 1024 * 1024;
    uint8_t * arena = nullptr;
    CUDA_CHECK(cudaMalloc(&arena, arena_cap), "arena malloc");

    // Keep A faithful to the production baseline: no idle spinning helper.
    // B creates one persistent helper outside both warmup and measured passes.
    GatherJob job;
    std::thread helper;
    if (two_thread) {
        helper = std::thread(helper_run, &job);
    }

    FILE * hash_file = nullptr;
    std::vector<uint64_t> * hash_vec = nullptr;
    std::vector<uint64_t> hashes;
    if (hash_path) {
        hash_file = fopen(hash_path, "w");
        if (!hash_file) {
            fprintf(stderr, "cannot write hash file %s\n", hash_path);
            return 2;
        }
        hash_vec = &hashes;
    }

    int rc = 0;
    for (int p = 0; p < warmup + passes; ++p) {
        RunMetrics m;
        if (!perf_pass(ranges, shard_maps, ring, stream, arena, arena_cap,
                       job, two_thread, hash_vec, m)) {
            rc = 1;
            break;
        }
        if (p >= warmup) {
            print_metrics(m, mode_str, label);
        }
    }

    // stop the helper before releasing everything
    if (hash_file) {
        fprintf(hash_file, "# seq fnv1a64\n");
        uint64_t seq = 0;
        for (uint64_t h : hashes) {
            fprintf(hash_file, "%llu %llu\n", (unsigned long long) ++seq, (unsigned long long) h);
        }
        fclose(hash_file);
    }

    job.quit.store(true, std::memory_order_release);
    if (helper.joinable()) {
        helper.join();
    }

    for (size_t i = 0; i < RING_SLOTS; ++i) {
        cudaEventDestroy(ring.events[i]);
        cudaFreeHost(ring.slots[i]);
    }
    cudaFree(arena);
    cudaStreamDestroy(stream);
    for (ShardMap & sm : shard_maps) {
        munmap(sm.base, (size_t) sm.size);
    }
    return rc;
}
