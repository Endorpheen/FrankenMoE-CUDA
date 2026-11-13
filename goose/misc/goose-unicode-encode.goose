// UTF-8 codepoint encoding in Goose (APX: P7-05).
//
// From unicode.cpp:1262-1290 unicode_cpt_to_utf8, the exact inverse of the decoder ported in
// EXP215. It runs when text is assembled for output and when the tokenizer rebuilds a word from
// codepoints, so a wrong length or a wrong continuation byte corrupts generated text rather than
// failing loudly.
//
// Native builds a std::string; the bytes are the content, so Goose writes into a caller buffer and
// returns the length. The four ranges and their bit layouts are the whole algorithm:
//   <= 0x7f        1 byte    0xxxxxxx
//   <= 0x7ff       2 bytes   110xxxxx 10xxxxxx
//   <= 0xffff      3 bytes   1110xxxx 10xxxxxx 10xxxxxx
//   <= 0x10ffff    4 bytes   11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
// Anything above U+10FFFF is not encoded at all: native falls through to an empty string.
extern "fg216e_fail" fn fail(code: i64);
extern "fg216e_st_u8" fn st_u8(base: u64, byte: i64, v: i64);

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}
// cpt_to_utf8: writes the encoding of `cpt` at `out`, returns the byte count (0 when the codepoint
// is out of range, which is what native returns as an empty string).

fn cpt_to_utf8(cpt: i64, out: u64, cap: i64) -> i64 {
    ensure(cpt >= 0, 100);
    ensure(cap >= 0, 101);

    if cpt <= 0x7f as i64 {
        ensure(cap >= 1 as i64, 102);
        st_u8(out, 0 as i64, cpt);
        return 1 as i64;
    }

    if cpt <= 0x7ff as i64 {
        ensure(cap >= 2 as i64, 103);
        st_u8(out, 0 as i64, 0xc0 as i64 | ( (cpt >> (6 as i64)) & 0x1f as i64));
        st_u8(out, 1 as i64, 0x80 as i64 | (cpt & 0x3f as i64));
        return 2 as i64;
    }

    if cpt <= 0xffff as i64 {
        ensure(cap >= 3 as i64, 104);
        st_u8(out, 0 as i64, 0xe0 as i64 | ( (cpt >> (12 as i64)) & 0x0f as i64));
        st_u8(out, 1 as i64, 0x80 as i64 | ( (cpt >> (6 as i64)) & 0x3f as i64));
        st_u8(out, 2 as i64, 0x80 as i64 | (cpt & 0x3f as i64));
        return 3 as i64;
    }

    if cpt <= 0x10ffff as i64 {
        ensure(cap >= 4 as i64, 105);
        st_u8(out, 0 as i64, 0xf0 as i64 | ( (cpt >> (18 as i64)) & 0x07 as i64));
        st_u8(out, 1 as i64, 0x80 as i64 | ( (cpt >> (12 as i64)) & 0x3f as i64));
        st_u8(out, 2 as i64, 0x80 as i64 | ( (cpt >> (6 as i64)) & 0x3f as i64));
        st_u8(out, 3 as i64, 0x80 as i64 | (cpt & 0x3f as i64));
        return 4 as i64;
    }
    return 0 as i64;
}

fn main() {
    cpt_to_utf8(0 as i64, 0 as u64, 0 as i64);
    cpt_to_utf8(0x41 as i64, 8 as u64, 4 as i64);
    cpt_to_utf8(0x20ac as i64, 16 as u64, 4 as i64);
}

