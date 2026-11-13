// Gate for the Edge0 pack reader: runs it on a real pack directory and checks
// the layout facts the loader will rely on.
#include "pack.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>

static std::string read_file(const std::string & path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

static int64_t file_size(const std::string & path) {
    struct stat st {};
    if (stat(path.c_str(), &st) != 0) {
        return -1;
    }
    return (int64_t) st.st_size;
}

int main(int argc, char ** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <edge0 pack directory>\n", argv[0]);
        return 2;
    }
    const std::string dir = argv[1];
    const std::string index_text = read_file(dir + "/index.txt");
    const std::string native_text = read_file(dir + "/native_experts.txt");
    if (index_text.empty() || native_text.empty()) {
        std::fprintf(stderr, "cannot read index.txt or native_experts.txt\n");
        return 2;
    }

    edge0::Index index;
    edge0::NativeExperts native;
    std::string error;
    if (!edge0::parse_index(index_text, index, error)) {
        std::fprintf(stderr, "index parse: %s\n", error.c_str());
        return 1;
    }
    if (!edge0::parse_native_experts(native_text, native, error)) {
        std::fprintf(stderr, "native parse: %s\n", error.c_str());
        return 1;
    }

    const int64_t dense = file_size(dir + "/dense.bin");
    const edge0::PackStats stats = edge0::validate(index, native, dense);
    const auto found = edge0::problems(index, native, dense);

    std::printf("align=%lld pool=%lld declared=%lld rows=%lld\n",
                (long long) index.align, (long long) index.pool,
                (long long) index.declared_tensors, (long long) stats.tensors);
    std::printf("in_pool=%lld pool_bytes=%lld max_end=%lld misaligned=%lld out_of_pool=%lld\n",
                (long long) stats.in_pool, (long long) stats.pool_bytes,
                (long long) stats.max_end, (long long) stats.misaligned,
                (long long) stats.out_of_pool);
    std::printf("native_layers=%lld record=%lld stride=%lld bad_type=%lld bad_offset=%lld\n",
                (long long) stats.native_layers, (long long) stats.expert_record,
                (long long) stats.expert_stride, (long long) stats.bad_expert_type,
                (long long) stats.bad_expert_offset);

    int failures = 0;
    auto expect = [&](bool ok, const char * what) {
        if (!ok) {
            std::printf("FAIL %s\n", what);
            ++failures;
        }
    };
    expect(index.align == 64, "align 64");
    expect(index.pool == 1538625024LL, "pool size");
    expect(index.declared_tensors == 1367, "declared tensor count");
    expect(stats.tensors == 1367, "parsed tensor count");
    expect(stats.in_pool == 1065, "tensors in the pool");
    expect(stats.misaligned == 0, "no misaligned tensor");
    expect(stats.out_of_pool == 0, "no tensor outside the pool");
    expect(stats.native_layers == 48, "48 expert layers");
    expect(stats.expert_record == 1164800, "expert record size");
    expect(stats.expert_stride == 596377600, "expert layer stride");
    expect(stats.bad_expert_type == 0, "expert type ids 11/20");
    expect(stats.bad_expert_offset == 0, "expert offsets follow the stride");
    expect(dense == index.pool, "dense.bin matches the pool");
    expect(found.empty(), "no validation problems");

    for (const auto & problem : found) {
        std::printf("problem: %s\n", problem.c_str());
    }
    std::printf(failures ? "FGEDGE0_PACK failures=%d\n" : "FGEDGE0_PACK failures=0\n",
                failures);
    return failures ? 1 : 0;
}
