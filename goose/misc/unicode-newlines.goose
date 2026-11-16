// Newline splitting: unicode_regex_split_custom_newlines in unicode.cpp.
//
// Each caller segment is cut into maximal runs of newline characters and runs of everything else. No
// classification table is needed - the only question asked is whether a codepoint is U+000A - so this module has no
// FFI at all beyond reading the codepoint array.

extern "nl_ld_u32" fn ld32(base: u64, byte: i64) -> i64;
extern "nl_ld_i64" fn ld64(base: u64, byte: i64) -> i64;
extern "nl_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "nl_fail" fn fail(code: i64);

const NL: i64 = 10;

fn ensure(v: bool, c: i64) {
    if !v {
        fail(c);
    }
}

fn run_segment(cpts: u64, out: u64, n_out: i64, cap: i64, ini: i64, end: i64) -> i64 {
    var count: i64 = n_out;
    var pos: i64 = ini;
    while pos < end {
        let is_nl: bool = ld32(cpts, pos * (4 as i64)) == NL;
        let run_start: i64 = pos;
        var more: bool = true;
        while more {
            if pos >= end {
                more = false;
            } else {
                if (ld32(cpts, pos * (4 as i64)) == NL) == is_nl {
                    pos = pos + (1 as i64);
                } else {
                    more = false;
                }
            }
        }
        ensure(pos - run_start > (0 as i64), 10);
        ensure(count < cap, 11);
        st(out, count * (8 as i64), pos - run_start);
        count = count + (1 as i64);
    }
    return count;
}

// Packet: k=0 {0 cpts, 1 n_offsets, 2 offsets, 3 out, 4 out_cap}
export fn nl_split(packet: i64) -> i64 {
    let p: u64 = packet as u64;
    let cpts: u64 = ld64(p, 0 as i64) as u64;
    let n_off: i64 = ld64(p, 8 as i64);
    let offsets: u64 = ld64(p, 16 as i64) as u64;
    let out: u64 = ld64(p, 24 as i64) as u64;
    let cap: i64 = ld64(p, 32 as i64);

    ensure(cpts != (0 as u64), 20);
    ensure(offsets != (0 as u64), 21);
    ensure(out != (0 as u64), 22);

    var count: i64 = 0;
    var start: i64 = 0;
    var i: i64 = 0;
    while i < n_off {
        let len: i64 = ld32(offsets, i * (8 as i64));
        let end: i64 = start + len;
        count = run_segment(cpts, out, count, cap, start, end);
        start = end;
        i = i + (1 as i64);
    }
    return count;
}
