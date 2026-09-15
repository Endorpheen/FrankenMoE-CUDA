// EXP-2026-09-16-074 G1: which mapping mode can the driver register on this host?
//
// The EXP-070 probe claimed MAP_SHARED cannot be registered but never tested it (its
// map_shards() hardcodes MAP_PRIVATE). The runtime maps the GGUF shards MAP_SHARED
// PROT_READ, so the integration fork (Option A0 vs Option A) depends on this.
//
// This probe uses the exact runtime API and flags the CUDA backend uses:
//   cudaHostRegister(ptr, size, cudaHostRegisterPortable | cudaHostRegisterReadOnly)
// The distro CUDA 12.4 runtime headers compile under g++-13, so no .cu compile is needed.
//
// usage: g1_pinprobe --ranges FILE --shard PATH [--shard PATH ...]

#include <cuda_runtime.h>

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

static constexpr uint64_t PAGE = 4096;

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

static void * map_file(const std::string & path, int flags, uint64_t * size_out) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) { fprintf(stderr, "cannot open %s\n", path.c_str()); return nullptr; }
    struct stat st;
    if (fstat(fd, &st) != 0) { close(fd); return nullptr; }
    void * p = mmap(nullptr, (size_t) st.st_size, PROT_READ, flags, fd, 0);
    close(fd);
    if (p == MAP_FAILED) { fprintf(stderr, "mmap failed %s\n", path.c_str()); return nullptr; }
    *size_out = (uint64_t) st.st_size;
    return p;
}

static const char * reg_result(cudaError_t e) {
    return cudaGetErrorString(e);
}

// register a window read-only, print result, unregister on success
static void try_register(const char * label, void * p, size_t n, unsigned flags) {
    cudaError_t e = cudaHostRegister(p, n, flags);
    printf("PROBE %-34s ptr=%p size=%-10zu flags=%-4u -> %s\n",
           label, p, n, flags, e == cudaSuccess ? "OK" : reg_result(e));
    if (e == cudaSuccess) {
        cudaError_t u = cudaHostUnregister(p);
        if (u != cudaSuccess) {
            printf("PROBE %-34s UNREGISTER FAILED -> %s\n", label, reg_result(u));
        }
    }
    (void) cudaGetLastError();
}

int main(int argc, char ** argv) {
    const char * ranges_path = nullptr;
    std::vector<std::string> shards;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--ranges" && i + 1 < argc) ranges_path = argv[++i];
        else if (a == "--shard" && i + 1 < argc) shards.push_back(argv[++i]);
        else { fprintf(stderr, "unknown arg %s\n", a.c_str()); return 2; }
    }
    if (!ranges_path || shards.empty()) { fprintf(stderr, "usage: ...\n"); return 2; }

    std::vector<Range> ranges;
    if (!load_ranges(ranges_path, ranges)) return 2;

    struct rlimit rl;
    getrlimit(RLIMIT_MEMLOCK, &rl);
    printf("PROBE rlimit_memlock_cur=%llu soft_max=%llu\n",
           (unsigned long long) rl.rlim_cur, (unsigned long long) rl.rlim_max);

    cudaError_t init = cudaFree(nullptr);
    printf("PROBE cuda init -> %s\n", init == cudaSuccess ? "OK" : reg_result(init));

    // shard 1 holds the first range; use it for the window tests
    const Range & first = ranges[0];
    uint64_t win_start = (first.addr / PAGE) * PAGE;
    const size_t WIN = 1 << 20;

    uint64_t shared_size = 0, private_size = 0;
    void * shared  = map_file(shards[first.shard], MAP_SHARED,  &shared_size);
    void * priv    = map_file(shards[first.shard], MAP_PRIVATE, &private_size);
    if (!shared || !priv) return 2;
    printf("PROBE shard=%s size=%llu win_start=%llu\n", shards[first.shard].c_str(),
           (unsigned long long) shared_size, (unsigned long long) win_start);

    // 2x2 matrix on a 1 MiB window, plus a full real interval
    try_register("map_shared  read_only window", (uint8_t *) shared + win_start, WIN,
                 cudaHostRegisterPortable | cudaHostRegisterReadOnly);
    try_register("map_shared  default    window", (uint8_t *) shared + win_start, WIN,
                 cudaHostRegisterDefault);
    try_register("map_private read_only window", (uint8_t *) priv + win_start, WIN,
                 cudaHostRegisterPortable | cudaHostRegisterReadOnly);
    try_register("map_private default    window", (uint8_t *) priv + win_start, WIN,
                 cudaHostRegisterDefault);

    // one real (page-aligned, merged) interval - the unit the runtime would register
    uint64_t iv_start = (first.addr / PAGE) * PAGE;
    uint64_t iv_end   = ((first.addr + first.bytes + PAGE - 1) / PAGE) * PAGE;
    printf("PROBE first_interval start=%llu end=%llu bytes=%llu\n",
           (unsigned long long) iv_start, (unsigned long long) iv_end,
           (unsigned long long) (iv_end - iv_start));
    try_register("map_shared  read_only interval", (uint8_t *) shared + iv_start,
                 (size_t) (iv_end - iv_start),
                 cudaHostRegisterPortable | cudaHostRegisterReadOnly);
    try_register("map_shared  default    interval", (uint8_t *) shared + iv_start,
                 (size_t) (iv_end - iv_start), cudaHostRegisterDefault);

    // RLIMIT_MEMLOCK: does an anonymous >8 MiB registration succeed at all?
    const size_t ANON = 16 << 20;
    void * anon = mmap(nullptr, ANON, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (anon != MAP_FAILED) {
        try_register("anonymous   read_only 16 MiB", anon, ANON,
                     cudaHostRegisterPortable | cudaHostRegisterReadOnly);
        try_register("anonymous   default   32 MiB", anon, ANON,
                     cudaHostRegisterDefault);
        munmap(anon, ANON);
    } else {
        printf("PROBE anonymous mmap failed\n");
    }

    munmap(shared, (size_t) shared_size);
    munmap(priv, (size_t) private_size);
    return 0;
}
