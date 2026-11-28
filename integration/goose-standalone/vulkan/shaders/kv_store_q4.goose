@storage(set = 0, binding = 0, access = read)
var x: f32[];
@storage(set = 0, binding = 2, access = write)
var cache: u32[];
struct Params {
    width: u32, rows: u32, batch: u32, epsilon: f32,
    position: u32, kv_heads: u32, context: u32, theta: f32, rotary: u32
}
@push_constant
var p: Params;
fn magnitude(value: f32) -> f32 {
    return gpu_u32_bits(gpu_f32_bits(value) & 0x7fffffffu);
}

@compute(local_size = (64, 1, 1))
fn main() {
    let pair = gpu_global_id_x();
    let pairs_per_row = (p.rows * p.width) / 64u;
    if pair >= pairs_per_row * p.batch { return; }
    var words: u32[9];
    var i = 0u;
    while i < 9u {
        words[i] = 0u;
        i += 1u;
    }
    var tile = 0u;
    while tile < 2u {
        let offset = pair * 64u + tile * 32u;
        var largest = 0.0;
        i = 0u;
        while i < 32u {
            let value = x[offset + i];
            if magnitude(value) > magnitude(largest) { largest = value; }
            i += 1u;
        }
        let scale = largest / -8.0;
        var inv = 0.0;
        if scale != 0.0 { inv = 1.0 / scale; }
        let bits = gpu_pack_half(scale, 0.0);
        let start = tile * 18u;
        words[start >> 2u] |= bits << ((start & 3u) * 8u);
        i = 0u;
        while i < 16u {
            precise let lo = x[offset + i] * inv + 8.5;
            precise let hi = x[offset + 16u + i] * inv + 8.5;
            let code = (clamp(lo as i32, 0, 15) as u32) | ((clamp(hi as i32, 0, 15) as u32) << 4u);
            let at = start + i + 2u;
            words[at >> 2u] |= code << ((at & 3u) * 8u);
            i += 1u;
        }
        tile += 1u;
    }
    let start = (p.position * pairs_per_row + pair) * 9u;
    i = 0u;
    while i < 9u {
        cache[start + i] = words[i];
        i += 1u;
    }
}
