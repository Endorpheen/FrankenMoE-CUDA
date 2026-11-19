// UTF-8 accounting used by the score-based tokenizer path.
// The byte count of a codepoint is 1 + (c >= 0x80) + (c >= 0x800) + (c >= 0x10000), and the
// lead byte of a multi-byte sequence is (0xF00 >> s) & 0xFF while the continuations are 0x80.
// Both are transcribed exactly, including the fact that the lead byte is derived by shift
// rather than by a lookup table: any "cleaner" form must produce the same bytes.
fn g_utf8_len(c: i64) -> i64 {
    var s: i64 = 1;
    if c >= 128 {
        s = s + 1;
    }
    if c >= 2048 {
        s = s + 1;
    }
    if c >= 65536 {
        s = s + 1;
    }
    return s;
}

// The byte at position i of the encoding of c, exactly as the native fallback builds it:
// a lead or continuation marker OR-ed with the payload bits taken at the right shift.
// Dropping the payload was a real defect in the first version of this port.
fn g_utf8_byte(c: i64, i: i64) -> i64 {
    let s = g_utf8_len(c);
    if s == 1 {
        return c;
    }
    var base: i64 = 128;
    if i == 0 {
        var v: i64 = 3840;
        var k: i64 = 0;
        while k < s {
            v = v / 2;
            k = k + 1;
        }
        base = v - 256 * (v / 256);
    }
    var shift: i64 = (s - i - 1) * 6;
    var payload: i64 = c;
    var d: i64 = 0;
    while d < shift {
        payload = payload / 2;
        d = d + 1;
    }
    payload = payload - 64 * (payload / 64);
    // base and payload occupy disjoint bits (base above 0x7F, payload below 0x40),
    // so the OR the native code performs is exactly this sum
    return base + payload;
}

fn main() {
    let a = g_utf8_len(233);
    let b = g_utf8_byte(233, 0);
    if a + b == 0 {
        return;
    }
}
