#include "llama-kv-cells.h"

#include <cstdio>

int main() {
    llama_kv_cells cache;
    cache.resize(12);
    cache.pos_set(3, 17);
    cache.seq_add(3, 0);
    cache.ext_set(3, {2, 5, 41});
    cache.pos_set(7, 25);
    cache.seq_add(7, 0);
    cache.ext_set(7, {3, 8, 67});

    const uint32_t indices[] = {7, 9, 3};
    constexpr uint32_t n = 3;
    constexpr uint32_t words = llama_kv_cells::goose_w();
    int64_t pos[n]{}, shifts[n]{}, seq[n * words]{};
    cache.goose_pos_shift_gather(indices, n, pos, shifts);
    cache.goose_seq_gather(seq, indices, n);
    if (pos[0] != 25 || pos[1] != -1 || pos[2] != 17 || shifts[0] || shifts[1] || shifts[2]) {
        return 1;
    }

    llama_kv_cells saved;
    saved.resize(n);
    saved.goose_restore_compact(pos, seq, n, words, cache, indices);
    cache.rm(3);
    cache.rm(7);
    cache.pos_set(9, 99);
    cache.seq_add(9, 1);
    cache.ext_set(9, {11, 14, 83});
    cache.goose_scatter(indices, pos, seq, saved, words);

    const auto & first = cache.ext_get(3);
    const auto & last = cache.ext_get(7);
    const bool ok = cache.get_used() == 2 && cache.is_empty(9)
        && cache.seq_pos_min(0) == 17 && cache.seq_pos_max(0) == 25
        && cache.seq_pos_min(1) == -1 && cache.seq_pos_max(1) == -1
        && first.x == 2 && first.y == 5 && first.tok == 41
        && last.x == 3 && last.y == 8 && last.tok == 67;
    std::printf("KV restore preserves occupancy, sequence bounds and M-RoPE extensions: %s\n", ok ? "yes" : "no");
    return ok ? 0 : 1;
}
