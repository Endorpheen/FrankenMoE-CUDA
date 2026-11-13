checkfree simd fn unpack_nibbles(src: const u8[:], dst: u8[:]) {
    assert(dst.len >= src.len * 2);
    for i in src.len {
        dst[2 * i] = src[i] & 15;
        dst[2 * i + 1] = src[i] >> 4;
    }
}

fn main() {
    let src: u8[4] = [15, 255, 0, 128];
    var dst: u8[8] = [0, 0, 0, 0, 0, 0, 0, 0];
    unpack_nibbles(src, dst);
    assert(dst[0] == 15 && dst[1] == 0 && dst[2] == 15 && dst[3] == 15);
    assert(dst[4] == 0 && dst[5] == 0 && dst[6] == 0 && dst[7] == 8);
    print("PASS: checkfree unpack and CPU SIMD versions");
}
