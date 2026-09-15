// EXP-2026-09-16-070: de-risk probe — can the 21.6 GiB pageable GGUF mmap be pinned with
// cuMemHostRegister on this host? RLIMIT_MEMLOCK is only 8 MiB and CapEff has no CAP_IPC_LOCK.
//
// The local CUDA 12.6 toolkit cannot compile any .cu file against glibc 2.43 (the rsqrt/cospi/
// sinpi exception-specification clash), so the whole model-free harness is written against the
// CUDA Driver API and built with a plain host compiler. No device code is needed for a
// memcpy-only workload.
//
// usage: exp070_pinprobe --ranges FILE --shard PATH [--shard PATH ...] [--max-range-bytes N]
//                        [--read-only]

#include <cuda.h>

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

static constexpr uint64_t PAGE = 4096;

#define CU_CHECK(call, where) do { CUresult e_ = (call); if (e_ != CUDA_SUCCESS) { \
    const char * n_ = nullptr; cuGetErrorName(e_, &n_); \
    fprintf(stderr, "CUDA error at %s: %s\n", where, n_ ? n_ : "?"); return 1; } } while (0)

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
        int fd = open(paths[i].c_str(), O_RDONLY);
        if (fd < 0) { fprintf(stderr, "cannot open %s\n", paths[i].c_str()); return false; }
        struct stat st;
        if (fstat(fd, &st) != 0) { close(fd); return false; }
        void * p = mmap(nullptr, (size_t) st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);
        if (p == MAP_FAILED) { fprintf(stderr, "mmap failed %s\n", paths[i].c_str()); return false; }
        out[i].base = p; out[i].size = (uint64_t) st.st_size;
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

int main(int argc, char ** argv) {
    const char * ranges_path = nullptr;
    uint64_t max_bytes = 0;
    // MAP_SHARED of a file cannot be registered (CUDA_ERROR_INVALID_VALUE); the shards are
    // therefore mapped MAP_PRIVATE PROT_READ and the source is registered read-only.
    unsigned reg_flags = CU_MEMHOSTREGISTER_READ_ONLY;
    std::vector<std::string> shards;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--ranges" && i + 1 < argc) ranges_path = argv[++i];
        else if (a == "--shard" && i + 1 < argc) shards.push_back(argv[++i]);
        else if (a == "--max-range-bytes" && i + 1 < argc) max_bytes = strtoull(argv[++i], nullptr, 10);
        else if (a == "--default-flags") reg_flags = 0;
        else if (a == "--read-only") reg_flags |= CU_MEMHOSTREGISTER_READ_ONLY;
        else { fprintf(stderr, "unknown arg %s\n", a.c_str()); return 2; }
    }
    if (!ranges_path || shards.empty()) { fprintf(stderr, "usage: ...\n"); return 2; }

    std::vector<Range> ranges;
    if (!load_ranges(ranges_path, ranges)) return 2;

    if (max_bytes > 0) {
        std::vector<Range> sel;
        uint64_t acc = 0;
        for (const Range & r : ranges) {
            if (acc + r.bytes > max_bytes) break;
            sel.push_back(r); acc += r.bytes;
        }
        ranges.swap(sel);
    }

    std::vector<ShardMap> maps;
    if (!map_shards(shards, maps)) return 2;

    std::vector<Interval> ivs;
    build_intervals(ranges, ivs);

    uint64_t want = 0;
    for (const Range & r : ranges) want += r.bytes;
    uint64_t iv_bytes = 0;
    for (const Interval & iv : ivs) iv_bytes += iv.end - iv.start;
    printf("{\"probe\":\"pin\",\"ranges\":%zu,\"range_bytes\":%llu,\"intervals\":%zu,"
           "\"interval_bytes\":%llu,\"flags\":\"%s\"}\n", ranges.size(),
           (unsigned long long) want, ivs.size(), (unsigned long long) iv_bytes,
           (reg_flags & CU_MEMHOSTREGISTER_READ_ONLY) ? "read_only" : "default");

    CU_CHECK(cuInit(0), "cuInit");
    CUdevice dev;
    CU_CHECK(cuDeviceGet(&dev, 0), "cuDeviceGet");
    CUcontext ctx;
    CU_CHECK(cuDevicePrimaryCtxRetain(&ctx, dev), "primaryCtxRetain");
    CU_CHECK(cuCtxSetCurrent(ctx), "ctxSetCurrent");

    size_t ok = 0, fail = 0;
    uint64_t pinned = 0;
    std::vector<Interval> registered;
    uint64_t shown = 0;
    for (const Interval & iv : ivs) {
        void * p = (uint8_t *) maps[iv.shard].base + iv.start;
        size_t n = (size_t) (iv.end - iv.start);
        CUresult e = cuMemHostRegister(p, n, reg_flags);
        if (e == CUDA_SUCCESS) { ok++; pinned += n; registered.push_back(iv); }
        else {
            fail++;
            if (shown < 5) {
                const char * nm = nullptr; cuGetErrorName(e, &nm);
                printf("PINFAIL offset=%llu size=%zu err=%s\n",
                       (unsigned long long) iv.start, n, nm ? nm : "?");
                shown++;
            }
            break;
        }
    }
    printf("{\"probe\":\"result\",\"registered_intervals\":%zu,\"failed_intervals\":%zu,"
           "\"registered_bytes\":%llu,\"registered_gib\":%.4f,\"target_gib\":%.4f,"
           "\"complete\":%s}\n",
           ok, fail, (unsigned long long) pinned, pinned / 1073741824.0,
           want / 1073741824.0, (fail == 0 && ok == ivs.size()) ? "true" : "false");

    for (const Interval & iv : registered) {
        cuMemHostUnregister((uint8_t *) maps[iv.shard].base + iv.start);
    }
    cuDevicePrimaryCtxRelease(dev);
    for (ShardMap & sm : maps) munmap(sm.base, (size_t) sm.size);
    return 0;
}
