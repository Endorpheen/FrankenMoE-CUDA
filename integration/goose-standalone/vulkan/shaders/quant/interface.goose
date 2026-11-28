@storage(set = 0, binding = 1, access = read)
var packed: u32[];
@storage(set = 0, binding = 2, access = write)
var y: f32[];
struct Params {
    width: u32, rows: u32, batch: u32, epsilon: f32,
    position: u32, kv_heads: u32, context: u32, theta: f32, rotary: u32
}
@push_constant
var p: Params;
fn read_byte(offset: u32) -> u32 {
    return (packed[offset >> 2u] >> ((offset & 3u) * 8u)) & 255u;
}

fn read_half(offset: u32) -> f32 {
    return gpu_unpack_half_low(read_byte(offset) | (read_byte(offset + 1u) << 8u));
}
