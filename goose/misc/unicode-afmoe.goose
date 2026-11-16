// AFMOE digit grouping: unicode_regex_split_custom_afmoe in unicode.cpp.
//
// Digits are grouped by the length of the whole run: the run leaves 1 or 2 digits in front when its length is not a
// multiple of three, then the rest is emitted in threes. Everything that is not a digit is left to the caller's
// trailing token.
//
// The number test comes from the classification view, which is indexed by codepoint and carries NUMBER = 0x0002 -
// the same bit the native code consults, so the two cannot disagree about what a digit is.

extern "af_ld_u32" fn ld32(base: u64, byte: i64) -> i64;
extern "af_ld_u16" fn ld16(base: u64, byte: i64) -> i64;
extern "af_ld_i64" fn ld64(base: u64, byte: i64) -> i64;
extern "af_st_i64" fn st(base: u64, byte: i64, v: i64);
extern "af_fail" fn fail(code: i64);

const NUMBER: i64 = 2;
const OUT_OF_RANGE: u32 = 0xFFFFFFFF;

fn is_number(flags: u64, cpt: i64) -> bool {
    if cpt < (0 as i64) || cpt >= (1114112 as i64) {
        return false;
    }
    return (ld16(flags, cpt * (2 as i64)) & NUMBER) != (0 as i64);
}

fn cpt_at(cpts: u64, pos: i64) -> i64 {
    return ld32(cpts, pos * (4 as i64));
}

// Returns the new output count and the new _prev_end through the loop's own arithmetic; kept in one function so the
// segment boundaries cannot be recomputed differently here and in the caller.
fn run_segment(cpts: u64, flags: u64, out: u64, cap: i64, n_out: i64, ini: i64, end: i64) -> i64 {
    var count: i64 = n_out;
    var prev_end: i64 = ini;
    var pos: i64 = ini;

    while pos < end {
        if is_number(flags, cpt_at(cpts, pos)) {
            let digit_start: i64 = pos;
            var digit_count: i64 = 0;
            var inside: bool = true;
            while inside {
                if pos >= end {
                    inside = false;
                } else {
                    if is_number(flags, cpt_at(cpts, pos)) {
                        digit_count = digit_count + (1 as i64);
                        pos = pos + (1 as i64);
                    } else {
                        inside = false;
                    }
                }
            }

            let remainder: i64 = digit_count - ((digit_count / (3 as i64)) * (3 as i64));
            var current: i64 = digit_start;
            if remainder > (0 as i64) {
                let len: i64 = (current + remainder) - prev_end;
                if len > (0 as i64) {
                    if count < cap {
                        st(out, count * (8 as i64), len);
                        count = count + (1 as i64);
                    }
                }
                prev_end = current + remainder;
                current = current + remainder;
            }
            var more: bool = current < digit_start + digit_count;
            while more {
                let stop: i64 = current + (3 as i64);
                let len: i64 = stop - prev_end;
                if len > (0 as i64) {
                    if count < cap {
                        st(out, count * (8 as i64), len);
                        count = count + (1 as i64);
                    }
                }
                prev_end = stop;
                current = stop;
                more = current < digit_start + digit_count;
            }
        } else {
            pos = pos + (1 as i64);
        }
    }

    let tail: i64 = end - prev_end;
    if tail > (0 as i64) {
        if count < cap {
            st(out, count * (8 as i64), tail);
            count = count + (1 as i64);
        }
    }
    return count;
}

// Packet: k=0 {0 cpts, 1 n_off, 2 offsets, 3 flags_view, 4 out, 5 cap}
export fn af_split(packet: i64) -> i64 {
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
