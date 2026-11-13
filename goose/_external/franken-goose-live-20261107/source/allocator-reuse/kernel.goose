struct FreeBlock { offset: u64, size: u64 }
struct Fit { index: i64, max_avail: u64 }
fn goose_choose_reuse(blocks: const FreeBlock[:], requested: u64) -> Fit {
    var best: i64 = -1;
    var best_size: u64 = 0;
    var available: u64 = 0;
    if blocks.len > 1 {
        for i in blocks.len - 1 {
            let value = blocks[i].size;
            if value > available { available = value; }
            if value >= requested && (best < 0 || value <= best_size) {
                best = i;
                best_size = value;
            }
        }
    }
    return Fit { index: best, max_avail: available };
}
fn main() {
    let blocks: FreeBlock[3] = [FreeBlock { offset: 0, size: 64 }, FreeBlock { offset: 64, size: 32 }, FreeBlock { offset: 96, size: 1024 }];
    let fit = goose_choose_reuse(blocks, 16);
    assert(fit.index == 1 && fit.max_avail == 64);
}
