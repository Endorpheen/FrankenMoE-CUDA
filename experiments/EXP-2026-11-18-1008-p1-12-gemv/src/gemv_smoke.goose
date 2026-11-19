import .moe_gemv;

fn main() {
    let w: u8[1152] = [0; 1152];
    let a: u8[292] = [0; 292];
    var out: f32[8] = default<f32[8]>();
    assert(moe_gemv::row_q4_k_x8(w, a, 1, 8, out));
    print("gemv smoke ok");
}
