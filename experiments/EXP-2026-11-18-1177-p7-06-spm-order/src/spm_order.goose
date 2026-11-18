// Merge order of the two tokenizer families.
// SPM pops the highest score first; on an equal score the smaller left index goes first.
// The symbol size is not part of the SPM key, even though it is carried in the struct.
// BPE uses the same shape with the merge rank reversed in sign: lower rank first.
// Both are expressed as "a is popped before b", which is what the native comparators mean.
fn g_spm_before(sa: f64, la: i64, sb: f64, lb: i64) -> i64 {
    if sa > sb {
        return 1;
    }
    if sa < sb {
        return 0;
    }
    if la < lb {
        return 1;
    }
    return 0;
}

fn g_bpe_before(ra: i64, la: i64, rb: i64, lb: i64) -> i64 {
    if ra < rb {
        return 1;
    }
    if ra > rb {
        return 0;
    }
    if la < lb {
        return 1;
    }
    return 0;
}

fn main() {
    let a = g_spm_before(1.5, 0, 0.5, 3);
    let b = g_bpe_before(2, 4, 5, 1);
    if a != b {
        return;
    }
}
