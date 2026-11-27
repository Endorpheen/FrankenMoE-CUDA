#include "grouped-moe-image.h"

static int launch_grouped_iq4_mmq(DevicePtr weight, uint64_t stride, int width, int rows, int kind, sl_i64 jobs) {
    static Module module;
    static Function pack, multiply[6];
    int count = (int)(jobs.len / 3);
    const bool wide = fg_standalone_option_enabled("I2_MOE_TILE16");
    const bool tile32 = wide && count >= 4096 && fg_standalone_option_enabled("I2_MOE_TILE32");
    const int entry = (kind == 23 ? 0 : 1) + (tile32 ? 4 : wide ? 2 : 0);
    const char *const kernels[] = {"goose_i2_xs_mmq", "goose_i2_nl_mmq",
        "goose_i2_xs_mmq16", "goose_i2_nl_mmq16", "goose_i2_xs_mmq32", "goose_i2_nl_mmq32"};
    int64_t *tiles = malloc((size_t)count * 3 * sizeof(*tiles));
    if (!tiles) return -1;
    int64_t tile_count = fg143_mmq_tiles(jobs, 3, 8, (sl_i64){tiles, 3ll * count});
    if (wide && tile_count > 0) {
        int64_t write = 0;
        for (int64_t read = 0; read < tile_count; ++read) {
            const int64_t first = tiles[3 * read];
            int64_t size = tiles[3 * read + 1];
            const int64_t plane = tiles[3 * read + 2];
            const int64_t limit = tile32 ? 32 : 16;
            while (size % 8 == 0 && size < limit && read + 1 < tile_count &&
                   tiles[3 * (read + 1)] == first + size && tiles[3 * (read + 1) + 2] == plane) {
                size += tiles[3 * (read + 1) + 1];
                ++read;
            }
            tiles[3 * write] = first;
            tiles[3 * write + 1] = size;
            tiles[3 * write + 2] = plane;
            ++write;
        }
        tile_count = write;
    }
    int error = tile_count < 1 || tile_count > 65535 ? -1 : 0;
    const size_t bytes = (size_t)(tile_count > 0 ? tile_count : 0) * 3 * sizeof(*tiles);
    if (!error && !module) error = cuda.module_load(&module, grouped_moe_cubin);
    if (!error && !pack) error = cuda.function_get(&pack, module, "goose_i2_moe_pack");
    if (!error && !multiply[entry]) error = cuda.function_get(&multiply[entry], module,
        kernels[entry]);
    if (!error) error = reserve(&cuda.mmq_tiles, bytes);
    if (!error) error = reserve(&cuda.mmq_packed, (size_t)count * width * sizeof(uint16_t));
    if (!error) error = upload_packet(cuda.mmq_tiles.address, tiles, bytes);
    free(tiles);
    void *pack_args[] = {&matrix_input, &cuda.jobs.address, &cuda.mmq_packed.address, &width, &count};
    if (!error) error = cuda.launch(pack, (unsigned)(width + 255) / 256, count, 1, 256, 1, 1, 0, NULL, pack_args, NULL);
    void *multiply_args[] = {&weight, &cuda.mmq_packed.address, &matrix_output, &cuda.jobs.address,
                            &cuda.mmq_tiles.address, &stride, &width, &rows};
    if (!error) error = cuda.launch(multiply[entry], (unsigned)(rows + 63) / 64, (unsigned)tile_count, 1,
                                     128, 1, 1, 0, NULL, multiply_args, NULL);
    return finish_matrix_launches(error);
}
