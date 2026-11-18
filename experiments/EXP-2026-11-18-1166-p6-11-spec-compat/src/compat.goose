// Draft-model compatibility, transcription of common_speculative_are_compatible's
// scalar decisions: vocab type equality, bos and eos agreement (add flag and id),
// and a bounded vocabulary-size difference.
fn g_spec_compatible(vtype_t: i64, add_bos_t: i64, bos_t: i64, add_eos_t: i64, eos_t: i64, nvocab_t: i64,
                     vtype_d: i64, add_bos_d: i64, bos_d: i64, add_eos_d: i64, eos_d: i64, nvocab_d: i64,
                     max_diff: i64, marker: i64) -> i64 {
    if marker != 190 {
        return -1;
    }
    if vtype_t != vtype_d {
        return 0;
    }
    if add_bos_t != add_bos_d {
        return 0;
    }
    if add_bos_t != 0 && bos_t != bos_d {
        return 0;
    }
    if add_eos_t != add_eos_d {
        return 0;
    }
    if add_eos_t != 0 && eos_t != eos_d {
        return 0;
    }
    var diff: i64 = nvocab_t - nvocab_d;
    if diff < 0 {
        diff = 0 - diff;
    }
    if diff > max_diff {
        return 0;
    }
    return 1;
}

fn main() {
    let r = g_spec_compatible(1, 1, 1, 1, 2, 100, 1, 1, 1, 1, 2, 100, 128, 190);
    if r < 0 {
        return;
    }
}
