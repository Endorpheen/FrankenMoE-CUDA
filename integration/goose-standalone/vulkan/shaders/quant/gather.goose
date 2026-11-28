@storage(set = 0, binding = 0, access = read)
var ids: u32[];
@compute(local_size = (64, 1, 1))
fn main() {
    let token = gpu_workgroup_id_x();
    let id = ids[token];
    let row = id - p.position;
    var i = gpu_local_id_x();
    while i < p.width {
        var weight = 0.0;
        if id >= p.position && row < p.rows { weight = decode_weight(row, i); }
        y[token * p.width + i] = weight;
        i += 64u;
    }
}
