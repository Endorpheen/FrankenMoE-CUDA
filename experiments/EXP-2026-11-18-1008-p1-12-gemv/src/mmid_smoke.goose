import .mmid_tiled_q4_k;

fn main() {
    let src0: u8[288] = [0; 288];
    let act: u8[584] = [0; 584];
    let routes: i32[4] = [0, 0, 0, 0];
    var dst: u8[64] = [0; 64];
    var acc: f32[16384] = default<f32[16384]>();
    var qbuf: u8[256] = default<u8[256]>();
    let rc = mmid_tiled_q4_k::mmid_tiled_q4_k(src0, 256, 1, 144, act, 292, routes, 2, 4, 4, 4, dst, 0, 1, acc, qbuf);
    print("mmid smoke ", rc);
}
