// Name to type id, transcription of the common_speculative_type_from_name map.
// Byte comparison, so no string runtime is required. Unknown names return 11, which is
// COMMON_SPECULATIVE_TYPE_COUNT and is how the native lookup signals failure.
fn g_eq(s: const u8[:], lit: const u8[:]) -> i64 {
    if s.len != lit.len {
        return 0;
    }
    var i: i64 = 0;
    while i < s.len {
        if s[i] != lit[i] {
            return 0;
        }
        i = i + 1;
    }
    return 1;
}

fn g_spec_type_id(name: const u8[:], marker: i64) -> i64 {
    if marker != 196 {
        return -1;
    }
    if g_eq(name, "none") == 1 { return 0; }
    if g_eq(name, "draft-simple") == 1 { return 1; }
    if g_eq(name, "draft-eagle3") == 1 { return 2; }
    if g_eq(name, "draft-mtp") == 1 { return 3; }
    if g_eq(name, "draft-dflash") == 1 { return 4; }
    if g_eq(name, "draft-dspark") == 1 { return 5; }
    if g_eq(name, "ngram-simple") == 1 { return 6; }
    if g_eq(name, "ngram-map-k") == 1 { return 7; }
    if g_eq(name, "ngram-map-k4v") == 1 { return 8; }
    if g_eq(name, "ngram-mod") == 1 { return 9; }
    if g_eq(name, "ngram-cache") == 1 { return 10; }
    return 11;
}

fn main() {
    let m: u8[4] = [110, 111, 110, 101];
    let r = g_spec_type_id(m, 196);
    if r < 0 {
        return;
    }
}
