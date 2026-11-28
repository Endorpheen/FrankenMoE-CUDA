@storage(set = 0, binding = 0, access = read)
var x: u32[];
@storage(set = 0, binding = 1, access = read)
var w: u32[];
@storage(set = 0, binding = 2, access = write)
var y: f32[];
struct Params {
    width: u32, rows: u32, batch: u32, epsilon: f32,
    position: u32, kv_heads: u32, context: u32, theta: f32, rotary: u32
}
@push_constant
var p: Params;

fn read_byte(offset: u32) -> u32 {
    return (w[offset >> 2u] >> ((offset & 3u) * 8u)) & 255u;
}

fn decode_weight(row: u32, column: u32) -> f32 {
    let block = (row * (p.width >> 5u) + (column >> 5u)) * 18u;
    let index = column & 31u;
    let byte_value = read_byte(block + 2u + (index & 15u));
    let quant = ((byte_value >> ((index >> 4u) * 4u)) & 15u) as i32;
    let scale = gpu_unpack_half_low(read_byte(block) | (read_byte(block + 1u) << 8u));
    return ((quant - 8) as f32) * scale;
}
@compute(local_size = (64, 1, 1))
fn main() {
    let token = gpu_workgroup_id_x();
    let id = x[token];
    let row = id - p.position;
    var i = gpu_local_id_x();
    while i < p.width {
        if id >= p.position && row < p.rows {
            y[token * p.width + i] = decode_weight(row, i);
        } else {
            y[token * p.width + i] = 0.0;
        }
        i += 64u;
    }
}
