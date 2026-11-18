extern "C" void fg919_goose_init(int argc, char ** argv);
#include "fg919.h"
#include <ggml.h>
#include <gguf.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
int main(int argc, char ** argv) {
    fg919_goose_init(0, nullptr);
    struct stat st{};
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0 || fstat(fd, &st) != 0) return 2;
    const int64_t bytes = st.st_size;
    void * base = mmap(nullptr, (size_t) bytes, PROT_READ, MAP_PRIVATE, fd, 0);
    if (base == MAP_FAILED) return 2;
    const int64_t hdr = bytes < 67108864 ? bytes : 67108864;
    std::vector<int64_t> state(8), kv(64 * 8), tens(1024 * 12), kvsl(256), tensl(4096);
    const int64_t rc = gr_parse(sl_u8{ (uint8_t *) base, hdr }, bytes, {state.data(), 8}, {kv.data(), (int64_t) kv.size()},
                                {tens.data(), (int64_t) tens.size()}, {kvsl.data(), (int64_t) kvsl.size()},
                                {tensl.data(), (int64_t) tensl.size()});
    struct gguf_init_params gp = { true, nullptr };
    struct gguf_context * ctx = gguf_init_from_file(argv[1], gp);
    if (rc != 0 || !ctx) { std::printf("parse failed rc=%lld\n", (long long) rc); return 1; }
    const int64_t n_gguf_t = gguf_get_n_tensors(ctx);
    const int64_t n_gguf_kv = gguf_get_n_kv(ctx);
    std::printf("goose: kv=%lld tensors=%lld | gguf: kv=%lld tensors=%lld\n",
                (long long) state[2], (long long) state[3], (long long) n_gguf_kv, (long long) n_gguf_t);
    int64_t same = 0, diff = 0, missing = 0;
    for (int64_t i = 0; i < n_gguf_t; ++i) {
        const char * name = gguf_get_tensor_name(ctx, i);
        const int64_t g = gr_find_tensor(sl_u8{ (uint8_t *) base, hdr }, {state.data(), 8}, {tens.data(), (int64_t) tens.size()},
                                         {tensl.data(), (int64_t) tensl.size()},
                                         sl_u8{ (uint8_t *) name, (int64_t) std::strlen(name) });
        if (g < 0) { ++missing; continue; }
        const int64_t off_g = gguf_get_tensor_offset(ctx, i);
        const int64_t off_s = tens[g * 12 + 8];
        if (off_g == off_s && gguf_get_tensor_type(ctx, i) == tens[g * 12 + 2]) ++same; else ++diff;
    }
    std::printf("tensor lookup via Goose: same=%lld diff=%lld missing=%lld\n", (long long) same, (long long) diff, (long long) missing);
    return (diff || missing || same != n_gguf_t) ? 1 : 0;
}
