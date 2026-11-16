// GPT-2 pre-tokenisation: unicode_regex_split_custom_gpt2 in unicode.cpp.
//
// Seven branches, in the order the native code tests them, because the order is the behaviour:
//   1. contractions      's 't 'm 'd, and 're 've 'll
//   2. space + letters
//   3. space + digits
//   4. space + punctuation (not whitespace, letter or digit, and inside the segment)
//   5. a whitespace run with a lookahead, \s+(?!\S)
//   6. a whitespace run, \s+
//   7. a single character
//
// `add` reproduces the native helper exactly, including that it returns the RUN LENGTH rather than the new position:
// callers write `pos += add(pos + n)`, which equals `pos + n` only while nothing was skipped before. Getting that
// wrong would look almost right and fail on contractions after a space.

extern "gp_ld_u32" fn ld32(base: u64, byte: i64) -> i64;
extern "gp_ld_u16" fn ld16(base: u64, byte: i64) -> i64;
extern "gp_ld_i64" fn ld64(base: u64, byte: i64) -> i64;
extern "gp_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "gp_fail" fn fail(code: i64);

const NUMBER: i64 = 0x0002;
const LETTER: i64 = 0x0004;
const WHITESPACE: i64 = 0x0100;
const OUT_OF_RANGE: i64 = 0xFFFFFFFF;
const SPACE: i64 = 32;
const QUOTE: i64 = 39;

fn in_seg(pos: i64, ini: i64, end: i64) -> bool {
    return pos >= ini && pos < end;
}

fn cpt_at(cpts: u64, pos: i64, ini: i64, end: i64) -> i64 {
    if !in_seg(pos, ini, end) {
        return OUT_OF_RANGE;
    }
    return ld32(cpts, pos * (4 as i64));
}

// The classification view is indexed by codepoint; outside the segment there are no flags at all, which the native
// code expresses as a default-constructed struct - every bit clear, as_uint() == 0.
fn flags_at(flags: u64, cpts: u64, pos: i64, ini: i64, end: i64) -> i64 {
    if !in_seg(pos, ini, end) {
        return 0;
    }
    let c: i64 = ld32(cpts, pos * (4 as i64));
    if c < (0 as i64) || c >= (1114112 as i64) {
        return 0;
    }
    return ld16(flags, c * (2 as i64));
}

fn is_letter(f: i64) -> bool {
    return (f & LETTER) != (0 as i64);
}
fn is_number(f: i64) -> bool {
    return (f & NUMBER) != (0 as i64);
}
fn is_space(f: i64) -> bool {
    return (f & WHITESPACE) != (0 as i64);
}
fn inside(f: i64) -> bool {
    return f != (0 as i64);
}
fn other(f: i64) -> bool {
    return !is_space(f) && !is_letter(f) && !is_number(f) && inside(f);
}

fn emit(out: u64, cap: i64, count: i64, prev_end: i64, at: i64) -> i64 {
    let len: i64 = at - prev_end;
    if len > (0 as i64) && count < cap {
        st(out, count * (8 as i64), len);
    }
    return len;
}

fn count_after(out: u64, count: i64, cap: i64, prev_end: i64, at: i64) -> i64 {
    let len: i64 = at - prev_end;
    if len > (0 as i64) && count < cap {
        st(out, count * (8 as i64), len);
        return count + (1 as i64);
    }
    return count;
}

fn run_segment(cpts: u64, flags: u64, out: u64, cap: i64, count_in: i64, ini: i64, end: i64) -> i64 {
    var count: i64 = count_in;
    var prev_end: i64 = ini;
    var pos: i64 = ini;

    while pos < end {
        let cpt: i64 = cpt_at(cpts, pos, ini, end);
        let f: i64 = flags_at(flags, cpts, pos, ini, end);

        var handled: bool = false;

        // 1. contractions
        if cpt == QUOTE && pos + (1 as i64) < end {
            let n1: i64 = cpt_at(cpts, pos + (1 as i64), ini, end);
            if n1 == 115 || n1 == 116 || n1 == 109 || n1 == 100 {
                let at: i64 = pos + (2 as i64);
                pos = pos + emit(out, cap, count, prev_end, at);
                count = count_after(out, count, cap, prev_end, at);
                prev_end = at;
                handled = true;
            } else {
                if pos + (2 as i64) < end {
                    let n2: i64 = cpt_at(cpts, pos + (2 as i64), ini, end);
                    if (n1 == 114 && n2 == 101) || (n1 == 118 && n2 == 101) || (n1 == 108 && n2 == 108) {
                        let at: i64 = pos + (3 as i64);
                        pos = pos + emit(out, cap, count, prev_end, at);
                        count = count_after(out, count, cap, prev_end, at);
                        prev_end = at;
                        handled = true;
                    }
                }
            }
        }

        if !handled {
            var f2: i64 = f;
            if cpt == SPACE {
                f2 = flags_at(flags, cpts, pos + (1 as i64), ini, end);
            }
            let skip: i64 = if cpt == SPACE { 1 } else { 0 };

            if is_letter(f2) {
                pos = pos + skip;
                f2 = flags_at(flags, cpts, pos, ini, end);
                var more: bool = is_letter(f2);
                while more {
                    pos = pos + (1 as i64);
                    f2 = flags_at(flags, cpts, pos, ini, end);
                    more = is_letter(f2);
                }
                count = count_after(out, count, cap, prev_end, pos);
                prev_end = pos;
                handled = true;
            }

            if !handled && is_number(f2) {
                pos = pos + skip;
                f2 = flags_at(flags, cpts, pos, ini, end);
                var more: bool = is_number(f2);
                while more {
                    pos = pos + (1 as i64);
                    f2 = flags_at(flags, cpts, pos, ini, end);
                    more = is_number(f2);
                }
                count = count_after(out, count, cap, prev_end, pos);
                prev_end = pos;
                handled = true;
            }

            if !handled && other(f2) {
                pos = pos + skip;
                f2 = flags_at(flags, cpts, pos, ini, end);
                var more: bool = other(f2);
                while more {
                    pos = pos + (1 as i64);
                    f2 = flags_at(flags, cpts, pos, ini, end);
                    more = other(f2);
                }
                count = count_after(out, count, cap, prev_end, pos);
                prev_end = pos;
                handled = true;
            }
        }

        if !handled {
            var nws: i64 = 0;
            var scanning: bool = true;
            while scanning {
                let ff: i64 = flags_at(flags, cpts, pos + nws, ini, end);
                if is_space(ff) {
                    nws = nws + (1 as i64);
                } else {
                    scanning = false;
                }
            }

            // 5. \s+(?!\S) - a whitespace run followed by something that is not whitespace keeps one for the next token
            if nws > (1 as i64) && cpt_at(cpts, pos + nws, ini, end) != OUT_OF_RANGE {
                pos = pos + (nws - (1 as i64));
                count = count_after(out, count, cap, prev_end, pos);
                prev_end = pos;
            } else {
                if nws > (0 as i64) {
                    pos = pos + nws;
                    count = count_after(out, count, cap, prev_end, pos);
                    prev_end = pos;
                } else {
                    pos = pos + (1 as i64);
                    count = count_after(out, count, cap, prev_end, pos);
                    prev_end = pos;
                }
            }
        }
    }
    return count;
}

// Packet: k=0 {0 cpts, 1 n_off, 2 offsets, 3 flags_view, 4 out, 5 cap}
export fn gp2_split(packet: i64) -> i64 {
    let p: u64 = packet as u64;
    let cpts: u64 = ld64(p, 0 as i64) as u64;
    let n_off: i64 = ld64(p, 8 as i64);
    let offsets: u64 = ld64(p, 16 as i64) as u64;
    let flags: u64 = ld64(p, 24 as i64) as u64;
    let out: u64 = ld64(p, 32 as i64) as u64;
    let cap: i64 = ld64(p, 40 as i64);

    var count: i64 = 0;
    var start: i64 = 0;
    var i: i64 = 0;
    while i < n_off {
        let len: i64 = ld32(offsets, i * (8 as i64));
        let end: i64 = start + len;
        count = run_segment(cpts, flags, out, cap, count, start, end);
        start = end;
        i = i + (1 as i64);
    }
    return count;
}
