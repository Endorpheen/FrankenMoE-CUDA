import cpu_simd;
fn probe(a: const u8[:], off: i64) -> i32 {
    let v = cpu_simd::loadu_u8(a, off);
    return v[0] as i32;
}
fn main() {
    let a: u8[64] = [0; 64];
    print(probe(a, 0));
}
