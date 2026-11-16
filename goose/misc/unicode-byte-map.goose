// Byte-to-codepoint map: unicode_byte_to_utf8_map in unicode.cpp.
//
// Three printable ranges keep their own value; every other byte is assigned the next codepoint from 256 upward, in
// byte order. The native version built an unordered_map and then repaired the gaps in a second pass; here it is one
// pass writing 256 codepoints, which is what the caller actually wants.
//
// Proved equal by comparing all 256 entries against the native map, so every range edge and every gap is covered.

extern "b2u_st_u32" fn st(buf: u64, byte: i64, v: i64);

fn printable(ch: i64) -> bool {
    if ch >= (0x21 as i64) && ch <= (0x7E as i64) {
        return true;
    }
    if ch >= (0xA1 as i64) && ch <= (0xAC as i64) {
        return true;
    }
    if ch >= (0xAE as i64) && ch <= (0xFF as i64) {
        return true;
    }
    return false;
}

export fn b2u_map(out: u64) -> i64 {
    var ch: i64 = 0;
    var n: i64 = 0;
    while ch < (256 as i64) {
        if printable(ch) {
            st(out, ch * (4 as i64), ch);
        } else {
            st(out, ch * (4 as i64), (256 as i64) + n);
            n = n + (1 as i64);
        }
        ch = ch + (1 as i64);
    }
    return n;
}
