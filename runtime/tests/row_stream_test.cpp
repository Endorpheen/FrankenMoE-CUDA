#include "row_stream.h"

#include "ggml.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace bmoe;

static int fail(const char * message) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

int main() {
    constexpr int64_t columns = 16;
    constexpr int64_t rows = 512;
    constexpr size_t bytes = columns * rows * sizeof(float);

    std::vector<unsigned char> contents(bytes);
    for (size_t i = 0; i < contents.size(); ++i) contents[i] = (unsigned char) (i * 37u + 11u);

    const std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("bmoe-row-stream-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()) + ".bin");
    {
        std::ofstream file(path, std::ios::binary);
        file.write((const char *) contents.data(), (std::streamsize) contents.size());
        if (!file) return fail("could not create the test file");
    }

    ggml_init_params params = {};
    params.mem_size = 1 << 20;
    ggml_context * ctx = ggml_init(params);
    if (!ctx) return fail("ggml_init failed");
    ggml_tensor * tensor = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, columns, rows);
    ggml_set_name(tensor, "synthetic.row_table");
    void * original = tensor->data;

    DenseTensorRef ref;
    ref.tensor = tensor;
    ref.file_off = 0;
    ref.size = bytes;
    ref.file_idx = 0;

    RowStream stream;
    if (!stream.init({ref}, {path.string()}, 4096, RowStream::slab_bytes))
        return fail("RowStream::init failed");
    if (tensor->data == original) return fail("table was not rebound");

    const int32_t row = 300;
    if (!stream.gather(tensor, &row, 1)) return fail("addressed row read failed");
    const size_t offset = (size_t) row * tensor->nb[1];
    if (std::memcmp((const char *) tensor->data + offset, contents.data() + offset, tensor->nb[1]) != 0)
        return fail("read row does not match the file");

    if (stream.materialize(tensor)) return fail("table larger than the window was fully materialized");
    const RowSourceStats stats = stream.stats();
    if (stats.io_errors != 1) return fail("materialize refusal was not recorded");
    if (stats.resident_bytes > RowStream::slab_bytes) return fail("resident window limit was exceeded");

    stream.shutdown();
    if (tensor->data != original) return fail("original table address was not restored");
    ggml_free(ctx);
    std::filesystem::remove(path);
    std::puts("row-stream address read and budget guard: ok");
    return 0;
}
