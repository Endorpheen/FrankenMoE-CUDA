// Han character classification: unicode_cpt_is_han in unicode.cpp.
//
// A pure range test over nine Unicode blocks. It is a decision with no state, which is why the original was nine
// if-statements; here the ranges are data and the loop is the whole logic.
//
// The replacement is proved by exhaustion rather than by sampling: the gate compares this against the native
// function for every codepoint in 0..0x10FFFF, so there is no range boundary that can be missed.

const N_RANGES: i64 = 9;

// lo, hi pairs, in the order the native function tests them.
fn range_lo(i: i64) -> i64 {
    if i == 0 { return 0x4E00; }   // CJK Unified Ideographs
    if i == 1 { return 0x3400; }   // Extension A
    if i == 2 { return 0x20000; }  // Extension B
    if i == 3 { return 0x2A700; }  // Extension C
    if i == 4 { return 0x2B740; }  // Extension D
    if i == 5 { return 0x2B820; }  // Extension E
    if i == 6 { return 0x2CEB0; }  // Extension F
    if i == 7 { return 0xF900; }   // Compatibility Ideographs
    return 0x2F800;                // Compatibility Ideographs Supplement
}

fn range_hi(i: i64) -> i64 {
    if i == 0 { return 0x9FFF; }
    if i == 1 { return 0x4DBF; }
    if i == 2 { return 0x2A6DF; }
    if i == 3 { return 0x2B73F; }
    if i == 4 { return 0x2B81F; }
    if i == 5 { return 0x2CEAF; }
    if i == 6 { return 0x2EBEF; }
    if i == 7 { return 0xFAFF; }
    return 0x2FA1F;
}

export fn uc_han(cpt: i64) -> i64 {
    if cpt < (0 as i64) {
        // A negative codepoint is not a codepoint; the native function takes uint32_t, so it cannot arrive there.
        // Refusing is safer than silently wrapping into a valid range.
        return 0;
    }
    var i: i64 = 0;
    while i < N_RANGES {
        if cpt >= range_lo(i) && cpt <= range_hi(i) {
            return 1;
        }
        i = i + (1 as i64);
    }
    return 0;
}
